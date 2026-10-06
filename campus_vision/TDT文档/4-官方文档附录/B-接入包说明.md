> ⬅ 返回目录：[README](../README.md)

## 附录 B：官方《选手 ROS 2 通信接入包》说明（`ros2_auto_framework/README.md` 全文）

> 位置：`~/下载/ros2_auto_framework/README.md`（与 `~/下载/campusgame_ros/ros2_auto_framework/README.md` 完全一致）

### 选手 ROS 2 通信接入包

```text
build.sh                编译
start.sh                启动 TCP 桥
src/tdt_interface/      模拟器消息定义
src/ros_tcp_endpoint/   Unity ROS TCP Endpoint（含许可证与来源）
```

环境：Ubuntu 24.04、ROS 2 Jazzy、`python3-colcon-common-extensions`，以及 ROS 的 `rosidl_default_generators`、`std_msgs`、`sensor_msgs`、`geometry_msgs`。

在本目录执行：

```bash
./build.sh
./start.sh
```

游戏通过 F8 启用 ROS，连接 `127.0.0.1:10000`。`Ctrl+C` 退出。需要更换监听地址或端口时：

```bash
TDT_ROS_IP=0.0.0.0 TDT_ROS_PORT=10000 ./start.sh
```

开发自己的节点时，在另一个终端加载消息环境：

```bash
source /opt/ros/jazzy/setup.bash
source install/local_setup.bash
# 然后编译或运行自己的 ROS 包
```

双方使用相同 `ROS_DOMAIN_ID`；启动脚本默认 DDS 本机发现，跨机器时自行设置 `ROS_AUTOMATIC_DISCOVERY_RANGE`。游戏与算法使用 UTC 时间戳，跨机器需同步系统时间。

| 话题 | 消息类型 | 方向 |
| --- | --- | --- |
| `/autoaim/frame` | `tdt_interface/msg/AutoAimFrame` | 游戏 → 算法：同帧图像、内参、自身状态 |
| `/navigation/enemies` | `std_msgs/msg/String` | 游戏 → 算法：导航敌情 JSON |
| `/game/rune/result` | `tdt_interface/msg/RuneResult` | 游戏 → 算法：符请求受理、增益激活事件 |
| `/target_speed_player_N` | `geometry_msgs/msg/TwistStamped` | 算法 → 游戏：底盘速度 |
| `/target_angles_player_N` | `tdt_interface/msg/SendData` | 算法 → 游戏：云台角度、射击许可 |
| `/game/rune/request` | `tdt_interface/msg/RuneRequest` | 算法 → 游戏：叫符请求 |

接入约定：

- N 由选手配置：1 蓝方、2 红方。状态与符消息不含玩家/会话标识；换局、换阵营时由算法清理任务状态。
- 图像和状态直接读 `AutoAimFrame`，其中 `image` 或 `compressed_image` 有且仅有一种载荷；内参为 `camera_info`，自身状态为 `state`。本包不拆分 `/autoaim/state` 或标准图像话题，不校验业务帧龄。
- 使用 RELIABLE/VOLATILE QoS，图像订阅推荐 depth=1。算法自行校验同帧时间戳、帧龄与序号，拒绝过期控制。
- `SendData.yaw` 为世界光轴角，Unity +Z 为零、顺时针正；`pitch` 向下正、范围 ±60 度；`if_shoot` 是射击许可。
- 底盘 `linear.x/y` 对应 Unity X/Z，`angular.z` 为绕 Unity +Y 的 rad/s。状态 `position/twist` 使用 FLU map：`(Unity Z, -Unity X, Unity Y)`；其 map Z 角速度与底盘指令旋转符号相反。距离与线速度使用场景单位，3 单位 = 1 米。
- `RuneRequest` 仅含 `header/request_id`，时间戳为请求创建时 UTC，ID 非零且单调递增。结果按 ID 关联；`accepted` 表示受理，`activated` 表示增益。游戏按实际开启次数选择“小、小、大、大、大”。
