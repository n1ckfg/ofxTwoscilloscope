#include "XYTransformer.h"

//--------------------------------------------------------------
XYTransformer::XYTransformer() {
    setup(width, height, sampleRate, freq);
}

//--------------------------------------------------------------
void XYTransformer::setup(float _width, float _height, int _sampleRate, float _freq, int waveSize) {
    width = _width;
    height = _height;
    sampleRate = std::max(1000, _sampleRate);
    freq = std::max(0.1f, _freq);

    encoder.setCanvasSize(width, height);
    encoder.sampleRate(sampleRate);
    encoder.waveSize(waveSize);
    encoder.freq(freq);
}

//--------------------------------------------------------------
size_t XYTransformer::getCycleFrames(float f, int sr) const {
    return std::max<size_t>(2, size_t(std::lround(sr / std::max(0.1f, f))));
}

//--------------------------------------------------------------
std::vector<ofPolyline> XYTransformer::transform(const std::vector<ofPolyline> & shapes) {
    encoder.setCanvasSize(width, height);
    encoder.freq(freq);
    encoder.clearWaves();
    encoder.polylines(shapes);
    encoder.buildWaves();
    return transform(encoder);
}

//--------------------------------------------------------------
std::vector<ofPolyline> XYTransformer::transform(const XYscope & scope) {
    float f = scope.freq().x;
    int sr = scope.sampleRate();
    size_t cycle = getCycleFrames(f, sr);
    scope.render(encoded, cycle * (std::max(0, settleCycles) + 1), scope.zAuto() ? 3 : 2);
    return processAndDecode(f, sr, scope.getWidth(), scope.getHeight());
}

//--------------------------------------------------------------
std::vector<ofPolyline> XYTransformer::transform(const ofSoundBuffer & encodedAudio) {
    encoded = encodedAudio;
    int sr = encoded.getSampleRate() > 0 ? encoded.getSampleRate() : sampleRate;
    encoded.setSampleRate(sr);
    return processAndDecode(freq, sr, width, height);
}

//--------------------------------------------------------------
std::vector<ofPolyline> XYTransformer::processAndDecode(float f, int sr, float w, float h) {
    processed = encoded;
    effects.reset();
    effects.process(processed);

    cycleFrames = std::min(getCycleFrames(f, sr), processed.getNumFrames());
    size_t nCh = processed.getNumChannels();
    result.clear();
    if (nCh < 2 || cycleFrames < 2) return result;

    std::vector<float> x, y, z;
    getProcessedCycle(x, y, z);

    XYDecoderSettings s = decoder;
    s.width = w;
    s.height = h;
    s.sampleRate = sr;
    s.freq = f;
    result = XYDecoder::decodeCycle(x.data(), y.data(), z.empty() ? nullptr : z.data(), x.size(), s);
    return result;
}

//--------------------------------------------------------------
void XYTransformer::getProcessedCycle(std::vector<float> & x, std::vector<float> & y, std::vector<float> & z) const {
    x.clear();
    y.clear();
    z.clear();
    size_t nCh = processed.getNumChannels();
    size_t n = processed.getNumFrames();
    size_t m = std::min(cycleFrames, n);
    if (nCh < 2 || m == 0) return;

    size_t start = n - m;
    x.resize(m);
    y.resize(m);
    if (nCh >= 3) z.resize(m);
    for (size_t i = 0; i < m; i++) {
        x[i] = processed[(start + i) * nCh];
        y[i] = processed[(start + i) * nCh + 1];
        if (nCh >= 3) z[i] = processed[(start + i) * nCh + 2];
    }
}

//--------------------------------------------------------------
ofSoundBuffer XYTransformer::getProcessedCycle() const {
    ofSoundBuffer cycle;
    size_t nCh = processed.getNumChannels();
    size_t n = processed.getNumFrames();
    size_t m = std::min(cycleFrames, n);
    cycle.setNumChannels(std::max<size_t>(1, nCh));
    cycle.setSampleRate(processed.getSampleRate());
    if (m > 0) {
        cycle.getBuffer().assign(processed.getBuffer().end() - m * nCh, processed.getBuffer().end());
    }
    return cycle;
}
