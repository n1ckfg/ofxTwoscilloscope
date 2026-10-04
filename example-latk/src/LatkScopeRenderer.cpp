#include "LatkScopeRenderer.h"

namespace {

// Cuts the segment a-b to the rectangle (Liang-Barsky). Returns false if none
// of it is inside, or sets t0 and t1 to where the inside part starts and ends.
bool clipSegment(const glm::vec2 & a, const glm::vec2 & b, const ofRectangle & r, float & t0, float & t1) {
	const glm::vec2 d = b - a;
	const float p[4] = { -d.x, d.x, -d.y, d.y };
	const float q[4] = { a.x - r.getLeft(), r.getRight() - a.x, a.y - r.getTop(), r.getBottom() - a.y };
	t0 = 0;
	t1 = 1;
	for (int i = 0; i < 4; i++) {
		if (p[i] == 0) {
			// parallel to this edge, so all in or all out
			if (q[i] < 0) return false;
			continue;
		}
		const float t = q[i] / p[i];
		if (p[i] < 0) t0 = std::max(t0, t);
		else t1 = std::min(t1, t);
		if (t0 > t1) return false;
	}
	return true;
}

}

//--------------------------------------------------------------
void LatkScopeRenderer::setup(int _sampleRate) {
	sampleRate = _sampleRate;
	parameters.setName("scope");
	parameters.add(loopFreq.set("loop Hz", 5, 1, 100));
	parameters.add(beamSize.set("beam size", 3, 0.5, 12));
	parameters.add(beamIntensity.set("beam intensity", 1, 0, 4));
}

//--------------------------------------------------------------
void LatkScopeRenderer::update(Latk & latk, const ofCamera & cam, const ofRectangle & viewport) {
	const uint64_t startMicros = ofGetElapsedTimeMicros();
	beamsDirty = true;
	strokesDirty = true;
	if (viewport.width < 1 || viewport.height < 1) return;

	// The loop is a whole number of samples, so XYscope plays it back one
	// table entry per sample.
	cycleFrames = std::max<size_t>(2, std::lround(sampleRate / std::max(0.1f, loopFreq.get())));
	freq = float(sampleRate) / cycleFrames;
	if (viewport != canvas || freq != transformer.getFreq()) {
		transformer.setup(viewport.width, viewport.height, sampleRate, freq);
	}
	canvas = viewport;

	project(latk, cam);
	encode();
	stats.ms = (ofGetElapsedTimeMicros() - startMicros) / 1000.0f;
}

//--------------------------------------------------------------
void LatkScopeRenderer::project(Latk & latk, const ofCamera & cam) {
	pieces.clear();
	// The camera's matrix, computed once, as ofCamera::worldToScreen() would for every point.
	const glm::mat4 mvp = cam.getModelViewProjectionMatrix(canvas);
	const ofRectangle bounds(0, 0, canvas.width, canvas.height);

	for (auto & layer : latk.layers) {
		if (layer.currentFrame < 0 || layer.currentFrame >= (int)layer.frames.size()) continue;

		for (auto & stroke : layer.frames[layer.currentFrame].strokes) {
			bool open = false; // whether the next segment continues the last piece
			bool lastValid = false;
			glm::vec2 last;
			for (auto & p : stroke.points) {
				// LatkStroke::draw() scales its points by globalScale, so match it.
				const glm::vec3 world = p * stroke.globalScale;
				const glm::vec4 clip = mvp * glm::vec4(world, 1);
				const glm::vec3 ndc = glm::vec3(clip) / clip.w;
				// Behind the camera or outside its depth range: break the stroke here.
				const bool valid = clip.w > 0 && ndc.z >= -1 && ndc.z <= 1;
				const glm::vec2 screen((ndc.x + 1) * 0.5f * canvas.width, (1 - ndc.y) * 0.5f * canvas.height);

				float t0, t1;
				if (valid && lastValid && clipSegment(last, screen, bounds, t0, t1)) {
					// The window is the scope's canvas, and past its edges the audio
					// would clip, so cut the stroke where it leaves the window.
					const glm::vec2 a = glm::mix(last, screen, t0);
					const glm::vec2 b = glm::mix(last, screen, t1);
					if (!open || t0 > 0) {
						pieces.emplace_back();
						pieces.back().color = stroke.strokeColor;
						pieces.back().strokeSize = stroke.strokeSize;
						pieces.back().points.push_back(a);
					}
					pieces.back().points.push_back(b);
					pieces.back().length += glm::distance(a, b);
					open = t1 == 1;
				} else {
					open = false;
				}
				last = screen;
				lastValid = valid;
			}
		}
	}

	stats.pathLength = 0;
	for (auto & piece : pieces) stats.pathLength += piece.length;
}

