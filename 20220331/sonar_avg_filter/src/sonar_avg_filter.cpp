 #include "ros/ros.h"
 #include "std_msgs/Int8.h"
 #include "std_msgs/Float32.h"
 #include "std_msgs/String.h"
 #include "std_msgs/Bool.h"  
 #include "sensor_msgs/Range.h"  
 #include "geometry_msgs/Twist.h" 
 #include "nav_msgs/Odometry.h"
 
 sensor_msgs::Range avg_range;
 
 float data = 0.0;
 int count = 0;
 
 void Range_Avg_Callback(const sensor_msgs::Range::ConstPtr&msg){
	 
	 data = msg -> range;
	
	 if(count<=4){
		 
		 avg_range.range += data;
		 // ROS_INFO("Range: [%f]\n", msg->range);
		 count++;
		 
	 }
	 else{
		 
	 avg_range.range = avg_range.range/5.0;
	 // ROS_INFO("Average Range: [%f]\n", avg_range.range);
	 count = 0;
	 avg_range.range = 0;
	 
 }
 }
 
 int main(int argc, char **argv){
	 
	 ros::init(argc, argv, "sonar_avg_filter");
	 
	 ros::NodeHandle n;
	 
	 ros::Subscriber sub_avg_range = n.subscribe("/range", 1000, Range_Avg_Callback);
	 ros::Publisher pub_avg_range= n.advertise<sensor_msgs::Range>("/range_avg", 1000);
	 
	 ros::Rate loop_rate(10);  
	 
	 while(ros::ok()){
		 
		 pub_avg_range.publish(avg_range);
		 
		 loop_rate.sleep();
		 ros::spinOnce();
		 
	 }
	 
	 return 0;
	 
 }
