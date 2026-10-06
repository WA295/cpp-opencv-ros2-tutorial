#include <rclcpp/rclcpp.hpp>
#include <memory>
#include <functional>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <tdt_interface/msg/auto_aim_frame.hpp>
#include <tdt_interface/msg/send_data.hpp>
#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <cctype>

using namespace cv;
using namespace std;
using namespace rclcpp;

/*
整个思路：

    ROS 2接收校园赛实时图像
    图像颜色通道分离
    红色通道和蓝色通道差值
    高斯模糊
    二值化处理
    轮廓检测
    最小外接矩形拟合
    筛选灯条(这个也太难了吧 参数怎么都调不对 可恶)
    灯条配对（我超威你配对给我配对好了啊喂 为什么会给我把地面当做灯条去配对啊 ）
    Armor
    绘制Armor
    实时显示结果
    Armor
    ROI
    SVM数字识别(为什么突然给我打不开文件了啊！可以弄了但是为什么给我把3识别成6啊喂)
    目标位置
    yaw / pitch
    ROS 2发布瞄准指令
*/


struct lightbar
{
    Point2f center;//储存灯条的中心点
    float width;//储存灯条的宽度
    float height;//储存灯条的高度
    float angle;//储存灯条的倾斜角度
    Point2f top;//储存灯条上边的中心点
    Point2f bottom;//储存灯条下边的中心点
    Point2f corners[4];
};//用来储存灯条的信息
struct Armor
{
    Point2f center;//储存装甲板的中心点
    vector<Point2f> corners;//储存装甲板的四个角点
    vector<Point2f>pnpCorners;//用来储存后面计算的pnp 的二维坐标点
    lightbar left;//储存装甲板左边的灯条
    lightbar right;//储存装甲板右边的灯条
    int number;//保存SVM识别出来的数字
};//用来储存装甲板的信息

double pointdistance(Point2f p1,Point2f p2)
{
    return sqrt(
        pow(p1.x-p2.x,2)+
        pow(p1.y-p2.y,2)
    );//计算两个点之间的距离
}


//后面创建trackbar  先定义一些变量；
int thresholdValue = 100;//二值化阈值
int minAreaValue = 70;//灯条最小面积
int maxAreaValue = 5000;//灯条最大面积
int maxAngleValue = 30;//灯条最大允许角度
int maxCenterAngleValue = 20;//两个灯条中心连线最大允许角度
double maxDisRatioValue = 4.0;//计算两个灯条之间的距离和灯条高度的比例

// 装甲板物理尺寸（单位：米），请按实测修改
const double ARMOR_WIDTH_M  = 0.135;// 两灯条中心之间的水平距离（先按模块总宽估算）
const double ARMOR_HEIGHT_M = 0.055;// 灯条上下端中点之间的距离（图中55mm）

// ===================== 阵营配置（红蓝双方通用） =====================
// 官方约定：话题后缀 N=1 为蓝方，N=2 为红方。
// 本节点通过 ROS 参数 "team" 识别自己在哪一方，运行时用参数覆盖即可，无需改代码：
//   蓝方：ros2 run campus_vision campus_node --ros-args -p team:=blue
//   红方：ros2 run campus_vision campus_node --ros-args -p team:=red
const int PLAYER_ID_BLUE = 1;// 蓝方对应的 player 编号
const int PLAYER_ID_RED  = 2;// 红方对应的 player 编号

