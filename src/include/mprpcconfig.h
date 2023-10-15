#pragma once

#include <unordered_map>
#include <string>

// rpcserverip   rpcserverport    zookeeperip   zookeeperport
/*
 * rpc框架读取配置文件类，ip+端口号用哈希表存取，不用进行排序，用unordered_map存储
 * */
class MprpcConfig {
public:
    // 负责解析加载配置文件
    void LoadConfigFile(const char *config_file);

    // 查询配置项信息
    std::string Load(const std::string &key);

private:
    /*
     * 所有容器都不是线程安全的，但是rpc框架只需要初始化一次，只用读取一次配置文件
     * */
    std::unordered_map<std::string, std::string> m_configMap;

    // 去掉字符串前后的空格
    void Trim(std::string &src_buf);
};