#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

Audio effects for XY signals: the tools XYTransformer uses to turn one
vector shape into another by treating the shape as sound.

Each effect works on X (channel 0) and Y (channel 1) and passes Z through
untouched, so a time-based effect moves the beam relative to its blanking,
as it would if only X and Y went through a real effects unit. Every setting
is an ofParameter, so an effect (or a whole chain) can go straight into an
ofxGui panel.

What they do to a shape:
    XYLowPass       rounds corners and swallows small detail
    XYHighPass      AC coupling: shapes sag and smear, like a cheap sound card
    XYChannelDelay  delays X or Y, shearing the shape and opening lines into loops
    XYEcho          ghost copies from earlier in the loop, blended in
    XYBitCrush      snaps the beam to a coarse grid
    XYSampleHold    lowers the sample rate: steps, corners and stray dots
    XYDrive         tanh saturation pushes shapes out towards a rounded square
    XYWavefold      folds the signal back at the edges, a kaleidoscope
    XYRingMod       multiplies by a sine: shapes pulse in and out of the center
    XYNoise         jitter (seeded, so the same settings give the same shape)
    XYRotate        mixes X and Y with a rotation matrix, optionally spinning
*/

#include "ofMain.h"

class XYEffect {

    public:

        XYEffect(const std::string & name);
        virtual ~XYEffect() = default;

        // Process channels 0 and 1 of an interleaved buffer in place.
        // Does nothing while the effect is disabled.
        void process(ofSoundBuffer & buffer);
        // Clear filter memory, delay lines and oscillators.
        virtual void reset() {}

        std::string getName() const { return parameters.getName(); }

        ofParameterGroup parameters;
        ofParameter<bool> enabled;

    protected:

        // Called once per buffer before the samples, with the buffer's sample rate.
        virtual void prepare(float sampleRate) {}
        virtual void processFrame(float & x, float & y) = 0;

        float sampleRate = 44100;

};

//--------------------------------------------------------------
// Second order filter from Robert Bristow-Johnson's Audio EQ Cookbook.
class XYBiquad {

    public:

        void lowPass(double cutoff, double q, double sampleRate);
        void highPass(double cutoff, double q, double sampleRate);
        void reset() { x1 = x2 = y1 = y2 = 0; }
        float process(float x);

    private:

        void set(double b0, double b1, double b2, double a0, double a1, double a2);

        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

};

//--------------------------------------------------------------
// A delay line read with linear interpolation, for fractional delays.
class XYDelayLine {

    public:

        void setup(size_t maxSamples);
        void reset();
        void write(float x);
        // delay in samples, at least 1
        float read(float delay) const;

    private:

        std::vector<float> buffer;
        size_t writeIndex = 0;

};

//--------------------------------------------------------------
class XYLowPass : public XYEffect {

    public:

        XYLowPass();
        void reset() override;
        ofParameter<float> cutoff;
        ofParameter<float> resonance;

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        XYBiquad filterX, filterY;

};

//--------------------------------------------------------------
class XYHighPass : public XYEffect {

    public:

        XYHighPass();
        void reset() override;
        ofParameter<float> cutoff;
        ofParameter<float> resonance;

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        XYBiquad filterX, filterY;

};

//--------------------------------------------------------------
class XYChannelDelay : public XYEffect {

    public:

        XYChannelDelay();
        void reset() override;
        ofParameter<float> delayX; // milliseconds
        ofParameter<float> delayY;

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        XYDelayLine lineX, lineY;
        float samplesX = 1, samplesY = 1;

};

//--------------------------------------------------------------
class XYEcho : public XYEffect {

    public:

        XYEcho();
        void reset() override;
        ofParameter<float> time; // milliseconds
        ofParameter<float> feedback;
        ofParameter<float> mix;

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        XYDelayLine lineX, lineY;
        float samples = 1;

};

//--------------------------------------------------------------
class XYBitCrush : public XYEffect {

    public:

        XYBitCrush();
        ofParameter<float> bits;

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        float levels = 8;

};

//--------------------------------------------------------------
class XYSampleHold : public XYEffect {

    public:

        XYSampleHold();
        void reset() override;
        ofParameter<float> rate; // Hz

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        double phase = 1;
        double step = 0.1;
        float heldX = 0, heldY = 0;

};

//--------------------------------------------------------------
class XYDrive : public XYEffect {

    public:

        XYDrive();
        ofParameter<float> gain;

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        float norm = 1;

};

//--------------------------------------------------------------
class XYWavefold : public XYEffect {

    public:

        XYWavefold();
        ofParameter<float> gain;

    protected:

        void processFrame(float & x, float & y) override;

};

//--------------------------------------------------------------
class XYRingMod : public XYEffect {

    public:

        XYRingMod();
        void reset() override;
        ofParameter<float> freq; // Hz
        ofParameter<float> depth;

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        double phase = 0;
        double step = 0;

};

//--------------------------------------------------------------
class XYNoise : public XYEffect {

    public:

        XYNoise();
        void reset() override;
        ofParameter<float> amount;
        ofParameter<int> seed;

    protected:

        void processFrame(float & x, float & y) override;
        std::mt19937 rng;
        std::uniform_real_distribution<float> dist{-1.0f, 1.0f};

};

//--------------------------------------------------------------
class XYRotate : public XYEffect {

    public:

        XYRotate();
        void reset() override;
        ofParameter<float> angle; // degrees
        ofParameter<float> spin;  // degrees per second

    protected:

        void prepare(float sampleRate) override;
        void processFrame(float & x, float & y) override;
        double time = 0;
        double dt = 0;

};

//--------------------------------------------------------------
class XYEffectChain {

    public:

        XYEffectChain();

        template<typename T>
        std::shared_ptr<T> add() {
            auto effect = std::make_shared<T>();
            add(effect);
            return effect;
        }
        void add(std::shared_ptr<XYEffect> effect);
        void clear();

        // run every enabled effect, in order
        void process(ofSoundBuffer & buffer);
        void reset();

        std::vector<std::shared_ptr<XYEffect>> effects;
        ofParameterGroup parameters;

};
