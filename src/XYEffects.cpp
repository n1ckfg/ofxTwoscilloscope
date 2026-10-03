#include "XYEffects.h"

//==============================================================
// XYEffect
//==============================================================

//--------------------------------------------------------------
XYEffect::XYEffect(const std::string & name) {
    parameters.setName(name);
    parameters.add(enabled.set("enabled", true));
}

//--------------------------------------------------------------
void XYEffect::process(ofSoundBuffer & buffer) {
    if (!enabled) return;
    size_t nCh = buffer.getNumChannels();
    if (nCh == 0) return;

    sampleRate = buffer.getSampleRate() > 0 ? buffer.getSampleRate() : 44100;
    prepare(sampleRate);

    std::vector<float> & samples = buffer.getBuffer();
    float dummy = 0;
    for (size_t i = 0; i < buffer.getNumFrames(); i++) {
        float & x = samples[i * nCh];
        float & y = nCh > 1 ? samples[i * nCh + 1] : dummy;
        processFrame(x, y);
    }
}

//==============================================================
// building blocks
//==============================================================

//--------------------------------------------------------------
void XYBiquad::set(double _b0, double _b1, double _b2, double a0, double _a1, double _a2) {
    b0 = _b0 / a0;
    b1 = _b1 / a0;
    b2 = _b2 / a0;
    a1 = _a1 / a0;
    a2 = _a2 / a0;
}

