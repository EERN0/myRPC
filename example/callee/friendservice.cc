#include <iostream>
#include <string>
#include "friend.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include <vector>
#include "logger.h"

class FriendService : public fixbug::FriendServiceRpc {
public:
    // 本地服务方法
    std::vector<std::string> GetFriendsList(uint32_t userid) {
        std::cout << "do GetFriendsList service! userid:" << userid << std::endl;
        std::vector<std::string> vec;
        vec.push_back("gao yang");
        vec.push_back("liu hong");
        vec.push_back("wang shuo");
        return vec;
    }

    // 重写基类方法
    void GetFriendsList(::google::protobuf::RpcController *controller,
                        const ::fixbug::GetFriendsListRequest *request,
                        ::fixbug::GetFriendsListResponse *response,
                        ::google::protobuf::Closure *done) {
        // 1.rpc框架给业务上报了请求参数LoginRequest，获取相应数据用来做本地业务
        uint32_t userid = request->userid();
        // 2.做本地业务
        std::vector<std::string> friendsList = GetFriendsList(userid);
        // 3.把响应写入  包括错误码、错误消息、返回值
        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");

        for (std::string &name: friendsList) {
            std::string *p = response->add_friends();
            *p = name;
        }

        // 4.执行回调操作，把响应对象数据的序列化和网络发送（都是由框架来完成的）
        done->Run();    // Closure的回调操作
    }
};

int main(int argc, char **argv) {
    // 日志测试
    LOG_INFO("log_test friendService");

    // 调用框架的初始化操作
    MprpcApplication::Init(argc, argv);

    // provider是一个rpc网络服务对象。把FriendService本地服务方法发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new FriendService());

    // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}