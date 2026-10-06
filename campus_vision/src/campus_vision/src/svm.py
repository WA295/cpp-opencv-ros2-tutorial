import os
# 导入os模块，用来处理文件和文件夹路径，例如查找文件夹、拼接文件路径等

import cv2
# 导入OpenCV的Python接口，后面用它来读取图片、灰度化图片以及创建SVM模型

import numpy as np
# 导入NumPy，用来处理图片矩阵、数组以及训练数据


data_path = "per_100_datasets"
# 定义数据集所在的文件夹路径
# 这里的数据集文件夹和当前Python文件在同一个目录下，所以直接写文件夹名字即可


classes = os.listdir(data_path)
# 获取per_100_datasets文件夹中的所有文件和文件夹名称
# 对于我们的数据集来说，这里得到的主要就是"1"、"2"、..."8"这8个类别文件夹


classes.sort(key=int)
# 对类别名称进行排序
# 因为从os.listdir()得到的是字符串，例如"1"、"2"、"10"
# 使用key=int后，会按照数字大小进行排序，而不是按照字符串顺序排序


images = []
# 创建一个空列表，用来储存读取和处理之后的所有图片
# 后面每读取一张图片，就会把处理后的图片放入这个列表


labels = []
# 创建一个空列表，用来储存每张图片对应的数字标签
# 例如读取4文件夹中的图片时，对应的标签就是4
# images和labels中的元素是一一对应的


train_images = []
# 创建训练图片列表，用来保存最终送给SVM训练的图片特征


train_labels = []
# 创建训练标签列表，用来保存训练图片对应的正确答案


test_images = []
# 创建测试图片列表，用来保存没有参加训练的图片特征
# 后面使用这些图片检查SVM的识别效果


test_labels = []
# 创建测试标签列表，用来保存测试图片真正对应的数字


rng = np.random.default_rng(42)
# 创建一个NumPy随机数生成器
# 42是随机种子，这样每次运行程序时，图片的随机顺序都一样
# 这样做的好处是实验结果具有可重复性


for cls in classes:
    # 遍历所有数字类别
    # 第一次cls可能是"1"，第二次是"2"，一直到"8"


    label = int(cls)
    # 将文件夹名称从字符串转换成整数
    # 例如cls="4"时，label就等于整数4
    # SVM最终需要知道每张训练图片对应的是哪个数字类别


    class_path = os.path.join(data_path, cls)
    # 把数据集路径和当前类别文件夹拼接起来
    # 例如data_path是per_100_datasets，cls是4
    # 最终得到per_100_datasets/4


    imgs = os.listdir(class_path)
    # 获取当前数字文件夹中的所有图片文件名
    # 例如4文件夹中有100张图片，这里就会得到100个文件名


    rng.shuffle(imgs)
    # 将当前类别中的100张图片随机打乱顺序
    # 这样后面前80张和后20张不会按照原始文件顺序固定分配
    # 可以让训练集和测试集的划分更加随机


    print(cls, len(imgs))
    # 输出当前类别以及图片数量
    # 例如输出"4 100"，表示4这个类别有100张图片


    for i, img_name in enumerate(imgs):
        # 遍历当前类别中的每一张图片
        # i表示当前图片的编号，从0开始
        # img_name表示当前图片的文件名


        img_path = os.path.join(class_path, img_name)
        # 将当前类别文件夹路径和当前图片文件名拼接起来
        # 得到完整的图片路径，例如per_100_datasets/4/xxx.jpg


        img = cv2.imread(img_path)
        # 使用OpenCV读取当前图片
        # 读取成功后，img就是一张图片对应的NumPy数组


        if img is None:
            # 判断图片是否读取失败
            # 如果OpenCV没有成功读取图片，img就可能是None


            print("读取失败：", img_path)
            # 输出读取失败的图片路径，方便我们定位哪张图片出现了问题


            continue
            # 跳过当前图片，继续读取下一张图片


        gray = cv2.cvtColor(
            img,
            cv2.COLOR_BGR2GRAY
        )
        # 将彩色图片转换成灰度图片
        # COLOR_BGR2GRAY表示把OpenCV默认的BGR三通道图像转换成单通道灰度图像
        # 数字识别主要关心数字的形状，因此这里先去掉颜色信息


        gray = cv2.resize(
            gray,
            (20, 28)
        )
        # 将图片统一调整成20×28的尺寸
        # 数据集本身就是这个尺寸，所以这里主要是保证所有输入尺寸完全一致
        # SVM要求所有训练样本具有相同数量的特征


        gray = gray.astype(np.float32) / 255.0
        # 将图片像素数据转换成float32类型
        # 原来的像素值范围一般是0~255
        # 除以255后，把像素值归一化到0~1
        # 这样可以让不同像素的数值范围更加统一，方便后面的模型训练


        feature = gray.reshape(-1)
        # 将20×28的二维图片展开成一维数组
        # 20×28一共有560个像素
        # 因此最终feature就是包含560个数字的一维特征向量
        # SVM最终处理的不是一张图片，而是这样的特征向量


        if i < 80:
            # 当前类别的前80张图片作为训练数据
            # 因为每个类别有100张图片，所以这里使用80张训练


            train_images.append(feature)
            # 将当前图片的560维特征加入训练图片列表


            train_labels.append(label)
            # 将当前图片对应的数字标签加入训练标签列表
            # 例如当前图片来自4文件夹，那么这里加入的就是4


        else:
            # 当前类别剩余的20张图片作为测试数据
            # 这些图片不会参与SVM训练，而是用来检查模型识别能力


            test_images.append(feature)
            # 将测试图片的特征加入测试数据列表


            test_labels.append(label)
            # 将测试图片对应的正确数字加入测试标签列表


