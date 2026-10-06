# T-DT 校园赛 2027 · 接口协议与 campus_node(main.cpp) 说明

> 本文档由官方游戏文件（`/opt/tdt-campus-game`、`/usr/share/tdt-campus-game`、
> `~/下载/ros2_auto_framework/README.md`）与本工作空间 `campus_ws` 的实际代码比对整理而成。
> 目的：把「官方接口协议」和「`main.cpp` 里到底用了哪些名字、字段是否正确」放在一起，方便查用。

---

## 0.0 main.cpp 全部编写逻辑（先看这个）

> 这一节把 `main.cpp` 从头到尾讲一遍，看完你就知道"代码为什么这么写、先写什么后写什么"。
> 后面第 4 节的变量表 / 函数表，是这一节的"查字典"。

### 一、程序全貌：其实就 5 块

`main.cpp` 从上到下就是这 5 块，照着顺序读就行：

| 顺序 | 内容 | 大致行号 | 作用 |
| --- | --- | --- | --- |
| 1 | 头文件 + `using` | 1–16 | 引入 ROS 2、消息、OpenCV、`<string>`/`<cctype>` 等标准库 |
| 2 | 全局参数 + 阵营 + 扫描 | 72–102 | 二值化阈值、灯条面积/角度、装甲板真实尺寸；阵营常量；自动扫描参数 |
| 3 | 两个结构体 | 42–60 | `lightbar`（灯条）、`Armor`（装甲板），用来"装数据" |
| 4 | 一个小工具函数 | 62–68 | `pointdistance`：算两点之间的距离 |
| 5 | 核心类 `CampusNode` | 104–1052 | 节点的全部逻辑（阵营识别 / 构造 / 收图 / 收状态 / 图像算法 / 发布） |
| 6 | `main` 函数 | 1055–1076 | 启动：初始化 → 建节点 → 转起来 → 关闭 |

> 记忆口诀：**先定参数和结构，再写节点类，最后 main 启动它。**

### 二、核心流程：从"收到一张图"到"发出瞄准指令"

这是整个程序最关键的 11 步，都在 `processFrame()` 里（`imageCallback` 收到图后调用它）：

```
收到一张游戏画面（JPEG 解码后）
   │
1. 每 3 帧才处理 1 帧           —— 省 CPU，不然太卡
2. 读取滑条上的参数             —— 现场手动调参用
3. 拆成 B/G/R 三通道，算"红−蓝"  —— 红色发光区会变亮，方便找灯条
4. 高斯模糊                     —— 去掉画面噪点
5. 二值化（变黑白）             —— 只留下最亮的红色区域
6. 找轮廓                       —— 得到一个个候选形状
7. 逐个轮廓筛"灯条"             —— 面积、宽高比、倾角都符合的才算灯条
8. 两两配对成"装甲板"           —— 两根差不多高、大致平行、距离合适的灯条
9. PnP 算目标位姿               —— 算出目标离相机多远、偏多少（要用相机内参）
10. 算出角度并发布              —— 把 yaw / pitch / 开火许可发给游戏
11. 截数字区域 + SVM 认数字 + 画框显示
```

每一步的"为什么 + 怎么做 + 以后怎么复用"：

| 步骤 | 为什么这么做 | 怎么做到的（用哪个函数 / 写法） | 以后遇到同类问题怎么办 |
| --- | --- | --- | --- |
| 1 每 3 帧处理 1 帧 | 识别很耗 CPU，隔帧处理能在不影响体验的前提下减负 | 在 `processFrame` 开头写 `static int frameCount=0; frameCount++; if(frameCount%3!=0) return;`——**静态计数器 + 取模 `%`**，不满足就 `return` 跳过 | 任何"降频 / 抽样处理"都用这套：计数器 + 取模 + `return`。`%2`=处理一半、`%5`=处理 1/5；配合 `ros2 topic hz` 看真实频率再调 |
| 2 读滑条参数 | 光照一变参数就要调，滑条能边看边调、不用重编译 | 构造函数里 `cv::namedWindow("参数")` + `cv::createTrackbar(...)` + `cv::setTrackbarPos(...)` 建好；每帧用 `cv::getTrackbarPos("threshold","参数")` 读回全局变量 | 要"运行时调参"就用滑条；更规范可用 ROS 参数 `declare_parameter` / `get_parameter`（也是改参数不重编译） |
| 3 红−蓝差值 | 红色灯条的红通道远大于蓝通道，相减后背景被压掉，灯条很突出 | `cv::split(frame, channels)` 拆通道 → `cv::subtract(channels[2], channels[0], red_diff)`。**OpenCV 是 B,G,R**，所以 `[2]`=红、`[0]`=蓝 | 想突出某种颜色就"目标通道 − 干扰通道"；蓝色目标反过来 `[0]-[2]`；也可以用 `cv::cvtColor` 转 HSV 后 `cv::inRange` 选色 |
| 4 高斯模糊 | 差值图有噪点，模糊一下让后面二值化更干净 | `cv::GaussianBlur(red_diff, red_diff, Size(9,9), 0)`；核必须是奇数，越大越糊 | 图有噪点就上它；想更平滑把核加大（11、15），想保细节把核减小（3、5） |
| 5 二值化 | 把"亮/暗"变成"白/黑"，轮廓检测只认黑白图 | `cv::threshold(red_diff, thre, thresholdValue, 255, THRESH_BINARY)` | 按阈值分黑白都用它；`THRESH_BINARY`=大于阈值变白；想自动定阈值用 `THRESH_OTSU`，想反色用 `THRESH_BINARY_INV` |
| 6 找轮廓 | OpenCV 只能处理黑白图形的边界，这一步把亮块变成"形状" | `cv::findContours(thre, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE)`；`RETR_EXTERNAL` 只取最外层 | 从黑白图提形状都用它；要嵌套轮廓改 `RETR_TREE`；之后常接 `cv::contourArea` / `cv::boundingRect` / `cv::minAreaRect` |
| 7 筛灯条 | 灯条是细长的，用几何特征过滤掉地面等杂物 | 每个轮廓用 `cv::contourArea` + `cv::minAreaRect` 算面积/宽高/倾角，再 `if(height/width>1.5 && area>minAreaValue && area<maxAreaValue && abs(lb.angle)<maxAngleValue)`；宽高归一用 `std::swap` | "按特征筛目标"的通用套路：先算特征（面积/长宽比/角度），再写一串 `if`；阈值都做成可调滑条 |
| 8 配装甲板 | 一块装甲板就是左右两根灯条 | 双重 `for` 两两组合，用 `pointdistance` 算距离，比较高度差/角度差/高度比/中心连线角，满足就组成 `Armor`；用 `center.x` 判断谁左谁右 | "两两配对"通用 = 嵌套循环 + 一组相似度条件；只要最像的一对就取最优，目标多时先按 `pointdistance` 排序 |
| 9 PnP | 只知道画面位置不够，还要知道"多远" | 把 4 角点按固定顺序列成 3D 点 `objectPoints`（填真实尺寸），调 `cv::solvePnP(objectPoints, imagePoints, cameraMatrix_, distCoeffs_, rvec, tvec, false, SOLVEPNP_IPPE)`，`tvec` 就是相机系下位置；前提是内参 `cameraMatrix_`/畸变 `distCoeffs_` 已从 `camera_info` 取到 | 已知真实尺寸 + 图像点想求位姿就用 `solvePnP`；平面目标用 `SOLVEPNP_IPPE`；**3D 点与 2D 点顺序必须一一对应**，否则结果全错 |
| 10 算出角度并发布 | 游戏只认 `SendData` | `atan2(tx,tz)*180/CV_PI` 得 yaw、`atan2(ty,tz)*180/CV_PI` 得 pitch，`world_yaw=last_state_.yaw_degrees+yaw_deg`；填 `send_msg.yaw/pitch/if_shoot` 后 `send_data_pub_->publish(send_msg)` | "把结果发出去"= 有 publisher → 填消息字段 → `publish()`；角度用 `atan2`（全象限都稳，比 `acos`/`asin` 好） |
| 11 截数字 + 认数字 + 画框 | 认敌人编号，并画出来方便肉眼自检 | `cv::boundingRect` 框角点 → 按比例扩展 → `frame(numberRect)` 抠 ROI → `cvtColor` 灰度 → `resize(20,28)` → `convertTo` 归一化 → `reshape` 展平 → `svm_->predict(feature)`；画图用 `circle`/`polylines`/`putText`/`imshow`/`waitKey` | 用模型分类的套路：抠 ROI → 统一尺寸 → 归一化 → 展平 → `predict`；训练见 `svm.py`，模型存 `.yml`，启动时 `SVM::load`；画完别忘了 `imshow` + `waitKey` 才会刷新窗口 |

### 三、两路消息，各干各的

程序其实**同时在收两种消息**：

| 收的消息 | 谁处理 | 干了什么 |
| --- | --- | --- |
| `/camera/image_raw/compressed`（画面） | `imageCallback` | 解码画面 → 调用 `processFrame` 做识别 |
| `/autoaim/frame`（自身状态 + 相机参数） | `frameCallback` | 把相机内参、畸变、自身姿态存到成员变量里备用 |

