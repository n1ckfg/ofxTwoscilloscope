#pragma once

/*
+ + +   ofxTwoscilloscope: vectors to audio, audio to vectors, and back   + + +
+ + +   Nick Fox-Gieg  https://fox-gieg.com                                 + + +

    XYscope xy;           // vector shapes -> XY audio, ported from XYscope (Processing)
    Oscilloscope scope;   // XY audio -> beam rendering + vector shapes, ported from Oscilloscope (oF)
    XYTransformer xform;  // vector shape -> audio -> effects -> new vector shape

    xy.setup();
    xy.openAudioOut();
    ...
    xy.clearWaves();
    xy.circle(256, 256, 200);
    xy.buildWaves();

    scope.setup(512, 512);
    scope.addBuffer(audio);          // from any thread
    scope.update();
    scope.draw();
    auto shapes = scope.getShapes(512, 512);

    xform.setup(512, 512);
    xform.effects.add<XYLowPass>();
    auto altered = xform.transform(shapes);
*/

// encoding: XYscope
#include "XYscope.h"
#include "XYWavetable.h"
#include "HersheyFont.h"

// decoding: Oscilloscope
#include "Oscilloscope.h"
#include "OsciMesh.h"
#include "StreamResampler.h"
#include "XYDecoder.h"
#include "XYPlayer.h"

// transforming
#include "XYEffects.h"
#include "XYTransformer.h"

// files
#include "WavFile.h"
