// 客户端的信道管理模块，一个直接面向用户的模块，内部包含多个向外提供的服务接口，用户需要什么服务，就调用客户端对应的服务接口
#pragma once
#include "muduo/net/TcpConnection.h"
#include "muduo/proto/codec.h"
#include "muduo/proto/dispatcher.h"
#include "../MQcommon/Log.hpp"
#include "../MQcommon/Helper.hpp"
#include "../MQcommon/mq_msg.pb.h"
#include "../MQcommon/mq_proto.pb.h"
#include "Consumer.hpp"

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

using namespace std;
using namespace ns_ClientConsumer;
using namespace MQ_message;

namespace ns_client_Channel
{
    typedef shared_ptr<google::protobuf::Message> MessagePtr;
    using ProtobufCodecPtr = shared_ptr<ProtobufCodec>;
    using BasicCommonResponsePtr = shared_ptr<BasicCommonResponse>;   // 通用响应类型
    using BasicConsumeResponsePtr = shared_ptr<BasicConsumeResponse>; // 推送消息响应类型

    class Channel
    {
    public:
        using ptr = shared_ptr<Channel>;
        Channel(const muduo::net::TcpConnectionPtr &conn, const ProtobufCodecPtr &codec) : _conn(conn), _codec(codec), _Cid(UUIDHelper::Getuuid())
        {
        }

        ~Channel()
        {
            // 信道析构时，订阅也要对应取消
            BasicCancel();
        }

        // 面向用户的公开接口
        // 信道的创建与删除
        bool OpenChannel()
        {
            OpenChannelRequest req;
            string rid = UUIDHelper::Getuuid(); // 请求ID是唯一的
            req.set_rid(rid);
            req.set_cid(_Cid);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            return bcrp->ok();
        }

        bool DeleteChannel()
        {
            CloseChannelRequest req;
            string rid = UUIDHelper::Getuuid(); // 请求ID是唯一的
            req.set_rid(rid);
            req.set_cid(_Cid);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            return bcrp->ok();
        }

        // 交换机的声明与删除
        bool DeclareExchange(const string &Ename, MQ_message::ExchangeType type, bool durable, bool auto_delete, google::protobuf::Map<string, string> &qargs)
        {
            // 1、构建一个声明交换机的请求对象
            DeclareExchangeRequest req;
            string rid = UUIDHelper::Getuuid(); // 请求ID是唯一的
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_exchange_name(Ename);
            req.set_exchange_type(type);
            req.set_durable(durable);
            req.set_auto_delete(auto_delete);
            req.mutable_args()->swap(qargs);

            // 2、向服务器发送请求
            _codec->send(_conn, req);

            // 3、等待服务器发来的响应
            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            // 4、返回结果
            return bcrp->ok();
        }

        bool DeleteExchange(const string &Ename)
        {
            DeleteExchangeRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_exchange_name(Ename);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            return bcrp->ok();
        }

        // 队列的声明与删除
        bool DeclareQueue(const string &Qname, bool durable, bool exclusive, bool auto_delete, google::protobuf::Map<string, string> &args)
        {
            DeclareQueueRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_queue_name(Qname);
            req.set_exclusive(exclusive);
            req.set_durable(durable);
            req.set_auto_delete(auto_delete);
            req.mutable_args()->swap(args);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            return bcrp->ok();
        }

        bool DeleteQueue(const string &Qname)
        {
            DeleteQueueRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_queue_name(Qname);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            return bcrp->ok();
        }

        // 交换机-队列的绑定与解绑
        bool ExchangeQueueBinding(const string &Ename, const string &Qname, const string &key)
        {
            QueueBindRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_exchange_name(Ename);
            req.set_queue_name(Qname);
            req.set_binding_key(key);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            return bcrp->ok();
        }

        bool ExchangeQueueUnBinding(const string &Ename, const string &Qname)
        {
            QueueUnBindRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_exchange_name(Ename);
            req.set_queue_name(Qname);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            return bcrp->ok();
        }

        // 发布消息（客户端发布消息时，不需要指定队列名称，只需要指定交换机名称，发布给交换机即可，具体放到哪个队列由交换机决定）
        bool BasicPublishMessage(const string &Ename, MQ_message::BasicProperties *bp, const string &body)
        {
            BasicPublishRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_exchange_name(Ename);
            req.set_body(body);
            if (bp != nullptr)
            {
                req.mutable_properties()->set_id(bp->id());
                req.mutable_properties()->set_delivery_mode(bp->delivery_mode());
                req.mutable_properties()->set_routing_key(bp->routing_key());
            }

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);
            if (bcrp->ok())
            {
                lg.LogMessage(info, "消息发布成功！\n");
            }
            return bcrp->ok();
        }