为什么分两路？因为**图像处理慢**（每 3 帧才处理 1 帧），而相机参数、自身姿态每帧都要。
所以先把状态缓存进 `cameraMatrix_`、`distCoeffs_`、`last_state_`，识别时直接拿来用，不用等。

- **怎么做到的**：用两次 `create_subscription` 分别建图像订阅和状态订阅，各自用 `std::bind` 绑定一个回调（`imageCallback` / `frameCallback`）；回调里把数据存进成员变量，另一个回调再读。
- **以后遇到同类问题**：把"慢的重活"和"快的状态更新"拆成不同的订阅 + 回调，用成员变量做缓存。如果状态更新必须实时，可给 `executor` 开多线程，或把识别放到单独线程，别堵住状态回调。

> ⚠️ 注意：`rclcpp::spin` 默认是**单线程**，两个回调不会真的同时跑，而是排队执行。
> 好处是共享变量基本安全；坏处是图像处理会"堵住"状态回调。这是可以优化的点（见第 7 节）。

### 四、代码从上到下逐段读

| 段 | 行号 | 在干嘛 |
| --- | --- | --- |
| 顶部 | 1–16 | 引入 `rclcpp`、`tdt_interface` 消息、OpenCV、`<cmath>`、`<string>`、`<cctype>` 等 |
| 结构体 | 42–60 | `lightbar` 存一根灯条（中心/宽高/倾角/上下中点/四角）；`Armor` 存一块装甲板（中心/四角/左右灯条/识别数字） |
| 工具函数 | 62–68 | `pointdistance(p1,p2)` 返回两点距离，配对时反复用到 |
| 全局参数 | 72–81 | 阈值、面积上下限、角度上限、装甲板真实宽高（**可调**） |
| 阵营配置 | 83–89 | `PLAYER_ID_BLUE=1` / `PLAYER_ID_RED=2` 常量 |
| 自动扫描参数 | 91–102 | `scan_enable_`、摆幅/周期/超时/俯仰（**可运行时覆盖**） |
| 构造函数 | 108–287 | ①加载 SVM ②读 `team` 参数定阵营 ③读扫描参数 ④建窗口+滑条 ⑤建 2 个订阅 ⑥建 1 个发布（话题按阵营动态） |
| `processFrame` | 291–947 | 识别 11 步 + PnP 瞄准发布 + 自动扫描 |
| `publishAim` | 985–996 | 统一发布瞄准/扫描指令，把 yaw 规范到 [-180,180] |
| `frameCallback` | 999–1021 | `camera_info.k` → `cameraMatrix_`；`d` → `distCoeffs_`；`state` → `last_state_` |
| `imageCallback` | 1023–1051 | `imdecode` 解码画面，交给 `processFrame` |
| `main` | 1055–1076 | ROS 2 标准启动模板：`init` → 建节点 → `spin` → `shutdown` |

### 五、记住这几句就够

- **收**用 `create_subscription`，**发**用 `create_publisher`。
- 收到消息后**自动执行**的那段代码叫"回调"（`imageCallback` / `frameCallback`）。
- `spin` 让程序一直挂着等消息，不写它程序会立刻结束。
- 识别算法全在 `processFrame`，主线是：**颜色 → 二值化 → 找轮廓 → 配对 → PnP → 发布**。

### 六、同类问题"万能套路"（以后照这个套）

把上面 11 步提炼成可复用的模板，以后遇到相似需求直接套：

| 我想解决的问题 | 万能套路 | 用到的东西 |
| --- | --- | --- |
| 处理太慢，想降频 | 计数器 + 取模 + `return` | `static int n; if(++n%N) return;` |
| 想运行时调参、不重编译 | 滑条读值 / ROS 参数 | `createTrackbar` + `getTrackbarPos`，或 `declare_parameter` |
| 想突出某种颜色 | 目标通道 − 干扰通道，或 HSV 阈值 | `split` + `subtract`，或 `cvtColor` + `inRange` |
| 图有噪点 | 先模糊再处理 | `GaussianBlur` |
| 想把图分成"要/不要" | 阈值二值化 | `threshold`（可换 `THRESH_OTSU`） |
| 想从黑白图提取形状 | 找轮廓 → 算特征 | `findContours` + `contourArea` / `boundingRect` / `minAreaRect` |
| 想按几何特征筛目标 | 算特征 + 一串 `if` | 面积、长宽比、角度 |
| 想把两个东西配对 | 嵌套循环 + 距离/相似度条件 | `pointdistance` |
| 想求目标距离和角度 | PnP（真实尺寸 + 相机内参） | `solvePnP` + `atan2` |
| 想把结果发给游戏 | 建 publisher → 填消息 → 发布 | `create_publisher` + `publish` |
| 想用模型认东西 | 抠 ROI → 统一尺寸 → 归一化 → 展平 → 预测 | `boundingRect` + `resize` + `convertTo` + `reshape` + `predict` |
| 想显示结果 | 画 → 显示 → 刷新 | `circle`/`polylines`/`putText` + `imshow` + `waitKey` |

---

## 0. 文档范围

- 官方游戏安装内容与文档位置
- `tdt_interface` 5 个消息的权威定义
- ROS 2 话题协议、坐标系与 QoS 约定
- `main.cpp` 逐项核对（节点名、话题名、消息名、字段名）
- 与 `main.cpp` 同一包内的配套文件（CMakeLists / package.xml / digit_svm.yml / svm.py / five.cpp / .vscode）
- 编译运行步骤、注意事项与排障

---

## 0.1 先读这段：大白话导读

> 如果你是第一次接触这个项目，先花两分钟把这一节看完，后面的表格就好懂了。

### 这套东西到底在干嘛？

想象一个机器人射击游戏：

- **游戏**（T-DT 校园赛）= 一个 3D 射击游戏，你操控一个机器人，它自带一个"眼睛"（摄像头画面）。
- 游戏允许你把"眼睛看到的东西"交给**自己的程序**去看；程序算好该往哪打之后，再把"往哪打、开不开火"告诉游戏。
- **你的程序**就是 `campus_ws` 里的 `campus_node`（`main.cpp`），它负责：**看图 → 找敌人的装甲板 → 算角度 → 下命令。**

一句话：**游戏给你画面，你给游戏角度。**

### 三个角色

| 角色 | 是什么 | 通俗理解 |
| --- | --- | --- |
| 游戏 | `tdt-campus-game`（已安装） | 出题人：发画面、收你的瞄准指令 |
| 你的节点 | `main.cpp` 编译出的 `campus_node` | 答题人：看图找目标、算角度 |
| ROS 2 | 系统自带的通信框架 | 邮局：帮双方收发消息 |

### 数据怎么流动（用"送快递"打比方）

- **话题(topic)** = 一个有名字的"邮箱/频道"，例如 `/camera/image_raw/compressed`。
- **消息(message)** = 一封封"包裹"，格式固定（像一张有固定栏目的表格）。
- **节点(node)** = 收发包裹的"人"，也就是一个程序。
- **订阅(subscribe)** = "这个邮箱我订了，有包裹就通知我"。
- **发布(publish)** = "我往这个邮箱里投一个包裹"。

所以：

```
游戏  ──投递画面──►  /camera/image_raw/compressed  ──►  你的程序（订阅）
游戏  ──投递状态──►  /autoaim/frame                ──►  你的程序（订阅）
你的程序 ──投递角度/开火──►  /target_angles_player_1  ──►  游戏（订阅）
```

### 你的 main.cpp 一句话流程

1. **收图**：从 `/camera/image_raw/compressed` 收一张张游戏画面（JPEG）。
2. **找灯条**：装甲板左右各有一根发光的"灯条"。程序先按颜色（红 − 蓝差值）把发亮的长条找出来。
3. **配成装甲板**：两根**靠得近、差不多高、大致平行**的灯条，就认为是一块装甲板。
4. **认数字**：装甲板上有数字（敌方编号），用 **SVM**（一个提前训练好的小分类器）认出来是 1~8 几号。
5. **算角度**：知道了装甲板在画面里的位置、再加上相机参数，用 **PnP** 算出它相对相机偏了多少、有多远。
6. **下命令**：把 **yaw（左右角度）** 和 **pitch（上下角度）** 打包成 `SendData` 发给游戏，并说"可以开火"。

### 名词小词典（遇到不懂就回来查）

| 名词 | 大白话 |
| --- | --- |
| ROS 2 | 一套让不同程序互相收发消息的"邮局系统" |
| 节点 node | 一个负责收发消息的程序 |
| 话题 topic | 消息的"频道/邮箱"，用 `/名字` 表示 |
| 消息 message | 一次发送的数据，格式固定（像一张表格） |
| 订阅 subscribe | 关注某个频道，有新消息就通知我 |
| 发布 publish | 往某个频道发消息 |
| 回调 callback | "有消息来了，就自动执行的那段代码" |
| QoS | 收发消息的"质量规则"（可靠/尽力、保留最近几条） |
| yaw / pitch | 左右转的角度 / 上下抬的角度 |
| 相机内参 K | 相机的"度数"，用来把画面像素换算成真实角度 |
| 畸变系数 D | 镜头让画面变形的程度，用来把画面修正回正常 |
| PnP | 已知物体的真实尺寸和它在画面里的位置，反推它离你多远、偏多少 |
| SVM | 一个提前训练好的小分类器（这里用来认数字 1~8） |
| ROI | 画面里"我关心的那一小块区域" |
| 灯条 | 装甲板两侧的发亮长条 |
| 装甲板 | 敌人身上贴着数字的板子，两侧各有一根灯条 |
| `ROS_DOMAIN_ID` | "电台频道号"，游戏和程序必须一样才能通话 |
| `tdt_interface` | 官方给的消息定义包，约定双方用什么格式说话 |

