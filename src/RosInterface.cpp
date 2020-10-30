//
// Created by controller on 1/11/18.
//

#include "phoxi_camera/RosInterface.h"

namespace phoxi_camera {
    RosInterface::RosInterface() : nh("~"), dynamicReconfigureServer(dynamicReconfigureMutex, nh),
                                   cloud_normal_preprocessed(new pcl::PointCloud<pcl::PointNormal>),
                                   PhoXi3DscannerDiagnosticTask("PhoXi3Dscanner",
                                                                boost::bind(&RosInterface::diagnosticCallback, this,
                                                                            _1)) {

        //create service servers
        getDeviceListService = nh.advertiseService("get_device_list", &RosInterface::getDeviceList, this);
        connectCameraService =nh.advertiseService("connect_camera", &RosInterface::connectCamera, this);
        isConnectedService = nh.advertiseService("is_connected", (bool (RosInterface::*)(phoxi_camera::IsConnected::Request&, phoxi_camera::IsConnected::Response&))&RosInterface::isConnected, this);
        isAcquiringService = nh.advertiseService("is_acquiring", (bool (RosInterface::*)(phoxi_camera::IsAcquiring::Request&, phoxi_camera::IsAcquiring::Response&))&RosInterface::isAcquiring, this);
        isConnectedServiceV2 = nh.advertiseService("V2/is_connected", (bool (RosInterface::*)(phoxi_camera::GetBool::Request&, phoxi_camera::GetBool::Response&))&RosInterface::isConnected, this);
        isAcquiringServiceV2 = nh.advertiseService("V2/is_acquiring", (bool (RosInterface::*)(phoxi_camera::GetBool::Request&, phoxi_camera::GetBool::Response&))&RosInterface::isAcquiring, this);
        startAcquisitionService = nh.advertiseService("start_acquisition", (bool (RosInterface::*)(std_srvs::Empty::Request&, std_srvs::Empty::Response&))&RosInterface::startAcquisition, this);
        stopAcquisitionService = nh.advertiseService("stop_acquisition", (bool (RosInterface::*)(std_srvs::Empty::Request&, std_srvs::Empty::Response&))&RosInterface::stopAcquisition, this);
        startAcquisitionServiceV2 = nh.advertiseService("V2/start_acquisition", (bool (RosInterface::*)(phoxi_camera::Empty::Request&, phoxi_camera::Empty::Response&))&RosInterface::startAcquisition, this);
        stopAcquisitionServiceV2 = nh.advertiseService("V2/stop_acquisition", (bool (RosInterface::*)(phoxi_camera::Empty::Request&, phoxi_camera::Empty::Response&))&RosInterface::stopAcquisition, this);
        triggerImageService =nh.advertiseService("trigger_image", &RosInterface::triggerImage, this);
        getFrameService = nh.advertiseService("get_frame", &RosInterface::getFrame, this);
        getAlignedDepthMapService = nh.advertiseService("get_aligned_depth_map", &RosInterface::getAlignedDepthMap, this);
        saveFrameService = nh.advertiseService("save_frame", &RosInterface::saveFrame, this);
        disconnectCameraService = nh.advertiseService("disconnect_camera", &RosInterface::disconnectCamera, this);
        getHardwareIdentificationService = nh.advertiseService("get_hardware_indentification", &RosInterface::getHardwareIdentification, this);
        getSupportedCapturingModesService = nh.advertiseService("get_supported_capturing_modes", &RosInterface::getSupportedCapturingModes, this);
        getApiVersionService = nh.advertiseService("get_api_version", &RosInterface::getApiVersion, this);
        getFirmwareVersionService = nh.advertiseService("get_firmware_version", &RosInterface::getFirmwareVersion, this);
#ifndef PHOXI_API_v1_1
        setCoordianteSpaceService = nh.advertiseService("V2/set_transformation",&RosInterface::setTransformation, this);
        setTransformationService = nh.advertiseService("V2/set_coordinate_space",&RosInterface::setCoordianteSpace, this);
        saveLastFrameService = nh.advertiseService("V2/save_last_frame", &RosInterface::saveLastFrame, this);
#endif

        //create publishers
        bool latch_topics;
        int topic_queue_size;
        nh.param<bool>("latch_topics", latch_topics, false);
        nh.param<int>("topic_queue_size", topic_queue_size, 1);
        cloudPub = nh.advertise<sensor_msgs::PointCloud2>("pointcloud", 1, latch_topics);
        normalMapPub = nh.advertise<sensor_msgs::Image>("normal_map", topic_queue_size, latch_topics);
        normalMapRectPub = nh.advertise<sensor_msgs::Image>("normal_map_rect", topic_queue_size, latch_topics);
        confidenceMapPub = nh.advertise<sensor_msgs::Image>("confidence_map", topic_queue_size, latch_topics);
        confidenceMapRectPub = nh.advertise<sensor_msgs::Image>("confidence_map_rect", topic_queue_size, latch_topics);
        rawTexturePub = nh.advertise<sensor_msgs::Image>("texture", topic_queue_size, latch_topics);
        rawTextureRectPub = nh.advertise<sensor_msgs::Image>("texture_rect", topic_queue_size, latch_topics);
        rgbTexturePub = nh.advertise<sensor_msgs::Image>("rgb_texture", topic_queue_size, latch_topics);
        rgbTextureRectPub = nh.advertise<sensor_msgs::Image>("rgb_texture_rect", topic_queue_size, latch_topics);
        depthMapPub = nh.advertise<sensor_msgs::Image>("depth_map", topic_queue_size, latch_topics);
        depthMapRectPub = nh.advertise<sensor_msgs::Image>("depth_map_rect", topic_queue_size, latch_topics);
        alignedDepthMapPub = nh.advertise < sensor_msgs::Image > ("aligned_depth_map", topic_queue_size, latch_topics);
        alignedDepthMapRectPub = nh.advertise < sensor_msgs::Image > ("aligned_depth_map_rect", topic_queue_size, latch_topics);
        externalCameraTexturePub = nh.advertise < sensor_msgs::Image > ("external_camera_texture", topic_queue_size,latch_topics);
        triggerIdPub = nh.advertise < std_msgs::Int32 > ("trigger_id", topic_queue_size, latch_topics);

        //create subscribers
        triggerScanSub = nh.subscribe("/pick_practice/trigger", 10, &RosInterface::triggerScanCallBack, this);

        //set diagnostic Hw id
        diagnosticUpdater.setHardwareID("none");
        diagnosticUpdater.add(PhoXi3DscannerDiagnosticTask);
        diagnosticTimer = nh.createTimer(ros::Duration(5.0), &RosInterface::diagnosticTimerCallback, this);
        diagnosticTimer.start();

        nh.param<std::string>("frame_id", frameId, "PhoXi3Dscanner_sensor");
        nh.param<bool>("send_aligned_depth_map", send_aligned_depth_map, false);
        nh.param<bool>("preprocess", preprocess, false);
        
	RosInterface::getDepthMapSetting();

	//set phoxi camera intrinsic parameter
	std::vector<double> cameraMatrix_array;
	std::vector<double> distCoeffs_array;
	nh.getParam("camera_matrix/data", cameraMatrix_array);
	nh.getParam("distortion_coefficients/data", distCoeffs_array);
	cameraMatrix = cv::Mat(3, 3, CV_64FC1, cameraMatrix_array.data()).clone();
	distCoeffs = cv::Mat(1, 5, CV_64FC1, distCoeffs_array.data()).clone();
        
	//connect to default scanner
        std::string scannerId;
        if (nh.param<std::string>("scanner_id", scannerId, "") && !scannerId.empty()) {
            try {
                RosInterface::connectCamera(scannerId);
                ROS_INFO("Connected to %s", scannerId.c_str());
            } catch (PhoXiInterfaceException& e) {
                ROS_WARN("Connection to default scanner %s failed. %s ", scannerId.c_str(), e.what());
            }
        }
        if (!PhoXiInterface::isConnected()) {
            getDefaultDynamicReconfigureConfig(dynamicReconfigureConfig);
        }
        //set dynamic reconfigure callback
        dynamicReconfigureServer.setCallback(boost::bind(&RosInterface::dynamicReconfigureCallback, this, _1, _2));
    }

