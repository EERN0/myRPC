#aux_source_directory(. SRC_LIST)
set(SRC_LIST 
    mprpcapplication.cc 
    mprpcconfig.cc 
    rpcheader.pb.cc 
    rpcprovider.cc 
    mprpcchannel.cc
    mprpccontroller.cc
    logger.cc
    zookeeperutil.cc)
add_library(mprpc ${SRC_LIST})

# 链接muduo静态库的时候，muduo_net、muduo_base先后顺序不能错！
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)