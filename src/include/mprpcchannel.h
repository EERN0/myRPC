/*
 * rpc调用方做的操作：参数打包、数据序列化、从zk上获取rpc节点的主机信息、发起rpc调用
 * */
#pragma once

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

class MprpcChannel : public google::protobuf::RpcChannel {
public:
    // 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做【rpc方法调用】的数据数据序列化和网络发送
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done);
};