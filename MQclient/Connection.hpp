// 连接管理模块（通过连接创立信道，再通过信道提供服务），通过连接搭建客户端，不需要再提供服务了

#pragma once
#include "muduo/proto/dispatcher.h"
#include "muduo/proto/codec.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/base/CountDownLatch.h"

#include "Channel.hpp"
#include "AsyncWorker.hpp"

using namespace std;
using namespace ns_client_Channel;
using namespace ns_AsyncWorker;
using namespace MQ_message;

namespace ns_client_Connection
{
    class Connection
    {
    public:
        using ptr = shared_ptr<Connection>;
        // 只会收到推送消息和通用响应两种响应：BasicConsumeResponsePtr  BasicCommonResponsePtr
        // 传入参数：服务器IP，服务器端口号，异步工作线程
        Connection(const string &Sip, int Sport, const AsyncWorker::ptr &worker) : _latch(1), // 初始化为1，说明有1个事件需要等待，内部是个计数器
                                                                                   _client(worker->loopthread.startLoop() /*EventLoop实例被绑定到TcpClient实例上，使EventLoop在监控到事件时可以调用TcpClient注册的回调函数进行处理*/, muduo::net::InetAddress(Sip, Sport), "Client"),
                                                                                   _dispatcher(bind(&Connection::onUnknownMessage, this, placeholders::_1, placeholders::_2, placeholders::_3)),
                                                                                   _codec(make_shared<ProtobufCodec>(bind(&ProtobufDispatcher::onProtobufMessage, &_dispatcher, placeholders::_1, placeholders::_2, placeholders::_3))),
                                                                                   _worker(worker), _channel_manager(make_shared<ChannelManager>())
        {
            // 注册不同业务（推送消息、通用）响应处理函数，根据参数不同，选择不同的回调函数处理
            _dispatcher.registerMessageCallback<BasicConsumeResponse>(bind(&Connection::ConsumeResponse, this, placeholders::_1, placeholders::_2, placeholders::_3));
            _dispatcher.registerMessageCallback<BasicCommonResponse>(bind(&Connection::CommonResponse, this, placeholders::_1, placeholders::_2, placeholders::_3));

            // 设置消息回调处理函数，对收到的数据进行协议解析（通过TcpConnection中的事件类型来判断）
            _client.setMessageCallback(bind(&ProtobufCodec::onMessage, _codec, placeholders::_1, placeholders::_2, placeholders::_3));

            // 建立链接成功后的回调函数（通过TcpConnection中的事件类型来判断）
            _client.setConnectionCallback(bind(&Connection::onConnection, this, placeholders::_1));

            // 连接服务器（非阻塞接口，即发起请求后会立刻返回，不保证连接一定成功），因此需要CountDownLatch接口，阻塞等待链接建立成功后才返回
            _client.connect();
            _latch.wait(); // 一直阻塞等待，直至连接建立成功（ConnectionBackFuction函数被调用）
        }

        // 打开/创建信道
        Channel::ptr OpenChannel()
        {
            Channel::ptr channel = _channel_manager->CreateChannel(_conn, _codec);
            bool res = channel->OpenChannel();
            if (res == false)
            {
                lg.LogMessage(errno, "打开信道失败！\n");
                return nullptr;
            }
            return channel;
        }

        // 关闭信道
        void CloseChannel(const Channel::ptr &channel)
        {
            bool res = channel->DeleteChannel();
            _channel_manager->RemoveChannel(channel->cid());
            if (res == false)
            {
                lg.LogMessage(error, "删除信道失败！\n");
                return;
            }
        }

    private:
        // 处理推送消息响应
        void ConsumeResponse(const muduo::net::TcpConnectionPtr &conn, const BasicConsumeResponsePtr &message, muduo::Timestamp)
        {
            // 1、找到信道
            Channel::ptr channel = _channel_manager->SelectChannel(message->cid());
            if (channel.get() == nullptr)
            {
                lg.LogMessage(errno, "查询信道消息失败！\n");
                return;
            }

            // 2、封装异步任务（消息处理任务），抛入线程池
            _worker->pool.push([channel, message]()
                               { channel->Consume(message); });
        }

        // 处理通用响应
        void CommonResponse(const muduo::net::TcpConnectionPtr &conn, const BasicCommonResponsePtr &message, muduo::Timestamp)
        {
            // 1、找到信道
            Channel::ptr channel = _channel_manager->SelectChannel(message->cid());
            if (channel.get() == nullptr)
            {
                lg.LogMessage(errno, "查询信道消息失败！\n");
                return;
            }

            // 2、将得到的响应对象添加到信道的通用响应hash_map中
            channel->PutBasicCommonResponse(message);
        }

        // muduo库中没有的响应，如何处理
        void onUnknownMessage(const muduo::net::TcpConnectionPtr &conn, const MessagePtr &message, muduo::Timestamp)
        {
            LOG_INFO << "未知响应: " << message->GetTypeName() << "\n";
            conn->shutdown();
        }

        // 建立链接成功后的回调函数，连接建立成功后，唤醒connect()的阻塞
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                _latch.countDown(); // 连接成功唤醒上方阻塞
                _conn = conn;
            }
            else
            {
                // 连接关闭后的操作
                _conn.reset();
            }
        }

        muduo::CountDownLatch _latch;            // 协调多线程之间的同步操作，确保连接的成功（发起连接后，其他进程陷入等待；连接建立成功，其他进程结束等待）
        muduo::net::EventLoopThread _loopthread; // 异步循环控制，为了避免网络事件和其他异步任务阻塞主线程，通过EventLoopThread创建子线程，这个线程会不断地轮询 I/O 事件来进行事件监控，该线程实例化成功会自动运行事件监控，不需要手动运行
        muduo::net::TcpConnectionPtr _conn;      // 客户端对应的连接，当客户端连接成功后，会对其进行赋值
        muduo::net::TcpClient _client;           // 客户端
        ProtobufDispatcher _dispatcher;          // 响应分发器
        ProtobufCodecPtr _codec;                 // protobuf协议处理器

        AsyncWorker::ptr _worker;             // 我们自己的异步工作线程
        ChannelManager::ptr _channel_manager; // 信道管理类
    };
};