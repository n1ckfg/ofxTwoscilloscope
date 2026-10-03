#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

Plays an XY audio file to the sound card and feeds the same audio to an
Oscilloscope, which is the job OsciAvAudioPlayer did in the Oscilloscope app:
one stream at the sound card's sample rate, one for the display.

    XYPlayer player;
    Oscilloscope scope;

    void ofApp::setup() {
        scope.setup(ofGetWidth(), ofGetHeight());
        player.load("xyscope.wav");
        player.setScope(&scope);
        player.setLoop(true);
        player.play();
        // ...open an ofSoundStream with this app as its output listener
    }

    void ofApp::audioOut(ofSoundBuffer & buffer) {
        player.audioOut(buffer);
    }

No sound card? Call player.update(ofGetLastFrameTime()) each frame instead
and it plays silently, on the clock.

Differences from the original:
* Files are read whole with WavFile instead of streamed with FFmpeg.
* The scope gets the file's own samples and upsamples them itself, rather
  than the player keeping a second 192kHz stream.
*/

#include "ofMain.h"

class Oscilloscope;

class XYPlayer : public ofBaseSoundOutput {

    public:

        bool load(const std::string & path);
        void setBuffer(const ofSoundBuffer & buffer, const std::string & name = "");
        void unload();

        void setScope(Oscilloscope * scope);

        // from the audio thread: resamples to the buffer's rate and channels
        void audioOut(ofSoundBuffer & buffer) override;
        // without a sound card: advance by this many seconds of playback
        void update(float seconds);

        void play();
        void stop();
        void setPaused(bool paused);
        void setLoop(bool loop);
        void setVolume(float volume);
        void setPosition(float pct);
        void setPositionMS(int ms);

        bool isLoaded() const;
        bool isPlaying() const;
        bool getLoop() const;
        float getPosition() const;
        int getPositionMS() const;
        int getDurationMS() const;
        int getNumChannels() const;
        int getSampleRate() const;
        std::string getFilename() const;
        // the whole file
        ofSoundBuffer getBuffer() const;

        // called when playback reaches the end (on the audio thread!)
        std::function<void()> onEnd = [](){};

    private:

        void feedScope(double from, double to);

        mutable std::mutex mutex;
        ofSoundBuffer sound;
        std::string filename;
        double position = 0; // in file frames
        bool playing = false;
        bool looping = false;
        float volume = 1;
        Oscilloscope * scope = nullptr;

};
