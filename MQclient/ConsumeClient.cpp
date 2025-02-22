// 订阅消息的消费者（订阅者）客户端
#include "Connection.hpp"

using namespace std;
using namespace ns_AsyncWorker;
using namespace ns_client_Channel;
using namespace ns_client_Connection;

void cb(Channel::ptr &channel, const string consumer_tag, const BasicProperties *bp, const string &body)
{
    lg.LogMessage(info, "正在进行消息处理……\n");
    lg.LogMessage(info, "%s 成功订阅了消息：%s\n", consumer_tag.c_str(), body.c_str());
    channel->BasicAckQueueMessage(bp->id()); // 确认消息
}

// 回调函数
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        lg.LogMessage(error, "输入错误，你应该这么输入：./ConsumeClient + 欲订阅的消息队列名");
        return -1;
    }
    // 1、实例化异步工作线程对象
    AsyncWorker::ptr worker = make_shared<AsyncWorker>();

    // 2、实例化连接对象
    Connection::ptr conn = make_shared<Connection>("127.0.0.1", 8085, worker);

    // 3、通过连接创建信道
    Channel::ptr channel = conn->OpenChannel();

    // 4、通过信道提供的服务完成所需
    //  4.1 声明一个交换机Exchange1，交换机类型为广播模式
    google::protobuf::Map<std::string, std::string> tmp_map;
    channel->DeclareExchange("exchange1", MQ_message::ExchangeType::topic, true, false, tmp_map);
    //  4.2 声明一个队列Queue1
    channel->DeclareQueue("queue1", true, false, false, tmp_map);
    //  4.3 声明一个队列Queue2
    channel->DeclareQueue("queue2", true, false, false, tmp_map);
    //  4.4 绑定queue1-exchange1，且binding_key设置为queue1
    channel->ExchangeQueueBinding("exchange1", "queue1", "queue1");
    //  4.5 绑定queue2-exchange1，且binding_key设置为news.music.#
    channel->ExchangeQueueBinding("exchange1", "queue2", "news.music.#");

    // 5、订阅消息
    auto functor = std::bind(cb, channel, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    channel->BasicConsume("consumer1", argv[1], false, functor);

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(3)); // 每三秒执行一次
    }
    conn->CloseChannel(channel);
    return 0;
}