#include "XYscope.h"
#include "WavFile.h"

//--------------------------------------------------------------
XYscope::XYscope() {
    tableX.setWaveform(std::vector<float>(waveSizeVal, 0.0f));
    tableY.setWaveform(std::vector<float>(waveSizeVal, 0.0f));
    tableZ.setWaveform(std::vector<float>(waveSizeVal, zaxisMax));
}

//--------------------------------------------------------------
XYscope::~XYscope() {
    closeAudioOut();
}

//--------------------------------------------------------------
void XYscope::setup(float width, float height, int _sampleRate, int _bufferSize) {
    setCanvasSize(width > 0 ? width : ofGetWidth(), height > 0 ? height : ofGetHeight());
    sampleRateVal = _sampleRate;
    bufferSizeVal = std::max(16, _bufferSize);
    waveSize(bufferSizeVal);
    limitPointsVal = bufferSizeVal;
    ofLogNotice("XYscope") << "XYscope 3.0.0 for openFrameworks - https://teddavis.org/xyscope";
}

//--------------------------------------------------------------
bool XYscope::openAudioOut(int deviceId, int numChannels) {
    closeAudioOut();

    ofSoundStreamSettings settings;
    settings.numOutputChannels = std::max(1, numChannels);
    settings.numInputChannels = 0;
    settings.sampleRate = sampleRateVal;
    settings.bufferSize = bufferSizeVal;
    settings.numBuffers = 4;
    settings.setOutListener(this);

    if (deviceId >= 0) {
        for (const auto & device : soundStream.getDeviceList()) {
            if (device.deviceID == deviceId) {
                settings.setOutDevice(device);
                break;
            }
        }
    }

    audioDeviceId = deviceId;
    audioChannels = settings.numOutputChannels;
    audioOutOpen = soundStream.setup(settings);
    if (!audioOutOpen) {
        ofLogWarning("XYscope") << "couldn't open an audio output, use process() to run without one";
    }
    return audioOutOpen;
}

//--------------------------------------------------------------
void XYscope::closeAudioOut() {
    if (audioOutOpen) {
        soundStream.close();
        audioOutOpen = false;
    }
}

//--------------------------------------------------------------
void XYscope::listDevices() const {
    ofSoundStream stream;
    for (const auto & device : stream.getDeviceList()) {
        if (device.outputChannels > 0) {
            ofLogNotice("XYscope") << device.deviceID << " = " << device.name << " (" << device.outputChannels << " out)";
        }
    }
}

//--------------------------------------------------------------
void XYscope::setCanvasSize(float width, float height) {
    xyWidth = std::max(1.0f, width);
    xyHeight = std::max(1.0f, height);
}

//--------------------------------------------------------------
void XYscope::sampleRate(int _sampleRate) {
    sampleRateVal = _sampleRate;
    if (audioOutOpen) openAudioOut(audioDeviceId, audioChannels);
}

//--------------------------------------------------------------
void XYscope::bufferSize(int _bufferSize) {
    if (_bufferSize > 16) bufferSizeVal = _bufferSize;
    if (audioOutOpen) openAudioOut(audioDeviceId, audioChannels);
}

//==============================================================
// audio
//==============================================================

//--------------------------------------------------------------
XYscope::Params XYscope::getParams() const {
    std::lock_guard<std::mutex> lock(paramMutex);
    return { freqVal, ampVal, panVal, useZ, zaxisMax };
}

//--------------------------------------------------------------
void XYscope::synth(ofSoundBuffer & buffer, bool add, Oscillators & o) const {
    Params params = getParams();
    auto waveX = tableX.getWaveformPtr();
    auto waveY = tableY.getWaveformPtr();
    auto waveZ = tableZ.getWaveformPtr();

    size_t nCh = buffer.getNumChannels();
    size_t nFrames = buffer.getNumFrames();
    double sr = buffer.getSampleRate() > 0 ? buffer.getSampleRate() : sampleRateVal;
    std::vector<float> & out = buffer.getBuffer();

    // Minim's Pan: equal power, -1 is all left and 1 is all right
    float thetaX = (params.pan.x + 1) * PI / 4;
    float thetaY = (params.pan.y + 1) * PI / 4;
    float lx = std::cos(thetaX), rx = std::sin(thetaX);
    float ly = std::cos(thetaY), ry = std::sin(thetaY);

    double stepX = params.freq.x / sr;
    double stepY = params.freq.y / sr;
    double stepZ = params.freq.z / sr;

    for (size_t i = 0; i < nFrames; i++) {
        float x = params.amp.x * XYWavetable::valueAt(*waveX, o.phaseX);
        float y = params.amp.y * XYWavetable::valueAt(*waveY, o.phaseY);
        // no Z wave means the beam stays on
        float z = params.useZ && !waveZ->empty() ? params.amp.z * XYWavetable::valueAt(*waveZ, o.phaseZ) : params.zMax;

        o.phaseX += stepX;
        o.phaseY += stepY;
        o.phaseZ += stepZ;
        o.phaseX -= std::floor(o.phaseX);
        o.phaseY -= std::floor(o.phaseY);
        o.phaseZ -= std::floor(o.phaseZ);

        float * frame = &out[i * nCh];
        float values[3] = { x * lx + y * ly, x * rx + y * ry, z };
        for (size_t c = 0; c < nCh && c < 3; c++) {
            frame[c] = add ? frame[c] + values[c] : values[c];
        }
        if (!add) {
            for (size_t c = 3; c < nCh; c++) frame[c] = 0;
        }
    }
}

