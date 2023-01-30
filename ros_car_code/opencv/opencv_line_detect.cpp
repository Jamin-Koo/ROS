#include <ros/ros.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>

#include "std_msgs/Int16.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdio.h> 
#include <string.h> 
#include <stdio.h>
#include <unistd.h>   
#include <stdint.h>   
#include <stdlib.h>  
#include <errno.h>

using namespace cv;
using namespace std;

#define IMG_Width     1280
#define IMG_Height    720

#define USE_DEBUG  1   // 1 Debug  사용
#define USE_CAMERA 1   // 1 CAMERA 사용  0 CAMERA 미사용

#define ROI_CENTER_Y  240  // 0~360
#define ROI_WIDTH     60

#define ASSIST_BASE_LINE 130
#define ASSIST_BASE_WIDTH 30

#define NO_LINE 20

using namespace cv;
using namespace std;

std::string gstreamer_pipeline (int capture_width, int capture_height, int display_width, int display_height, int framerate, int flip_method) {
    return "nvarguscamerasrc ! video/x-raw(memory:NVMM), width=(int)" + std::to_string(capture_width) + ", height=(int)" +
           std::to_string(capture_height) + ", format=(string)NV12, framerate=(fraction)" + std::to_string(framerate) +
           "/1 ! nvvidconv flip-method=" + std::to_string(flip_method) + " ! video/x-raw, width=(int)" + std::to_string(display_width) + ", height=(int)" +
           std::to_string(display_height) + ", format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! appsink";
}

Mat Canny_Edge_Detection(Mat img)
{
   Mat mat_blur_img, mat_canny_img;
   blur(img, mat_blur_img, Size(3,3));                 // noise del
   Canny(mat_blur_img,mat_canny_img, 100,200,3);        // canny edge

   return mat_canny_img;
}


Mat Region_of_Interest(Mat image, Point *points)
{
  Mat img_mask =Mat::zeros(image.rows,image.cols,CV_8UC1);
 
  Scalar mask_color = Scalar(255,255,255);
  const Point* pt[1]={ points };    
  int npt[] = { 4 };
  fillPoly(img_mask,pt,npt,1,Scalar(255,255,255),LINE_8);
  Mat masked_img;
  bitwise_and(image,img_mask,masked_img);
 
  return masked_img;
}

Mat Region_of_Interest_crop(Mat image, Point *points)
{
   Mat img_roi_crop;

   Rect bounds(0,0,image.cols,image.rows);
   Rect r(points[0].x,points[0].y,image.cols, points[2].y-points[0].y);  
   //printf("%d %d %d %d\n",points[0].x,points[0].y,points[2].x, points[2].y-points[0].y);
   //printf("%d  %d\n", image.cols, points[2].y-points[0].y);

   img_roi_crop = image(r & bounds);
   
   return img_roi_crop;
}