> 下面第 1~3 节是"背景知识"，第 4 节是"你的代码逐行对照"，第 5~8 节是"怎么跑、注意什么"，最后的附录是"官方原文"。

---

## 1. 官方游戏与运行环境

已安装版本 **1.1.0**，目标 Ubuntu 24.04 amd64，内置 **ROS 2 Jazzy + Python 3.12**。

| 路径 | 内容 |
| --- | --- |
| `/opt/tdt-campus-game/` | Unity 游戏本体、内置 ROS/Python 运行库、通信桥脚本 |
| `/usr/bin/tdt-campus-game` | 启动命令入口 |
| `/usr/share/applications/tdt-campus-game.desktop` | 应用菜单「T-DT 校园赛 2027」 |
| `/usr/share/tdt-campus-game/tdt_interface/` | **接口源码**（供算法端编译） |
| `/usr/share/doc/tdt-campus-game/DEBIAN_RELEASE.md.gz` | 官方发行说明（主要文档） |
| `~/.local/state/tdt-campus-game/logs/` | 每次启动一个日志目录（launcher/endpoint/relay/unity.log） |

**内置通信桥**（`/opt/tdt-campus-game/scripts/`）自动完成，算法端无需启动：

- `AppRun` → `ros-runtime/run-python scripts/appimage/launch.py`
- `launch.py`：预留端口（默认 `127.0.0.1:10000`，被占用自动换）→ 启动 Unity + `endpoint.py` + `relay.py`
- `endpoint.py`：Unity 端 ROS-TCP-Endpoint
- `autoaim_relay.py`：校验 `/autoaim/frame` 并拆出 `/autoaim/state` 和标准图像
- `run-python`：只在子进程隔离设置 ROS 环境，**不污染算法终端的系统 ROS**

**相关环境变量 / 参数**

| 名称 | 默认 | 说明 |
| --- | --- | --- |
| `ROS_DOMAIN_ID` | `0` | 游戏与算法必须一致 |
| `TDT_ROS_PORT` | `10000` | 桥监听端口 |
| `TDT_ROS_IP` | `127.0.0.1` | 监听地址 |
| `TDT_SENSOR_FPS` | `120` | 图像采集目标频率（示例，实际受显卡/负载影响） |

启动示例：`ROS_DOMAIN_ID=7 TDT_SENSOR_FPS=60 tdt-campus-game`
游戏内 **F8** 查看连接状态；卸载保留用户设置与日志；**不要用 sudo 启动**。

---

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

---

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

---

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

---

## 4.5 main.cpp 变量清单

> **一句话**：把代码里出现的所有"变量"列出来，告诉你每个存的是什么、在哪一行。
> 下表按作用域整理 `campus_ws/src/campus_vision/src/main.cpp` 中出现的**全部变量**（行号对应当前版本）。
> 标 ⭐ 的变量与 `tdt_interface` 接口直接相关。

### A. 文件级全局变量（可调参数 + 新增配置）

| 变量名 | 类型 | 行 | 作用 |
| --- | --- | --- | --- |
| `thresholdValue` | `int` | 72 | 二值化阈值，初值 100 |
| `minAreaValue` | `int` | 73 | 灯条最小面积，初值 70 |
| `maxAreaValue` | `int` | 74 | 灯条最大面积，初值 5000 |
| `maxAngleValue` | `int` | 75 | 灯条最大允许倾角，初值 30 |
| `maxCenterAngleValue` | `int` | 76 | 两灯条中心连线最大允许角度，初值 20 |
| `maxDisRatioValue` | `double` | 77 | 灯条间距/高度比例上限，初值 4.0 |
| `ARMOR_WIDTH_M` | `const double` | 80 | 装甲板两灯条中心水平距离(米)，估算 0.135 |
| `ARMOR_HEIGHT_M` | `const double` | 81 | 灯条上下端中点距离(米)，估算 0.055 |
| `PLAYER_ID_BLUE` | `const int` | 88 | 蓝方 player 编号 = 1（**新增**） |
| `PLAYER_ID_RED` | `const int` | 89 | 红方 player 编号 = 2（**新增**） |
| `scan_enable_` | `bool` | 98 | 是否开启自动扫描，初值 `true`（**新增**） |
| `scan_amplitude_deg_` | `double` | 99 | 扫描左右摆幅(度)，初值 45.0 |
| `scan_period_s_` | `double` | 100 | 一次"左—右—回中"周期(秒)，初值 4.0 |
| `scan_timeout_s_` | `double` | 101 | 连续多久没瞄准就扫描(秒)，初值 0.5 |
| `scan_pitch_deg_` | `double` | 102 | 扫描时的云台俯仰(度)，初值 0.0 |

> 这些扫描参数虽然是全局变量，但构造函数里用 `declare_parameter` 读取允许**运行时覆盖**（见 D 组）。

### B. 自定义数据结构

**`struct lightbar`（灯条，42–51 行）**

| 成员名 | 类型 | 作用 |
| --- | --- | --- |
| `center` | `Point2f` | 灯条中心点 |
| `width` | `float` | 灯条宽度 |
| `height` | `float` | 灯条高度 |
| `angle` | `float` | 相对竖直方向倾角 |
| `top` | `Point2f` | 上边中心点 |
| `bottom` | `Point2f` | 下边中心点 |
| `corners` | `Point2f[4]` | 四个角点 |

**`struct Armor`（装甲板，52–60 行）**

| 成员名 | 类型 | 作用 |
| --- | --- | --- |
| `center` | `Point2f` | 装甲板中心点 |
| `corners` | `vector<Point2f>` | 四个角点（left.top→right.top→right.bottom→left.bottom） |
| `pnpCorners` | `vector<Point2f>` | 供 PnP 使用的二维点 |
| `left` | `lightbar` | 左灯条 |
| `right` | `lightbar` | 右灯条 |
| `number` | `int` | SVM 识别出的数字（-1 表示未识别） |

### C. 工具函数参数

| 函数 | 参数 | 类型 | 作用 |
| --- | --- | --- | --- |
| `pointdistance` | `p1`、`p2` | `Point2f` | 两点欧氏距离 |
| `processFrame` | `frame` | `cv::Mat&` | 待处理的 BGR 图像 |
| `publishAim` | `world_yaw_deg`、`pitch_deg`、`shoot` | `double`/`double`/`bool` | 统一发布瞄准/扫描指令（**新增**；内部把 yaw 规范到 [-180,180]） |
| `frameCallback` | `msg` | `AutoAimFrame::SharedPtr` | 收到的 AutoAimFrame |
| `imageCallback` | `msg` | `CompressedImage::SharedPtr` | 收到的压缩图像 |

### D. 类 CampusNode 成员变量（950 行起）

| 变量名 | 类型 | 行 | 作用 |
| --- | --- | --- | --- |
| ⭐ `image_sub_` | `Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr` | 952–954 | 图像订阅器 |
| `svm_` | `cv::Ptr<cv::ml::SVM>` | 956 | SVM 数字识别模型 |
| `cameraMatrix_` | `cv::Mat` | 958 | 相机内参矩阵 K |
| `distCoeffs_` | `cv::Mat` | 959 | 相机畸变系数 D |
| ⭐ `frame_sub_` | `Subscription<tdt_interface::msg::AutoAimFrame>::SharedPtr` | 961–963 | AutoAimFrame 订阅器 |
| ⭐ `send_data_pub_` | `Publisher<tdt_interface::msg::SendData>::SharedPtr` | 965–967 | 瞄准指令发布器 |
| ⭐ `last_state_` | `tdt_interface::msg::AutoAimState` | 969 | 最近一帧自身状态 |
| `team_` | `std::string` | 972 | 当前阵营字符串（blue / red，**新增**） |
| `player_id_` | `int` | 973 | 阵营对应的 player 编号（1=蓝，2=红，**新增**） |
| `is_blue_` | `bool` | 974 | 当前是否蓝方（**新增**） |
| `last_aim_time_` | `rclcpp::Time` | 977 | 最近一次瞄准到目标的时刻（**新增**） |
| `scan_center_yaw_` | `double` | 978 | 扫描中心角（进入扫描时锁定，**新增**） |
| `is_scanning_` | `bool` | 979 | 当前是否正在扫描摆动（**新增**） |

> 说明：构造函数里还声明了 ROS 参数 `team`、`scan_enable`、`scan_amplitude_deg`、`scan_period_s`、`scan_timeout_s`、`scan_pitch_deg`（它们不存为成员，而是直接写入上表对应的变量）。

### E. 构造 CampusNode() 中创建的窗口 / 调节条（字符串名，非变量）