//--------------------------------------------------------------
void LatkScopeRenderer::encode() {
	const size_t n = cycleFrames;
	stats.samples = n;
	stats.dropped = 0;

	// Every piece takes a blank sample that jumps the beam to its start, and at
	// least two lit ones, for its ends. If the loop is too short for that, the
	// shortest pieces are left out.
	const size_t maxPieces = n / 3;
	if (pieces.size() > maxPieces) {
		vector<size_t> order(pieces.size());
		std::iota(order.begin(), order.end(), 0);
		std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return pieces[a].length > pieces[b].length; });
		vector<bool> keep(pieces.size(), false);
		for (size_t i = 0; i < maxPieces; i++) keep[order[i]] = true;
		vector<Piece> kept;
		kept.reserve(maxPieces);
		for (size_t i = 0; i < pieces.size(); i++) {
			if (keep[i]) kept.push_back(std::move(pieces[i]));
		}
		stats.dropped = pieces.size() - kept.size();
		pieces.swap(kept);
	}
	stats.pieces = pieces.size();

	// The rest of the loop is shared out by length, so the beam moves at an
	// even speed, as it does in XYscope's waveforms.
	double totalLength = 0;
	for (auto & piece : pieces) totalLength += piece.length;
	const size_t spare = n - 3 * pieces.size();

	// one loop in XYscope's format: X, Y and Z interleaved, the canvas mapped
	// to -1..1 with +Y up, and Z blanking the beam between pieces
	const XYDecoderSettings & levels = transformer.decoder;
	vector<float> cycle(n * 3);
	size_t i = 0;
	auto write = [&](const glm::vec2 & p, bool lit) {
		cycle[i * 3] = p.x / canvas.width * 2 - 1;
		cycle[i * 3 + 1] = 1 - p.y / canvas.height * 2;
		cycle[i * 3 + 2] = lit ? levels.zMax : levels.zMin;
		i++;
	};

	double before = 0; // length of the pieces so far
	for (size_t k = 0; k < pieces.size(); k++) {
		auto & piece = pieces[k];
		// rounded from running totals, so the shares add up to exactly the spare samples
		const double after = before + piece.length;
		size_t share;
		if (totalLength > 0) {
			share = size_t(std::llround(spare * after / totalLength) - std::llround(spare * before / totalLength));
		} else {
			share = spare * (k + 1) / pieces.size() - spare * k / pieces.size();
		}
		before = after;

		piece.start = i;
		piece.lit = 2 + share;
		write(piece.points.front(), false);

		// lit samples at even steps along the piece, from its first point to its last
		size_t seg = 0;
		float segStart = 0; // length along the piece to points[seg]
		float segLength = glm::distance(piece.points[0], piece.points[1]);
		for (size_t j = 0; j < piece.lit; j++) {
			const float at = piece.length * j / (piece.lit - 1);
			while (seg + 2 < piece.points.size() && segStart + segLength < at) {
				segStart += segLength;
				seg++;
				segLength = glm::distance(piece.points[seg], piece.points[seg + 1]);
			}
			const float t = segLength > 0 ? ofClamp((at - segStart) / segLength, 0, 1) : 1;
			write(glm::mix(piece.points[seg], piece.points[seg + 1], t), true);
		}
	}
	// with nothing to draw, the beam rests blanked in the middle
	while (i < n) write(glm::vec2(canvas.width, canvas.height) * 0.5f, false);

	// XYTransformer runs the effects over a few loops, so that filters and
	// echoes settle, and keeps the last one.
	const size_t loops = std::max(0, transformer.settleCycles) + 1;
	ofSoundBuffer encoded;
	encoded.allocate(n * loops, 3);
	encoded.setSampleRate(sampleRate);
	for (size_t l = 0; l < loops; l++) {
		std::copy(cycle.begin(), cycle.end(), encoded.getBuffer().begin() + l * cycle.size());
	}
	transformer.transform(encoded);
	transformer.getProcessedCycle(x, y, z);
}

