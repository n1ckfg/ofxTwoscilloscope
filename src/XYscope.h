#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

A port of XYscope.java by Ted Davis (https://teddavis.org/xyscope), the
vector-to-audio half of the addon.

Drawing commands don't draw to the screen. They collect shapes, and
buildWaves() turns those shapes into wavetables that loop at freq() Hz:
X on the left channel, Y on the right, and an optional Z (beam blanking)
channel. Play them through a DC-coupled sound card into an oscilloscope in
X-Y mode, a modded Vectrex or a laser, and the shapes appear on the display.

    XYscope xy;

    void ofApp::setup() {
        xy.setup();          // canvas = window size, 44.1kHz, 512 sample waves
        xy.openAudioOut();   // default sound card
    }

    void ofApp::draw() {
        xy.clearWaves();
        xy.circle(ofGetWidth() / 2, ofGetHeight() / 2, ofGetHeight() / 2);
        xy.buildWaves();
        xy.drawXY();         // preview of what the scope will show
    }

Coordinates work like Processing's: pixels on a canvas (the window, by
default) with y pointing down. "XYscope format" audio maps the canvas to
-1..1, with +Y up: x = 2 * px / width - 1, y = 1 - 2 * py / height.

Differences from the Java original:
* Minim is replaced by ofSoundStream. openAudioOut() opens a stream that
  XYscope owns, or call audioOut() from your own ofApp::audioOut().
  audioOutAdd() mixes into a buffer instead of overwriting it, which is how
  several XYscopes patch together for additive synthesis.
* Z goes out as a third channel on the same device, rather than on a
  second sound card. render() and the recorder write it as a 3-channel WAV,
  the Z-modulated layout the Oscilloscope app reads.
* Processing's screenX()/screenY() are replaced by a transform stack of
  XYscope's own (pushMatrix, translate, rotate...). It projects 3D points
  the way Processing's default P3D camera does, so 2D drawing is unchanged
  and 3D shapes need no camera setup.
* render() renders audio offline and process() runs the oscillators
  without a sound card. Both work headless.
* Laser RGB mode (5-channel laser DACs) isn't ported.
*/

#include "ofMain.h"
#include "XYWavetable.h"
#include "HersheyFont.h"

class XYscope : public ofBaseSoundOutput {

    public:

        XYscope();
        ~XYscope();

        // ---------------------------------------------------------------- setup

        // width/height of 0 use the window size.
        void setup(float width = 0, float height = 0, int sampleRate = 44100, int bufferSize = 512);

        // Open a sound card output that this XYscope owns and fills.
        // deviceId comes from listDevices(), -1 for the default device.
        // Use 3 channels to send Z on the third.
        bool openAudioOut(int deviceId = -1, int numChannels = 2);
        void closeAudioOut();
        bool isAudioOutOpen() const { return audioOutOpen; }
        void listDevices() const;

        // Fill an audio buffer: X -> channel 0, Y -> channel 1, Z -> channel 2.
        void audioOut(ofSoundBuffer & buffer) override;
        // The same, but adds to what's already in the buffer.
        void audioOutAdd(ofSoundBuffer & buffer);
        // Run the oscillators for this many seconds without a sound card,
        // feeding the recorder and the drawXY()/drawWave() previews.
        void process(float seconds, int numChannels = 2);

        // Render audio offline. Starts from phase 0 every time and doesn't
        // touch the live oscillators, so it can run while audio is playing.
        ofSoundBuffer render(float seconds, int numChannels = 2) const;
        void render(ofSoundBuffer & buffer, size_t numFrames, int numChannels = 2) const;

        void setCanvasSize(float width, float height);
        float getWidth() const { return xyWidth; }
        float getHeight() const { return xyHeight; }

        int sampleRate() const { return sampleRateVal; }
        void sampleRate(int sampleRateVal);
        int bufferSize() const { return bufferSizeVal; }
        void bufferSize(int bufferSizeVal);

        // ---------------------------------------------------------------- waves

        void clearWaves();
        void buildWaves();
        // Waveforms in -1..1, as built by buildWaves(). Z is optional.
        void setWaveforms(const std::vector<float> & x, const std::vector<float> & y, const std::vector<float> & z = {});
        // Custom waveforms in 0..1, resampled to waveSize().
        void buildX(const std::vector<float> & wave);
        void buildY(const std::vector<float> & wave);
        void buildZ(const std::vector<float> & wave);

