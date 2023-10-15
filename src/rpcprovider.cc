#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"
/*
 * RpcProvider的功能：
 * 1.网络功能用muduo库实现，用来发起rpc请求和响应rpc调用结果
 * 2.把rpc节点要发布的rpc服务注册到zk服务中心，zk作为服务注册和发现中心
 * 3.接收一个rpc调用请求，知道要调用应用程序的哪个服务对象的的哪个rpc方法——生成一张表，记录服务对象和其发布的所有服务方法
 *
 * protobuf提供了【Service类 对象】和【Method类 方法】
 *               UserService         Login       Register
 *               FriendService       AddFriend   DelFriend  GetFriendsList
 * */

/*
 * service_name =>  service描述
 *                      =》 service*指针：记录服务对象
 *                          method_name  =>  method方法对象
 */

/*
 * rpc节点的【本地方法】发布成【rpc服务方法】
 * NotifyService方法——rpc框架提供给调用方使用的，可以发布rpc方法的函数接口
 * rpc框架调用的rpc请求方法，而不是本地服务调用的
 * */
void RpcProvider::NotifyService(google::protobuf::Service *service) {
    ServiceInfo service_info;   // service服务类的信息

    // 获取了服务对象的描述信息     service->GetDescriptor()
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    // 获取服务对象service中方法的数量
    int methodCnt = pserviceDesc->method_count();

    // 控制台输出的信息太多了！用日志来记录   std::cout << "service_name:" << service_name << std::endl;
    LOG_INFO("service_name:%s", service_name.c_str());

    for (int i = 0; i < methodCnt; ++i) {
        // 获取了服务对象指定下标的服务方法的描述（抽象的，不是具体的方法）     UserService   Login
        const google::protobuf::MethodDescriptor *pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});

        LOG_INFO("method_name:%s", method_name.c_str());
    }
    service_info.m_service = service;   // 具体服务     UserService
    // 把服务对象和方法写到map表中
    m_serviceMap.insert({service_name, service_info});
}

/*
 * 启动rpc节点，提供rpc远程网络调用服务
 * */
void RpcProvider::Run() {
    // 读取配置文件rpcserver的信息
    // rpc框架服务器的ip和端口号
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    // 绑定连接回调函数和消息读写回调函数，用muduo库很好地分离了网络代码和业务代码
    // 1.绑定TCP连接回调函数
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    // 2.绑定读写回调函数
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3));

    // 设置muduo库的线程数量
    server.setThreadNum(4); // 1个IO线程、3个工作线程

    // 把当前rpc节点上要发布的服务（服务对象、方法、ip-端口号）全部注册到zk服务端上，让rpc client(zk客户端)可以从zk上发现服务
    // session timeout：30s     zkclient 网络I/O线程  1/3 * timeout 时间发送ping消息
    ZkClient zkCli;
    zkCli.Start();
    // service_name为永久性节点    method_name为临时性节点
    for (auto &sp: m_serviceMap) {
        // 创建父节点（服务作为父节点，永久节点）：/service_name ---> /UserServiceRpc
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0); // 创建znode节点，service是永久节点

        // 创建临时节点（服务的方法作为临时节点）：/service_name/method_name ---> /UserServiceRpc/Login 存储当前这个rpc服务节点主机的ip和port
        for (auto &mp: sp.second.m_methodMap) {
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);

            // ZOO_EPHEMERAL表示znode是一个临时性节点
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }

    // rpc服务端准备启动，打印信息
    LOG_INFO("RpcProvider start service at ip:[%s] port:[%d]", ip.c_str(), port);
//    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;

    // 启动网络服务
    server.start();
    m_eventLoop.loop();     // 启动epoll_wait，以阻塞的方式等待远程连接
}

// 新的socket连接回调函数
/*
 * 连接建立和断开，肯定不用序列化和反序列化
 * */
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn) {
    if (!conn->connected()) {   // 和rpc调用请求方的连接断开了，释放连接资源，给其他rpc客户端继续提供服务
        conn->shutdown();
    }
}

/*
 * 在框架内部，RpcProvider和RpcConsumer协商好之间通信用的protobuf数据类型
 * service_name method_name args    定义proto的message类型，进行数据头的序列化和反序列化
                                 service_name method_name args_size
16UserServiceLoginzhang san123456

header_size(4个字节) + header_str + args_str

 * string中的copy方法
 */