//--------------------------------------------------------------
void LatkScopeRenderer::buildBeams() {
	beamsDirty = false;
	beams.clear();
	if (x.size() != cycleFrames) return;

	// Scope units: -1..1 up the canvas, and as far across it as its shape
	// allows, so the beam stays round in any window.
	const float aspect = canvas.width / canvas.height;
	vector<float> sx(x.size());
	for (size_t i = 0; i < x.size(); i++) sx[i] = x[i] * aspect;
	osci.uSize = beamSize / (canvas.height / 2);

	// OsciMesh joins each run of samples to the end of the last one, lit as
	// the run's first sample. Keep that jump dark.
	vector<float> bright(x.size(), 1);
	bright[0] = 0;

	// One mesh per colour: the beams add up, so the drawing order doesn't matter.
	std::map<int, size_t> beamOfColor;
	vector<vector<const Piece *>> members;
	for (const auto & piece : pieces) {
		auto found = beamOfColor.find(piece.color.getHex());
		if (found == beamOfColor.end()) {
			found = beamOfColor.emplace(piece.color.getHex(), beams.size()).first;
			beams.push_back({ piece.color, ofMesh() });
			members.emplace_back();
		}
		members[found->second].push_back(&piece);
	}

	double stepSum = 0;
	size_t steps = 0;
	for (size_t b = 0; b < beams.size(); b++) {
		osci.clear();
		for (const Piece * piece : members[b]) {
			const size_t first = piece->start + 1;
			osci.addLines(&sx[first], &y[first], bright.data(), int(piece->lit));
			for (size_t i = first + 1; i < first + piece->lit; i++) {
				stepSum += glm::distance(glm::vec2(sx[i - 1], y[i - 1]), glm::vec2(sx[i], y[i]));
				steps++;
			}
		}
		std::swap(beams[b].mesh, osci.mesh);
	}

	// A beam leaves less light on a line the faster it moves, so a longer
	// drawing or a shorter loop comes out dimmer. Scale the light by the
	// average step, so a stroke peaks at about beamIntensity either way.
	const float sigma = osci.uSize / 3;
	beamExposure = steps > 0 ? float(stepSum / steps) / (sigma * std::sqrt(TWO_PI)) : 1;
}

//--------------------------------------------------------------
void LatkScopeRenderer::drawBeams() {
	if (beamsDirty) buildBeams();

	ofPushMatrix();
	ofTranslate(canvas.getCenter());
	// scope +Y is up
	ofScale(canvas.height / 2, -canvas.height / 2);
	osci.uIntensity = beamIntensity * beamExposure;
	for (auto & beam : beams) {
		std::swap(osci.mesh, beam.mesh);
		osci.uRgb = glm::vec3(beam.color.r, beam.color.g, beam.color.b);
		osci.draw();
		std::swap(osci.mesh, beam.mesh);
	}
	ofPopMatrix();
}

//--------------------------------------------------------------
void LatkScopeRenderer::decodeStrokes() {
	strokesDirty = false;
	strokes.clear();
	strokePieces.clear();
	if (x.size() != cycleFrames) return;

	XYDecoderSettings settings = transformer.decoder;
	settings.width = canvas.width;
	settings.height = canvas.height;
	settings.sampleRate = sampleRate;
	settings.freq = freq;
	for (size_t k = 0; k < pieces.size(); k++) {
		// Each piece's samples, blank and all, decoded on their own so that
		// whatever the effects made of them keeps the piece's colour.
		const Piece & piece = pieces[k];
		const size_t s = piece.start;
		for (auto & line : XYDecoder::decodeCycle(&x[s], &y[s], z.empty() ? nullptr : &z[s], piece.lit + 1, settings)) {
			strokes.push_back(std::move(line));
			strokePieces.push_back(k);
		}
	}
}

//--------------------------------------------------------------
const vector<ofPolyline> & LatkScopeRenderer::getStrokes() {
	if (strokesDirty) decodeStrokes();
	return strokes;
}

//--------------------------------------------------------------
void LatkScopeRenderer::drawStrokes() {
	if (strokesDirty) decodeStrokes();

	ofPushStyle();
	ofPushMatrix();
	ofTranslate(canvas.x, canvas.y);
	ofNoFill();
	for (size_t i = 0; i < strokes.size(); i++) {
		const Piece & piece = pieces[strokePieces[i]];
		ofSetLineWidth(piece.strokeSize);
		ofSetColor(piece.color);
		strokes[i].draw();
	}
	ofPopMatrix();
	ofPopStyle();
}