        int waveSize() const { return waveSizeVal; }
        void waveSize(int newSize);
        int steps() const { return stepsSize; }
        void steps(float newSteps);
        int limitPoints() const { return limitPointsVal; }
        void limitPoints(int newLimit);
        float limitPath() const { return limitVal; }
        void limitPath(float newLimit);
        void waveReset();
        void resetWaves() { waveReset(); }

        XYWavetable tableX;
        XYWavetable tableY;
        XYWavetable tableZ;

        // ---------------------------------------------------------------- oscillators

        glm::vec3 freq() const { return freqVal; }
        void freq(float newFreq);
        void freq(float newFreqX, float newFreqY);
        void freq(float newFreqX, float newFreqY, float newFreqZ);
        void freq(const glm::vec3 & newFreq);

        glm::vec3 amp() const { return ampVal; }
        void amp(float newAmp);
        void amp(float newAmpX, float newAmpY);
        void amp(float newAmpX, float newAmpY, float newAmpZ);
        void amp(const glm::vec3 & newAmp);

        // Equal power pan for the X and Y oscillators, -1 (left) to 1 (right).
        // The default (-1, 1) sends X left and Y right; (1, -1) swaps them.
        void pan(float panX, float panY);

        // Z output for beam on (zMax) and blanked (zMin). Swap for inverted Z inputs.
        glm::vec2 zRange() const { return glm::vec2(zaxisMin, zaxisMax); }
        void zRange(float zMin, float zMax);
        bool zAuto() const { return useZ; }
        void zAuto(bool zAutoBool) { useZ = zAutoBool; }

        // ---------------------------------------------------------------- vectrex

        // Match the canvas to a modded Vectrex: 310 x 410, rotation 0, 90 or -90.
        void vectrex(int rotation = 0);
        void vectrex(float width, float height, float initAmp, int rotation);
        float vectrexRatio() const { return vectrexAmp; }
        void vectrexRatio(float ratio);

        // ---------------------------------------------------------------- shapes

        void beginShape();
        void vertex(float x, float y);
        void vertex(float x, float y, float z);
        void vertex(const glm::vec2 & p);
        void vertex(const glm::vec3 & p);
        // Sent as a normal vertex, as in XYscope.
        void curveVertex(float x, float y) { vertex(x, y); }
        void curveVertex(float x, float y, float z) { vertex(x, y, z); }
        void endShape(bool close = false);

        void point(float x, float y);
        void point(float x, float y, float z);
        void line(float x1, float y1, float x2, float y2);
        void line(float x1, float y1, float z1, float x2, float y2, float z2);
        void rect(float x, float y, float w);
        void rect(float x, float y, float w, float h);
        void square(float x, float y, float extent) { rect(x, y, extent, extent); }
        void rectMode(ofRectMode mode) { rectM = mode; }
        void ellipse(float x, float y, float d) { ellipse(x, y, d, d); }
        void ellipse(float x, float y, float w, float h);
        void circle(float x, float y, float d) { ellipse(x, y, d, d); }
        int ellipseDetail() const { return ellipseDetailVal; }
        void ellipseDetail(int detail) { ellipseDetailVal = std::max(3, std::abs(detail)); }
        void lissajous(float x, float y, float radius, float ratioA, float ratioB, float phase, float resolution);
        void box(float size) { box(size, size, size); }
        void box(float w, float h, float d);
        void sphere(float r, int detail = 24) { ellipsoid(r, r, r, detail, detail); }
        void ellipsoid(float rx, float ry, float rz, int detailW = 24, int detailH = 24);
        void torus(float radius, float tubeRadius, int detailX = 24, int detailY = 24);

        // openFrameworks shapes
        void polyline(const ofPolyline & polyline);
        void polylines(const std::vector<ofPolyline> & polylines);
        void path(const ofPath & path);

        // ---------------------------------------------------------------- transforms

        void pushMatrix();
        void popMatrix();
        void resetMatrix();
        void translate(float x, float y, float z = 0);
        // angles in radians, as in Processing
        void rotate(float angle) { rotateZ(angle); }
        void rotateX(float angle);
        void rotateY(float angle);
        void rotateZ(float angle);
        void scale(float s) { scale(s, s, s); }
        void scale(float x, float y, float z = 1);
        // Perspective for 3D points, on by default. Off flattens z.
        void perspective(bool usePerspective) { usePerspectiveVal = usePerspective; }

        // ---------------------------------------------------------------- text

