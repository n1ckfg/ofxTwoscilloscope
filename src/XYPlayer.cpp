#include "XYPlayer.h"
#include "Oscilloscope.h"
#include "WavFile.h"

//--------------------------------------------------------------
bool XYPlayer::load(const std::string & path) {
    ofSoundBuffer loaded;
    if (!WavFile::load(path, loaded)) return false;
    setBuffer(loaded, ofFilePath::getFileName(path));
    return true;
}

//--------------------------------------------------------------
void XYPlayer::setBuffer(const ofSoundBuffer & buffer, const std::string & name) {
    std::lock_guard<std::mutex> lock(mutex);
    sound = buffer;
    if (sound.getSampleRate() == 0) sound.setSampleRate(44100);
    filename = name;
    position = 0;
    if (scope) scope->clear();
}

//--------------------------------------------------------------
void XYPlayer::unload() {
    std::lock_guard<std::mutex> lock(mutex);
    sound.clear();
    filename.clear();
    position = 0;
    playing = false;
}

//--------------------------------------------------------------
void XYPlayer::setScope(Oscilloscope * _scope) {
    std::lock_guard<std::mutex> lock(mutex);
    scope = _scope;
}

//--------------------------------------------------------------
void XYPlayer::feedScope(double from, double to) {
    if (!scope) return;
    size_t n = sound.getNumFrames();
    size_t a = size_t(std::max(0.0, std::floor(from)));
    size_t b = std::min(n, size_t(std::max(0.0, std::floor(to))));
    if (b <= a) return;
    size_t nCh = sound.getNumChannels();
    scope->addSamples(&sound.getBuffer()[a * nCh], b - a, nCh, sound.getSampleRate());
}

//--------------------------------------------------------------
void XYPlayer::audioOut(ofSoundBuffer & buffer) {
    bool ended = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        buffer.set(0);
        size_t n = sound.getNumFrames();
        if (!playing || n == 0) return;

        size_t inCh = sound.getNumChannels();
        size_t outCh = buffer.getNumChannels();
        double outRate = buffer.getSampleRate() > 0 ? buffer.getSampleRate() : sound.getSampleRate();
        double step = sound.getSampleRate() / outRate;
        const std::vector<float> & in = sound.getBuffer();
        std::vector<float> & out = buffer.getBuffer();

        double start = position;
        for (size_t i = 0; i < buffer.getNumFrames(); i++) {
            if (position >= n) {
                feedScope(start, n);
                if (!looping) {
                    playing = false;
                    ended = true;
                    position = n;
                    break;
                }
                position -= n;
                start = 0;
            }

            size_t i0 = size_t(position);
            size_t i1 = i0 + 1 < n ? i0 + 1 : (looping ? 0 : i0);
            float frac = float(position - i0);
            for (size_t c = 0; c < outCh; c++) {
                // mono files go to every channel, extra file channels (Z) only to the scope
                size_t src = inCh == 1 ? 0 : c;
                if (src >= inCh) continue;
                float a = in[i0 * inCh + src];
                float b = in[i1 * inCh + src];
                out[i * outCh + c] = (a + frac * (b - a)) * volume;
            }
            position += step;
        }
        if (!ended) feedScope(start, position);
    }
    if (ended) onEnd();
}

//--------------------------------------------------------------
void XYPlayer::update(float seconds) {
    bool ended = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        size_t n = sound.getNumFrames();
        if (!playing || n == 0 || seconds <= 0) return;

        double target = position + seconds * sound.getSampleRate();
        while (target >= n) {
            feedScope(position, n);
            if (!looping) {
                playing = false;
                ended = true;
                position = n;
                break;
            }
            target -= n;
            position = 0;
        }
        if (!ended) {
            feedScope(position, target);
            position = target;
        }
    }
    if (ended) onEnd();
}

//--------------------------------------------------------------
void XYPlayer::play() {
    std::lock_guard<std::mutex> lock(mutex);
    if (sound.getNumFrames() == 0) return;
    if (position >= sound.getNumFrames()) position = 0;
    playing = true;
}

//--------------------------------------------------------------
void XYPlayer::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    playing = false;
}

//--------------------------------------------------------------
void XYPlayer::setPaused(bool paused) {
    if (paused) stop();
    else play();
}

//--------------------------------------------------------------
void XYPlayer::setLoop(bool loop) {
    std::lock_guard<std::mutex> lock(mutex);
    looping = loop;
}

//--------------------------------------------------------------
void XYPlayer::setVolume(float _volume) {
    std::lock_guard<std::mutex> lock(mutex);
    volume = _volume;
}

//--------------------------------------------------------------
void XYPlayer::setPosition(float pct) {
    std::lock_guard<std::mutex> lock(mutex);
    position = ofClamp(pct, 0, 1) * sound.getNumFrames();
}

//--------------------------------------------------------------
void XYPlayer::setPositionMS(int ms) {
    std::lock_guard<std::mutex> lock(mutex);
    position = ofClamp(ms / 1000.0 * sound.getSampleRate(), 0, sound.getNumFrames());
}

//--------------------------------------------------------------
bool XYPlayer::isLoaded() const {
    std::lock_guard<std::mutex> lock(mutex);
    return sound.getNumFrames() > 0;
}

//--------------------------------------------------------------
bool XYPlayer::isPlaying() const {
    std::lock_guard<std::mutex> lock(mutex);
    return playing;
}

//--------------------------------------------------------------
bool XYPlayer::getLoop() const {
    std::lock_guard<std::mutex> lock(mutex);
    return looping;
}

//--------------------------------------------------------------
float XYPlayer::getPosition() const {
    std::lock_guard<std::mutex> lock(mutex);
    size_t n = sound.getNumFrames();
    return n > 0 ? float(position / n) : 0;
}

//--------------------------------------------------------------
int XYPlayer::getPositionMS() const {
    std::lock_guard<std::mutex> lock(mutex);
    return sound.getSampleRate() > 0 ? int(position * 1000.0 / sound.getSampleRate()) : 0;
}

//--------------------------------------------------------------
int XYPlayer::getDurationMS() const {
    std::lock_guard<std::mutex> lock(mutex);
    return sound.getSampleRate() > 0 ? int(sound.getNumFrames() * 1000.0 / sound.getSampleRate()) : 0;
}

//--------------------------------------------------------------
int XYPlayer::getNumChannels() const {
    std::lock_guard<std::mutex> lock(mutex);
    return sound.getNumChannels();
}

//--------------------------------------------------------------
int XYPlayer::getSampleRate() const {
    std::lock_guard<std::mutex> lock(mutex);
    return sound.getSampleRate();
}

//--------------------------------------------------------------
std::string XYPlayer::getFilename() const {
    std::lock_guard<std::mutex> lock(mutex);
    return filename;
}

//--------------------------------------------------------------
ofSoundBuffer XYPlayer::getBuffer() const {
    std::lock_guard<std::mutex> lock(mutex);
    return sound;
}
