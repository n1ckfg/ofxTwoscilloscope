#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

Turns XY audio back into vector shapes: ofPolylines on an XYscope-style
canvas (pixels, y down).

The Oscilloscope app only ever drew the beam. This goes one step further and
recovers the drawing itself, the inverse of XYscope::buildWaves():

1. Find the period: the signal loops at XYscope's freq(), so the shape is one
   cycle long. Given freq it's sampleRate / freq; otherwise it's found with
   the YIN difference function.
2. Take the most recent full cycle and map each sample to the canvas:
   px = (x + 1) / 2 * width,  py = (1 - y) / 2 * height.
3. Cut it into strokes wherever the beam blanks (Z below zThreshold, when
   there's a Z channel) or jumps (a step much longer than the typical one,
   which is XYscope's pen moving from one shape to the next).
4. Join the stroke that runs off the end of the cycle back onto the one at
   the start, since the cycle loops, then simplify each stroke.
*/

#include "ofMain.h"

struct XYDecoderSettings {

    // the canvas the shapes are mapped to
    float width = 512;
    float height = 512;

    float sampleRate = 44100;
    // loop frequency of the signal (XYscope's freq()), 0 to detect it
    float freq = 0;
    // range searched when detecting
    float minFreq = 20;
    float maxFreq = 1000;

    // strokes break where a step is longer than this many pixels...
    float jumpThreshold = 0;
    // ...or, when jumpThreshold is 0, longer than jumpFactor x the median step
    float jumpFactor = 8;

    // with a Z channel, break the strokes where the beam is blanked
    bool useZ = true;
    float zMin = -1;          // XYscope's blanked level
    float zMax = 1;           // XYscope's beam-on level
    float zThreshold = 0.5f;  // 0..1 between zMin and zMax

    // close strokes whose ends are this close (pixels): 0 for 2.5 x the median step, -1 never
    float closeThreshold = 0;

    // Douglas-Peucker tolerance in pixels, 0 keeps every sample
    float simplify = 0.5f;
    // strokes with fewer points than this are dropped
    int minPoints = 2;
    // shorter strokes than this (in pixels) are dropped
    float minLength = 0;

};

class XYDecoder {

    public:

        // Decode the latest full cycle of a signal. z can be nullptr.
        static std::vector<ofPolyline> decode(const float * x, const float * y, const float * z, size_t n,
                                              const XYDecoderSettings & settings);

        // Decode an interleaved buffer: X on channel 0, Y on 1, Z on 2 (if any).
        // Mono buffers decode with the signal on Y, the way the Oscilloscope app draws them.
        static std::vector<ofPolyline> decode(const ofSoundBuffer & buffer, XYDecoderSettings settings);

        // Decode exactly these samples as one loop, no period detection.
        static std::vector<ofPolyline> decodeCycle(const float * x, const float * y, const float * z, size_t n,
                                                   const XYDecoderSettings & settings);

        // Period in samples (fractional), or 0 if nothing periodic was found.
        static float detectPeriod(const float * x, const float * y, size_t n, float minPeriod, float maxPeriod);
        // How well the end of the signal repeats after period samples:
        // 0 for a perfect loop, around 1 for no relation at all.
        static float periodError(const float * x, const float * y, size_t n, float period);

        // Save shapes as an SVG of open paths.
        static bool saveSvg(const std::string & path, const std::vector<ofPolyline> & shapes,
                            float width, float height, const ofColor & stroke = ofColor(0), float strokeWidth = 1);

};
