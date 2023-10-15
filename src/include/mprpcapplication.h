#pragma once    // 防止头文件重复包含
#include "mprpcconfig.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"

/*
 * rpc框架的基础类，负责框架的一些初始化操作
 * rpc框架只需要一个，共享配置信息、日志信息。【配置文件类、日志类对象都只用一个对象，全是单例模式】
 * 用饿汉式单例模式来设计
 * */
class MprpcApplication {
public:
    static void Init(int argc, char **argv);    // 初始化rpc框架

    // 专门获取实例的方法
    static MprpcApplication &GetInstance();

    static MprpcConfig &GetConfig();

private:
    static MprpcConfig m_config;    // 配置文件

    MprpcApplication() {}

    // 把所有和拷贝相关的构造函数都去掉
    MprpcApplication(const MprpcApplication &) = delete;

    MprpcApplication(MprpcApplication &&) = delete;
};