| 名称 | 类型 | 说明 |
| --- | --- | --- |
| `"result"` | 窗口名 | 识别结果窗口 |
| `"参数"` | 窗口名 | 参数调节窗口 |
| `"maxDisRatio"` | Trackbar 名 | 距离比例上限（读回 `/10.0`） |
| `"threshold"` | Trackbar 名 | 二值化阈值 |
| `"minArea"` | Trackbar 名 | 最小面积 |
| `"maxArea"` | Trackbar 名 | 最大面积 |
| `"maxAngle"` | Trackbar 名 | 灯条最大倾角 |
| `"centerAngle"` | Trackbar 名 | 中心连线最大角度 |

### F. processFrame() 局部变量

| 变量名 | 类型 | 行 | 作用 |
| --- | --- | --- | --- |
| `result` | `Mat` | 293 | 用于绘制识别结果的图像副本 |
| `frameCount` | `static int` | 294 | 帧计数，每 3 帧处理 1 帧 |
| `channels` | `vector<Mat>` | 338 | B、G、R 三通道 |
| `red_diff` | `Mat` | 350 | 红−蓝差值图 |
| `thre` | `Mat` | 371 | 二值化结果 |
| `contours` | `vector<vector<Point>>` | 386 | 轮廓点集 |
| `lightbars` | `vector<lightbar>` | 396 | 合格灯条集合 |
| `armors` | `vector<Armor>` | 397 | 识别到的装甲板集合 |
| `area` | `double` | 408 | 当前轮廓面积 |
| `rect` | `RotatedRect` | 413 | 轮廓最小外接旋转矩形 |
| `height` | `float` | 419 | 轮廓高度（长边） |
| `width` | `float` | 422 | 轮廓宽度（短边） |
| `pts` | `Point2f[4]` | 435 | 轮廓四个角点 |
| `edgeLength` | `double[4]` | 440 | 四条边长 |
| `shortEdge` | `int` | 452 | 最短边编号 |
| `oppositeEdge` | `int` | 463 | 最短边对边编号 |
| `point1` | `Point2f` | 467 | 第一条短边中点 |
| `point2` | `Point2f` | 474 | 另一条短边中点 |
| `top` | `Point2f` | 481 | 灯条上端中点 |
| `bottom` | `Point2f` | 482 | 灯条下端中点 |
| `dx` | `double` | 500 | bottom 与 top 的 x 差 |
| `dy` | `double` | 504 | bottom 与 top 的 y 差 |
| `lb` | `lightbar` | 508 | 当前灯条临时对象 |
| `lb_count` | `static int` | 543 | 灯条累计计数（每 50 打印日志） |
| `lightbarCorners` | `vector<Point>` | 560 | 灯条角点整数坐标 |
| `dis` | `double` | 614 | 两灯条中心距离 |
| `heightDiff` | `double` | 621 | 两灯条中心高度差 |
| `angleDiff` | `double` | 628 | 两灯条倾角差 |
| `centerDx` | `double` | 635 | 两灯条中心 x 差 |
| `centerDy` | `double` | 640 | 两灯条中心 y 差 |
| `centerAngle` | `double` | 645 | 中心连线相对水平角 |
| `avgHeight` | `double` | 650 | 两灯条平均高度 |
| `disRatio` | `double` | 657 | 间距/平均高度 |
| `heightRatio` | `double` | 662 | 两灯条高度比 |
| `armor` | `Armor` | 683 | 当前装甲板临时对象 |
| `pnp_count` | `static int` | 737 | 进入 PnP 段计数 |
| `objectPoints` | `vector<Point3f>` | 742 | 装甲板 4 个角点的 3D 坐标 |
| `imagePoints` | `vector<Point2f>` | 749 | 与 objectPoints 对应的 2D 点 |
| `rvec` / `tvec` | `Mat` | 755 | 旋转向量 / 平移向量 |
| `ok` | `bool` | 756 | solvePnP 是否成功 |
| `ok_count` | `static int` | 769 | solvePnP 成功计数 |
| `tx` / `ty` / `tz` | `double` | 772–774 | 目标在相机系下的 X / Y / Z |
| ⭐ `yaw_deg` | `double` | 778 | 目标相机系偏角（右偏正） |
| ⭐ `pitch_deg` | `double` | 779 | 目标相机系俯仰（下偏正） |
| ⭐ `world_yaw` | `double` | 782 | `last_state_.yaw_degrees + yaw_deg` |
| `info` | `string` | 791 | 画面叠加的 yaw/pitch/dist 文本 |
| `drawCorners` | `vector<Point>` | 802 | 装甲板角点整数坐标 |
| `p` | `const Point2f&` | 806 | 遍历 armor.corners 的引用 |
| `numberRect` | `Rect` | 822 | 数字识别外接矩形（含扩展） |
| `expandX` / `expandY` | `int` | 823–824 | 水平/竖直扩展量（各 20%） |
| `imageRect` | `Rect` | 832 | 图像范围，用于取交集防越界 |
| `numberROI` | `Mat` | 834 | 数字区域 ROI |
| `gray` | `Mat` | 838 | ROI 灰度图（缩放 20×28） |
| `feature` | `Mat` | 845 | 归一化后的一维特征（560 维） |
| `prediction` | `int` | 850 | SVM 预测结果 → `armor.number` |
| `am_count` | `static int` | 853 | 装甲板累计计数 |
| `text` | `string` | 856 | 画面叠加的数字文本 |
| `parameterImage` | `Mat` | 933 | 参数窗口黑底图像 |

**自动扫描块（886–926 行）新增的局部变量：**

| 变量名 | 类型 | 行 | 作用 |
| --- | --- | --- | --- |
| `now` | `rclcpp::Time` | 892 | 当前时间 |
| `since_aim` | `double` | 899 | 距上次瞄准过了多少秒 |
| `phase` | `double` | 911 | 正弦摆动的相位 |
| `scan_yaw` | `double` | 912 | 本次要发送的扫描角度 |

> `send_msg`（`SendData`）不再在 `processFrame` 里创建，**已移进 `publishAim`**（991 行）。

### G. frameCallback() 局部变量

| 变量名 | 类型 | 行 | 作用 |
| --- | --- | --- | --- |
| `msg` | `AutoAimFrame::SharedPtr` | 1000 | 输入消息 |
| `fc_count` | `static int` | 1018 | 回调计数（每 100 打印日志） |

### H. imageCallback() 局部变量

| 变量名 | 类型 | 行 | 作用 |
| --- | --- | --- | --- |
| `msg` | `CompressedImage::SharedPtr` | 1024 | 输入压缩图像 |
| `frame` | `Mat` | 1027 | `imdecode` 解码后的图像 |
| `ic_count` | `static int` | 1048 | 回调计数（每 100 打印日志） |

### I. main() 局部变量

| 变量名 | 类型 | 行 | 作用 |
| --- | --- | --- | --- |
| `argc` / `argv` | `int` / `char*[]` | 1055 | 命令行参数 |
| `node` | `std::shared_ptr<CampusNode>` | 1063 | 节点实例 |

### J. 循环 / 临时变量

| 变量名 | 类型 | 出现处 | 说明 |
| --- | --- | --- | --- |
| `i` | `size_t` / `int` | 404、529、606 | 轮廓/灯条循环；**529 行内层又声明了一个 `int i`，会遮蔽外层** |
| `j` | `size_t` | 610 | 灯条配对内层循环 |
| `k` | `int` / `size_t` | 442、454、529、548、563、750、805 | 边、角点、角点转换等循环 |

### K. 与 tdt_interface 直接相关的变量（速查）

| 变量 | 关联接口 |
| --- | --- |
| `image_sub_` | 订阅 `sensor_msgs/msg/CompressedImage` |
| `frame_sub_` | 订阅 `tdt_interface/msg/AutoAimFrame` |
| `send_data_pub_` | 发布 `tdt_interface/msg/SendData`（话题名按 `player_id_` 动态拼接） |
| `last_state_` | `tdt_interface/msg/AutoAimState` |
| `cameraMatrix_` / `distCoeffs_` | 取自 `AutoAimFrame.camera_info`（`k` / `d`） |
| `yaw_deg` / `pitch_deg` / `world_yaw` | 计算后经 `publishAim` 写入 `SendData.yaw` / `SendData.pitch` |
| `publishAim` | 内部创建 `SendData`（`.yaw` / `.pitch` / `.if_shoot`）并发布 |

---

## 4.6 main.cpp 函数清单（附「什么时候用」）

> **一句话**：把代码里调用的每个函数列出来，除了"参数、作用"，还告诉你**什么时候该用它**——
> 记不住用哪个的时候，先看下面的「ROS 2 四个基本动作」和最后的「按我想做什么查」。

### 先搞懂 ROS 2：其实就四个基本动作

写一个 ROS 2 节点，来来回回就这四件事，`main.cpp` 也不例外：

| 我想干嘛 | 用哪个 | 在 `main.cpp` 里的样子 |
| --- | --- | --- |
| 当"一个节点" | 继承 `rclcpp::Node` | `class CampusNode : public rclcpp::Node` |
| **收**消息 | `create_subscription<类型>(话题, 队列, 回调)` | 收图像、收 `/autoaim/frame` |
| **发**消息 | `create_publisher<类型>(话题, 队列)` | 发 `/target_angles_player_1` |
| 让程序**一直运行** | `rclcpp::spin(node)` | `main` 里调用 |

