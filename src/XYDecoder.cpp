#include "XYDecoder.h"

namespace {

    std::vector<ofPolyline> decodeSamples(const float * x, const float * y, const float * z, size_t n,
                                          const XYDecoderSettings & s, bool cyclic) {
        std::vector<ofPolyline> result;
        if (n < 2) return result;

        // samples -> canvas, inverting XYscope's mapping
        std::vector<glm::vec2> pts(n);
        for (size_t i = 0; i < n; i++) {
            pts[i] = glm::vec2((x[i] + 1) * 0.5f * s.width, (1 - y[i]) * 0.5f * s.height);
        }

        std::vector<bool> blank(n, false);
        if (z && s.useZ && s.zMax != s.zMin) {
            for (size_t i = 0; i < n; i++) {
                blank[i] = (z[i] - s.zMin) / (s.zMax - s.zMin) < s.zThreshold;
            }
        }

        // step[i] is the distance from the previous sample (wrapping, for a loop)
        std::vector<float> step(n);
        for (size_t i = 1; i < n; i++) step[i] = glm::distance(pts[i], pts[i - 1]);
        step[0] = cyclic ? glm::distance(pts[0], pts[n - 1]) : 0;

        std::vector<float> moving;
        for (size_t i = 1; i < n; i++) {
            if (step[i] > 1e-4f && !blank[i]) moving.push_back(step[i]);
        }
        float median = 0;
        if (!moving.empty()) {
            std::nth_element(moving.begin(), moving.begin() + moving.size() / 2, moving.end());
            median = moving[moving.size() / 2];
        }
        float threshold = s.jumpThreshold > 0 ? s.jumpThreshold : std::max(1.0f, s.jumpFactor * median);
        float closeThreshold = s.closeThreshold > 0 ? s.closeThreshold : 2.5f * median;

        // walk the samples, cutting at blanks and jumps
        struct Stroke {
            size_t start;
            size_t end; // inclusive
            std::vector<glm::vec2> points;
        };
        std::vector<Stroke> strokes;
        bool open = false;
        for (size_t i = 0; i < n; i++) {
            if (blank[i]) {
                // the beam is still where the stroke ended when it blanks
                // (XYscope blanks right on a shape's last point), so keep that spot
                if (open && step[i] <= threshold) {
                    strokes.back().points.push_back(pts[i]);
                    strokes.back().end = i;
                }
                open = false;
                continue;
            }
            if (open && step[i] > threshold) open = false;
            if (!open) {
                strokes.push_back({ i, i, {} });
                open = true;
            }
            strokes.back().points.push_back(pts[i]);
            strokes.back().end = i;
        }

        // the cycle loops, so a stroke running off the end continues at the start
        bool joined = cyclic && !blank[0] && !blank[n - 1] && step[0] <= threshold;
        bool closedLoop = false;
        if (joined && !strokes.empty() && strokes.front().start == 0 && strokes.back().end == n - 1) {
            if (strokes.size() > 1) {
                auto & last = strokes.back();
                last.points.insert(last.points.end(), strokes.front().points.begin(), strokes.front().points.end());
                strokes.erase(strokes.begin());
            } else {
                closedLoop = true; // one unbroken loop
            }
        }

        for (const auto & stroke : strokes) {
            ofPolyline poly;
            for (const auto & p : stroke.points) {
                if (poly.size() == 0 || glm::distance(glm::vec2(poly.getVertices().back()), p) > 1e-3f) {
                    poly.addVertex(p.x, p.y);
                }
            }
            // XYscope blanks a closed shape's last point, which leaves a gap of a sample or two
            if (closedLoop || (s.closeThreshold >= 0 && poly.size() > 3 &&
                glm::distance(glm::vec2(poly.getVertices().front()), glm::vec2(poly.getVertices().back())) <= closeThreshold)) {
                poly.setClosed(true);
            }
            if (s.simplify > 0 && poly.size() > 2) poly.simplify(s.simplify);
            if (int(poly.size()) < s.minPoints) continue;
            if (s.minLength > 0 && poly.getPerimeter() < s.minLength) continue;
            result.push_back(poly);
        }

        return result;
    }

}

//--------------------------------------------------------------
std::vector<ofPolyline> XYDecoder::decodeCycle(const float * x, const float * y, const float * z, size_t n,
                                               const XYDecoderSettings & settings) {
    return decodeSamples(x, y, z, n, settings, true);
}

