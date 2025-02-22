### **消息发布者（生产者）流程总结**

1. **创建异步工作线程**

   - 实例化 `AsyncWorker` 以支持异步任务处理。

2. **建立连接**

   - 通过 `Connection` 类创建与消息队列服务器的连接（IP: `127.0.0.1`, 端口: `8085`）。

3. **创建信道**

   - 通过 `Connection` 的 `OpenChannel` 方法创建通信信道。

4. **配置交换机和队列**

   - **声明交换机** `exchange1`（类型为 `topic`，持久化 `true`）。

   - **声明队列** `queue1` 和 `queue2`（持久化 `true`）。

   - 绑定队列到交换机

     ：

     - `queue1` 绑定到 `exchange1`，`binding_key="queue1"`。
     - `queue2` 绑定到 `exchange1`，`binding_key="news.music.#"`（匹配 `news.music.` 开头的所有主题）。

5. **发布消息**

   - 向 

     ```
     exchange1
     ```

      发布多条消息：

     - `"news.music.pop"` → `"Hello World-0"`
     - `"news.music.sport"` → `"这是动感音乐新闻！"`
     - `"news.sport"` → `"这是运动新闻！"`

6. **关闭信道**

   - 调用 `conn->CloseChannel(channel)` 关闭信道，释放资源。

### **说明**

- **交换机类型**：采用 `topic` 模式，消息根据 `routing_key` 进行模糊匹配分发。

- 消息属性

  ：

  - `UUIDHelper::Getuuid()` 生成唯一 ID。
  - `DeliveryMode::durable` 设定消息为持久化。

- 不同模式测试

  ：

  - **广播模式（fanout）**：所有绑定的队列均接收消息。
  - **直接交换（direct）**：使用完全匹配的 `routing_key` 进行路由。
  - **主题交换（topic）**：支持通配符匹配（`*` 匹配一个单词，`#` 匹配多个单词）。