外加两个"开关"：

- 开头：`rclcpp::init(argc, argv)` —— 用 ROS 之前必须先初始化。
- 结尾：`rclcpp::shutdown()` —— 退出前关闭。

**"回调"是什么？** 就是"收到消息后自动去执行的那段代码"。
你通过 `std::bind(&CampusNode::imageCallback, this, _1)` 告诉订阅器：
"以后这个话题来消息了，就调用我的 `imageCallback` 函数。"

- 队列（qos）就是"最多先攒几条消息"：攒太多延迟高，攒太少会丢，图像用 1 就够。
- 一句话记忆：**收 = `create_subscription`，发 = `create_publisher`，收到后跑"回调"，`spin` 让它别停。**

---

### A. 本文件自定义函数 / 成员函数

| 函数（签名） | 参数 | 作用 | 什么时候用 |
| --- | --- | --- | --- |
| `double pointdistance(Point2f p1, Point2f p2)` | `p1`、`p2`：两个二维点 | 算两点距离 | 想知道"两点离多远"时（配对灯条、算边长） |
| `CampusNode::CampusNode()` | 无 | 节点构造：加载 SVM、建窗口/滑条、建订阅和发布 | 程序启动、建节点对象时**自动**跑一次 |
| `void CampusNode::processFrame(cv::Mat& frame)` | `frame`：BGR 图像 | 灯条识别 + 装甲板配对 + 认数字 + PnP + 发布 + 自动扫描 | **每收到一张图**解码后调用（识别主线） |
| `void CampusNode::publishAim(double world_yaw_deg, double pitch_deg, bool shoot)` | 世界 yaw(度)、pitch(度)、是否开火 | 把 yaw 规范到 [-180,180]，填 `SendData` 并发布 | 瞄准成功或需要扫描时（**新增**，统一出口） |
| `void CampusNode::frameCallback(const AutoAimFrame::SharedPtr msg)` | `msg`：收到的状态消息 | 存下相机内参、畸变、自身姿态 | **每收到一条 `/autoaim/frame`** 自动跑 |
| `void CampusNode::imageCallback(const CompressedImage::SharedPtr msg)` | `msg`：收到的压缩图 | 解码 JPEG，转交 `processFrame` | **每收到一张 `/camera/image_raw/compressed`** 自动跑 |
| `int main(int argc, char* argv[])` | `argc/argv`：命令行参数 | 初始化 ROS、建节点、spin、退出 | 程序入口，从头到尾跑一次 |

---

### B. ROS 2 / rclcpp 相关

| 函数（调用形式） | 参数 | 作用 | 什么时候用 |
| --- | --- | --- | --- |
| `rclcpp::init(argc, argv)` | 命令行参数 | 初始化 ROS 2 | **程序最开头**，用任何 ROS 功能之前 |
| `std::make_shared<CampusNode>()` | 无 | 创建节点对象（智能指针） | 想"造出"节点时 |
| `rclcpp::spin(node)` | `node`：节点指针 | 一直运行、等消息、分发回调 | 建好节点后；**不写它程序会立刻退出** |
| `rclcpp::shutdown()` | 无 | 关闭 ROS 2 | **程序结束前** |
| `this->create_subscription<T>(topic, qos, callback)` | `T`：消息类型；`topic`：话题名；`qos`：队列深度；`callback`：回调 | 创建**订阅器**（收消息） | 你要**接收**某个话题时 |
| `this->create_publisher<T>(topic, qos)` | `T`：消息类型；`topic`：话题名；`qos`：队列深度 | 创建**发布器**（发消息） | 你要**发送**某个话题时 |
| `std::bind(fn, this, _1)` | `fn`：成员函数；`this`：当前对象；`_1`：消息占位符 | 把成员函数绑成回调 | 建订阅器、指定"收到后调哪个函数"时 |
| `this->get_logger()` | 无 | 取节点日志器 | 想打日志时（配合下一行） |
| `RCLCPP_INFO(logger, fmt, ...)` | `logger`、格式串、可变参数 | 输出 INFO 日志 | 想打印"正常信息"时 |
| `RCLCPP_WARN(logger, fmt, ...)` | 同上 | 输出 WARN 日志 | 想打印"警告"时（如解码失败） |

---

### C. OpenCV 图像处理与解算

| 函数（调用形式） | 参数 | 作用 | 什么时候用 |
| --- | --- | --- | --- |
| `cv::imdecode(buf, flags)` | `buf`：压缩字节；`flags=IMREAD_COLOR` | JPEG 解码成图像 | 收到的是压缩图、想变成 `Mat` 时 |
| `cv::split(src, mv)` | `src`：图；`mv`：输出的通道数组 | 拆出 B/G/R 三通道 | 想单独处理某个颜色通道时 |
| `cv::subtract(src1, src2, dst)` | 两个通道 + 输出 | 两通道相减（红−蓝） | 想"突出某种颜色、压掉背景"时 |
| `cv::GaussianBlur(src, dst, ksize, sigmaX)` | `ksize=Size(9,9)`；`sigmaX=0` | 高斯模糊、去噪 | 图有噪点、想让后处理更干净时 |
| `cv::threshold(src, dst, thresh, maxval, type)` | 阈值、最大值、类型 | 二值化（变黑白） | 想把图按亮度分成"要/不要"时 |
| `cv::findContours(image, contours, mode, method)` | `mode=RETR_EXTERNAL`；`method=CHAIN_APPROX_SIMPLE` | 找轮廓 | 有黑白图、想提取里面的形状边界时 |
| `cv::contourArea(contour)` | 一个轮廓 | 算轮廓面积 | 想按"大小"筛形状时 |
| `cv::minAreaRect(points)` | 点集 | 拟合最小外接**旋转**矩形 | 想把一个形状包成一个可倾斜的矩形时 |
| `rect.points(pts)` | 输出的 4 个角点 | 取旋转矩形的四个顶点 | 拿到 `minAreaRect` 后想取角点时 |
| `cv::solvePnP(..., flags)` | 3D 点、2D 点、内参、畸变、出参 rvec/tvec、`flags` | 由点对解算目标位姿（距离/角度） | 知道物体真实尺寸、想求它离多远偏多少时 |
| `cv::cvtColor(src, dst, code)` | `code=COLOR_BGR2GRAY` | 转换颜色空间 | 想转灰度（或别的色彩空间）时 |
| `cv::resize(src, dst, dsize)` | `dsize=Size(20,28)` | 缩放尺寸 | 想把图缩成模型要求的固定大小 |
| `gray.convertTo(feature, rtype, alpha)` | `rtype=CV_32F`；`alpha=1/255` | 改数据类型 / 归一化 | 想喂给 SVM 前做归一化时 |
| `feature.reshape(cn, rows)` | `cn=1`；`rows=1` | 改变矩阵形状（展平） | 想把二维图变成一维特征向量时 |
| `cv::ml::SVM::load(filepath)` | 模型文件路径 | 加载 SVM 模型 | **程序启动时加载一次** |
| `svm_->predict(samples)` | `samples`：特征 | 预测分类（认数字） | 想用模型判断"这是几"时 |
| `cv::boundingRect(array)` | 点集 | 求点集的外接正矩形 | 想用不倾斜的矩形框住一堆点时 |
| `numberRect & imageRect` | 两个 `Rect` | 求交集 | 想防止 ROI 超出图像、越界时 |
| `frame(numberRect)` | 一个 `Rect` | 抠出子图（ROI） | 想从大图里只取一小块单独处理时 |

---

### D. OpenCV 绘图、窗口与显示

| 函数（调用形式） | 参数 | 作用 | 什么时候用 |
| --- | --- | --- | --- |
| `cv::namedWindow(winname, flags)` | 窗口名；`flags=WINDOW_NORMAL` | 创建窗口 | 想显示图像前先建窗口 |
| `cv::createTrackbar(name, win, value, count)` | 滑条名；窗口；`nullptr`；上限 | 创建调参滑条 | 想现场手动调参（不用重编译） |
| `cv::setTrackbarPos(name, win, pos)` | 初始位置 | 设滑条位置 | 想给滑条设个初始值时 |
| `cv::getTrackbarPos(name, win)` | — | 读滑条当前值 | 每帧想拿最新参数时 |
| `cv::circle(img, center, radius, color, thickness)` | 圆心、半径、颜色、线宽 | 画点/圆 | 想在图上标一个点时（`-1` 实心） |
| `cv::polylines(img, pts, isClosed, color, thickness)` | 点集、是否闭合、颜色、线宽 | 画折线/多边形 | 想把一组点连成轮廓时 |
| `cv::putText(img, text, org, fontFace, fontScale, color, thickness)` | 文本、位置、字体、大小、颜色、线宽 | 在图上写字 | 想显示数字/角度等文字时 |
| `cv::imshow(winname, mat)` | 窗口名、图 | 显示图像 | 想看到结果时 |
| `cv::waitKey(delay)` | 毫秒 | 刷新窗口 + 等按键 | 每次画完窗口后（否则窗口不刷） |
| `cv::Mat::zeros(rows, cols, type)` | 行、列、类型 | 造一张全黑图 | 想把某张图当"空白画布"时 |

