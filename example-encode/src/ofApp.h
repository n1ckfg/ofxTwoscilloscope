#pragma once

#include "ofMain.h"

#include "ofxTwoscilloscope.h"

// Example 1: vector shapes -> XYscope format audio.
//
// Shapes drawn into an XYscope come out of the sound card as X (left) and
// Y (right) audio. Plug that into an oscilloscope in X-Y mode, or record it
// and open it in example-decode.
class ofApp : public ofBaseApp {

    public:

        void setup();
        void update();
        void draw();

        void keyPressed(int key);
        void mousePressed(int x, int y, int button);
        void mouseDragged(int x, int y, int button);

        // Draws the current scene into an XYscope. The scope and the time are
        // arguments, so the same drawing can go to the sound card or to an
        // offline export.
        void drawScene(XYscope & scope, float time);
        void exportAnimation(float seconds);

        XYscope xy;
        bool audioOk;

        int scene;
        std::vector<std::string> sceneNames;
        std::vector<std::string> fontNames;
        int fontIndex;
        std::vector<ofPolyline> drawing; // the mouse drawing, in canvas pixels

        float canvasSize;
        std::string status;

};
