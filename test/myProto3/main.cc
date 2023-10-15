#include "test.pb.h"
#include <iostream>
#include <string>

using namespace fixbug;

int main1() {
    // req对象
    LoginRequest req;
    req.set_name("zhangsan");
    req.set_pwd("123456");

    // 对象序列化成字节流后，才能发送到网络上
    std::string send_str;
    if (req.SerializeToString(&send_str)) { // 把对象序列化到字符串send_str里面
        std::cout << send_str.c_str() << std::endl;
    }

    // protobuf反序列化——ParseFromString
    // 从send_str字符串反序列化出一个login请求对象reqB
    LoginRequest reqB;
    if (reqB.ParseFromString(send_str)) {
        std::cout << reqB.name() << std::endl;
        std::cout << reqB.pwd() << std::endl;
    }

    return 0;
}

int main() {
    GetFriendListsResponse rsp;
    ResultCode *rc = rsp.mutable_result_code();     // 类成员对象的类型是另一个类，用mutable
    rc->set_errcode(0);

    User *user1 = rsp.add_friend_list();    // 列表类，用add
    user1->set_name("A");
    user1->set_age(10);
    user1->set_sex(User::MAN);

    User *user2 = rsp.add_friend_list();
    user2->set_name("B");
    user2->set_age(20);
    user2->set_sex(User::WOMAN);

    // 输出rsp好友列表中好友数
    std::cout << "rsp好友列表中好友数: " << rsp.friend_list_size() << std::endl;
    std::cout << "rsp好友列表中第一个好友名字: " << rsp.friend_list().begin()->name() << std::endl;

    return 0;
}