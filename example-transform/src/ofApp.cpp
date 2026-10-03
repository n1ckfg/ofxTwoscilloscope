#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    ofSetWindowTitle("ofxTwoscilloscope: vector -> audio -> vector");
    ofSetFrameRate(60);
    ofBackground(0);

    canvasSize = 512;
    panelSize = 320;
    panel1 = glm::vec2(250, 40);
    panel2 = glm::vec2(panel1.x + panelSize + 25, 40);
    panel3 = glm::vec2(panel2.x + panelSize + 25, 40);

    // shapes on a 512 x 512 canvas, encoded at 44.1kHz as a 50Hz loop
    transformer.setup(canvasSize, canvasSize, 44100, 50);

    // the effect chain, in order; every setting shows up in the panel
    auto lowPass = transformer.effects.add<XYLowPass>();
    lowPass->cutoff = 1500;
    auto delay = transformer.effects.add<XYChannelDelay>();
    delay->delayY = 0.6f;
    transformer.effects.add<XYHighPass>();
    transformer.effects.add<XYEcho>();
    transformer.effects.add<XYRingMod>();
    transformer.effects.add<XYRotate>();
    transformer.effects.add<XYDrive>();
    transformer.effects.add<XYWavefold>();
    transformer.effects.add<XYBitCrush>();
    transformer.effects.add<XYSampleHold>();
    transformer.effects.add<XYNoise>();
    for (size_t i = 2; i < transformer.effects.effects.size(); i++) {
        transformer.effects.effects[i]->enabled = false;
    }
    soloIndex = -1;

    gui.setup(transformer.effects.parameters, "effects.xml", 10, 10);
    for (auto & effect : transformer.effects.effects) {
        if (!effect->enabled) gui.getGroup(effect->getName()).minimize();
    }

    // the altered loop plays back through an XYscope, and into the beam
    player.setup(canvasSize, canvasSize, 44100, 512);
    player.freq(transformer.getFreq());
    scope.setup(panelSize, panelSize);

    ofSoundStreamSettings settings;
    settings.setOutListener(this);
    settings.numOutputChannels = 2;
    settings.numInputChannels = 0;
    settings.sampleRate = 44100;
    settings.bufferSize = 512;
    settings.numBuffers = 4;
    audioOk = soundStream.setup(settings);
    silentFrames = 0;
    status = audioOk ? "the altered shape is playing on the default audio out" : "no sound card found, running silently";

    sourceNames = { "shapes", "text", "spiral", "drawing" };
    sourceIndex = 0;
    generation = 0;
    makeSource();
}

//--------------------------------------------------------------
void ofApp::makeSource() {
    source.clear();
    generation = 0;
    float s = canvasSize;

    switch (sourceIndex) {
        case 0: {
            ofPolyline circle;
            for (int i = 0; i < 60; i++) {
                float a = TWO_PI * i / 60;
                circle.addVertex(s * 0.3f + std::cos(a) * s * 0.17f, s * 0.3f + std::sin(a) * s * 0.17f);
            }
            circle.setClosed(true);
            source.push_back(circle);

            ofPolyline square;
            square.addVertex(s * 0.56f, s * 0.14f);
            square.addVertex(s * 0.88f, s * 0.14f);
            square.addVertex(s * 0.88f, s * 0.46f);
            square.addVertex(s * 0.56f, s * 0.46f);
            square.setClosed(true);
            source.push_back(square);

            ofPolyline star;
            for (int i = 0; i < 5; i++) {
                float a = -HALF_PI + i * TWO_PI * 2 / 5;
                star.addVertex(s * 0.5f + std::cos(a) * s * 0.2f, s * 0.72f + std::sin(a) * s * 0.2f);
            }
            star.setClosed(true);
            source.push_back(star);
            break;
        }
        case 1: {
            HersheyFont font;
            font.load("timesr");
            source = font.getStrokes("ofx", s / 2, s * 0.42f, s * 0.3f, s * 0.4f, OF_ALIGN_HORZ_CENTER, OF_ALIGN_VERT_CENTER);
            font.load("futural");
            auto small = font.getStrokes("vector > audio > vector", s / 2, s * 0.75f, s * 0.05f, s * 0.08f,
                                         OF_ALIGN_HORZ_CENTER, OF_ALIGN_VERT_CENTER);
            source.insert(source.end(), small.begin(), small.end());
            break;
        }
        case 2: {
            ofPolyline spiral;
            for (int i = 0; i <= 400; i++) {
                float t = i / 400.0f;
                float a = t * TWO_PI * 5;
                spiral.addVertex(s / 2 + std::cos(a) * t * s * 0.42f, s / 2 + std::sin(a) * t * s * 0.42f);
            }
            source.push_back(spiral);
            break;
        }
        case 3:
            source = drawing;
            break;
    }
}