//--------------------------------------------------------------
size_t XYscope::previewFrames() const {
    Params params = getParams();
    float slowest = std::max(1.0f, std::min(params.freq.x, params.freq.y));
    return std::max<size_t>(bufferSizeVal, std::min<size_t>(sampleRateVal, std::ceil(sampleRateVal / slowest)));
}

//--------------------------------------------------------------
void XYscope::finishBuffer(const ofSoundBuffer & buffer) {
    size_t keep = previewFrames();
    std::lock_guard<std::mutex> lock(audioMutex);

    // keep at least one full cycle for drawXY() and drawWave()
    size_t nCh = buffer.getNumChannels();
    std::vector<float> & last = lastBuffer.getBuffer();
    if (lastBuffer.getNumChannels() != nCh) {
        last.clear();
        lastBuffer.setNumChannels(nCh);
    }
    last.insert(last.end(), buffer.getBuffer().begin(), buffer.getBuffer().end());
    if (last.size() > keep * nCh) {
        last.erase(last.begin(), last.end() - keep * nCh);
    }
    lastBuffer.setSampleRate(buffer.getSampleRate());

    if (recording) {
        if (recordBuffer.size() == 0) {
            recordBuffer.setNumChannels(nCh);
            recordBuffer.setSampleRate(buffer.getSampleRate());
        }
        if (recordBuffer.getNumChannels() == nCh) {
            auto & rec = recordBuffer.getBuffer();
            rec.insert(rec.end(), buffer.getBuffer().begin(), buffer.getBuffer().end());
        }
    }
}

//--------------------------------------------------------------
void XYscope::audioOut(ofSoundBuffer & buffer) {
    {
        std::lock_guard<std::mutex> lock(oscMutex);
        synth(buffer, false, liveOscs);
    }
    finishBuffer(buffer);
}

//--------------------------------------------------------------
void XYscope::audioOutAdd(ofSoundBuffer & buffer) {
    {
        std::lock_guard<std::mutex> lock(oscMutex);
        synth(buffer, true, liveOscs);
    }
    finishBuffer(buffer);
}

//--------------------------------------------------------------
void XYscope::process(float seconds, int numChannels) {
    double frames = seconds * sampleRateVal + processRemainder;
    size_t numFrames = size_t(std::max(0.0, std::floor(frames)));
    processRemainder = frames - numFrames;
    if (numFrames == 0) return;

    ofSoundBuffer buffer;
    buffer.allocate(numFrames, std::max(1, numChannels));
    buffer.setSampleRate(sampleRateVal);
    audioOut(buffer);
}

//--------------------------------------------------------------
ofSoundBuffer XYscope::render(float seconds, int numChannels) const {
    ofSoundBuffer buffer;
    render(buffer, size_t(std::max(0.0f, seconds) * sampleRateVal), numChannels);
    return buffer;
}

//--------------------------------------------------------------
void XYscope::render(ofSoundBuffer & buffer, size_t numFrames, int numChannels) const {
    buffer.allocate(numFrames, std::max(1, numChannels));
    buffer.setSampleRate(sampleRateVal);
    Oscillators oscs;
    synth(buffer, false, oscs);
}

//--------------------------------------------------------------
ofSoundBuffer XYscope::getLastBuffer() const {
    std::lock_guard<std::mutex> lock(audioMutex);
    return lastBuffer;
}

//==============================================================
// waves
//==============================================================

//--------------------------------------------------------------
void XYscope::clearWaves() {
    shapes.clear();
    shapeOpen = false;
    // Processing resets the matrix at the start of every draw(), and XYscope
    // drew through Processing's matrix, so start each frame from scratch too.
    resetMatrix();
}

