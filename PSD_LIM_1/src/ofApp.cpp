#include "ofApp.h"

// TODO: at the same distance and the same imposed illumination, a light-colored object will get brighter than a dark-colored object. Make the calculated distance proportional to the amount of signal given the amount of "noise" illumination (i.e. lightness of sample when light is off.
// TODO: see the scene recon better
// TODO: start and stop flashing with spacebar
// TODO: record animation
// TODO: make flashing speed dependent on changing video speed
// TODO: make threshold dependent on chanding scene speed
// TODO: research... is brightest illuminaton always better? Is there a point at which it's overpowering given a certain amount of ambient illumination?
// TODO: account for different angle of screen to camera


//--------------------------------------------------------------
void ofApp::setup(){
    ofBackground(0);
    
//    ofSetFrameRate(60);
    
    ofSetFrameRate(120);
    ofSetVerticalSync(true);
    
    cam.setDistance(2000);
    
    generalControls.setName("General Controls");
    generalControls.add(lightnessThreshold.set("Lightness Thresh", 1.05, 1., 1.1));
    generalControls.add(distanceMulitplier.set("Distance Mult", 10, 0, 100));
    generalControls.add(pointSpacing.set("Point Spacing", 5, 0, 50));
    generalControls.add(pointSize.set("Point Size", 1, 0, 20));
    generalControls.add(drawDebugFrames.set("Debug Video", false));
    
    panel.setup();
    panel.add(generalControls);
    panel.loadFromFile("settings.xml");
    
    // setup the video grabber
    vidGrabber.setDeviceID(1);
    vidGrabber.setVerbose(true);
    vidGrabber.setup(camWidth, camHeight);
    
    // allocate memory / initialize objects
    nPixels = camWidth * camHeight;
    pixelsPerSample = int(pow(2.f, (float)sampleSize));
    srcWidth = camWidth / sampleSize;
    srcHeight = camHeight / sampleSize;
    nSamples = srcWidth * srcHeight;
    thisData = new ofFloatColor[nSamples];
    lastData = new ofFloatColor[nSamples];
    sampleWeights = new float[nSamples];
    signals = new float[nSamples];
    
    // images for debugging
    srcImage.allocate(srcWidth, srcHeight, OF_IMAGE_COLOR);
    sigImage.allocate(srcWidth, srcHeight, OF_IMAGE_GRAYSCALE);
    
    // setup scene recon mesh
    sceneMesh.setMode(OF_PRIMITIVE_POINTS);
    sceneMesh.enableColors();
    
    // set the pixel weights (weighted toward center and bottom --> typical of webcam of head and upper torso)
    float minWeight = 9999.;
    float maxWeight = -9999.;
    for (int i = 0; i < nSamples; i++) {
        int x = i % srcWidth;
        int y = i / srcWidth;
        
        sampleWeights[i] =
            1 - sqrt(
                     pow((float)(x - srcWidth / 2), 2.f)
                     + pow((float)y - (float)srcHeight / 3., 2.f)
                     )
            / sqrt(
                   pow(float(srcWidth) / 2., 2.f) + pow(float(srcHeight) * 2. / 3., 2.f)
                   )
            + 0.75 * (float)y / (float)srcHeight;
        
        // find min and max weights
        if (sampleWeights[i] < minWeight) minWeight = sampleWeights[i];
        if (sampleWeights[i] > maxWeight) maxWeight = sampleWeights[i];
    }
    
    float scaleFrom = maxWeight - minWeight;
    float scaleTo = 1. - minDesiredWeight;
    for (int i = 0; i < nSamples; i++) {
        // normalize weights
        sampleWeights[i] = (sampleWeights[i] - minWeight) / scaleFrom * scaleTo + minDesiredWeight;
        
        // find max lightness
        maxLightness += sampleWeights[i];
    }
    maxLightness /= (float)nSamples;
    
    // debug sample weights visually
//    imgWeights.allocate(srcWidth, srcHeight, OF_IMAGE_GRAYSCALE);
//    for (int i = 0; i < nSamples; i++) {
//        imgWeights.getPixels()[i] = char(sampleWeights[i] * 255.);
//    }
//    imgWeights.update();

}

