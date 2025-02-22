// 异步工作线程模块，不是只服务于某个连接/信道，可以应用于多个连接多个信道，因此单独进行封装
#pragma once
#include "muduo/net/EventLoopThread.h"
#include "../MQcommon/Log.hpp"
#include "../MQcommon/Helper.hpp"
#include "../MQcommon/ThreadPool.hpp"

using namespace ns_ThreadPool;

namespace ns_AsyncWorker
{
    class AsyncWorker
    {
    public:
        using ptr = shared_ptr<AsyncWorker>;
        muduo::net::EventLoopThread loopthread; // muduo库中客户端连接的异步循环线程EventLoopThread
        ThreadPool pool;                        // 收到消息后进行异步处理的工作线程池
    };
}