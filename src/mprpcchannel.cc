#include "mprpcchannel.h"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "mprpcapplication.h"
#include "mprpccontroller.h"
#include "zookeeperutil.h"

/*
 * rpc调用方主要就是重写的RpcChannel类中CallMethod()虚函数，多态性
 *
 * rpc数据格式：header_size + service_name method_name args_size + args
 *
 * rpc调用方，发起rpc请求都要使用CallMethod()，
 *      - 把rpc请求数据进行序列化，再通过网络发送给rpc节点（中间要查询zk）
 *      - 把从rpc节点发来的，通过网络传输的响应字符流，反序列化成response对象，调用方就可以使用rpc请求的结果和响应了
 *      - 执行完整个CallMethod()，才表示一个完整的rpc请求的调用和响应
 */

// 所有通过stub代理类对象调用的rpc方法，都走到这里CallMethod方法，统一做rpc调用数据的序列化和网络发送
// 作用：参数打包、数据序列化、从zk上获取rpc节点的主机信息、发起rpc调用
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                              google::protobuf::RpcController *controller,  // rpc请求调用的状态信息
                              const google::protobuf::Message *request,
                              google::protobuf::Message *response,
                              google::protobuf::Closure *done) {
    const google::protobuf::ServiceDescriptor *sd = method->service();
    std::string service_name = sd->name();      // 服务名
    std::string method_name = method->name();   // 方法名

    // 获取rpc请求的参数request序列化后的字符串长度 args_size，防止粘包（读到下一个rpc数据了）
    uint32_t args_size = 0;
    std::string args_str;       // rpc请求参数列表request序列化成字符串存在args_str里
    if (request->SerializeToString(&args_str)) {
        args_size = args_str.size();
    }
    else {
        controller->SetFailed("serialize request error!");  // 序列化请求出错
        return;
    }

    // 定义rpc的请求header
    // rpc请求头部分：service_name + method_name + args_size（方法参数request序列化成字符串后的大小）
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    // header_size是rpc请求头的大小，args_size是请求参数的大小，避免粘包
    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str)) {
        header_size = rpc_header_str.size();
    }
    else {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    // 1.打包要发送的rpc请求数据
    // 组织要在网路上发送给rpc节点的rpc请求的字符串send_rpc_str
    // rpc调用方要发送的数据：head_size + rpcheader + args_str (rpc请求头部长度用来获取服务名和方法名、参数长度用来避免和下一个rpc请求发生粘包)
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char *) &header_size, 4)); // rpc请求的前4个字节，是header_size，用来获取rpc请求头（服务名+方法名）
    send_rpc_str += rpc_header_str; // rpcheader
    send_rpc_str += args_str;       // args(请求方法的参数)

    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "============================================" << std::endl;

    // 2.通过网络发送rpc请求数据
    // 客户端使用简单的TCP编程即可，不用高并发
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) {
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 初期测试阶段：
    // 不用zk的服务发现，直接读取配置文件rpcserver的信息拿到ip和端口
    // std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    // uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());

    // 3.rpc调用方先去zk上查询服务方法，获取rpc节点的ip和端口
    // rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在rpc节点的host信息
    ZkClient zkCli;
    zkCli.Start();
    //  znode节点路径：/UserServiceRpc/Login
    std::string method_path = "/" + service_name + "/" + method_name;
    // 127.0.0.1:8000
    std::string host_data = zkCli.GetData(method_path.c_str());     // 获取znode临时节点的数据，格式是 ip:端口号
    if (host_data == "") {
        controller->SetFailed(method_path + " is not exist!");
        return;
    }
    int idx = host_data.find(":");
    if (idx == -1) {
        controller->SetFailed(method_path + " address is invalid!");
        return;
    }
    // 取出ip和端口号
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());

    // 4.rpc调用方和rpc节点建立tcp连接
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 连接rpc节点（rpc服务提供方）
    if (-1 == connect(clientfd, (struct sockaddr *) &server_addr, sizeof(server_addr))) {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 5.发送rpc请求
    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0)) {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 6.接收rpc请求的响应
    char recv_buf[1024] = {0};
    int recv_size = 0;  // 接收到的数据长度
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0))) {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 反序列化rpc调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size); // bug出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败
    // if (!response->ParseFromString(response_str))
    if (!response->ParseFromArray(recv_buf, recv_size)) {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
    // 反序列化rpc响应成功，调用方可以用google::protobuf::Message *response拿到response对象，从而获取响应信息

    close(clientfd);
}