int main(int argc, char **argv)
{
    /////////////////////////////////   영상 변수 선언  ////////////////////////////////////
    int img_width, img_height;
 
    Mat mat_image_org_color(IMG_Height,IMG_Width,CV_8UC3);
    Mat mat_image_org_gray;
    Mat mat_image_roi;
    Mat mat_image_canny_edge;
   
    Point points[4];
   
    int capture_width = 1280 ;
    int capture_height = 720 ;
    int display_width = 640 ;
    int display_height = 360 ;
    int framerate = 60 ;
    int flip_method = 2 ;
    
    /// my use ////
    int img_width_quarterL = 40;
    int img_width_quarterR = 605;   


    /// my use ////
    img_width  = 640;
    img_height = 360;

    img_width  = 640;

    if(USE_CAMERA == 0) img_height = 480;

    ////////////////////////// steering control /////////////////////////////
   int i;
   int steer_angle_new, steer_angle_old;
   
   steer_angle_new = steer_angle_old =0; 
 
   float gradient[NO_LINE]  = {0,};
   float intersect[NO_LINE] = {0,};
   float intersect_base[NO_LINE]  = {0,};
   float c_x_sum=0;
   /////////////////////////////////////////////////////////////////////////
    std::string pipeline = gstreamer_pipeline(capture_width,
        capture_height,
        display_width,
        display_height,
        framerate,
        flip_method);
    std::cout << "Using pipeline: \n\t" << pipeline << "\n";
 
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
   
   // cap.set(CV_CAP_PROP_FRAME_WIDTH, img_width);
//cap.set(CV_CAP_PROP_FRAME_HEIGHT, img_height);

   

if(!cap.isOpened())
{
cerr <<"Error , 카메라를 열 수 없습니다. \n";
mat_image_org_color = imread("./img/line_1.jpg", IMREAD_COLOR);
img_height = mat_image_org_color.rows;
img_width  = mat_image_org_color.cols;
//return -1;  
}
else
{
printf("카메라가 잘 작동 됩니다.\n");
cap.read(mat_image_org_color);
}


if(USE_CAMERA == 0)  mat_image_org_color = imread("./img/line_2.jpg", IMREAD_COLOR);
     
   
    if(mat_image_org_color.empty())
    {
       cerr << "image file error!";
    }

   ros::init(argc, argv, "ros_opencv_line");

  /**
   * NodeHandle is the main access point to communications with the ROS system.
   * The first NodeHandle constructed will fully initialize this node, and the last
   * NodeHandle destructed will close down the node.
   */
   ros::NodeHandle nh;
   
   ros::Publisher car_control_pub_cmd = nh.advertise<std_msgs::Int16>("Car_Control_cmd/SteerAngle_Int16", 10);
   
   std_msgs::Int16 cmd_steering_msg;      
   cmd_steering_msg.data  = 0;
   
   std::cout << "OpenCV version : " << CV_VERSION << std::endl;
  
   ros::Rate loop_rate(10);
   int countRos = 0;
   
   float  c[NO_LINE] = {0.0, };
   float  d[NO_LINE] = {0.0, };
   float  line_center_x = 0.0; 
   float  steering_conversion = 0.7;   // steering control value

    Scalar GREEN(0,255,0);
    Scalar RED(255,0,0);
    Scalar BLUE(0,0,255);
    Scalar YELLOW(255,255,0);
    //////////////////////////////////////////////////////////////////////////////////////

   
    printf("Image size[%3d,%3d]\n", img_width,img_height);
   
    namedWindow("Display Window", cv::WINDOW_NORMAL);
    resizeWindow("Display Window", img_width,img_height);
    moveWindow("Display Window", 10, 10);
   
    namedWindow("Gray Image Window", cv::WINDOW_NORMAL);
    resizeWindow("Gray Image Window", img_width,img_height);
    moveWindow("Gray Image Window", 700, 10);
   
    namedWindow("Gray ROI Image Window", cv::WINDOW_AUTOSIZE);  
    moveWindow("Gray ROI Image Window", 10, 600);
   
    namedWindow("Canny Edge Image Window", cv::WINDOW_AUTOSIZE);  
    moveWindow("Canny Edge Image Window", 700, 600);
   
   
   

points[0] =  Point(0,ROI_CENTER_Y-ROI_WIDTH);
points[1] =  Point(0,ROI_CENTER_Y+ROI_WIDTH);
points[2] =  Point(img_width,ROI_CENTER_Y+ROI_WIDTH);
points[3] =  Point(img_width,ROI_CENTER_Y-ROI_WIDTH);
   // imshow("Display Window", mat_image_org_color);

   // for(int j = 0; j<4; j++){
   //    printf("\n\n%lf",points[j]);
   // }


 //////////==   ==  line_center_x ==  ==//////////

    while (ros::ok())
    {
      if(USE_CAMERA == 1)  cap.read(mat_image_org_color);
      else                 mat_image_org_color = imread("./img/line_2.jpg", IMREAD_COLOR);    
      cvtColor(mat_image_org_color, mat_image_org_gray, CV_RGB2GRAY);        // color to gray conversion  
      mat_image_roi = Region_of_Interest_crop(mat_image_org_gray,points);    // ROI ¿µ¿ªÀ» ÃßÃâÇÔ      
      mat_image_canny_edge = Canny_Edge_Detection(mat_image_roi);
     
    vector<Vec4i> linesP;
    HoughLinesP(mat_image_canny_edge, linesP, 1, CV_PI/180, 70,30,40);     //라인검출 최소 교차수, 직선의 최소 길이, 선 위의 점들 사이의 최대 거리(길면 다른 선)
    printf("Line Number : %3d\n", linesP.size());
 
    line_center_x = 0.0;

    /// my use ////
    int countR = 0;     // > 340
    int countL = 0;     // < 340
    int count = 0;      // ++ ==> line_center_x = 0
    int count4 = 0;
    int count3 = 0;     // linesP == 3 use
    int count2 = 0;
    int count1 = 0;
    
    float selectR[20] = {0.0, };
    float selectL[20] = {0.0, };

    float sum_selectR = 0.0;
    float sum_selectL = 0.0;

    /// my use ////
for(int i=0; i<linesP.size();i++){

    float intersect = 0.0;

    if(i>=NO_LINE) break;
    Vec4i L= linesP[i];

    // int cx1 = linesP[i][0];
    // int cy1 = linesP[i][1];
    // int cx2 = linesP[i][2];
    // int cy2 = linesP[i][3];


    c[i] =  (L[2]-L[0])/(L[3]-L[1]);
    d[i] = (L[0]+L[2])/2 - (c[i] *(L[1]+L[3])) ;

      // for(int j = 0; j<4; j++){
      //    printf("\n\n%lf",L[j]);
      // }

    intersect = c[i]*(float)ROI_CENTER_Y +d[i];

        if((L[0]+L[2])/2 < 340){
            countL ++;
            selectL[i] = intersect;
            sum_selectL += selectL[i];

        }
        else if((L[0]+L[2])/2 > 340){
            countR ++;
            selectR[i] = intersect;
            sum_selectR += selectR[i];

        }


    if(linesP.size() > 4){

        if (countR > 2) {
            count++;
            intersect = 0;
        }

        else if (countL > 2) {
            count++;
            intersect = 0;
        }
    }

    else if(linesP.size() == 4){

        if (countR > 2) {
            count4++;
            count++;
            intersect = 0;
        }

        else if (countL > 2) {
            count4--;
            count++;
            intersect = 0;
        }
    }

    else if(linesP.size() == 3){
        if (countR == 3) {
            count3 ++;
            count++;
        }
        else if (countR == 2) {
            count3 ++;
        }
        else if (countR == 1) {
            count3 --;
        }
        else {
            count3 ++;
            count++;
        }
    }

    else if(linesP.size() == 2){
        if(countR==2){
            ++count2;
        }//Right2
        else if (countR == 1 && countL == 1){
            count2 = 0;
        }//center
        else if(countR == 0) {
            --count2;
        }//Left2

    }

    else if(linesP.size() == 1){
        if(countR == 1){
            count1 ++;
        }
        else {
            count1 --;
        }

    }

line_center_x += intersect;

line(mat_image_org_color,Point(L[0],L[1]+ROI_CENTER_Y-ROI_WIDTH),Point(L[2],L[3]+ROI_CENTER_Y-ROI_WIDTH), Scalar(255,0,255), 3, LINE_AA);    //dont touch _ line_detect

    if(USE_DEBUG ==1){
        printf("L[%d] :[%3d, %3d] , [%3d , %3d] \n",i,  L[0],L[1], L[2],L[3]);
        //printf("x[%d] = [%6.3f] *y + [%6.3f] \n",i, c[i],d[i]);
        //printf("x[%d] = [%f] *y + [%f] \n", i,c[i],d[i]);
        //printf("H :[%3d , %3d] , [%3d , %3d] \n", cx1,cy1, cx2,cy2);
        printf("intersect =[%f] [%f]\n", intersect, line_center_x);
        printf("countR = %d, countL = %d, count =%d, count2 =%d\n",countR,countL,count, count2);
        printf("selectL = %f, selectR = %f, \n", selectL[i], selectR[i]);
    }
}  //for exit______________


// ((sum_selectL/countL)+(sum_selectR/countR))/2-img_width/2
// if (countR == 0 || countL == 0) line_center_x = 0;
// if (countR == 0) line_center_x = (sum_selectL/countL)- 100 ;
// else if (countL == 0) line_center_x = (sum_selectR/countR)- 100 ;      

 if(linesP.size() > 4) {
    line_center_x = line_center_x = ((sum_selectL/countL)+(sum_selectR/countR))/2 - img_width/2;
    }    // linesP >= 4 

 else if(linesP.size() == 4) {          //inesP == 4
    if (count == 2){
        count4 > 0 ? line_center_x = - 100 : line_center_x = 100;
    }
    else if(count==1){
       line_center_x = ((sum_selectL/countL)+(sum_selectR/countR))/2 - img_width/2;
    }
    else {
       line_center_x = ((sum_selectL/countL)+(sum_selectR/countR))/2 - img_width/2;
    }
 }

 else if(linesP.size() == 3) {  
    if (countR == 3 || countL == 3) {
        countR > countL ? sum_selectR/countR - img_width_quarterR : sum_selectL/countL - img_width_quarterL;
 } 
    else{
    count3 > 0 ? line_center_x = ((sum_selectL/countL)+(sum_selectR/countR))/2 - img_width/2 - (img_width + img_width_quarterR)/2 : line_center_x = line_center_x / (float)(linesP.size()-count) - (img_width - img_width_quarterL)/2; 
}
}              // linesP == 3
 
 else if(linesP.size() == 2) {
    if(count2 == 0 ) {line_center_x = line_center_x / (float)(linesP.size()-count) - img_width/2 ;} // L1R1
    else if(count2 >= 1)  {
            line_center_x = sum_selectR/countR - img_width_quarterR;
        } // R2
        else if(count2 <= -1) {
        line_center_x = sum_selectL/countL - img_width_quarterL;
    } // L2
 }// linesP == 2


 else if(linesP.size() == 1) {line_center_x = 0 ;}              // linesP == 1
 
 //////////==   ==  line_center_x ==  ==//////////
 
    else {
        line_center_x = 100;
        steer_angle_old =  steer_angle_new ;
        }

    steer_angle_new = (int)( line_center_x*steering_conversion);  //스티어링 조정값 계수 수정 필요 
    printf("line_center_x= %f    steer_angle_new = %d\n",line_center_x , steer_angle_new);
    printf("Line sumL : %f , sumR : %f\n",sum_selectL,sum_selectR);

    cmd_steering_msg.data  = steer_angle_new;      //
        
    if(steer_angle_old !=  steer_angle_new) car_control_pub_cmd.publish(cmd_steering_msg);
 
 if(USE_DEBUG ==1)
 {
    printf("\n\n\n");
 }

     int guide_width1= 85;
      int guide_height1= 20;
      int guide_l_center = 130;  //130
      int guide_r_center = img_width-130; // 520

  rectangle(mat_image_org_color, Point(10,ASSIST_BASE_LINE-ASSIST_BASE_WIDTH),Point(img_width-10 ,ASSIST_BASE_LINE+ASSIST_BASE_WIDTH),Scalar(0,255,0), 1, LINE_AA);
      line(mat_image_org_color,Point((int)line_center_x+img_width/2,ROI_CENTER_Y-ROI_WIDTH),Point((int)line_center_x+img_width/2,ROI_CENTER_Y+ROI_WIDTH), Scalar(0,255,255), 1, LINE_AA);
     
     
      line(mat_image_org_color, Point(guide_l_center-guide_width1,ASSIST_BASE_LINE),Point(guide_l_center+guide_width1,ASSIST_BASE_LINE ), Scalar(0,255,255),1,0);
      line(mat_image_org_color, Point(guide_l_center-guide_width1,ASSIST_BASE_LINE-guide_height1),Point(guide_l_center-guide_width1,ASSIST_BASE_LINE+guide_height1 ), Scalar(0,255,255),1,0);
      line(mat_image_org_color, Point(guide_l_center+guide_width1,ASSIST_BASE_LINE-guide_height1),Point(guide_l_center+guide_width1,ASSIST_BASE_LINE+guide_height1 ), Scalar(0,255,255),1,0);
     
      line(mat_image_org_color, Point(guide_r_center-guide_width1,ASSIST_BASE_LINE),Point(guide_r_center+guide_width1,ASSIST_BASE_LINE ), Scalar(0,255,255),1,0);
      line(mat_image_org_color, Point(guide_r_center-guide_width1,ASSIST_BASE_LINE-guide_height1),Point(guide_r_center-guide_width1,ASSIST_BASE_LINE+guide_height1 ), Scalar(0,255,255),1,0);
      line(mat_image_org_color, Point(guide_r_center+guide_width1,ASSIST_BASE_LINE-guide_height1),Point(guide_r_center+guide_width1,ASSIST_BASE_LINE+guide_height1 ), Scalar(0,255,255),1,0);
               

      line(mat_image_org_color, Point(img_width/2, ASSIST_BASE_LINE-guide_height1*1.5) ,Point(img_width/2, ASSIST_BASE_LINE+guide_height1*1.5), Scalar(255,255,255), 2,0);

      imshow("Display Window", mat_image_org_color);

      imshow("Gray Image Window", mat_image_org_gray);
      imshow("Gray ROI Image Window",mat_image_roi);  
      imshow("Canny Edge Image Window",mat_image_canny_edge);
    
      if (waitKey(25) >= 0)
      {
            break;
      }
        ros::spinOnce();

        loop_rate.sleep();
        ++countRos;
     }            
   
    if(USE_CAMERA == 1)  cap.release();    
    destroyAllWindows();

 
   return 0;
}
