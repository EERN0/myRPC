/*
 * 业务代码，把本地服务变成一个rpc远程服务
 * */
#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"

/*
 * UserService原来是一个本地服务类，提供了两个本地方法，Login 和 Register
 */
class UserService : public fixbug::UserServiceRpc // 本地业务UserServiceRpc，重写虚函数，把本地方法发布成rpc方法，使用在rpc服务发布端（rpc服务提供者）
{
public:
    // 本地方法
    bool Login(std::string name, std::string pwd) {
        std::cout << "------doing local service: Login------" << std::endl;
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;
        return false;
    }

    bool Register(uint32_t id, std::string name, std::string pwd) {
        std::cout << "------doing local service: Register------" << std::endl;
        std::cout << "id:" << id << "name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    /*
     * 重写基类UserServiceRpc的虚函数 下面这些方法都是框架直接调用的
     * 1. caller   ===>   Login(LoginRequest)  => muduo =>   callee
     * 2. callee   ===>   Login(LoginRequest)  => 交到下面重写的这个Login方法上了
     */
    void Login(::google::protobuf::RpcController *controller,
               const ::fixbug::LoginRequest *request,
               ::fixbug::LoginResponse *response,
               ::google::protobuf::Closure *done) {
        // 1.rpc框架给业务上报了请求参数LoginRequest，获取相应数据用来做本地业务
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 2.做本地业务
        bool login_result = Login(name, pwd);

        // 3.把响应写入  包括错误码、错误消息、返回值
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);   // 没有错误
        code->set_errmsg("");
        response->set_sucess(login_result);

        // 4.执行回调操作，把响应对象数据的序列化和网络发送（都是由框架来完成的）
        done->Run();
    }

    void Register(::google::protobuf::RpcController *controller,
                  const ::fixbug::RegisterRequest *request,
                  ::fixbug::RegisterResponse *response,
                  ::google::protobuf::Closure *done) {
        uint32_t id = request->id();
        std::string name = request->name();
        std::string pwd = request->pwd();

        bool ret = Register(id, name, pwd);

        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        response->set_sucess(ret);

        done->Run();
    }
};

int main(int argc, char **argv) {
    /*
     * 框架的使用，本地业务callee没有调用Login这些方法，全是由rpc框架调用的
     * */
    // 初始化rpc框架
    MprpcApplication::Init(argc, argv); // ip地址和端口号，不能写死，从配置文件test.config中读取

    // provider是一个rpc网络服务对象，用于发布rpc方法，把本地业务UserService对象发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new UserService());  // 发布服务对象Service作为rpc节点

    // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}