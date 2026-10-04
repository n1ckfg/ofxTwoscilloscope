#pragma once

#include "ofMain.h"
#include "ofxLatk.h"
#include "ofxTwoscilloscope.h"

// Draws a Latk animation through ofxTwoscilloscope, the way its
// example-transform draws a shape: the current frame is projected to the
// screen, encoded as one loop of XY audio, run through the effect chain, and
// drawn back from the altered audio.
//
// The strokes are encoded here rather than by XYscope, so that every sample
// of the loop is known to belong to one stroke. The effects pass Z through
// untouched, so that still holds after them, and each stroke is drawn from
// its own samples in its own colour.
class LatkScopeRenderer {

	public:
		void setup(int sampleRate = 44100);

		// Projects, encodes and transforms the current frame of each layer.
		void update(Latk & latk, const ofCamera & cam, const ofRectangle & viewport);

		// The altered audio drawn by the oscilloscope beam.
		void drawBeams();
		// The altered audio decoded back into strokes.
		void drawStrokes();

		// The decoded strokes, on a canvas the size of the viewport.
		const vector<ofPolyline> & getStrokes();
		float getFreq() const { return freq; }

		// shapes -> audio -> effects -> shapes; add effects to transformer.effects
		XYTransformer transformer;

		ofParameterGroup parameters;
		ofParameter<float> loopFreq;      // Hz: lower gives the drawing more samples
		ofParameter<float> beamSize;      // beam radius, px
		ofParameter<float> beamIntensity; // brightness of a stroke drawn at an even speed

		struct Stats {
			size_t pieces = 0;
			size_t dropped = 0;     // pieces left out because the loop is too short
			size_t samples = 0;     // per loop
			float pathLength = 0;   // px
			float ms = 0;           // projecting, encoding and transforming
		};
		const Stats & getStats() const { return stats; }

	private:
		struct Piece {
			vector<glm::vec2> points; // canvas px
			ofColor color;
			float strokeSize = 4;
			float length = 0;
			size_t start = 0;         // first sample: blanked, on the first point
			size_t lit = 0;           // then this many lit samples, from end to end
		};

		void project(Latk & latk, const ofCamera & cam);
		void encode();
		void buildBeams();
		void decodeStrokes();

		int sampleRate = 44100;
		float freq = 5;
		size_t cycleFrames = 8820;
		ofRectangle canvas;

		vector<Piece> pieces;
		vector<float> x, y, z; // one loop of the altered audio

		struct Beam {
			ofFloatColor color;
			ofMesh mesh;
		};
		OsciMesh osci;
		vector<Beam> beams;
		float beamExposure = 1;
		bool beamsDirty = true;

		vector<ofPolyline> strokes;
		vector<size_t> strokePieces; // the piece each stroke was decoded from
		bool strokesDirty = true;

		Stats stats;

};
