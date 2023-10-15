#include "logger.h"
#include <time.h>
#include <iostream>

// 获取日志的单例
Logger &Logger::GetInstance() {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    // 启动专门的写日志线程（后台磁盘IO线程，专门从buffer中取日志写入磁盘的）
    // 磁盘IO线程，后台线程，一直运行，当buffer有日志信息时，就拿出数据，就写到当天的日志文件中
    // 如果日志buffer队列为空时，就可以关闭日志文件，每一次写完日志信息，附带着日志的打开关闭
    // 如果buffer队列一直有信息，就把日志信息写完再关文件
    std::thread writeLogTask([&]() {
        for (;;) {
            // 获取当前的日期，取日志信息，写入相应的日志文件当中
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);

            char file_name[128];
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday);

            FILE *pf = fopen(file_name, "a+");  // 追加的方式a+写日志
            if (pf == nullptr) {
                std::cout << "logger file : " << file_name << " open error!" << std::endl;
                exit(EXIT_FAILURE);
            }

            // 磁盘IO线程每写完一次日志，都打开关闭日志文件一次
            std::string msg = m_lckQue.Pop();   // buffer队头的日志信息
            // 日志时分秒的时间戳信息
            char time_buf[128] = {0};
            sprintf(time_buf, "%d:%d:%d =>[%s] ", nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec,
                    (m_loglevel == INFO ? "info" : "error"));
            msg.insert(0, time_buf);
            msg.append("\n");
            fputs(msg.c_str(), pf);

            // 关闭之前打开的文件，把数据刷到磁盘上
            fclose(pf);
        }
    });
    // 设置线程分离
    writeLogTask.detach();
}

// 设置日志级别 
void Logger::SetLogLevel(LogLevel level) {
    m_loglevel = level;
}

// 写日志
// 把日志信息写入lockqueue缓冲区当中
void Logger::Log(std::string msg) {
    m_lckQue.Push(msg);
}