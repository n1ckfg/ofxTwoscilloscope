#include "StreamResampler.h"

//--------------------------------------------------------------
void StreamResampler::setup(double _inRate, double _outRate, Interpolation _interpolation) {
    inRate = std::max(1.0, _inRate);
    outRate = std::max(1.0, _outRate);
    interpolation = _interpolation;
    taps = interpolation == SINC ? 4 : 1;
    step = inRate / outRate;
    reset();
}

//--------------------------------------------------------------
void StreamResampler::reset() {
    // start with a little silence, so the first samples have neighbours
    history.assign(taps, 0.0f);
    pos = taps;
}

//--------------------------------------------------------------
float StreamResampler::kernel(double x) const {
    if (x == 0) return 1;
    double a = taps;
    if (std::abs(x) >= a) return 0;
    double px = PI * x;
    return float(a * std::sin(px) * std::sin(px / a) / (px * px));
}

//--------------------------------------------------------------
void StreamResampler::process(const float * in, size_t n, size_t stride, std::vector<float> & out) {
    for (size_t i = 0; i < n; i++) history.push_back(in[i * stride]);

    // an output sample at pos needs input samples up to floor(pos) + taps
    while (std::floor(pos) + taps < history.size()) {
        long i0 = long(std::floor(pos));
        double frac = pos - i0;

        if (interpolation == LINEAR) {
            out.push_back(float(history[i0] + frac * (history[i0 + 1] - history[i0])));
        } else {
            double sum = 0;
            double weights = 0;
            for (long k = i0 - taps + 1; k <= i0 + taps; k++) {
                double w = kernel(pos - k);
                sum += w * history[k];
                weights += w;
            }
            out.push_back(float(weights != 0 ? sum / weights : 0));
        }
        pos += step;
    }

    // drop the samples that no future output will need
    long keepFrom = long(std::floor(pos)) - taps + 1;
    if (keepFrom > 0) {
        keepFrom = std::min<long>(keepFrom, history.size());
        history.erase(history.begin(), history.begin() + keepFrom);
        pos -= keepFrom;
    }
}
