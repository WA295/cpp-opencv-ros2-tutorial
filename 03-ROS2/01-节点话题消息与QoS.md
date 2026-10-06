# ROS 2 节点、话题、消息与 QoS（C++ 详解）

> 这是 ROS 2 最核心的一篇：**怎么写一个节点、怎么收发消息、QoS 怎么选**。
> 对应头文件主要是 `#include <rclcpp/rclcpp.hpp>`。
> 每个 API 都给：**作用 → 参数 → 例子 → 什么时候用**。

---

## 目录

1. [最小节点模板](#1-最小节点模板)
2. [节点类 Node 的常用 API](#2-节点类-node-的常用-api)
3. [发布消息 create_publisher](#3-发布消息-create_publisher)
4. [订阅消息 create_subscription](#4-订阅消息-create_subscription)
5. [回调怎么写（bind 与 lambda）](#5-回调怎么写bind-与-lambda)
6. [消息怎么用（字段访问）](#6-消息怎么用字段访问)
7. [QoS 详解](#7-qos-详解)
8. [定时器 create_wall_timer](#8-定时器-create_wall_timer)
9. [参数 declare_parameter](#9-参数-declare_parameter)
10. [日志 RCLCPP_INFO](#10-日志-rclcpp_info)
11. [时间与时钟](#11-时间与时钟)
12. [执行器 spin 与多线程](#12-执行器-spin-与多线程)
13. [完整可运行示例](#13-完整可运行示例)
14. [速查表](#14-速查表)

---

## 1. 最小节点模板

**每个 ROS 2 C++ 节点都长这个样子：**

```cpp
#include <rclcpp/rclcpp.hpp>          // 一切的基础

class MyNode : public rclcpp::Node    // 继承 Node
{
public:
    MyNode() : Node("my_node")        // 给节点起名
    {
        RCLCPP_INFO(this->get_logger(), "节点启动了！");
        // 在这里创建订阅、发布、定时器……
    }
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);                       // 1. 初始化
    auto node = std::make_shared<MyNode>();         // 2. 创建节点
    rclcpp::spin(node);                             // 3. 开始转（等消息）
    rclcpp::shutdown();                             // 4. 关闭
    return 0;
}
```

**四步记忆：`init` → 建节点 → `spin` → `shutdown`。**

---

## 2. 节点类 Node 的常用 API

| 方法 | 作用 | 什么时候用 |
| --- | --- | --- |
| `this->get_logger()` | 获取日志器 | 想打日志时 |
| `this->create_publisher<T>(话题, qos)` | 创建发布者 | 要发消息 |
| `this->create_subscription<T>(话题, qos, 回调)` | 创建订阅者 | 要收消息 |
| `this->create_wall_timer(周期, 回调)` | 创建定时器 | 要按固定频率做事 |
| `this->create_service<T>(名字, 回调)` | 创建服务端 | 要提供"请求-应答" |
| `this->create_client<T>(名字)` | 创建服务客户端 | 要调用服务 |
| `this->declare_parameter(名, 默认值)` | 声明参数 | 想让参数可配置 |
| `this->get_parameter(名)` | 读参数 | 读取配置 |
| `this->now()` | 当前时间 | 打时间戳/计时 |
| `this->get_name()` | 节点名 | 日志里区分 |

---

## 3. 发布消息 create_publisher

```cpp
template<typename MessageT>
typename rclcpp::Publisher<MessageT>::SharedPtr
create_publisher(const std::string& topic_name, size_t qos_history_depth);
```

或完整 QoS 版本：

```cpp
create_publisher<MessageT>(topic_name, rclcpp::QoS(10));
```

**示例：**

```cpp
// 1. 声明成员变量（构造函数外也要用）
rclcpp::Publisher<tdt_interface::msg::SendData>::SharedPtr send_pub_;

// 2. 构造函数里创建
send_pub_ = this->create_publisher<tdt_interface::msg::SendData>(
    "/target_angles_player_1", 10);      // 话题名, 队列深度

// 3. 在需要的地方发布
tdt_interface::msg::SendData msg;
msg.yaw = 12.3f;
msg.pitch = -3.2f;
msg.if_shoot = true;
send_pub_->publish(msg);
```

| 参数 | 说明 |
| --- | --- |
| `topic_name` | 话题名，`/` 开头 |
| 第二个参数 | 队列深度（能缓冲几条），一般 10 |

**什么时候用：** 你有数据要"广播"出去时。

**要点：** 发布者是"发完不管"的，没人订阅也不会报错（只是白发）。

---

## 4. 订阅消息 create_subscription

```cpp
template<typename MessageT, typename CallbackT>
typename rclcpp::Subscription<MessageT>::SharedPtr
create_subscription(const std::string& topic_name,
                    const rclcpp::QoS& qos,
                    CallbackT&& callback);
```

**示例：**

```cpp
// 1. 成员变量
rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr image_sub_;

// 2. 构造函数里创建
image_sub_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
    "/camera/image_raw/compressed",       // 话题名
    1,                                    // 队列深度（图像用 1，延迟低）
    std::bind(&MyNode::imageCallback, this, std::placeholders::_1));

// 3. 回调（收到消息自动调用）
void imageCallback(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
{
    // msg->data 就是图像数据
}
```

| 参数 | 说明 |
| --- | --- |
| `MessageT` | 消息类型（尖括号里） |
| `topic_name` | 话题名 |
| qos/队列 | 缓冲几条；**图像建议 1** |
| callback | 收到消息后调用的函数 |

**什么时候用：** 你要用别人发的数据时。

---

## 5. 回调怎么写（bind 与 lambda）

### 5.1 std::bind（传统写法，本项目用的）

```cpp
#include <functional>

this->create_subscription<T>(topic, 1,
    std::bind(&MyNode::onMsg, this, std::placeholders::_1));
```

| 部分 | 含义 |
| --- | --- |
| `&MyNode::onMsg` | 要调用的成员函数 |
| `this` | 在哪个对象上调用 |
| `std::placeholders::_1` | "消息"参数位置（第一个参数） |

### 5.2 lambda（现代写法，更直观）

```cpp
this->create_subscription<T>(topic, 1,
    [this](const T::SharedPtr msg) {
        RCLCPP_INFO(this->get_logger(), "收到消息");
        // 直接用 msg
    });
```

**建议：** 只在一处用、逻辑短 → lambda；要在多个订阅复用 → 成员函数。

---

## 6. 消息怎么用（字段访问）

消息是"结构体"，用 `->` 访问字段：

```cpp
void frameCallback(const tdt_interface::msg::AutoAimFrame::SharedPtr msg)
{
    // 普通字段
    float t = msg->capture_to_publish_ms;

    // 嵌套消息（消息套消息）
    auto& state = msg->state;
    float yaw = state.yaw_degrees;

    // 数组字段
    int32_t h0 = state.enemy_healths[0];
    double fx = msg->camera_info.k[0];

    // 变长数组（vector）
    for (size_t i = 0; i < msg->camera_info.d.size(); i++) { ... }

    // 时间戳
    int32_t sec = msg->header.stamp.sec;
    uint32_t nsec = msg->header.stamp.nanosec;
}
```

| 访问方式 | 什么时候 |
| --- | --- |
| `msg->字段` | 消息是指针（`SharedPtr`） |
| `msg.字段` | 消息是对象（值） |
| `msg->header.stamp` | 时间戳 |
| `msg->字段[i]` | 数组 |

**注意：** 消息指针是**只读**的（`const`），要发消息得新建一个。

---

## 7. QoS 详解

**QoS（Quality of Service）= 通信规则。** 不匹配的话，两个节点可能"连上了但收不到"。

### 7.1 最重要的两个策略

| 策略 | 取值 | 含义 |
| --- | --- | --- |
| Reliability（可靠性） | `RELIABLE` | 保证送达（会重传，慢一点） |
| | `BEST_EFFORT` | 尽力而为（可能丢，快，适合图像流） |
| History（历史） | `KEEP_LAST` | 只保留最近 N 条 |
| | `KEEP_ALL` | 全部保留 |
| Depth（深度） | 整数 | `KEEP_LAST` 时保留几条 |

### 7.2 构造函数里的简写

```cpp
this->create_publisher<T>("/topic", 10);          // 队列深度 10，默认 RELIABLE
this->create_subscription<T>("/topic", 1, cb);    // 队列深度 1
```

### 7.3 完整写法

```cpp
#include <rclcpp/qos.hpp>

rclcpp::QoS qos(1);                               // 深度 1
qos.reliable();                                   // 可靠
// qos.best_effort();                             // 尽力
// qos.keep_last(1);                              // 保留最近 1 条
// qos.transient_local();                         // 新订阅者也能收到最后一条
// qos.durability_volatile();                     // 默认：不保留

this->create_subscription<T>("/topic", qos, cb);
```

### 7.4 预设 QoS（懒人包）

| 预设 | 特点 | 什么时候用 |
| --- | --- | --- |
| `rclcpp::QoS(10)` | 自定义 | 一般情况 |
| `rclcpp::SensorDataQoS()` | BEST_EFFORT + 深度 5 | **图像/点云等传感器流** |
| `rclcpp::SystemDefaultsQoS()` | RELIABLE + 深度 10 | 默认 |

### 7.5 怎么选

| 数据 | 建议 |
| --- | --- |
| 图像（高频、丢一帧没关系） | `SensorDataQoS()` 或 `depth=1` + best_effort |
| 控制指令（不能丢） | `RELIABLE` + `depth=10` |
| 状态（要最新的） | `RELIABLE` + `depth=1` |

> **校园赛约定：RELIABLE/VOLATILE，图像订阅 depth=1。**
> 两边 QoS 不兼容的典型症状：`ros2 topic list` 能看到话题，但 `echo` 不到数据。

### 7.6 查看 QoS

```bash
ros2 topic info /话题名 -v        # 显示发布者和订阅者的 QoS
```

---

## 8. 定时器 create_wall_timer

```cpp
rclcpp::TimerBase::SharedPtr
create_wall_timer(std::chrono::duration 周期, callback);
```

```cpp
#include <chrono>
using namespace std::chrono_literals;   // 让 100ms 这种写法可用

timer_ = this->create_wall_timer(100ms, [this]() {
    RCLCPP_INFO(this->get_logger(), "每 100ms 执行一次");
});
```

| 周期写法 | 含义 |
| --- | --- |
| `100ms` | 100 毫秒 |
| `1s` | 1 秒 |
| `10hz` | 每秒 10 次（等价 100ms） |

**什么时候用：**

- 周期性检查/发布（比如每 50ms 发一次控制指令）。
- 统计/上报（每秒打印一次状态）。

---

## 9. 参数 declare_parameter

```cpp
// 声明（带默认值）
this->declare_parameter("threshold", 100);
this->declare_parameter("max_speed", 1.5);

// 读取
int    t = this->get_parameter("threshold").as_int();
double s = this->get_parameter("max_speed").as_double();
std::string name = this->get_parameter("model_path").as_string();
bool   b = this->get_parameter("enable").as_bool();
```

命令行修改：

```bash
ros2 run 包名 节点名 --ros-args -p threshold:=150
ros2 param set /节点名 threshold 150     # 运行时改（如果节点支持动态参数）
ros2 param list
```

**什么时候用：** 参数会变化、不想重新编译时（阈值、路径、开关）。

---

## 10. 日志 RCLCPP_INFO

```cpp
RCLCPP_INFO(this->get_logger(), "普通信息：x=%f", x);
RCLCPP_WARN(this->get_logger(), "警告：%s", reason);
RCLCPP_ERROR(this->get_logger(), "错误！");
RCLCPP_DEBUG(this->get_logger(), "调试信息");
RCLCPP_FATAL(this->get_logger(), "致命错误！");
```

| 级别 | 什么时候用 |
| --- | --- |
| DEBUG | 开发细节（默认不显示） |
| INFO | 常规状态 |
| WARN | 可能有问题 |
| ERROR | 出错了 |
| FATAL | 致命，要退出 |

**格式化占位符：** `%d` 整数、`%f` 小数、`%s` 字符串、`%zu` size_t。

**查看某个级别的日志：**

```bash
ros2 run 包名 节点名 --ros-args --log-level debug
```

**要点：**

- 日志比 `cout` 好：带时间、级别、节点名，可以过滤。
- **高频日志要降频**（`if (++n % 100 == 0)`），否则拖慢程序。

---

## 11. 时间与时钟

```cpp
rclcpp::Time now = this->now();              // 当前 ROS 时间
int64_t ns = now.nanoseconds();              // 纳秒
double sec = now.seconds();                  // 秒

// 消息时间戳
msg.header.stamp = this->now();

// 计算两个时间差（毫秒）
auto dt = (this->now() - last_time).nanoseconds() / 1e6;   // ms
```

**为什么用 `this->now()` 而不是系统时间？**
因为 ROS 2 可能用仿真时间（`use_sim_time`），要用统一的"ROS 时钟"。

---

## 12. 执行器 spin 与多线程

### 12.1 单线程（默认，最常用）

```cpp
rclcpp::spin(node);       // 阻塞在这个循环里，直到 shutdown
```

**特点：** 所有回调**排队执行**。简单安全，但一个回调慢会堵住其他的。

### 12.2 多线程执行器

```cpp
#include <rclcpp/executors/multi_threaded_executor.hpp>

rclcpp::executors::MultiThreadedExecutor executor;
executor.add_node(node);
executor.spin();
```

**特点：** 多个回调可以并行。**代价**：共享数据要加锁。

### 12.3 手动 spin（想自己控制循环时）

```cpp
rclcpp::Rate rate(10);            // 10 Hz
while (rclcpp::ok()) {
    rclcpp::spin_some(node);      // 处理一下待办回调
    // 自己做别的事……
    rate.sleep();
}
```

**什么时候用：** 主循环要做别的事情（如 OpenCV 显示）时。

---

## 13. 完整可运行示例

**功能：** 订阅两个话题，把结果发布出去。

```cpp
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <tdt_interface/msg/auto_aim_frame.hpp>
#include <tdt_interface/msg/send_data.hpp>
#include <opencv2/opencv.hpp>
#include <functional>

class DemoNode : public rclcpp::Node
{
public:
    DemoNode() : Node("demo_node")
    {
        // 订阅图像
        image_sub_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
            "/camera/image_raw/compressed", 1,
            std::bind(&DemoNode::imageCallback, this, std::placeholders::_1));

        // 订阅状态
        frame_sub_ = this->create_subscription<tdt_interface::msg::AutoAimFrame>(
            "/autoaim/frame", 1,
            std::bind(&DemoNode::frameCallback, this, std::placeholders::_1));

        // 发布控制
        pub_ = this->create_publisher<tdt_interface::msg::SendData>(
            "/target_angles_player_1", 10);

        RCLCPP_INFO(this->get_logger(), "demo_node 启动");
    }

private:
    void imageCallback(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
    {
        cv::Mat img = cv::imdecode(msg->data, cv::IMREAD_COLOR);
        if (img.empty()) {
            RCLCPP_WARN(this->get_logger(), "解码失败");
            return;
        }

        // 模拟：只知道图像大小
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                             "收到图像 %dx%d", img.cols, img.rows);
    }

    void frameCallback(const tdt_interface::msg::AutoAimFrame::SharedPtr msg)
    {
        last_state_ = msg->state;      // 缓存自身状态
    }

    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr image_sub_;
    rclcpp::Subscription<tdt_interface::msg::AutoAimFrame>::SharedPtr frame_sub_;
    rclcpp::Publisher<tdt_interface::msg::SendData>::SharedPtr pub_;
    tdt_interface::msg::AutoAimState last_state_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<DemoNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
```

> `RCLCPP_INFO_THROTTLE(logger, clock, 毫秒, ...)`：**限频日志**，每隔指定毫秒最多打一条。
> 高频回调里打日志必备。

---

## 14. 速查表

| 我想…… | API |
| --- | --- |
| 建节点 | `class X : public rclcpp::Node` + `Node("名字")` |
| 初始化/关闭 | `rclcpp::init` / `rclcpp::shutdown` |
| 让程序跑起来 | `rclcpp::spin(node)` |
| 发消息 | `create_publisher<T>(话题, 队列)` + `publish(msg)` |
| 收消息 | `create_subscription<T>(话题, 队列, 回调)` |
| 绑定回调 | `std::bind(&X::cb, this, _1)` 或 lambda |
| 定时做事 | `create_wall_timer(100ms, cb)` |
| 配置参数 | `declare_parameter` / `get_parameter` |
| 打日志 | `RCLCPP_INFO/WARN/ERROR` |
| 限频日志 | `RCLCPP_INFO_THROTTLE` |
| 当前时间 | `this->now()` |
| 多线程 | `MultiThreadedExecutor` |
| 手动循环 | `spin_some` + `Rate` |

---

> 上一篇：[ROS 2 概念与工作空间](00-ROS2概念与工作空间.md)
> 下一篇：[ROS 2 常用命令行与调试](02-常用命令行与调试.md)
