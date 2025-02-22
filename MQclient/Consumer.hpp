// 订阅者模块，在客户端中并不直接向用户展示，作用只是描述当前信道是个订阅者（消费者）信道
// 项目为简化操作，一个信道只有一个消费者（实际上，不同的订阅者保存着不同的回调函数，用来处理消息）
#pragma once

#include "../MQcommon/Helper.hpp"
#include "../MQcommon/Log.hpp"
#include "../MQcommon/mq_msg.pb.h"

#include <iostream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>

using namespace std;
using namespace ns_helper;

namespace ns_ClientConsumer
{
    // 消费者（订阅者）结构
    using CallBack = function<void(const string, const MQ_message::BasicProperties *bp, const string)>;

    class Consumer
    {
    public:
        using ptr = shared_ptr<Consumer>;
        Consumer()
        {
            lg.LogMessage(info, "新增消费者ID：%s\n", _tag.c_str());
        }

        Consumer(const string &tag, const string &Qname, bool auto_ack, const CallBack &callback) : _tag(tag), _Qname(Qname), _auto_ack(auto_ack), _callback(callback)
        {
            lg.LogMessage(info, "新增消费者ID：%s\n", _tag.c_str());
        }

        ~Consumer()
        {
            lg.LogMessage(info, "%s 消费者已退出\n", _tag.c_str());
        }
        string _tag;        // 消费者标识
        string _Qname;      // 订阅的消息队列名称
        bool _auto_ack;     // 是否自动应答标志（若为真，一个消息被消费者消费后，会直接移除该待确认信息；若为假，需要等待客户端确认）
        CallBack _callback; // 消息的回调函数（客户端调用这个函数进行消息的处理）
    };
}