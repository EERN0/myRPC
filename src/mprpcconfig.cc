/*
 * rpc框架读取配置文件类，配置文件就是ip、端口号，键值对形式的
 * 就是处理字符串，去空格，去掉注释
 * */
#include "mprpcconfig.h"

#include <iostream>
#include <string>

// 负责解析加载配置文件
void MprpcConfig::LoadConfigFile(const char *config_file) {
    FILE *pf = fopen(config_file, "r");
    // 配置文件不存在
    if (nullptr == pf) {
        std::cout << config_file << " is note exist!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 处理配置文件的ip、端口的键值对，下面3种情况
    // 1.注释   2.正确的配置项 =    3.去掉开头的多余的空格
    while (!feof(pf)) {
        char buf[512] = {0};
        fgets(buf, 512, pf);

        // 去掉字符串前面、后面多余的空格
        std::string read_buf(buf);
        Trim(read_buf);

        // 判断#的注释，遇到注释行，直接读取下一行
        if (read_buf[0] == '#' || read_buf.empty()) {
            continue;
        }

        // 解析配置项
        int idx = read_buf.find('=');   // 找到=的下标
        if (idx == -1) {
            // 配置项不合法
            continue;
        }

        std::string key;
        std::string value;
        // key=value
        // rpcserverip=127.0.0.1\n
        key = read_buf.substr(0, idx);  // 键值
        Trim(key);  // 去掉key的空格

        int endidx = read_buf.find('\n', idx);  // 找到最后的换行符

        value = read_buf.substr(idx + 1, endidx - idx - 1); // 实值
        Trim(value);    // 去掉value的空格
        m_configMap.insert({key, value});
    }

    fclose(pf);
}

// 查询配置项信息
std::string MprpcConfig::Load(const std::string &key) {
    auto it = m_configMap.find(key);
    if (it == m_configMap.end()) {  // 没有key，说明没有对应的配置项
        return "";
    }
    return it->second;
}

// 去掉字符串前后的空格
void MprpcConfig::Trim(std::string &src_buf) {
    int idx = src_buf.find_first_not_of(' ');   // idx是第一个非空格的下标

    if (idx != -1) {    // 说明字符串前面有空格
        src_buf = src_buf.substr(idx, src_buf.size() - idx);
    }
    // 去掉字符串后面多余的空格
    idx = src_buf.find_last_not_of(' ');    // 从后面找到第一个非空格的下标
    if (idx != -1) {    // 找到了，说明字符串后面有空格
        src_buf = src_buf.substr(0, idx + 1);
    }
}