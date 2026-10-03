#include "Oscilloscope.h"

namespace {

    // How bright the beam is at intensity 1, for audio upsampled to 192kHz.
    // Beam light builds up with every sample drawn, so it's scaled by the
    // visual sample rate to look the same at any rate.
    const float BEAM_GAIN = 0.15f;

    // never queue more than this many seconds of samples for update(), so a
    // slow frame can't snowball into a bigger mesh and an even slower frame
    const float MAX_PENDING_SECONDS = 1.0f / 15.0f;
    // keep this many seconds of input for getShapes()
    const float HISTORY_SECONDS = 2.0f;
    // the window getShapes() searches for a loop
    const float DECODE_SECONDS = 0.5f;

}

//--------------------------------------------------------------
Oscilloscope::Oscilloscope() {
}

//--------------------------------------------------------------
void Oscilloscope::setup(int _width, int _height, int _visualSampleRate) {
    resize(_width, _height);
    setVisualSampleRate(_visualSampleRate);
}

//--------------------------------------------------------------
void Oscilloscope::resize(int _width, int _height) {
    width = std::max(1, _width);
    height = std::max(1, _height);
    needsClear = true;
}

//--------------------------------------------------------------
void Oscilloscope::setVisualSampleRate(int rate) {
    std::lock_guard<std::mutex> lock(mutex);
    visualSampleRate = std::max(8000, rate);
    numChannels = 0; // reconfigure on the next samples
}

//--------------------------------------------------------------
void Oscilloscope::configure(int _numChannels, int sampleRate) {
    numChannels = _numChannels;
    sourceSampleRate = sampleRate;
    activeInterpolation = interpolation;

    switch (numChannels) {
        case 1: layout = MONO; break;
        case 2: layout = STEREO; break;
        case 3: layout = STEREO_ZMODULATED; break;
        default: layout = numChannels >= 4 ? QUAD : STEREO; break;
    }

    int used = std::min(numChannels, 4);
    resamplers.assign(used, StreamResampler());
    for (auto & resampler : resamplers) resampler.setup(sampleRate, visualSampleRate, interpolation);
    pending.assign(used, std::vector<float>());
    history.clear();
    historyFrames = 0;
}

//--------------------------------------------------------------
void Oscilloscope::addSamples(const float * interleaved, size_t numFrames, int _numChannels, int sampleRate) {
    if (interleaved == nullptr || numFrames == 0 || _numChannels <= 0) return;
    if (sampleRate <= 0) sampleRate = 44100;

    std::lock_guard<std::mutex> lock(mutex);
    if (_numChannels != numChannels || sampleRate != sourceSampleRate || interpolation != activeInterpolation) {
        configure(_numChannels, sampleRate);
    }

    // the visual stream, upsampled
    int used = int(resamplers.size());
    for (int c = 0; c < used; c++) {
        resamplers[c].process(interleaved + c, numFrames, _numChannels, pending[c]);
    }

    size_t maxPending = size_t(visualSampleRate * MAX_PENDING_SECONDS);
    if (!pending.empty() && pending[0].size() > maxPending) {
        size_t excess = pending[0].size() - maxPending;
        for (auto & p : pending) p.erase(p.begin(), p.begin() + std::min(excess, p.size()));
        dropped++;
    }

    // the source stream, for decoding shapes
    for (size_t i = 0; i < numFrames; i++) {
        const float * frame = interleaved + i * _numChannels;
        history.insert(history.end(), frame, frame + used);
    }
    historyFrames += numFrames;
    size_t maxFrames = size_t(sampleRate * HISTORY_SECONDS);
    if (historyFrames > 2 * maxFrames) {
        // trim now and then rather than on every buffer
        size_t drop = historyFrames - maxFrames;
        history.erase(history.begin(), history.begin() + drop * used);
        historyFrames = maxFrames;
    }
}

//--------------------------------------------------------------
void Oscilloscope::addBuffer(const ofSoundBuffer & buffer) {
    addSamples(buffer.getBuffer().data(), buffer.getNumFrames(), buffer.getNumChannels(), buffer.getSampleRate());
}

//--------------------------------------------------------------
void Oscilloscope::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto & resampler : resamplers) resampler.reset();
    for (auto & p : pending) p.clear();
    history.clear();
    historyFrames = 0;
    needsClear = true;
}

//--------------------------------------------------------------
void Oscilloscope::update() {
    std::vector<std::vector<float>> samples;
    Layout currentLayout;
    {
        std::lock_guard<std::mutex> lock(mutex);
        samples.swap(pending);
        pending.assign(samples.size(), std::vector<float>());
        currentLayout = layout;
    }

    mesh.clear();
    mesh2.clear();
    mesh.uSize = strokeWeight / 1000.0f;
    mesh2.uSize = mesh.uSize;

    if (samples.empty() || samples[0].empty()) return;
    changed = true;
    int n = int(samples[0].size());

    switch (currentLayout) {
        case MONO: {
            // a sawtooth sweeps the beam across, as on a scope in Y-T mode
            std::vector<float> sweepX(n);
            for (int i = 0; i < n; i++) {
                sweepX[i] = -1 + 2 * sweep;
                sweep += 1.0f / 2048;
                if (sweep >= 1) sweep -= 1;
            }
            mesh.addLines(sweepX.data(), samples[0].data(), nullptr, n);
            break;
        }
        case STEREO:
            mesh.addLines(samples[0].data(), samples[1].data(), nullptr, n);
            break;
        case STEREO_ZMODULATED: {
            std::vector<float> bright;
            if (zModulation) {
                bright.resize(n);
                float range = zRange.y - zRange.x;
                for (int i = 0; i < n; i++) {
                    bright[i] = range != 0 ? ofClamp((samples[2][i] - zRange.x) / range, 0, 1) : 1;
                }
            }
            mesh.addLines(samples[0].data(), samples[1].data(), zModulation ? bright.data() : nullptr, n);
            break;
        }
        case QUAD:
            mesh.addLines(samples[0].data(), samples[1].data(), nullptr, n);
            mesh2.addLines(samples[2].data(), samples[3].data(), nullptr, n);
            break;
    }
}

