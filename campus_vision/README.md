# campus_vision · T-DT 校园赛 2027 视觉节点

> 这是一个真实的机器人视觉项目：订阅比赛模拟器的画面，识别敌方装甲板，
> 用 PnP 解算目标角度，把瞄准指令发回游戏。它也是本教程里大量例子的来源。

---

## 它做什么

```
游戏画面 (JPEG) ──► 你的节点 ──► 瞄准指令 (yaw/pitch/开火)
```

1. 订阅 `/camera/image_raw/compressed`（压缩图像）和 `/autoaim/frame`（状态 + 相机内参）
2. OpenCV：通道差值找灯条 → 二值化 → 找轮廓 → 筛选 → 配对成装甲板
3. SVM：识别装甲板上的数字
4. `solvePnP`：由装甲板 4 个角点解算距离与角度
5. 发布 `/target_angles_player_N`（`tdt_interface/msg/SendData`）

支持：**红蓝阵营**（ROS 参数 `team`）、**未找到目标时自动扫描**（正弦摆动 yaw）。

---

## 目录结构

```
campus_vision/
├── TDT接口与main.cpp说明.md      ← 完整说明：接口协议 + main.cpp 逐项核对（推荐先看）
├── TDT文档/                       ← 上面那份的分章版，方便单篇查阅
│   ├── README.md                 ← 分章版总目录
│   ├── 0-导读与编写逻辑/
│   ├── 1-接口协议/
│   ├── 2-main.cpp详解/
│   ├── 3-运行与排障/
│   └── 4-官方文档附录/
├── digit_svm.yml                  ← 训练好的数字识别模型（1~8）
├── .vscode/                       ← IntelliSense 配置（消息头文件路径）
└── src/
    └── campus_vision/
        ├── CMakeLists.txt
        ├── package.xml
        ├── tdt_interface/         ← 消息定义（从官方安装包拷来的）
        └── src/
            ├── main.cpp           ← 主程序（ROS 节点 campus_node）
            ├── five.cpp           ← 纯 OpenCV 原型（学习用，不参与编译）
            ├── svm.py             ← 训练 digit_svm.yml 的脚本
            ├── digit_svm.yml      ← 模型副本
            └── per_100_datasets/  ← SVM 训练数据集（1~8 每个数字一个文件夹）
```

---

## 怎么编译和运行

### 1. 环境

- Ubuntu 24.04 + ROS 2 Jazzy + OpenCV
- 消息接口包（从官方游戏安装包获取）：

```bash
mkdir -p ~/campus_ws/src
cp -R /usr/share/tdt-campus-game/tdt_interface ~/campus_ws/src/
cp -r campus_vision/src/campus_vision ~/campus_ws/src/
cd ~/campus_ws
source /opt/ros/jazzy/setup.bash
colcon build
source install/setup.bash
```

### 2. 运行

```bash
# 终端 A：启动游戏（会自动开通信桥）
tdt-campus-game

# 终端 B：运行视觉节点（与游戏用同一个 ROS_DOMAIN_ID）
export ROS_DOMAIN_ID=0
ros2 run campus_vision campus_node
```

### 3. 常用参数

```bash
# 红方
ros2 run campus_vision campus_node --ros-args -p team:=red

# 关闭自动扫描 / 调整扫描摆幅
ros2 run campus_vision campus_node --ros-args -p scan_enable:=false
ros2 run campus_vision campus_node --ros-args -p scan_amplitude_deg:=30.0
```

> ⚠️ 模型路径在 `main.cpp` 里写死为 `/home/robot/campus_ws/src/campus_vision/src/digit_svm.yml`，
> clone 到别的机器后需要改路径。

---

## 详细文档

- **完整版**：[`TDT接口与main.cpp说明.md`](TDT接口与main.cpp说明.md) —— 官方接口协议 + 5 个消息定义 + `main.cpp` 逐项核对 + 变量/函数清单 + 排障
- **分章版**：[`TDT文档/README.md`](TDT文档/README.md) —— 按章节拆分，方便单篇查阅
- **配套教程**：仓库根目录的 [`README.md`](../README.md) —— 从零学 C++ / OpenCV / ROS 2

---

## 说明

- 这是个人比赛学习项目，代码里有大量中文注释和"踩坑记录"，风格偏教学。
- `per_100_datasets/` 是 SVM 训练用的数字截图数据集，供复现训练用。
