#include "ofApp.h"

#define OFAPP_TOLERANCE_THRESHOLD 0.6f // below that, ignored

//--------------------------------------------------------------
void ofApp::setup(){
    // Set up the camera feed
    vector<ofVideoDevice> devices = vidGrabber.listDevices();
    for(int i = 0; i < devices.size(); i++) {
        if(devices[i].bAvailable) {
            ofLogNotice("Found camera '" + devices[i].deviceName + "' (" + devices[i].hardwareName + ") at index  " + ofToString(i));
        } else {
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName << " - unavailable ";
        }
    }
    loadSettings();
    
    // Look for the cen_ files in model/patch_experts. They have to be downloaded separately.
    // See the addon GUI at https://github.com/antimodular/ofxOpenFace/wiki.
    ofDirectory dir("model/patch_experts");
    dir.allowExt("mat");
    dir.allowExt("dat");
    dir.listDir();
    if (dir.size() == 0) {
        ofLogError("ofApp", "The patch expert files were not found. See the Wiki instructions. Exiting app.");
        std::exit(1);
    }
    
    // Initialize the grabber
    vidGrabber.setDeviceID(settings.nCameraIndex);
    vidGrabber.setDesiredFrameRate(settings.nCameraFrameRate);
    vidGrabber.initGrabber(settings.nCameraWidth, settings.nCameraHeight);
    ofVec2f ptCameraDims(vidGrabber.getWidth(), vidGrabber.getHeight());
    imgToProcess.allocate(settings.nCameraWidth, settings.nCameraHeight, ofImageType::OF_IMAGE_COLOR);
    
    // Configure GUI
    gui.setup();
    gui.setName("ofxOpenFace demo");
    gui.setHeaderBackgroundColor(ofColor::hotPink);
    gui.setDefaultWidth(300);
    
    // Log the choice of detectors to the console
    ofLogNotice("ofApp", "Face detector: " + ofxOpenFace::FaceDetectorToString(settings.eDetectorFace));
    ofLogNotice("ofApp", "Landmark detector: " + ofxOpenFace::LandmarkDetectorToString(settings.eDetectorLandmarks));
    
    // Is there a video file to play?
    ofDirectory dirVideos(""); // the root data folder
    dirVideos.allowExt("mp4");
    dirVideos.allowExt("mov");
    dirVideos.allowExt("mpg");
    dirVideos.allowExt("mpeg");
    dirVideos.listDir();
    if (dirVideos.size() == 0) {
        ofLogNotice("ofApp", "No video files found, the app will run in webcam mode.");
    } else {
        bUseVideoFile = true;
        auto vid = dirVideos[0];
        videoPlayer.load(vid.getAbsolutePath());
        videoPlayer.play();
        ofLogNotice("ofApp", "Loading video file '" + vid.getAbsolutePath() + "'");
    }
    
    gui.add(lblWebcam.setup("Webcam/video", bUseVideoFile ? "video" : "webcam"));
    gui.add(lblDetectorFace.setup("Face Detector", ofxOpenFace::FaceDetectorToString(settings.eDetectorFace)));
    gui.add(lblDetectorLandmarks.setup("Landmark Detector", ofxOpenFace::LandmarkDetectorToString(settings.eDetectorLandmarks)));
    gui.add(lblSingleMultiple.setup("Mode", settings.bMultipleFaces ? "Multiple faces (max " + ofToString(settings.nMaxFaces) + ")" : "Single face"));
    gui.add(togDoTracking.setup("Do ofxCv tracking", settings.bDoCvTracking));
    togDoTracking.addListener(this, &ofApp::onDoTrackingChanged);
    gui.add(lblTrackingPersistence.setup("Tracking persistence", ofToString(settings.nTrackingPersistenceMs)));
    gui.add(lblTrackingMaxDistance.setup("Tracking max distance", ofToString(settings.nTrackingTolerancePx)));
    gui.add(sliCertainty.setup("OpenFace certainty", settings.fCertaintyNorm, 0.0f, 1.0f));
    sliCertainty.addListener(this, &ofApp::callbackCertaintyChanged);
    gui.add(sliKillAfterNotSeenMs.setup("Kill after not seen (ms)", settings.nKillAfterDisappearedMs, 500, 4000));
    sliKillAfterNotSeenMs.addListener(this, &ofApp::callbackKillAfterChanged);
    gui.add(lblCameraDimensions.setup("Actual camera dimensions", ofToString(ptCameraDims.x) + "x" + ofToString(ptCameraDims.y)));
    gui.add(lblCameraSettings.setup("Camera settings", "fx: " + ofToString(settings.fx) + " fy: " + ofToString(settings.fy) + " cx: " + ofToString(settings.cx) + " cy: " + ofToString(settings.cy)));
    gui.add(lblCameraIndex.setup("Camera index", ofToString(settings.nCameraIndex)));
    gui.setPosition(640, 0);
    
    ofAddListener(ofxOpenFace::eventOpenFaceDataClear, this, &ofApp::onFaceDataClear);
    if (settings.bDoCvTracking) {
        ofAddListener(ofxOpenFace::eventOpenFaceDataSingleTracked, this, &ofApp::onFaceDataSingleTracked);
        ofAddListener(ofxOpenFace::eventOpenFaceDataMultipleTracked, this, &ofApp::onFaceDataMultipleTracked);
    } else {
        ofAddListener(ofxOpenFace::eventOpenFaceDataSingleRaw, this, &ofApp::onFaceDataSingleRaw);
        ofAddListener(ofxOpenFace::eventOpenFaceDataMultipleRaw, this, &ofApp::onFaceDataMultipleRaw);
    }
    
    openFace.startThread();
    ofxOpenFace::CameraSettings camSettings;
    camSettings.fx = settings.fx;
    camSettings.fy = settings.fy;
    camSettings.cx = settings.cx;
    camSettings.cy = settings.cy;
    openFace.setup(settings.bMultipleFaces, settings.nCameraWidth, settings.nCameraHeight, settings.eDetectorFace, settings.eDetectorLandmarks, camSettings, settings.nTrackingPersistenceMs, settings.nTrackingTolerancePx, settings.nMaxFaces);
    ofxOpenFace::s_fCertaintyNorm = settings.fCertaintyNorm;
    ofxOpenFace::s_nKillAfterDisappearedMs = settings.nKillAfterDisappearedMs;
    
    bDrawFaces = true;
    bOpenFaceEnabled = true;
    ofSetFrameRate(settings.nAppFrameRate);
    ofLogNotice("ofApp", "Application frame rate: " + ofToString(settings.nAppFrameRate));
}

