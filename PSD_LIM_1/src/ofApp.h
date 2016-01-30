#pragma once

#include "ofMain.h"
#include "ofxGui.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
        void exit();
    
    // ------------------
    // --- LIVE VIDEO ---
    // ------------------
    
    ofVideoGrabber vidGrabber;
    int camWidth = 640;         // source video size
    int camHeight = 480;
    unsigned long nPixels;      // total number of source pixels
    
    // ------------------
    // ----- SOURCE ----- ( resampled video )
    // ------------------
    
    int sampleSize = 6;         // 1 sample = 2^n pixels where n = {1, 6}, higher numbers mean the signal is integrated over a larger area, sacrificing spatial resolution for 3d image quality (noise reduction)
    int pixelsPerSample;
    int srcWidth;               // source image size
    int srcHeight;
    unsigned long nSamples;     // number of discrete samples in source data
    
    ofFloatColor *thisData;     // stores source data (signal + noise or just noise)
    ofFloatColor *lastData;
    
    ofImage srcImage;
    
    // ------------------
    // -- REMOVE NOISE --
    // ------------------
    
    float *sampleWeights;       // how the signals are weighed in calculating the lightness
    float minDesiredWeight = 0.3; // weights will go from {minDesiredWeight, 1.0}
    ofImage imgWeights;
    
    float maxLightness = 0;     // max lightness possible for a single frame (sum of all weighted lightnesses)
    float lastLightness = 1.;   // {0, 1}
    
    // ------------------
    // ----- SIGNAL -----
    // ------------------
    
    float *signals;             // this is the true "signal" (no noise)
    ofImage sigImage;
    
    // --------------------------
    // -- SCENE RECONSTRUCTION --
    // --------------------------
    
    ofMesh sceneMesh;
    
    // ------------------
    // ---- UTILITIES ---
    // ------------------
    
    ofEasyCam cam;

    vector<float> videoTimes;
    float videoFPS = 0;
    
    vector<float> sceneTimes;
    float sceneFPS = 0;
    
    ofParameter<float> lightnessThreshold;
    ofParameter<float> distanceMulitplier;
    ofParameter<float> pointSpacing;
    ofParameter<float> pointSize;
    ofParameter<bool> drawDebugFrames;
    ofParameterGroup generalControls;
    
    ofxPanel panel;
    
    
    
};
