#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

The audio-to-vector half of the addon: the rendering pipeline of Hansi
Raber's Oscilloscope app (https://github.com/kritzikratzi/Oscilloscope),
taken out of the app and packed into one class.

Feed it audio from any thread (an XYPlayer, your audioIn() or audioOut(),
an XYscope), then update() and draw() it like any other openFrameworks
object:

    Oscilloscope scope;

    void ofApp::setup() {
        scope.setup(ofGetWidth(), ofGetHeight());
    }

    void ofApp::audioIn(ofSoundBuffer & buffer) {
        scope.addBuffer(buffer);
    }

    void ofApp::update() { scope.update(); }
    void ofApp::draw()   { scope.draw(); }

The channels decide the layout, as they did for the app's audio files:
1 channel draws the signal against a sawtooth sweep, 2 are X and Y, 3 are
X, Y and Z (brightness), 4 are two stereo pairs drawn as a red/cyan
anaglyph.

getShapes() turns the most recent audio back into vector shapes with
XYDecoder.

Differences from the original:
* The app's Globals settings are plain members here.
* Audio is upsampled to the visual sample rate (192kHz by default) with
  StreamResampler instead of FFmpeg's swresample, for any input, not
  just files. That's why live input needs no intensity boost here.
* The mesh is drawn with the openFrameworks matrices into an FBO the size
  of setup(); draw() scales the FBO to fit wherever it's drawn.
* Z is read through zRange. The default (0, 1) shows the app's 0..1
  brightness as-is and blanks XYscope's -1 (beam off) level.
*/

#include "ofMain.h"
#include "OsciMesh.h"
#include "StreamResampler.h"
#include "XYDecoder.h"

class Oscilloscope : public ofBaseSoundInput {

    public:

        enum Layout {
            MONO,              // signal on Y, swept along X by a sawtooth
            STEREO,            // X-Y
            STEREO_ZMODULATED, // X-Y plus brightness
            QUAD               // two X-Y pairs, red and cyan
        };

        Oscilloscope();

        void setup(int width, int height, int visualSampleRate = 192000);
        void resize(int width, int height);

        // ---------------------------------------------------------------- input (any thread)

        void addSamples(const float * interleaved, size_t numFrames, int numChannels, int sampleRate);
        void addBuffer(const ofSoundBuffer & buffer);
        // so an Oscilloscope can listen to an ofSoundStream directly
        void audioIn(ofSoundBuffer & buffer) override { addBuffer(buffer); }
        void clear();

        // ---------------------------------------------------------------- update/draw (main thread)

        void update();
        void draw();
        void draw(float x, float y);
        void draw(float x, float y, float w, float h);
        ofFbo & getFbo() { return fbo; }

        // ---------------------------------------------------------------- vector shapes

        // Decode the most recent audio into shapes on a width x height canvas.
        // Uses the source sample rate; set decoderSettings.freq if you know it.
        std::vector<ofPolyline> getShapes(float width, float height);
        XYDecoderSettings decoderSettings;
        // The last couple of seconds of input at its own sample rate.
        ofSoundBuffer getHistory() const;
        // The loop period found by the last getShapes(), in samples (0 = none).
        float getDetectedPeriod() const { return detectedPeriod; }

        // ---------------------------------------------------------------- settings

        float scale = 1;            // 1 fills the shorter side of the FBO
        bool invertX = false;
        bool invertY = false;
        bool flipXY = false;
        bool zModulation = true;
        glm::vec2 zRange = glm::vec2(0, 1); // Z values for black and full brightness

        float strokeWeight = 10;    // 1..20
        float intensity = 0.4f;     // 0..1
        float afterglow = 0.5f;     // 0..1, how much of each frame is left for the next
        float hue = 50;             // 0..360, 360 is white

        StreamResampler::Interpolation interpolation = StreamResampler::SINC;
        void setVisualSampleRate(int rate);
        int getVisualSampleRate() const { return visualSampleRate; }

        Layout getLayout() const { return layout; }
        int getSourceSampleRate() const { return sourceSampleRate; }
        // how many sample batches were dropped to keep up (the app's "Dropped")
        int getDropped() const { return dropped; }

        OsciMesh mesh;
        OsciMesh mesh2; // the second pair in QUAD layout

    private:

        void configure(int numChannels, int sampleRate);
        void drawMesh();

        int width = 512;
        int height = 512;
        int visualSampleRate = 192000;

        ofFbo fbo;
        bool changed = false;
        bool needsClear = true;
        int dropped = 0;
        float sweep = 0;

        mutable std::mutex mutex;
        Layout layout = STEREO;
        int numChannels = 0;
        int sourceSampleRate = 0;
        StreamResampler::Interpolation activeInterpolation = StreamResampler::SINC;
        std::vector<StreamResampler> resamplers;
        // visual rate samples waiting for update()
        std::vector<std::vector<float>> pending;
        // source rate history for getShapes()
        std::vector<float> history;
        size_t historyFrames = 0;

        float detectedPeriod = 0;

};