//--------------------------------------------------------------
void XYscope::buildWaves() {
    if (shapeOpen) endShape();
    if (shapes.empty()) {
        emptyWave();
        return;
    }

    // waveform gen v4 (mar 2020): spread each shape's segments over the
    // wave in proportion to their length, so the beam moves at an even speed
    size_t totalPoints = 0;
    double totalDist = 0;
    for (const auto & shape : shapes) {
        totalPoints += shape.size();
        for (size_t j = 0; j + 1 < shape.size(); j++) {
            totalDist += glm::distance(glm::vec2(shape[j]), glm::vec2(shape[j + 1]));
        }
    }
    double waveSizeD = double(totalPoints) * stepsSize;

    // x, y and a blank flag on each shape's last point
    std::vector<glm::vec3> waveCol;
    for (const auto & shape : shapes) {
        for (size_t j = 0; j + 1 < shape.size(); j++) {
            glm::vec2 p1(shape[j]);
            glm::vec2 p2(shape[j + 1]);
            double lineDist = glm::distance(p1, p2);
            long secPer = std::lround(1 + (totalDist > 0 ? lineDist / totalDist * waveSizeD : 0));
            bool lastSegment = j + 2 == shape.size();
            for (long k = 0; k <= secPer; k++) {
                glm::vec2 seg = glm::mix(p1, p2, float(double(k) / secPer));
                waveCol.emplace_back(seg, (lastSegment && k == secPer) ? 1.0f : 0.0f);
            }
        }
    }

    size_t n = waveSizeVal;
    size_t m = waveCol.size();
    std::vector<float> mfx(n), mfy(n), mfz(n);
    for (size_t i = 0; i < n; i++) {
        size_t from = i * m / n;
        size_t to = std::max(from + 1, (i + 1) * m / n);
        const glm::vec3 & tc = waveCol[from];
        mfx[i] = tc.x * 2 - 1;
        mfy[i] = tc.y * -2 + 1;

        // blank if any point that falls in this sample ends a shape,
        // so the blanking can't get skipped over by the resampling
        bool blank = false;
        for (size_t k = from; k < to && k < m; k++) blank = blank || waveCol[k].z == 1.0f;
        mfz[i] = blank ? zaxisMin : zaxisMax;

        if (useVectrex) {
            float tfxx = mfx[i];
            float tfyy = mfy[i];
            if (vectrexRotation == 90) {
                mfx[i] = tfyy;
                mfy[i] = -tfxx;
            } else if (vectrexRotation == -90) {
                mfx[i] = -tfyy;
                mfy[i] = tfxx;
            } else {
                mfx[i] = -tfxx;
                mfy[i] = -tfyy;
            }
        }
    }

    setWaveforms(mfx, mfy, mfz);
}

//--------------------------------------------------------------
void XYscope::setWaveforms(const std::vector<float> & mfx, const std::vector<float> & mfy, const std::vector<float> & mfz) {
    if (useLimitPoints && (int(mfx.size()) > limitPointsVal || int(mfy.size()) > limitPointsVal)) {
        auto limit = [&](const std::vector<float> & src) {
            std::vector<float> dst;
            if (src.empty()) return dst;
            dst.resize(limitPointsVal);
            for (int i = 0; i < limitPointsVal; i++) {
                dst[i] = src[size_t(i) * src.size() / limitPointsVal];
            }
            return dst;
        };
        tableX.setWaveform(limit(mfx));
        tableY.setWaveform(limit(mfy));
        if (useZ) tableZ.setWaveform(limit(mfz));
    } else {
        tableX.setWaveform(mfx);
        tableY.setWaveform(mfy);
        if (useZ) tableZ.setWaveform(mfz);
    }
}

//--------------------------------------------------------------
void XYscope::emptyWave() {
    tableX.setWaveform(std::vector<float>());
    tableY.setWaveform(std::vector<float>());
    if (useZ) tableZ.setWaveform(std::vector<float>());
}

//--------------------------------------------------------------
void XYscope::buildX(const std::vector<float> & wave) {
    if (wave.empty()) return;
    std::vector<float> out(waveSizeVal);
    for (int i = 0; i < waveSizeVal; i++) {
        out[i] = ofMap(wave[size_t(i) * wave.size() / waveSizeVal], 0, 1, -1, 1);
    }
    tableX.setWaveform(std::move(out));
}

//--------------------------------------------------------------
void XYscope::buildY(const std::vector<float> & wave) {
    if (wave.empty()) return;
    std::vector<float> out(waveSizeVal);
    for (int i = 0; i < waveSizeVal; i++) {
        out[i] = ofMap(wave[size_t(i) * wave.size() / waveSizeVal], 0, 1, 1, -1);
    }
    tableY.setWaveform(std::move(out));
}

//--------------------------------------------------------------
void XYscope::buildZ(const std::vector<float> & wave) {
    if (wave.empty()) return;
    std::vector<float> out(waveSizeVal);
    for (int i = 0; i < waveSizeVal; i++) {
        out[i] = ofMap(wave[size_t(i) * wave.size() / waveSizeVal], 0, 1, zaxisMin, zaxisMax);
    }
    tableZ.setWaveform(std::move(out));
}