    bool
    RosInterface::getDeviceList(phoxi_camera::GetDeviceList::Request& req, phoxi_camera::GetDeviceList::Response& res) {
        try {
            res.out = PhoXiInterface::cameraList();
            phoXiDeviceInforamtionToRosMsg(PhoXiInterface::deviceList(), res.device_information_list);
            res.len = res.out.size();
            res.success = true;
            res.message = OKRESPONSE;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool
    RosInterface::connectCamera(phoxi_camera::ConnectCamera::Request& req, phoxi_camera::ConnectCamera::Response& res) {
        try {
            RosInterface::connectCamera(req.name);
            res.success = true;
            res.message = OKRESPONSE;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool RosInterface::isConnected(phoxi_camera::IsConnected::Request& req, phoxi_camera::IsConnected::Response& res) {
        res.connected = PhoXiInterface::isConnected();
        return true;
    }

    bool RosInterface::isAcquiring(phoxi_camera::IsAcquiring::Request& req, phoxi_camera::IsAcquiring::Response& res) {
        res.is_acquiring = PhoXiInterface::isAcquiring();
        return true;
    }

    bool RosInterface::isConnected(phoxi_camera::GetBool::Request& req, phoxi_camera::GetBool::Response& res) {
        res.value = PhoXiInterface::isConnected();
        res.message = OKRESPONSE; //todo tot este premysliet
        res.success = true;
        return true;
    }

    bool RosInterface::isAcquiring(phoxi_camera::GetBool::Request& req, phoxi_camera::GetBool::Response& res) {
        res.value = PhoXiInterface::isAcquiring();
        res.message = OKRESPONSE; //todo tot este premysliet
        res.success = true;
        return true;
    }

    bool RosInterface::startAcquisition(std_srvs::Empty::Request& req, std_srvs::Empty::Response& res) {
        try {
            PhoXiInterface::startAcquisition();
            dynamicReconfigureConfig.start_acquisition = true;
            dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
            diagnosticUpdater.force_update();
        } catch (PhoXiInterfaceException& e) {
            ROS_ERROR("%s", e.what());
        }
        return true;
    }

    bool RosInterface::stopAcquisition(std_srvs::Empty::Request& req, std_srvs::Empty::Response& res) {
        try {
            PhoXiInterface::stopAcquisition();
            dynamicReconfigureConfig.start_acquisition = false;
            dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
            diagnosticUpdater.force_update();
        } catch (PhoXiInterfaceException& e) {
            ROS_ERROR("%s", e.what());
        }
        return true;
    }

    bool RosInterface::startAcquisition(phoxi_camera::Empty::Request& req, phoxi_camera::Empty::Response& res) {
        try {
            dynamicReconfigureConfig.start_acquisition = true;
            dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
            PhoXiInterface::startAcquisition();
            res.message = OKRESPONSE;
            res.success = true;
        } catch (PhoXiInterfaceException& e) {
            res.message = e.what();
            res.success = false;
        }
        return true;
    }

    bool RosInterface::stopAcquisition(phoxi_camera::Empty::Request& req, phoxi_camera::Empty::Response& res) {
        try {
            dynamicReconfigureConfig.start_acquisition = false;
            dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
            PhoXiInterface::stopAcquisition();
            res.message = OKRESPONSE;
            res.success = true;
        } catch (PhoXiInterfaceException& e) {
            res.message = e.what();
            res.success = false;
        }
        return true;
    }

    bool
    RosInterface::triggerImage(phoxi_camera::TriggerImage::Request& req, phoxi_camera::TriggerImage::Response& res) {
        try {
            res.id = RosInterface::triggerImage();
            res.success = true;
            res.message = OKRESPONSE;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }

        return true;
    }

    bool RosInterface::getFrame(phoxi_camera::GetFrame::Request &req, phoxi_camera::GetFrame::Response &res){
        try {
            ros::WallTime start_scan_time = ros::WallTime::now();
            pho::api::PFrame frame = getPFrame(req.in);
            ros::WallTime end_scan_time = ros::WallTime::now();
            double scan_time = (end_scan_time - start_scan_time).toNSec() * 1e-9;
            ROS_INFO_STREAM("Frame Getting Time(s): " << scan_time);
    
    	    //ros::WallTime start_build_msg_time = ros::WallTime::now();
            if (send_aligned_depth_map) publishAlignedDepthMap(frame);
            publishFrame(frame);
    	    ros::WallTime end_build_msg_time = ros::WallTime::now();
            //double build_msg_time = (end_build_msg_time - start_build_msg_time).toNSec() * 1e-9;
            //ROS_INFO_STREAM("Building All Msg Time(s): " << build_msg_time);
    
    	    double total_time = (end_build_msg_time - start_scan_time).toNSec() * 1e-9;
    	    ROS_INFO_STREAM("Total Time(s): " << total_time);
    	    std::cout << std::endl;
    
            if(!frame){
                res.success = false;
                res.message = "Null frame!";
            }
            else{
                res.success = true;
                res.message = OKRESPONSE;
            }
        }catch (PhoXiInterfaceException &e){
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool RosInterface::getAlignedDepthMap(phoxi_camera::GetAlignedDepthMap::Request &req, phoxi_camera::GetAlignedDepthMap::Response &res){
        try {
            pho::api::PFrame frame = getPFrame(req.in);
            publishAlignedDepthMap(frame);
            if(!frame){
                res.success = false;
                res.message = "Null frame!";
            } else {
                res.success = true;
                res.message = OKRESPONSE;
            }
        } catch (PhoXiInterfaceException &e){
            res.success = false;
            res.message = e.what();
        }
        return true;
    }    

    bool RosInterface::saveFrame(phoxi_camera::SaveFrame::Request& req, phoxi_camera::SaveFrame::Response& res) {
        try {
            pho::api::PFrame frame = RosInterface::getPFrame(req.in);
            if (!frame) {
                res.success = false;
                res.message = "Null frame!";
                return true;
            }
            size_t pos = req.path.find("~");
            if (pos != std::string::npos) {
                char* home = std::getenv("HOME");
                if (!home) {
                    res.message = "'~' found in 'path' parameter but environment variable 'HOME' not found. Export' HOME' variable or pass absolute value to 'path' parameter.";
                    res.success = false;
                    return true;
                }
                req.path.replace(pos, 1, home);
            }
            ROS_INFO("path: %s", req.path.c_str());
            frame->SaveAsPly(req.path);
            res.message = OKRESPONSE;
            res.success = true;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool RosInterface::disconnectCamera(std_srvs::Empty::Request& req, std_srvs::Empty::Response& res) {
        try {
            PhoXiInterface::disconnectCamera();
            diagnosticUpdater.force_update();
        } catch (PhoXiInterfaceException& e) {
            //scanner is already disconnected on exception
        }
        return true;
    }

    bool RosInterface::getHardwareIdentification(phoxi_camera::GetHardwareIdentification::Request& req,
                                                 phoxi_camera::GetHardwareIdentification::Response& res) {
        try {
            res.hardware_identification = PhoXiInterface::getHardwareIdentification();
            res.success = true;
            res.message = OKRESPONSE;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool RosInterface::getSupportedCapturingModes(phoxi_camera::GetSupportedCapturingModes::Request& req,
                                                  phoxi_camera::GetSupportedCapturingModes::Response& res) {
        try {
            std::vector<pho::api::PhoXiCapturingMode> modes = PhoXiInterface::getSupportedCapturingModes();
            for (int i = 0; i < modes.size(); i++) {
                phoxi_camera::PhoXiSize size;
                size.Height = modes[i].Resolution.Height;
                size.Width = modes[i].Resolution.Width;
                res.supported_capturing_modes.push_back(size);
            }
            res.success = true;
            res.message = OKRESPONSE;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool RosInterface::getApiVersion(phoxi_camera::GetString::Request& req, phoxi_camera::GetString::Response& res) {
        res.value = PhoXiInterface::getApiVersion();
        res.success = true;
        return true;
    }

    bool
    RosInterface::getFirmwareVersion(phoxi_camera::GetString::Request& req, phoxi_camera::GetString::Response& res) {
        try {
            auto dl = PhoXiInterface::deviceList();
            auto it = std::find(dl.begin(), dl.end(), PhoXiInterface::getHardwareIdentification());
            if (it != dl.end()) {
                res.value = it->firmwareVersion;
            }
        }
        catch (phoxi_camera::PhoXiInterfaceException& e) {
            res.message = e.what();
            res.success = false;
        }
        res.success = true;
        return true;
    }
    
    void RosInterface::triggerScanCallBack(const std_msgs::Int32::ConstPtr& msg)
    {
        ROS_INFO_STREAM("Subscribe Trigger! TriggerID: " << (int)msg->data);
        try {
            ros::WallTime start_scan_time = ros::WallTime::now();
            pho::api::PFrame frame = getPFrame(-1);
            ros::WallTime end_scan_time = ros::WallTime::now();
            double scan_time = (end_scan_time - start_scan_time).toNSec() * 1e-9;
            ROS_INFO_STREAM("Frame Getting Time(s): " << scan_time);
    
    	    ros::WallTime start_build_msg_time = ros::WallTime::now();
            if (send_aligned_depth_map) publishAlignedDepthMap(frame);
            publishFrame(frame);
    	    ros::WallTime end_build_msg_time = ros::WallTime::now();
            double build_msg_time = (end_build_msg_time - start_build_msg_time).toNSec() * 1e-9;
            ROS_INFO_STREAM("Building All Msg Time(s): " << build_msg_time);
            
	    std_msgs::Int32 trigger_id;
            trigger_id.data = msg->data;
            triggerIdPub.publish(trigger_id);
    
    	    double total_time = (end_build_msg_time - start_scan_time).toNSec() * 1e-9;
    	    ROS_INFO_STREAM("Total Time(s): " << total_time);
    
        }catch (PhoXiInterfaceException &e){
            std::cout << "Error Acquiring Data: " << e.what() << std::endl;
        }
        
    }

    void RosInterface::publishFrame(pho::api::PFrame frame) {
        if (!frame) {
            ROS_WARN("NUll frame!");
            return;
        }
        
        ros::Time timeNow = ros::Time::now();

        std_msgs::Header header;
        header.stamp = timeNow;
        header.frame_id = frameId;
        header.seq = frame->Info.FrameIndex;

        if (frame->PointCloud.Empty()) {
            ROS_WARN("Empty point cloud!");
        } else {
            ros::WallTime start_pointcloud_time = ros::WallTime::now();
            //auto cloud = PhoXiInterface::getPointCloudFromFrame(frame, dynamicReconfigureConfig.organized_cloud);
            
	    auto cloud_normal_raw = PhoXiInterface::getPointCloudFromFrame(frame, dynamicReconfigureConfig.organized_cloud);
	    sensor_msgs::PointCloud2 output_cloud;
	    if (preprocess) {
	        pcl::copyPointCloud(*cloud_normal_raw, *cloud_normal_preprocessed);
	        preprocessPointCloud();
                pcl::toROSMsg(*cloud_normal_preprocessed, output_cloud);
	    }
	    else {
	        pcl::toROSMsg(*cloud_normal_raw, output_cloud);
	    }
            output_cloud.header = header;
            cloudPub.publish(output_cloud);
            ros::WallTime end_pointcloud_time = ros::WallTime::now();
            double pointcloud_time = (end_pointcloud_time - start_pointcloud_time).toNSec() * 1e-9;
            ROS_INFO_STREAM("Building Point Cloud Msg Time(s): " << pointcloud_time);
        }

	
        if (frame->DepthMap.Empty()) {
            ROS_WARN("Empty depth map!");
        } else {
            sensor_msgs::Image depth_map;
            depth_map.header = header;
            depth_map.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
            sensor_msgs::fillImage(depth_map,
                                   sensor_msgs::image_encodings::TYPE_32FC1,
                                   frame->DepthMap.Size.Height, // height
                                   frame->DepthMap.Size.Width, // width
                                   frame->DepthMap.Size.Width * sizeof(float), // stepSize
                                   frame->DepthMap.operator[](0));
            depthMapPub.publish(depth_map);
	
	    cv::Mat depth_rect;
	    cv::Mat map_x, map_y;
	    cv::Size imageSize(frame->DepthMap.Size.Width, frame->DepthMap.Size.Height);
	    cv::Mat distorted_depth(imageSize, CV_32FC1, frame->DepthMap.operator[](0));
	    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix, imageSize, CV_32FC1, map_x, map_y);
	    cv::remap(distorted_depth, depth_rect, map_x, map_y, CV_INTER_NN);
            cv_bridge::CvImage depth_map_rect(header, sensor_msgs::image_encodings::TYPE_32FC1, depth_rect);
	    depthMapRectPub.publish(depth_map_rect);

        }

        if (frame->Texture.Empty()) {
            ROS_WARN("Empty texture!");
        } else {
            sensor_msgs::Image texture;
            texture.header = header;
            texture.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
            sensor_msgs::fillImage(texture, sensor_msgs::image_encodings::TYPE_32FC1,
                                   frame->Texture.Size.Height, // height
                                   frame->Texture.Size.Width, // width
                                   frame->Texture.Size.Width * sizeof(float), // stepSize
                                   frame->Texture.operator[](0));
            rawTexturePub.publish(texture);

	    cv::Mat rectified_rawtexture;
	    cv::Mat map_x, map_y;
	    cv::Size imageSize(frame->Texture.Size.Width, frame->Texture.Size.Height);
	    cv::Mat distorted_rawtexture(imageSize, CV_32FC1, frame->Texture.operator[](0));
	    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix, imageSize, CV_32FC1, map_x, map_y);
	    cv::remap(distorted_rawtexture, rectified_rawtexture, map_x, map_y, CV_INTER_NN);
            cv_bridge::CvImage rawtexture_rect(header, sensor_msgs::image_encodings::TYPE_32FC1, rectified_rawtexture);
	    rawTextureRectPub.publish(rawtexture_rect);

            cv::Mat cvGreyTexture(frame->Texture.Size.Height, frame->Texture.Size.Width, CV_32FC1,
                                  frame->Texture.operator[](0));
            cv::normalize(cvGreyTexture, cvGreyTexture, 0, 255, CV_MINMAX);
            cvGreyTexture.convertTo(cvGreyTexture, CV_8U);
            cv::equalizeHist(cvGreyTexture, cvGreyTexture);
            cv::Mat cvRgbTexture;
            cv::cvtColor(cvGreyTexture, cvRgbTexture, CV_GRAY2RGB);
            cv_bridge::CvImage rgbTexture(header, sensor_msgs::image_encodings::RGB8, cvRgbTexture);
            rgbTexturePub.publish(rgbTexture.toImageMsg());
	    
	    cv::Mat rectified_rgbtexture;
	    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix, imageSize, CV_32FC1, map_x, map_y);
	    cv::remap(cvRgbTexture, rectified_rgbtexture, map_x, map_y, CV_INTER_NN);
            cv_bridge::CvImage rgbtexture_rect(header, sensor_msgs::image_encodings::RGB8, rectified_rgbtexture);
	    rgbTextureRectPub.publish(rgbtexture_rect);
        }
        if (frame->ConfidenceMap.Empty()) {
            ROS_WARN("Empty confidence map!");
        } else {
            sensor_msgs::Image confidence_map;
            confidence_map.header = header;
            confidence_map.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
            sensor_msgs::fillImage(confidence_map,
                                   sensor_msgs::image_encodings::TYPE_32FC1,
                                   frame->ConfidenceMap.Size.Height, // height
                                   frame->ConfidenceMap.Size.Width, // width
                                   frame->ConfidenceMap.Size.Width * sizeof(float), // stepSize
                                   frame->ConfidenceMap.operator[](0));
            confidenceMapPub.publish(confidence_map);
	    
	    cv::Mat confidence_rect;
	    cv::Mat map_x, map_y;
	    cv::Size imageSize(frame->ConfidenceMap.Size.Width, frame->ConfidenceMap.Size.Height);
	    cv::Mat distorted_confidence(imageSize, CV_32FC1, frame->ConfidenceMap.operator[](0));
	    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix, imageSize, CV_32FC1, map_x, map_y);
	    cv::remap(distorted_confidence, confidence_rect, map_x, map_y, CV_INTER_NN);
            cv_bridge::CvImage confidence_map_rect(header, sensor_msgs::image_encodings::TYPE_32FC1, confidence_rect);
	    confidenceMapRectPub.publish(confidence_map_rect);
        }

        if (frame->NormalMap.Empty()) {
            ROS_WARN("Empty normal map!");
        } else {
            sensor_msgs::Image normal_map;
            normal_map.header = header;
            normal_map.encoding = sensor_msgs::image_encodings::TYPE_32FC3;
            sensor_msgs::fillImage(normal_map,
                                   sensor_msgs::image_encodings::TYPE_32FC3,
                                   frame->NormalMap.Size.Height, // height
                                   frame->NormalMap.Size.Width, // width
                                   frame->NormalMap.Size.Width * sizeof(float) * 3, // stepSize
                                   frame->NormalMap.operator[](0));
            normalMapPub.publish(normal_map);
	    
	    cv::Mat normal_rect;
	    cv::Mat map_x, map_y;
	    cv::Size imageSize(frame->NormalMap.Size.Width, frame->NormalMap.Size.Height);
	    cv::Mat distorted_normal(imageSize, CV_32FC3, frame->NormalMap.operator[](0));
	    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix, imageSize, CV_32FC1, map_x, map_y);
	    cv::remap(distorted_normal, normal_rect, map_x, map_y, CV_INTER_NN);
	    cv_bridge::CvImage normal_map_rect(header, sensor_msgs::image_encodings::TYPE_32FC3, normal_rect);
	    normalMapRectPub.publish(normal_map_rect);
        }

    }

    void RosInterface::publishAlignedDepthMap(pho::api::PFrame frame) {
        if (!frame) {
            ROS_WARN("NUll frame!");
            return;
        }
    
        ros::WallTime start_aligning_time = ros::WallTime::now();
        if (frame->DepthMap.Empty()){
            ROS_WARN("Empty depth map!");
        }
    
        //if (DepthMapSetting.flag == 0) getDepthMapSetting();
        getExternalCameraFrame();
    
        pho::api::AdditionalCamera::Aligner Aligner(scanner, DepthMapSetting.Calibration);
        if (!(Aligner.GetAlignedDepthMap(DepthMapSetting.DepthMap)))
        {
            ROS_ERROR("Computation of aligned depth map was NOT successful!");
        }
    
	sensor_msgs::Image aligned_depth_map;
	aligned_depth_map.header.frame_id = external_camera_header.frame_id;
	aligned_depth_map.header.stamp = ros::Time::now();
	aligned_depth_map.header.seq = frame->Info.FrameIndex;
	aligned_depth_map.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
	sensor_msgs::fillImage(aligned_depth_map,
			    sensor_msgs::image_encodings::TYPE_32FC1,
			    DepthMapSetting.DepthMap.Size.Height, // height
			    DepthMapSetting.DepthMap.Size.Width, // width
			    DepthMapSetting.DepthMap.Size.Width * sizeof(float), // stepSize
			    DepthMapSetting.DepthMap.operator[](0));
        
	std_msgs::Header header;
	header.frame_id = external_camera_header.frame_id;
	header.stamp = ros::Time::now();
	header.seq = frame->Info.FrameIndex;
	cv::Mat depth_rect;
	cv::Mat distorted_depth(DepthMapSetting.DepthMap.Size.Height, DepthMapSetting.DepthMap.Size.Width, CV_32FC1, DepthMapSetting.DepthMap.operator[](0));
	cv::remap(distorted_depth, depth_rect, map_x, map_y, CV_INTER_NN);
        cv_bridge::CvImage aligned_depth_map_rect(header, sensor_msgs::image_encodings::TYPE_32FC1, depth_rect);

        cv_bridge::CvImage external_camera_texture(external_camera_header, sensor_msgs::image_encodings::BGR8, ex_img);
    
        alignedDepthMapPub.publish(aligned_depth_map);
	alignedDepthMapRectPub.publish(aligned_depth_map_rect);
        externalCameraTexturePub.publish(external_camera_texture);
    	
        ros::WallTime end_aligning_time = ros::WallTime::now();
        double aligning_time = (end_aligning_time - start_aligning_time).toNSec() * 1e-9;
        ROS_INFO_STREAM("Building Aligned Depth Map Msg Time(s): " << aligning_time);
    }

    void RosInterface::dynamicReconfigureCallback(phoxi_camera::phoxi_cameraConfig& config, uint32_t level) {
        if (!PhoXiInterface::isConnected()) {
            config = this->dynamicReconfigureConfig;
            ROS_WARN("Node is not connected to scanner");
            return;
        }
        try {
            if (level & (1 << 1)) {
                switch (config.resolution) {
                    case 0:
                        PhoXiInterface::setLowResolution();
                        break;
                    case 1:
                        PhoXiInterface::setHighResolution();
                        break;
                    default:
                        ROS_WARN("Resolution not supported!");
                        break;
                }
                this->dynamicReconfigureConfig.resolution = config.resolution;
            }

            if (level & (1 << 2)) {
                this->isOk();
                scanner->CapturingSettings->ScanMultiplier = config.scan_multiplier;
                this->dynamicReconfigureConfig.scan_multiplier = config.scan_multiplier;
            }

            if (level & (1 << 3)) {
                this->isOk();
                scanner->CapturingSettings->ShutterMultiplier = config.shutter_multiplier;
                this->dynamicReconfigureConfig.shutter_multiplier = config.shutter_multiplier;
            }

            if (level & (1 << 4)) {
                PhoXiInterface::setTriggerMode(config.trigger_mode, config.start_acquisition);
                this->dynamicReconfigureConfig.trigger_mode = config.trigger_mode;
                this->dynamicReconfigureConfig.start_acquisition = config.start_acquisition;
            }

            if (level & (1 << 5)) {
                this->isOk();
                scanner->Timeout = config.timeout;
                this->dynamicReconfigureConfig.timeout = config.timeout;
            }

            if (level & (1 << 6)) {
                this->isOk();
                scanner->ProcessingSettings->Confidence = config.confidence;
                this->dynamicReconfigureConfig.confidence = config.confidence;
            }

            if (level & (1 << 7)) {
                this->isOk();
                scanner->OutputSettings->SendPointCloud = config.send_point_cloud;
                this->dynamicReconfigureConfig.send_point_cloud = config.send_point_cloud;
            }

            if (level & (1 << 8)) {
                this->isOk();
                scanner->OutputSettings->SendNormalMap = config.send_normal_map;
                this->dynamicReconfigureConfig.send_normal_map = config.send_normal_map;
            }

            if (level & (1 << 9)) {
                this->isOk();
                scanner->OutputSettings->SendConfidenceMap = config.send_confidence_map;
                this->dynamicReconfigureConfig.send_confidence_map = config.send_confidence_map;
            }

            if (level & (1 << 10)) {
                this->isOk();
                scanner->OutputSettings->SendTexture = config.send_texture;
                this->dynamicReconfigureConfig.send_texture = config.send_texture;
            }

            if (level & (1 << 11)) {
                this->isOk();
                scanner->OutputSettings->SendDepthMap = config.send_depth_map;
                this->dynamicReconfigureConfig.send_depth_map = config.send_depth_map;
            }

#ifndef PHOXI_API_v1_1
            if (level & (1 << 12)) {
                this->isOk();
                PhoXiInterface::setCoordinateSpace(config.coordinate_space);
                this->dynamicReconfigureConfig.coordinate_space = config.coordinate_space;
            }
#endif
            if (level & (1 << 13)) {
                this->dynamicReconfigureConfig.organized_cloud = config.organized_cloud;
            }

#ifndef PHOXI_API_v1_1
            if (level & (1 << 14)) {
                this->isOk();
                scanner->CapturingSettings->AmbientLightSuppression = config.ambient_light_suppression;
                this->dynamicReconfigureConfig.ambient_light_suppression = config.ambient_light_suppression;
            }

            if (level & (1 << 15)) {
                this->isOk();
                std::vector<double> supportedSPE = scanner->SupportedSinglePatternExposures;
                if (!supportedSPE.empty()) {    // ignore setting if setting is not supported
                    scanner->CapturingSettings->SinglePatternExposure = supportedSPE.at(config.single_pattern_exposure);
                    this->dynamicReconfigureConfig.single_pattern_exposure = config.single_pattern_exposure;
                } else {
                    ROS_WARN("Scanner setting 'Single pattern exposure' is not supported by the scanner firmware.");
                }
            }

            if (level & (1 << 16)) {
                this->isOk();
                scanner->CapturingSettings->CameraOnlyMode = config.camera_only_mode;
                this->dynamicReconfigureConfig.camera_only_mode = config.camera_only_mode;
            }
#endif

        } catch (PhoXiInterfaceException& e) {
            ROS_WARN("%s", e.what());
        }
    }

    pho::api::PFrame RosInterface::getPFrame(int id) {
        pho::api::PFrame frame = PhoXiInterface::getPFrame(id);
        //update dynamic reconfigure
        dynamicReconfigureConfig.trigger_mode = PhoXiInterface::scanner->TriggerMode.GetValue();
        dynamicReconfigureConfig.start_acquisition = PhoXiInterface::scanner->isAcquiring();
        dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
        return frame;
    }

    int RosInterface::triggerImage() {
        int id = PhoXiInterface::triggerImage(false);
        //update dynamic reconfigure
        dynamicReconfigureConfig.coordinate_space = pho::api::PhoXiTriggerMode::Software;
        dynamicReconfigureConfig.start_acquisition = true;
        dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
        return id;
    }

    void
    RosInterface::connectCamera(std::string HWIdentification, pho::api::PhoXiTriggerMode mode, bool startAcquisition) {
        PhoXiInterface::connectCamera(HWIdentification, mode, startAcquisition);
        bool initFromConfig = false;
        nh.getParam("init_from_config", initFromConfig);
        if (initFromConfig) {
            getDefaultDynamicReconfigureConfig(dynamicReconfigureConfig);
            this->dynamicReconfigureCallback(dynamicReconfigureConfig, std::numeric_limits<uint32_t>::max());
        } else {
            initFromPhoXi();
        }
        dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
        diagnosticUpdater.force_update();
    }

    void RosInterface::diagnosticCallback(diagnostic_updater::DiagnosticStatusWrapper& status) {
        if (PhoXiInterface::isConnected()) {
            if (PhoXiInterface::isAcquiring()) {
                status.summary(diagnostic_msgs::DiagnosticStatus::OK, "Ready");
            } else {
                status.summary(diagnostic_msgs::DiagnosticStatus::WARN, "Acquisition not started");
            }
            status.add("HardwareIdentification", std::string(scanner->HardwareIdentification));
            status.add("Trigger mode", getTriggerMode(scanner->TriggerMode));

        } else {
            status.summary(diagnostic_msgs::DiagnosticStatus::ERROR, "Not connected");
        }
    }

    void RosInterface::diagnosticTimerCallback(const ros::TimerEvent&) {
        diagnosticUpdater.force_update();
    }

    std::string RosInterface::getTriggerMode(pho::api::PhoXiTriggerMode mode) {
        switch (mode) {
            case pho::api::PhoXiTriggerMode::Freerun:
                return "Freerun";
            case pho::api::PhoXiTriggerMode::Software:
                return "Software";
            case pho::api::PhoXiTriggerMode::Hardware:
                return "Hardware";
            case pho::api::PhoXiTriggerMode::NoValue:
                return "NoValue";
            default:
                return "Undefined";
        }
    }

    void RosInterface::initFromPhoXi() {
        getDefaultDynamicReconfigureConfig(dynamicReconfigureConfig);
        if (!scanner->isConnected()) {
            ROS_WARN("Scanner not connected.");
            return;
        }
        ///resolution
        pho::api::PhoXiCapturingMode mode = scanner->CapturingMode;
        if ((mode.Resolution.Width == 2064) && (mode.Resolution.Height = 1544)) {
            this->dynamicReconfigureConfig.resolution = 1;
        } else {
            this->dynamicReconfigureConfig.resolution = 0;
        }

        pho::api::PhoXiCapturingSettings capturingSettings;
        capturingSettings = scanner->CapturingSettings;
        this->dynamicReconfigureConfig.scan_multiplier = capturingSettings.ScanMultiplier;
        this->dynamicReconfigureConfig.shutter_multiplier = capturingSettings.ShutterMultiplier;
        this->dynamicReconfigureConfig.confidence = scanner->ProcessingSettings->Confidence;

        pho::api::FrameOutputSettings outputSettings;
        outputSettings = scanner->OutputSettings;
        this->dynamicReconfigureConfig.send_point_cloud = outputSettings.SendPointCloud;
        this->dynamicReconfigureConfig.send_normal_map = outputSettings.SendNormalMap;
        this->dynamicReconfigureConfig.send_confidence_map = outputSettings.SendConfidenceMap;
        this->dynamicReconfigureConfig.send_depth_map = outputSettings.SendDepthMap;
        this->dynamicReconfigureConfig.send_texture = outputSettings.SendTexture;
#ifndef PHOXI_API_v1_1
        this->dynamicReconfigureConfig.coordinate_space = scanner->CoordinatesSettings->CoordinateSpace;
        this->dynamicReconfigureConfig.ambient_light_suppression = capturingSettings.AmbientLightSuppression;

        std::vector<double> supportedSPE = scanner->SupportedSinglePatternExposures;
        if (!supportedSPE.empty()) {
            auto actualParam_it = std::find(supportedSPE.begin(), supportedSPE.end(), capturingSettings.SinglePatternExposure);
            if (actualParam_it != supportedSPE.end()) {
                this->dynamicReconfigureConfig.single_pattern_exposure = actualParam_it - supportedSPE.begin();
            } else {
                int singlePatternExposure_index;
                nh.getParam("single_pattern_exposure", singlePatternExposure_index);
                this->dynamicReconfigureConfig.single_pattern_exposure = singlePatternExposure_index;
                ROS_WARN("Can not update Single Pattern Exposure parameter in dynamic reconfigure, set default value from config.");
            }
        } else {
            ROS_WARN("Scanner setting 'Single pattern exposure' is not supported by the scanner firmware.");
        }

        this->dynamicReconfigureConfig.camera_only_mode = capturingSettings.CameraOnlyMode;
#endif
        this->dynamicReconfigureConfig.trigger_mode = scanner->TriggerMode.GetValue();
        this->dynamicReconfigureConfig.start_acquisition = scanner->isAcquiring();
        this->dynamicReconfigureConfig.timeout = scanner->Timeout.GetValue();
    }

#ifndef PHOXI_API_v1_1

    bool RosInterface::setCoordianteSpace(phoxi_camera::SetCoordinatesSpace::Request& req,
                                          phoxi_camera::SetCoordinatesSpace::Response& res) {
        try {
            PhoXiInterface::setCoordinateSpace(req.coordinates_space);
            //update dynamic reconfigure
            dynamicReconfigureConfig.coordinate_space = req.coordinates_space;
            dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
            res.success = true;
            res.message = OKRESPONSE;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool RosInterface::setTransformation(phoxi_camera::SetTransformationMatrix::Request& req,
                                         phoxi_camera::SetTransformationMatrix::Response& res) {
        try {
            Eigen::Affine3d transform;
            tf::transformMsgToEigen(req.transform, transform);
            PhoXiInterface::setTransformation(transform.matrix(), req.coordinates_space, req.set_space,
                                              req.save_settings);
            //update dynamic reconfigure
            if (req.set_space) {
                dynamicReconfigureConfig.coordinate_space = req.coordinates_space;
            }
            dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
            res.success = true;
            res.message = OKRESPONSE;
        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }
        return true;
    }

    bool
    RosInterface::saveLastFrame(phoxi_camera::SaveLastFrame::Request& req, phoxi_camera::SaveLastFrame::Response& res) {
        std::string file_path = req.file_path;

        try {
            // ~ error handling
            size_t pos = file_path.find('~');
            if (pos != std::string::npos) {
                char* home = std::getenv("HOME");
                if (!home) {
                    res.message = "'~' found in 'file_path' parameter but environment variable 'HOME' not found. Export' HOME' variable or pass absolute value to 'path' parameter.";
                    res.success = false;
                    return true;
                }
                file_path.replace(pos, 1, home);
            }

            // extension error handling
            const std::string extensions[] = {"praw", "ply", "ptx", "tif", "prawf"};
            bool correct_ext = false;
            pos = file_path.rfind('.') + 1;
            std::string extension = file_path.substr(pos, file_path.length());

            for (const auto& ext: extensions) {
                if (ext == extension) {
                    correct_ext = true;
                }
            }
            if (!correct_ext) {
                res.message = "Wrong extension.";
                res.success = false;
                return true;
            }

            // save last frame
            ROS_INFO("File path: %s", file_path.c_str());
            bool result = PhoXiInterface::saveLastFrame(file_path);
            if (result) {
                res.message = OKRESPONSE;
                res.success = true;
            } else {
                res.message = "Unsuccessful save.";
                res.success = false;
            }

        } catch (PhoXiInterfaceException& e) {
            res.success = false;
            res.message = e.what();
        }

        return true;
    }

#endif

    void RosInterface::getDefaultDynamicReconfigureConfig(phoxi_camera::phoxi_cameraConfig& config) {
        dynamicReconfigureServer.getConfigDefault(config);
        config.__fromServer__(nh);
    }

    void RosInterface::getDepthMapSetting(){
        std::string proj_path = ros::package::getPath("phoxi_camera");
        std::string calibrationFile = "";
        nh.getParam("external_camera_calibration_file", calibrationFile);
        std::string file_path = proj_path + "/config/" + calibrationFile;
    
        std::ifstream stream(file_path.c_str());
        if (!stream.good()) {
            std::cout << "Missing File" << std::endl;
            // return DepthMapSettingsResult::FileMissing;    
        }
    
        bool CorrectCalibration = true;
        DepthMapSetting.Calibration.LoadFromFile(file_path);
        CorrectCalibration &= DepthMapSetting.Calibration.CalibrationSettings.DistortionCoefficients.size() > 4;
        CorrectCalibration &= DepthMapSetting.Calibration.CameraResolution.Width != 0 && DepthMapSetting.Calibration.CameraResolution.Height != 0;

	cv::Mat cameraMatrix(3, 3, CV_64FC1, DepthMapSetting.Calibration.CalibrationSettings.CameraMatrix.operator[](0));
	cv::Mat distCoeffs(1, 5, CV_64FC1, DepthMapSetting.Calibration.CalibrationSettings.DistortionCoefficients.data());
	cv::Size imageSize(DepthMapSetting.Calibration.CameraResolution.Width, DepthMapSetting.Calibration.CameraResolution.Height);
        cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix, imageSize, CV_32FC1, map_x, map_y);

        if (CorrectCalibration)
        {
            //DepthMapSetting.flag = 1;
            ROS_INFO("Success to Load Calibration file");
        }
        else ROS_ERROR("Error for Depth Map Setting");
    }
    
    void RosInterface::getExternalCameraFrame(){
        std::string externalCameraTopicName = "";
        nh.getParam("external_camera_topic_name", externalCameraTopicName);
        sensor_msgs::ImageConstPtr msg = ros::topic::waitForMessage<sensor_msgs::Image>(externalCameraTopicName);
        cv_bridge::CvImagePtr cv_ptr;
        cv_ptr = cv_bridge::toCvCopy(msg, "bgr8");
    
        external_camera_header = msg->header;
        ex_img = cv_ptr->image.clone();
    }

    void RosInterface::preprocessPointCloud() {
        // downsampling
	pcl::VoxelGrid<pcl::PointNormal> voxelSampler;
	voxelSampler.setInputCloud(cloud_normal_preprocessed);
	voxelSampler.setLeafSize(0.002, 0.002, 0.002);
	voxelSampler.filter(*cloud_normal_preprocessed);

	// outlier remover (statistical)
	pcl::StatisticalOutlierRemoval<pcl::PointNormal> sor;
	sor.setInputCloud(cloud_normal_preprocessed);
	sor.setMeanK(50);
	sor.setStddevMulThresh(0.4);
	sor.filter(*cloud_normal_preprocessed);

        // outlier remover (radius)
        //pcl::RadiusOutlierRemoval<pcl::PointNormal> outrem;
        //outrem.setInputCloud(cloud_normal);
        //outrem.setRadiusSearch(0.8);
        //outrem.setMinNeighborsInRadius(50);
        //outrem.filter(*cloud_normal);
    }
}


