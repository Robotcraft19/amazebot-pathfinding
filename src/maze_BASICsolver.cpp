#include <iostream>
#include "ros/ros.h"
#include "geometry_msgs/Twist.h"
#include "sensor_msgs/LaserScan.h"

#define TARGET_DISTANCE 0.20

class BasicSolver {

private:

    ros::NodeHandle n;

    // Publishers
    ros::Publisher cmd_vel_pub;

    // Subscribers
    ros::Subscriber front_ir_sub;
    ros::Subscriber left_ir_sub;
    ros::Subscriber right_ir_sub;

    // Global variables
    float front_distance;
    float left_distance;
    float right_distance;

    // PID control
    float old_prop_error;
    float integral_error;
    
    float KP = 10.0;
    float KI = 0.0;
    float KD = 0.0;
    float time_interval = 0.1;

    bool robot_lost;
    int lost_counter;


    geometry_msgs::Twist calculateCommand(){

	// Create message
	auto msg = geometry_msgs::Twist();

	if (front_distance < TARGET_DISTANCE) 
    {
        // Prevent robot from crashing
        msg.angular.z = 1.5;
        msg.linear.x = -0.04;
    } 
    else if (robot_lost == true)
    {
        // Robot is lost, go straight to find wall
        msg.linear.x = 0.08;
    } 
    else 
    {
        // Robot keeps using normal PID controller
        float gain = calculateGain(right_distance);
        msg.linear.x = 0.08;
        msg.angular.z = gain;
    }

	return msg;

    }
    

    void frontIRCallback(const sensor_msgs::LaserScan::ConstPtr& msg){
    	this->front_distance = msg->ranges[0]; // Extract range, first (and only) element of array
    }
    void leftIRCallback(const sensor_msgs::LaserScan::ConstPtr& msg){
    	this->left_distance = msg->ranges[0]; // Extract range, first (and only) element of array
    }
    void rightIRCallback(const sensor_msgs::LaserScan::ConstPtr& msg){
    	this->right_distance = msg->ranges[0]; // Extract range, first (and only) element of array
    }

    float calculateGain(float value) 
	{	
	    float error = TARGET_DISTANCE - value;
	    //if (error >= TARGET_DISTANCE / 2) error = TARGET_DISTANCE / 2.0;
	    //if (error <= -TARGET_DISTANCE) error = -TARGET_DISTANCE / 10.0;
	    float new_der_err = error - this->old_prop_error;
	    float new_int_err = this->integral_error + error;

	    float gain = this->KP*error + this->KI*new_int_err*this->time_interval
	                 + this->KD*new_der_err/this->time_interval;

	    this->old_prop_error = error;
	    this->integral_error = new_int_err;         

	    return gain;
	}

	void calculateRobotLost() 
	{
	    // Calculations needed to check if robot is lost
	    if (front_distance > TARGET_DISTANCE && right_distance > TARGET_DISTANCE 
	        && left_distance > TARGET_DISTANCE) 
	    {
	            ++lost_counter;

	            if (lost_counter >= 75) robot_lost = true;
	    } 
	    else if(front_distance < TARGET_DISTANCE || right_distance < TARGET_DISTANCE) 
	    {
	            robot_lost = false;
	            lost_counter = 0;
	    }
	}


public:

    BasicSolver(){
        // Initialize ROS
        this->n = ros::NodeHandle();

        // Setup publishers
        // Create a publisher object, able to push messages
    	this->cmd_vel_pub = this->n.advertise<geometry_msgs::Twist>("cmd_vel", 5);

        // Setup subscribers
        this->front_ir_sub = this->n.subscribe("base_scan_1", 10, &BasicSolver::frontIRCallback, this);
        this->left_ir_sub = this->n.subscribe("base_scan_2", 10, &BasicSolver::leftIRCallback, this);
        this->right_ir_sub = this->n.subscribe("base_scan_3", 10, &BasicSolver::rightIRCallback, this);
    }

    void run(){
        // Send messages in a loop
        ros::Rate loop_rate(10);
        while (ros::ok())
        {
        	// Calculate the command to apply
        	auto msg = calculateCommand();

        	 // Publish the new command
        	this->cmd_vel_pub.publish(msg);

            // Receive messages and publish inside callbacks
            ros::spinOnce();

            // And throttle the loop
            loop_rate.sleep();
        }
    }
};


int main(int argc, char **argv){
    // Initialize ROS
    ros::init(argc, argv, "maze_basic_solver");

    // Create our controller object and run it
    auto controller = BasicSolver();
    controller.run();

    // And make good on our promise
    return 0;
}