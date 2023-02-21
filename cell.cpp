#include<iostream>
#include <stdlib.h>  
#include <stdio.h>  
#include <thread>
#include <mutex>
#include <time.h>
#include <algorithm>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\imgproc\imgproc.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2\core.hpp>



using namespace std;
using namespace cv;
bool mode = true;
  VideoCapture capture("E:\\vs\\red.mp4");
  //VideoCapture capture("http://blu:123@192.168.1.7:8081/");

//缓存容器
	vector<Mat> m_vec_frame;
//互斥锁,防止进程冲突
	mutex m_mutex;
	void run();
	bool get_frame(Mat & frame);
	void put_frame(Mat frame);
	void ChangeGain(Mat& src, Mat& des, float a, int b);

void run()
{

	Mat current_frame;
	capture >> current_frame;
	waitKey(50);
	while (1)
	{
		capture >> current_frame;

		if (current_frame.empty())
		{
			cout << "frame empty\n" << endl;
			return;
		}
		put_frame(current_frame);
	}
}

void put_frame(Mat frame)
{
	m_mutex.lock();
	//清理
	if (m_vec_frame.size() > 3)
		m_vec_frame.clear();
	//存入
	m_vec_frame.push_back(frame);
	m_mutex.unlock();
	return;
}

bool get_frame(Mat& frame)
{
	m_mutex.lock();
	if (m_vec_frame.size() < 1)
		return false;
	else
	{
		//从容器中取图像
		frame = m_vec_frame.back();
	}
	m_mutex.unlock();
	return true;
}


void ChangeGain(Mat& src, Mat& des, float a, int b)
{
	des.create(src.rows, src.cols, src.type());
	for (int r = 0; r < src.rows; r++)
	{
		for (int c = 0; c < src.cols; c++)
		{
			for (int i = 0; i < 3; i++)
			{
				des.at<Vec3b>(r, c)[i] =
					saturate_cast<uchar>(a * src.at<Vec3b>(r, c)[i] + b);
			}
		}
	}
}


