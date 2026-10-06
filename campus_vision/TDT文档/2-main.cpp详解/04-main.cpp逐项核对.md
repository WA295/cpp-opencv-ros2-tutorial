> ⬅ 返回目录：[README](../README.md)

## 4. main.cpp 逐项核对

> **一句话**：这一节把你的程序"干了什么"讲清楚，并逐项检查代码里用到的名字和官方接口对不对得上。
> 结论先给：**全部对得上，没有写错名字。**

源文件：`campus_ws/src/campus_vision/src/main.cpp`（ROS 节点 `campus_node`）
功能：订阅校园赛图像 → OpenCV 识别装甲板灯条 → 最小外接矩形/配对 → SVM 数字识别 → `solvePnP` 解算 → 发布 yaw/pitch。
此外还支持：**红蓝阵营（`team` 参数）**、**未找到目标时自动扫描**（正弦摆动 yaw）。

### 4.1 头文件与依赖（均正确）

| main.cpp 写法 | 核对结果 |
| --- | --- |
| `#include <tdt_interface/msg/auto_aim_frame.hpp>` | ✅ 与消息 `AutoAimFrame` 对应 |
| `#include <tdt_interface/msg/send_data.hpp>` | ✅ 与消息 `SendData` 对应 |
| `#include <sensor_msgs/msg/compressed_image.hpp>` | ✅ |
| `#include <rclcpp/rclcpp.hpp>`、OpenCV | ✅ |
| `#include <string>`、`<cctype>` | ✅ 新增：阵营字符串处理（`std::string` + `std::tolower`） |

### 4.2 订阅 / 发布（话题名与方向均正确）

| main.cpp | 类型 | 队列 | 核对 |
| --- | --- | --- | --- |
| 订阅 `/camera/image_raw/compressed` | `sensor_msgs/msg/CompressedImage` | 10 | ✅ 与 relay 输出一致（建议改 depth=1） |
| 订阅 `/autoaim/frame` | `tdt_interface::msg::AutoAimFrame` | 1 | ✅ 官方话题，队列深度符合建议 |
| 发布 `/target_angles_player_` + `player_id_` | `tdt_interface::msg::SendData` | 10 | ✅ **按阵营动态拼接**：blue→`_1`、red→`_2` |

> 发布器命名 `send_data_pub_`，订阅器 `image_sub_`、`frame_sub_`，状态成员 `last_state_`（`AutoAimState`），内参 `cameraMatrix_`、畸变 `distCoeffs_`。
>
> **阵营机制（新增）**：构造函数用 `declare_parameter<std::string>("team","blue")` 读阵营，转小写后设置 `player_id_`（蓝=1、红=2），发布话题名就是这么拼出来的。运行时用 `-p team:=red` 即可切换，**无需改代码**。

### 4.3 消息字段使用核对

| main.cpp 访问 | 消息中字段 | 核对 |
| --- | --- | --- |
| `msg->camera_info.k[0..8]` | `sensor_msgs/CameraInfo.k` | ✅ 9 元素内参矩阵 |
| `msg->camera_info.d` | `sensor_msgs/CameraInfo.d` | ✅ 畸变系数 |
| `msg->state` | `AutoAimFrame.state` ∈ `AutoAimState` | ✅ |
| `last_state_ = msg->state` | — | ✅ 保存自身状态 |
| `last_state_.yaw_degrees` | `AutoAimState.yaw_degrees` | ✅ 字段名存在 |
| `send_msg.yaw` | `SendData.yaw` | ✅（在 `publishAim` 内赋值） |
| `send_msg.pitch` | `SendData.pitch` | ✅（在 `publishAim` 内赋值） |
| `send_msg.if_shoot` | `SendData.if_shoot` | ✅（参数 `shoot`：瞄准=true，扫描=false） |

> **新增封装**：发送逻辑统一收进 `publishAim(world_yaw_deg, pitch_deg, shoot)`，
> 它在发布前把 `world_yaw_deg` 循环规范到 `[-180, 180]`，避免 yaw 连续累加越绕越大。

**结论：`main.cpp` 用到的消息类型名、话题名、字段名与官方 `tdt_interface` 完全一致，无拼写/命名错误。**

### 4.4 数据流

```
/camera/image_raw/compressed ──► imageCallback ──imdecode──► processFrame
                                                                 │
                            通道分离 → R-B 差值 → 高斯模糊 → 二值化 → 轮廓
                                                                 │
                            筛选灯条 → 灯条配对 → Armor → 数字 ROI → SVM
                                                                 │
                                             solvePnP(cameraMatrix_, distCoeffs_)
                                                                 │
                              yaw/pitch → publishAim(true) ──► /target_angles_player_N
                                                                 │
                             没瞄到目标 → publishAim(scan, false)（正弦摆动搜索）
/autoaim/frame ──► frameCallback ──► cameraMatrix_ / distCoeffs_ / last_state_
```