//--------------------------------------------------------------
void ofApp::soloEffect(int index) {
    // turn on one effect at a time, to see what each one does
    soloIndex = index;
    for (size_t i = 0; i < transformer.effects.effects.size(); i++) {
        auto & effect = transformer.effects.effects[i];
        effect->enabled = int(i) == index;
        if (effect->enabled) gui.getGroup(effect->getName()).maximize();
        else gui.getGroup(effect->getName()).minimize();
    }
}

//--------------------------------------------------------------
void ofApp::update() {
    // the whole round trip, every frame: shape -> audio -> effects -> shape
    result = transformer.transform(source);

    // loop the altered audio, Z (blanking) included
    std::vector<float> x, y, z;
    transformer.getProcessedCycle(x, y, z);
    player.setWaveforms(x, y, z);

    if (!audioOk) {
        // no sound card: run the playback on the clock instead
        silentFrames += ofGetLastFrameTime() * 44100;
        int frames = int(silentFrames);
        silentFrames -= frames;
        if (frames > 0) {
            ofSoundBuffer buffer;
            buffer.allocate(frames, 3);
            buffer.setSampleRate(44100);
            player.audioOut(buffer);
            scope.addBuffer(buffer);
        }
    }
    scope.update();
}

//--------------------------------------------------------------
void ofApp::audioOut(ofSoundBuffer & buffer) {
    // render X, Y and Z, show all three, send X and Y
    ofSoundBuffer xyz;
    xyz.allocate(buffer.getNumFrames(), 3);
    xyz.setSampleRate(buffer.getSampleRate());
    player.audioOut(xyz);
    scope.addBuffer(xyz);

    size_t nCh = buffer.getNumChannels();
    for (size_t i = 0; i < buffer.getNumFrames(); i++) {
        for (size_t c = 0; c < nCh; c++) {
            buffer[i * nCh + c] = c < 2 ? xyz[i * 3 + c] : 0;
        }
    }
}

//--------------------------------------------------------------
glm::vec2 ofApp::toCanvas(float x, float y) const {
    return glm::vec2(x - panel1.x, y - panel1.y) * (canvasSize / panelSize);
}

//--------------------------------------------------------------
void ofApp::drawPanel(float x, float y, const std::string & title) {
    ofSetColor(14);
    ofFill();
    ofDrawRectangle(x, y, panelSize, panelSize);
    ofSetColor(220);
    ofDrawBitmapString(title, x, y - 10);
}

//--------------------------------------------------------------
void ofApp::drawShapes(const std::vector<ofPolyline> & shapes, float x, float y, bool colored, bool points) {
    ofPushMatrix();
    ofTranslate(x, y);
    ofScale(panelSize / canvasSize);
    ofNoFill();
    for (size_t i = 0; i < shapes.size(); i++) {
        if (colored) ofSetColor(ofColor::fromHsb((i * 37) % 255, 120, 255));
        shapes[i].draw();
        if (points) {
            for (const auto & v : shapes[i].getVertices()) ofDrawCircle(v.x, v.y, 1.5f);
        }
    }
    ofPopMatrix();
}