//--------------------------------------------------------------
void ofApp::update(){
    
    vidGrabber.update();
    
    if(vidGrabber.isFrameNew()) {
        
        // add timestamp to video fps counter
        videoTimes.push_back(ofGetElapsedTimef());
        
        // Resample video feed to remove noise. Also find weighted lightness of image.
        ofPixelsRef pixelRef = vidGrabber.getPixels();
        int nChannels = pixelRef.getNumChannels();
        float thisLightness = 0;
        for (int i = 0; i < nSamples; i++) {
//            int srcX = i % srcWidth;
//            int srcY = i / srcWidth;
//            int camX = srcX * sampleSize;
//            int camY = srcY * sampleSize;
//            unsigned long pixelIndex = (camX + camY * camWidth) * nChannels;
            
            // find index of top left most pixel
            int x = (i % srcWidth) * sampleSize;
            int y = (i / srcWidth) * sampleSize;
            unsigned long pixelIndex = (x + y * camWidth) * nChannels;
            
            // find average color across this block of pixels
            ofFloatColor thisColor;
            for (int j = 0; j < 3; j++) {                   // channel
                float channelSum = 0;
                for (int k = 0; k < sampleSize; k++){       // row
                    for (int l = 0; l < sampleSize; l++) {  // column
                        channelSum += (float)pixelRef[
                                                      pixelIndex
                                                      + j
                                                      + k * camWidth * nChannels
                                                      + l * nChannels
                                                      ];
                    }
                }
                thisColor[j] = channelSum / ((float)pixelsPerSample * 255.);
            }
            
            thisData[i] = thisColor;
                
            // debug resampled video (source data)
            for (int j = 0; j < 3; j++) {
                srcImage.getPixels()[i * 3 + j] = char(thisColor[j] * 255.);
            }

            thisLightness += thisData[i].getLightness() * sampleWeights[i];
        }
        srcImage.update();

        // normalize lightness
        thisLightness /= ((float)nSamples * maxLightness);
        
        // Find the signal by filtering out the noise: Determine whether the light is on in this frame.
        // should be a function of ambient lightness -- could change depending on number of scene frames we're getting (e.g. if scene frames > frequency, raise threshold)
        if (thisLightness > (lastLightness * lightnessThreshold)) {
            
            // add timestamp to scene fps counter
            sceneTimes.push_back(ofGetElapsedTimef());
            
            sceneMesh.clear();
            
            float maxSampleLightness = 0;
            
            // find the signal
            for (int i = 0; i < nSamples; i++) {
                
                // cull out negative signals
                float thisSignal = MAX(0., thisData[i].getLightness() - lastData[i].getLightness());
//                float thisSignal = MAX(0., thisData[i].getLightness() - lastData[i].getLightness()) / thisData[i].getLightness();
                if (thisSignal > maxSampleLightness) maxSampleLightness = thisSignal;
                signals[i] = thisSignal;
                
                // alt:
//                signals[i] = (thisData[i].getLightness() - lastData[i].getLightness()) / (lastData[i].getLightness() - pow(minDesiredWeight, 2.f));
                
                // Reconstruct scene by turning signals into distances
                sceneMesh.addVertex(ofVec3f(float(i % srcWidth) * pointSpacing,
                                            float(i / srcWidth) * pointSpacing,
                                            1. / sqrt(signals[i]) * distanceMulitplier));
//                sceneMesh.addColor(ofFloatColor(1.)); // B&W
                sceneMesh.addColor(thisData[i]); // COLOR (from light on)
            }
            
            // visually debug signal
            for (int i = 0; i < nSamples; i++) {
                sigImage.getPixels()[i] = char(signals[i] / maxSampleLightness * 255.);
            }
            sigImage.update();
        }

        // store last samples
        for (int i = 0; i < nSamples; i++) {
            lastData[i] = thisData[i];
        }
        
        // set last lightness
        lastLightness = thisLightness;
    }

    // calculate video fps
    if (videoTimes.size() > 0) {
        while (videoTimes.front() < (ofGetElapsedTimef() - 1.) && videoTimes.size() > 0) {
            videoTimes.erase(videoTimes.begin());
        }
    }
    videoFPS = videoFPS * 0.67 + videoTimes.size() * 0.33;
    
    // calculate 3d video fps
    if (sceneTimes.size() > 0) {
        while (sceneTimes.front() < (ofGetElapsedTimef() - 1.) && sceneTimes.size() > 0) {
            sceneTimes.erase(sceneTimes.begin());
        }
    }
    sceneFPS = sceneFPS * 0.67 + sceneTimes.size() * 0.33;

}

//--------------------------------------------------------------
void ofApp::draw(){
    // modulate backgrund at half the frequency of video fps
    float interval = 50.; // 100.
    ofBackground(floor(fmod((double)(ofGetElapsedTimeMillis() / interval), 2.)) * 255);
    
    // draw distances
    ofSetColor(255);
    cam.begin();
//    ofTranslate(ofGetWidth() / 2 - newCamWidth / 2 * pointSpacing, ofGetHeight() / 2 - newCamHeight / 2 * pointSpacing);
    sceneMesh.draw();
    cam.end();
    
    // draw thumbnail of video capture
    if (drawDebugFrames) {
        vidGrabber.draw(0, ofGetHeight() - camHeight / 2, camWidth / 2, camHeight / 2);
        srcImage.draw(camWidth / 2, ofGetHeight() - camHeight / 2, camWidth / 2, camHeight / 2);
        sigImage.draw(camWidth, ofGetHeight() - camHeight / 2, camWidth / 2, camHeight / 2);
    }
    
    panel.draw();
    
    // debug sample weights visually
//    imgWeights.draw(200, 200);
    
    // debug fps
    ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate()) + "\t" + ofToString(videoFPS) + "\t" + ofToString(sceneFPS), 10, 20);
}

//--------------------------------------------------------------

void ofApp::exit() {
    
    panel.saveToFile("settings.xml");
    
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    
    if (key == 'f') ofToggleFullscreen();

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
