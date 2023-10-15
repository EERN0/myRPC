#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>
#include <string>

MprpcConfig MprpcApplication::m_config;

void ShowArgsHelp() {
    std::cout << "rpc启动格式: command -i <configfile>" << std::endl;
}

// 类外实现静态方法，不用带static了
void MprpcApplication::Init(int argc, char **argv) {
    // rpc节点启动没传入参数，启动失败
    if (argc < 2) {
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }

    int c = 0;
    std::string config_file;
    while ((c = getopt(argc, argv, "i:")) != -1) {  // getopt获取命令行参数中有-i的，去掉-
        switch (c) {
            case 'i':   // 出现了-i，并且带了参数的
                config_file = optarg;
                break;
            case '?':
                ShowArgsHelp();
                exit(EXIT_FAILURE);
            case ':':   // 出现-i，但是没带参数
                ShowArgsHelp();
                exit(EXIT_FAILURE);
            default:
                break;
        }
    }
/*
 * 通过命令行参数，拿到用户指定的配置文件，然后解析配置文件
 * */
    // 开始加载配置文件了
    // 配置文件的内容：rpcserver_ip=xx  rpcserver_port=xx   zookeeper_ip=xx  zookepper_port=xx
    m_config.LoadConfigFile(config_file.c_str());

    // 打印配置信息
    // std::cout << "rpcserverip:" << m_config.Load("rpcserverip") << std::endl;
    // std::cout << "rpcserverport:" << m_config.Load("rpcserverport") << std::endl;
    // std::cout << "zookeeperip:" << m_config.Load("zookeeperip") << std::endl;
    // std::cout << "zookeeperport:" << m_config.Load("zookeeperport") << std::endl;
}

// 专门获取MprpcApplication实例的方法
MprpcApplication &MprpcApplication::GetInstance() {
    static MprpcApplication app;
    return app;
}

// 初始化静态方法GetConfig：读取配置文件
MprpcConfig &MprpcApplication::GetConfig() {
    return m_config;
}