//--------------------------------------------------------------
std::vector<ofPolyline> XYDecoder::decode(const float * x, const float * y, const float * z, size_t n,
                                          const XYDecoderSettings & settings) {
    float period = 0;
    if (settings.freq > 0) {
        period = settings.sampleRate / settings.freq;
    } else {
        period = detectPeriod(x, y, n, settings.sampleRate / std::max(1.0f, settings.maxFreq),
                              settings.sampleRate / std::max(1.0f, settings.minFreq));
    }

    size_t m = size_t(std::lround(period));
    if (m < 2 || m > n) {
        // nothing loops: decode everything we have, as one open run
        return decodeSamples(x, y, z, n, settings, false);
    }

    size_t start = n - m;
    return decodeSamples(x + start, y + start, z ? z + start : nullptr, m, settings, true);
}

//--------------------------------------------------------------
std::vector<ofPolyline> XYDecoder::decode(const ofSoundBuffer & buffer, XYDecoderSettings settings) {
    size_t nCh = buffer.getNumChannels();
    size_t n = buffer.getNumFrames();
    if (nCh == 0 || n == 0) return {};
    if (buffer.getSampleRate() > 0) settings.sampleRate = buffer.getSampleRate();

    std::vector<float> x(n), y(n), z;
    if (nCh == 1) {
        // a mono signal is a waveform: time across, signal up
        for (size_t i = 0; i < n; i++) {
            x[i] = -1 + 2.0f * i / std::max<size_t>(1, n - 1);
            y[i] = buffer[i];
        }
        return decodeSamples(x.data(), y.data(), nullptr, n, settings, false);
    }

    for (size_t i = 0; i < n; i++) {
        x[i] = buffer[i * nCh];
        y[i] = buffer[i * nCh + 1];
    }
    if (nCh == 3) {
        z.resize(n);
        for (size_t i = 0; i < n; i++) z[i] = buffer[i * nCh + 2];
    }
    return decode(x.data(), y.data(), z.empty() ? nullptr : z.data(), n, settings);
}

//--------------------------------------------------------------
// After YIN (de Cheveigné & Kawahara 2002), on both channels at once.
static float yinPeriod(const float * x, const float * y, size_t n, float minPeriod, float maxPeriod) {
    long minLag = std::max(2L, long(std::floor(minPeriod)));
    long maxLag = std::min(long(n / 2), long(std::ceil(maxPeriod)));
    if (maxLag <= minLag + 1) return 0;

    // compare the most recent window against itself, shifted by up to maxLag + 1
    long window = std::min(long(n) - maxLag - 1, 2048L);
    long t0 = long(n) - maxLag - 1 - window;
    if (window < 1) return 0;

    double energy = 0;
    for (long t = t0; t < t0 + window; t++) energy += x[t] * x[t] + y[t] * y[t];
    if (energy < 1e-9 * window) return 0; // silence

    std::vector<double> d(maxLag + 2, 0.0);
    for (long tau = 1; tau <= maxLag + 1; tau++) {
        double sum = 0;
        for (long t = t0; t < t0 + window; t++) {
            double dx = x[t] - x[t + tau];
            double dy = y[t] - y[t + tau];
            sum += dx * dx + dy * dy;
        }
        d[tau] = sum;
    }

    // cumulative mean normalized difference
    std::vector<double> dn(maxLag + 2, 1.0);
    double running = 0;
    for (long tau = 1; tau <= maxLag + 1; tau++) {
        running += d[tau];
        dn[tau] = running > 0 ? d[tau] * tau / running : 1.0;
    }

    long best = -1;
    const double threshold = 0.1;
    for (long tau = minLag; tau <= maxLag; tau++) {
        if (dn[tau] < threshold) {
            while (tau + 1 <= maxLag && dn[tau + 1] < dn[tau]) tau++;
            best = tau;
            break;
        }
    }
    if (best < 0) {
        best = minLag;
        for (long tau = minLag; tau <= maxLag; tau++) {
            if (dn[tau] < dn[best]) best = tau;
        }
        if (dn[best] > 0.5) return 0; // not periodic enough to trust
    }

    // parabolic interpolation for a fractional period
    double refined = best;
    if (best > 1 && best < maxLag + 1) {
        double a = dn[best - 1], b = dn[best], c = dn[best + 1];
        double denom = a - 2 * b + c;
        if (std::abs(denom) > 1e-12) refined = best + 0.5 * (a - c) / denom;
    }
    return float(refined);
}