//--------------------------------------------------------------
void XYBiquad::lowPass(double cutoff, double q, double sampleRate) {
    double w0 = TWO_PI * ofClamp(cutoff, 1, sampleRate * 0.49) / sampleRate;
    double alpha = std::sin(w0) / (2 * std::max(0.05, q));
    double c = std::cos(w0);
    set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

//--------------------------------------------------------------
void XYBiquad::highPass(double cutoff, double q, double sampleRate) {
    double w0 = TWO_PI * ofClamp(cutoff, 1, sampleRate * 0.49) / sampleRate;
    double alpha = std::sin(w0) / (2 * std::max(0.05, q));
    double c = std::cos(w0);
    set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

//--------------------------------------------------------------
float XYBiquad::process(float x) {
    double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return float(y);
}

//--------------------------------------------------------------
void XYDelayLine::setup(size_t maxSamples) {
    if (buffer.size() != maxSamples + 2) {
        buffer.assign(maxSamples + 2, 0.0f);
        writeIndex = 0;
    }
}

//--------------------------------------------------------------
void XYDelayLine::reset() {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

//--------------------------------------------------------------
void XYDelayLine::write(float x) {
    if (buffer.empty()) return;
    buffer[writeIndex] = x;
    writeIndex = (writeIndex + 1) % buffer.size();
}

//--------------------------------------------------------------
float XYDelayLine::read(float delay) const {
    size_t n = buffer.size();
    if (n < 2) return 0;
    delay = ofClamp(delay, 1, n - 1);
    // the newest sample sits just behind writeIndex
    double pos = double(writeIndex) - delay;
    while (pos < 0) pos += n;
    size_t i0 = size_t(pos) % n;
    size_t i1 = (i0 + 1) % n;
    float frac = float(pos - std::floor(pos));
    return buffer[i0] + frac * (buffer[i1] - buffer[i0]);
}

//==============================================================
// effects
//==============================================================

//--------------------------------------------------------------
XYLowPass::XYLowPass() : XYEffect("low pass") {
    parameters.add(cutoff.set("cutoff", 2000, 20, 20000));
    parameters.add(resonance.set("resonance", 0.707f, 0.3f, 10));
}

void XYLowPass::reset() {
    filterX.reset();
    filterY.reset();
}

void XYLowPass::prepare(float sr) {
    filterX.lowPass(cutoff, resonance, sr);
    filterY.lowPass(cutoff, resonance, sr);
}

void XYLowPass::processFrame(float & x, float & y) {
    x = filterX.process(x);
    y = filterY.process(y);
}

//--------------------------------------------------------------
XYHighPass::XYHighPass() : XYEffect("high pass") {
    parameters.add(cutoff.set("cutoff", 60, 1, 2000));
    parameters.add(resonance.set("resonance", 0.707f, 0.3f, 10));
}

void XYHighPass::reset() {
    filterX.reset();
    filterY.reset();
}

void XYHighPass::prepare(float sr) {
    filterX.highPass(cutoff, resonance, sr);
    filterY.highPass(cutoff, resonance, sr);
}

void XYHighPass::processFrame(float & x, float & y) {
    x = filterX.process(x);
    y = filterY.process(y);
}

//--------------------------------------------------------------
XYChannelDelay::XYChannelDelay() : XYEffect("channel delay") {
    parameters.add(delayX.set("delay x (ms)", 0, 0, 20));
    parameters.add(delayY.set("delay y (ms)", 2, 0, 20));
}

void XYChannelDelay::reset() {
    lineX.reset();
    lineY.reset();
}

void XYChannelDelay::prepare(float sr) {
    size_t maxSamples = size_t(std::ceil(delayX.getMax() / 1000.0f * sr)) + 2;
    lineX.setup(maxSamples);
    lineY.setup(maxSamples);
    samplesX = delayX / 1000.0f * sr;
    samplesY = delayY / 1000.0f * sr;
}

void XYChannelDelay::processFrame(float & x, float & y) {
    lineX.write(x);
    lineY.write(y);
    // the newest sample is a delay of 1, so shift by one to make 0 ms a straight pass
    if (samplesX > 0) x = lineX.read(samplesX + 1);
    if (samplesY > 0) y = lineY.read(samplesY + 1);
}

//--------------------------------------------------------------
XYEcho::XYEcho() : XYEffect("echo") {
    parameters.add(time.set("time (ms)", 5, 0.1f, 1000));
    parameters.add(feedback.set("feedback", 0.5f, 0, 0.95f));
    parameters.add(mix.set("mix", 0.5f, 0, 1));
}

void XYEcho::reset() {
    lineX.reset();
    lineY.reset();
}

void XYEcho::prepare(float sr) {
    size_t maxSamples = size_t(std::ceil(time.getMax() / 1000.0f * sr)) + 2;
    lineX.setup(maxSamples);
    lineY.setup(maxSamples);
    samples = std::max(1.0f, time / 1000.0f * sr);
}

void XYEcho::processFrame(float & x, float & y) {
    float dx = lineX.read(samples);
    float dy = lineY.read(samples);
    lineX.write(x + feedback * dx);
    lineY.write(y + feedback * dy);
    x = (1 - mix) * x + mix * dx;
    y = (1 - mix) * y + mix * dy;
}

//--------------------------------------------------------------
XYBitCrush::XYBitCrush() : XYEffect("bit crush") {
    parameters.add(bits.set("bits", 4, 1, 16));
}

void XYBitCrush::prepare(float sr) {
    levels = std::pow(2.0f, bits - 1);
}

void XYBitCrush::processFrame(float & x, float & y) {
    x = std::round(x * levels) / levels;
    y = std::round(y * levels) / levels;
}

//--------------------------------------------------------------
XYSampleHold::XYSampleHold() : XYEffect("sample & hold") {
    parameters.add(rate.set("rate (Hz)", 2000, 50, 48000));
}

void XYSampleHold::reset() {
    phase = 1;
    heldX = heldY = 0;
}

void XYSampleHold::prepare(float sr) {
    step = rate / sr;
}

void XYSampleHold::processFrame(float & x, float & y) {
    if (phase >= 1) {
        phase -= std::floor(phase);
        heldX = x;
        heldY = y;
    }
    phase += step;
    x = heldX;
    y = heldY;
}

//--------------------------------------------------------------
XYDrive::XYDrive() : XYEffect("drive") {
    parameters.add(gain.set("gain", 3, 1, 20));
}

void XYDrive::prepare(float sr) {
    norm = 1.0f / std::tanh(float(gain));
}

void XYDrive::processFrame(float & x, float & y) {
    x = std::tanh(gain * x) * norm;
    y = std::tanh(gain * y) * norm;
}

//--------------------------------------------------------------
XYWavefold::XYWavefold() : XYEffect("wavefold") {
    parameters.add(gain.set("gain", 2, 1, 8));
}

void XYWavefold::processFrame(float & x, float & y) {
    x = std::sin(gain * x * HALF_PI);
    y = std::sin(gain * y * HALF_PI);
}

//--------------------------------------------------------------
XYRingMod::XYRingMod() : XYEffect("ring mod") {
    parameters.add(freq.set("freq (Hz)", 150, 0.1f, 2000));
    parameters.add(depth.set("depth", 0.5f, 0, 1));
}

void XYRingMod::reset() {
    phase = 0;
}

void XYRingMod::prepare(float sr) {
    step = freq / sr;
}

void XYRingMod::processFrame(float & x, float & y) {
    float m = 1 - depth + depth * float(std::sin(TWO_PI * phase));
    phase += step;
    phase -= std::floor(phase);
    x *= m;
    y *= m;
}

//--------------------------------------------------------------
XYNoise::XYNoise() : XYEffect("noise") {
    parameters.add(amount.set("amount", 0.02f, 0, 0.5f));
    parameters.add(seed.set("seed", 1, 0, 1000));
    reset();
}

void XYNoise::reset() {
    rng.seed(seed);
}

void XYNoise::processFrame(float & x, float & y) {
    x += dist(rng) * amount;
    y += dist(rng) * amount;
}

//--------------------------------------------------------------
XYRotate::XYRotate() : XYEffect("rotate") {
    parameters.add(angle.set("angle", 30, -180, 180));
    parameters.add(spin.set("spin (deg/s)", 0, -3600, 3600));
}

void XYRotate::reset() {
    time = 0;
}

void XYRotate::prepare(float sr) {
    dt = 1.0 / sr;
}

void XYRotate::processFrame(float & x, float & y) {
    double theta = ofDegToRad(angle + spin * time);
    time += dt;
    float c = std::cos(theta), s = std::sin(theta);
    float rx = x * c - y * s;
    float ry = x * s + y * c;
    x = rx;
    y = ry;
}

//==============================================================
// XYEffectChain
//==============================================================

//--------------------------------------------------------------
XYEffectChain::XYEffectChain() {
    parameters.setName("effects");
}

//--------------------------------------------------------------
void XYEffectChain::add(std::shared_ptr<XYEffect> effect) {
    if (!effect) return;
    effects.push_back(effect);
    parameters.add(effect->parameters);
}

//--------------------------------------------------------------
void XYEffectChain::clear() {
    effects.clear();
    parameters.clear();
}

//--------------------------------------------------------------
void XYEffectChain::process(ofSoundBuffer & buffer) {
    for (auto & effect : effects) effect->process(buffer);
}

//--------------------------------------------------------------
void XYEffectChain::reset() {
    for (auto & effect : effects) effect->reset();
}
