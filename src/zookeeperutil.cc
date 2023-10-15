#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <semaphore.h>
#include <iostream>
#include "logger.h"

// 全局的watcher观察器   zkserver给zkclient的通知
void global_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx) {
    // type：回调的消息类型是和会话相关的消息类型（会话就是连接，或断开连接）
    if (type == ZOO_SESSION_EVENT) {
        // zkclient和zkserver连接成功，信号量
        if (state == ZOO_CONNECTED_STATE) {
            sem_t *sem = (sem_t *) zoo_get_context(zh); // 拿出句柄zhandle_t里面的信号量sem
            sem_post(sem);  // v操作，sem加1
        }
    }
}

// 构造函数，句柄初始化为null
ZkClient::ZkClient() : m_zhandle(nullptr) {}

// 析构，关闭句柄，释放资源  类似MySQL_Conn
ZkClient::~ZkClient() {
    if (m_zhandle != nullptr) {
        zookeeper_close(m_zhandle);
    }
}

/*
 * zkClient连接zkServer
 * 异步连接过程，要绑定一个全局的回调函数，回调函数操作信号量sem，判断连接是否成功
 * */
void ZkClient::Start() {
    // 加载zk的ip和端口（zk默认是2181端口）
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    // zk的host格式【ip:port】
    std::string connstr = host + ":" + port;

    /*
     * zookeeper_mt：多线程版本
     * zookeeper的API客户端程序提供了三个线程：
     *  1.API调用线程
     *  2.网络I/O线程，使用pthread_create  和 poll（不是epoll），ZkClient是客户端程序不用做到高并发
     *  3.watcher回调线程 pthread_create
     */
    // global_watcher是回调函数，是zk服务端给zk客户端的通过该回调函数进行通知
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr, 0); // session的超时时间设置为30s
    if (nullptr == m_zhandle) { // 创建句柄m_zhandle资源失败了（可能是内存不够了），而不是连接服务端失败了
        LOG_ERR("zookeeper_init error!");
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 创建句柄成功，而不是连接zk服务端成功

    sem_t sem;  // 信号量
    sem_init(&sem, 0, 0);   // 初始信号量sem为0，下面sem_wait()会阻塞，zkClient和zkServer连接成功后退出阻塞
    zoo_set_context(m_zhandle, &sem);   // 句柄m_zhandle里面绑定sem

    sem_wait(&sem); // zk服务端给zk客户端响应时，执行了v操作，sem变成1，退出阻塞
    LOG_INFO("zookeeper_init success!");
    std::cout << "zookeeper_init success!" << std::endl;
}

/*
 * 根据路径，创建znode节点
 * -data：znode节点数据，ip+端口号
 * -state：znode节点状态，0-永久性节点，1-临时性节点
 * */
void ZkClient::Create(const char *path, const char *data, int datalen, int state) {
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
    // 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
    flag = zoo_exists(m_zhandle, path, 0, nullptr);

    if (ZNONODE == flag) {  // path的znode节点不存在
        // 创建指定path的znode节点
        flag = zoo_create(m_zhandle, path, data, datalen,
                          &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK) {
            LOG_INFO("znode create success  path:%s", path);
            std::cout << "znode create success... path:" << path << std::endl;
        }
        else {
            std::cout << "flag:" << flag << std::endl;
            std::cout << "znode create error... path:" << path << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * 根据指定的path，获取znode节点的值——ip+端口号
 * */
std::string ZkClient::GetData(const char *path) {
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if (flag != ZOK) {
        std::cout << "get znode error... path:" << path << std::endl;
        return "";
    }
    else {
        return buffer;
    }
}