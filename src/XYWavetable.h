#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

A port of XYscope's XYWavetable.java, itself Hansi Raber's fix of Minim's
Wavetable: a float array you can sample with a normalized [0,1] position.

The Java version fixed an ArrayIndexOutOfBoundsException that happened when
the drawing thread replaced the array while the audio thread was reading it.
Here the array lives behind a shared_ptr that gets swapped under a mutex, so
the audio thread can grab the current table once per buffer and keep reading
it safely even if a new one gets swapped in halfway through.

Differences from the Java original:
* The transform methods (scale, smooth, warp...) build a new table and swap
  it in, rather than editing the one the audio thread might be reading.
* smooth() is a true moving average. Minim's divided a window of n+1
  samples by n.
*/

#include "ofMain.h"

class XYWavetable {

    public:

        XYWavetable(size_t size = 0);
        XYWavetable(const std::vector<float> & waveform);

        void setWaveform(const std::vector<float> & waveform);
        void setWaveform(std::vector<float> && waveform);

        // A copy of the current table.
        std::vector<float> getWaveform() const;
        // The current table itself, safe to keep reading on another thread.
        std::shared_ptr<const std::vector<float>> getWaveformPtr() const;

        float get(size_t i) const;
        void set(size_t i, float value);
        size_t size() const;

        // Sample the table at a position in [0,1], with linear interpolation.
        // Positions outside [0,1] wrap around, so does the last sample, which
        // interpolates back to the first one.
        float value(float at) const;
        static float valueAt(const std::vector<float> & wave, double at);

        void scale(float scale);
        void offset(float amount);
        void normalize();
        void invert();
        void flip(float in);
        void addNoise(float sigma);
        void rectify();
        void smooth(int windowLength);
        void warp(float warpPoint, float warpTarget);

    private:

        template<typename F> void modify(F func);

        mutable std::mutex mutex;
        std::shared_ptr<const std::vector<float>> waveform;

};
