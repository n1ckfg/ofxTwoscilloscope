#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    ofSetWindowTitle("ofxTwoscilloscope: audio -> vectors");
    ofSetFrameRate(60);
    ofBackground(0);

    // two square panels side by side, with a strip of text underneath
    panelSize = ofGetWidth() / 2;

    // the beam renders into a panelSize FBO, from audio upsampled to 192kHz
    scope.setup(panelSize, panelSize, 192000);
    scope.decoderSettings.simplify = 0.75f; // px; 0 keeps every sample

    player.setScope(&scope);
    player.setLoop(true);
    loadFile("xyscope.wav");

    liveInput = false;
    showPoints = false;
    audioOk = openSoundStream(false);
    if (!audioOk) status = "no sound card found, playing silently";
}

//--------------------------------------------------------------
bool ofApp::openSoundStream(bool withInput) {
    soundStream.close();

    ofSoundStreamSettings settings;
    settings.setOutListener(this);
    settings.numOutputChannels = 2;
    settings.sampleRate = 44100;
    settings.bufferSize = 512;
    settings.numBuffers = 4;
    if (withInput) {
        settings.setInListener(this);
        settings.numInputChannels = 2;
    }
    return soundStream.setup(settings);
}

//--------------------------------------------------------------
void ofApp::loadFile(const std::string & path) {
    if (player.load(path)) {
        player.play();
        status = "playing " + player.getFilename();
    } else {
        status = "couldn't load " + path + " (drop a .wav on the window)";
    }
}

//--------------------------------------------------------------
void ofApp::audioOut(ofSoundBuffer & buffer) {
    if (liveInput) {
        buffer.set(0);
    } else {
        player.audioOut(buffer); // also feeds the scope
    }
}

//--------------------------------------------------------------
void ofApp::audioIn(ofSoundBuffer & buffer) {
    if (liveInput) scope.addBuffer(buffer);
}

//--------------------------------------------------------------
void ofApp::update() {
    if (!audioOk && !liveInput) player.update(ofGetLastFrameTime());

    scope.update();

    // turn the latest loop of audio back into vector shapes
    shapes = scope.getShapes(panelSize, panelSize);
}

//--------------------------------------------------------------
void ofApp::draw() {
    // left: the audio as an analog scope draws it
    scope.draw(0, 0, panelSize, panelSize);

    // right: the vector shapes decoded from it
    ofPushMatrix();
    ofTranslate(panelSize, 0);
    ofSetColor(12);
    ofFill();
    ofDrawRectangle(0, 0, panelSize, panelSize);
    ofSetColor(28);
    for (int i = 1; i < 8; i++) {
        ofDrawLine(i * panelSize / 8, 0, i * panelSize / 8, panelSize);
        ofDrawLine(0, i * panelSize / 8, panelSize, i * panelSize / 8);
    }

    size_t numPoints = 0;
    ofNoFill();
    for (size_t i = 0; i < shapes.size(); i++) {
        // a different hue per shape, so you can see where the strokes break
        ofSetColor(ofColor::fromHsb((i * 37) % 255, 120, 255));
        shapes[i].draw();
        numPoints += shapes[i].size();
        if (showPoints) {
            for (const auto & v : shapes[i].getVertices()) ofDrawCircle(v.x, v.y, 1.5f);
        }
    }
    ofPopMatrix();

    // info
    float y = panelSize + 22;
    ofSetColor(255);
    ofDrawBitmapString("beam (Oscilloscope)", 12, y);
    ofDrawBitmapString("vector shapes (XYDecoder)", panelSize + 12, y);

    ofSetColor(160);
    std::string source = liveInput ? "line in" :
        player.getFilename() + "  " + ofToString(player.getPositionMS() / 1000.0f, 1) + " / " +
        ofToString(player.getDurationMS() / 1000.0f, 1) + "s  " + ofToString(player.getNumChannels()) + "ch";
    ofDrawBitmapString(source, 12, y + 18);
    float period = scope.getDetectedPeriod();
    std::string loop = period > 0 ? ofToString(scope.getSourceSampleRate() / period, 2) + " Hz loop" : "no loop found";
    ofDrawBitmapString(loop + ", " + ofToString(shapes.size()) + " shapes, " + ofToString(numPoints) + " points",
                       panelSize + 12, y + 18);

    ofDrawBitmapString("space play/pause  </> seek  i line in  s save svg  p points  +/- beam  g glow  h hue  z z-mod",
                       12, y + 40);
    ofSetColor(120);
    ofDrawBitmapString(status, 12, ofGetHeight() - 10);
    ofDrawBitmapString(ofToString(ofGetFrameRate(), 0) + " fps, " + ofToString(scope.getDropped()) + " dropped",
                       panelSize + 12, ofGetHeight() - 10);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
    if (key == ' ') {
        player.setPaused(player.isPlaying());
    } else if (key == OF_KEY_LEFT) {
        player.setPositionMS(std::max(0, player.getPositionMS() - 2000));
    } else if (key == OF_KEY_RIGHT) {
        player.setPositionMS(player.getPositionMS() + 2000);
    } else if (key == 'i') {
        liveInput = !liveInput;
        scope.clear();
        audioOk = openSoundStream(liveInput);
        if (liveInput && !audioOk) {
            liveInput = false;
            audioOk = openSoundStream(false);
            status = "no audio input found";
        } else {
            status = liveInput ? "listening to the line input" : "playing " + player.getFilename();
        }
    } else if (key == 's') {
        std::string path = "shapes_" + ofGetTimestampString("%Y%m%d_%H%M%S") + ".svg";
        XYDecoder::saveSvg(path, shapes, panelSize, panelSize);
        status = "saved " + path;
    } else if (key == 'p') {
        showPoints = !showPoints;
    } else if (key == '+' || key == '=') {
        scope.strokeWeight = std::min(20.0f, scope.strokeWeight + 1);
    } else if (key == '-') {
        scope.strokeWeight = std::max(1.0f, scope.strokeWeight - 1);
    } else if (key == 'g') {
        scope.afterglow = scope.afterglow > 0.85f ? 0 : scope.afterglow + 0.15f;
    } else if (key == 'h') {
        scope.hue = scope.hue >= 360 ? 0 : scope.hue + 30;
    } else if (key == 'z') {
        scope.zModulation = !scope.zModulation;
    }
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
    if (!dragInfo.files.empty()) {
        liveInput = false;
        loadFile(dragInfo.files[0]);
    }
}