// 已建立连接用户的读写事件回调函数
// 有一个rpc服务调用请求，那么OnMessage方法就会响应
/*
 * 在读写事件回调函数(OnMessage)中完成rpc请求和响应的序列化和反序列化
 * */
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp) {
    // 网络上接收的远程rpc调用请求的字符流，要包含rpc方法的名字+参数
    std::string recv_buf = buffer->retrieveAllAsString();

    // 从字符流中读取前4个字节的内容，作为header_size
    uint32_t header_size = 0;
    recv_buf.copy((char *) &header_size, 4, 0);

    // 根据header_size读取数据头的原始字符流
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    // 反序列化原始字符流数据，得到rpc请求的详细消息
    mprpc::RpcHeader rpcHeader; // rpc数据头对象，rpc数据头=服务名+方法名+参数大小
    std::string service_name;   // 服务名
    std::string method_name;    // 方法名
    uint32_t args_size;         // 参数大小
    if (rpcHeader.ParseFromString(rpc_header_str)) {    // 字符流数据【反序列化】到rpc数据头对象中
        // 在proto文件中定义的message类中的变量，会生成读写方法。用读方法获取到具体的服务名、方法名和参数大小
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else {  // rpc数据头反序列化失败
        LOG_ERR("rpc_header_str: %s  parse error!", rpc_header_str.c_str());
//        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        return;
    }

    // 获取rpc方法参数的字符流数据      rpc数据格式：header_size + header_str + args_str
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印调试信息
//    LOG_INFO("============================================");
//    LOG_INFO("header_size: %d", header_size);
//    LOG_INFO("rpc_header_str: %s", rpc_header_str.c_str());
//    LOG_INFO("service_name: %s", service_name.c_str());
//    LOG_INFO("method_name: %s", method_name.c_str());
//    LOG_INFO("args_str: %s", args_str.c_str());
//    LOG_INFO("============================================");
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "============================================" << std::endl;

    // 获取service对象和method对象，查map表
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end()) {
        // map表中没有这个服务，说明当前rpc节点没有提供这个rpc服务
        LOG_ERR("%s is not exist!", service_name.c_str());
//        std::cout << service_name << " is not exist!" << std::endl;
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end()) {
        // 没有这个rpc方法
        LOG_ERR("%s:%sis not exist!", service_name.c_str(), method_name.c_str());
//        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }

    google::protobuf::Service *service = it->second.m_service;      // 获取service对象  eg：new UserService
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method对象   eg：Login

    // 生成rpc方法调用的请求request和响应response参数，要看具体的方法参数和返回值
    // protobuf已经进行抽象处理了，可以通过服务对象获取到在proto文件里定义好的message类型的请求和响应对象。（看user.proto描述文件）
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str)) {  // 请求参数，rpc请求中序列化后的args_str，反序列化到request对象中
        LOG_ERR("request parse error, content:%s", args_str.c_str());
//        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }
    // 响应，不在rpc节点中处理，rpc节点调用本地服务方法，在本地业务做完之后返回给rpc节点(rpc框架)响应response对象
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的method方法的调用，绑定一个Closure的回调函数。
    // 这个回调函数会调用SendRpcResponse函数，负责把响应序列化后，通过网络发送出去
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider,    // 强制指定模板类型实参
            const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(
            this, &RpcProvider::SendRpcResponse, conn, response);

    // rpc框架根据远端的rpc请求，调用当前rpc节点上发布的方法。（由rpc框架去调用应用层的方法，返回执行本地业务的响应，由框架再执行响应消息的序列化和消息的发送）
    // rpc框架调用应用层的方法，具体调用——new UserService().Login(controller, request, response, done)，抽象如下：
    service->CallMethod(method, nullptr, request, response, done);
}


/*
 * Closure的回调操作：序列化rpc请求的响应 和 通过网络发送这个响应
 * */
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response) {
    std::string response_str;
    if (response->SerializeToString(&response_str)) {   // response进行序列化
        // 序列化成功后，通过网络把rpc方法的响应结果发送回rpc的调用方
        conn->send(response_str);
    }
    else {
        LOG_ERR("serialize response_str error!");
//        std::cout << "serialize response_str error!" << std::endl;
    }

    // 模拟http的短链接服务，rpc节点（服务提供者rpcprovider）主动断开连接
    // 释放连接资源，给其他rpc客户端继续提供服务
    conn->shutdown();
}