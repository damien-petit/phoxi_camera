//
// Created by controller on 1/11/18.
//

#include "phoxi_camera/RosInterface.h"
#include <pcl/point_types.h>
#include <pcl_ros/point_cloud.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/fill_image.h>
#include <phoxi_camera/PhoXiException.h>
#include <eigen_conversions/eigen_msg.h>
#include <cv_bridge/cv_bridge.h>

RosInterface::RosInterface() : nh("~"), dynamicReconfigureServer(dynamicReconfigureMutex,nh), PhoXi3DscannerDiagnosticTask("PhoXi3Dscanner",boost::bind(&RosInterface::diagnosticCallback, this, _1)) {

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
    stopAcquisitionServiceV2 = nh.advertiseService("V2/stop_acquisition", (bool (RosInterface::*)(phoxi_camera::Empty::Request&, phoxi_camera::Empty::Response&))&RosInterface::startAcquisition, this);
    triggerImageService =nh.advertiseService("trigger_image", &RosInterface::triggerImage, this);
    getFrameService = nh.advertiseService("get_frame", &RosInterface::getFrame, this);
    getAlignedDepthMapService = nh.advertiseService("get_aligned_depth_map", &RosInterface::getAlignedDepthMap, this);
    saveFrameService = nh.advertiseService("save_frame", &RosInterface::saveFrame, this);
    disconnectCameraService = nh.advertiseService("disconnect_camera", &RosInterface::disconnectCamera, this);
    getHardwareIdentificationService = nh.advertiseService("get_hardware_indentification", &RosInterface::getHardwareIdentification, this);
    getSupportedCapturingModesService = nh.advertiseService("get_supported_capturing_modes", &RosInterface::getSupportedCapturingModes, this);
    setCoordianteSpaceService = nh.advertiseService("V2/set_transformation",&RosInterface::setTransformation, this);
    setTransformationService = nh.advertiseService("V2/set_coordination_space",&RosInterface::setCoordianteSpace, this);

    //create publishers
    bool latch_tipics;
    int topic_queue_size;
    nh.param<bool>("latch_tipics", latch_tipics, false);
    nh.param<int>("topic_queue_size", topic_queue_size, 1);
    cloudPub = nh.advertise <pcl::PointCloud<pcl::PointXYZ >>("pointcloud", 1,latch_tipics);
    normalMapPub = nh.advertise < sensor_msgs::Image > ("normal_map", topic_queue_size,latch_tipics);
    confidenceMapPub = nh.advertise < sensor_msgs::Image > ("confidence_map", topic_queue_size,latch_tipics);
    rawTexturePub = nh.advertise < sensor_msgs::Image > ("texture", topic_queue_size,latch_tipics);
    rgbTexturePub = nh.advertise < sensor_msgs::Image > ("rgb_texture", topic_queue_size,latch_tipics);
    depthMapPub = nh.advertise < sensor_msgs::Image > ("depth_map", topic_queue_size,latch_tipics);
    alignedDepthMapPub = nh.advertise < sensor_msgs::Image > ("aligned_depth_map", topic_queue_size,latch_tipics);
    externalCameraTexturePub = nh.advertise < sensor_msgs::Image > ("external_camera_texture", topic_queue_size,latch_tipics);
    triggerIdPub = nh.advertise < std_msgs::Int32 > ("trigger_id", topic_queue_size, latch_tipics);

    //create subscribers
    triggerScanSub = nh.subscribe("/pick_practice/trigger", 10, &RosInterface::triggerScanCallBack, this);

    //set dynamic reconfigure callback
    dynamicReconfigureServer.setCallback(boost::bind(&RosInterface::dynamicReconfigureCallback,this, _1, _2));

    //set diagnostic Hw id
    diagnosticUpdater.setHardwareID("none");
    diagnosticUpdater.add(PhoXi3DscannerDiagnosticTask);
    diagnosticTimer  = nh.createTimer(ros::Duration(5.0),&RosInterface::diagnosticTimerCallback, this);
    diagnosticTimer.start();

    //connect to default scanner
    std::string scannerId;
    nh.param<std::string>("scanner_id", scannerId, "InstalledExamples-PhoXi-example");
    nh.param<std::string>("frame_id", frameId, "PhoXi3Dscanner_sensor");
    nh.param<bool>("send_aligned_depth_map", send_aligned_depth_map, false);

    try {
        RosInterface::connectCamera(scannerId);
        ROS_INFO("Connected to %s",scannerId.c_str());
    }catch(PhoXiInterfaceException& e) {
        ROS_WARN("Connection to default scanner %s failed. %s ",scannerId.c_str(),e.what());
    }

}