//--------------------------------------------------------------
float XYDecoder::detectPeriod(const float * x, const float * y, size_t n, float minPeriod, float maxPeriod) {
    // Search a decimated copy first, so long periods stay cheap...
    int factor = std::max(1, int(std::ceil(maxPeriod / 512.0f)));
    if (factor == 1) return yinPeriod(x, y, n, minPeriod, maxPeriod);

    size_t m = n / factor;
    size_t offset = n - m * factor; // line the blocks up with the newest sample
    std::vector<float> xd(m), yd(m);
    for (size_t i = 0; i < m; i++) {
        float sx = 0, sy = 0;
        for (int k = 0; k < factor; k++) {
            sx += x[offset + i * factor + k];
            sy += y[offset + i * factor + k];
        }
        xd[i] = sx / factor;
        yd[i] = sy / factor;
    }
    float coarse = yinPeriod(xd.data(), yd.data(), m, minPeriod / factor, maxPeriod / factor);
    if (coarse <= 0) return 0;

    // ...then refine around its answer at full resolution.
    long center = std::lround(coarse * factor);
    long lo = std::max(2L, center - 2 * factor);
    long hi = center + 2 * factor;
    long window = std::min(long(n) - hi - 2, 1024L);
    if (window < 64) return coarse * factor;
    long t0 = long(n) - hi - 2 - window;

    std::vector<double> d(hi - lo + 3, 0.0);
    for (long tau = lo - 1; tau <= hi + 1; tau++) {
        if (tau < 1) continue;
        double sum = 0;
        for (long t = t0; t < t0 + window; t++) {
            double dx = x[t] - x[t + tau];
            double dy = y[t] - y[t + tau];
            sum += dx * dx + dy * dy;
        }
        d[tau - lo + 1] = sum;
    }
    long best = lo;
    for (long tau = lo; tau <= hi; tau++) {
        if (d[tau - lo + 1] < d[best - lo + 1]) best = tau;
    }
    double a = d[best - lo], b = d[best - lo + 1], c = d[best - lo + 2];
    double denom = a - 2 * b + c;
    double refined = best;
    if (best - 1 >= 1 && std::abs(denom) > 1e-12) refined = best + 0.5 * (a - c) / denom;
    return float(refined);
}

//--------------------------------------------------------------
float XYDecoder::periodError(const float * x, const float * y, size_t n, float period) {
    long p = std::lround(period);
    if (p < 1 || size_t(p) + 16 > n) return 1;
    long window = std::min(long(n) - p, 1024L);
    long t0 = long(n) - p - window;

    double mx = 0, my = 0;
    for (long t = t0; t < t0 + window + p; t++) {
        mx += x[t];
        my += y[t];
    }
    mx /= window + p;
    my /= window + p;

    double diff = 0, energy = 0;
    for (long t = t0; t < t0 + window; t++) {
        double ax = x[t] - mx, ay = y[t] - my;
        double bx = x[t + p] - mx, by = y[t + p] - my;
        diff += (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
        energy += ax * ax + ay * ay + bx * bx + by * by;
    }
    return energy > 1e-12 ? float(diff / energy) : 1;
}

//--------------------------------------------------------------
bool XYDecoder::saveSvg(const std::string & path, const std::vector<ofPolyline> & shapes,
                        float width, float height, const ofColor & stroke, float strokeWidth) {
    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height
        << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
    char color[8];
    std::snprintf(color, sizeof(color), "#%02x%02x%02x", stroke.r, stroke.g, stroke.b);

    for (const auto & shape : shapes) {
        const auto & verts = shape.getVertices();
        if (verts.size() < 2) continue;
        svg << "  <path fill=\"none\" stroke=\"" << color << "\" stroke-width=\"" << strokeWidth
            << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\" d=\"M";
        for (size_t i = 0; i < verts.size(); i++) {
            svg << (i == 0 ? " " : " L ") << ofToString(verts[i].x, 2) << " " << ofToString(verts[i].y, 2);
        }
        if (shape.isClosed()) svg << " Z";
        svg << "\"/>\n";
    }
    svg << "</svg>\n";

    ofBuffer buffer;
    buffer.set(svg.str());
    return ofBufferToFile(path, buffer);
}