        // 确认消息（确认收到哪一个队列的哪一条消息）
        bool BasicAckQueueMessage(const string &msg_id)
        {
            BasicAckRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_queue_name(_consumer->_Qname); // 确认消息说明是订阅者，一定有队列名
            req.set_message_id(msg_id);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);
            if (bcrp->ok())
            {
                lg.LogMessage(info, "消息已确认\n");
            }
            return bcrp->ok();
        }

        // 队列的订阅（需要额外传入收到推送消息后的业务处理回调函数）
        bool BasicConsume(const string &consumer_tag, const string &queue_name, bool auto_ack, const CallBack &cb)
        {
            if (_consumer != nullptr)
            {
                lg.LogMessage(warning, "当前信道订阅者已订阅队列！请取消订阅后再订阅其它队列消息！\n");
                return false;
            }
            BasicConsumeRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_consumer_tag(consumer_tag);
            req.set_queue_name(queue_name);
            req.set_auto_ack(auto_ack);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);

            if (bcrp->ok() == false)
            {
                lg.LogMessage(errno, "订阅消息队列失败！\n");
                return false;
            }

            _consumer = make_shared<Consumer>(consumer_tag, queue_name, auto_ack, cb); // 关键，处理消息都要依仗会地哦啊函数
            lg.LogMessage(info, "消费者 %s 订阅了消息队列 %s\n", consumer_tag.c_str(), queue_name.c_str());
            return true;
        }

        // 订阅的取消（订阅后，由关联的消费者提供参数，不需要用户给予）
        bool BasicCancel()
        {
            if (_consumer.get() == nullptr)
            {
                // 不是订阅者角色，不需要取消订阅，直接返回即可
                return true;
            }
            BasicCancelRequest req;
            string rid = UUIDHelper::Getuuid();
            req.set_rid(rid);
            req.set_cid(_Cid);
            req.set_consumer_tag(_consumer->_tag);
            req.set_queue_name(_consumer->_Qname);

            _codec->send(_conn, req);

            BasicCommonResponsePtr bcrp = WaitConsume(rid);
            _consumer.reset();

            return bcrp->ok();
        }

        // 其它一些公开接口
        // 向连接提供的接口：连接收到基础响应后，向hash_map中添加响应（每次收到响应，调用此函数，把该响应放进消息队列）
        void PutBasicCommonResponse(const BasicCommonResponsePtr &rep)
        {
            unique_lock<mutex> lock(_mutex);
            _basic_resp[rep->rid()] = rep;
            _cv.notify_all(); // 添加完响应后通过条件变量 (_cv) 通知所有等待的线程继续
        }

        // 连接收到消息推送后，需要通过信道找到对应的消费者对象，通过回调函数进行消息处理（该接口会被封装成一个任务交给线程池执行，消息处理并不在主线程运行）
        void Consume(const BasicConsumeResponsePtr &rep)
        {
            if (_consumer.get() == nullptr)
            {
                lg.LogMessage(error, "消息处理失败！未找到订阅者信息！\n");
                return;
            }
            if (_consumer->_tag != rep->consumer_tag())
            {
                lg.LogMessage(error, "消息处理失败！收到的推送消息中的消费者标识：%s，与当前信道消费者标识：%s不一致！\n", _consumer->_tag.c_str(), rep->consumer_tag().c_str());
                return;
            }
            _consumer->_callback(rep->consumer_tag(), rep->mutable_properties(), rep->body());
            lg.LogMessage(info, "消息处理完毕\n");
        }

        string cid()
        {
            return _Cid;
        }

    private:
        // 等待响应的函数
        BasicCommonResponsePtr WaitConsume(const string &rid)
        {
            unique_lock<mutex> lock(_mutex);
            _cv.wait(lock, [&rid, this]()
                     { return _basic_resp.find(rid) != _basic_resp.end(); }); // 条件变量阻塞等待，结果完成后才继续
            BasicCommonResponsePtr bcrp = _basic_resp[rid];
            _basic_resp.erase(rid);
            return bcrp;
        }

        string _Cid;                                               // 信道ID
        muduo::net::TcpConnectionPtr _conn;                        // 信道关联的网络通信连接对象
        ProtobufCodecPtr _codec;                                   // protobuf协议处理对象
        Consumer::ptr _consumer;                                   // 信道关联的消费者
        unordered_map<string, BasicCommonResponsePtr> _basic_resp; // 请求ID对应的通用响应消息队列映射（只要收到的不是推送消息的响应，都放入此队列中，便于快速判断响应）
        mutex _mutex;                                              // 互斥锁
        condition_variable _cv;                                    // 条件变量，大部分请求都是阻塞操作，发送请求后进程需要阻塞，等到有响应了才能继续
    };

    // 信道管理类，一个连接可能有多个信道
    class ChannelManager
    {
    public:
        using ptr = shared_ptr<ChannelManager>;

        ChannelManager()
        {
        }

        // 创建信道
        Channel::ptr CreateChannel(const muduo::net::TcpConnectionPtr &conn, const ProtobufCodecPtr &codec)
        {
            unique_lock<mutex> lock(_mutex);
            auto channel = make_shared<Channel>(conn, codec);
            _channels[channel->cid()] = channel;
            return channel;
        }

        // 删除信道
        void RemoveChannel(const string &Cid)
        {
            unique_lock<mutex> lock(_mutex);
            _channels.erase(Cid);
        }

        // 查询信道
        Channel::ptr SelectChannel(const string &Cid)
        {
            unique_lock<mutex> lock(_mutex);
            auto it = _channels.find(Cid);
            if (it == _channels.end())
            {
                lg.LogMessage(warning, "该信道不存在！\n");
                return nullptr;
            }
            return it->second;
        }

    private:
        mutex _mutex;
        unordered_map<string, Channel::ptr> _channels; // Cid与信道的映射集合
    };
}