// ===================== 自动扫描（比赛开始后自己转动摄像头） =====================
// 原理：云台只有 SendData.yaw（世界光轴绝对角）这一个控制量。
//   没找到敌人时 -> 持续发一串缓慢变化的 yaw，让云台左右摆动搜索；
//   找到敌人后   -> 立即改发真实瞄准角，扫描自动让位。
// 因为图像只有进入对局才会有，而本逻辑写在 processFrame 里，所以它天然"比赛开始后才转"。
// 以下参数都可在运行时用 ROS 参数覆盖，不用重新编译：
//   ros2 run campus_vision campus_node --ros-args -p scan_enable:=true -p scan_amplitude_deg:=45.0
bool   scan_enable_        = true; // 是否开启自动扫描
double scan_amplitude_deg_ = 45.0; // 左右摆幅（度），以进入扫描时的朝向为中心
double scan_period_s_      = 4.0;  // 一次"左—右—回中"的周期（秒），越大转得越慢
double scan_timeout_s_     = 0.5;  // 连续多久没瞄准到目标就进入扫描（秒）
double scan_pitch_deg_     = 0.0;  // 扫描时的云台俯仰（度，向下为正）

class CampusNode : public rclcpp::Node
{
public:

    CampusNode()
        : Node("campus_node")
    {
        svm_=cv::ml::SVM::load("/home/robot/campus_ws/src/campus_vision/src/digit_svm.yml");//程序启动时只加载一次SVM模型

        // ---------- 阵营识别（改动点1） ----------
        // 声明名为 "team" 的 ROS 参数，默认蓝方；这是红蓝双方唯一的配置入口。
        this->declare_parameter<std::string>("team","blue");
        team_=this->get_parameter("team").as_string();//读取当前阵营

        // 归一化处理：统一转小写，兼容 "Blue"/"RED"/直接写数字 "1"/"2" 等情况
        std::string team_lower = team_;
        std::transform(
            team_lower.begin(),
            team_lower.end(),
            team_lower.begin(),
            [](unsigned char c){ return std::tolower(c); }
        );

        // 根据阵营决定 player 编号（蓝=1，红=2）
        if(team_lower=="red" || team_lower=="2")
        {
            is_blue_=false;
            player_id_=PLAYER_ID_RED;
        }
        else
        {
            is_blue_=true;
            player_id_=PLAYER_ID_BLUE;
        }

        // 打印识别结果，方便现场确认跑在哪一方
        RCLCPP_INFO(
            this->get_logger(),
            "阵营识别完成:team=%s -> player_%d(%s方)",
            team_.c_str(),
            player_id_,
            is_blue_ ? "蓝" : "红"
        );
        // ---------- 阵营识别结束 ----------

        // ---------- 读取自动扫描参数（改动点4） ----------
        // 允许运行时覆盖，便于现场调摆幅/周期
        scan_enable_        = this->declare_parameter<bool>("scan_enable", scan_enable_);
        scan_amplitude_deg_ = this->declare_parameter<double>("scan_amplitude_deg", scan_amplitude_deg_);
        scan_period_s_      = this->declare_parameter<double>("scan_period_s", scan_period_s_);
        scan_timeout_s_     = this->declare_parameter<double>("scan_timeout_s", scan_timeout_s_);
        scan_pitch_deg_     = this->declare_parameter<double>("scan_pitch_deg", scan_pitch_deg_);
        RCLCPP_INFO(
            this->get_logger(),
            "自动扫描：enable=%d 摆幅=%.1f度 周期=%.1fs 超时=%.1fs",
            scan_enable_, scan_amplitude_deg_, scan_period_s_, scan_timeout_s_
        );
        // ---------- 自动扫描参数读取结束 ----------

        
        RCLCPP_INFO(
            this->get_logger(),
            "校园赛节点启动！"
        );//输出节点启动信息


        cv::namedWindow(
            "参数",
            cv::WINDOW_NORMAL
        );//创建参数调节窗口

        cv::createTrackbar(
            "maxDisRatio",
            "参数",
            nullptr,
            100
        );

        cv::createTrackbar(
            "threshold",
            "参数",
            nullptr,
            255
        );//创建二值化阈值调节条


        cv::createTrackbar(
            "minArea",
            "参数",
            nullptr,
            1000
        );//创建最小面积调节条


        cv::createTrackbar(
            "maxArea",
            "参数",
            nullptr,
            10000
        );//创建最大面积调节条


        cv::createTrackbar(
            "maxAngle",
            "参数",
            nullptr,
            45
        );//创建灯条最大角度调节条


        cv::createTrackbar(
            "centerAngle",
            "参数",
            nullptr,
            45
        );//创建中心连线最大角度调节条
// 使用nullptr空指针来减小算法压力 不然会卡死程序 不会弹出窗口

        cv::setTrackbarPos(
            "threshold",
            "参数",
            thresholdValue
        );//设置二值化阈值初始值

        cv::setTrackbarPos(
            "minArea",
            "参数",
            minAreaValue
        );//创建最小面积调节条

        cv::setTrackbarPos(
            "maxArea",
            "参数",
            maxAreaValue
        );//创建最大面积调节条

        cv::setTrackbarPos(
            "maxAngle",
            "参数",
            maxAngleValue
        );//创建最大角度调节条

        cv::setTrackbarPos(
            "centerAngle",
            "参数",
            maxCenterAngleValue
        );//创建中线连线最大角度调节条

        cv::setTrackbarPos(
            "maxDisRatio",
            "参数",
            static_cast<int>(maxDisRatioValue * 10)  //还必须要有空格？
        );//创建灯条之间的距离和灯条高度比例的调节条

        image_sub_=
            this->create_subscription<sensor_msgs::msg::CompressedImage>(
                "/camera/image_raw/compressed",
                10,
                std::bind(
                    &CampusNode::imageCallback,
                    this,
                    std::placeholders::_1
                )
            );//创建图像订阅器，订阅校园赛发送的压缩图像

        frame_sub_=
            this->create_subscription<tdt_interface::msg::AutoAimFrame>(
                "/autoaim/frame",
                1,
                std::bind(
                    &CampusNode::frameCallback,
                    this,
                    std::placeholders::_1
                )
            );//创建AutoAimFrame订阅器，获取camera_info

        send_data_pub_=
            this->create_publisher<tdt_interface::msg::SendData>(
                // 改动点2：话题名根据阵营动态拼接
                // 蓝方 -> /target_angles_player_1，红方 -> /target_angles_player_2
                "/target_angles_player_" + std::to_string(player_id_),
                10
            );//创建瞄准指令发布器
    }
    


