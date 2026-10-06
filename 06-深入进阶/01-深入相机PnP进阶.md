# 深入 1：相机 PnP 进阶

> 会用 `solvePnP` 只是开始。这一篇把**容易踩坑、决定精度的细节**讲透：
> 角点怎么排序、结果怎么验证、怎么滤波不抖、多目标怎么处理。
> 学完你的测距和瞄准能上一个台阶。

---

## 目录

1. [第一大坑：角点顺序](#1-第一大坑角点顺序)
2. [角点排序算法（完整代码）](#2-角点排序算法完整代码)
3. [验证 PnP 结果对不对](#3-验证-pnp-结果对不对)
4. [从 rvec 拿到欧拉角](#4-从-rvec-拿到欧拉角)
5. [坐标约定：别把方向搞反](#5-坐标约定别把方向搞反)
6. [多目标处理](#6-多目标处理)
7. [结果滤波：让角度不抖](#7-结果滤波让角度不抖)
8. [提高精度的手段汇总](#8-提高精度的手段汇总)

---

## 1. 第一大坑：角点顺序

`solvePnP` 的核心规则只有一条：

> **`objectPoints[i]` 和 `imagePoints[i]` 必须是同一个物理点。**

`objectPoints`（3D 点）是你自己定义的，顺序可控；
问题出在 `imagePoints`（图像角点）—— 它们来自 `minAreaRect`，
**`rect.points()` 返回的顺序不保证是"左上、右上、右下、左下"**，
甚至每一帧都可能变（因为矩形旋转方向随目标角度变化）。

**后果：** 点和点对应不上 → 解算出来的位姿乱跳、方向反、距离离谱。

**解决：** 拿到角点后**自己排序**，固定成"左上→右上→右下→左下"。

---

## 2. 角点排序算法（完整代码）

### 方法 A：x+y / x−y 法（经典，推荐）

原理：

- **左上角**：x+y 最小（最靠近原点）。
- **右下角**：x+y 最大。
- 剩下两个点：右上（x 大）和左下（x 小），用 x−y 或 x 来分。

```cpp
#include <opencv2/opencv.hpp>
#include <algorithm>

// 把 4 个角点排成固定顺序：左上、右上、右下、左下
std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point2f>& pts)
{
    std::vector<cv::Point2f> out(4);
    std::vector<float> s, d;
    for (const auto& p : pts) {
        s.push_back(p.x + p.y);    // 左上最小，右下最大
        d.push_back(p.x - p.y);    // 右上偏大，左下偏小
    }

    out[0] = pts[std::min_element(s.begin(), s.end()) - s.begin()];  // 左上
    out[2] = pts[std::max_element(s.begin(), s.end()) - s.begin()];  // 右下
    out[1] = pts[std::max_element(d.begin(), d.end()) - d.begin()];  // 右上
    out[3] = pts[std::min_element(d.begin(), d.end()) - d.begin()];  // 左下
    return out;
}

// 使用
std::vector<cv::Point2f> pts;
rect.points(pts.data());                  // 注意：先拿到 4 个角点
std::vector<cv::Point2f> ordered = orderCorners(pts);
```

> `rect.points(pts.data())` 需要数组；也可以 `Point2f arr[4]; rect.points(arr);`
> 再 `std::vector<cv::Point2f> pts(arr, arr+4);`

### 方法 B：先分左右，再分上下（更直观）

```cpp
std::vector<cv::Point2f> orderCorners2(const std::vector<cv::Point2f>& pts)
{
    // 按 x 排序，前两个是左，后两个是右
    auto sorted = pts;
    std::sort(sorted.begin(), sorted.end(),
              [](const cv::Point2f& a, const cv::Point2f& b){ return a.x < b.x; });

    cv::Point2f leftTop    = sorted[0].y < sorted[1].y ? sorted[0] : sorted[1];
    cv::Point2f leftBottom = sorted[0].y < sorted[1].y ? sorted[1] : sorted[0];
    cv::Point2f rightTop    = sorted[2].y < sorted[3].y ? sorted[2] : sorted[3];
    cv::Point2f rightBottom = sorted[2].y < sorted[3].y ? sorted[3] : sorted[2];

    return {leftTop, rightTop, rightBottom, leftBottom};
}
```

### 排序后立刻验证

排序对不对，画出来看一眼：

```cpp
for (int i = 0; i < 4; i++) {
    cv::putText(img, std::to_string(i), ordered[i],
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
}
```

> 0=左上、1=右上、2=右下、3=左下。**校园赛的 `main.cpp` 就是这么干的（写编号到角点旁）。**

---

## 3. 验证 PnP 结果对不对

PnP 算完不验证等于没算。两种验证：

### 3.1 projectPoints 画回（最直观）

把 3D 模型按解算出的位姿投影回图像，和真实角点对比：

```cpp
std::vector<cv::Point2f> projected;
cv::projectPoints(objectPoints, rvec, tvec, cameraMatrix, distCoeffs, projected);

for (size_t i = 0; i < projected.size(); i++) {
    cv::circle(img, projected[i], 3, cv::Scalar(0, 255, 0), -1);   // 绿色：投影
    cv::circle(img, imagePoints[i], 3, cv::Scalar(0, 0, 255), -1); // 红色：实际
}
```

**绿色和红色重合 → 解算正确。差很远 → 角点顺序错或内参错。**

### 3.2 重投影误差（量化）

```cpp
double err = 0;
for (size_t i = 0; i < projected.size(); i++) {
    err += cv::norm(projected[i] - imagePoints[i]);
}
double mean_err = err / projected.size();
std::cout << "平均重投影误差: " << mean_err << " 像素\n";
// 一般 < 1~2 像素说明不错；> 5 像素有问题
```

### 3.3 基本 sanity check（先查这个）

```cpp
double tz = tvec.at<double>(2);   // 距离
if (tz <= 0) {
    // 目标应该在相机前方，tz 为负说明角点顺序/方向错
    std::cout << "警告：tz = " << tz << "，结果不可信\n";
}
```

---

## 4. 从 rvec 拿到欧拉角

`solvePnP` 输出的 `rvec` 是**旋转向量**（轴角表示），不是欧拉角。
想拿 yaw/pitch/roll，要先转旋转矩阵：

```cpp
cv::Mat R;
cv::Rodrigues(rvec, R);      // 旋转向量 → 3x3 旋转矩阵

// 从旋转矩阵提取欧拉角（相机系 Z-Y-X 约定，符号要实测验证）
double sy = std::sqrt(R.at<double>(0,0)*R.at<double>(0,0) +
                      R.at<double>(1,0)*R.at<double>(1,0));
double yaw, pitch, roll;
if (sy > 1e-6) {
    yaw   = std::atan2(R.at<double>(1,0), R.at<double>(0,0));
    pitch = std::atan2(-R.at<double>(2,0), sy);
    roll  = std::atan2(R.at<double>(2,1), R.at<double>(2,2));
} else {
    // 万向锁特殊处理
    yaw   = std::atan2(-R.at<double>(1,2), R.at<double>(1,1));
    pitch = std::atan2(-R.at<double>(2,0), sy);
    roll  = 0;
}
// 结果单位是弧度，转角度 ×180/π
```

> ⚠️ 欧拉角提取的公式因"旋转顺序约定"不同而不同，**务必用实测验证符号**。
> 校园赛场景：**只用 tvec 算 yaw/pitch（atan2），通常不需要上面的欧拉角。**

---

## 5. 坐标约定：别把方向搞反

OpenCV 相机坐标系：

```
+Z：向前（光轴方向）
+X：向右
+Y：向下
```

所以：

```cpp
double yaw   = atan2(tvec(0), tvec(2));   // 目标在右 → yaw 为正
double pitch = atan2(tvec(1), tvec(2));   // 目标在下 → pitch 为正
```

**但外部系统的约定可能不一样**（比如游戏的 yaw 以 +Z 为 0、顺时针为正）。
换算要点：

1. 查文档：读接口说明里的角度定义。
2. 实测验证：目标放右边，看输出符号和大小对不对。
3. 加上当前云台角度：`世界角度 = 云台当前角度 + 相机系偏角`（正负看约定）。

**坐标系问题没有捷径，就是"文档 + 实测"双确认。**

---

## 6. 多目标处理

画面里多个装甲板时：

```cpp
for (auto& armor : armors) {
    std::vector<cv::Point3f> objectPoints = { ... };
    // 用这块装甲板自己的 4 个角点
    std::vector<cv::Point2f> imagePoints = armor.pnpCorners;

    cv::Mat rvec, tvec;
    bool ok = cv::solvePnP(objectPoints, imagePoints,
                           cameraMatrix, distCoeffs, rvec, tvec,
                           false, cv::SOLVEPNP_IPPE);
    if (!ok || tvec.at<double>(2) <= 0) continue;

    armor.distance = tvec.at<double>(2);
    // 存下结果，最后选"最近的"或"正在打的"
}

// 选目标：通常选距离最近、或接近画面中心的
auto target = std::min_element(armors.begin(), armors.end(),
    [](const Armor& a, const Armor& b){ return a.distance < b.distance; });
```

**选择策略：**

| 策略 | 适用 |
| --- | --- |
| 最近的 | 先解决威胁大的 |
| 最接近画面中心 | 云台转得最少、最快到位 |
| 正在跟踪的（帧间关联） | 稳定跟踪，不跳目标 |

---

## 7. 结果滤波：让角度不抖

PnP 结果每帧都有噪声，直接发出去会抖。三种滤波：

### 7.1 指数滑动平均（EMA，最简单好用）

```cpp
// 权重 α：越大跟得越快，越小越平滑（0.1~0.3 常用）
double alpha = 0.2;
static double smooth_yaw = 0;
static bool has_value = false;

if (!has_value) {
    smooth_yaw = yaw;          // 第一帧直接采信
    has_value = true;
} else {
    smooth_yaw = alpha * yaw + (1 - alpha) * smooth_yaw;
}
```

### 7.2 中值滤波（抗野值）

```cpp
#include <deque>
#include <algorithm>

std::deque<double> history;      // 存最近 N 个值
const size_t N = 5;

void addAndGet(double value) {
    history.push_back(value);
    if (history.size() > N) history.pop_front();

    auto sorted = std::vector<double>(history.begin(), history.end());
    std::sort(sorted.begin(), sorted.end());
    double median = sorted[sorted.size() / 2];   // 中值：抗单帧野值
    // 用 median
}
```

### 7.3 混合：中值 + 滑动平均（推荐组合）

```cpp
// 1. 先中值去野值（上面的代码）
// 2. 再对中值做滑动平均
double sum = 0;
for (double v : sorted) sum += v;
double mean = sum / sorted.size();
```

### 7.4 使用建议

| 场景 | 推荐 |
| --- | --- |
| 目标平稳移动 | EMA（α≈0.2） |
| 偶尔有野值（误匹配） | 中值滤波 |
| 两者都有 | 中值 + 均值 |
| 想预测运动 | 卡尔曼（进阶，需要模型） |

**另外：** 只在"识别成功"时更新滤波器，丢目标时保持上一值或标记无效。

---

## 8. 提高精度的手段汇总

按性价比排序：

| 手段 | 做法 | 收益 |
| --- | --- | --- |
| 角点顺序正确 | 排序 + 编号验证 | **必须** |
| 真实尺寸准确 | 用尺子量、查规则手册 | 高（距离直接正比） |
| 内参准确 | 标定 / 用游戏给的内参 | 高 |
| 畸变传入 | `distCoeffs` 别传空 | 中~高（边缘目标） |
| 滤波 | EMA/中值 | 中（不抖） |
| 亚像素角点 | `cornerSubPix` | 中 |
| 多帧平均 3D 点 | 连续 N 帧 tvec 平均 | 中 |
| RANSAC | 抗错误角点 | 中 |
| RefineLM | 精化 | 小~中 |
| 提升分辨率/光学变焦 | 硬件 | 大（但改不了） |

**心法：精度问题先查"顺序对不对、尺寸准不准、内参对不对"，而不是先去调算法参数。**

---

> 上一篇：[深入 0：OpenCV 颜色分割与调参方法论](00-深入OpenCV颜色分割与调参.md)
> 下一篇：[深入 2：ROS 2 通信进阶](02-深入ROS2通信进阶.md)
