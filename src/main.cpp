#include<iostream>
#include "manager.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <vector>
#include <chrono>
#include <map>
#include <cmath>
#include <time.h>
#include <jsoncpp/json/json.h>

#include "kafka.h"
using namespace cv;

bool parse_args(int argc, char** argv,  std::string& video_uri) {
    if (argc < 2) 
	{
		printf("U should input video uri first ! \n");
		return false;
	}
	else{
        auto video_uri = std::string(argv[1]);
        return true;
    }
    return false;
}


int main(int argc ,char ** argv){
	// calculate every person's (id,(up_num,down_num,average_x,average_y))
	map<int,vector<int>> personstate;
	map<int,int> classidmap;
	if (argc < 2) 
	{
		printf("U should input video uri first ! \n");
		return false;
	}
	std::string video_uri = argv[1];	
	char* yolo_engine = (char *)"../resources/yolov5s.engine";
	char* sort_engine = (char * )"../resources/deepsort.engine";
	float conf_thre = 0.35;
	Trtyolosort yosort(yolo_engine,sort_engine);
	VideoCapture capture;
	cv::Mat frame;
	// maybe consider haikang camera
	frame = capture.open(video_uri);
	if (!capture.isOpened()){
		std::cout<<"can not open "  << video_uri <<std::endl;
		return -1 ;
	}

	// init kafka
    ProducerKafka* producer = new ProducerKafka;  
    if (PRODUCER_INIT_SUCCESS == producer->init_kafka(0, (char*)"10.10.31.97:9092", (char*)"test-topic"))  
    {  
        printf("producer init success\n");  
    }  
    else  
    {  
        printf("producer init failed\n");  
        return -1;  
    }  

	std::vector<DetectBox> det;
	clock_t start_draw,end_draw;
	start_draw = clock();

	// package
	Json::StreamWriterBuilder writerBuilder;
	std::unique_ptr<Json::StreamWriter> json_write(writerBuilder.newStreamWriter());
	
	// main loop
	while(capture.read(frame)){
		auto start = std::chrono::system_clock::now();
		yosort.TrtDetect(frame,conf_thre,det);
		auto end = std::chrono::system_clock::now();
		int delay_infer = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		std::cout  << "all delay_infer:" << delay_infer << "ms" << std::endl;
		
		std::ostringstream ss;
		Json::Value root;
  		Json::Value arrayObj;
		for (auto box : det) {
			Json::Value singleObj;

			singleObj["xmin"] = (int)box.x1;
			singleObj["xmax"] = (int)box.x2;
			singleObj["ymin"] = (int)box.y1;
			singleObj["ymax"] = (int)box.y2;
			singleObj["classId"] = box.classID;
			singleObj["trackId"] = box.trackID;
			singleObj["confidence"] = box.confidence;
			// add plate. if exists do nothing else ,try to rec plate.
			singleObj["plate"] = "";

			arrayObj.append(singleObj);
		}
		root["algoResult"] = arrayObj;
		root["deviceId"] = "syagx_001";
		std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
        	std::chrono::system_clock::now().time_since_epoch());
		root["timeStamp"] = to_string(ms.count());

		json_write->write(root, &ss);
		std::string strContent = ss.str();
		// std::cout << "strContent : " << strContent  << strContent.length() << std::endl;

		char kafka_data[4096];
		memcpy(kafka_data,strContent.c_str() + '\0',strContent.length() +1 );
		if (PUSH_DATA_SUCCESS == producer->push_data_to_kafka(kafka_data, strlen(kafka_data)))  
			printf("push data success \n");  
		else  
			printf("push data failed \n"); 
	}
	capture.release();
	// producer->destroy();  

	return 0;
	
}
