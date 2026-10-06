# 深入 0：OpenCV 颜色分割与调参方法论

> "为什么我调的颜色参数换个光照就不行了？"
> 这是每个做视觉的人都会问的问题。这一篇把**颜色分割**这件事讲透：
> 原理、系统化调参流程、以及"让分割变稳"的技巧。

---

## 目录

1. [为什么用 HSV 而不是 BGR](#1-为什么用-hsv-而不是-bgr)
2. [HSV 空间怎么理解](#2-hsv-空间怎么理解)
3. [红色的跨 0° 问题（必须会）](#3-红色的跨-0-问题必须会)
4. [系统化调参流程（六步法）](#4-系统化调参流程六步法)
5. [怎么判断"分割质量"好不好](#5-怎么判断分割质量好不好)
6. [形态学怎么用才到位](#6-形态学怎么用才到位)
7. [光照鲁棒性：自适应与混合](#7-光照鲁棒性自适应与混合)
8. [实战案例：灯条分割的完整调参记录](#8-实战案例灯条分割的完整调参记录)

---

## 1. 为什么用 HSV 而不是 BGR

BGR 里一个颜色是三个数（蓝、绿、红），但"光照变亮"时**三个数会一起变大**：

```
同一个红色：
  亮光下：B=40,  G=30,  R=250
  暗光下：B=10,  G=8,   R=70
```

在 BGR 里写死"R > 200 才是红色"，暗光下就找不到了。

**HSV 把颜色拆成三件事，互不干扰：**

| 分量 | 含义 | 光照影响 |
| --- | --- | --- |
| H（Hue 色调） | **是哪种颜色**（红/绿/蓝） | 基本不受影响 |
| S（Saturation 饱和度） | 颜色浓不浓 | 受一点 |
| V（Value 明度） | 亮不亮 | **完全受** |

所以"找红色"在 HSV 里就是"找 H 在红色范围、S 够高"——**不用管 V 具体是多少**（除非想排除太暗的）。

**结论：按颜色找东西，一律 HSV。**

---

## 2. HSV 空间怎么理解

OpenCV 的 HSV 取值范围（**和其他软件不一样，注意！**）：

| 分量 | 范围 | 含义 |
| --- | --- | --- |
| H | **0 ~ 180** | 色相环（不是 0~360！） |
| S | 0 ~ 255 | 0=灰白，255=最鲜艳 |
| V | 0 ~ 255 | 0=全黑，255=最亮 |

**常见颜色的 H 值（OpenCV 版）：**

| 颜色 | H 大致范围 |
| --- | --- |
| 红 | 0~10 和 170~180（跨 0°） |
| 橙 | 11~25 |
| 黄 | 26~34 |
| 绿 | 35~85 |
| 青 | 86~99 |
| 蓝 | 100~130 |
| 紫 | 131~155 |
| 品红 | 156~170 |

**怎么看 H 值？** 写个小工具，鼠标指着哪里就打印哪里的 HSV：

```cpp
void onMouse(int event, int x, int y, int, void* data)
{
    if (event != cv::EVENT_LBUTTONDOWN) return;
    cv::Mat& hsv = *(cv::Mat*)data;
    cv::Vec3b v = hsv.at<cv::Vec3b>(y, x);
    std::cout << "H=" << (int)v[0] << " S=" << (int)v[1] << " V=" << (int)v[2] << "\n";
}

// main 里：
cv::setMouseCallback("窗口", onMouse, &hsv);
```

> **这个工具能解决"我不知道目标是什么颜色值"的问题 —— 点一下就知道。强烈建议自己写一个。**

---

## 3. 红色的跨 0° 问题（必须会）

色相环是圆的：红在 0° 附近，但 0° = 360°（OpenCV 里 180 = 0），
所以红色被"劈"成两端：**0~10 和 170~180**。

只取一段会丢掉一半的红色。正确做法是**两段取掩码再合并**：

```cpp
cv::Mat hsv, mask1, mask2, mask;
cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

cv::inRange(hsv, cv::Scalar(0, 100, 100),   cv::Scalar(10, 255, 255), mask1);
cv::inRange(hsv, cv::Scalar(170, 100, 100), cv::Scalar(180, 255, 255), mask2);

cv::bitwise_or(mask1, mask2, mask);     // 合并
```

**什么颜色会跨 0°？只有红色。** 其他颜色（绿、蓝、黄）取一段就行。

---

## 4. 系统化调参流程（六步法）

别瞎拖滑条。按这个顺序，三步之内就能调到能用：

```
① 定 H 范围：先别管 S/V，把 S 设 50、V 设 50（宽）
   拖动 H，让"目标颜色"全变白。看目标在色相环的哪一段。
   （红色记得两段！）
        ↓
② 收 H：两端往里收，收到"目标刚好全白、相邻颜色开始变黑"为止
        ↓
③ 调 S_low：往上抬，把"发白的、不鲜艳的"干扰排除
        ↓
④ 调 V_low：往上抬，把暗处/阴影里的干扰排除
        ↓
⑤ 加形态学：开运算去小噪点，闭运算补小洞
        ↓
⑥ 验证：换几个场景（亮/暗/不同背景）再跑一遍
```

**调参口诀：先让目标"出现"（放宽），再让目标"干净"（收紧）。**

### 完整可复用的"调参器"代码

```cpp
// 六段 HSV + 形态学核大小，全部滑条化
int h1=0, h2=10, h3=170, h4=180, s_low=100, v_low=100;

// 建滑条（省略 setTrackbarPos，见练手项目 B）

cv::Mat hsv, m1, m2, mask;
cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
cv::inRange(hsv, cv::Scalar(h1, s_low, v_low), cv::Scalar(h2, 255, 255), m1);
cv::inRange(hsv, cv::Scalar(h3, s_low, v_low), cv::Scalar(h4, 255, 255), m2);
cv::bitwise_or(m1, m2, mask);

// 形态学去噪（核大小也可以上滑条）
cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

cv::imshow("mask", mask);
```

**调好之后：** 把参数记下来写回代码（或做成 ROS 参数/配置文件）。

---

## 5. 怎么判断"分割质量"好不好

光看掩码图不严谨。用这几个指标：

### 5.1 直观标准

| 现象 | 判断 |
| --- | --- |
| 目标完整变白、无空洞 | ✅ 好 |
| 目标断成几块 | S 或形态学问题 → 闭运算、放宽 S |
| 背景大片变白 | H 范围太大 → 收紧 |
| 目标边缘毛糙 | 先模糊原图再分 |
| 目标黏着别的白色物体 | 加面积/形状筛选，别只靠颜色 |

### 5.2 量化标准（进阶）

```cpp
// 目标白色像素占比
double white_ratio = (double)cv::countNonZero(mask) / mask.total();
std::cout << "白色占比: " << white_ratio * 100 << "%\n";

// 目标轮廓面积 vs 外接矩形面积（充实程度）
double area = cv::contourArea(contour);
cv::Rect box = cv::boundingRect(contour);
double fullness = area / (box.width * box.height);
// fullness 接近 1 说明轮廓充实，接近 0 说明很散
```

**什么时候用：** 要客观对比两套参数、或者写自动化评估时。

### 5.3 调试可视化（必做）

把**原图、掩码、抠出结果**并排显示，一眼看出问题：

```cpp
cv::Mat result;
cv::bitwise_and(img, img, result, mask);

cv::imshow("原图", img);
cv::imshow("掩码", mask);
cv::imshow("抠出", result);
```

---

## 6. 形态学怎么用才到位

| 操作 | 效果 | 典型用途 |
| --- | --- | --- |
| 开运算 OPEN | 先腐蚀后膨胀 | **去小白噪点** |
| 闭运算 CLOSE | 先膨胀后腐蚀 | **补小洞、连断裂** |
| 腐蚀 ERODE | 白缩小 | 断开细连接 |
| 膨胀 DILATE | 白扩大 | 加粗、连接 |

### 核（kernel）怎么选

```cpp
cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
```

| 核 | 说明 |
| --- | --- |
| `MORPH_RECT` | 方形（最常用） |
| `MORPH_ELLIPSE` | 椭圆（边缘更平滑） |
| `MORPH_CROSS` | 十字（保留细线） |

**核大小经验：**

- 目标很小（几十像素）→ 3×3 就够，5×5 可能把目标吃掉。
- 目标较大 → 5×5 或 7×7。
- **原则：核要比"噪点大"，比"目标小"。** 不确定就从小到大试。

### 常见组合（背下来）

```cpp
// 去噪 + 连接断裂 + 再修边缘 —— "三段式"
cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  kernel5);    // 去小白点
cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel9);    // 补洞连断
cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  kernel5);    // 修掉粘连
```

---

## 7. 光照鲁棒性：自适应与混合

### 7.1 问题：全局参数扛不住光照变化

一个固定阈值，白天好用、晚上就废。三种解法，按复杂度排序：

### 7.2 方案 A：把 V_low 放低，主要靠 H、S

因为 H、S 对光照相对不敏感，**尽量不用 V 来做主要筛选**：

```cpp
// 差：把 V_low 设很高，一暗就全丢
// 好：V_low 只排除"接近全黑"，主要靠 H 和 S
cv::inRange(hsv, cv::Scalar(h1, 100, 30), cv::Scalar(h2, 255, 255), mask);
```

### 7.3 方案 B：先用别的通道做亮度归一

"红 − 蓝差值"（校园赛的做法）本质上就是**利用通道差抵消整体亮度**：

```cpp
// 光照整体变亮时，红、蓝一起变亮，相减后几乎不变
cv::subtract(red, blue, red_diff);
```

### 7.4 方案 C：自适应阈值

```cpp
// 对灰度图（或差值图）用自适应阈值，每个小区域自己定阈值
cv::adaptiveThreshold(gray, dst, 255,
                      cv::ADAPTIVE_THRESH_MEAN_C,
                      cv::THRESH_BINARY, 31, 5);
```

### 7.5 方案 D：多套参数 + 自动选择

光照变化太极端时：备两套参数，运行时根据"当前亮度"选择：

```cpp
double brightness = cv::mean(gray)[0];
bool is_dark = brightness < 80;
// 用不同的 H/S/V 范围
```

### 7.6 总结

| 场景 | 方案 |
| --- | --- |
| 光照基本稳定 | 固定 HSV 参数 |
| 光照变化但颜色单一 | 通道差（红−蓝） |
| 光照很不均匀 | 自适应阈值 |
| 白天黑夜切换 | 多套参数 + 亮度判断 |

---

## 8. 实战案例：灯条分割的完整调参记录

**目标：** 从游戏画面里分割出红色灯条。

**第一次尝试（失败）：**

```cpp
cv::threshold(red_diff, binary, 100, 255, cv::THRESH_BINARY);
// 结果：灯条出来了，但地面反光也出来了（都是亮的）
```

**分析：** 只靠亮度不够，反光干扰太大。

**第二次尝试（改进）：** 加面积筛选

```cpp
if (area < 70 || area > 5000) continue;
// 结果：小噪点没了，但大片反光还在
```

**分析：** 反光面积大，但形状是"一片"，灯条是"细长"。

**第三次尝试（成功）：** 加形状筛选

```cpp
cv::RotatedRect rect = cv::minAreaRect(contour);
float w = rect.size.width, h = rect.size.height;
if (w > h) std::swap(w, h);          // 归一
if (h / w < 1.5) continue;           // 细长
if (std::abs(rect.angle) > 30) continue;   // 接近竖直
// 结果：只有灯条留下来了
```

**教训（也是通用方法论）：**

> **颜色 → 面积 → 形状 → 位置**，一层层加约束。
> 先靠颜色"海选"，再用几何特征"精选"，永远比死磕单一阈值强。

---

> 上一篇：[练手 3：综合 —— 迷你视觉瞄准项目](../05-练手项目/03-综合迷你视觉瞄准项目.md)
> 下一篇：[深入 1：相机 PnP 进阶](01-深入相机PnP进阶.md)