//--------------------------------------------------------------
void XYscope::waveSize(int newSize) {
    waveSizeVal = std::max(2, newSize);
}

//--------------------------------------------------------------
void XYscope::steps(float newSteps) {
    stepsSize = std::max(1, int(newSteps));
}

//--------------------------------------------------------------
void XYscope::limitPoints(int newLimit) {
    if (newLimit == 0) {
        useLimitPoints = false;
    } else {
        limitPointsVal = std::abs(newLimit);
        useLimitPoints = true;
    }
}

//--------------------------------------------------------------
void XYscope::limitPath(float newLimit) {
    limitVal = newLimit;
    useLimitPath = true;
}

//--------------------------------------------------------------
void XYscope::waveReset() {
    std::lock_guard<std::mutex> lock(oscMutex);
    liveOscs = Oscillators();
}

//==============================================================
// oscillators
//==============================================================

//--------------------------------------------------------------
void XYscope::freq(float newFreq) {
    freq(glm::vec3(newFreq));
}

//--------------------------------------------------------------
void XYscope::freq(float newFreqX, float newFreqY) {
    freq(glm::vec3(newFreqX, newFreqY, freqVal.z));
}

//--------------------------------------------------------------
void XYscope::freq(float newFreqX, float newFreqY, float newFreqZ) {
    freq(glm::vec3(newFreqX, newFreqY, newFreqZ));
}

//--------------------------------------------------------------
void XYscope::freq(const glm::vec3 & newFreq) {
    std::lock_guard<std::mutex> lock(paramMutex);
    freqVal = newFreq;
}

//--------------------------------------------------------------
void XYscope::amp(float newAmp) {
    amp(newAmp, newAmp, newAmp);
}

//--------------------------------------------------------------
void XYscope::amp(float newAmpX, float newAmpY) {
    amp(newAmpX, newAmpY, ampVal.z);
}

//--------------------------------------------------------------
void XYscope::amp(float newAmpX, float newAmpY, float newAmpZ) {
    std::lock_guard<std::mutex> lock(paramMutex);
    ampVal.x = ofClamp(newAmpX, 0, 1);
    if (useVectrex) ampVal.x *= vectrexAmp;
    ampVal.y = ofClamp(newAmpY, 0, 1);
    ampVal.z = ofClamp(newAmpZ, 0, 1);
}

//--------------------------------------------------------------
void XYscope::amp(const glm::vec3 & newAmp) {
    amp(newAmp.x, newAmp.y, newAmp.z);
}

//--------------------------------------------------------------
void XYscope::pan(float panX, float panY) {
    std::lock_guard<std::mutex> lock(paramMutex);
    panVal = glm::vec2(ofClamp(panX, -1, 1), ofClamp(panY, -1, 1));
}

//--------------------------------------------------------------
void XYscope::zRange(float zMin, float zMax) {
    std::lock_guard<std::mutex> lock(paramMutex);
    zaxisMin = zMin;
    zaxisMax = zMax;
}

//==============================================================
// vectrex
//==============================================================

//--------------------------------------------------------------
void XYscope::vectrex(int rotation) {
    if (rotation == 90 || rotation == -90) {
        vectrex(410, 310, vectrexAmpInit, rotation);
    } else {
        vectrex(310, 410, vectrexAmpInit, 0);
    }
}

//--------------------------------------------------------------
void XYscope::vectrex(float width, float height, float initAmp, int rotation) {
    useVectrex = true;
    vectrexRotation = rotation;
    setCanvasSize(width, height);
    vectrexAmpInit = initAmp;
    amp(vectrexAmpInit);
}

//--------------------------------------------------------------
void XYscope::vectrexRatio(float ratio) {
    vectrexAmp = ofClamp(ratio, 0, 1);
    amp(vectrexAmpInit);
}

//==============================================================
// shapes
//==============================================================

//--------------------------------------------------------------
void XYscope::beginShape() {
    if (shapeOpen) endShape();
    shapes.emplace_back();
    shapeOpen = true;
}

//--------------------------------------------------------------
void XYscope::vertex(float x, float y) {
    vertexAdd(glm::vec3(x, y, 0));
}

//--------------------------------------------------------------
void XYscope::vertex(float x, float y, float z) {
    vertexAdd(glm::vec3(x, y, z));
}

//--------------------------------------------------------------
void XYscope::vertex(const glm::vec2 & p) {
    vertexAdd(glm::vec3(p, 0));
}

//--------------------------------------------------------------
void XYscope::vertex(const glm::vec3 & p) {
    vertexAdd(p);
}

