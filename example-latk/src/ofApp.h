#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofxLatk.h"
#include "LatkScopeRenderer.h"

class ofApp : public ofBaseApp {

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

		void soloEffect(int index);

		Latk latk;
		ofEasyCam cam;
		LatkScopeRenderer scope;
		XYscope player; // loops the altered audio out of the sound card
		ofxPanel gui;

		enum View { BEAMS, STROKES, LINES };
		View view = BEAMS;
		int soloIndex = -1;
		bool showGui = true;
		string status;

};
