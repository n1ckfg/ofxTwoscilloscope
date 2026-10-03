#pragma once

#include "ofMain.h"

#include "ofxGui.h"
#include "ofxTwoscilloscope.h"

// Example 3: a vector shape -> XY audio -> audio effects -> a new vector shape.
//
// The shape on the left is encoded as XYscope audio, run through the effect
// chain in the panel, and decoded back into the shape on the right. The
// altered audio also loops out of the sound card (and through the beam in
// the middle), so what you hear is what you see.
class ofApp : public ofBaseApp {

    public:

        void setup();
        void update();
        void draw();

        void keyPressed(int key);
        void mousePressed(int x, int y, int button);
        void mouseDragged(int x, int y, int button);

        void audioOut(ofSoundBuffer & buffer);

        void makeSource();
        void soloEffect(int index);
        void drawPanel(float x, float y, const std::string & title);
        void drawShapes(const std::vector<ofPolyline> & shapes, float x, float y, bool colored, bool points);
        void drawWave(const ofSoundBuffer & audio, float x, float y, float w, float h, const ofColor & color);
        glm::vec2 toCanvas(float x, float y) const;

        XYTransformer transformer; // shape -> audio -> effects -> shape
        XYscope player;            // loops the altered audio
        Oscilloscope scope;        // shows it as a beam
        ofSoundStream soundStream;
        bool audioOk;
        double silentFrames;

        ofxPanel gui;

        std::vector<std::string> sourceNames;
        int sourceIndex;
        int generation;
        std::vector<ofPolyline> drawing; // the mouse drawing, in canvas pixels
        std::vector<ofPolyline> source;
        std::vector<ofPolyline> result;
        int soloIndex;

        float canvasSize; // the shapes live on a canvasSize square
        float panelSize;  // and are drawn at panelSize
        glm::vec2 panel1, panel2, panel3;
        std::string status;

};
