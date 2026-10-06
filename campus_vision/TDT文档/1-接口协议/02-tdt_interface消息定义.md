> ⬅ 返回目录：[README](../README.md)

## 2. tdt_interface 消息定义（权威）

> **一句话**：这是游戏和你的程序约定好的 5 种"表格格式"，双方都按它来填数据，才看得懂对方。

包 `tdt_interface` 版本 `0.0.1`，Apache-2.0，依赖 `std_msgs / sensor_msgs / geometry_msgs / builtin_interfaces`。
共 5 个消息：

### AutoAimFrame（游戏 → 算法，同帧图像 + 内参 + 自身状态）
```
std_msgs/Header header
tdt_interface/AutoAimState state
sensor_msgs/CameraInfo camera_info
sensor_msgs/Image image
sensor_msgs/CompressedImage compressed_image
float32 capture_to_publish_ms
```
> `image` 与 `compressed_image` **有且仅有一种**有载荷。

### AutoAimState（自身状态 / 机器人状态）
```
std_msgs/Header header
uint64 frame_sequence
geometry_msgs/Point position
geometry_msgs/Twist twist
float32 yaw_degrees
float32 pitch_degrees
int32 health
int32 max_health
bool is_weakened
float32 projectile_speed
float32 remaining_seconds
int32[7] enemy_healths
int32 ammo_remaining
int32 ammo_capacity
```

### SendData（算法 → 游戏，云台角度 + 射击许可）
```
float32 yaw       # 世界光轴角(度): Unity +Z = 0，顺时针为正，与底盘解耦
float32 pitch     # 世界光轴俯仰(度): 向下为正，范围 [-60, 60]
bool if_shoot     # 射击许可，需配合云台对齐才真正开火
```

### RuneRequest（算法 → 游戏，叫符请求）
```
std_msgs/Header header
uint64 request_id
```

### RuneResult（游戏 → 算法，符受理/增益事件）
```
std_msgs/Header header
uint64 request_id
bool accepted
bool activated
```

### 字段速查（人话版）

| 字段 | 属于 | 大白话 |
| --- | --- | --- |
| `header` | 所有消息 | 消息的"抬头"：带时间戳和参考坐标系 |
| `frame_sequence` | AutoAimState | 第几帧，用来判断先后、有没有丢帧 |
| `position` / `twist` | AutoAimState | 自己的位置 / 速度 |
| `yaw_degrees` / `pitch_degrees` | AutoAimState | 自己云台**当前**朝向（左右 / 上下角度） |
| `health` / `max_health` | AutoAimState | 自己当前血量 / 满血值 |
| `is_weakened` | AutoAimState | 自己是否处于"被削弱"状态 |
| `projectile_speed` | AutoAimState | 弹丸速度（算提前量会用到） |
| `remaining_seconds` | AutoAimState | 本局剩余时间（秒） |
| `enemy_healths[7]` | AutoAimState | 7 个敌人的血量 |
| `ammo_remaining` / `ammo_capacity` | AutoAimState | 剩余弹药 / 弹匣容量 |
| `camera_info` | AutoAimFrame | 相机参数（内参 `k`、畸变 `d`），PnP 要用 |
| `image` / `compressed_image` | AutoAimFrame | 画面本身，**两种只会有一个有数据** |
| `capture_to_publish_ms` | AutoAimFrame | 这一帧从"拍下"到"发出"花了多少毫秒 |
| `state` | AutoAimFrame | 就是上面那份 AutoAimState（自己状态） |
| `yaw` / `pitch` | SendData | **你要告诉游戏的**目标角度（左右 / 上下） |
| `if_shoot` | SendData | 开火许可：true 表示"允许开火" |
| `request_id` | RuneRequest / RuneResult | 一次"叫符"请求的编号，用来把请求和结果对上 |
| `accepted` / `activated` | RuneResult | 请求被受理 / 增益已激活 |
