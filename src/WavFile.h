#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

A minimal, dependency-free RIFF/WAVE reader and writer.

XYscope's recorder writes WAV files, and WAV is how most oscilloscope music
gets passed around, so it's the one format both halves of the addon need.
The original Oscilloscope app decoded audio with FFmpeg (ofxAvCodec); this
replaces that with something that builds everywhere openFrameworks does.

Reads 8/16/24/32-bit PCM and 32/64-bit float files with any number of
channels, including WAVE_FORMAT_EXTENSIBLE headers. Writes 16-bit PCM,
24-bit PCM or 32-bit float.
*/

#include "ofMain.h"

class WavFile {

    public:

        enum Format {
            PCM_16 = 16,
            PCM_24 = 24,
            FLOAT_32 = 32
        };

        // Relative paths are resolved with ofToDataPath().
        static bool load(const std::string & path, ofSoundBuffer & buffer);
        static bool save(const std::string & path, const ofSoundBuffer & buffer, Format format = PCM_16);

};
