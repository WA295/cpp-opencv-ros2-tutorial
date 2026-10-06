> ⬅ 返回目录：[README](../README.md)

## 3. 话题协议、坐标与 QoS

> **一句话**：这是游戏和你的程序之间收发的"频道清单"，以及坐标单位、收发规则的说明。

**怎么读这张表？**

- **话题**：频道名，前面带 `/`。游戏和你的程序就往这些频道收发数据。
- **类型**：这个频道里消息的"格式"（就是第 2 节那 5 张表之一）。
- **方向**：谁发给谁——「游戏 → 算法」= 游戏发、你收；「算法 → 游戏」= 你发、游戏收。

**QoS 是什么？** 就是收发消息的"规则"，其中两个最重要：

| 开关 | 取值 | 大白话 |
| --- | --- | --- |
| 可靠性 Reliability | `RELIABLE` / `BEST_EFFORT` | 一定要送到 / 尽力而为（可能丢） |
| 队列深度 depth | 整数 | 最多先攒几条；攒太多延迟高，太少会丢 |

> 官方约定用 `RELIABLE` + 图像订阅 `depth=1`（图像变得快，攒 1 条就够）。

| 话题 | 类型 | 方向 |
| --- | --- | --- |
| `/autoaim/frame` | `tdt_interface/msg/AutoAimFrame` | 游戏 → 算法：**同帧**图像+内参+自身状态 |
| `/autoaim/state` | `tdt_interface/msg/AutoAimState` | 内置 Relay 从 frame 拆出 |
| `/camera/image_raw/compressed` | `sensor_msgs/msg/CompressedImage` | 内置 Relay 拆出的标准 JPEG（960×720） |
| `/navigation/enemies` | `std_msgs/msg/String` | 游戏 → 算法：导航敌情 JSON |
| `/game/rune/result` | `tdt_interface/msg/RuneResult` | 游戏 → 算法 |
| `/game/rune/request` | `tdt_interface/msg/RuneRequest` | 算法 → 游戏 |
| `/target_speed_player_N` | `geometry_msgs/msg/TwistStamped` | 算法 → 游戏：底盘速度 |
| `/target_angles_player_N` | `tdt_interface/msg/SendData` | 算法 → 游戏：云台角度+射击许可 |

- **N = 1 蓝方，N = 2 红方**（由算法配置，不属于消息内容）。
- QoS：RELIABLE / VOLATILE；**图像订阅建议 depth=1**。
- 时间戳为 **UTC**，跨机需校时；`frame_sequence` 用于同帧/乱序校验。
- 距离与线速度单位：**3 场景单位 = 1 米**。
- 姿态用 FLU map：`(Unity Z, -Unity X, Unity Y)`，其 map Z 角速度与底盘指令旋转符号相反。
- 底盘 `linear.x/y` 对应 Unity X/Z，`angular.z` 绕 Unity +Y（rad/s）。
- 进入对局后才有图像；菜单阶段没有比赛图像。