//--------------------------------------------------------------
glm::vec2 XYscope::project(const glm::vec3 & p, bool & valid) const {
    glm::vec4 t = matrix * glm::vec4(p, 1);
    valid = true;
    if (!usePerspectiveVal || t.z == 0) return glm::vec2(t.x, t.y);

    // Processing's default P3D camera: the eye sits in front of the middle
    // of the canvas, far enough back that z = 0 maps 1:1 to pixels
    float cameraZ = (xyHeight / 2) / std::tan(PI / 6);
    float depth = cameraZ - t.z;
    if (depth < cameraZ * 0.1f) {
        valid = false; // closer than Processing's near plane
        return glm::vec2(t.x, t.y);
    }
    float s = cameraZ / depth;
    return glm::vec2(xyWidth / 2 + (t.x - xyWidth / 2) * s,
                     xyHeight / 2 + (t.y - xyHeight / 2) * s);
}

//--------------------------------------------------------------
void XYscope::vertexAdd(const glm::vec3 & p) {
    if (!shapeOpen) beginShape();

    bool valid;
    glm::vec2 screen = project(p, valid);
    if (useLimitPath) {
        valid = valid && screen.x >= limitVal && screen.x <= xyWidth - limitVal &&
                screen.y >= limitVal && screen.y <= xyHeight - limitVal;
    }

    if (valid) {
        shapes.back().emplace_back(screen.x / xyWidth, screen.y / xyHeight, 0);
    } else {
        // break the shape where it leaves the canvas
        endShape();
        beginShape();
    }
}

//--------------------------------------------------------------
void XYscope::endShape(bool close) {
    if (!shapeOpen || shapes.empty()) return;
    auto & shape = shapes.back();
    if (close && !shape.empty()) {
        glm::vec3 first = shape.front();
        shape.emplace_back(first.x, first.y, 0);
    }
    if (shape.size() > 1) {
        shape.back().z = 1; // the beam blanks here, on its way to the next shape
    } else {
        shapes.pop_back();
    }
    shapeOpen = false;
}

//--------------------------------------------------------------
void XYscope::point(float x, float y) {
    line(x, y, x + 1, y + 1);
}

//--------------------------------------------------------------
void XYscope::point(float x, float y, float z) {
    line(x, y, z, x + 1, y + 1, z + 1);
}

//--------------------------------------------------------------
void XYscope::line(float x1, float y1, float x2, float y2) {
    beginShape();
    vertex(x1, y1);
    vertex(x2, y2);
    endShape();
}

//--------------------------------------------------------------
void XYscope::line(float x1, float y1, float z1, float x2, float y2, float z2) {
    beginShape();
    vertex(x1, y1, z1);
    vertex(x2, y2, z2);
    endShape();
}

//--------------------------------------------------------------
void XYscope::rect(float x, float y, float w) {
    rect(x, y, w, w);
}

//--------------------------------------------------------------
void XYscope::rect(float x, float y, float w, float h) {
    if (rectM == OF_RECTMODE_CENTER) {
        x -= w / 2;
        y -= h / 2;
    }
    vertexRect(x, y, w, h);
}

//--------------------------------------------------------------
void XYscope::vertexRect(float x1, float y1, float w1, float h1) {
    beginShape();
    vertex(x1, y1);
    vertex(x1 + w1, y1);
    vertex(x1 + w1, y1 + h1);
    vertex(x1, y1 + h1);
    vertex(x1, y1);
    endShape();
}

//--------------------------------------------------------------
// based on http://stackoverflow.com/questions/5886628/effecient-way-to-draw-ellipse-with-opengl-or-d3d
void XYscope::ellipse(float cx, float cy, float rx, float ry) {
    float theta = TWO_PI / float(ellipseDetailVal);
    float c = std::cos(theta);
    float s = std::sin(theta);
    float x = 0.5f; // start at angle = 0
    float y = 0;

    beginShape();
    for (int ii = 0; ii < ellipseDetailVal + 1; ii++) {
        vertex(x * rx + cx, y * ry + cy);
        // apply the rotation matrix
        float t = x;
        x = c * x - s * y;
        y = s * t + c * y;
    }
    endShape();
}

//--------------------------------------------------------------
void XYscope::lissajous(float xPos, float yPos, float radius, float ratioA, float ratioB, float phase, float resolution) {
    resolution = ofClamp(resolution, 1, 360);
    float theta = TWO_PI / resolution;
    beginShape();
    for (int i = 0; i < resolution + 1; i++) {
        float x = std::sin(i * theta * ratioA) * radius;
        float y = std::sin(ofDegToRad(phase) + i * theta * ratioB) * radius;
        vertex(xPos + x, yPos + y);
    }
    endShape();
}