bool RosInterface::getDeviceList(phoxi_camera::GetDeviceList::Request &req, phoxi_camera::GetDeviceList::Response &res){
    try {
        res.out = PhoXiInterface::cameraList();
        res.len = res.out.size();
        res.success = true;
        res.message = OKRESPONSE;
    }catch (PhoXiInterfaceException &e){
        res.success = false;
        res.message = e.what();
    }
    return true;
}
bool RosInterface::connectCamera(phoxi_camera::ConnectCamera::Request &req, phoxi_camera::ConnectCamera::Response &res){
    try {
        RosInterface::connectCamera(req.name);
        res.success = true;
        res.message = OKRESPONSE;
    }catch (PhoXiInterfaceException &e){
        res.success = false;
        res.message = e.what();
    }
    return true;
}
bool RosInterface::isConnected(phoxi_camera::IsConnected::Request &req, phoxi_camera::IsConnected::Response &res){
    res.connected = PhoXiInterface::isConnected();
    return true;
}
bool RosInterface::isAcquiring(phoxi_camera::IsAcquiring::Request &req, phoxi_camera::IsAcquiring::Response &res){
    res.is_acquiring = PhoXiInterface::isAcquiring();
    return true;
}
bool RosInterface::isConnected(phoxi_camera::GetBool::Request &req, phoxi_camera::GetBool::Response &res){
    res.value = PhoXiInterface::isConnected();
    res.message = OKRESPONSE; //todo tot este premysliet
    res.success = true;
    return true;
}
bool RosInterface::isAcquiring(phoxi_camera::GetBool::Request &req, phoxi_camera::GetBool::Response &res){
    res.value = PhoXiInterface::isAcquiring();
    res.message = OKRESPONSE; //todo tot este premysliet
    res.success = true;
    return true;
}
bool RosInterface::startAcquisition(std_srvs::Empty::Request &req, std_srvs::Empty::Response &res){
    try {
        PhoXiInterface::startAcquisition();
        diagnosticUpdater.force_update();
    }catch (PhoXiInterfaceException &e){
        ROS_ERROR("%s",e.what());
    }
    return true;
}
bool RosInterface::stopAcquisition(std_srvs::Empty::Request &req, std_srvs::Empty::Response &res){
    try {
        PhoXiInterface::stopAcquisition();
        diagnosticUpdater.force_update();
    }catch (PhoXiInterfaceException &e){
        ROS_ERROR("%s",e.what());
    }
    return true;
}
bool RosInterface::startAcquisition(phoxi_camera::Empty::Request &req, phoxi_camera::Empty::Response &res){
    try {
        //todo
        PhoXiInterface::startAcquisition();
        res.message = OKRESPONSE;
        res.success = true;
    }catch (PhoXiInterfaceException &e){
        res.message = e.what();
        res.success = false;
    }
    return true;
}
bool RosInterface::stopAcquisition(phoxi_camera::Empty::Request &req, phoxi_camera::Empty::Response &res){
    try {
        PhoXiInterface::stopAcquisition();
        res.message = OKRESPONSE;
        res.success = true;
    }catch (PhoXiInterfaceException &e){
        res.message = e.what();
        res.success = false;
    }
    return true;
}
bool RosInterface::triggerImage(phoxi_camera::TriggerImage::Request &req, phoxi_camera::TriggerImage::Response &res){
    try {
        res.id = RosInterface::triggerImage();
        res.success = true;
        res.message = OKRESPONSE;
    }catch (PhoXiInterfaceException &e){
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
bool RosInterface::saveFrame(phoxi_camera::SaveFrame::Request &req, phoxi_camera::SaveFrame::Response &res){
    try {
        pho::api::PFrame frame = RosInterface::getPFrame(req.in);
        if(!frame){
            res.success = false;
            res.message = "Null frame!";
            return true;
        }
        size_t pos = req.path.find("~");
        if(pos != std::string::npos){
            char* home = std::getenv("HOME");
            if(!home){
                res.message = "'~' found in 'path' parameter but environment variable 'HOME' not found. Export' HOME' variable or pass absolute value to 'path' parameter.";
                res.success = false;
                return true;
            }
            req.path.replace(pos,1,home);
        }
        ROS_INFO("path: %s",req.path.c_str());
        frame->SaveAsPly(req.path);
        res.message = OKRESPONSE;
        res.success = true;
    }catch (PhoXiInterfaceException &e){
        res.success = false;
        res.message = e.what();
    }
    return true;
}
bool RosInterface::disconnectCamera(std_srvs::Empty::Request &req, std_srvs::Empty::Response &res){
    try {
        PhoXiInterface::disconnectCamera();
        diagnosticUpdater.force_update();
    }catch (PhoXiInterfaceException &e){
        //scanner is already disconnected on exception
    }
    return true;
}
bool RosInterface::getHardwareIdentification(phoxi_camera::GetHardwareIdentification::Request &req, phoxi_camera::GetHardwareIdentification::Response &res){
    try {
        res.hardware_identification = PhoXiInterface::getHardwareIdentification();
        res.success = true;
        res.message = OKRESPONSE;
    }catch (PhoXiInterfaceException &e){
        res.success = false;
        res.message = e.what();
    }
    return true;
}
bool RosInterface::getSupportedCapturingModes(phoxi_camera::GetSupportedCapturingModes::Request &req, phoxi_camera::GetSupportedCapturingModes::Response &res){
    try {
        std::vector<pho::api::PhoXiCapturingMode> modes = PhoXiInterface::getSupportedCapturingModes();
        for(int i =0; i < modes.size(); i++){
            phoxi_camera::PhoXiSize size;
            size.Height = modes[i].Resolution.Height;
            size.Width = modes[i].Resolution.Width;
            res.supported_capturing_modes.push_back(size);
        }
        res.success = true;
        res.message = OKRESPONSE;
    }catch (PhoXiInterfaceException &e){
        res.success = false;
        res.message = e.what();
    }
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

        std_msgs::Int32 trigger_id;
        trigger_id.data = msg->data;
        triggerIdPub.publish(trigger_id);

	ros::WallTime start_build_msg_time = ros::WallTime::now();
        if (send_aligned_depth_map) publishAlignedDepthMap(frame);
        publishFrame(frame);
	ros::WallTime end_build_msg_time = ros::WallTime::now();
        double build_msg_time = (end_build_msg_time - start_build_msg_time).toNSec() * 1e-9;
        ROS_INFO_STREAM("Building All Msg Time(s): " << build_msg_time);

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
    if (frame->PointCloud.Empty()){
        ROS_WARN("Empty point cloud!");
    }
    if (frame->DepthMap.Empty()){
        ROS_WARN("Empty depth map!");
    }
    if (frame->Texture.Empty()){
        ROS_WARN("Empty texture!");
    }
    if (frame->ConfidenceMap.Empty()){
        ROS_WARN("Empty confidence map!");
    }
    if (frame->NormalMap.Empty()){
        ROS_WARN("Empty normal map!");
    }
    sensor_msgs::Image texture, confidence_map, normal_map, depth_map;
    ros::Time timeNow = ros::Time::now();

    std_msgs::Header header;
    header.stamp = timeNow;
    header.frame_id = frameId;
    header.seq = frame->Info.FrameIndex;

    texture.header = header;
    confidence_map.header = header;
    normal_map.header = header;
    depth_map.header = header;

    cv::Mat cvGreyTexture(frame->Texture.Size.Height, frame->Texture.Size.Width, CV_32FC1, frame->Texture.operator[](0));
    cv::normalize(cvGreyTexture, cvGreyTexture, 0, 255, CV_MINMAX);
    cvGreyTexture.convertTo(cvGreyTexture,CV_8U);
    cv::equalizeHist(cvGreyTexture, cvGreyTexture);
    cv::Mat cvRgbTexture;
    cv::cvtColor(cvGreyTexture,cvRgbTexture,CV_GRAY2RGB);
    cv_bridge::CvImage rgbTexture(header,sensor_msgs::image_encodings::BGR8,cvRgbTexture);

    texture.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
    sensor_msgs::fillImage(texture, sensor_msgs::image_encodings::TYPE_32FC1,
                           frame->Texture.Size.Height, // height
                           frame->Texture.Size.Width, // width
                           frame->Texture.Size.Width * sizeof(float), // stepSize
                           frame->Texture.operator[](0));
    confidence_map.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
    sensor_msgs::fillImage(confidence_map,
                           sensor_msgs::image_encodings::TYPE_32FC1,
                           frame->ConfidenceMap.Size.Height, // height
                           frame->ConfidenceMap.Size.Width, // width
                           frame->ConfidenceMap.Size.Width * sizeof(float), // stepSize
                           frame->ConfidenceMap.operator[](0));
    normal_map.encoding = sensor_msgs::image_encodings::TYPE_32FC3;
    sensor_msgs::fillImage(normal_map,
                           sensor_msgs::image_encodings::TYPE_32FC3,
                           frame->NormalMap.Size.Height, // height
                           frame->NormalMap.Size.Width, // width
                           frame->NormalMap.Size.Width * sizeof(float) * 3, // stepSize
                           frame->NormalMap.operator[](0));
    depth_map.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
    sensor_msgs::fillImage(depth_map,
                           sensor_msgs::image_encodings::TYPE_32FC1,
                           frame->DepthMap.Size.Height, // height
                           frame->DepthMap.Size.Width, // width
                           frame->DepthMap.Size.Width * sizeof(float), // stepSize
                           frame->DepthMap.operator[](0));

    ros::WallTime start_pointcloud_time = ros::WallTime::now();
    std::shared_ptr<pcl::PointCloud<pcl::PointNormal>> cloud = PhoXiInterface::getPointCloudFromFrame(frame);

    sensor_msgs::PointCloud2 output_cloud;
    pcl::toROSMsg(*cloud,output_cloud);
    output_cloud.header.frame_id = frameId;
    output_cloud.header.stamp = timeNow;
    output_cloud.header.seq = frame->Info.FrameIndex;
    
    ros::WallTime end_pointcloud_time = ros::WallTime::now();
    double pointcloud_time = (end_pointcloud_time - start_pointcloud_time).toNSec() * 1e-9;
    ROS_INFO_STREAM("Building Point Cloud Msg Time(s): " << pointcloud_time);

    cloudPub.publish(output_cloud);
    normalMapPub.publish(normal_map);
    confidenceMapPub.publish(confidence_map);
    rawTexturePub.publish(texture);
    rgbTexturePub.publish(rgbTexture.toImageMsg());
    depthMapPub.publish(depth_map);
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

    if (DepthMapSetting.flag == 0) getDepthMapSetting();
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

    cv_bridge::CvImage external_camera_texture(external_camera_header, sensor_msgs::image_encodings::BGR8, ex_img);

    alignedDepthMapPub.publish(aligned_depth_map);
    externalCameraTexturePub.publish(external_camera_texture);
	
    ros::WallTime end_aligning_time = ros::WallTime::now();
    double aligning_time = (end_aligning_time - start_aligning_time).toNSec() * 1e-9;
    ROS_INFO_STREAM("Building Aligned Depth Map Msg Time(s): " << aligning_time);
}

bool RosInterface::setCoordianteSpace(phoxi_camera::SetCoordinatesSpace::Request &req, phoxi_camera::SetCoordinatesSpace::Response &res){
    try {
        PhoXiInterface::setCoordinateSpace(req.coordinates_space);
        //update dynamic reconfigure
        dynamicReconfigureConfig.coordination_space = req.coordinates_space;
        dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
        res.success = true;
        res.message = OKRESPONSE;
    }catch (PhoXiInterfaceException &e){
        res.success = false;
        res.message = e.what();
    }
    return true;
}

bool RosInterface::setTransformation(phoxi_camera::SetTransformationMatrix::Request &req, phoxi_camera::SetTransformationMatrix::Response &res){
    try {
        Eigen::Affine3d transform;
        tf::transformMsgToEigen(req.transform,transform);
        PhoXiInterface::setTransformation(transform.matrix(),req.coordinates_space,req.set_space,req.save_settings);
        //update dynamic reconfigure
        dynamicReconfigureConfig.coordination_space = req.coordinates_space;
        dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
        res.success = true;
        res.message = OKRESPONSE;
    }catch (PhoXiInterfaceException &e){
        res.success = false;
        res.message = e.what();
    }
    return true;
}

void RosInterface::dynamicReconfigureCallback(phoxi_camera::phoxi_cameraConfig &config, uint32_t level) {
    if(!PhoXiInterface::isConnected()){
        config = this->dynamicReconfigureConfig;
        return;
    }
    if (level & (1 << 1)) {
        try {
            switch (config.resolution){
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
        }catch(PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 2)) {
        try{
            this->isOk();
            scanner->CapturingSettings->ScanMultiplier = config.scan_multiplier;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 3)) {
        try{
            this->isOk();
            scanner->CapturingSettings->ShutterMultiplier = config.shutter_multiplier;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 4)) {
        try{
            PhoXiInterface::setTriggerMode(config.trigger_mode,config.start_acquisition);
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 5)) {
        try{
            this->isOk();
            scanner->Timeout = config.timeout;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 6)) {
        try{
            this->isOk();
            scanner->ProcessingSettings->Confidence = config.confidence;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 7)) {
        try{
            this->isOk();
            scanner->OutputSettings->SendPointCloud = config.send_point_cloud;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 8)) {
        try{
            this->isOk();
            scanner->OutputSettings->SendNormalMap = config.send_normal_map;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 9)) {
        try{
            this->isOk();
            scanner->OutputSettings->SendConfidenceMap = config.send_confidence_map;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }
    
    if (level & (1 << 10)) {
        try{
            this->isOk();
            scanner->OutputSettings->SendTexture = config.send_texture;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }

    if (level & (1 << 11)) {
        try{
            this->isOk();
            scanner->OutputSettings->SendTexture = config.send_depth_map;
        }catch (PhoXiInterfaceException &e){
            ROS_WARN("%s",e.what());
        }
    }
}

pho::api::PFrame RosInterface::getPFrame(int id){
    pho::api::PFrame frame = PhoXiInterface::getPFrame(id);
    //update dynamic reconfigure
    dynamicReconfigureConfig.coordination_space = pho::api::PhoXiTriggerMode::Software;
    dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
    return frame;
}

void RosInterface::getDepthMapSetting(){
    std::string proj_path = ros::package::getPath("phoxi_camera");
    std::string calibrationFile = "";
    nh.getParam("calibration_file", calibrationFile);
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

    if (CorrectCalibration)
    {
        DepthMapSetting.flag = 1;
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

int RosInterface::triggerImage(){
    int id = PhoXiInterface::triggerImage();
    //update dynamic reconfigure
    dynamicReconfigureConfig.coordination_space = pho::api::PhoXiTriggerMode::Software;
    dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
    return id;
}

void RosInterface::connectCamera(std::string HWIdentification, pho::api::PhoXiTriggerMode mode, bool startAcquisition){
    PhoXiInterface::connectCamera(HWIdentification,mode,startAcquisition);
    bool initFromConfig = false;
    nh.getParam("init_from_config",initFromConfig);
    if(initFromConfig){
        dynamicReconfigureServer.getConfigDefault(dynamicReconfigureConfig);
    }
    else{
        initFromPhoXi();
    }
    dynamicReconfigureServer.updateConfig(dynamicReconfigureConfig);
    this->dynamicReconfigureCallback(dynamicReconfigureConfig,std::numeric_limits<uint32_t>::max());
    diagnosticUpdater.force_update();
}

void RosInterface::diagnosticCallback(diagnostic_updater::DiagnosticStatusWrapper& status){
    if(PhoXiInterface::isConnected()){
        if(PhoXiInterface::isAcquiring()){
            status.summary(diagnostic_msgs::DiagnosticStatus::OK,"Ready");
        }
        else{
            status.summary(diagnostic_msgs::DiagnosticStatus::WARN,"Acquisition not started");
        }
        status.add("HardwareIdentification",std::string(scanner->HardwareIdentification));
        status.add("Trigger mode",getTriggerMode(scanner->TriggerMode));

    }
    else{
        status.summary(diagnostic_msgs::DiagnosticStatus::ERROR,"Not connected. ");
    }
}

void RosInterface::diagnosticTimerCallback(const ros::TimerEvent&){
    diagnosticUpdater.force_update();
}

std::string RosInterface::getTriggerMode(pho::api::PhoXiTriggerMode mode){
    switch (mode){
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

void RosInterface::initFromPhoXi(){
    dynamicReconfigureServer.getConfigDefault(dynamicReconfigureConfig);
    if(!scanner->isConnected()){
        ROS_WARN("Scanner not connected.");
        dynamicReconfigureServer.getConfigDefault(dynamicReconfigureConfig);
        return;
    }
    ///resolution
    pho::api::PhoXiCapturingMode mode = scanner->CapturingMode;
    if((mode.Resolution.Width == 2064) && (mode.Resolution.Height = 1544)){
        this->dynamicReconfigureConfig.resolution = 1;
    }
    else{
        this->dynamicReconfigureConfig.resolution = 0;
    }
    this->dynamicReconfigureConfig.scan_multiplier = scanner->CapturingSettings->ScanMultiplier;
    this->dynamicReconfigureConfig.shutter_multiplier = scanner->CapturingSettings->ShutterMultiplier;
    this->dynamicReconfigureConfig.trigger_mode = scanner->TriggerMode.GetValue();
    this->dynamicReconfigureConfig.start_acquisition = scanner->isAcquiring();
    this->dynamicReconfigureConfig.timeout = scanner->Timeout.GetValue();
    this->dynamicReconfigureConfig.confidence = scanner->ProcessingSettings->Confidence;
    this->dynamicReconfigureConfig.send_point_cloud = scanner->OutputSettings->SendPointCloud;
    this->dynamicReconfigureConfig.send_normal_map = scanner->OutputSettings->SendNormalMap;
    this->dynamicReconfigureConfig.send_confidence_map = scanner->OutputSettings->SendConfidenceMap;
    this->dynamicReconfigureConfig.send_depth_map = scanner->OutputSettings->SendDepthMap;
    this->dynamicReconfigureConfig.send_texture = scanner->OutputSettings->SendTexture;
}



