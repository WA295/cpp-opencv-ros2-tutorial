# 练手 1：OpenCV 三个图像小程序

> 语法过关了？现在**动手写能看、能玩的小程序**。
> 这三个项目每个都是"完整可运行"的，跟着敲完、跑起来、再改一改，OpenCV 就上手了。
> 全部只用一个文件 + `pkg-config` 编译。

---

## 目录

1. [准备：一行命令编译 OpenCV 程序](#1-准备一行命令编译-opencv-程序)
2. [项目 A：显示图片和摄像头](#2-项目-a显示图片和摄像头)
3. [项目 B：颜色检测器（带滑条调参）](#3-项目-b颜色检测器带滑条调参)
4. [项目 C：轮廓标注器](#4-项目-c轮廓标注器)
5. [三个项目做完你会什么](#5-三个项目做完你会什么)

---

## 1. 准备：一行命令编译 OpenCV 程序

先确认 OpenCV 装好：

```bash
pkg-config --modversion opencv4
```

编译单文件（不用 CMake，练习专用）：

```bash
g++ 文件名.cpp -o 可执行名 `pkg-config --cflags --libs opencv4`
./可执行名
```

> **为什么要 `pkg-config`？** 它会自动补全 OpenCV 的头文件路径和库。
> 正式项目里用 CMake（见 `00-新手必读/01`），练手用这一行更快。

---

## 2. 项目 A：显示图片和摄像头

### 目标

- 读一张图片显示出来，按 ESC 退出。
- 顺便支持打开摄像头。

### 学到什么

- `imread` / `VideoCapture`
- `imshow` + `waitKey` 的固定搭配
- 图像为空要判空

### 完整代码 `show_image.cpp`

```cpp
#include <opencv2/opencv.hpp>
#include <iostream>

int main(int argc, char** argv)
{
    // 情况 1：给了图片路径 → 显示图片
    if (argc > 1) {
        cv::Mat img = cv::imread(argv[1], cv::IMREAD_COLOR);
        if (img.empty()) {
            std::cout << "读图失败，路径：" << argv[1] << std::endl;
            return 1;
        }
        std::cout << "尺寸：" << img.cols << " x " << img.rows << std::endl;

        cv::imshow("图片", img);
        std::cout << "按任意键退出" << std::endl;
        cv::waitKey(0);                          // 无限等待按键
        return 0;
    }

    // 情况 2：没给路径 → 打开摄像头
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cout << "打不开摄像头" << std::endl;
        return 1;
    }

    cv::Mat frame;
    while (true) {
        cap.read(frame);
        if (frame.empty()) break;

        cv::imshow("摄像头", frame);
        if (cv::waitKey(1) == 27) break;         // ESC 退出
    }
    return 0;
}
```

### 运行

```bash
g++ show_image.cpp -o show_image `pkg-config --cflags --libs opencv4`
./show_image 某张图片.jpg     # 显示图片
./show_image                  # 打开摄像头
```

### 自己动手

1. 把图片缩小一半再显示（`cv::resize`）。
2. 显示灰度版（`cv::cvtColor`）。
3. 摄像头版：按 `s` 键保存当前帧（`cv::imwrite`）。

---

## 3. 项目 B：颜色检测器（带滑条调参）

### 目标

一个"实时找红色物体"的程序：摄像头/视频画面里，**红色的地方变白，其余全黑**，并且可以用滑条实时调整颜色范围。

### 学到什么

- HSV + `inRange`（找颜色标准做法）
- `createTrackbar` 实时调参
- 掩码显示与"调试可视化"

### 完整代码 `color_detector.cpp`

```cpp
#include <opencv2/opencv.hpp>
#include <iostream>

int H_LOW = 0,  H_HIGH = 10;     // 红色在 HSV 里跨 0，先只取 0~10 这段
int S_LOW = 100, S_HIGH = 255;
int V_LOW = 100, V_HIGH = 255;

int main()
{
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) return 1;

    cv::namedWindow("参数", cv::WINDOW_NORMAL);
    cv::createTrackbar("H_low",  "参数", nullptr, 180);
    cv::createTrackbar("H_high", "参数", nullptr, 180);
    cv::createTrackbar("S_low",  "参数", nullptr, 255);
    cv::createTrackbar("S_high", "参数", nullptr, 255);
    cv::createTrackbar("V_low",  "参数", nullptr, 255);
    cv::createTrackbar("V_high", "参数", nullptr, 255);

    cv::setTrackbarPos("H_low",  "参数", H_LOW);
    cv::setTrackbarPos("H_high", "参数", H_HIGH);
    cv::setTrackbarPos("S_low",  "参数", S_LOW);
    cv::setTrackbarPos("S_high", "参数", S_HIGH);
    cv::setTrackbarPos("V_low",  "参数", V_LOW);
    cv::setTrackbarPos("V_high", "参数", V_HIGH);

    cv::Mat frame, hsv, mask, result;
    while (true) {
        cap.read(frame);
        if (frame.empty()) break;

        // 1. 读滑条
        H_LOW  = cv::getTrackbarPos("H_low",  "参数");
        H_HIGH = cv::getTrackbarPos("H_high", "参数");
        S_LOW  = cv::getTrackbarPos("S_low",  "参数");
        S_HIGH = cv::getTrackbarPos("S_high", "参数");
        V_LOW  = cv::getTrackbarPos("V_low",  "参数");
        V_HIGH = cv::getTrackbarPos("V_high", "参数");

        // 2. BGR → HSV → inRange 得到掩码
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv,
                    cv::Scalar(H_LOW, S_LOW, V_LOW),
                    cv::Scalar(H_HIGH, S_HIGH, V_HIGH),
                    mask);

        // 3. 用掩码从原图里抠出彩色目标
        cv::bitwise_and(frame, frame, result, mask);

        // 4. 显示（三张图并排看更直观）
        cv::imshow("原图", frame);
        cv::imshow("掩码", mask);
        cv::imshow("抠出目标", result);

        cv::Mat param_img = cv::Mat::zeros(100, 400, CV_8UC3);
        cv::imshow("参数", param_img);

        if (cv::waitKey(1) == 27) break;
    }
    return 0;
}
```

### 运行与调参

```bash
g++ color_detector.cpp -o color_detector `pkg-config --cflags --libs opencv4`
./color_detector
```

**调参流程（重点学习）：**

1. 先把 S_low、V_low 设低一点（50、50），让目标**大概**露出来。
2. 调 H_low/H_high：让目标区域变白、背景变黑。**红色目标用两段**（0~10 和 170~180），后面"深入进阶"讲。
3. 调 S_low：把淡色的干扰排除（提高 S_low 只留浓颜色）。
4. 调 V_low：把暗处排除。
5. 最后再微调 H 范围，让目标完整、背景干净。

> **调参口诀：先粗后细。先让目标"出现"，再让它"干净"。**

### 自己动手

1. 把目标换成蓝色（H 大约 100~130）、绿色（H 大约 40~80）。
2. 把"红色跨 0 度"补全：两段 `inRange` + `bitwise_or`。
3. 给掩码加形态学（`morphologyEx`）去掉小噪点。

---

## 4. 项目 C：轮廓标注器

### 目标

从摄像头画面里找二值化后的**所有轮廓**，给每个画上框，并打印"编号、面积、中心"。

### 学到什么

- 完整轮廓套路：二值化 → `findContours` → 特征 → 筛选 → 画
- 用 `drawContours` / `rectangle` 调试
- 把结果转成"文字信息"（这就是识别输出的雏形）

### 完整代码 `contour_tagger.cpp`

```cpp
#include <opencv2/opencv.hpp>
#include <iostream>

int main()
{
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) return 1;

    cv::namedWindow("结果", cv::WINDOW_NORMAL);
    cv::namedWindow("参数");
    cv::createTrackbar("threshold", "参数", nullptr, 255);
    cv::createTrackbar("minArea",   "参数", nullptr, 2000);
    cv::setTrackbarPos("threshold", "参数", 100);
    cv::setTrackbarPos("minArea",   "参数", 200);

    cv::Mat frame, gray, binary, show;
    while (true) {
        cap.read(frame);
        if (frame.empty()) break;

        int thresh = cv::getTrackbarPos("threshold", "参数");
        int minArea = cv::getTrackbarPos("minArea", "参数");

        // 1. 灰度 + 二值化
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, binary, thresh, 255, cv::THRESH_BINARY);

        // 2. 找轮廓
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours,
                         cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        // 3. 逐个处理
        show = frame.clone();
        int id = 0;
        for (const auto& c : contours) {
            double area = cv::contourArea(c);
            if (area < minArea) continue;          // 面积筛选

            cv::Rect box = cv::boundingRect(c);    // 外接正矩形
            cv::rectangle(show, box, cv::Scalar(0, 255, 0), 2);

            // 轮廓重心
            cv::Moments m = cv::moments(c);
            cv::Point2f center(m.m10 / m.m00, m.m01 / m.m00);
            cv::circle(show, center, 4, cv::Scalar(0, 0, 255), -1);

            std::cout << "轮廓 " << id++ << ": 面积=" << (int)area
                      << " 中心=(" << (int)center.x << ", " << (int)center.y
                      << ")\n";
        }

        cv::imshow("结果", show);
        cv::imshow("二值", binary);
        cv::Mat param_img = cv::Mat::zeros(100, 400, CV_8UC3);
        cv::imshow("参数", param_img);
        if (cv::waitKey(1) == 27) break;
    }
    return 0;
}
```

### 运行

```bash
g++ contour_tagger.cpp -o contour_tagger `pkg-config --cflags --libs opencv4`
./contour_tagger
```

对着摄像头，屏幕里每个"亮块"都会被绿框框住、红点标出中心，终端打印它们的面积和坐标。

### 自己动手

1. 只保留"面积最大"的轮廓。
2. 把 `boundingRect` 换成 `minAreaRect`，框会跟着物体旋转。
3. 加一个"高宽比"筛选，只框**细长**的物体（灯条！）。

---

## 5. 三个项目做完你会什么

| 能力 | 从哪个项目来 |
| --- | --- |
| 读图/开摄像头/显示/按键退出 | 项目 A |
| HSV 找颜色 + 滑条调参 + 掩码 | 项目 B |
| 二值化 + 找轮廓 + 量面积 + 画框 + 重心 | 项目 C |
| "看到什么 → 变成文字信息" | 项目 B、C |

**下一步：** 把项目 B（找颜色）和项目 C（找轮廓）串起来 —— 先按颜色找到灯条，再量它们 —— 这就是校园赛识别的前半段！综合项目里你会真正做一次。

---

> 上一篇：[练手 0：从零开始写代码](00-从零开始写代码.md)
> 下一篇：[练手 2：ROS 2 两个通信小程序](02-ROS2两个通信小程序.md)
