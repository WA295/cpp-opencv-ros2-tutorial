# OpenCV 入门与图像基础

> 这一篇是 OpenCV 的"地基"：搞懂**图像在计算机里到底是什么**，以及 **Mat 这个核心类型**。
> 不把这层想明白，后面所有函数都会用得很心虚。

---

## 目录

1. [OpenCV 是什么](#1-opencv-是什么)
2. [图像在计算机里是什么](#2-图像在计算机里是什么)
3. [Mat：OpenCV 的核心](#3-matopencv-的核心)
4. [读、写、显示图像](#4-读写显示图像)
5. [颜色空间与转换](#5-颜色空间与转换)
6. [坐标系与图像尺寸](#6-坐标系与图像尺寸)
7. [图像数据类型](#7-图像数据类型)
8. [在 CMake 项目里用 OpenCV](#8-在-cmake-项目里用-opencv)
9. [OpenCV 模块与常用头文件](#9-opencv-模块与常用头文件)
10. [新手常见坑](#10-新手常见坑)

---

## 1. OpenCV 是什么

OpenCV（Open Source Computer Vision Library）是一个开源的**计算机视觉库**，
里面有一千多个现成的图像/视频处理函数：

- 读图/显示/保存
- 颜色转换、滤波、二值化、边缘检测
- 找轮廓、形状拟合、几何变换
- 相机标定、位姿解算（PnP）
- 模板匹配、光流、特征点
- 机器学习（SVM、KNN 等）

**一句话：你不需要自己写图像算法，绝大多数需求 OpenCV 都有现成的。**

### 安装（Ubuntu）

```bash
sudo apt install libopencv-dev python3-opencv
```

查看版本：

```bash
pkg-config --modversion opencv4
```

### 在代码里引入

```cpp
#include <opencv2/opencv.hpp>   // 一把梭：引入最常用的模块（方便，编译慢一点）
```

或者只引需要的（编译快）：

```cpp
#include <opencv2/core.hpp>      // Mat 等核心类型
#include <opencv2/imgproc.hpp>   // 图像处理
#include <opencv2/highgui.hpp>   // 显示窗口
#include <opencv2/imgcodecs.hpp> // 读写图片
#include <opencv2/calib3d.hpp>   // 相机标定、PnP
#include <opencv2/ml.hpp>        // 机器学习（SVM）
```

**命名空间：** OpenCV 的东西都在 `cv::` 里，习惯写 `using namespace cv;`。

---

## 2. 图像在计算机里是什么

### 2.1 像素：图像的最小单位

一张数字图像就是一个**二维网格**，每个格子叫一个**像素（pixel）**，
每个像素记录"这个点的颜色"。

```
960 列 →
┌────────────────────────────┐
│ (0,0) (1,0) (2,0) ...      │  ┐
│ (0,1) (1,1) ...            │  │ 720 行
│ ...                        │  ┘
└────────────────────────────┘
```

- **分辨率**：960×720 = 宽 960、高 720，一共 691200 个像素。
- 分辨率越高越清晰，但处理越慢（像素数按面积增长）。

### 2.2 通道：一个像素怎么表示颜色

**彩色图（3 通道）：** 每个像素用 3 个数字表示 —— **B、G、R**（蓝、绿、红）。

```
一个像素 = [B, G, R] = [34, 200, 12]
```

> ⚠️ **OpenCV 的顺序是 BGR，不是 RGB！**
> 这是新手第一大坑。`channels[0]` 是蓝、`channels[1]` 是绿、`channels[2]` 是红。
> 用 matplotlib 或别的库显示时，常要自己转一下。

**灰度图（1 通道）：** 每个像素只有一个数（0~255），0 是黑、255 是白。

**带透明通道（4 通道）：** BGRA。

### 2.3 取值范围

| 类型 | 范围 | 说明 |
| --- | --- | --- |
| 8 位无符号（CV_8U） | 0 ~ 255 | **最常用**，每个通道 1 字节 |
| 32 位浮点（CV_32F） | 任意小数 | 机器学习特征、计算中间结果 |

### 2.4 为什么"红−蓝差值"能找红色灯条

```
红色像素 ≈ [B=20, G=30, R=220]  → R−B = 200
灰色背景 ≈ [B=120, G=120, R=120] → R−B = 0
蓝色像素 ≈ [B=220, G=30, R=20]   → R−B = -200（相减后变 0）
```

所以"红通道 − 蓝通道"后，**红色区域变得很亮，其他区域被压暗**。
这就是校园赛找灯条的第一步。反过来找蓝色目标就用"蓝 − 红"。

---

## 3. Mat：OpenCV 的核心

`cv::Mat` 是 OpenCV 表示图像/矩阵的类。**几乎所有函数都在操作 Mat。**

### 3.1 创建 Mat

```cpp
cv::Mat img;                                  // 空
cv::Mat img2(720, 960, CV_8UC3);              // 高, 宽, 类型（3通道8位）
cv::Mat img3(720, 960, CV_8UC3, cv::Scalar(0,0,0));   // 并填成黑色
cv::Mat zeros = cv::Mat::zeros(150, 600, CV_8UC3);    // 全 0（黑图）
cv::Mat ones  = cv::Mat::ones(3, 3, CV_32F);          // 全 1
cv::Mat eye   = cv::Mat::eye(3, 3, CV_64F);           // 单位矩阵（内参初值常用）
```

**参数顺序永远是"高（行数）在前，宽（列数）在后"，和直觉相反，务必记牢！**

### 3.2 常用成员

| 成员 | 含义 | 例子 |
| --- | --- | --- |
| `img.rows` | 行数（高） | `720` |
| `img.cols` | 列数（宽） | `960` |
| `img.size()` | 尺寸（宽, 高） | `Size(960,720)` |
| `img.channels()` | 通道数 | 彩色 3 |
| `img.type()` | 类型编码 | `CV_8UC3` |
| `img.empty()` | 是否为空 | **用前必查** |
| `img.total()` | 像素总数 | rows*cols |
| `img.at<T>(y,x)` | 访问像素 | 见下 |
| `img.clone()` | 深拷贝 | 改副本不影响原图 |
| `img.copyTo(dst)` | 拷贝 | 同上 |
| `img.convertTo(dst, type, alpha, beta)` | 转换类型/缩放 | 归一化到 0~1 |
| `img.reshape(cn, rows)` | 改形状 | 展平成一维特征 |
| `img.ptr<T>(y)` | 取某行首地址 | 高性能遍历 |

### 3.3 访问像素

```cpp
cv::Vec3b pixel = img.at<cv::Vec3b>(y, x);   // 彩色图（8UC3）
uchar b = pixel[0], g = pixel[1], r = pixel[2];

uchar gray = gray_img.at<uchar>(y, x);       // 灰度图（8UC1）
float v = f_img.at<float>(y, x);             // 32F 图
double d = m.at<double>(i, j);               // double 矩阵
```

**注意 `at<T>(行, 列)` 是 `(y, x)`，不是 `(x, y)`。**

> 遍历整图一个一个 `at` 比较慢，只在需要时用；大量处理尽量用 OpenCV 的现成函数。

### 3.4 深浅拷贝（重要！）

```cpp
cv::Mat a = cv::imread("a.jpg");
cv::Mat b = a;             // 浅拷贝：a、b 指向同一份数据！改 b 会改 a
cv::Mat c = a.clone();     // 深拷贝：独立的数据，互不影响
```

**实战：** 想"在原图上画东西但保留原图"时，必须 `clone()`：

```cpp
cv::Mat result = frame.clone();       // 之后在 result 上画
```

### 3.5 ROI（取子图）

```cpp
cv::Rect roi(x, y, w, h);             // 位置 + 宽高
cv::Mat sub = img(roi);               // 注意：这是"视图"，不是拷贝！
cv::Mat sub_copy = img(roi).clone();  // 想要独立副本就 clone
```

**实战：** 从整图里抠出装甲板区域，只对这一小块做数字识别 —— 又快又准。

### 3.6 遍历图像的高效写法

```cpp
// 遍历所有像素（以灰度图为例）
for (int y = 0; y < img.rows; y++) {
    const uchar* row = img.ptr<uchar>(y);   // 拿这一行首地址
    for (int x = 0; x < img.cols; x++) {
        uchar v = row[x];
        // ...
    }
}
```

---

## 4. 读、写、显示图像

### 4.1 读写

```cpp
#include <opencv2/imgcodecs.hpp>

cv::Mat img = cv::imread("test.jpg");                  // 读（默认彩色）
cv::Mat gray = cv::imread("test.jpg", cv::IMREAD_GRAYSCALE);   // 读成灰度
cv::Mat same = cv::imread("test.jpg", cv::IMREAD_COLOR);       // 读成三通道

if (img.empty()) {                                     // 一定检查！
    std::cout << "读图失败，路径不对？\n";
    return;
}

cv::imwrite("out.png", img);                           // 保存
```

| 标志 | 含义 |
| --- | --- |
| `cv::IMREAD_COLOR` | 彩色（3 通道，默认） |
| `cv::IMREAD_GRAYSCALE` | 灰度（1 通道） |
| `cv::IMREAD_UNCHANGED` | 原样（含透明通道） |

### 4.2 从内存字节解码（ROS 2 图像常用）

```cpp
// msg->data 是 JPEG 压缩字节，直接解码成 Mat
cv::Mat img = cv::imdecode(msg->data, cv::IMREAD_COLOR);
if (img.empty()) return;
```

**对应关系：** `imread` 从文件读，`imdecode` 从内存字节读 —— 网络/消息传的图用后者。

反方向（编码成 JPEG 字节）：

```cpp
std::vector<uchar> buf;
cv::imencode(".jpg", img, buf);      // 把 Mat 编码成 JPEG 字节流
```

### 4.3 显示

```cpp
#include <opencv2/highgui.hpp>

cv::namedWindow("result", cv::WINDOW_NORMAL);   // 建窗口（可缩放）
cv::imshow("result", img);                       // 显示
int key = cv::waitKey(1);                        // 等待 1ms，必须调用！

if (key == 'q') break;                           // 按键检测
```

**关键点：**

1. `imshow` 之后**必须** `waitKey`，否则画面不刷新（新手第一大坑）。
2. `waitKey(1)` = 等待 1 毫秒（适合视频流）；`waitKey(0)` = 无限等待按键（适合看单图）。
3. 在无图形界面的环境（服务器/SSH）会报错，需要 `DISPLAY`。

### 4.4 视频读写

```cpp
cv::VideoCapture cap(0);              // 打开摄像头 0
// 或 cv::VideoCapture cap("video.mp4");   // 打开视频文件
if (!cap.isOpened()) return;

cv::Mat frame;
while (cap.read(frame)) {             // 读一帧
    cv::imshow("cam", frame);
    if (cv::waitKey(1) == 27) break;  // ESC 退出
}

cv::VideoWriter writer("out.avi",
    cv::VideoWriter::fourcc('M','J','P','G'), 25, cv::Size(640,480));
writer.write(frame);
```

---

## 5. 颜色空间与转换

### 5.1 常见颜色空间

| 空间 | 组成 | 什么时候用 |
| --- | --- | --- |
| BGR | 蓝、绿、红 | OpenCV 默认 |
| 灰度 | 亮度 | 只需明暗（边缘、轮廓、数字识别） |
| HSV | 色调、饱和度、明度 | **按颜色找东西时最好用** |
| Lab | 亮度 + 两轴色度 | 颜色差异度量 |

### 5.2 cvtColor

```cpp
cv::Mat gray, hsv;
cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);   // 彩色 → 灰度
cv::cvtColor(img, hsv,  cv::COLOR_BGR2HSV);    // 彩色 → HSV
cv::cvtColor(gray, img, cv::COLOR_GRAY2BGR);   // 灰度 → 彩色（画彩色标记得先转）
```

| 常用转换码 | 含义 |
| --- | --- |
| `COLOR_BGR2GRAY` | BGR → 灰度 |
| `COLOR_BGR2RGB` | BGR → RGB（给别的库显示用） |
| `COLOR_BGR2HSV` | BGR → HSV |
| `COLOR_HSV2BGR` | HSV → BGR |
| `COLOR_GRAY2BGR` | 灰度 → BGR |

### 5.3 用 HSV 找颜色（比红蓝差值更稳）

```cpp
cv::Mat hsv, mask;
cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

// 红色在 HSV 里跨 0°，所以要两段
cv::inRange(hsv, cv::Scalar(0, 100, 100),   cv::Scalar(10, 255, 255), mask1);
cv::inRange(hsv, cv::Scalar(170, 100, 100), cv::Scalar(180, 255, 255), mask2);
cv::bitwise_or(mask1, mask2, mask);   // 合并
```

**什么时候用：** 光照变化大、背景杂时，HSV + `inRange` 比 RGB 差值更稳。

---

## 6. 坐标系与图像尺寸

### 6.1 图像坐标系

```
(0,0) ───────→ x（列，向右）
  │
  │
  ↓
  y（行，向下）
```

**要点：**

- 原点在**左上角**。
- x 向右增大，y 向**下**增大（不是数学里的向上）。
- 访问用 `(y, x)` 顺序：`img.at<T>(y, x)`、`cv::Point(x, y)`（Point 是 x 在前）。

### 6.2 尺寸的几种写法

| 形式 | 顺序 | 例子 |
| --- | --- | --- |
| `cv::Size(w, h)` | 宽, 高 | `cv::Size(960, 720)` |
| Mat 构造 `(rows, cols)` | 高, 宽 | `cv::Mat(720, 960, ...)` |
| `img.size()` 返回 | Size(宽, 高) | `.width` `.height` |

**记法：构造 Mat 用"高、宽"，Size 用"宽、高"。** 弄混会导致图像被拉变形。

---

## 7. 图像数据类型

`CV_<位数><有无符号>C<通道数>`：

| 类型 | 含义 | 什么时候用 |
| --- | --- | --- |
| `CV_8U` / `CV_8UC1` | 8 位无符号，1 通道 | 灰度图、掩码 |
| `CV_8UC3` | 8 位无符号，3 通道 | **普通彩色图** |
| `CV_8UC4` | 4 通道 | 带透明 |
| `CV_32F` | 32 位浮点 | 特征、归一化数据、SVM 输入 |
| `CV_64F` | 64 位浮点 | 内参矩阵、PnP 结果 |

**转换：**

```cpp
cv::Mat feature;
gray.convertTo(feature, CV_32F, 1.0/255.0);   // 8U → 32F，并把 0~255 缩到 0~1
```

> `convertTo(dst, type, alpha, beta)` 的公式：`dst = src * alpha + beta`。

---

## 8. 在 CMake 项目里用 OpenCV

```cmake
find_package(OpenCV REQUIRED)                 # 找 OpenCV

add_executable(demo main.cpp)

target_include_directories(demo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(demo ${OpenCV_LIBS})
```

ROS 2 包里的写法（结合上一章）：

```cmake
find_package(OpenCV REQUIRED)
add_executable(campus_node src/main.cpp)
ament_target_dependencies(campus_node rclcpp sensor_msgs tdt_interface)
target_link_libraries(campus_node ${OpenCV_LIBS})   # OpenCV 单独链
```

**手动编译（调试单文件时）：**

```bash
g++ demo.cpp -o demo `pkg-config --cflags --libs opencv4`
```

---

## 9. OpenCV 模块与常用头文件

| 模块/头文件 | 提供什么 | 常用函数 |
| --- | --- | --- |
| `opencv2/core.hpp` | 核心类型、矩阵运算 | Mat、Vec、Scalar、Point、`add` `subtract` |
| `opencv2/imgcodecs.hpp` | 图片编解码 | `imread` `imwrite` `imdecode` `imencode` |
| `opencv2/highgui.hpp` | 窗口、显示、滑条 | `imshow` `waitKey` `namedWindow` `createTrackbar` |
| `opencv2/imgproc.hpp` | 图像处理（最大最常用） | `cvtColor` `threshold` `GaussianBlur` `findContours` `resize` `drawContours` `circle` `putText` |
| `opencv2/calib3d.hpp` | 相机标定、三维 | `solvePnP` `calibrateCamera` `findHomography` |
| `opencv2/ml.hpp` | 机器学习 | `SVM` `KNearest` `NormalBayesClassifier` |
| `opencv2/features2d.hpp` | 特征点 | `ORB` `SIFT` `BFMatcher` |
| `opencv2/video.hpp` | 视频分析 | 光流、背景减除 |
| `opencv2/opencv.hpp` | 一次引入常用模块 | 图方便就用它 |

---

## 10. 新手常见坑

| 坑 | 现象 | 正解 |
| --- | --- | --- |
| 以为通道顺序是 RGB | 颜色不对（红蓝反了） | OpenCV 是 **BGR**：`channels[0]`=B、`[2]`=R |
| `imshow` 后没 `waitKey` | 窗口不刷新 / 卡死 | 每次 `imshow` 后跟 `waitKey(1)` |
| Mat 构造顺序搞反 | 图像拉伸、报错 | `Mat(rows=高, cols=宽, type)` |
| 忘记 `empty()` 检查 | 段错误 | 用前先 `if (img.empty()) return;` |
| 浅拷贝当深拷贝 | 改了副本原图也变 | 要独立副本用 `.clone()` |
| `at<T>` 写 `(x,y)` | 数据错位 | 是 `at<T>(y, x)` |
| ROI 越界 | 崩溃 | 用 `Rect 交集` 限制在图像内 |
| 忘了 `cvtColor` 就做灰度算法 | 报错/结果怪 | 先转 `COLOR_BGR2GRAY` |
| 在无 DISPLAY 环境用 `imshow` | 报错退出 | 服务器上别显示，或存文件 |
| 中文路径读图 | 读不到 | 用相对/英文路径，或 `imdecode` |

---

> 下一篇：[OpenCV 图像处理函数大全](01-图像处理函数大全.md)
