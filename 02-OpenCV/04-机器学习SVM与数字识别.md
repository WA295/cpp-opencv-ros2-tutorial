# OpenCV 机器学习（SVM）与数字识别

> 这一篇讲"怎么让程序自己学会认东西"，重点是 OpenCV 自带的 **SVM**。
> 校园赛里"认出装甲板上的数字 1~8"就是这一篇的内容。
> 对应头文件：`#include <opencv2/ml.hpp>`。

---

## 目录

1. [机器学习是什么（大白话）](#1-机器学习是什么大白话)
2. [OpenCV 的 ml 模块](#2-opencv-的-ml-模块)
3. [SVM 是怎么回事](#3-svm-是怎么回事)
4. [训练一个 SVM：完整代码](#4-训练一个-svm完整代码)
5. [加载与预测](#5-加载与预测)
6. [特征工程：怎么把图片变成数字](#6-特征工程怎么把图片变成数字)
7. [完整的数字识别流程](#7-完整的数字识别流程)
8. [KNN：另一种简单分类器](#8-knn另一种简单分类器)
9. [深度学习（dnn 模块）简介](#9-深度学习dnn-模块简介)
10. [实战建议与常见坑](#10-实战建议与常见坑)

---

## 1. 机器学习是什么（大白话）

传统编程：

```
你写规则 → 程序执行
（if 红色 → 是灯条）
```

机器学习：

```
你给例子（图片 + 正确答案）→ 程序自己总结规则 → 以后遇到新图片自己判断
```

**三个关键词：**

| 词 | 含义 | 数字识别里的例子 |
| --- | --- | --- |
| 样本（sample） | 一个输入 | 一张数字图片 |
| 标签（label） | 正确答案 | 数字 3 |
| 特征（feature） | 输入被转成的一串数字 | 20×28 的灰度值拉平成 560 个数 |
| 训练（train） | 用样本+标签让模型学 | 用几千张图训练 |
| 预测（predict） | 用模型判断新样本 | 输入新图，输出 3 |

**关键点：** 机器不认识"图片"，它只认识**一串数字（特征向量）**。
所以"把图片转成特征"这步（特征工程）非常关键。

---

## 2. OpenCV 的 ml 模块

OpenCV 自带几个经典机器学习算法（不用装别的库）：

| 类 | 算法 | 什么时候用 |
| --- | --- | --- |
| `cv::ml::SVM` | 支持向量机 | **分类首选**，小样本表现好（本项目用） |
| `cv::ml::KNearest` | K 近邻 | 更简单，数据多时可用 |
| `cv::ml::NormalBayesClassifier` | 朴素贝叶斯 | 特征独立时 |
| `cv::ml::ANN_MLP` | 多层感知机（浅层神经网络） | 非线性复杂问题 |
| `cv::ml::DTrees` / `Boost` / `RTrees` | 决策树/提升/随机森林 | 表格类数据 |

**共同的使用套路（都差不多）：**

```
创建 → 设置参数 → 喂数据 train() → 保存 save() → 加载 load() → predict()
```

---

## 3. SVM 是怎么回事

### 3.1 一句话理解

SVM 要在两类点之间**画一条最宽的分界线**：

```
      ○ ○
   ○     ○      ← 分界线尽量离两边都远
  ────────────
     ●  ●
   ●     ●
```

- 二维时是"线"，三维是"平面"，更高维叫"超平面"。
- "最大间隔"：分界线离最近的样本越远越好 → 泛化能力强。
- 多分类（1~8）：SVM 内部用"一对一"组合多个二分类器。

### 3.2 核函数（kernel）：解决"分不开"的问题

有些数据在低维分不开，SVM 可以把它们映射到高维去分：

| kernel | 特点 | 什么时候用 |
| --- | --- | --- |
| `LINEAR` | 直线分 | 数据本身线性可分、特征多 |
| `RBF` | 曲线边界，万能 | **最常用**（本项目用的就是 RBF） |
| `POLY` | 多项式 | 特殊场景 |
| `SIGMOID` | 类似神经网络 | 少用 |

### 3.3 两个重要参数

| 参数 | 含义 | 大/小的影响 |
| --- | --- | --- |
| `C` | 惩罚系数：错分一个点罚多重 | C 大 → 容忍错分少、可能过拟合；C 小 → 更宽松 |
| `gamma` | RBF 的"影响半径" | gamma 大 → 每个点影响小、可能过拟合；gamma 小 → 更平滑 |

> 本项目模型 `digit_svm.yml` 里：`kernel=RBF, C=100, gamma≈0.00714`。

---

## 4. 训练一个 SVM：完整代码

**场景：** 有 8 个文件夹（`1/` ~ `8/`），每个文件夹里放着对应数字的图片。

```cpp
#include <opencv2/opencv.hpp>
#include <opencv2/ml.hpp>
#include <filesystem>       // C++17
#include <iostream>

namespace fs = std::filesystem;

int main() {
    std::vector<cv::Mat> samples;      // 每个样本：1 x 560 的 CV_32F
    std::vector<int> labels;           // 对应标签：1~8

    // 1. 读数据
    for (int digit = 1; digit <= 8; digit++) {
        std::string dir = "dataset/" + std::to_string(digit);
        for (auto& entry : fs::directory_iterator(dir)) {
            cv::Mat img = cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE);
            if (img.empty()) continue;

            cv::Mat gray;
            cv::resize(img, gray, cv::Size(20, 28));       // 统一尺寸

            cv::Mat feature;
            gray.convertTo(feature, CV_32F, 1.0 / 255.0);  // 归一化到 0~1
            feature = feature.reshape(1, 1);               // 展平成 1 x 560

            samples.push_back(feature);
            labels.push_back(digit);
        }
    }
    std::cout << "样本数：" << samples.size() << "\n";

    // 2. 把 vector 合成一个大矩阵（每行一个样本）
    cv::Mat trainData;
    cv::vconcat(samples, trainData);                       // 纵向拼接
    cv::Mat trainLabels(labels);                           // 转成 Mat

    // 3. 创建并配置 SVM
    auto svm = cv::ml::SVM::create();
    svm->setType(cv::ml::SVM::C_SVC);
    svm->setKernel(cv::ml::SVM::RBF);
    svm->setC(100);
    svm->setGamma(0.007);                                  // 或 svm->setGamma(0) 自动
    svm->setTermCriteria(cv::TermCriteria(
        cv::TermCriteria::MAX_ITER + cv::TermCriteria::EPS, 3000, 1e-6));

    // 4. 训练
    svm->train(trainData, cv::ml::ROW_SAMPLE, trainLabels);

    // 5. 保存
    svm->save("digit_svm.yml");
    std::cout << "训练完成，模型已保存\n";

    // 6. 自测：拿训练集预测一遍看准确率
    int correct = 0;
    for (size_t i = 0; i < samples.size(); i++) {
        float pred = svm->predict(samples[i]);
        if ((int)pred == labels[i]) correct++;
    }
    std::cout << "训练集准确率：" << 100.0 * correct / samples.size() << "%\n";
}
```

**要点：**

- `trainData` 必须是 **N 行 × 特征数** 的 `CV_32F` 矩阵（每行一个样本）。
- `labels` 必须是 **N 行 1 列** 的 `CV_32S`（整数）。
- `ROW_SAMPLE` 表示"每行是一个样本"。
- 保存成 `.yml`，以后加载不用重新训练。

---

## 5. 加载与预测

### 5.1 加载（程序启动时做一次）

```cpp
cv::Ptr<cv::ml::SVM> svm = cv::ml::SVM::load("digit_svm.yml");
if (svm.empty()) {
    std::cerr << "模型加载失败，路径对吗？\n";
    return;
}
```

### 5.2 预测

```cpp
// feature 必须是 1 x N 的 CV_32F 矩阵，和训练时一模一样
float label = svm->predict(feature);
int digit = static_cast<int>(label);
```

**最容易出错的地方：预测时的特征提取必须和训练时完全一致！**
（同样的灰度化、同样的 resize 尺寸、同样的归一化、同样的展平方式）
不一致模型就"看不懂"输入。

### 5.3 想拿置信度

```cpp
float prob = svm->predict(feature, cv::noArray(), cv::ml::StatModel::RAW_OUTPUT);
// 或开启概率输出（trainAuto 时设置）
```

---

## 6. 特征工程：怎么把图片变成数字

这是识别效果好坏的**决定性一步**。

### 6.1 基础流程（本项目用的）

```
裁剪出数字区域（ROI）
   │ cvtColor 转灰度
   ▼
灰度图
   │ resize 到固定尺寸 20 × 28
   ▼
20×28 小图
   │ convertTo 归一化：像素值 /255 → 0~1 之间
   ▼
浮点小图
   │ reshape(1,1) 展平
   ▼
1 × 560 的一维向量（特征）
```

```cpp
cv::Mat gray, feature;
cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
cv::resize(gray, gray, cv::Size(20, 28));
gray.convertTo(feature, CV_32F, 1.0 / 255.0);
feature = feature.reshape(1, 1);        // 1 x 560
float digit = svm->predict(feature);
```

### 6.2 为什么要归一化

原始像素 0~255，数值大，不同光照下差异大。
除以 255 后变成 0~1，训练更稳定、收敛更快。

### 6.3 为什么统一尺寸

SVM 要求**每个样本的特征长度一样**。20×28 是经验尺寸（也有人用 28×28、32×32）。

### 6.4 更高级的特征（了解）

| 特征 | 说明 | 优点 |
| --- | --- | --- |
| 原始像素（本项目） | 直接展平 | 简单，小样本够用 |
| HOG | 方向梯度直方图 | 对光照变化更稳，行人检测经典 |
| 二值化 + 投影 | 水平/垂直投影 | 极简，对固定字体好 |
| 深度特征 | CNN 输出 | 效果最好，但需要更多数据和算力 |

```cpp
// HOG 示例
cv::HOGDescriptor hog(cv::Size(20,28), cv::Size(10,14),
                      cv::Size(5,7), cv::Size(5,7), 9);
std::vector<float> descriptors;
hog.compute(gray, descriptors);
cv::Mat feature(1, descriptors.size(), CV_32F, descriptors.data());
```

### 6.5 数据增强（样本太少就靠它）

对训练图片做小变换，生成更多样本：

- 轻微旋转（±10°）
- 轻微平移、缩放
- 改变亮度/对比度
- 加少量噪声

**原则：** 变换要"像真实场景里会出现的变化"，不能乱变。

---

## 7. 完整的数字识别流程

把"找数字区域 → 特征 → 预测"串起来：

```cpp
// ===== 在装甲板识别之后 =====
// armor.corners 是这块装甲板的 4 个角点

// 1. 用正外接矩形框住，再稍微扩大（多留点边）
cv::Rect r = cv::boundingRect(armor.corners);
int ex = r.width * 0.2, ey = r.height * 0.2;     // 扩大 20%
r.x -= ex; r.y -= ey; r.width += 2*ex; r.height += 2*ey;

// 2. 防止越界（和图像范围取交集）
cv::Rect imageRect(0, 0, frame.cols, frame.rows);
r &= imageRect;

// 3. 抠出 ROI
cv::Mat roi = frame(r);
if (roi.empty()) return;

// 4. 特征提取（和训练时一致的流程！）
cv::Mat gray, feature;
cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
cv::resize(gray, gray, cv::Size(20, 28));
gray.convertTo(feature, CV_32F, 1.0 / 255.0);
feature = feature.reshape(1, 1);

// 5. 预测
float pred = svm->predict(feature);
armor.number = static_cast<int>(pred);

// 6. 画出来看看
cv::putText(result, std::to_string(armor.number),
            cv::Point(r.x, r.y), cv::FONT_HERSHEY_SIMPLEX,
            1.0, cv::Scalar(255, 255, 255), 2);
```

**提高识别率的实用技巧：**

| 技巧 | 说明 |
| --- | --- |
| ROI 别抠太紧也别太松 | 留一点边，让数字完整 |
| 训练数据要和实际一致 | 实际是游戏截图，就用游戏截图训练 |
| 加二值化预处理 | 有些场景灰度归一化不如先二值化 |
| 置信度过滤 | 拿不准就不识别（宁缺毋滥） |
| 时序投票 | 连续几帧识别到同一个数字才确认 |

---

## 8. KNN：另一种简单分类器

KNN（K 近邻）：看新样本离哪些训练样本最近，按最近的 K 个"投票"。

```cpp
auto knn = cv::ml::KNearest::create();
knn->setDefaultK(3);
knn->setIsClassifier(true);
knn->train(trainData, cv::ml::ROW_SAMPLE, trainLabels);

float result = knn->predict(feature);       // 预测
```

| 对比 | SVM | KNN |
| --- | --- | --- |
| 训练速度 | 慢一点 | 极快（其实不训练，只是存数据） |
| 预测速度 | 快 | 慢（要和所有样本比） |
| 小数据效果 | 好 | 好 |
| 大数据效果 | 好 | 差（太慢） |

**什么时候用：** 数据量不大、想快速试一个基线，KNN 很适合。

---

## 9. 深度学习（dnn 模块）简介

OpenCV 的 `dnn` 模块能**加载已经训练好的神经网络**（自己训练需要别的框架）：

```cpp
#include <opencv2/dnn.hpp>

cv::dnn::Net net = cv::dnn::readNetFromONNX("model.onnx");
cv::Mat blob = cv::dnn::blobFromImage(img, 1.0/255.0, cv::Size(224,224));
net.setInput(blob);
cv::Mat out = net.forward();          // 输出结果
```

**什么时候用：** 目标复杂、SVM 效果不够时。**注意：** dnn 主要负责"用模型"，
训练模型要用 PyTorch/TensorFlow 等。

**对新手：先用 SVM 或模板匹配，够用就别上深度学习。**

---

## 10. 实战建议与常见坑

### 10.1 常见坑

| 坑 | 现象 | 解决 |
| --- | --- | --- |
| 预测特征和训练不一致 | 预测结果乱 | 两边用**同一个**特征函数 |
| 忘了归一化 | 效果差 | `convertTo(..., 1.0/255.0)` |
| reshape 顺序不对 | 训练报错/结果乱 | 确保是 `1 x 特征数` 一行 |
| 标签类型不对 | 训练报错 | 标签用整数 `CV_32S` |
| 模型路径写错 | load 失败 | 检查路径；打印 `svm.empty()` |
| 数据分布不均衡 | 偏向某一类 | 每类样本数量尽量相当 |
| 测试集污染 | 准确率虚高 | 留一部分"没参与训练"的图片做测试 |
| 光照/背景变化大 | 实测拉胯 | 训练数据要覆盖实际场景 |

### 10.2 训练数据的经验

1. **训练数据 ≥ 每类几百张**（越多越好，但 SVM 小数据也能用）。
2. **场景一致**：游戏截图训练的模型别直接拿去认现实照片。
3. **划分训练/测试**：比如 80% 训练、20% 测试，用测试集评估真实水平。
4. **及时看错误样本**：把认错的图存下来，看看有没有规律（尺寸不对？光照不对？）。

### 10.3 调试识别的顺序

```
① 先把"抠出来的 ROI"显示出来 —— 看看有没有抠对
② 再把"resize 后的小图"显示出来 —— 看看形状对不对
③ 最后核对"特征提取代码"和"训练代码"是否完全一致
```

**90% 的识别问题，出在 ROI 抠错了或特征不一致，而不是模型本身。**

---

> 上一篇：[OpenCV 相机模型、PnP 与标定](03-相机模型PnP与标定.md)
> 下一篇：[OpenCV 绘图、窗口与实战套路](05-绘图窗口与实战套路.md)
