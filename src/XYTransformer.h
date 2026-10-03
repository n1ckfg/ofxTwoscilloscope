#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

The new feature, and the reason the two halves of the addon live together:
turn a vector shape into a new vector shape by passing it through sound.

    shape --XYscope--> XY audio --XYEffects--> altered audio --XYDecoder--> new shape

    XYTransformer transformer;

    void ofApp::setup() {
        transformer.setup(ofGetWidth(), ofGetHeight());
        transformer.effects.add<XYLowPass>()->cutoff = 800;
        transformer.effects.add<XYChannelDelay>();
    }

    ...
    std::vector<ofPolyline> altered = transformer.transform(shapes);

The shapes are encoded exactly as XYscope would send them to a scope (one
loop of freq() Hz), run through the effect chain for a few loops so filters
and echoes settle into a steady state, and the last loop is decoded back
into polylines on the same canvas.

The altered audio is available too: getProcessedCycle() is one loop of it,
ready for XYscope::setWaveforms(), so you can hear (or put on a real scope)
exactly the shape you see.
*/

#include "ofMain.h"
#include "XYscope.h"
#include "XYEffects.h"
#include "XYDecoder.h"

class XYTransformer {

    public:

        XYTransformer();

        void setup(float width, float height, int sampleRate = 44100, float freq = 50, int waveSize = 512);

        // shapes in canvas pixels -> altered shapes in canvas pixels
        std::vector<ofPolyline> transform(const std::vector<ofPolyline> & shapes);
        // whatever an XYscope has built with buildWaves(), at its freq and canvas size
        std::vector<ofPolyline> transform(const XYscope & scope);
        // audio that's already XYscope format, looping at getFreq()
        std::vector<ofPolyline> transform(const ofSoundBuffer & encodedAudio);

        XYEffectChain effects;
        XYDecoderSettings decoder;
        // loops rendered before the one that's decoded
        int settleCycles = 4;

        const std::vector<ofPolyline> & getResult() const { return result; }
        const ofSoundBuffer & getEncodedAudio() const { return encoded; }
        const ofSoundBuffer & getProcessedAudio() const { return processed; }
        // the last loop of the processed audio: -1..1 waves for XYscope::setWaveforms()
        void getProcessedCycle(std::vector<float> & x, std::vector<float> & y, std::vector<float> & z) const;
        ofSoundBuffer getProcessedCycle() const;

        // the XYscope that encodes shapes (set its steps(), zRange()... here)
        XYscope & getEncoder() { return encoder; }

        float getWidth() const { return width; }
        float getHeight() const { return height; }
        float getFreq() const { return freq; }
        int getSampleRate() const { return sampleRate; }

    private:

        std::vector<ofPolyline> processAndDecode(float freq, int sampleRate, float width, float height);
        size_t getCycleFrames(float freq, int sampleRate) const;

        XYscope encoder;
        ofSoundBuffer encoded;
        ofSoundBuffer processed;
        std::vector<ofPolyline> result;

        float width = 512;
        float height = 512;
        int sampleRate = 44100;
        float freq = 50;
        size_t cycleFrames = 882;

};