//--------------------------------------------------------------
void ofApp::drawWave(const ofSoundBuffer & audio, float x, float y, float w, float h, const ofColor & color) {
    // one loop of X (top) and Y (bottom)
    size_t nCh = audio.getNumChannels();
    size_t n = audio.getNumFrames();
    if (nCh < 2 || n < 2) return;
    ofSetColor(color);
    for (int c = 0; c < 2; c++) {
        ofPolyline line;
        for (size_t i = 0; i < n; i++) {
            line.addVertex(x + w * i / (n - 1), y + h * (0.25f + 0.5f * c) - h * 0.22f * audio[i * nCh + c]);
        }
        line.draw();
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
    gui.draw();

    // 1. the source shape
    drawPanel(panel1.x, panel1.y, "1. vector shape" + std::string(generation > 0 ? " (generation " + ofToString(generation) + ")" : ""));
    ofSetColor(255);
    drawShapes(source, panel1.x, panel1.y, false, false);

    // 2. the altered audio, as a beam
    drawPanel(panel2.x, panel2.y, "2. as XY audio, through the effects");
    scope.draw(panel2.x, panel2.y, panelSize, panelSize);

    // 3. the altered audio decoded, over a ghost of the source
    drawPanel(panel3.x, panel3.y, "3. decoded vector shape");
    // effects can push the shape off the canvas, so clip to the panel
    glEnable(GL_SCISSOR_TEST);
    glScissor(panel3.x, ofGetViewportHeight() - (panel3.y + panelSize), panelSize, panelSize);
    ofSetColor(55);
    drawShapes(source, panel3.x, panel3.y, false, false);
    drawShapes(result, panel3.x, panel3.y, true, true);
    glDisable(GL_SCISSOR_TEST);

    // the audio: one loop before (grey) and after (green) the effects
    float waveY = panel1.y + panelSize + 40;
    float waveW = panel3.x + panelSize - panel1.x;
    ofSetColor(220);
    ofDrawBitmapString("one loop of X (top) and Y (bottom): encoded (grey), after the effects (green)", panel1.x, waveY - 10);
    ofSetColor(14);
    ofFill();
    ofDrawRectangle(panel1.x, waveY, waveW, 200);
    ofNoFill();
    ofSoundBuffer encodedCycle;
    {
        const ofSoundBuffer & encoded = transformer.getEncodedAudio();
        size_t nCh = encoded.getNumChannels();
        size_t m = std::min<size_t>(transformer.getProcessedCycle().getNumFrames(), encoded.getNumFrames());
        encodedCycle.setNumChannels(nCh);
        if (m > 0) encodedCycle.getBuffer().assign(encoded.getBuffer().end() - m * nCh, encoded.getBuffer().end());
    }
    drawWave(encodedCycle, panel1.x, waveY, waveW, 200, ofColor(110));
    drawWave(transformer.getProcessedCycle(), panel1.x, waveY, waveW, 200, ofColor(60, 255, 120));

    // info
    size_t sourcePoints = 0, resultPoints = 0;
    for (const auto & p : source) sourcePoints += p.size();
    for (const auto & p : result) resultPoints += p.size();
    float infoY = waveY + 230;
    ofSetColor(160);
    ofDrawBitmapString("source: " + sourceNames[sourceIndex] + ", " + ofToString(source.size()) + " shapes, " +
                       ofToString(sourcePoints) + " points   ->   result: " + ofToString(result.size()) + " shapes, " +
                       ofToString(resultPoints) + " points", panel1.x, infoY);
    ofDrawBitmapString("1-4 source (4: draw in panel 1)   e solo next effect   n no effects   a apply (feed the result back in)\n"
                       "c clear drawing   s save svg   w save wav", panel1.x, infoY + 22);
    ofSetColor(120);
    ofDrawBitmapString(status, panel1.x, ofGetHeight() - 12);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
    if (key >= '1' && key <= '4') {
        sourceIndex = key - '1';
        makeSource();
    } else if (key == 'e') {
        soloEffect((soloIndex + 1) % int(transformer.effects.effects.size()));
        status = "solo: " + transformer.effects.effects[soloIndex]->getName();
    } else if (key == 'n') {
        soloEffect(-1);
        status = "no effects: the round trip on its own";
    } else if (key == 'a') {
        // feed the altered shape back in, and alter it again
        source = result;
        generation++;
        status = "generation " + ofToString(generation);
    } else if (key == 'c') {
        drawing.clear();
        if (sourceIndex == 3) makeSource();
    } else if (key == 's') {
        std::string path = "transformed_" + ofGetTimestampString("%Y%m%d_%H%M%S") + ".svg";
        XYDecoder::saveSvg(path, result, canvasSize, canvasSize);
        status = "saved " + path;
    } else if (key == 'w') {
        // four seconds of the altered loop, X Y Z
        std::string path = "transformed_" + ofGetTimestampString("%Y%m%d_%H%M%S") + ".wav";
        WavFile::save(path, player.render(4, 3));
        status = "saved " + path;
    }
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
    if (!ofRectangle(panel1.x, panel1.y, panelSize, panelSize).inside(x, y)) return;
    if (sourceIndex != 3) {
        sourceIndex = 3;
        drawing.clear();
    }
    drawing.emplace_back();
    drawing.back().addVertex(glm::vec3(toCanvas(x, y), 0));
    makeSource();
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
    if (sourceIndex != 3 || drawing.empty()) return;
    if (!ofRectangle(panel1.x, panel1.y, panelSize, panelSize).inside(x, y)) return;
    glm::vec2 p = toCanvas(x, y);
    auto & line = drawing.back();
    if (glm::distance(glm::vec2(line.getVertices().back()), p) > 4) {
        line.addVertex(glm::vec3(p, 0));
        makeSource();
    }
}