//--------------------------------------------------------------
void Oscilloscope::drawMesh() {
    ofPushMatrix();
    ofTranslate(width / 2.0f, height / 2.0f);
    float s = std::min(width, height) / 2.0f * scale;
    // scope +Y is up
    ofScale(s * (invertX ? -1 : 1), -s * (invertY ? -1 : 1));
    if (flipXY) {
        ofMultMatrix(glm::mat4(0, 1, 0, 0,
                               1, 0, 0, 0,
                               0, 0, 1, 0,
                               0, 0, 0, 1));
    }

    float gain = intensity * BEAM_GAIN * 192000.0f / visualSampleRate;
    mesh.uIntensity = gain;
    mesh2.uIntensity = gain;

    if (layout == QUAD) {
        mesh.uRgb = glm::vec3(1, 0, 0);
        mesh2.uRgb = glm::vec3(0, 1, 1);
    } else if (hue >= 360) {
        mesh.uRgb = glm::vec3(1, 1, 1);
    } else {
        ofFloatColor col = ofFloatColor::fromHsb(hue / 360.0f, 1, 1);
        mesh.uRgb = glm::vec3(col.r, col.g, col.b);
    }

    mesh.draw();
    mesh2.draw();
    ofPopMatrix();
}

//--------------------------------------------------------------
void Oscilloscope::draw() {
    draw(0, 0, width, height);
}

//--------------------------------------------------------------
void Oscilloscope::draw(float x, float y) {
    draw(x, y, width, height);
}

//--------------------------------------------------------------
void Oscilloscope::draw(float x, float y, float w, float h) {
    if (!fbo.isAllocated() || int(fbo.getWidth()) != width || int(fbo.getHeight()) != height) {
        fbo.allocate(width, height, GL_RGBA);
        needsClear = true;
    }

    if (needsClear) {
        fbo.begin();
        ofClear(0, 255);
        fbo.end();
        needsClear = false;
    }

    if (changed) {
        fbo.begin();
        ofPushStyle();
        // the afterglow: fade what's there, rather than clearing it
        ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
        ofSetColor(0, (1 - ofClamp(afterglow, 0, 1)) * 255);
        ofFill();
        ofDrawRectangle(0, 0, width, height);
        ofPopStyle();
        drawMesh();
        fbo.end();
        changed = false;
    }

    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_DISABLED);
    ofSetColor(255);
    fbo.draw(x, y, w, h);
    ofPopStyle();
}

//--------------------------------------------------------------
ofSoundBuffer Oscilloscope::getHistory() const {
    std::lock_guard<std::mutex> lock(mutex);
    ofSoundBuffer buffer;
    int used = std::max(1, int(resamplers.size()));
    buffer.setNumChannels(used);
    buffer.setSampleRate(sourceSampleRate > 0 ? sourceSampleRate : 44100);
    buffer.getBuffer() = history;
    return buffer;
}

//--------------------------------------------------------------
std::vector<ofPolyline> Oscilloscope::getShapes(float w, float h) {
    XYDecoderSettings s = decoderSettings;
    s.width = w;
    s.height = h;
    s.zMin = zRange.x;
    s.zMax = zRange.y;
    s.useZ = s.useZ && zModulation;

    // copy out only the most recent stretch
    size_t nCh, n;
    std::vector<float> x, y, z;
    {
        std::lock_guard<std::mutex> lock(mutex);
        nCh = std::max(1, int(resamplers.size()));
        s.sampleRate = sourceSampleRate > 0 ? sourceSampleRate : 44100;
        size_t total = history.size() / nCh;
        n = std::min(total, size_t(s.sampleRate * DECODE_SECONDS));
        size_t start = total - n;
        x.resize(n);
        y.resize(n);
        if (nCh == 3) z.resize(n);
        for (size_t i = 0; i < n; i++) {
            const float * frame = &history[(start + i) * nCh];
            if (nCh == 1) {
                y[i] = frame[0];
            } else {
                x[i] = frame[0];
                y[i] = frame[1];
                if (nCh == 3) z[i] = frame[2];
            }
        }
    }
    if (n < 2) return {};

    if (nCh == 1) {
        ofSoundBuffer mono;
        mono.setNumChannels(1);
        mono.setSampleRate(s.sampleRate);
        mono.getBuffer() = y;
        detectedPeriod = 0;
        return XYDecoder::decode(mono, s);
    }

    if (s.freq > 0) {
        detectedPeriod = s.sampleRate / s.freq;
    } else {
        // keep the last period while the signal still loops at it (and not at half of it)
        bool stillLoops = detectedPeriod > 0 &&
            XYDecoder::periodError(x.data(), y.data(), n, detectedPeriod) < 0.02f &&
            XYDecoder::periodError(x.data(), y.data(), n, detectedPeriod / 2) > 0.1f;
        if (!stillLoops) {
            detectedPeriod = XYDecoder::detectPeriod(x.data(), y.data(), n, s.sampleRate / std::max(1.0f, s.maxFreq),
                                                     s.sampleRate / std::max(1.0f, s.minFreq));
        }
        if (detectedPeriod > 0) s.freq = s.sampleRate / detectedPeriod;
    }

    return XYDecoder::decode(x.data(), y.data(), z.empty() ? nullptr : z.data(), n, s);
}