//--------------------------------------------------------------
// extended from: https://stackoverflow.com/a/72277489/10885535
void XYscope::box(float w, float h, float d) {
    // half size: keep the pivot at the center of the mesh
    float rx = w * 0.5f;
    float ry = h * 0.5f;
    float rz = d * 0.5f;
    beginShape();
    // back (-z)
    vertex(-rx, -ry, -rz);
    vertex(+rx, -ry, -rz);
    vertex(+rx, +ry, -rz);
    vertex(-rx, +ry, -rz);
    // slide to otherside
    vertex(-rx, -ry, -rz);
    // front (+z)
    vertex(-rx, -ry, +rz);
    vertex(+rx, -ry, +rz);
    vertex(+rx, +ry, +rz);
    vertex(-rx, +ry, +rz);
    // top (-y)
    vertex(-rx, -ry, +rz);
    vertex(-rx, -ry, -rz);
    vertex(+rx, -ry, -rz);
    vertex(+rx, -ry, +rz);
    // bottom (+y)
    vertex(+rx, +ry, +rz);
    vertex(+rx, +ry, -rz);
    vertex(-rx, +ry, -rz);
    vertex(-rx, +ry, +rz);
    // left (-x)
    vertex(-rx, -ry, +rz);
    vertex(-rx, -ry, -rz);
    vertex(-rx, +ry, -rz);
    vertex(-rx, +ry, +rz);
    // slide to otherside
    vertex(-rx, -ry, +rz);
    // right (+x)
    vertex(+rx, -ry, +rz);
    vertex(+rx, -ry, -rz);
    vertex(+rx, +ry, -rz);
    vertex(+rx, +ry, +rz);
    endShape();
}

//--------------------------------------------------------------
// based on: Processing Examples » Topics » Textures » Texture Sphere
void XYscope::ellipsoid(float rx, float ry, float rz, int dx, int dy) {
    int numvW = ofClamp(dx, 1, 50);
    int numvH_2pi = ofClamp(dy, 1, 50);

    // the number of points around the width and height
    int numPointsW = numvW + 1;
    int numPointsH_2pi = numvH_2pi; // how many actual points around the sphere (not just from top to bottom)
    int numPointsH = std::ceil(numPointsH_2pi / 2.0f) + 1; // how many points from top to bottom

    std::vector<float> coorX(numPointsW);   // all the x-coor in a horizontal circle radius 1
    std::vector<float> coorY(numPointsH);   // all the y-coor in a vertical circle radius 1
    std::vector<float> coorZ(numPointsW);   // all the z-coor in a horizontal circle radius 1
    std::vector<float> multXZ(numPointsH);  // the radius of each horizontal circle

    for (int i = 0; i < numPointsW; i++) {
        float thetaW = i * 2 * PI / (numPointsW - 1);
        coorX[i] = std::sin(thetaW);
        coorZ[i] = std::cos(thetaW);
    }

    for (int i = 0; i < numPointsH; i++) {
        if (numPointsH_2pi % 2 != 0 && i == numPointsH - 1) { // odd numPointsH_2pi and the last point
            float thetaH = (i - 1) * 2 * PI / numPointsH_2pi;
            coorY[i] = std::cos(PI + thetaH);
            multXZ[i] = 0;
        } else {
            // allows a flat bottom if numPointsH is odd
            float thetaH = i * 2 * PI / numPointsH_2pi;
            // PI+ makes the top always the point instead of the bottom
            coorY[i] = std::cos(PI + thetaH);
            multXZ[i] = std::sin(thetaH);
        }
    }

    beginShape();
    for (int i = 0; i < numPointsH - 1; i++) {
        for (int j = 0; j < numPointsW; j++) {
            vertex(coorX[j] * multXZ[i] * rx, coorY[i] * ry, coorZ[j] * multXZ[i] * rz);
            vertex(coorX[j] * multXZ[i + 1] * rx, coorY[i + 1] * ry, coorZ[j] * multXZ[i + 1] * rz);
        }
    }
    endShape();
}

//--------------------------------------------------------------
// built upon: https://processing.org/examples/toroid.html
void XYscope::torus(float radius, float tubeRadius, int dx, int dy) {
    dx = ofClamp(dx, 1, 50);
    dy = ofClamp(dy, 1, 50);

    std::vector<glm::vec3> vertices(dx + 1);
    std::vector<glm::vec3> vertices2(dx + 1);

    float angle = 0;
    for (int i = 0; i <= dx; i++) {
        vertices[i].x = radius + std::sin(ofDegToRad(angle)) * tubeRadius;
        vertices[i].z = std::cos(ofDegToRad(angle)) * tubeRadius;
        angle += 360.0f / dx;
    }

    float latheAngle = 0;
    for (int i = 0; i <= dy; i++) {
        beginShape();
        for (int j = 0; j <= dx; j++) {
            if (i > 0) vertex(vertices2[j]);
            vertices2[j].x = std::cos(ofDegToRad(latheAngle)) * vertices[j].x;
            vertices2[j].y = std::sin(ofDegToRad(latheAngle)) * vertices[j].x;
            vertices2[j].z = vertices[j].z;
            vertex(vertices2[j]);
        }
        latheAngle += 360.0f / dy;
        endShape();
    }
}

