// 发布消息的生产者（发布者）客户端
#include "Connection.hpp"

using namespace std;
using namespace ns_AsyncWorker;
using namespace ns_client_Channel;
using namespace ns_client_Connection;

int main()
{
    // 1、实例化异步工作线程对象
    AsyncWorker::ptr worker = make_shared<AsyncWorker>();

    // 2、实例化连接对象
    Connection::ptr conn = make_shared<Connection>("127.0.0.1", 8085, worker);

    // 3、通过连接创建信道
    Channel::ptr channel = conn->OpenChannel();

    // 4、通过信道提供的服务完成所需
    //  4.1 声明一个交换机Exchange1，声明交换机类型
    google::protobuf::Map<std::string, std::string> tmp_map;
    // 广播模式测试  channel->DeclareExchange("exchange1", MQ_message::ExchangeType::fanout, true, false, tmp_map);
    // 直接交换模式测试  channel->DeclareExchange("exchange1", MQ_message::ExchangeType::direct, true, false, tmp_map);
    channel->DeclareExchange("exchange1", MQ_message::ExchangeType::topic, true, false, tmp_map);
    //  4.2 声明一个队列Queue1
    channel->DeclareQueue("queue1", true, false, false, tmp_map);
    //  4.3 声明一个队列Queue2
    channel->DeclareQueue("queue2", true, false, false, tmp_map);
    //  4.4 绑定queue1-exchange1，且binding_key设置为queue1
    channel->ExchangeQueueBinding("exchange1", "queue1", "queue1");
    //  4.5 绑定queue2-exchange1，且binding_key设置为news.music.#
    channel->ExchangeQueueBinding("exchange1", "queue2", "news.music.#");
    // 5、循环向交换机发布消息
    for (int i = 0; i < 1; i++)
    {
        /*
            广播模式测试
            channel->BasicPublishMessage("exchange1", nullptr, "Hello World-" + to_string(i));
        */
        BasicProperties bp;
        bp.set_id(UUIDHelper::Getuuid());
        bp.set_delivery_mode(DeliveryMode::durable);
        bp.set_routing_key("news.music.pop");
        // 直接交换模式测试  bp.set_routing_key("queue1");
        channel->BasicPublishMessage("exchange1", &bp, "Hello World-" + std::to_string(i));
    }

    BasicProperties bp;
    bp.set_id(UUIDHelper::Getuuid());
    bp.set_delivery_mode(DeliveryMode::durable);
    bp.set_routing_key("news.music.sport");
    channel->BasicPublishMessage("exchange1", &bp, "这是动感音乐新闻！");

    bp.set_routing_key("news.sport");
    channel->BasicPublishMessage("exchange1", &bp, "这是运动新闻！");
    // 6、关闭信道
    conn->CloseChannel(channel);
    return 0;
}