---

### E. C++ 标准库与数学

| 函数（调用形式） | 参数 | 作用 | 什么时候用 |
| --- | --- | --- | --- |
| `std::sqrt(x)` | 被开方数 | 开平方 | 用勾股定理求距离时 |
| `std::pow(base, exp)` | 底数、指数 | 幂运算 | 求距离时算平方 |
| `std::atan2(y, x)` | y、x | 由坐标求角度 | 求灯条倾角、目标偏角时（**关键**） |
| `std::abs(x)` / `abs(x)` | 一个数 | 绝对值 | 比较角度差/高度差大小时 |
| `std::min(a,b)` / `std::max(a,b)` | 两个值 | 取最小/最大 | 求高度比等 |
| `std::swap(a,b)` | 两个变量 | 交换 | 想让"高>宽"、归一化时 |
| `std::to_string(v)` | 数值 | 转字符串 | 想把数字拼进日志/画面文字 |
| `to_string(v).substr(pos, len)` | 起点、长度 | 截取字符串 | 只显示小数前几位时 |
| `std::static_cast<int>(x)` | 待转换值 | 类型转换 | 把 SVM 的浮点输出转成整数时 |
| `vector::push_back` / `emplace_back` | 元素 | 往容器加元素 | 想把新找到的灯条/装甲板存起来 |
| `vector::size()` / `empty()` | — | 大小 / 判空 | 循环前看有没有元素、判断是否为空 |
| `Mat::clone()` | — | 复制矩阵 | 想改图又不想动原图时（画结果用） |
| `Mat::empty()` | — | 矩阵是否为空 | 解码/取 ROI 后检查是否有效 |
| `Mat::at<double>(i)` | 索引 | 读矩阵元素 | 想取 `tvec` 里的 x/y/z 时 |

---

### F. 常用 OpenCV 常量 / 枚举（作为函数实参）

| 常量 | 用在哪 | 含义 |
| --- | --- | --- |
| `CV_PI` | 角度换算 | π 值 |
| `THRESH_BINARY` | `threshold` | 二值化类型 |
| `RETR_EXTERNAL` | `findContours` | 只取最外层轮廓 |
| `CHAIN_APPROX_SIMPLE` | `findContours` | 压缩轮廓点 |
| `IMREAD_COLOR` | `imdecode` | 解码为三通道彩图 |
| `COLOR_BGR2GRAY` | `cvtColor` | BGR → 灰度 |
| `CV_32F` / `CV_8UC3` | `convertTo` / `Mat::zeros` | 数据类型 |
| `FONT_HERSHEY_SIMPLEX` / `FONT_HERSHEY_COMPLEX` | `putText` | 字体 |
| `SOLVEPNP_IPPE` | `solvePnP` | 平面目标位姿解算方法 |
| `WINDOW_NORMAL` | `namedWindow` | 可缩放窗口 |

---

### G. 记不住用哪个？按「我想做什么」查

| 我想…… | 用这些函数 |
| --- | --- |
| 收到图、把它变成能处理的图像 | `imdecode` |
| 把彩色图拆成单通道 | `split` |
| 突出某种颜色（如红色灯条） | `subtract`（红−蓝） |
| 图太脏，想去噪 | `GaussianBlur` |
| 按亮度把图变成黑白 | `threshold` |
| 从黑白图里提取形状 | `findContours` |
| 按大小筛形状 | `contourArea` |
| 把形状包成可倾斜的矩形 | `minAreaRect` → `rect.points` |
| 算两点距离 | `pointdistance`（内部用 `sqrt`/`pow`） |
| 由坐标求角度 | `atan2` |
| 求目标离我多远 / 偏多少 | `solvePnP` → `Mat::at<double>` |
| 认装甲板上的数字 | `SVM::load`（启动时）+ `predict` |
| 抠出目标区域单独处理 | `boundingRect` → `frame(rect)` |
| 把区域缩成模型要的尺寸 | `resize` / `convertTo` / `reshape` |
| 在图上画点/框/字 | `circle` / `polylines` / `putText` |
| 显示结果 | `namedWindow` / `imshow` / `waitKey` |
| 现场手动调参 | `createTrackbar` / `getTrackbarPos` / `setTrackbarPos` |
| 打印调试信息 | `RCLCPP_INFO` / `RCLCPP_WARN` |
| 收 / 发消息 | `create_subscription` / `create_publisher` |
| 让程序一直运行 | `rclcpp::spin` |

---

## 5. 与 main.cpp 相关的文件

> **一句话**：`main.cpp` 不是单独跑的，它需要这些"同伴文件"配合。

| 文件 | 作用 |
| --- | --- |
| `src/campus_vision/src/main.cpp` | ROS 主程序（本说明的核心） |
| `src/campus_vision/CMakeLists.txt` | 生成可执行 `campus_node`，链接 `rclcpp / sensor_msgs / tdt_interface / OpenCV` |
| `src/campus_vision/package.xml` | 包名 `campus_vision`，`depend` 含 `tdt_interface` |
| `src/campus_vision/tdt_interface/` | 从 `/usr/share/tdt-campus-game/` 拷贝的消息源码（**嵌在 campus_vision 内**，colcon 会一并发现） |
| `src/campus_vision/src/digit_svm.yml` | main.cpp 加载的 SVM 模型（RBF，8 类 1–8，560 维特征 20×28） |
| `src/campus_vision/src/svm.py` | 用 `per_100_datasets/` 训练出 `digit_svm.yml` 的脚本 |
| `src/campus_vision/src/per_100_datasets/1..8` | SVM 训练数据集 |
| `src/campus_vision/src/five.cpp` | 纯 OpenCV 灯条识别原型（**无 ROS/无 tdt_interface**），是 main.cpp 算法的前身/练习 |
| `campus_ws/.vscode/c_cpp_properties.json` | IntelliSense 额外 include 了 `build/tdt_interface/rosidl_generator_cpp` |

CMakeLists 关键内容：
```cmake
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(tdt_interface REQUIRED)
add_executable(campus_node src/main.cpp)
ament_target_dependencies(campus_node rclcpp sensor_msgs tdt_interface)
target_link_libraries(campus_node ${OpenCV_LIBS})
install(TARGETS campus_node DESTINATION lib/${PROJECT_NAME})
```

---

## 6. 编译与运行

> **一句话**：先编译，再开游戏，最后运行你的节点；两个程序要用同一个 `ROS_DOMAIN_ID`。

运行前先记住三件事（每次开新终端都要做前两步）：

| 要做的事 | 命令 | 为什么 |
| --- | --- | --- |
| 加载系统 ROS | `source /opt/ros/jazzy/setup.bash` | 让终端认识 `ros2`、`colcon` |
| 加载你自己编的包 | `source ~/campus_ws/install/setup.bash` | 让终端认识 `campus_vision`、`tdt_interface` |
| 设同一个"频道号" | `export ROS_DOMAIN_ID=0` | 游戏和程序频道号不同就互相收不到 |

```bash
cd ~/campus_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select tdt_interface campus_vision
source install/setup.bash

# 终端 A：先启动游戏（会自动开通信桥）
tdt-campus-game
# 或指定 domain / 频率
ROS_DOMAIN_ID=0 TDT_SENSOR_FPS=120 tdt-campus-game

# 终端 B：运行视觉节点
source /opt/ros/jazzy/setup.bash
source ~/campus_ws/install/setup.bash
export ROS_DOMAIN_ID=0        # 与游戏一致
ros2 run campus_vision campus_node

# 红方：加参数    ros2 run campus_vision campus_node --ros-args -p team:=red
# 关自动扫描：    ros2 run campus_vision campus_node --ros-args -p scan_enable:=false
# 调扫描摆幅：    ros2 run campus_vision campus_node --ros-args -p scan_amplitude_deg:=30.0
```

验证（另开一个终端，先做上面那两步 source）：

```bash
ros2 topic list -t
ros2 topic hz /camera/image_raw/compressed
ros2 topic echo /autoaim/state
ros2 topic echo /target_angles_player_1
```

| 命令 | 大白话作用 | 什么时候用 |
| --- | --- | --- |
| `ros2 topic list -t` | 列出当前所有"频道"及其消息类型 | 想知道有没有连上、有哪些话题 |
| `ros2 topic hz 话题` | 看某频道每秒来几条消息 | 想知道图像/状态有没有在发、频率多少 |
| `ros2 topic echo 话题` | 把某频道的消息实时打印出来 | 想亲眼看消息内容对不对 |

注意：`main.cpp` 会创建 OpenCV 窗口（`result`、`参数`），**必须在有 `DISPLAY` 的桌面环境运行**；
SVM 模型路径写死为 `/home/robot/campus_ws/src/campus_vision/src/digit_svm.yml`，换机器要改。

---

## 7. 注意事项 / 潜在问题

> **一句话**：代码能跑，但下面这些地方"能更好"或"要小心"，建议逐条看看。

1. **图像与状态不同源、非严格同帧**
   现在分别订阅 `/camera/image_raw/compressed`（relay 拆包输出）和 `/autoaim/frame`（原始）。
   relay 异步拆包，二者到达顺序不保证严格同帧。
   如要做严格同帧解算，建议**直接订阅 `/autoaim/frame`**，用其 `compressed_image` + `camera_info` + `state` 一次拿全
   （注意 `image`/`compressed_image` 只有一个有数据）。