//--------------------------------------------------------------
void XYscope::polyline(const ofPolyline & poly) {
    const auto & verts = poly.getVertices();
    if (verts.size() < 2) return;
    beginShape();
    for (const auto & v : verts) vertex(v);
    if (poly.isClosed()) vertex(verts.front());
    endShape();
}

//--------------------------------------------------------------
void XYscope::polylines(const std::vector<ofPolyline> & polys) {
    for (const auto & poly : polys) polyline(poly);
}

//--------------------------------------------------------------
void XYscope::path(const ofPath & p) {
    for (const auto & outline : p.getOutline()) polyline(outline);
}

//==============================================================
// transforms
//==============================================================

//--------------------------------------------------------------
void XYscope::pushMatrix() {
    matrixStack.push_back(matrix);
}

//--------------------------------------------------------------
void XYscope::popMatrix() {
    if (matrixStack.empty()) {
        ofLogWarning("XYscope") << "popMatrix() without a pushMatrix()";
        return;
    }
    matrix = matrixStack.back();
    matrixStack.pop_back();
}

//--------------------------------------------------------------
void XYscope::resetMatrix() {
    matrix = glm::mat4(1);
    matrixStack.clear();
}

//--------------------------------------------------------------
void XYscope::translate(float x, float y, float z) {
    matrix = glm::translate(matrix, glm::vec3(x, y, z));
}

//--------------------------------------------------------------
void XYscope::rotateX(float angle) {
    matrix = glm::rotate(matrix, angle, glm::vec3(1, 0, 0));
}

//--------------------------------------------------------------
void XYscope::rotateY(float angle) {
    matrix = glm::rotate(matrix, angle, glm::vec3(0, 1, 0));
}

//--------------------------------------------------------------
void XYscope::rotateZ(float angle) {
    matrix = glm::rotate(matrix, angle, glm::vec3(0, 0, 1));
}

//--------------------------------------------------------------
void XYscope::scale(float x, float y, float z) {
    matrix = glm::scale(matrix, glm::vec3(x, y, z));
}

//==============================================================
// text
//==============================================================

//--------------------------------------------------------------
bool XYscope::textFont(const std::string & fontName) {
    return font.load(fontName);
}

//--------------------------------------------------------------
void XYscope::textSize(float size) {
    textSizeVal = size;
    textLeadingVal = size * 1.5f;
}

//--------------------------------------------------------------
void XYscope::textAlign(ofAlignHorz alignX, ofAlignVert alignY) {
    textAlignX = alignX;
    textAlignY = alignY;
}

//--------------------------------------------------------------
void XYscope::text(const std::string & s, float x, float y) {
    for (const auto & stroke : textPaths(s, x, y)) {
        beginShape();
        for (const auto & v : stroke.getVertices()) vertex(v.x, v.y);
        endShape();
    }
}

//--------------------------------------------------------------
float XYscope::textWidth(const std::string & s) const {
    return font.getWidth(s, textSizeVal);
}

//--------------------------------------------------------------
std::vector<ofPolyline> XYscope::textPaths(const std::string & s, float x, float y) const {
    return font.getStrokes(s, x, y, textSizeVal, textLeadingVal, textAlignX, textAlignY);
}

//==============================================================
// inspection
//==============================================================

//--------------------------------------------------------------
std::vector<ofPolyline> XYscope::getPolylines() const {
    std::vector<ofPolyline> result;
    for (const auto & shape : shapes) {
        ofPolyline poly;
        for (const auto & p : shape) poly.addVertex(p.x * xyWidth, p.y * xyHeight);
        result.push_back(poly);
    }
    return result;
}

//--------------------------------------------------------------
std::vector<glm::vec3> XYscope::wavePoints() const {
    std::vector<glm::vec3> points;
    for (const auto & shape : shapes) points.insert(points.end(), shape.begin(), shape.end());
    return points;
}

//==============================================================
// drawing
//==============================================================

//--------------------------------------------------------------
void XYscope::drawAll() {
    drawPath();
    drawWaveform();
    drawWave();
    drawXY();
    drawPoints();
}

//--------------------------------------------------------------
void XYscope::drawPath(const ofColor & color) {
    ofPushStyle();
    ofNoFill();
    ofSetColor(color);
    for (const auto & poly : getPolylines()) poly.draw();
    ofPopStyle();
}

//--------------------------------------------------------------
void XYscope::drawPoints(const ofColor & color) {
    ofPushStyle();
    ofFill();
    ofSetColor(color);
    for (const auto & shape : shapes) {
        for (const auto & p : shape) ofDrawCircle(p.x * xyWidth, p.y * xyHeight, 1.5f);
    }
    ofPopStyle();
}

