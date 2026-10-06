#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
using namespace std;
using namespace cv;
/*
整个思路：    先对图像通过颜色通道进行分离 然后对红色通道和蓝色通道进行差值运算 得到红色区域的图像
            然后对差值图进行二值化处理 得到红色区域的二值图
            再对二值图进行轮廓检测 得到所有的轮廓
            然后对每个轮廓进行最小外接矩形拟合 得到每个轮廓的最小外接矩形
            然后对每个最小外接矩形进行筛选 帅选出符合灯条特征的矩形 
            用到的特征 高度宽度比 面积 倾斜角度 
            用路线图来表示整个思路：

            图像颜色通道分离 -> 红色通道和蓝色通道差值 -> 二值化处理 -> 轮廓检测 -> 最小外接矩形拟合 -> 筛选灯条 -> 绘制灯条矩形

用到的函数：
 VideoCapture(用来读取视频) 
split（用来分离图像通道） 
subtract（用来计算通道差值）
threshold（用来进行二值化处理）
GaussianBlur（用来进行高斯模糊处理 减少图像噪声） 
findContours（用来寻找轮廓） 
contourArea（用来计算轮廓面积）
minAreaRect（用来计算最小外接矩形）
swap（用来交换两个变量的值 注意引用头文件<algorithm>）

用到的类：
Point2f（用来表示二维点）
RotatedRect (用来表示旋转矩形)

应该注意的点：
1.在计算灯条的倾斜角度时 使用的是atan2(dx,dy)而不是atan2(dy,dx) 因为灯条的倾斜角度是相对于竖直方向的 而不是水平方向的
2.在筛选灯条是 使用的是abs(lb.angle)<20 而不是abs(lb.angle)<10 因为灯条的倾斜角度可能会有一定的误差 
3.其余的参数都是根据实际情况进行调整的 例如宽高比 面积等 没有固定的标准
4.在绘制灯条轮廓时 使用的是polylines 而不是drawContours 因为drawContours是用来绘制轮廓的 而polylines是用来绘制多边形的
  这里的灯条是一个四边形（因为倾斜的原因 所以不能看作是矩形） 所以使用polylines来绘制灯条的轮廓
5.一般绘制点使用circle函数 绘制线条使用line函数 绘制多边形使用polylines函数 绘制轮廓使用drawContours函数
  使用这些函数时 要注意参数的类型和顺序 例如circle函数的参数是 圆心 坐标 半径 颜色 线宽等 而line函数的参数是 起点坐标 终点坐标 颜色 线宽等
  注意polylines函数的参数是 点集 是否闭合 颜色 线宽等 而drawContours函数的参数是 图像 轮廓集 轮廓索引 颜色 线宽等
6.在使用类的时候 要注意类的成员变量和成员函数的使用 
    例如Point2f类的成员变量是x和y 分别表示点的横坐标和纵坐标 而RotatedRect类的成员变量是center size angle 分别表示矩形的中心点 尺寸和旋转角度

这里写上文件中所有用到的opencv函数的参数顺序
thresold函数的参数顺序：
源图像 目标图像 阈值 最大值 阈值类型）
GaussianBlur函数的参数顺序：
源图像 目标图像 核大小 sigmaX sigmaY 边界类型
findContours函数的参数顺序：
源图像 轮廓集 检索模式 近似方法 偏移
drawContours函数的参数顺组：
图像 轮廓集 轮廓索引 颜色 线宽 线型 偏移
putText函数的参数顺序：
图像 文本内容 位置 字体类型 字体大小 颜色 线宽
substract函数的参数顺序：
源图像1 源图像2 目标图像



*/
struct lightbar{
    Point2f center;//储存灯条的中心点
    float width;//储存灯条的宽度
    float height;//储存灯条的高度
    float angle;//储存灯条的倾斜角度
    Point2f top;//储存灯条上边的中心点
    Point2f bottom;//储存灯条下边的中心点
};//用来储存装甲板的信息

struct Armor{
    Point2f center;//储存装甲板的中心点
    vector<Point2f>corners;//储存装甲板的四个角点
    lightbar left;//储存装甲板左边的灯条
    lightbar right;//储存装甲板右边的灯条
};//构建的是装甲板的类 用来储存装甲板的信息

double pointdistance(cv::Point2f p1,cv::Point2f p2)
{
    return sqrt(pow(p1.x-p2.x,2)+pow(p1.y-p2.y,2));//定义函数来计算两个点之间的距离
};//pow函数用来计算平方 也可以计算其他次方 只是改变参数就可以