        static const std::vector<std::string> & fonts() { return HersheyFont::getFontNames(); }
        bool textFont(const std::string & fontName);
        // size is the height of a capital letter; it also resets the leading
        float textSize() const { return textSizeVal; }
        void textSize(float size);
        float textLeading() const { return textLeadingVal; }
        void textLeading(float leading) { textLeadingVal = leading; }
        void textAlign(ofAlignHorz alignX, ofAlignVert alignY = OF_ALIGN_VERT_TOP);
        void text(const std::string & s, float x, float y);
        float textWidth(const std::string & s) const;
        std::vector<ofPolyline> textPaths(const std::string & s, float x, float y) const;
        HersheyFont & getFont() { return font; }

        // ---------------------------------------------------------------- inspection

        // Shapes as normalized 0..1 points. z is 1 on a shape's last point,
        // where the beam blanks, as in XYscope.
        const std::vector<std::vector<glm::vec3>> & getShapes() const { return shapes; }
        // Shapes in canvas pixels.
        std::vector<ofPolyline> getPolylines() const;
        std::vector<glm::vec3> wavePoints() const;
        // The last buffer the oscillators produced.
        ofSoundBuffer getLastBuffer() const;

        // ---------------------------------------------------------------- drawing

        void drawAll();
        void drawPath(const ofColor & color = ofColor(255));
        void drawPoints(const ofColor & color = ofColor(0, 255, 0));
        // The output signal plotted X against Y, like the scope will show it.
        void drawXY(const ofColor & color = ofColor(50, 255, 50));
        // The wavetables: X in the top half, Y in the bottom, Z through the middle.
        void drawWaveform(const ofColor & colorX = ofColor(50, 50, 255), const ofColor & colorY = ofColor(255, 50, 50));
        // The output signal over time: left channel on top, right below.
        void drawWave(const ofColor & color = ofColor(255));
        bool debugView() const { return debugWave; }
        void debugView(bool debug) { debugWave = debug; }

        // ---------------------------------------------------------------- recording

        // Record the output to bin/data/<name>_<timestamp>.wav
        void recorderBegin(const std::string & name = "XYscope");
        std::string recorderEnd();
        bool isRecording() const { return recording; }

    private:

        struct Oscillators {
            double phaseX = 0;
            double phaseY = 0;
            double phaseZ = 0;
        };

        struct Params {
            glm::vec3 freq;
            glm::vec3 amp;
            glm::vec2 pan;
            bool useZ;
            float zMax;
        };

        Params getParams() const;
        void synth(ofSoundBuffer & buffer, bool add, Oscillators & oscs) const;
        void finishBuffer(const ofSoundBuffer & buffer);
        size_t previewFrames() const;
        void vertexAdd(const glm::vec3 & p);
        glm::vec2 project(const glm::vec3 & p, bool & valid) const;
        void vertexRect(float x, float y, float w, float h);
        void emptyWave();

        float xyWidth = 512;
        float xyHeight = 512;
        int sampleRateVal = 44100;
        int bufferSizeVal = 512;
        int waveSizeVal = 512;
        int stepsSize = 24;

        bool useLimitPoints = false;
        int limitPointsVal = 512;
        bool useLimitPath = false;
        float limitVal = 1;

        glm::vec3 freqVal = glm::vec3(50);
        glm::vec3 ampVal = glm::vec3(1);
        glm::vec2 panVal = glm::vec2(-1, 1);
        bool useZ = true;
        float zaxisMin = -1;
        float zaxisMax = 1;

        bool useVectrex = false;
        float vectrexAmp = 0.82f;
        float vectrexAmpInit = 0.6f;
        int vectrexRotation = 0;

        ofRectMode rectM = OF_RECTMODE_CORNER;
        int ellipseDetailVal = 30;
        bool debugWave = false;

        std::vector<std::vector<glm::vec3>> shapes;
        bool shapeOpen = false;

        std::vector<glm::mat4> matrixStack;
        glm::mat4 matrix = glm::mat4(1);
        bool usePerspectiveVal = true;

        HersheyFont font;
        float textSizeVal = HersheyFont::CAP_HEIGHT;
        float textLeadingVal = HersheyFont::CAP_HEIGHT * 1.5f;
        ofAlignHorz textAlignX = OF_ALIGN_HORZ_LEFT;
        ofAlignVert textAlignY = OF_ALIGN_VERT_TOP;

        // live output, shared with the audio thread
        mutable std::mutex paramMutex;
        std::mutex oscMutex;
        Oscillators liveOscs;
        double processRemainder = 0;
        mutable std::mutex audioMutex;
        ofSoundBuffer lastBuffer;
        bool recording = false;
        std::string recordingPath;
        ofSoundBuffer recordBuffer;

        ofSoundStream soundStream;
        bool audioOutOpen = false;
        int audioDeviceId = -1;
        int audioChannels = 2;

};