//--------------------------------------------------------------
void XYscope::drawXY(const ofColor & color) {
    ofSoundBuffer buffer = getLastBuffer();
    size_t nCh = buffer.getNumChannels();
    if (nCh < 2 || buffer.getNumFrames() < 2) return;

    ofPolyline poly;
    for (size_t i = 0; i < buffer.getNumFrames(); i++) {
        float l = buffer[i * nCh];
        float r = buffer[i * nCh + 1];
        float lAudio = l * xyWidth / 2;
        float rAudio = r * xyHeight / 2;
        if (useVectrex) {
            // undo the Vectrex wiring, so the preview stays upright
            if (vectrexRotation == 90) {
                lAudio = -r * xyWidth / 2;
                rAudio = l * xyHeight / 2;
            } else if (vectrexRotation == -90) {
                lAudio = r * xyWidth / 2;
                rAudio = -l * xyHeight / 2;
            } else {
                lAudio = -l * xyWidth / 2;
                rAudio = -r * xyHeight / 2;
            }
        }
        poly.addVertex(lAudio, -rAudio);
    }

    ofPushStyle();
    ofNoFill();
    ofSetColor(color);
    ofPushMatrix();
    ofTranslate(xyWidth / 2, xyHeight / 2);
    poly.draw();

    if (debugWave) {
        Params params = getParams();
        float mouseT = ofGetMouseX() / xyWidth;
        float mx = tableX.value(mouseT) * xyWidth / 2 * params.amp.x;
        float my = -tableY.value(mouseT) * xyHeight / 2 * params.amp.y;
        ofFill();
        ofDrawCircle(mx, my, 5);
    }

    ofPopMatrix();
    ofPopStyle();
}

//--------------------------------------------------------------
void XYscope::drawWaveform(const ofColor & colorX, const ofColor & colorY) {
    int w = std::max(2, int(xyWidth));
    auto plot = [&](const XYWavetable & table, float centerY) {
        auto wave = table.getWaveformPtr();
        ofPolyline poly;
        for (int i = 0; i < w; i++) {
            poly.addVertex(i, centerY - xyHeight * 0.125f * XYWavetable::valueAt(*wave, float(i) / w));
        }
        poly.draw();
    };

    ofPushStyle();
    ofNoFill();
    ofSetColor(colorX);
    plot(tableX, xyHeight * 0.25f);
    ofSetColor(colorY);
    plot(tableY, xyHeight * 0.75f);
    if (useZ) {
        ofSetColor(50, 255, 50);
        plot(tableZ, xyHeight * 0.5f);
    }

    if (debugWave) {
        float t = ofGetMouseX() / xyWidth;
        ofFill();
        ofSetColor(colorX);
        ofDrawCircle(ofGetMouseX(), xyHeight * 0.25f - xyHeight * 0.125f * tableX.value(t), 5);
        ofSetColor(colorY);
        ofDrawCircle(ofGetMouseX(), xyHeight * 0.75f - xyHeight * 0.125f * tableY.value(t), 5);
    }
    ofPopStyle();
}

//--------------------------------------------------------------
void XYscope::drawWave(const ofColor & color) {
    ofSoundBuffer buffer = getLastBuffer();
    size_t nCh = buffer.getNumChannels();
    size_t nFrames = buffer.getNumFrames();
    if (nFrames < 2) return;

    ofPushStyle();
    ofNoFill();
    ofSetColor(color);
    float centers[3] = { xyHeight * 0.25f, xyHeight * 0.75f, xyHeight * 0.5f };
    for (size_t c = 0; c < nCh && c < 3; c++) {
        ofPolyline poly;
        for (size_t i = 0; i < nFrames; i++) {
            poly.addVertex(ofMap(i, 0, nFrames, 0, xyWidth), centers[c] - xyHeight * 0.25f * buffer[i * nCh + c]);
        }
        poly.draw();
    }
    ofPopStyle();
}

//==============================================================
// recording
//==============================================================

//--------------------------------------------------------------
void XYscope::recorderBegin(const std::string & name) {
    std::lock_guard<std::mutex> lock(audioMutex);
    recordingPath = name + "_" + ofGetTimestampString("%Y_%m_%d_%H%M%S%i") + ".wav";
    recordBuffer.clear();
    recordBuffer.setNumChannels(1);
    recording = true;
    ofLogNotice("XYscope") << "beginRecord";
}

//--------------------------------------------------------------
std::string XYscope::recorderEnd() {
    ofSoundBuffer toSave;
    std::string path;
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (!recording) return "";
        recording = false;
        std::swap(toSave, recordBuffer);
        path = recordingPath;
    }
    if (toSave.getNumFrames() == 0) {
        ofLogWarning("XYscope") << "endRecord: nothing was recorded";
        return "";
    }
    WavFile::save(path, toSave, WavFile::PCM_16);
    ofLogNotice("XYscope") << "endRecord + saved " << ofToDataPath(path);
    return ofToDataPath(path, true);
}