    void processFrame(cv::Mat& frame)
    {
        Mat result=frame.clone();//复制一张图像用来绘制识别结果
        static int frameCount=0;//记录当前处理的帧数

        frameCount++;//每收到一帧图像就加1

        if(frameCount%3!=0)
        {
        return;//每3帧处理1帧，降低CPU占用
        }

        maxDisRatioValue=
            cv::getTrackbarPos(
            "maxDisRatio",
            "参数"
             )/10.0;//获取当前灯条最大距离比例

        thresholdValue=
        cv::getTrackbarPos(
            "threshold",
            "参数"
        );

        minAreaValue=
        cv::getTrackbarPos(
            "minArea",
            "参数"
        );

        maxAreaValue=
        cv::getTrackbarPos(
            "maxArea",
            "参数"
        );

        maxAngleValue=
        cv::getTrackbarPos(
            "maxAngle",
            "参数"
        );


         
        //图像通道分离
         

        vector<Mat>channels;//用来储存B、G、R三个通道

        cv::split(
            frame,
            channels
        );//将图像分离成B、G、R三个通道


         
        //红色通道和蓝色通道差值
         

        Mat red_diff;//用来储存红色差值图

        cv::subtract(
            channels[2],
            channels[0],
            red_diff
        );//计算红色通道和蓝色通道的差值


        cv::GaussianBlur(
            red_diff,
            red_diff,
            Size(9,9),
            0
        );//对差值图进行高斯模糊处理


        
        //二值化
         

        Mat thre;//用来储存二值化后的图像

        cv::threshold(
            red_diff,
            thre,
            thresholdValue,
            255,
            THRESH_BINARY
        );//对差值图进行二值化处理


        
        //轮廓检测
        

        vector<vector<Point>>contours;//用来储存轮廓的点集

        cv::findContours(
            thre,
            contours,
            RETR_EXTERNAL,
            CHAIN_APPROX_SIMPLE
        );//寻找轮廓


        vector<lightbar>lightbars;//用来储存所有符合条件的灯条
        vector<Armor>armors;//储存识别到的所有装甲板


        
        //筛选灯条
        

        for(size_t i=0;
            i<contours.size();
            i++)
        {
            double area=
                contourArea(
                    contours[i]
                );//计算的是轮廓的面积

            RotatedRect rect=
                minAreaRect(
                    contours[i]
                );//计算的是轮廓的最小外接矩形


            float height=
                rect.size.height;//获取轮廓的高度

            float width=
                rect.size.width;//获取轮廓的宽度


            if(width>height)
            {
                swap(
                    width,
                    height
                );//如果宽度大于高度，则交换宽度和高度，因为灯条的高度应该大于宽度
            }


            Point2f pts[4];//用来储存轮廓的四个角点

            rect.points(pts);//获取轮廓的四个角点


            double edgeLength[4];//用来储存四条边的长度

            for(int k=0;k<4;k++)
            {
                edgeLength[k]=
                    pointdistance(
                        pts[k],
                        pts[(k+1)%4]
                    );//计算当前边的长度
            }


            int shortEdge=0;//记录最短边的编号

            for(int k=1;k<4;k++)
            {
                if(edgeLength[k]<edgeLength[shortEdge])
                {
                    shortEdge=k;//找到最短的那条边
                }
            }


            int oppositeEdge=
                (shortEdge+2)%4;//找到与最短边相对的另一条短边


            Point2f point1=
                (
                    pts[shortEdge]+
                    pts[(shortEdge+1)%4]
                )/2;//计算第一条短边的中心点


            Point2f point2=
                (
                    pts[oppositeEdge]+
                    pts[(oppositeEdge+1)%4]
                )/2;//计算另一条短边的中心点


            Point2f top;
            Point2f bottom;


            if(point1.y<point2.y)
            {
                top=point1;
                bottom=point2;
            }//根据两个短边中心点的y坐标判断上下关系
            else
            {
                top=point2;
                bottom=point1;
            }//根据两个短边中心点的y坐标判断上下关系


            


            double dx=
                bottom.x-top.x;//计算灯条在x方向上的变化


            double dy=
                bottom.y-top.y;//计算灯条在y方向上的变化


            lightbar lb;//用来储存当前灯条的信息


            lb.center=
                rect.center;//储存灯条的中心点

            lb.width=
                width;//储存灯条的宽度

            lb.height=
                height;//储存灯条的高度

            lb.top=
                top;//储存灯条上边的中心点

            lb.bottom=
                bottom;//储存灯条下边的中心点


            lb.angle=
                atan2(dx,dy)*180/CV_PI;//计算灯条相对于竖直方向的倾斜角度
            for(int i=0;i<4;i++){
                lb.corners[i]=pts[i];
            }

            if(
                height/width>1.5 &&
                area> minAreaValue &&
                area<maxAreaValue &&
                abs(lb.angle)<maxAngleValue
            )
            {
                lightbars.push_back(
                    lb
                );//将符合条件的灯条加入灯条数组
                static int lb_count=0;
                if(++lb_count%50==0)
                    RCLCPP_INFO(this->get_logger(), "灯条总数 %d", lb_count);


                for(int k=0;k<4;k++)
                {
                    cv::circle(
                        result,
                        pts[k],
                        4,
                        Scalar(0,255,0),
                        -1
                    );
                }//绘制灯条的四个角点


                vector<Point>lightbarCorners;//用来储存灯条的四个角点


                for(int k=0;k<4;k++)
                {
                    lightbarCorners.push_back(
                        Point(
                            cvRound(pts[k].x),
                            cvRound(pts[k].y)
                        )
                    );
                }//将灯条的四个角点转换成整数坐标


                cv::polylines(
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


       
        //灯条配对

        for(size_t i=0;
            i<lightbars.size();
            i++)
        {
            for(size_t j=i+1;
                j<lightbars.size();
                j++)
            {
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


               double centerDx =
                        lightbars[j].center.x-
                        lightbars[i].center.x;//计算两个灯条中心点在x方向上的距离


                double centerDy =
                        lightbars[j].center.y-
                        lightbars[i].center.y;//计算两个灯条中心点在y方向上的距离


                double centerAngle=
                        atan2(
                            centerDy,
                            centerDx
                        )*180/CV_PI;//计算两个灯条中心连线相对于水平方向的角度
                double avgHeight=
                    (
                        lightbars[i].height+
                        lightbars[j].height
                    )/2.0;//计算两个灯条的平均高度


                double disRatio=
                    dis/avgHeight;
                    //计算两个灯条之间的距离和灯条高度的比例


                double heightRatio=
                    max(
                        lightbars[i].height,
                        lightbars[j].height
                    )/
                    min(
                        lightbars[i].height,
                        lightbars[j].height
                    );//计算两个灯条高度之间的比例

                if(
                    
                    disRatio>1.5 &&
                    disRatio<maxDisRatioValue &&
                    heightDiff<avgHeight*0.5 &&
                    angleDiff<15 &&
                    heightRatio<1.5 &&
                    abs(centerAngle)<maxCenterAngleValue

                )
                {
                    Armor armor;//用来储存装甲板的信息
                    armor.number=-1;//认为还没有结果 先初始化


                    if(
                        lightbars[i].center.x<
                        lightbars[j].center.x
                    )
                    {
                        armor.left=
                            lightbars[i];

                        armor.right=
                            lightbars[j];
                    }
                    else
                    {
                        armor.left=
                            lightbars[j];

                        armor.right=
                            lightbars[i];
                    }//判断两个灯条的左右关系


                    armor.center=
                        (
                            armor.left.center+
                            armor.right.center
                        )/2;//计算装甲板的中心点

                    armor.corners.push_back(
                            armor.left.top
                        );//将左灯条上边中心点存入装甲板角点

                    armor.corners.push_back(
                            armor.right.top
                        );//将右灯条上边中心点存入装甲板角点

                    armor.corners.push_back(
                            armor.right.bottom
                        );//将右灯条下边中心点存入装甲板角点

                    armor.corners.push_back(
                            armor.left.bottom
                        );//将左灯条下边中心点存入装甲板角点


                       armor.pnpCorners = armor.corners;//暂时将角点复制给pnpCorners

                    // ---------- PnP 解算目标位置 ----------
                    // 仅当相机内参已获取（frameCallback已写入）时才做
                    if(!cameraMatrix_.empty() && !distCoeffs_.empty())
                    {
                        static int pnp_count=0;
                        if(++pnp_count%30==0)
                            RCLCPP_INFO(this->get_logger(), "进入PnP段 %d 次", pnp_count);
                        // 四个角点在装甲板平面上的真实3D坐标，z=0（装甲板所在平面）
                        // 对应顺序: left.top, right.top, right.bottom, left.bottom
                        vector<Point3f> objectPoints = {
                            Point3f(-ARMOR_WIDTH_M/2.0f, -ARMOR_HEIGHT_M/2.0f, 0.0f),
                            Point3f( ARMOR_WIDTH_M/2.0f, -ARMOR_HEIGHT_M/2.0f, 0.0f),
                            Point3f( ARMOR_WIDTH_M/2.0f,  ARMOR_HEIGHT_M/2.0f, 0.0f),
                            Point3f(-ARMOR_WIDTH_M/2.0f,  ARMOR_HEIGHT_M/2.0f, 0.0f)
                        };

                        vector<Point2f> imagePoints;
                        for(size_t k=0;k<armor.pnpCorners.size();k++)
                        {
                            imagePoints.push_back(armor.pnpCorners[k]);// 与上面3D点一一对应的图像2D点
                        }

                        Mat rvec, tvec;// 旋转向量、平移向量（平移即目标在相机坐标系下的位置）
                        bool ok = cv::solvePnP(
                            objectPoints,
                            imagePoints,
                            cameraMatrix_,
                            distCoeffs_,
                            rvec,
                            tvec,
                            false,
                            cv::SOLVEPNP_IPPE// 对平面目标更稳的解算方法
                        );

                        if(ok)
                        {
                            static int ok_count=0;
                            if(++ok_count%30==0)
                                RCLCPP_INFO(this->get_logger(), "solvePnP成功 %d 次, tz=%.3f", ok_count, tvec.at<double>(2));
                            double tx = tvec.at<double>(0);// 目标在相机系下X（右为正）
                            double ty = tvec.at<double>(1);// 目标在相机系下Y（下为正）
                            double tz = tvec.at<double>(2);// 目标在相机系下Z（前为正，即距离）

                            if(tz > 0)
                            {
                                double yaw_deg   = atan2(tx, tz) * 180.0 / CV_PI;// 目标在相机系下偏角（右偏为正）
                                double pitch_deg = atan2(ty, tz) * 180.0 / CV_PI;// 目标在相机系下偏角（下偏为正）

                                // 转成世界光轴角：世界yaw = 自身当前世界yaw + 目标在相机系下的偏角
                                double world_yaw = last_state_.yaw_degrees + yaw_deg;

                                // 瞄准到目标：更新"最后瞄准时间"，扫描逻辑会自动暂停
                                last_aim_time_ = this->get_clock()->now();

                                // 改动点5：统一通过 publishAim 发布，true=允许开火，并限定只在[-180,180]
                                publishAim(world_yaw, pitch_deg, true);

                                // 在画面上显示解算的yaw/pitch和距离
                                string info = "yaw:" + to_string(yaw_deg).substr(0,6) +
                                              " pitch:" + to_string(pitch_deg).substr(0,6) +
                                              " dist:" + to_string(tz).substr(0,6);
                                putText(result, info, Point(10, 30),
                                        FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0,255,255), 2);
                            }
                        }
                    }
                    // ---------- PnP 解算结束 ----------


                    vector<Point>drawCorners;//用来储存装甲板的四个角点


                    for(size_t k=0;k<armor.corners.size();k++){
                        const Point2f&p=armor.corners[k];
                        drawCorners.emplace_back(
                            cvRound(p.x),
                            cvRound(p.y)
                        );//将角点坐标转化为整数
                        putText(
                            result,
                            to_string(k),
                            Point(cvRound(p.x)+5,cvRound(p.y)-5),
                            FONT_HERSHEY_COMPLEX,
                            0.8,
                            Scalar(110,200,25),
                            2
                        );//在每个角点旁边写上编号

                    }
                    Rect numberRect=boundingRect(drawCorners);//计算装甲板角点的外接矩形
                    int expandX = numberRect.width*0.2;//将水平宽度扩大原宽度的20%
                    int expandY = numberRect.height*0.2; //竖直方向上扩大原高度的20%

                    numberRect.x -=expandX;//左边向左扩展
                    numberRect.y -=expandY;//上边向上扩展

                    numberRect.width += 2* expandX;//左右两边一起扩展
                    numberRect.height += 2*expandY;//上下两边一起扩展

                    Rect imageRect(0,0,frame.cols,frame.rows);//划一块区域 和图像一样大小
                    numberRect=numberRect&imageRect;//取交集 防止越界
                    Mat numberROI = frame(numberRect);//从画面中截取感兴趣区域
                    if(numberROI.empty()){
                        continue;
                    }
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
                    int prediction = static_cast<int>(svm_->predict(feature));//获取SVM识别到的数字
                    armor.number=prediction;//将识别到的数字存入armor.number
                    armors.push_back(armor);//将当前识别到的加入装甲板组
                    static int am_count=0;
                    if(++am_count%10==0)
                        RCLCPP_INFO(this->get_logger(), "装甲板总数 %d", am_count);
                    string text = to_string(armor.number);//将数字转换为string字符串
                    putText(result,
                            text,
                            Point(numberRect.x,numberRect.y),
                            FONT_HERSHEY_COMPLEX,
                            1,
                            Scalar(255,255,255),
                            2);

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


        // ===================== 自动扫描（改动点6） =====================
        // 放在整帧识别之后：如果一直没有瞄准到目标（超过 scan_timeout_s_），
        // 就让摄像头自己左右摆动，主动去找敌人。找到后上面 PnP 分支会刷新
        // last_aim_time_，扫描就会自动让位，不会和瞄准打架。
        if(scan_enable_)
        {
            rclcpp::Time now = this->get_clock()->now();

            // 首次处理图像时先记下时间，"比赛开始后"从这一刻开始计时，
            // 这样即使一直没看到敌人，超时后也会开始转动搜索。
            if(!last_aim_time_.nanoseconds())
                last_aim_time_ = now;

            double since_aim = (now - last_aim_time_).seconds();// 距上次瞄准过了多久

            if(since_aim > scan_timeout_s_)
            {
                // 刚进入扫描时，以当前云台朝向作为摆动中心，避免突然乱转
                if(!is_scanning_)
                {
                    is_scanning_ = true;
                    scan_center_yaw_ = last_state_.yaw_degrees;
                }

                // 加上一条正弦摆动曲线
                double phase = now.seconds() * 2.0 * CV_PI / scan_period_s_;// 相位
                double scan_yaw = scan_center_yaw_ + scan_amplitude_deg_ * std::sin(phase);

                // 只发角度、不发开火许可，避免乱开枪
                publishAim(scan_yaw, scan_pitch_deg_, false);

                // 画面上提示正在扫描
                putText(result, "SCANNING", Point(10, 60),
                        FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0,165,255), 2);
            }
            else
            {
                is_scanning_ = false;//瞄准到目标了，退出扫描
            }
        }
        // ===================== 自动扫描结束 =====================

        cv::imshow(
            "result",
            result
        );//显示最终识别结果

        cv::Mat parameterImage =
            cv::Mat::zeros(
                150,
                600,
                CV_8UC3
            );//创建一张黑色参数窗口背景

        cv::imshow(
            "参数",
            parameterImage
        );//显示参数窗口


        cv::waitKey(1);//等待1毫秒，让OpenCV窗口刷新
    }


private:

    rclcpp::Subscription<
        sensor_msgs::msg::CompressedImage
    >::SharedPtr image_sub_;//用来储存图像订阅器
    
    cv::Ptr<cv::ml::SVM> svm_;//用来储存SVM模型

    cv::Mat cameraMatrix_;//储存相机内参矩阵K
    cv::Mat distCoeffs_;//储存相机畸变参数D

    rclcpp::Subscription<
        tdt_interface::msg::AutoAimFrame
    >::SharedPtr frame_sub_;//储存AutoAimFrame订阅器

    rclcpp::Publisher<
        tdt_interface::msg::SendData
    >::SharedPtr send_data_pub_;//储存瞄准指令发布器

    tdt_interface::msg::AutoAimState last_state_;//保存最近一次AutoAimFrame里的自身状态

    // ---------- 阵营相关成员（改动点3） ----------
    std::string team_;//保存当前阵营字符串（blue / red）
    int player_id_ = PLAYER_ID_BLUE;//当前阵营对应的 player 编号：1=蓝，2=红
    bool is_blue_ = true;//当前是否为蓝方，便于日志与后续按阵营做差异化处理

    // ---------- 自动扫描相关成员（改动点7） ----------
    rclcpp::Time last_aim_time_;//最近一次成功瞄准到目标的时刻（未初始化表示还没瞄过）
    double scan_center_yaw_ = 0.0;//扫描中心角，进入扫描时锁定为当时的朝向
    bool is_scanning_ = false;//当前是否正在扫描摆动

    // publishAim: 统一封装瞄准/扫描指令的发布（把世界角规范到[-180,180]，防止绕圈）
    //   world_yaw_deg : 要发送的世界光轴角（度）
    //   pitch_deg     : 俯仰（度，向下为正）
    //   shoot         : 是否允许开火
    void publishAim(double world_yaw_deg, double pitch_deg, bool shoot)
    {
        // 把角度规范到 [-180,180]，否则连续累加会越绕越大
        while(world_yaw_deg > 180.0)  world_yaw_deg -= 360.0;
        while(world_yaw_deg < -180.0) world_yaw_deg += 360.0;

        tdt_interface::msg::SendData send_msg;
        send_msg.yaw = static_cast<float>(world_yaw_deg);
        send_msg.pitch = static_cast<float>(pitch_deg);
        send_msg.if_shoot = shoot;
        send_data_pub_->publish(send_msg);
    }

    // frameCallback: 从AutoAimFrame消息中取出camera_info，保存到cameraMatrix_/distCoeffs_，供solvePnP使用
    void frameCallback(
        const tdt_interface::msg::AutoAimFrame::SharedPtr msg
    )
    {
        // CameraInfo.k 是按行存储的3x3内参矩阵 [fx,0,cx, 0,fy,cy, 0,0,1]
        if(msg->camera_info.k.size() >= 9)
        {
            cameraMatrix_ = (cv::Mat_<double>(3,3) <<
                msg->camera_info.k[0], msg->camera_info.k[1], msg->camera_info.k[2],
                msg->camera_info.k[3], msg->camera_info.k[4], msg->camera_info.k[5],
                msg->camera_info.k[6], msg->camera_info.k[7], msg->camera_info.k[8]);
        }
        // distortion_model 里畸变系数一般为5个 [k1,k2,p1,p2,k3]
        if(!msg->camera_info.d.empty())
        {
            distCoeffs_ = cv::Mat(msg->camera_info.d).clone();
        }
        // 保存当前云台世界姿态，供把相机系yaw换成世界yaw
        last_state_ = msg->state;
        static int fc_count=0;
        if(++fc_count%100==0)
            RCLCPP_INFO(this->get_logger(), "frameCallback收到 %d 次, cameraMatrix_ empty=%d", fc_count, cameraMatrix_.empty());
    }

    void imageCallback(
        const sensor_msgs::msg::CompressedImage::SharedPtr msg
    )
    {
        cv::Mat frame=
            cv::imdecode(
                msg->data,
                cv::IMREAD_COLOR
            );//将ROS 2中的压缩图像解码成OpenCV图像


        if(frame.empty())
        {
            RCLCPP_WARN(
                this->get_logger(),
                "图像解码失败"
            );//输出图像解码失败的警告

            return;
        }


        processFrame(
            frame
        );//将接收到的图像交给OpenCV处理
        static int ic_count=0;
        if(++ic_count%100==0)
            RCLCPP_INFO(this->get_logger(), "imageCallback收到 %d 帧", ic_count);
    }
};


int main(int argc,char* argv[])
{
    rclcpp::init(
        argc,
        argv
    );//初始化ROS 2


    auto node=
        std::make_shared<CampusNode>();//创建校园赛节点


    rclcpp::spin(
        node
    );//让节点持续运行并等待ROS 2消息


    rclcpp::shutdown();//关闭ROS 2


    return 0;
}