2. **开火许可（`if_shoot`）**
   现在瞄准成功时走 `publishAim(..., true)`、扫描时走 `publishAim(..., false)`，**不再是恒为 `true`**。
   但仍**没有角度误差阈值和连发节流**：只要 `solvePnP` 成功就允许开火。
   建议加：角度容差、`projectile_speed`/`remaining_seconds` 判断、发射间隔限制。

3. **`world_yaw` 依赖 `yaw_degrees` 的零位与符号**
   `world_yaw = last_state_.yaw_degrees + yaw_deg`。`SendData.yaw` 要求「世界光轴角，Unity +Z=0，顺时针为正」。
   需确认 `AutoAimState.yaw_degrees` 与之同零位同符号，否则会出现 180° 偏差或正负号相反。
   `pitch` 直接使用相机系向下为正的偏角，与 `SendData.pitch`（向下为正、±60°）定义一致 ✅。

4. **PnP 前提**
   `cameraMatrix_` 和 `distCoeffs_` 都非空才做 PnP。若 `camera_info.d` 为空，`distCoeffs_` 会一直为空 → 永不进入 PnP。
   `camera_info.k` 是固定 9 元素数组，`size() >= 9` 恒成立，可简化。

5. **装甲板尺寸是估算值**
   `ARMOR_WIDTH_M = 0.135`、`ARMOR_HEIGHT_M = 0.055` 为估算（代码注释也提示按实测修改），会影响测距精度。
   `objectPoints` 顺序必须与 `armor.corners` 一致（left.top → right.top → right.bottom → left.bottom），
   当前顺序一致 ✅，且已用 `SOLVEPNP_IPPE`（适合平面目标）。

6. **队列深度**
   `AutoAimFrame` 用 depth=1 符合官方建议；图像订阅用了 10，官方建议图像 depth=1，可改为 1 降低延迟。

7. **阵营话题（已支持红蓝，无需改代码）**
   发布话题按阵营动态拼接：`/target_angles_player_` + `player_id_`（蓝=1、红=2），由 ROS 参数 `team` 决定。
   运行示例：`ros2 run campus_vision campus_node --ros-args -p team:=red`。
   ⚠️ 注意：底盘速度话题 `/target_speed_player_N` 要算法自行保证用同一个 N；朝向/坐标约定也可能按阵营不同，需实测验证。

8. **`five.cpp` 与 `main.cpp` 无关到 ROS**
   `five.cpp` 是纯 OpenCV 原型，不参与节点编译（CMakeLists 只编译 `src/main.cpp`），保留供参考。

9. **自动扫描会主动转动云台（新增）**
   `scan_enable_` 默认开启：连续 `scan_timeout_s_`（0.5s）没瞄准到目标，就以正弦曲线左右摆动 yaw（`if_shoot=false`）。
   摆幅/周期/俯仰都可运行时覆盖（`-p scan_amplitude_deg:=45.0` 等）。
   若不想让程序自己转云台，运行时加 `-p scan_enable:=false`。
   注意：扫描用的是**世界光轴绝对角** `SendData.yaw`，若云台反馈的 `yaw_degrees` 零位/符号与之一致才能对得上（同第 3 条）。

---

## 8. 快速排障

> **一句话**：出问题时，先照着下面的"现象 → 检查"对一对。

| 现象 | 检查 |
| --- | --- |
| 收不到图像 | 是否已进入对局；`ROS_DOMAIN_ID` 是否与游戏一致；`ros2 topic hz /camera/image_raw/compressed` |
| 图像收到但无 PnP | `/autoaim/frame` 是否在发；`camera_info.d` 是否非空 |
| 瞄准方向反了/偏 180° | 核对 `AutoAimState.yaw_degrees` 与 `SendData.yaw` 的零位与符号 |
| 桥没起来 | 看 `~/.local/state/tdt-campus-game/logs/<时间戳>/` 下 `launcher/endpoint/relay/unity.log` |
| 编译找不到 tdt_interface | 先 `colcon build --packages-select tdt_interface`，再 source `install/setup.bash` |

---
---

# 附录：官方说明文档整理

> **一句话**：下面是官方自己写的说明书，我原样搬了过来当"字典"，遇到不懂的可以翻。
> 本节把校园赛游戏及其配套接入包里自带的说明文档**原文**整理到本文档，方便离线查阅。
> 来源：
> - `/usr/share/doc/tdt-campus-game/DEBIAN_RELEASE.md.gz`（安装包内官方发行说明，v1.1.0）
> - `~/下载/ros2_auto_framework/README.md` 与 `~/下载/campusgame_ros/ros2_auto_framework/README.md`（两份内容一致）
> - 接入包内 `build.sh`、`start.sh`、`src/ros_tcp_endpoint/`

---

## 附录 A：官方发行说明（`DEBIAN_RELEASE.md`，v1.1.0 全文）

### Linux deb 安装版发布

版本 **1.1.0**，目标 **Ubuntu 24.04 amd64（x86_64）**。安装包包含 Unity 游戏、ROS 2 Jazzy Endpoint、图像/状态 Relay、`tdt_interface` 和 Python/ROS 运行依赖。玩家无需安装 Unity、Python 或 ROS，也不用手动 source 环境或启动通信桥；系统提供桌面环境、glibc 和显卡驱动。其他发行版、旧版 Ubuntu 和 ARM 不在此版本验证范围内。

#### 安装与启动

在收到安装包的目录打开终端：

```bash
sudo apt install ./tdt-campus-game_1.1.0_amd64.deb
```

安装后在应用菜单搜索 **T-DT 校园赛 2027**（英文环境为 **T-DT Campus Game 2027**），点击图标启动。应用菜单与窗口图标使用 `Assets/Resource/image/app-icon-rounded.png`，为原图 `gj.png` 加上 Apple 官方模板精确轮廓的连续圆角版本。也可以从任意工作目录运行：

```bash
tdt-campus-game
```

启动器预留通信端口后即启动 Unity，内置通信桥同时初始化，Unity 启动不等待桥就绪。有 `DISPLAY` 且 Player 目录包含 `libtdt_startup_window.so` 时，启动器仅为 Unity 子进程预加载该库，将同一 X11 主窗口的初始背景设为不透明黑色，窗口随正常创建流程显示。游戏随后在该窗口中进入轻量 `Startup` 场景，以 `Assets/Resource/image/startup-landscape.png` 16:9 横版扩图铺满窗口，用约 1.5 秒从黑色平滑渐亮，同时异步加载 `Menu`；渐亮完成且菜单首帧准备好后，以约 0.15 秒让照片淡出、菜单显现，在原窗口中完成交接，无额外停留。窗口和菜单始终不透明，转场结束后恢复输入。约 1.5 秒指渐亮动画时长，实际启动耗时还取决于进程初始化和场景加载。缺少 `DISPLAY` 或该库时使用常规 Unity 开屏。

开屏图片包含在 Unity Player 资源中，启动器不创建独立开屏窗口，也不执行窗口位置或尺寸跟随。该图由 imagegen 生成，只用于开屏。更新图片后需重新构建 Player 并打包，导入设置与原图保留规则见 [启动画面与应用图标](STARTUP_PRESENTATION.md)。

选择阵营和控制模式，进入对局后即可发布所选机器人的 ROS 图像和消息。菜单阶段没有比赛图像。全手动、两种半自动和全自动模式都支持发布；自动移动、瞄准和射击需要外部算法发布控制指令，安装包不包含自动控制算法。

需要调整采集目标或 ROS domain 时，从终端启动：

```bash
ROS_DOMAIN_ID=7 TDT_SENSOR_FPS=60 tdt-campus-game
```

默认 domain 为 `0`、图像采集目标为 `120 Hz`，实际接收频率取决于图形性能及传输负载。标准图像输出为 `960×720` JPEG。游戏参数也可直接传入，例如 `tdt-campus-game -screen-fullscreen 0`。

卸载：

```bash
sudo apt remove tdt-campus-game
```

卸载会移除应用和菜单入口，用户设置与日志保留在用户目录。不要使用 sudo 启动游戏。

#### ROS 订阅

应用启动时先预留通信端口，再启动游戏，并同时初始化内置 Endpoint 和 Relay；游戏窗口的启动不等待通信桥就绪，关闭游戏会结束本次启动的通信桥。默认连接 `127.0.0.1:10000`，该端口被占用时会自动分配空闲端口并传给游戏。已保存的远程 IP 或 ROS 禁用设置不会阻止本次自动连接，F8 可查看连接状态。

算法端使用相同的 `ROS_DOMAIN_ID` 和兼容的消息定义。标准压缩图像类型是 `sensor_msgs/msg/CompressedImage`；自定义消息定义安装在 `/usr/share/tdt-campus-game/tdt_interface/`，也可从仓库 `tdt_interface/` 获取。修改接口后应同步重建两端。

在安装了 ROS 2 Jazzy 的算法终端中：

```bash
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=0
ros2 topic list -t
ros2 topic hz /camera/image_raw/compressed
```

