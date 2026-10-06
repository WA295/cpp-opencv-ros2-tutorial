# OpenCV 相机模型、PnP 与标定

> 这一篇解释"**相机是怎么把一个 3D 世界拍成 2D 图片的**"，以及怎么反推回去：
> 目标在哪、离多远、偏多少角度。核心函数是 `solvePnP`。
> 对应头文件：`#include <opencv2/calib3d.hpp>`。

---

## 目录

1. [针孔相机模型](#1-针孔相机模型)
2. [内参矩阵 K](#2-内参矩阵-k)
3. [畸变系数 D](#3-畸变系数-d)
4. [四个坐标系](#4-四个坐标系)
5. [solvePnP：由 2D 求 3D 位姿](#5-solvepnp由-2d-求-3d-位姿)
6. [由位姿算角度和距离](#6-由位姿算角度和距离)
7. [projectPoints：反过来投影](#7-projectpoints反过来投影)
8. [相机标定 calibrateCamera](#8-相机标定-calibratecamera)
9. [ROS 2 里的相机参数](#9-ros-2-里的相机参数)
10. [实战：校园赛测距与对准](#10-实战校园赛测距与对准)
11. [常见坑](#11-常见坑)

---

## 1. 针孔相机模型

最简单的理解：相机就是一个小孔成像。

```
3D 世界中的点 P(X, Y, Z)
        │  光线穿过光心
        ▼
2D 图像上的点 p(u, v)
```

数学上（忽略畸变）：

```
u = fx * X/Z + cx
v = fy * Y/Z + cy
```

| 符号 | 含义 |
| --- | --- |
| `X, Y, Z` | 点相对**相机**的坐标（Z 是"前方距离"） |
| `fx, fy` | 焦距（单位是像素） |
| `cx, cy` | 光心（图像中心附近）的像素坐标 |
| `u, v` | 图像上的像素坐标 |

**关键结论：**

- 同一个物体，**离得越远（Z 越大），在图像里越小（u 越靠近中心）**。
- 光知道一个像素坐标 `(u,v)`，**推不出**唯一的 3D 位置（因为 Z 未知）——这就是"深度丢失"。
- 要想反推，必须有**额外信息**（比如知道物体的真实尺寸），这正是 PnP 做的事。

---

## 2. 内参矩阵 K

把上面的公式写成矩阵：

```
K = [ fx   0   cx ]
    [  0  fy   cy ]
    [  0   0    1 ]
```

- `K` 叫**内参矩阵**（intrinsic matrix），只和相机本身有关（镜头、传感器），和拍什么无关。
- OpenCV 用 `cv::Mat cameraMatrix` 存，`Mat::eye(3,3,CV_64F)` 是初值。
- ROS 2 的 `sensor_msgs/CameraInfo` 里，`k` 就是按行排列的这 9 个数：

```cpp
// msg->camera_info.k = [fx, 0, cx, 0, fy, cy, 0, 0, 1]
cv::Mat cameraMatrix = (cv::Mat_<double>(3,3) <<
    msg->camera_info.k[0], msg->camera_info.k[1], msg->camera_info.k[2],
    msg->camera_info.k[3], msg->camera_info.k[4], msg->camera_info.k[5],
    msg->camera_info.k[6], msg->camera_info.k[7], msg->camera_info.k[8]);
```

**量纲记忆：** `fx, fy, cx, cy` 的单位都是**像素**。

---

## 3. 畸变系数 D

真实镜头会"变形"：

| 畸变类型 | 现象 | 系数 |
| --- | --- | --- |
| 径向畸变（桶形/枕形） | 直线变弯 | `k1, k2, k3` |
| 切向畸变 | 图像轻微倾斜拉歪 | `p1, p2` |

OpenCV 按 `[k1, k2, p1, p2, k3]` 的顺序存畸变系数：

```cpp
// msg->camera_info.d 就是这串数
cv::Mat distCoeffs(msg->camera_info.d);     // 直接用它
```

- 畸变对**测距和角度**影响不小，尤其是广角镜头和图像边缘。
- `solvePnP` 会自动用畸变系数校正，所以**一定要把 D 传进去**。
- 平角/无畸变的模拟相机，D 可能是空的或全 0。

---

## 4. 四个坐标系

搞清这四个，PnP 就不再神秘：

```
世界坐标系 (World)
   │ 外参：旋转 R + 平移 t（相机在哪、朝哪）
   ▼
相机坐标系 (Camera)：Z 向前，X 向右，Y 向下
   │ 内参：K（fx, fy, cx, cy）+ 畸变 D
   ▼
图像坐标系 (Image)：以像素为单位，(u, v)
   │ 去掉光心偏移/除以焦距
   ▼
归一化平面坐标：x = X/Z, y = Y/Z
```

**PnP 解决的问题：** 已知物体上几个点的**3D 世界坐标**和它们在图像里的**2D 像素坐标**，
求**相机相对物体的位置和姿态**（即外参）。

---

## 5. solvePnP：由 2D 求 3D 位姿

### 5.1 函数签名

```cpp
bool cv::solvePnP(InputArray objectPoints,    // 物体上的 3D 点（世界坐标）
                  InputArray imagePoints,     // 对应的 2D 图像点
                  InputArray cameraMatrix,    // 内参 K
                  InputArray distCoeffs,      // 畸变 D
                  OutputArray rvec,           // 输出：旋转向量
                  OutputArray tvec,           // 输出：平移向量
                  bool useExtrinsicGuess = false,
                  int flags = SOLVEPNP_ITERATIVE);
```

返回 `bool`：是否成功。

### 5.2 参数详解

| 参数 | 说明 |
| --- | --- |
| `objectPoints` | `vector<Point3f>`，物体上各点的 3D 坐标（**用物体自己的坐标系**，单位任意但要和真实尺寸一致） |
| `imagePoints` | `vector<Point2f>`，这些点在图像里的像素坐标 |
| `cameraMatrix` | 3×3 内参 `cv::Mat`（CV_64F） |
| `distCoeffs` | 畸变系数（可以是空 Mat） |
| `rvec` | 输出 3×1 旋转向量（罗德里格斯表示） |
| `tvec` | 输出 3×1 平移向量 = **物体在相机坐标系下的位置** |

**最关键的一条规则：`objectPoints[i]` 和 `imagePoints[i]` 必须是同一个点。**
顺序错一个，结果全错。这是新手第一大错。

### 5.3 flags 选哪个

| flag | 说明 | 什么时候用 |
| --- | --- | --- |
| `SOLVEPNP_ITERATIVE` | 通用迭代法（默认） | 点较多、非平面 |
| `SOLVEPNP_IPPE` | **平面目标**专用 | 装甲板、棋盘格（**最常用**） |
| `SOLVEPNP_IPPE_SQUARE` | 正方形平面 | 正方形标定板 |
| `SOLVEPNP_EPNP` | 快速、点少 | 点数 ≥ 4，速度快 |
| `SOLVEPNP_P3P` | 只要 3 个点 | 恰好 3 点时 |
| `SOLVEPNP_SQPNP` | 点多更准（OpenCV 4.1+） | 点数多、精度要求高 |

> 平面目标（4 个点在一个平面上）用 `SOLVEPNP_IPPE` 最稳。

### 5.4 完整例子

```cpp
#include <opencv2/calib3d.hpp>

// 1. 物体 3D 点：装甲板宽 0.135m、高 0.055m，物体中心为原点，z=0 平面
float W = 0.135f / 2, H = 0.055f / 2;
std::vector<cv::Point3f> objectPoints = {
    {-W, -H, 0},   // 左上
    { W, -H, 0},   // 右上
    { W,  H, 0},   // 右下
    {-W,  H, 0},   // 左下
};

// 2. 对应的图像 2D 点（顺序必须一一对应！）
//    来自 minAreaRect 的角点，已按"左上→右上→右下→左下"排好
std::vector<cv::Point2f> imagePoints = {p1, p2, p3, p4};

// 3. 解算
cv::Mat rvec, tvec;
bool ok = cv::solvePnP(objectPoints, imagePoints,
                       cameraMatrix, distCoeffs,
                       rvec, tvec, false, cv::SOLVEPNP_IPPE);

// 4. 用结果
if (ok) {
    double x = tvec.at<double>(0);   // 右为正
    double y = tvec.at<double>(1);   // 下为正
    double z = tvec.at<double>(2);   // 前为正（距离）
    std::cout << "距离 " << z << " m, 偏移 (" << x << ", " << y << ")\n";
}
```

### 5.5 四个角点的顺序怎么排

`minAreaRect` 返回的 4 个角点**顺序不固定**（取决于点的排列和方向）。
你需要在代码里把它排成固定顺序（如"左上、右上、右下、左下"）：

```cpp
// 方法：用 sum(x+y) 最小的是左上，最大的是右下；用差来分右上/左下
std::vector<cv::Point2f> orderPoints(const std::vector<cv::Point2f>& pts) {
    std::vector<cv::Point2f> out(4);
    std::vector<float> s, d;
    for (auto& p : pts) { s.push_back(p.x + p.y); d.push_back(p.x - p.y); }

    out[0] = pts[std::min_element(s.begin(), s.end()) - s.begin()];  // 左上
    out[2] = pts[std::max_element(s.begin(), s.end()) - s.begin()];  // 右下
    out[1] = pts[std::max_element(d.begin(), d.end()) - d.begin()];  // 右上
    out[3] = pts[std::min_element(d.begin(), d.end()) - d.begin()];  // 左下
    return out;
}
```

> 或者简单粗暴：先按 x 分成左右两组，每组再按 y 分上下，得到"左上下、右上下"。

### 5.6 更稳的版本

```cpp
// 有误匹配/噪声时用 RANSAC 版
bool ok = cv::solvePnPRansac(objectPoints, imagePoints, cameraMatrix, distCoeffs,
                             rvec, tvec, false, 100, 8.0, 0.99, inliers,
                             cv::SOLVEPNP_ITERATIVE);

// 想更进一步优化
cv::solvePnPRefineLM(objectPoints, imagePoints, cameraMatrix, distCoeffs, rvec, tvec);
```

---

## 6. 由位姿算角度和距离

拿到 `tvec`（目标在相机系下的位置）后：

```cpp
double tx = tvec.at<double>(0);   // 右为正
double ty = tvec.at<double>(1);   // 下为正
double tz = tvec.at<double>(2);   // 前为正（距离）

// 相机坐标系下的偏角（弧度 → 角度）
double yaw   = std::atan2(tx, tz) * 180.0 / CV_PI;   // 右偏为正
double pitch = std::atan2(ty, tz) * 180.0 / CV_PI;   // 下偏为正

// 距离（沿光轴）
double dist = tz;
// 或直线距离：
double dist3d = cv::norm(cv::Vec3d(tx, ty, tz));
```

**注意方向约定：**

- 相机系：+Z 向前、+X 向右、+Y 向下（OpenCV 约定）。
- 所以 `yaw > 0` 表示目标在右边，`pitch > 0` 表示目标在下方。
- 游戏要求的"世界光轴角"可能以别的方向为零点（例如 Unity +Z=0、顺时针为正），
  需要**加上当前云台的世界角度**并确认符号，这一步很容易出错，务必用实测验证。

**增加云台当前角度得到世界角度（示例）：**

```cpp
double world_yaw = last_state.yaw_degrees + yaw;      // 视接口定义而定
```

---

## 7. projectPoints：反过来投影

已知 3D 点和相机位姿，求它们会落在图像的哪儿（用于画虚拟框、验证）：

```cpp
void cv::projectPoints(InputArray objectPoints, InputArray rvec, InputArray tvec,
                       InputArray cameraMatrix, InputArray distCoeffs,
                       OutputArray imagePoints);
```

```cpp
std::vector<cv::Point2f> projected;
cv::projectPoints(objectPoints, rvec, tvec, cameraMatrix, distCoeffs, projected);

// 把投影点画出来：如果和真实目标重合，说明位姿解算正确
for (auto& p : projected) cv::circle(img, p, 3, cv::Scalar(0,255,0), -1);
```

**什么时候用：** **验证 PnP 结果**！把 3D 模型投影回图像，和实际目标对比，能一眼看出对错。

---

## 8. 相机标定 calibrateCamera

标定 = 求内参 K 和畸变 D。常用棋盘格。

```cpp
// 准备：多张不同角度的棋盘格照片
std::vector<std::vector<cv::Point3f>> objectPoints;   // 棋盘格的 3D 点
std::vector<std::vector<cv::Point2f>> imagePoints;    // 检测到的角点
cv::Size imageSize = img.size();

// 1. 对每张图 findChessboardCorners 找角点
cv::Mat gray;
cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
std::vector<cv::Point2f> corners;
bool found = cv::findChessboardCorners(gray, cv::Size(9, 6), corners);

// 2. 亚像素精化（更准）
cv::cornerSubPix(gray, corners, cv::Size(11,11), cv::Size(-1,-1),
    cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001));

// 3. 标定
cv::Mat cameraMatrix, distCoeffs;
std::vector<cv::Mat> rvecs, tvecs;
double rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize,
                                 cameraMatrix, distCoeffs, rvecs, tvecs);
std::cout << "重投影误差 " << rms << "（越小越好，<1 一般可接受）\n";

// 4. 保存
cv::FileStorage fs("calib.yml", cv::FileStorage::WRITE);
fs << "K" << cameraMatrix << "D" << distCoeffs;
```

**什么时候用：** 你自己的相机要精度时。**模拟器/游戏一般直接给你相机参数**，
所以校园赛项目不用自己标定，直接订阅 `camera_info` 就行。

**标定板注意：** 棋盘格角点数写 `Size(列内角数, 行内角数)`（不是格子数！）。

---

## 9. ROS 2 里的相机参数

`/autoaim/frame` 消息里的 `camera_info`（类型 `sensor_msgs/CameraInfo`）：

| 字段 | 含义 | 怎么用 |
| --- | --- | --- |
| `k` | 内参 3×3（行优先，9 个数） | 填 `cameraMatrix` |
| `d` | 畸变系数 | 填 `distCoeffs` |
| `width` / `height` | 图像尺寸 | 检查一致性 |
| `distortion_model` | 畸变模型名 | 一般 `plumb_bob` |
| `p` | 投影矩阵 | 立体视觉才用 |

```cpp
// 收到消息后缓存起来（不要每帧重新构造）
if (msg->camera_info.k.size() >= 9) {
    cameraMatrix_ = (cv::Mat_<double>(3,3) <<
        msg->camera_info.k[0], msg->camera_info.k[1], msg->camera_info.k[2],
        msg->camera_info.k[3], msg->camera_info.k[4], msg->camera_info.k[5],
        msg->camera_info.k[6], msg->camera_info.k[7], msg->camera_info.k[8]);
}
if (!msg->camera_info.d.empty()) {
    distCoeffs_ = cv::Mat(msg->camera_info.d).clone();
}
```

**用之前一定检查：** `cameraMatrix_.empty()` / `distCoeffs_.empty()`，
否则 `solvePnP` 会失败或给错误结果。

---

## 10. 实战：校园赛测距与对准

把整条链路写清楚（这正是校园赛项目做的事）：

```
① 识别到装甲板，拿到它的 4 个角点（图像像素坐标）
        │
② 把 4 个角点排成固定顺序（左上→右上→右下→左下）
        │
③ 构造对应的 3D 点（用装甲板真实尺寸，中心为原点，z=0）
        │
④ solvePnP(objects, images, K, D, rvec, tvec, false, SOLVEPNP_IPPE)
        │
⑤ tvec = 目标在相机系下位置 → 算 yaw/pitch/距离
        │
⑥ 加上云台当前世界角度 → 得到世界角度
        │
⑦ 填进 SendData 发布给游戏
```

**精度提升建议：**

| 手段 | 效果 |
| --- | --- |
| 角点顺序正确且一致 | **必须**，否则全错 |
| 真实尺寸量准 | 直接影响距离精度 |
| 用亚像素角点（可选） | 提高角点精度 |
| 多帧平均（滤波） | 抑制抖动 |
| 用 `solvePnPRansac` | 抗误匹配 |
| `solvePnPRefineLM` | 精化结果 |

---

## 11. 常见坑

| 坑 | 现象 | 解决 |
| --- | --- | --- |
| object/image 点顺序不一致 | 位姿乱跳、方向反 | 两边用同一套顺序，写注释 |
| 不检查 `cameraMatrix_.empty()` | solvePnP 失败/崩溃 | 用前判空 |
| 尺寸单位混乱 | 距离差 10 倍/100 倍 | 统一用米（或统一用别的），文档写清楚 |
| `tvec.at<double>` 用成 `<float>` | 读到乱码 | PnP 输出是 `CV_64F` |
| 直接把相机系角度当世界角度 | 瞄准偏 180°/反向 | 加上云台角度并确认符号 |
| 忘传畸变 | 边缘目标测距不准 | 把 `distCoeffs` 传进去 |
| 平面目标却用 `ITERATIVE` | 结果不稳 | 用 `SOLVEPNP_IPPE` |
| 角点取自不同灯条 | 目标位置错 | 确保 4 个角点来自同一块装甲板 |

---

> 上一篇：[OpenCV 轮廓、形状与几何](02-轮廓形状与几何.md)
> 下一篇：[OpenCV 机器学习（SVM）与数字识别](04-机器学习SVM与数字识别.md)
