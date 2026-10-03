#include "XYWavetable.h"

//--------------------------------------------------------------
XYWavetable::XYWavetable(size_t size) {
    waveform = std::make_shared<const std::vector<float>>(size, 0.0f);
}

//--------------------------------------------------------------
XYWavetable::XYWavetable(const std::vector<float> & _waveform) {
    waveform = std::make_shared<const std::vector<float>>(_waveform);
}

//--------------------------------------------------------------
void XYWavetable::setWaveform(const std::vector<float> & _waveform) {
    auto next = std::make_shared<const std::vector<float>>(_waveform);
    std::lock_guard<std::mutex> lock(mutex);
    waveform = next;
}

//--------------------------------------------------------------
void XYWavetable::setWaveform(std::vector<float> && _waveform) {
    auto next = std::make_shared<const std::vector<float>>(std::move(_waveform));
    std::lock_guard<std::mutex> lock(mutex);
    waveform = next;
}

//--------------------------------------------------------------
std::vector<float> XYWavetable::getWaveform() const {
    return *getWaveformPtr();
}

//--------------------------------------------------------------
std::shared_ptr<const std::vector<float>> XYWavetable::getWaveformPtr() const {
    std::lock_guard<std::mutex> lock(mutex);
    return waveform;
}

//--------------------------------------------------------------
float XYWavetable::get(size_t i) const {
    auto wave = getWaveformPtr();
    return i < wave->size() ? (*wave)[i] : 0.0f;
}

//--------------------------------------------------------------
void XYWavetable::set(size_t i, float value) {
    modify([&](std::vector<float> & w) {
        if (i < w.size()) w[i] = value;
    });
}

//--------------------------------------------------------------
size_t XYWavetable::size() const {
    return getWaveformPtr()->size();
}

//--------------------------------------------------------------
float XYWavetable::value(float at) const {
    return valueAt(*getWaveformPtr(), at);
}

//--------------------------------------------------------------
float XYWavetable::valueAt(const std::vector<float> & wave, double at) {
    size_t n = wave.size();
    if (n == 0) return 0;
    double wrapped = at - std::floor(at);
    double whichSample = n * wrapped;

    // linearly interpolate between the two samples we want
    size_t lowSamp = size_t(whichSample) % n;
    size_t hiSamp = (lowSamp + 1) % n;
    float rem = float(whichSample - std::floor(whichSample));

    return wave[lowSamp] + rem * (wave[hiSamp] - wave[lowSamp]);
}

//--------------------------------------------------------------
template<typename F>
void XYWavetable::modify(F func) {
    std::vector<float> next = getWaveform();
    func(next);
    setWaveform(std::move(next));
}

//--------------------------------------------------------------
void XYWavetable::scale(float scale) {
    modify([&](std::vector<float> & w) {
        for (float & v : w) v *= scale;
    });
}

//--------------------------------------------------------------
void XYWavetable::offset(float amount) {
    modify([&](std::vector<float> & w) {
        for (float & v : w) v += amount;
    });
}

//--------------------------------------------------------------
void XYWavetable::normalize() {
    modify([&](std::vector<float> & w) {
        float max = 0;
        for (float v : w) max = std::max(max, std::abs(v));
        if (max > 0) {
            for (float & v : w) v /= max;
        }
    });
}

//--------------------------------------------------------------
void XYWavetable::invert() {
    flip(0);
}

//--------------------------------------------------------------
void XYWavetable::flip(float in) {
    modify([&](std::vector<float> & w) {
        for (float & v : w) v = in - (v - in);
    });
}

//--------------------------------------------------------------
void XYWavetable::addNoise(float sigma) {
    modify([&](std::vector<float> & w) {
        static std::mt19937 rng(std::random_device{}());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (float & v : w) v += dist(rng) * sigma;
    });
}

//--------------------------------------------------------------
void XYWavetable::rectify() {
    modify([&](std::vector<float> & w) {
        for (float & v : w) v = std::abs(v);
    });
}

//--------------------------------------------------------------
void XYWavetable::smooth(int windowLength) {
    if (windowLength < 1) return;
    modify([&](std::vector<float> & w) {
        std::vector<float> temp = w;
        for (size_t i = windowLength; i < w.size(); i++) {
            float avg = 0;
            for (size_t j = i - windowLength; j <= i; j++) {
                avg += temp[j];
            }
            w[i] = avg / (windowLength + 1);
        }
    });
}

//--------------------------------------------------------------
void XYWavetable::warp(float warpPoint, float warpTarget) {
    modify([&](std::vector<float> & w) {
        std::vector<float> source = w;
        for (size_t s = 0; s < w.size(); s++) {
            float lookup = float(s) / w.size();
            if (lookup <= warpTarget) {
                // normalize look up to [0,warpTarget], expand to [0,warpPoint]
                lookup = warpTarget > 0 ? (lookup / warpTarget) * warpPoint : 0;
            } else {
                // map (warpTarget,1] to (warpPoint,1]
                lookup = warpPoint + (1 - (1 - lookup) / (1 - warpTarget)) * (1 - warpPoint);
            }
            w[s] = valueAt(source, lookup);
        }
    });
}
