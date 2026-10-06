# code_leran · 从零到项目：C++ / OpenCV / ROS 2 教程

![License](https://img.shields.io/badge/License-MIT-blue)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![OpenCV](https://img.shields.io/badge/OpenCV-4.x-5C3EE8?logo=opencv&logoColor=white)
![ROS 2](https://img.shields.io/badge/ROS2-Jazzy-22314E?logo=ros&logoColor=white)
![Ubuntu](https://img.shields.io/badge/Ubuntu-24.04-E95420?logo=ubuntu&logoColor=white)

> 这是一套**面向编程小白**的实战教程：从"什么是程序"讲到"能独立写一个机器人视觉节点"。
> 内容分三大板块（**C++ / OpenCV / ROS 2**）+ 新手必读 + **练手项目** + **深入进阶** + 综合实战，
> 全部按"为什么 → 怎么做 → 什么时候用"组织。
>
> **配套项目：** `~/campus_ws`（T-DT 校园赛视觉节点），教程里的例子大量来自这个真实项目。
> **相关文档：** `~/campus_ws/TDT接口与main.cpp说明.md`（接口协议与代码核对）。

---

## 👀 适合谁看

| 如果你 | 从这里开始 |
| --- | --- |
| 完全没写过代码 | `00-新手必读/` 两篇 → `01-C++/00` |
| 想知道计算机专业学什么、怎么规划 | ⭐ `00-新手必读/03-计算机专业完整学习路线` |
| 会一点 C++，想学图像识别 | `02-OpenCV/00` 开始 |
| 想做机器人/视觉项目 | `03-ROS2/00` 开始 |
| 想快速上手写代码 | ⭐ `05-练手项目/00`（动手优先） |
| 基础都会了，想更稳更准 | ⭐ `06-深入进阶/00` |
| 已经在做校园赛项目 | `04-综合实战/00` + 仓库外的配套文档 |

## 🚀 怎么开始（三步）

```
① 花 20 分钟读 00-新手必读/00-编程入门与项目开发逻辑.md
      （建立"程序怎么来、问题怎么查"的认知）
        ↓
② 打开终端，跟着 05-练手项目/ 一个个敲代码
      （每个项目都有完整可运行代码，改一改就跑）
        ↓
③ 遇到问题，查对应手册的"排错"和"速查表"
      （每篇末尾都有"按需求查"）
```

## 🌍 English Introduction

A hands-on tutorial series for **complete beginners** who want to go from zero
to building real robotics vision projects.

- **C++** — the language: syntax, standard library, common pitfalls
- **OpenCV** — computer vision: image processing, contours, camera model & PnP, SVM digit recognition
- **ROS 2** (Jazzy) — robotics communication: nodes, topics, QoS, custom messages, debugging

Every topic follows the same pattern: *why → how → when to use*, with runnable
examples, troubleshooting guides and look-up tables. All examples are drawn from
a real project (T-DT Campus Game 2027 vision node).

> **Learn by doing:** start with `05-练手项目/` (hands-on mini projects with
> complete runnable code), then go deeper with `06-深入进阶/`.

---

## 📚 怎么用这套教程

### 如果你完全没写过代码

按顺序读：

```
00-新手必读/00-编程入门与项目开发逻辑   ← 先建立认知
00-新手必读/01-环境与工具速查          ← 会敲命令、会编译
01-C++/00                              ← 学语法
01-C++/01                              ← 学常用函数
```

### 如果你会一点 C++，想学视觉

```
02-OpenCV/00 → 01 → 02 → 03 → 04 → 05
（图像基础 → 处理函数 → 轮廓几何 → 相机PnP → SVM识别 → 绘图调试）
```

### 如果你想做机器人项目

```
03-ROS2/00 → 01 → 02 → 03 → 04
（概念 → 节点/话题 → 命令行 → 消息定义 → 从零写节点）
最后：04-综合实战/00（把三者串起来）
```

### ⭐ 如果你想"快速上手写代码"

**先动手，别光看！** 跟着练手项目一个个敲：

```
05-练手项目/00-从零开始写代码          ← 写代码五步法 + 6 个必做练习
05-练手项目/01-OpenCV三个图像小程序    ← 显示图/找颜色/找轮廓（可运行）
05-练手项目/02-ROS2两个通信小程序      ← talker/listener + 图像亮度检测
05-练手项目/03-综合迷你视觉瞄准项目    ← 一个完整的"小校园赛"（毕业项目）
```

### ⭐ 如果基础会了，想往深钻

```
06-深入进阶/00-深入OpenCV颜色分割与调参  ← 颜色找得稳不稳，看这篇
06-深入进阶/01-深入相机PnP进阶           ← 角点排序/验证/滤波，精度靠这篇
06-深入进阶/02-深入ROS2通信进阶          ← QoS/多线程/bag调试/时间戳
```

### 如果你已经有项目，只是遇到问题

直接跳到对应手册查：

| 问题 | 去处 |
| --- | --- |
| 不知道用哪个函数 | `01-C++/01`、`02-OpenCV` 各篇的"按需求查" |
| 报错看不懂 | `01-C++/02` 排错手册、`00-新手必读/00` 第 4 节 |
| 程序崩溃 | `01-C++/02` 第 2 节 |
| 收不到 ROS 消息 | `03-ROS2/02` 第 11 节、`06-深入进阶/02` 第 1 节 |
| 识别效果差 | `02-OpenCV/04`、`04-综合实战/00` 第 7 节 |
| 颜色分割不稳 | `06-深入进阶/00` |
| PnP 结果乱跳 | `06-深入进阶/01` |
| 不知道项目怎么开始 | `04-综合实战/00` 第 3、5 节 |

---

## 📂 目录结构

```
code_leran/
├── README.md                                  ← 你正在看的这份
│
├── 00-新手必读/
│   ├── 00-编程入门与项目开发逻辑.md            ← 第一课：程序是什么、项目怎么做、问题怎么查
│   ├── 01-环境与工具速查.md                    ← 终端、g++、CMake、colcon、VSCode、Git
│   ├── 02-GitHub提交项目步骤.md                ← 怎么把项目提交到 GitHub（日常 3 条命令）
│   └── 03-计算机专业完整学习路线.md            ← ★全景地图：必修/选修/学了能做什么/做一件事需要什么
│
├── 01-C++/
│   ├── 00-C++基础语法与头文件.md               ← 语法 + ★头文件总表（什么功能引哪个头文件）
│   ├── 01-C++标准库常用函数.md                 ← ★按功能分类的函数手册（含"什么时候用"）
│   └── 02-C++实战套路与排错.md                 ← 十个代码套路 + 常见错误 + 性能常识
│
├── 02-OpenCV/
│   ├── 00-OpenCV入门与图像基础.md              ← 像素/通道/Mat/颜色空间/坐标系
│   ├── 01-图像处理函数大全.md                  ← 转换/滤波/阈值/形态学/边缘/几何
│   ├── 02-轮廓形状与几何.md                    ← 找轮廓/量形状/配对（识别核心）
│   ├── 03-相机模型PnP与标定.md                 ← 内参/畸变/solvePnP/测距角度
│   ├── 04-机器学习SVM与数字识别.md             ← 训练/加载/预测 + 数字识别全流程
│   └── 05-绘图窗口与实战套路.md                ← 画图/显示/滑条/调试可视化套路
│
├── 03-ROS2/
│   ├── 00-ROS2概念与工作空间.md                ← 节点/话题/服务/参数/DDS/工作空间
│   ├── 01-节点话题消息与QoS.md                 ← ★C++ API 手册（订阅/发布/回调/QoS）
│   ├── 02-常用命令行与调试.md                  ← ★ros2 CLI 手册 + 排查流程
│   ├── 03-消息定义与接口包.md                  ← .msg 语法 + 建消息包 + tdt_interface 拆解
│   └── 04-实战从零写一个节点.md                ← 手把手做一个完整节点
│
├── 04-综合实战/
│   └── 00-校园赛项目全流程拆解.md               ← 需求→设计→分步实现→调试→优化（真实案例）
│
├── 05-练手项目/                                 ★动手写代码的地方
│   ├── 00-从零开始写代码.md                     ← 写代码五步法 + 6 个必做练习（带答案）
│   ├── 01-OpenCV三个图像小程序.md               ← 显示图/找颜色/找轮廓（完整可运行）
│   ├── 02-ROS2两个通信小程序.md                 ← 聊天程序 + 图像亮度检测节点
│   └── 03-综合迷你视觉瞄准项目.md               ← 毕业项目：完整"小校园赛"（一个文件）
│
└── 06-深入进阶/                                 ★往深钻的重点
    ├── 00-深入OpenCV颜色分割与调参.md           ← HSV 原理、红色跨0°、六步调参法
    ├── 01-深入相机PnP进阶.md                    ← 角点排序、验证、滤波、精度提升
    └── 02-深入ROS2通信进阶.md                   ← QoS排查、多线程、bag调试、时间戳
```

★ = 最常当"字典"用的两篇。

---

## 🗺️ 学习路线图

```
阶段一：能写能跑（1~2 周）
  00-新手必读 → 01-C++/00 → 会写 hello world、会用 g++ 编译
  动手：05-练手项目/00 的 6 个练习 ← 一定亲手敲
        ↓
阶段二：会处理图像（2~4 周）
  02-OpenCV/00 → 01 → 02 → 能读图、找颜色、找轮廓、画框
  动手：05-练手项目/01 三个图像小程序
        ↓
阶段三：会做机器人节点（2~4 周）
  03-ROS2/00 → 01 → 04 → 能写"订阅→处理→发布"的节点
  动手：05-练手项目/02 两个通信小程序
        ↓
阶段四：能做项目（持续）
  02-OpenCV/03 → 04 → 04-综合实战/00 → 能独立完成视觉识别项目
  动手：05-练手项目/03 迷你瞄准项目（毕业项目）
        ↓
阶段五：往深钻（按需）
  06-深入进阶/00（颜色稳） → 01（PnP 准） → 02（通信稳）
```

**每阶段的标准：能不看教程写出一个能跑的小例子。**

---

## 🎯 三个板块分别解决什么问题

| 板块 | 解决 | 核心问题 | 对应文件 |
| --- | --- | --- | --- |
| **C++** | "怎么把想法写成代码" | 变量/函数/容器/排错 | `01-C++/` |
| **OpenCV** | "怎么让程序看懂图像" | 图像处理/形状/相机/识别 | `02-OpenCV/` |
| **ROS 2** | "怎么让程序之间通信" | 节点/话题/消息/调试 | `03-ROS2/` |
| **练手项目** | "怎么动手写出来" | 完整可运行的小项目 | `05-练手项目/` |
| **深入进阶** | "怎么写得稳、准" | 调参/精度/通信稳定性 | `06-深入进阶/` |

**三者的关系：**

```
C++    = 语言（写代码的工具）
OpenCV = 眼睛（处理图像）
ROS 2  = 神经系统（收发数据、和别的程序协作）
```

一个完整的机器人视觉程序 = **C++ 写逻辑 + OpenCV 做识别 + ROS 2 通信**。

---

## 📖 每篇的写法说明

为了让你能当"教科书/字典"用，每篇都尽量包含：

| 元素 | 说明 |
| --- | --- |
| **大白话解释** | 先讲"这是什么、为什么需要"，再讲语法 |
| **函数表** | 作用 / 参数 / 例子 / **什么时候用** |
| **头文件** | 每个功能要 `#include` 什么 |
| **实战套路** | 真实项目里怎么用（从校园赛项目里提炼） |
| **排错** | 常见错的报错、原因、解决 |
| **速查表** | 每篇末尾"按需求查" |

---

## 🔗 相关资源

### 本项目相关

| 资源 | 位置 |
| --- | --- |
| 视觉项目（源码 + 完整文档包） | [`campus_vision/`](campus_vision/README.md) |
| 校园赛接口协议 + 代码核对 | [`campus_vision/TDT接口与main.cpp说明.md`](campus_vision/TDT接口与main.cpp说明.md) |
| 分章版（方便查阅） | [`campus_vision/TDT文档/README.md`](campus_vision/TDT文档/README.md) |
| 项目主程序 | [`campus_vision/src/campus_vision/src/main.cpp`](campus_vision/src/campus_vision/src/main.cpp) |
| 消息定义 | [`campus_vision/src/campus_vision/tdt_interface/msg/`](campus_vision/src/campus_vision/tdt_interface/msg/) |
| 官方接入包 | `~/下载/ros2_auto_framework/` |

### 官方文档（外网）

| 内容 | 地址 |
| --- | --- |
| OpenCV 文档 | https://docs.opencv.org/ |
| ROS 2 文档 | https://docs.ros.org/en/jazzy/ |
| C++ 参考 | https://en.cppreference.com/ |

### 系统内的帮助

```bash
ros2 interface show 类型        # 查消息字段
ros2 topic list -t              # 查话题
man 命令                        # 查系统命令
```

---

## ⚡ 一页速查（把最常用的放这）

### C++

| 我想 | 用 | 头文件 |
| --- | --- | --- |
| 打印 | `std::cout` | `<iostream>` |
| 动态数组 | `std::vector` | `<vector>` |
| 排序/最值/交换 | `std::sort/min/max/swap` | `<algorithm>` |
| 求角度 | `std::atan2` | `<cmath>` |
| 数字转字符串 | `std::to_string` | `<string>` |
| 计时 | `std::chrono::steady_clock` | `<chrono>` |

### OpenCV

| 我想 | 用 |
| --- | --- |
| 解码图像 | `cv::imdecode` |
| 转灰度 | `cv::cvtColor(..., COLOR_BGR2GRAY)` |
| 突显颜色 | `cv::split` + `cv::subtract` |
| 去噪 | `cv::GaussianBlur` |
| 二值化 | `cv::threshold` |
| 找形状 | `cv::findContours` |
| 量面积/框 | `cv::contourArea` / `boundingRect` / `minAreaRect` |
| 求位姿 | `cv::solvePnP` |
| 认数字 | `svm->predict` |
| 显示 | `cv::imshow` + `cv::waitKey` |

### ROS 2

| 我想 | 用 |
| --- | --- |
| 收消息 | `create_subscription<T>(话题, 队列, 回调)` |
| 发消息 | `create_publisher<T>(话题, 队列)` + `publish` |
| 一直运行 | `rclcpp::spin(node)` |
| 定时做事 | `create_wall_timer` |
| 打日志 | `RCLCPP_INFO` |
| 查话题 | `ros2 topic list -t` |
| 看数据 | `ros2 topic echo /话题 --once` |
| 查字段 | `ros2 interface show 类型` |

---

## 💡 给小白的三句话

1. **不要背，要查。** 这套文档就是给你查的，用多了自然记住。
2. **不要攒，要跑。** 写一点就跑一次，跑通了再往下。
3. **不要慌，要看。** 报错第一行 + 行号，永远是解决问题的钥匙。

> 祝学习顺利。遇到问题，先回到 `00-新手必读/00` 的第 4 节"遇到问题怎么办"。
