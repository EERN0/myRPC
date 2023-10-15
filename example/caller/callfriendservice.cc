#include <iostream>
#include "mprpcapplication.h"
#include "friend.pb.h"
#include "logger.h"

int main(int argc, char **argv) {
    // 整个程序启动以后，使用rpc框架来调用rpc服务，先调用框架的初始化函数（rpc框架只初始化一次）
    MprpcApplication::Init(argc, argv);

    // 调用远程发布的rpc方法
    fixbug::FriendServiceRpc_Stub stub(new MprpcChannel());
    // rpc方法的请求参数
    fixbug::GetFriendsListRequest request;
    request.set_userid(1000);
    // rpc方法的响应
    fixbug::GetFriendsListResponse response;
    // 发起rpc方法的调用  同步的rpc调用过程  MprpcChannel::callmethod

    /*
     * 用stub代理对象调用服务方法时，所有的代理对象调用的rpc方法都转到RpcChannel里的callMethod里面了
     * */
    MprpcController controller;
    // RpcChannel->RpcChannel::callMethod方法     集中来做所有rpc方法调用的参数序列化和网络发送
    stub.GetFriendsList(&controller, &request, &response, nullptr);     // 发起一次rpc调用，callMethod中的多个过程可能出错，用controller来记录rpc调用的状态信息


    // 一次rpc调用完成，读调用的结果
    if (controller.Failed()) {
        std::cout << controller.ErrorText() << std::endl;
    }
    else {  // rpc请求调用成功，才执行
        if (0 == response.result().errcode()) {
            LOG_INFO("1111");
            LOG_INFO("rpc GetFriendsList response success!");
            // 在控制台输出一遍，方便查看
            std::cout << "rpc GetFriendsList response success!" << std::endl;
            int size = response.friends_size();
            for (int i = 0; i < size; ++i) {    // 打印所有好友的姓名
                std::cout << "index:" << (i + 1) << " name:" << response.friends(i) << std::endl;
            }
        }
        else {
            LOG_ERR("rpc GetFriendsList response error : %s", response.result().errmsg().c_str());
            // 在控制台输出一遍，方便查看
            std::cout << "rpc GetFriendsList response error : " << response.result().errmsg() << std::endl;
        }
    }
    return 0;
}