//--------------------------------------------------------------
void ofApp::update(){
    if (bUseVideoFile) {
        videoPlayer.update();
        if(videoPlayer.isFrameNew()) {
            ofPixels& pixels = videoPlayer.getPixels();
            imgToProcess.setFromPixels(pixels);
            if (bOpenFaceEnabled) {
                openFace.setImage(imgToProcess);
            }
        }
    } else {
        vidGrabber.update();
        if(vidGrabber.isFrameNew()) {
            ofPixels& pixels = vidGrabber.getPixels();
            imgToProcess.setFromPixels(pixels);
            if (bOpenFaceEnabled) {
                openFace.setImage(imgToProcess);
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    ofSetBackgroundColor(ofColor::black);
    ofSetColor(ofColor::white);
    imgToProcess.draw(0, 0);
    if (bDrawFaces && bOpenFaceEnabled) {
        if (settings.bDoCvTracking) {
            // draw the tracked faces
            if (!settings.bMultipleFaces) {
                if (!latestDataSingleTracked.cleared) {
                    latestDataSingleTracked.draw(true);
                }
            } else {
                for (auto d : latestDataMultipleTracked) {
                    if (!d.cleared) {
                        d.draw(true);
                    }
                }
            }
        } else {
            // draw the raw faces
            if (!settings.bMultipleFaces) {
                if (!latestDataSingle.cleared) {
                    latestDataSingle.draw(true);
                }
            } else {
                for (auto d : latestDataMultiple) {
                    if (!d.cleared) {
                        d.draw(true);
                    }
                }
            }
        }
    }
    
    // Draw the FPS
    ofSetColor(ofColor::red);
    ofDrawBitmapString(ofToString((int)ofGetFrameRate()) + " FPS (app)", 40, 40);
    ofDrawBitmapString(ofToString(openFace.getFPS()) + " FPS (tracking)", 40, 60);
    
    gui.draw();
}

//--------------------------------------------------------------
void ofApp::exit(){
    if (settings.bDoCvTracking) {
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataSingleTracked, this, &ofApp::onFaceDataSingleTracked);
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataMultipleTracked, this, &ofApp::onFaceDataMultipleTracked);
    } else {
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataSingleRaw, this, &ofApp::onFaceDataSingleRaw);
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataMultipleRaw, this, &ofApp::onFaceDataMultipleRaw);
    }
    
    openFace.exit();
    saveSettings();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    // restart the tracker
    if (key == 'r') {
        openFace.resetFaceModel();
    } else if (key == 'b') {
        // Toggle
        bDrawFaces = !bDrawFaces;
    } else if (key == 'h') {
        // Disable OpenFace
        bOpenFaceEnabled = !bOpenFaceEnabled;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

void ofApp::loadSettings() {
    ofxXmlSettings s;
    s.loadFile("settings.xml");
    settings.nAppFrameRate = s.getValue("settings:app:framerate", 30);
    settings.nCameraIndex = s.getValue("settings:camera:index", 0);
    settings.nCameraFrameRate = s.getValue("settings:camera:framerate", 30);
    settings.nCameraWidth = s.getValue("settings:camera:width", 640);
    settings.nCameraHeight = s.getValue("settings:camera:height", 480);
    settings.bDoCvTracking = s.getValue("settings:tracking:doCvTracking", true);
    settings.bMultipleFaces = s.getValue("settings:tracking:multipleFaces", true);
    settings.nMaxFaces = s.getValue("settings:tracking:maxFaces", 4);
    settings.nTrackingPersistenceMs = s.getValue("settings:tracking:persistenceMs", 30);
    settings.nTrackingTolerancePx = s.getValue("settings:tracking:tolerancePixels", 200);
    settings.nKillAfterDisappearedMs = s.getValue("settings:tracking:killAfterDisappearedMs", 3000);
    settings.fCertaintyNorm = s.getValue("settings:tracking:certainty", 0.4f);
    settings.fx = s.getValue("settings:camera:fx", 500);
    settings.fy = s.getValue("settings:camera:fy", 500);
    settings.cx = s.getValue("settings:camera:cx", settings.nCameraWidth/2.0f);
    settings.cy = s.getValue("settings:camera:cy", settings.nCameraHeight/2.0f);
    settings.eDetectorFace = (LandmarkDetector::FaceModelParameters::FaceDetector)s.getValue("settings:tracking:detector:face", (int)LandmarkDetector::FaceModelParameters::FaceDetector::HAAR_DETECTOR);
    settings.eDetectorLandmarks = (LandmarkDetector::FaceModelParameters::LandmarkDetector)s.getValue("settings:tracking:detector:landmarks", (int)LandmarkDetector::FaceModelParameters::LandmarkDetector::CECLM_DETECTOR);
}

void ofApp::saveSettings() {
    ofxXmlSettings s;
    s.setValue("settings:app:framerate", settings.nAppFrameRate);
    s.setValue("settings:camera:index", settings.nCameraIndex);
    s.setValue("settings:camera:framerate", settings.nCameraFrameRate);
    s.setValue("settings:camera:width", settings.nCameraWidth);
    s.setValue("settings:camera:height", settings.nCameraHeight);
    s.setValue("settings:tracking:doCvTracking", settings.bDoCvTracking);
    s.setValue("settings:tracking:multipleFaces", settings.bMultipleFaces);
    s.setValue("settings:tracking:maxFaces", settings.nMaxFaces);
    s.setValue("settings:tracking:detector:face", (int)settings.eDetectorFace);
    s.setValue("settings:tracking:detector:landmarks", (int)settings.eDetectorLandmarks);
    s.setValue("settings:tracking:persistenceMs", settings.nTrackingPersistenceMs);
    s.setValue("settings:tracking:tolerancePixels", settings.nTrackingTolerancePx);
    s.setValue("settings:tracking:killAfterDisappearedMs", settings.nKillAfterDisappearedMs);
    s.setValue("settings:tracking:certainty", settings.fCertaintyNorm);
    s.setValue("settings:camera:fx", settings.fx);
    s.setValue("settings:camera:fy", settings.fy);
    s.setValue("settings:camera:cx", settings.cx);
    s.setValue("settings:camera:cy", settings.cy);
    s.saveFile("settings.xml"); //puts settings.xml file in the bin/data folder
}

void ofApp::onFaceDataClear(bool& data) {
    mutexFaceData.lock();
    if (settings.bMultipleFaces) {
        // TODO: is it a good idea to clear the vectors?
        if (settings.bDoCvTracking) {
            latestDataMultipleTracked.clear();
        } else {
            latestDataMultiple.clear();
        }
    } else {
        if (settings.bDoCvTracking) {
            latestDataSingleTracked.cleared = true;
        } else {
            latestDataSingle.cleared = true;
        }
    }
    mutexFaceData.unlock();
}

void ofApp::onFaceDataSingleRaw(ofxOpenFaceDataSingleFace& data) {
    mutexFaceData.lock();
    latestDataSingle = data;
    mutexFaceData.unlock();
}

void ofApp::onFaceDataMultipleRaw(vector<ofxOpenFaceDataSingleFace>& data) {
    mutexFaceData.lock();
    latestDataMultiple = data;
    mutexFaceData.unlock();
}

void ofApp::onFaceDataSingleTracked(ofxOpenFaceDataSingleFaceTracked &data) {
    mutexFaceData.lock();
    latestDataSingleTracked = data;
    mutexFaceData.unlock();
}

void ofApp::onFaceDataMultipleTracked(vector<ofxOpenFaceDataSingleFaceTracked> &data) {
    mutexFaceData.lock();
    latestDataMultipleTracked = data;
    mutexFaceData.unlock();
}

void ofApp::onDoTrackingChanged(const void* sender, bool& pressed) {
    // Remove existing listeners, add other ones
    ofRemoveListener(ofxOpenFace::eventOpenFaceDataClear, this, &ofApp::onFaceDataClear);
    ofAddListener(ofxOpenFace::eventOpenFaceDataClear, this, &ofApp::onFaceDataClear);
    
    if (pressed) {
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataSingleRaw, this, &ofApp::onFaceDataSingleRaw);
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataMultipleRaw, this, &ofApp::onFaceDataMultipleRaw);
        ofAddListener(ofxOpenFace::eventOpenFaceDataSingleTracked, this, &ofApp::onFaceDataSingleTracked);
        ofAddListener(ofxOpenFace::eventOpenFaceDataMultipleTracked, this, &ofApp::onFaceDataMultipleTracked);
    } else {
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataSingleTracked, this, &ofApp::onFaceDataSingleTracked);
        ofRemoveListener(ofxOpenFace::eventOpenFaceDataMultipleTracked, this, &ofApp::onFaceDataMultipleTracked);
        ofAddListener(ofxOpenFace::eventOpenFaceDataSingleRaw, this, &ofApp::onFaceDataSingleRaw);
        ofAddListener(ofxOpenFace::eventOpenFaceDataMultipleRaw, this, &ofApp::onFaceDataMultipleRaw);
    }
    settings.bDoCvTracking = pressed;
}

void ofApp::callbackCertaintyChanged(float &value) {
    ofxOpenFace::s_fCertaintyNorm = value;
    settings.fCertaintyNorm = value;
}

void ofApp::callbackKillAfterChanged(int &value) {
    ofxOpenFace::s_nKillAfterDisappearedMs = value;
    settings.nKillAfterDisappearedMs = value;
}
