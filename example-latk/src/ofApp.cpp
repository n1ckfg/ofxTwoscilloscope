#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
	latk = Latk("jellyfish.latk");

	// ofEasyCam normally hooks up its mouse and update listeners in begin(),
	// but the strokes are projected by hand and never drawn inside it.
	cam.setEvents(ofEvents());

	scope.setup(44100);

	// The effect chain from ofxTwoscilloscope's example-transform, in order.
	// Every setting shows up in the panel.
	auto & effects = scope.transformer.effects;
	auto lowPass = effects.add<XYLowPass>();
	lowPass->cutoff = 1500;
	auto delay = effects.add<XYChannelDelay>();
	delay->delayY = 0.6f;
	effects.add<XYHighPass>();
	effects.add<XYEcho>();
	effects.add<XYRingMod>();
	effects.add<XYRotate>();
	effects.add<XYDrive>();
	effects.add<XYWavefold>();
	effects.add<XYBitCrush>();
	effects.add<XYSampleHold>();
	effects.add<XYNoise>();
	for (size_t i = 2; i < effects.effects.size(); i++) {
		effects.effects[i]->enabled = false;
	}

	gui.setup(effects.parameters, "effects.xml", 10, 10);
	gui.add(scope.parameters);
	for (auto & effect : effects.effects) {
		if (!effect->enabled) gui.getGroup(effect->getName()).minimize();
	}

	// The altered loop plays out of the sound card, X left and Y right, so
	// what you hear is what you see.
	player.setup(0, 0, 44100, 512);
	status = player.openAudioOut() ? "the altered strokes are playing on the default audio out"
		: "no sound card found, running silently";
}

//--------------------------------------------------------------
void ofApp::update() {
	// Latk::run() advances and draws. Only advance here; draw() does the drawing.
	if (latk.checkInterval()) {
		for (auto & layer : latk.layers) layer.nextFrame();
	}
	latk.lastMillis = ofGetElapsedTimeMillis();

	// The whole round trip, every frame: strokes -> audio -> effects -> strokes.
	scope.update(latk, cam, ofRectangle(0, 0, ofGetWidth(), ofGetHeight()));

	// loop the altered audio, Z (blanking) included
	vector<float> x, y, z;
	scope.transformer.getProcessedCycle(x, y, z);
	player.freq(scope.getFreq());
	player.setWaveforms(x, y, z);

	// Dragging a slider shouldn't orbit the camera. Leave the camera's mouse
	// off while the pointer is over the panel, and don't switch mid-drag.
	if (!ofGetMousePressed()) {
		const bool overGui = showGui && gui.getShape().inside(ofGetMouseX(), ofGetMouseY());
		if (overGui && cam.getMouseInputEnabled()) cam.disableMouseInput();
		if (!overGui && !cam.getMouseInputEnabled()) cam.enableMouseInput();
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(0);
	string mode;
	switch (view) {
		case BEAMS:
			scope.drawBeams();
			mode = "beams";
			break;
		case STROKES:
			scope.drawStrokes();
			mode = "decoded strokes";
			break;
		case LINES:
			cam.begin();
			// LatkStroke::draw() calls ofNoFill() and leaves it. Restore the style afterwards.
			ofPushStyle();
			for (auto & layer : latk.layers) layer.run();
			ofPopStyle();
			cam.end();
			mode = "original lines";
			break;
	}
	if (showGui) gui.draw();

	const auto & stats = scope.getStats();
	string info = ofToString(ofGetFrameRate(), 0) + " fps | " + mode + " | loop " + ofToString(scope.getFreq(), 1) + " Hz: "
		+ ofToString(stats.samples) + " samples for " + ofToString(stats.pathLength, 0) + " px of " + ofToString(stats.pieces) + " strokes";
	if (stats.dropped > 0) info += " (" + ofToString(stats.dropped) + " too short to fit)";
	info += " | " + ofToString(stats.ms, 1) + " ms";
	ofDrawBitmapStringHighlight(info, 10, ofGetHeight() - 50);
	ofDrawBitmapStringHighlight("l view   e solo next effect   n no effects   m mute   g panel   s save svg   w save wav   o save latk", 10, ofGetHeight() - 30);
	ofDrawBitmapStringHighlight(status, 10, ofGetHeight() - 10);
}

//--------------------------------------------------------------
void ofApp::soloEffect(int index) {
	// turn on one effect at a time, to see what each one does
	soloIndex = index;
	auto & effects = scope.transformer.effects.effects;
	for (size_t i = 0; i < effects.size(); i++) {
		auto & effect = effects[i];
		effect->enabled = int(i) == index;
		if (effect->enabled) gui.getGroup(effect->getName()).maximize();
		else gui.getGroup(effect->getName()).minimize();
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == 'i') {
		// TODO
	}

	if (key == 'l') {
		view = View((view + 1) % 3);
	}

	if (key == 'e') {
		soloEffect((soloIndex + 1) % int(scope.transformer.effects.effects.size()));
		status = "solo: " + scope.transformer.effects.effects[soloIndex]->getName();
	}

	if (key == 'n') {
		soloEffect(-1);
		status = "no effects: the round trip on its own";
	}

	if (key == 'm') {
		if (player.isAudioOutOpen()) {
			player.closeAudioOut();
			status = "muted";
		} else {
			status = player.openAudioOut() ? "playing on the default audio out" : "no sound card found, running silently";
		}
	}

	if (key == 'g') {
		showGui = !showGui;
	}

	if (key == 's') {
		string path = "transformed_" + ofGetTimestampString("%Y%m%d_%H%M%S") + ".svg";
		XYDecoder::saveSvg(path, scope.getStrokes(), ofGetWidth(), ofGetHeight());
		status = "saved " + path;
	}

	if (key == 'w') {
		// four seconds of the altered loop, X Y Z
		string path = "transformed_" + ofGetTimestampString("%Y%m%d_%H%M%S") + ".wav";
		WavFile::save(path, player.render(4, 3));
		status = "saved " + path;
	}

	if (key == 'o') {
		latk.write("test.latk");
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ) {

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
	// ofEasyCam takes its mouse area from the viewport it's begun with, and
	// it's only begun while drawing the original lines.
	cam.setControlArea(ofRectangle(0, 0, w, h));
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) { 

}