int main(int argc, char* argv)
{
        utils::logging::setLogLevel(utils::logging::LOG_LEVEL_ERROR);
	    double t = 0;
	    string fps;		
		if (mode == true) 
		{
			//开始线程,防止网络摄像头延迟过高
			thread th1(run);
			th1.detach();
			waitKey(2000);
		}

	while (1)
	{
		

		t = (double)getTickCount();
		Mat OriginalFrame, ContrastFrame,gray,mask,bgr[3], minus[3];
       //OriginalFrame= imread("E:\\vs\\red3.png");
		if(mode==true){
			bool ret = get_frame(OriginalFrame);
			if (ret == false || OriginalFrame.empty()) 
			{
				cout << "false！" << endl;
				return 0;
			}
		}
		else 
		{
			capture>>OriginalFrame;
			if (OriginalFrame.empty())
			{
				cout << "false！" << endl;
				return 0;
			}
		}
		
		
		//对比度
		ChangeGain(OriginalFrame, ContrastFrame , 2, -80);

		Mat element11b11 = getStructuringElement(MORPH_ELLIPSE, Size(11, 11));
		Mat element3b3 = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
		//一次分离灯，泛用
		split(OriginalFrame, bgr);
		cvtColor(OriginalFrame, gray, COLOR_BGR2GRAY);
		threshold(gray, gray, 230, 255, THRESH_BINARY);
		minus[0] = bgr[2] - bgr[0];
		threshold(minus[0], minus[0], 60, 255, THRESH_BINARY);
		minus[1] = bgr[2] - bgr[1];
		threshold(minus[1], minus[1], 60, 255, THRESH_BINARY);
		minus[2] = minus[1] + minus[0];

		dilate(minus[2], minus[2], element11b11);
		gray = gray & minus[2];
        //二次分离灯，对暗处分离较好
		split(ContrastFrame, bgr);
		minus[0] = bgr[2] - bgr[0];
		threshold(minus[0], minus[0], 60, 255, THRESH_BINARY);
		minus[1] = bgr[2] - bgr[1];
		threshold(minus[1], minus[1], 60, 255, THRESH_BINARY);
		minus[2] = minus[1] + minus[0];

		dilate(minus[2], minus[2], element3b3);
		erode(minus[2], minus[2], element3b3);
		minus[2].copyTo(mask);

		threshold(mask, mask, 150, 255, THRESH_BINARY_INV);
		vector<vector<Point>> contoursbefore;
		vector<Vec4i> hierachybefore;

		findContours(minus[2], contoursbefore, hierachybefore, RETR_EXTERNAL, CHAIN_APPROX_NONE, Point(0, 0));
		for (size_t i = 0; i < contoursbefore.size(); i++)
		{
			drawContours(mask, contoursbefore, i, Scalar(0, 0, 0, 0), FILLED, 8, hierachybefore, 1, Point(0, 0));
		}
		minus[2] = mask + minus[2];
		erode(minus[2], minus[2], element3b3);
		threshold(minus[2], minus[2], 150, 255, THRESH_BINARY_INV);

		minus[2] = gray | minus[2];

		vector<vector<Point>> contours;
		vector<Vec4i> hierachy;
        findContours(minus[2], contours, hierachy, RETR_EXTERNAL, CHAIN_APPROX_NONE, Point(0, 0));

		//定义条件

		struct
		{
			double light_min_area;//最小面积
			double light_max_angle;//最大的倾斜角
			double light_min_size;
			double light_contour_min_solidity;//最小凸度
			double light_max_ratio;//最大长宽比
			double light_min_ratio;//最小长宽比
			double light_max_angle_diff;//最大角差
			double light_max_height_diff_ratio;//最大高度差比例
			double light_max_y_diff_ratio;//最大y差比例
			double light_min_x_diff_ratio;//最小x差比例
			double armor_max_aspect_ratio;//最大宽比例
			double armor_min_aspect_ratio;//最小宽比例

		}light;

		light.light_min_area = 30;
		light.light_min_size = 5.0;
		light.light_contour_min_solidity = 0.4;
		light.light_max_ratio = 0.8;
		light.light_min_ratio = 0.1;
		light.light_max_angle_diff = 3;
		light.light_max_height_diff_ratio = 1.3;
		light.light_max_y_diff_ratio = 1.4;
		light.light_min_x_diff_ratio = 0.8;
		light.armor_max_aspect_ratio = 5;
		light.armor_min_aspect_ratio = 1;
		vector<RotatedRect> lights;

		struct theRect
		{
			float width;
			float length;
			Point2f center;
			float angle;
			float area;
		};
		
		//精选灯

		for (size_t i = 0; i < contours.size(); i++) 
		{
			float lightContourArea = contourArea(contours[i]);
			if (lightContourArea < light.light_min_area)
				continue;
			RotatedRect lightRec;
			lightRec = fitEllipse(contours[i]);
			if (lightRec.angle > 45 && lightRec.angle < 135)
				continue;
			if (lightRec.size.width / lightRec.size.height > light.light_max_ratio 
				|| lightContourArea / lightRec.size.area() < light.light_contour_min_solidity 
				|| lightRec.size.width / lightRec.size.height < light.light_min_ratio)
				continue;
			lights.push_back(lightRec);
			Scalar color = (0, 250, 0, 255);
		}

		
		//排序灯
        
		if (lights.size() > 2)
		{
			for (int i = 0; i < lights.size() - 1; i++)	
			{
					for (int j = 0; j < lights.size() - i - 1; j++)	
					{
							if (lights[j + 1].center.x > lights[j].center.x)
							{
									RotatedRect temp = lights[j];
									lights[j] = lights[j + 1];
									lights[j + 1] = temp;
					
							}
					}
			}
		}

		struct roi {
			Point p1;
			Point p2;
			double angle;
		};
		vector<roi> rois;
		long same = 0, different = 0;
		double probability;
		for (size_t i = 0; i < lights.size(); i++) {
			for (size_t j = i + 1; j < lights.size(); j++) {
				RotatedRect& leftLight = lights[i];
				RotatedRect& rightLight = lights[j];
				double langle, rangle;
				if (leftLight.size.width <= leftLight.size.height)
				{
					langle = leftLight.angle + 90;
				}
				else
				{
					langle = leftLight.angle;
				}
				if (rightLight.size.width <= rightLight.size.height)
				{
					rangle = rightLight.angle + 90;
				}
				else
				{
					rangle = rightLight.angle;
				}

				float angleDiff = abs(leftLight.angle -rightLight.angle);
				if (angleDiff >= 90)angleDiff = 180-angleDiff;
				
				float LenDiff_ratio = max(leftLight.size.height,rightLight.size.height) / min(leftLight.size.height, rightLight.size.height);

				if (angleDiff > light.light_max_angle_diff || LenDiff_ratio > light.light_max_height_diff_ratio)
				{
					continue;
				}
				
				
				float dis = sqrt(pow(leftLight.center.x - rightLight.center.x, 2) + pow(leftLight.center.y - rightLight.center.y, 2));

				float Len = (leftLight.size.height + rightLight.size.height) / 2;

				float yDiff = abs(leftLight.center.y - rightLight.center.y);

				float yDiff_ratio = yDiff / Len;

				float xDiff = abs(leftLight.center.x - rightLight.center.x);

				float xDiff_ratio = xDiff / Len;

				float ratio = dis / Len;

				if (yDiff_ratio > light.light_max_y_diff_ratio || xDiff_ratio < light.light_min_x_diff_ratio || ratio > light.armor_max_aspect_ratio || ratio < light.armor_min_aspect_ratio)
				{
					continue;
				}
				
				roi temp;
				temp.p1 = Point(leftLight.center.x, leftLight.center.y + Len);
				temp.p2 = Point(rightLight.center.x, rightLight.center.y - Len);

				//转矩形
				double a = ((langle + rangle )) / 2,rx0=(temp.p1.x+temp.p2.x)/2, ry0=(temp.p1.y+temp.p2.y)/2;
				RotatedRect rRect1 = RotatedRect(Point2f(rx0,ry0 ), Size2f(xDiff, Len*2), a);
				//画矩形
				Point2f vertices1[4];      
				rRect1.points(vertices1);   
				for (int i = 0; i < 4; i++)
					line(OriginalFrame, vertices1[i], vertices1[(i + 1) % 4], Scalar(0, 255, 0, 255), 1, 8, 0);
				while (a > 45) {
					a -= 90;
				}
				temp.angle = a;	
				rois.push_back(temp);
				//画圈圈
				Scalar color = (0, 250, 0, 255);
				cout<<"装甲板坐标"<<i+1<<endl << (leftLight.center.x+rightLight.center.x)/2<<","  << (leftLight.center.y+rightLight.center.y )/2<< endl ;
				circle(OriginalFrame, Point((leftLight.center.x + rightLight.center.x) / 2, (leftLight.center.y + rightLight.center.y) / 2), 2, Scalar(0, 0, 255, 255), 2, 0);
				ellipse(OriginalFrame, leftLight.center, leftLight.size, leftLight.angle, 0, 360, color, 3);
				ellipse(OriginalFrame, rightLight.center, rightLight.size, rightLight.angle, 0, 360, color, 3);
			}
		}

	        Mat* Numbers = new Mat[rois.size()];
			int count = 0;
	        for (int i = 0; i < rois.size(); i++) 
			{
				Numbers[i] = ContrastFrame(Range(max(min(rois[i].p1.y, rois[i].p2.y),1), min(max(rois[i].p1.y, rois[i].p2.y), OriginalFrame.rows -1)),Range(max(min(rois[i].p1.x, rois[i].p2.x),1),min(max(rois[i].p1.x, rois[i].p2.x), OriginalFrame.cols-1)));
				cvtColor(Numbers[i], Numbers[i], COLOR_BGR2GRAY);
				threshold(Numbers[i], Numbers[i], 180, 255, THRESH_BINARY);
				count++;
			}
			int maxx, maxy,themax;
			//寻找最大装甲板（大的好识别）
			if(count>0)
			{
			for (int i = 0; i < count; i++) 
			{
				if(i==0)
				{
					maxy = Numbers[i].rows;
					maxx = Numbers[i].cols;
					themax = i;
				}
				else
				{
					if (Numbers[i].rows * Numbers[i].cols > maxx * maxy) 
					{
                        maxy = Numbers[i].rows;
					    maxx = Numbers[i].cols;
					    themax = i;
					}
				}
                 
			}
			//倾斜修正
			if (Numbers[themax].rows * Numbers[themax].cols > 200 * 200) 
			{
				Mat rotation_matix = getRotationMatrix2D(Point(100, 100), rois[themax].angle, 1.0);
				warpAffine(Numbers[themax], Numbers[themax], rotation_matix, Numbers[themax].size());
			}
			resize(Numbers[themax], Numbers[themax], Size(200,200), 0, 0, INTER_LINEAR);
			//模板对比
			Mat compare;
			string out;
			double maxp=0;
			int thenum=0;
			for(int num=0;num<10;num++){
				compare = imread("E:\\vs\\nums\\"+to_string(num)+".png");
				cvtColor(compare, compare, COLOR_BGR2GRAY);
				threshold(compare, compare, 180, 255, THRESH_BINARY);

			
				for (int i = 0; i < Numbers[themax].rows; i++)
				{
					for (int j = 0; j < Numbers[themax].cols; j++)
					{
						if (Numbers[themax].at<uchar>(i, j) == compare.at<uchar>(i, j))
						{
							same++;
						}
						else
						{
							different++;
						}
					}
				}
				probability = ((double)same / (double)(same + different));
				if (maxp < probability) {
					maxp = probability;
					thenum = num;
				}
				
			}
			
			//概率输出
				out = "equal:"+ to_string(thenum)+" p:"+to_string(maxp);
				cout << "大概是" << thenum<<endl;
				putText(OriginalFrame,out, cv::Point(10, 90), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255));
				imshow("number", Numbers[themax]);
			}
			
			
		t = (getTickCount() - t) / getTickFrequency();
		fps ="fps:"+ to_string((int)(1/t));
		putText(OriginalFrame, fps, cv::Point(10, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255));

		imshow("gray", minus[2]);
		imshow("output", OriginalFrame);
		
		waitKey(1);
	}
	return 0;
}