如算法工作区尚无自定义消息包，可从安装版复制并构建（需要已安装 colcon 和 ROS 消息生成工具）：

```bash
mkdir -p ~/tdt_ros_ws/src
cp -R /usr/share/tdt-campus-game/tdt_interface ~/tdt_ros_ws/src/
cd ~/tdt_ros_ws
colcon build --packages-select tdt_interface
source install/setup.bash
```

已构建接口时，只需 source 对应工作区，再执行：

```bash
ros2 topic echo /autoaim/state
```

主要话题：

| 话题 | 类型 / 用途 |
| --- | --- |
| `/autoaim/frame` | `tdt_interface/msg/AutoAimFrame`，同步图像、相机参数和状态 |
| `/autoaim/state` | `tdt_interface/msg/AutoAimState`，自身状态 |
| `/camera/image_raw/compressed` | `sensor_msgs/msg/CompressedImage`，标准 JPEG 图像 |

完整协议及控制指令见 [自瞄时间同步说明](AUTOAIM_SYNC.md)。内置 ROS 运行库仅供应用自身使用，不修改算法终端的系统 ROS 环境。

#### 从源码生成安装包

构建机使用 Ubuntu 24.04 amd64，安装项目版本的 Unity Linux Build Support、ROS 2 Jazzy、colcon，以及 `dpkg-deb`、Python 3 和 `desktop-file-validate`。主窗口启动库的编译还需要 `cc`（可由 `build-essential` 安装）和 `libx11-dev`。ROS 构建准备见 [源码运行说明](SINGLE_PLAYER_JAZZY.md#构建与启动)。保存并关闭使用此工程的 Unity 编辑器，在项目根目录执行：

```bash
./scripts/build_linux.sh
bash scripts/setup_ros_tcp_endpoint.sh
./scripts/package_deb.sh
```

`scripts/build_linux.sh` 在 Unity Player 构建成功后自动调用 `scripts/build_startup_window.sh`，将 `libtdt_startup_window.so` 编译到 Player 根目录；deb 封包会随 Player 一起复制该库。

发布版本读取 `ProjectSettings/ProjectSettings.asset` 中的 `bundleVersion`，当前为 `1.1.0`。修改版本后先重新构建 Player，再封包；打包脚本使用已有 Player，不会自动重编译游戏。

封包时使用内置 Python 为标准库生成可搬移的哈希校验字节码缓存，减少每次启动时的解释器编译开销；运行时仍保持只读并禁止写入缓存。

默认产物：

```text
Builds/Release/tdt-campus-game_1.1.0_amd64.deb
Builds/Release/tdt-campus-game_1.1.0_amd64.deb.sha256
```

脚本同时生成 SHA-256 校验文件。分发 `.deb` 即可，玩家不需要源码或相邻的 Unity 资源目录。

其他输入与输出位置可通过 `./scripts/package_deb.sh --help` 查看。相对路径按项目根目录解析，输出须在 Player 和 ROS 工作区之外；图标默认使用 `Assets/Resource/image/app-icon-rounded.png`。`--icon` 只替换安装后的应用菜单图标，Player 图标与开屏由 Unity 构建配置决定。打包在临时目录进行，不修改输入 Player，排除 Unity 标为不应分发的调试/备份目录，ROS 工作区符号链接在封包时转为实际文件。

#### 安装布局与诊断

| 路径 | 内容 |
| --- | --- |
| `/opt/tdt-campus-game/` | 游戏资源、内置 ROS/Python 运行库和启动管理器 |
| `/usr/bin/tdt-campus-game` | 命令入口 |
| `/usr/share/applications/tdt-campus-game.desktop` | 应用菜单入口 |
| `/usr/share/pixmaps/tdt-campus-game.png` | 应用图标 |
| `/usr/share/doc/tdt-campus-game/` | 安装说明与许可 |
| `/usr/share/tdt-campus-game/tdt_interface/` | 供算法端编译的 ROS 自定义消息包源码 |
| `~/.local/state/tdt-campus-game/logs/` | 每次启动的独立日志目录 |

设置了绝对路径的 `XDG_STATE_HOME` 时，日志写入该目录下的 `tdt-campus-game/logs/`。查看 `launcher.log`、`endpoint.log`、`relay.log` 和 `unity.log` 可区分通信桥启动问题与游戏问题。应用目录由 root 持有，普通用户运行不需写入安装目录。

维护者应验证安装包的版本、架构、文件权限和桌面入口，并从安装路径启动游戏，进入对局实际订阅图像和状态，再关闭窗口检查桥进程退出。构建或 ROS 节点注册成功不能替代图像实收检查。

打包格式参考 [dpkg-deb 官方手册](https://manpages.debian.org/bookworm/dpkg/dpkg-deb.1.en.html)，菜单入口字段遵循 [freedesktop 桌面入口规范](https://specifications.freedesktop.org/desktop-entry/latest/recognized-keys.html)。

> 附：本机 `DEBIAN_RELEASE.md` 路径与读取方式
> ```bash
> zcat /usr/share/doc/tdt-campus-game/DEBIAN_RELEASE.md.gz
> ```

---

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

---

## 附录 C：官方接入包脚本与来源说明

### `build.sh`（编译）
```bash
#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
set +u
source /opt/ros/jazzy/setup.bash
set -u
cd "$root"
colcon build --base-paths "$root/src" \
    --cmake-args -DPython3_EXECUTABLE=/usr/bin/python3
```

### `start.sh`（启动 TCP 桥）
```bash
#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
[[ -f "$root/install/local_setup.bash" ]] || { printf '请先运行 ./build.sh\n' >&2; exit 1; }
set +u
source /opt/ros/jazzy/setup.bash
source "$root/install/local_setup.bash"
set -u
export ROS_AUTOMATIC_DISCOVERY_RANGE="${ROS_AUTOMATIC_DISCOVERY_RANGE:-LOCALHOST}"
exec ros2 run ros_tcp_endpoint default_server_endpoint --ros-args \
    -p "ROS_IP:=${TDT_ROS_IP:-127.0.0.1}" -p "ROS_TCP_PORT:=${TDT_ROS_PORT:-10000}" "$@"
```

说明：`build.sh` 编译 `src/` 下的 `tdt_interface` 与 `ros_tcp_endpoint`；`start.sh` 以 `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` 启动 Unity ROS TCP Endpoint，默认监听 `127.0.0.1:10000`（可用 `TDT_ROS_IP` / `TDT_ROS_PORT` 覆盖）。**这是给算法端单机自检用的 TCP 桥**；正常安装版游戏已内置同样的桥，二者择一即可。

### `ros_tcp_endpoint` 来源
```
Unity-Technologies/ROS-TCP-Endpoint main-ros2
commit 54c1a64b6d5ef6ffa0a0431570bb74329b79b15b
Local packaging: remove development/launch files; initialize executor before
accepting TCP registrations and handle Ctrl+C shutdown in the default entry.
```
包名 `ros_tcp_endpoint`，版本 `0.0.1`，maintainer `Unity Robotics <unity-robotics@unity3d.com>`，Apache 2.0。

---

## 附录 D：官方文档索引与缺失文档

### 本机可读到的官方文档

| 文档 | 位置 | 内容 |
| --- | --- | --- |
| 发行说明 | `/usr/share/doc/tdt-campus-game/DEBIAN_RELEASE.md.gz` | 见附录 A（安装、启动、ROS、打包、诊断） |
| 许可 | `/usr/share/doc/tdt-campus-game/copyright` | Apache-2.0 |
| 接入包说明 | `~/下载/ros2_auto_framework/README.md` | 见附录 B（话题与接入约定） |
| 接入包脚本 | `~/下载/ros2_auto_framework/{build.sh,start.sh}` | 见附录 C |
| 接口源码 | `/usr/share/tdt-campus-game/tdt_interface/` | 5 个 `.msg` |
| 内置运行时清单 | `/opt/tdt-campus-game/ros-runtime/manifest.json` | ROS Jazzy / Python 3.12 / overlay 包 |

### 发行说明中提到、但本机未随包分发的文档

| 文档 | 被引用处 | 说明 |
| --- | --- | --- |
| `AUTOAIM_SYNC.md` | 发行说明「ROS 订阅」 | 自瞄时间同步与完整控制协议（开发仓库文档） |
| `STARTUP_PRESENTATION.md` | 发行说明「安装与启动」 | 启动画面与应用图标 |
| `SINGLE_PLAYER_JAZZY.md` | 发行说明「从源码生成安装包」 | 源码运行说明 |

> 这三份属于开发仓库文档，安装包内未包含；本机磁盘上也未找到。如需完整协议以 `AUTOAIM_SYNC.md` 为准。

---

## 附录 E：许可证

- **`tdt-campus-game`**：Apache-2.0（见 `/usr/share/doc/tdt-campus-game/copyright`）。
- **`tdt_interface`**：Apache-2.0，maintainer `zhujunheng2005@163.com`。
- **`ros_tcp_endpoint`**：Apache-2.0，源自 Unity Technologies `ROS-TCP-Endpoint`（main-ros2 分支）。
- 游戏内第三方组件许可见 `/opt/tdt-campus-game/ros-runtime/licenses/` 与 `/opt/tdt-campus-game/ThirdPartyNotices/`。