int main(){
     Ptr<ml::SVM> svm = ml::SVM::load("digit_svm.yml");//加载模型
     if(svm.empty()){
        cout<<"模型加载失败"<<endl;
        return -1;
     }
    VideoCapture cap("Infantry_red1.mp4");
// 蓝色视频的文件名 hero_blue.mp4  红色视频的文件名 Infantry_red1.mp4  Infantry_red2.mp4
    int color=1;//color=0是蓝色 1是红色
        //最后识别红蓝只需要更改这个数值即可
    Mat frame;
    double fps=0.0;//用来储存视频的帧率
    double lasttime=(double)getTickCount();//用来储存上一次的时间

    if(!cap.isOpened()){
        cout<<"can not find video"<<endl;
        return -1;
    }
    int textX= frame.cols-200;//保证fps显示在右上角
    while(true){

        cap>>frame;
        double currenttime=(double)getTickCount();//用来获取当前时间
        double timediff=(currenttime-lasttime)/getTickFrequency();//用来计算时间差
        lasttime=currenttime;//更新上一次时间
        fps=1.0/timediff;//计算帧率
        string text = "fps:"+to_string(fps);
        putText(frame,text,Point(textX,50),FONT_HERSHEY_SIMPLEX,0.8,Scalar(255,255,255),2);
        if(frame.empty())break;

        vector<Mat>channels;
        split(frame,channels);//将图像分离成B、G、R三个通道

        Mat red_diff,thre,blue_diff;


        if(color==0){
            subtract(
                channels[0],
                channels[2],
                blue_diff
            );
            GaussianBlur(blue_diff,
                blue_diff,
                Size(9,9),
                0
            );
            threshold(blue_diff,
            thre,
            120,
            255,
            THRESH_BINARY);
        }//上面的if是对于蓝色灯条的处理
        if(color==1){
        subtract(
            channels[2],
            channels[0],
            red_diff);//计算红色通道和蓝色通道的差值
        GaussianBlur(red_diff,
            red_diff,
            Size(9,9),
            0);//对差值图进行高斯模糊处理
        threshold(red_diff,
            thre,
            100,
            255,
            THRESH_BINARY);//对差值图进行二值化处理
        }//这个if语句是对于红色灯条的处理


        vector<vector<Point>>contours;//用来储存轮廓的点集

        cv::findContours(
            thre,
            contours,
            RETR_EXTERNAL,
            CHAIN_APPROX_SIMPLE
        );//寻找轮廓

        Mat result = frame.clone();//克隆一张图像用来显示结果

        vector<lightbar>lightbars;//用来储存所有的灯条
for(int i=0;i<contours.size();i++){

    double area = contourArea(contours[i]);//计算的是轮廓的面积

    RotatedRect rect = minAreaRect(contours[i]);//计算的是轮廓的最小外接矩形

    float height = rect.size.height;//获取轮廓的高度
    float width = rect.size.width;//获取轮廓的宽度

    if(width>height){
        swap(width,height);//如果宽度大于高度，则交换宽度和高度，因为灯条的高度应该大于宽度
    }

    Point2f pts[4];//用来储存轮廓的四个角点

    rect.points(pts);//获取轮廓的四个角点

    sort(pts,pts+4,[](Point2f a,Point2f b){
        return a.y<b.y;
    });//按照y坐标对四个角点进行排序

    Point2f top=(pts[0]+pts[1])/2;
    Point2f bottom=(pts[2]+pts[3])/2;

    double dx=bottom.x-top.x;
    double dy=bottom.y-top.y;

    lightbar lb;//用来储存当前灯条的信息

    lb.center=rect.center;//储存灯条的中心点
    lb.width=width;//储存灯条的宽度
    lb.height=height;//储存灯条的高度
    lb.top=top;//储存灯条上边的中心点
    lb.bottom=bottom;//储存灯条下边的中心点

    lb.angle=atan2(dx,dy)*180/CV_PI;//计算灯条相对于竖直方向的倾斜角度

    if(height/width>1.2 &&
       area>70 &&
       area<1000 &&
       abs(lb.angle)<20)
    {
        lightbars.push_back(lb);//将符合条件的灯条加入灯条数组

        for(int k=0;k<4;k++){

           cv::circle(
                result,
                pts[k],
                4,
                Scalar(0,255,0),
                -1
            );
        }//绘制灯条的四个角点

        vector<Point>lightbarCorners;//用来储存灯条的四个角点

        for(int k=0;k<4;k++){

            lightbarCorners.push_back(
                Point(
                    cvRound(pts[k].x),
                    cvRound(pts[k].y)
                )
            );
        }//将灯条的四个角点转换成整数坐标

        polylines(
            result,
            lightbarCorners,
            true,
            Scalar(255,0,0),
            2
        );//绘制灯条的轮廓

        cv::circle(
            result,
            top,
            5,
            Scalar(0,255,0),
            -1
        );//绘制灯条上边的中心点

        cv::circle(
            result,
            bottom,
            5,
            Scalar(255,0,0),
            -1
        );//绘制灯条下边的中心点
    }
}

        for(int i=0;i<lightbars.size();i++){

            for(int j=i+1;j<lightbars.size();j++){

                double dis=
                    pointdistance(
                        lightbars[i].center,
                        lightbars[j].center
                    );//计算两个灯条中心点之间的距离

                double heightDiff=
                    abs(
                        lightbars[i].center.y-
                        lightbars[j].center.y
                    );//计算两个灯条之间的高度差

                double angleDiff=
                    abs(
                        lightbars[i].angle-
                        lightbars[j].angle
                    );//计算两个灯条之间的角度差
                    double avgHeight = 
                    abs(
                      lightbars[i].height+
                      lightbars[j].height  
                    )/2;//计算两个灯条的平均高度
                    double disRatio=
                    dis/avgHeight;// 计算灯条之间的距离与灯条高度的比例
                    double heightRatio=
                    max(lightbars[i].height,lightbars[j].height)/
                    min(lightbars[i].height,lightbars[j].height);// 计算两个灯条的高度比例

                if(disRatio>1.5 &&
                    disRatio<8.0 &&
                    heightDiff<avgHeight*0.5 &&
                    angleDiff<10 &&
                    heightRatio<1.5)
                    {
                    Armor armor;//用来储存装甲板的信息

                    if(
                        lightbars[i].center.x<
                        lightbars[j].center.x
                    ){
                        armor.left=lightbars[i];
                        armor.right=lightbars[j];
                    }
                    else{
                        armor.left=lightbars[j];
                        armor.right=lightbars[i];
                    }//判断两个灯条的左右关系

                    armor.center=
                        (armor.left.center+
                         armor.right.center)/2;//计算装甲板的中心点

                    armor.corners.push_back(
                        armor.left.top
                    );//将装甲板的四个角点存入corners中
                    
                    armor.corners.push_back(
                        armor.right.top
                    );

                    armor.corners.push_back(
                        armor.right.bottom
                    );

                    armor.corners.push_back(
                        armor.left.bottom
                    );

                    vector<Point>drawCorners;//用来储存装甲板的四个角点
                   


                    for(const Point2f& p:armor.corners){

                        drawCorners.emplace_back(
                            cvRound(p.x),
                            cvRound(p.y)
                        );
                    }//将装甲板的四个角点转换成整数坐标
                    Rect numberRect= boundingRect(drawCorners);//计算装甲板区域的最小矩形
                    int expandX=numberRect.width*0.39;//水平方向扩大原宽度的39%
                    int expandY=numberRect.height*0.39;//竖直方向扩大原高度的39%

                    numberRect.x-=expandX;//左边向左扩展
                    numberRect.y-=expandY;//上边向上扩展

                    numberRect.width+=2*expandX;//左右两边一起扩展
                    numberRect.height+=2*expandY;//上下两边一起扩展

                    Rect imageRect(0,0,frame.cols,frame.rows);
                    numberRect = numberRect&imageRect;//& 这个在这里可以理解为取交集 防止越界 让数字区域限制在图像范围之内
                    Mat numberROI = frame(numberRect);//将数字区域裁剪下来
                    Mat gray;
                    cvtColor(numberROI,
                                gray,
                            COLOR_BGR2GRAY);
                    resize(gray,
                            gray,
                            Size(20,28));
                    Mat feature;
                    gray.convertTo(feature,
                        CV_32F,
                        1.0/255.0);
                    feature=feature.reshape(1,1);
                    float result1 = svm -> predict(feature);//使用模型进行预测 但是经过多次尝试 总是会将3认成6
                    string text = to_string(int(result1));
                    putText(result,
                            text,
                            Point(numberRect.x,numberRect.y),
                            FONT_HERSHEY_COMPLEX,
                            1,
                            Scalar(255,255,255),
                            2);
                    imshow("numberROI",numberROI);
                    //imshow("num",numberROI);
                    cv::polylines(
                        result,
                        drawCorners,
                        true,
                        Scalar(0,255,0),
                        2
                    );//绘制装甲板的轮廓

                    cv::circle(
                        result,
                        armor.center,
                        5,
                        Scalar(255,0,0),
                        -1
                    );//绘制装甲板的中心点
                }
            }
        }

        imshow("result",result);

        if(waitKey(30)==27)break;
    }

    cv::destroyAllWindows();

    return 0;
}