#pragma once

#include <queue>
#include <thread>
#include <mutex>                // pthread_mutex_t
#include <condition_variable>   // 条件变量   pthread_condition_t

// 异步写日志的日志队列
template<typename T>
class LockQueue {
public:
    // Push()：rpc框架的工作线程调用，多个worker线程都会写日志队列缓冲区
    void Push(const T &data) {
        std::lock_guard<std::mutex> lock(m_mutex);  // lock_guard构造加锁lock，析构解锁unlock
        m_queue.push(data);
        m_condvariable.notify_one();    // buffer队列有数据，唤醒磁盘IO线程来消费
    }

    // Pop()：从buffer取日志信息，磁盘IO线程调用
    // 只有一个线程读日志buffer队列，磁盘IO线程，写入到日志文件中
    T Pop() {
        std::unique_lock<std::mutex> lock(m_mutex); // unique_lock是条件变量需要用的锁，

        // 日志buffer队列为空，线程进入wait状态，并释放持有的锁
        while (m_queue.empty()) {
            m_condvariable.wait(lock);  // 磁盘IO线程阻塞，并释放锁
        }

        T data = m_queue.front();
        m_queue.pop();
        return data;
    }

    // buffer队列是否为空
    bool Empty() {
        if (m_queue.empty()) return true;
        else return false;
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condvariable;
};