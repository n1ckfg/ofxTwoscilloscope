#pragma once

#include "ofMain.h"

#include "ofxTwoscilloscope.h"

// Example 2: XYscope format audio -> vector shapes.
//
// An audio file (or the line input) plays through the Oscilloscope renderer,
// which draws it the way an analog scope would. The same audio is decoded
// back into vector shapes, drawn on the right and saved as SVG.
class ofApp : public ofBaseApp {

    public:

        void setup();
        void update();
        void draw();

        void keyPressed(int key);
        void dragEvent(ofDragInfo dragInfo);

        void audioOut(ofSoundBuffer & buffer);
        void audioIn(ofSoundBuffer & buffer);

        void loadFile(const std::string & path);
        bool openSoundStream(bool withInput);

        Oscilloscope scope; // renders the beam, and decodes the shapes
        XYPlayer player;    // plays the file, to the sound card and to the scope
        ofSoundStream soundStream;
        bool audioOk;
        bool liveInput;

        std::vector<ofPolyline> shapes;
        float panelSize;
        bool showPoints;
        std::string status;

};
