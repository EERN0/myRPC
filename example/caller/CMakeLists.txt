# 客户端的rpc请求：FriendServiceRpc

# set(SRC_LIST calluserservice.cc ../user.pb.cc)
set(SRC_LIST callfriendservice.cc ../friend.pb.cc)

add_executable(consumer ${SRC_LIST})


# 嘿嘿，解决了
# target_link_libraries(consumer mprpc protobuf) # 链接不上protobuf库
# protobuf库被安装在/usr/local/lib下，cmake编译不了，用这种方式找到protobuf库安装的位置

# FindProtobuf module
include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROJECT_SOURCE_DIR} ${PROTOBUF_INCLUDE_DIR})

link_directories(${PROTOBUF_LIBRARY})

target_link_libraries(consumer mprpc ${PROJECT_NAME} ${PROTOBUF_LIBRARY})
# target_link_libraries(consumer mprpc protobuf) # 链接不上protobuf库
