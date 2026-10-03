#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

Streaming sample rate conversion for one channel of audio.

The Oscilloscope app drew its lines from audio upsampled to a high
"visual" sample rate (192kHz or more) with FFmpeg's swresample, so the beam
follows the band-limited curve between samples instead of cutting straight
across. This does the same job without FFmpeg: SINC is a windowed sinc
(Lanczos, 4 lobes), like swresample's default filter, and LINEAR matches the
app's "interpolate = false" option.
*/

#include "ofMain.h"

class StreamResampler {

    public:

        enum Interpolation {
            LINEAR,
            SINC
        };

        void setup(double inRate, double outRate, Interpolation interpolation = SINC);
        void reset();

        // Converts the next n input samples (every stride-th float of in) and
        // appends the output samples to out.
        void process(const float * in, size_t n, size_t stride, std::vector<float> & out);

        double getInRate() const { return inRate; }
        double getOutRate() const { return outRate; }

    private:

        float kernel(double x) const;

        double inRate = 44100;
        double outRate = 192000;
        Interpolation interpolation = SINC;
        int taps = 4;
        double step = 44100.0 / 192000.0;

        std::vector<float> history;
        double pos = 0;

};
