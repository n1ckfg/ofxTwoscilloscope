#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    ofSetWindowTitle("ofxTwoscilloscope: vectors -> audio");
    ofSetFrameRate(60);
    ofBackground(0);

    // the drawing canvas is the square on the left of the window
    canvasSize = ofGetHeight();

    // canvas size, sample rate, buffer size (which is also the wavetable size)
    xy.setup(canvasSize, canvasSize, 44100, 512);
    xy.freq(50); // the whole drawing repeats 50 times a second

    // X on the left channel, Y on the right. Pass 3 channels to send Z
    // (beam blanking) on the third, if your sound card has one.
    audioOk = xy.openAudioOut();
    if (!audioOk) {
        // no sound card: keep the oscillators running on the clock, so the
        // previews and the recorder still work
        status = "no sound card: running silently";
    } else {
        status = "audio out: default device";
    }

    sceneNames = { "shapes", "text", "3D", "draw" };
    fontNames = { "futural", "scripts", "gothiceng", "timesr", "rowmand", "cursive" };
    scene = 0;
    fontIndex = 0;
}

//--------------------------------------------------------------
void ofApp::update() {
    if (!audioOk) xy.process(ofGetLastFrameTime());
}

//--------------------------------------------------------------
void ofApp::drawScene(XYscope & scope, float t) {
    float s = canvasSize;

    switch (scene) {
        case 0: {
            // Processing-style primitives
            scope.ellipse(s * 0.28f, s * 0.28f, s * (0.26f + 0.06f * std::sin(t * 2)));

            scope.rectMode(OF_RECTMODE_CENTER);
            scope.pushMatrix();
            scope.translate(s * 0.72f, s * 0.28f);
            scope.rotate(t * 0.5f);
            scope.rect(0, 0, s * 0.26f, s * 0.26f);
            scope.popMatrix();

            scope.lissajous(s * 0.28f, s * 0.72f, s * 0.14f, 3, 2, t * 40, 120);

            scope.pushMatrix();
            scope.translate(s * 0.72f, s * 0.72f);
            scope.rotate(-t);
            scope.beginShape();
            for (int i = 0; i < 5; i++) {
                // a star, every second point of a pentagon
                float a = -HALF_PI + i * TWO_PI * 2 / 5;
                scope.vertex(std::cos(a) * s * 0.15f, std::sin(a) * s * 0.15f);
            }
            scope.endShape(true);
            scope.popMatrix();
            break;
        }

        case 1: {
            // Hershey single stroke fonts
            const std::string & fontName = fontNames[fontIndex];
            if (scope.getFont().getName() != fontName) scope.textFont(fontName);

            scope.textAlign(OF_ALIGN_HORZ_CENTER, OF_ALIGN_VERT_CENTER);
            scope.textSize(s * 0.13f);
            scope.text("XYscope", s / 2, s * 0.32f);
            scope.textSize(s * 0.09f);
            char clock[16];
            std::snprintf(clock, sizeof(clock), "%02d:%02d:%02d", ofGetHours(), ofGetMinutes(), ofGetSeconds());
            scope.text(clock, s / 2, s * 0.56f);
            scope.textSize(s * 0.04f);
            scope.text(fontName, s / 2, s * 0.78f);
            break;
        }

        case 2: {
            // 3D, through the default perspective
            scope.translate(s / 2, s / 2);
            scope.rotateY(t * 0.7f);
            scope.rotateX(t * 0.4f);
            scope.torus(s * 0.2f, s * 0.08f, 16, 10);
            break;
        }

        case 3: {
            scope.polylines(drawing);
            if (drawing.empty()) {
                if (scope.getFont().getName() != "futural") scope.textFont("futural");
                scope.textAlign(OF_ALIGN_HORZ_CENTER, OF_ALIGN_VERT_CENTER);
                scope.textSize(s * 0.05f);
                scope.text("draw with the mouse", s / 2, s / 2);
            }
            break;
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
    // build this frame's waves, the way an XYscope sketch does in draw()
    xy.clearWaves();
    drawScene(xy, ofGetElapsedTimef());
    xy.buildWaves();

    // the canvas: the vector shapes, faintly, under the signal as a scope shows it
    ofSetColor(30);
    ofNoFill();
    ofDrawRectangle(0.5f, 0.5f, canvasSize - 1, canvasSize - 1);
    xy.drawPath(ofColor(255, 255, 255, 60));
    xy.drawXY();

    // the side panel: the audio itself
    float panelX = canvasSize + 16;
    float panelW = ofGetWidth() - panelX - 16;
    float y = 24;
    ofSetColor(255);
    ofDrawBitmapString("vectors -> audio", panelX, y);
    ofSetColor(160);
    ofDrawBitmapString("scene: " + sceneNames[scene], panelX, y += 24);
    ofDrawBitmapString(ofToString(xy.getShapes().size()) + " shapes, " + ofToString(xy.wavePoints().size()) + " points", panelX, y += 16);
    ofDrawBitmapString(ofToString(xy.freq().x, 0) + " Hz loop, " + ofToString(xy.waveSize()) + " samples", panelX, y += 16);

    // the wavetables: X (blue) on top, Y (red) below
    y += 24;
    ofSetColor(255);
    ofDrawBitmapString("wavetables", panelX, y);
    ofPushMatrix();
    ofTranslate(panelX, y + 8);
    ofScale(panelW / canvasSize, 200 / canvasSize);
    xy.drawWaveform();
    ofPopMatrix();

    // what's going out of the sound card: left on top, right below
    y += 230;
    ofSetColor(255);
    ofDrawBitmapString("audio out (L / R)", panelX, y);
    ofPushMatrix();
    ofTranslate(panelX, y + 8);
    ofScale(panelW / canvasSize, 200 / canvasSize);
    xy.drawWave(ofColor(50, 255, 50));
    ofPopMatrix();

    y += 236;
    ofSetColor(160);
    std::string keys =
        "1-4   scenes\n"
        "f     next font\n"
        "r     record " + std::string(xy.isRecording() ? "(RECORDING)" : "") + "\n"
        "e     export 10s offline\n"
        "c     clear drawing\n"
        "d     debug view";
    ofDrawBitmapString(keys, panelX, y);

    ofSetColor(xy.isRecording() ? ofColor(255, 60, 60) : ofColor(120));
    ofDrawBitmapString(status, panelX, ofGetHeight() - 30);
}

//--------------------------------------------------------------
void ofApp::exportAnimation(float seconds) {
    // A second XYscope with no sound card renders the animation offline,
    // frame by frame, without disturbing the live output.
    XYscope exporter;
    exporter.setup(canvasSize, canvasSize, xy.sampleRate(), xy.bufferSize());
    exporter.freq(xy.freq().x);
    exporter.recorderBegin("export");

    int frames = int(seconds * 60);
    for (int f = 0; f < frames; f++) {
        exporter.clearWaves();
        drawScene(exporter, f / 60.0f);
        exporter.buildWaves();
        exporter.process(1 / 60.0f, 3); // X, Y and Z
    }

    std::string path = exporter.recorderEnd();
    status = "exported to bin/data/\n" + ofFilePath::getFileName(path);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
    if (key >= '1' && key <= '4') {
        scene = key - '1';
    } else if (key == 'f') {
        fontIndex = (fontIndex + 1) % fontNames.size();
        scene = 1;
    } else if (key == 'r') {
        if (xy.isRecording()) {
            std::string path = xy.recorderEnd();
            status = path.empty() ? "nothing recorded" : "saved to bin/data/\n" + ofFilePath::getFileName(path);
        } else {
            xy.recorderBegin("XYscope");
            status = "recording...";
        }
    } else if (key == 'e') {
        exportAnimation(10);
    } else if (key == 'c') {
        drawing.clear();
    } else if (key == 'd') {
        xy.debugView(!xy.debugView());
    }
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
    if (scene != 3 || x >= canvasSize) return;
    drawing.emplace_back();
    drawing.back().addVertex(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
    if (scene != 3 || drawing.empty() || x >= canvasSize) return;
    auto & line = drawing.back();
    // skip tiny steps, they only cost points
    if (glm::distance(glm::vec2(line.getVertices().back()), glm::vec2(x, y)) > 4) {
        line.addVertex(x, y);
    }
}