train_images = np.array(
    train_images,
    dtype=np.float32
)
# 将Python列表转换成NumPy数组
# SVM训练需要规则的矩阵形式的数据
# dtype=np.float32表示将数据统一存储成32位浮点数


train_labels = np.array(
    train_labels,
    dtype=np.int32
)
# 将训练标签列表转换成NumPy整数数组
# 每一个元素代表一张训练图片对应的正确数字


test_images = np.array(
    test_images,
    dtype=np.float32
)
# 将测试图片列表转换成NumPy数组
# 最终每一行代表一张测试图片的560维特征


test_labels = np.array(
    test_labels,
    dtype=np.int32
)
# 将测试标签列表转换成NumPy整数数组
# 用来和SVM最终预测出来的数字进行比较


print("训练图片数量：", len(train_images))
# 输出训练图片的数量
# 每个类别80张，共8个类别，所以正常情况下应该是640张


print("训练标签数量：", len(train_labels))
# 输出训练标签数量
# 应该和训练图片数量完全相同，因为每张图片必须对应一个标签


print("测试图片数量：", len(test_images))
# 输出测试图片数量
# 每个类别20张，共8个类别，所以正常情况下应该是160张


print("测试标签数量：", len(test_labels))
# 输出测试标签数量
# 应该和测试图片数量完全相同


svm = cv2.ml.SVM_create()
# 创建一个OpenCV的SVM模型
# SVM是一种机器学习分类算法，我们这里使用它来区分数字1~8


svm.setType(cv2.ml.SVM_C_SVC)
# 设置SVM的类型为C-SVC
# C-SVC是SVM中常用的一种分类模型，适合解决我们的数字多分类问题


svm.setKernel(cv2.ml.SVM_RBF)
# 设置SVM使用RBF高斯核
# RBF核可以把原本比较复杂的分类问题映射到更容易区分的空间
# 这里暂时不需要深入数学推导，先理解它是一种常用的非线性分类方式


svm.setC(100)
# 设置SVM的惩罚参数C
# C决定模型对训练错误的容忍程度
# 这里使用50是当前这版实验使用的参数，后面可以继续调节


feature_size = train_images.shape[1]
# 获取每张图片有多少个特征
# 我们的图片是20×28，所以这里应该得到560


svm.setGamma(4.0 / feature_size)
# 设置RBF核中的Gamma参数
# Gamma决定单个训练样本对分类边界影响的范围
# 这里根据特征数量进行设置，让参数能够随着输入维度变化


svm.setTermCriteria(
    (
        cv2.TERM_CRITERIA_MAX_ITER +
        cv2.TERM_CRITERIA_EPS,
        3000,
        1e-6
    )
)
# 设置SVM训练的终止条件
# MAX_ITER表示最多训练3000次迭代
# EPS表示当模型变化足够小时也可以提前停止
# 1e-6是停止判断使用的误差阈值
# 这样可以避免模型无限训练


svm.train(
    train_images,
    cv2.ml.ROW_SAMPLE,
    train_labels
)
# 正式开始训练SVM
# train_images是训练图片的特征数据
# ROW_SAMPLE表示每一行代表一个训练样本
# train_labels是每个训练样本对应的正确答案


print("SVM训练完成!")
# 告诉我们模型已经完成训练


_, predictions = svm.predict(test_images)
# 使用刚刚训练好的SVM对测试图片进行预测
# test_images里面放的是模型训练阶段没有使用的图片
# predictions里面保存的是SVM认为每张图片属于哪个数字


predictions = predictions.reshape(-1).astype(np.int32)
# 将OpenCV返回的预测结果整理成一维数组
# 同时转换成整数类型，方便后面和正确标签进行比较


correct = np.sum(predictions == test_labels)
# 将预测结果和真实标签逐个比较
# 相同的位置表示预测正确
# np.sum()可以统计总共有多少个预测结果是正确的


accuracy = correct / len(test_labels)
# 计算测试集准确率
# 正确数量除以测试图片总数量就是识别准确率


print("测试集正确数量：", correct)
# 输出测试集里一共有多少张图片预测正确


print("测试集总数量：", len(test_labels))
# 输出测试集总共有多少张图片


print("测试集准确率：", accuracy * 100, "%")
# 将0~1之间的准确率转换成百分数并打印
# 例如0.95就会显示95%


svm.save("digit_svm.yml")
# 将训练好的SVM模型保存到当前文件夹
# 保存以后就不需要每次运行C++程序时重新训练
# 后面我们的C++视觉程序可以直接读取这个模型


print("模型已经保存为 digit_svm.yml")
# 告诉我们模型文件已经成功保存