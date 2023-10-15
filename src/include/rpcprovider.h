#pragma once

#include "google/protobuf/service.h"
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <string>
#include <functional>
#include <google/protobuf/descriptor.h>
#include <unordered_map>

/*
 * 框架提供的专门发布rpc服务的网络对象类
 * rpc框架考虑高并发，使用muduo库
 * */
class RpcProvider {
public:
    // 发布rpc服务对象Service成为rpc节点
    void NotifyService(google::protobuf::Service *service); // rpc框架接收的是任意服务对象，不能写死了。Service抽象类，可以接受子类对象

    // 启动rpc服务节点，开始提供rpc远程网络调用服务
    void Run();

private:
    // 组合EventLoop
    muduo::net::EventLoop m_eventLoop;

    // service服务类的信息
    struct ServiceInfo {
        google::protobuf::Service *m_service; // 保存服务对象，服务方法由服务对象来调用
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap; // m_methodMap中保存服务方法名-服务方法指针
    };
    // m_serviceMap表——保存了发布的服务对象和服务方法，rpc节点上有这些方法（存储注册成功的服务对象和其中服务方法的所有信息）
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    // 新的socket【连接事件】回调函数
    void OnConnection(const muduo::net::TcpConnectionPtr &);

    /* 已建立连接用户的【读写事件】回调函数
     * 在读写事件回调函数(OnMessage)中完成rpc请求和响应的序列化和反序列化
     * */
    void OnMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *, muduo::Timestamp);

    // Closure的回调操作，用于序列化rpc请求的响应 和 网络发送这个响应
    // 响应对象要序列化成字符流后，再通过网络发送出去
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &, google::protobuf::Message *);
};