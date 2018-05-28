/*
* ofxOpenFace.h
* openFrameworks
*
* The interface to the OpenFace library.
*
*/

// Forward declaration, otherwise we get a compilation error
class ofxOpenFaceDataSingleFaceTracked;
namespace ofxCv {
    float trackingDistance(const ofxOpenFaceDataSingleFaceTracked& a, const ofxOpenFaceDataSingleFaceTracked& b);
}

#include "ofMain.h"
#include "ofxCv.h"
#include "ofThread.h"

// OpenFace
#include "LandmarkCoreIncludes.h"
#include <VisualizationUtils.h>
#include <Visualizer.h>
#include <RotationHelpers.h>
#include <SequenceCapture.h>
#include <RecorderOpenFace.h>
#include <RecorderOpenFaceParameters.h>
#include <GazeEstimation.h>
#include <FaceAnalyser.h>

#include <fstream>
#include <sstream>

// OpenCV includes
#include <opencv2/videoio/videoio.hpp>  // Video write
#include <opencv2/videoio/videoio_c.h>  // Video write
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

// ofxOpenFace addon
#include "ofxOpenFaceDataSingleFace.h"
#include "ofxOpenFaceDataSingleFaceTracked.h"

// Some useful preprocessor definitions
//#define OFX_OPENFACE_DO_FACE_ANALYSIS 1 // uncomment to do AU analysis
#define OFX_OPENFACE_MODEL "model/main_ceclm_general.txt"
#define OFX_OPENFACE_DETECTOR_HAAR "classifiers/haarcascade_frontalface_alt.xml"
#define OFX_OPENFACE_DETECTOR_MTCNN "model/mtcnn_detector/MTCNN_detector.txt"

#pragma once

class ofxOpenFace : public ofThread {
    public:
        struct CameraSettings {
            int fx, fy, cx, cy;
        };
    
        ofxOpenFace();
        ~ofxOpenFace();
        void setup(bool bTrackMultipleFaces, int nWidth, int nHeight, LandmarkDetector::FaceModelParameters::FaceDetector eMethod, CameraSettings settings, int persistenceMs, int maxDistancePx, int nMaxFacesTracked);
        void setImage(ofImage img);
        vector<ofxOpenFaceDataSingleFaceTracked> getTracked();

        void exit();
        void stop();
        void resetFaceModel();
        int getFPS();
    
        // Events for the raw OpenFace data
        static ofEvent<ofxOpenFaceDataSingleFace>              eventOpenFaceDataSingleRaw;
        static ofEvent<vector<ofxOpenFaceDataSingleFace>>      eventOpenFaceDataMultipleRaw;
    
        // Events for the tracked OpenFace data
        static ofEvent<ofxOpenFaceDataSingleFaceTracked>              eventOpenFaceDataSingleTracked;
        static ofEvent<vector<ofxOpenFaceDataSingleFaceTracked>>      eventOpenFaceDataMultipleTracked;
    
        // Camera settings
        static CameraSettings s_camSettings;
    
    private:
        void setupSingleFace();
        void setupMultipleFaces(LandmarkDetector::FaceModelParameters::FaceDetector eMethod);
        ofxOpenFaceDataSingleFace processImageSingleFace();
        vector<ofxOpenFaceDataSingleFace> processImageMultipleFaces();
        virtual void threadedFunction();
        void setFPS(float value);
    
        static void NonOverlapingDetections(const vector<LandmarkDetector::CLNF>& clnf_models, vector<cv::Rect_<float>>& face_detections);
    
        int                                             nImgWidth;   // the width of the image used for tracking
        int                                             nImgHeight;  // the height of the image used for tracking
        int                                             nMaxFaces; // the maximum number of faces
        int                                             nFrameCount; // count the frames being tracked
    
#ifdef OFX_OPENFACE_DO_FACE_ANALYSIS
        FaceAnalysis::FaceAnalyserParameters*           pFace_analysis_params = nullptr;
        FaceAnalysis::FaceAnalyser*                     pFace_analyser = nullptr;
#endif
        vector<LandmarkDetector::CLNF>                  vFace_models;
        LandmarkDetector::CLNF*                         pFace_model;
        vector<bool>                                    vActiveModels;
        LandmarkDetector::FaceModelParameters           det_parameters;
        vector<LandmarkDetector::FaceModelParameters>   vDet_parameters;
        bool                                            bExit = false; // flag to close the thread
        ofMutex                                         mutexImage;
        float                                           fTimePerRunMs = 0.0f;
        bool                                            bMultipleFaces;
        bool                                            bHaveNewImage = false; // there is a new image available
        Utilities::FpsTracker                           fps_tracker;
        cv::Mat                                         matToProcessColor; // the material to process for tracking
        cv::Mat                                         matToProcessGrayScale; // the material to process for tracking
        ofxCv::TrackerFollower<ofxOpenFaceDataSingleFace, ofxOpenFaceDataSingleFaceTracked>  tracker;
};
