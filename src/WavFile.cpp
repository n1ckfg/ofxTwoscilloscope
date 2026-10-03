#include "WavFile.h"

namespace {

    uint16_t readU16(const unsigned char * p) {
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }

    uint32_t readU32(const unsigned char * p) {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }

    void writeU16(std::ofstream & out, uint16_t v) {
        char b[2] = { char(v & 0xff), char((v >> 8) & 0xff) };
        out.write(b, 2);
    }

    void writeU32(std::ofstream & out, uint32_t v) {
        char b[4] = { char(v & 0xff), char((v >> 8) & 0xff), char((v >> 16) & 0xff), char((v >> 24) & 0xff) };
        out.write(b, 4);
    }

    const uint16_t FORMAT_PCM = 1;
    const uint16_t FORMAT_FLOAT = 3;
    const uint16_t FORMAT_EXTENSIBLE = 0xFFFE;

}

//--------------------------------------------------------------
bool WavFile::load(const std::string & path, ofSoundBuffer & buffer) {
    std::string fullPath = ofToDataPath(path, true);
    std::ifstream in(fullPath, std::ios::binary);
    if (!in) {
        ofLogError("WavFile") << "couldn't open " << fullPath;
        return false;
    }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    size_t size = data.size();

    if (size < 12 || std::memcmp(&data[0], "RIFF", 4) != 0 || std::memcmp(&data[8], "WAVE", 4) != 0) {
        ofLogError("WavFile") << fullPath << " is not a RIFF/WAVE file";
        return false;
    }

    uint16_t format = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bits = 0;
    size_t dataPos = 0;
    size_t dataLen = 0;
    bool haveFmt = false;
    bool haveData = false;

    size_t pos = 12;
    while (pos + 8 <= size) {
        const unsigned char * chunk = &data[pos];
        uint32_t len = readU32(chunk + 4);
        size_t body = pos + 8;

        if (std::memcmp(chunk, "fmt ", 4) == 0 && body + 16 <= size) {
            format = readU16(&data[body]);
            channels = readU16(&data[body + 2]);
            sampleRate = readU32(&data[body + 4]);
            bits = readU16(&data[body + 14]);
            // the real format hides in the first two bytes of the SubFormat GUID
            if (format == FORMAT_EXTENSIBLE && len >= 40 && body + 26 <= size) {
                format = readU16(&data[body + 24]);
            }
            haveFmt = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataPos = body;
            // some writers leave the length at 0 or 0xFFFFFFFF while streaming
            dataLen = (len == 0 || body + len > size) ? size - body : len;
            haveData = true;
        }

        if (haveFmt && haveData) break;
        pos = body + len + (len & 1); // chunks are word aligned
    }

    if (!haveFmt || !haveData || channels == 0 || sampleRate == 0) {
        ofLogError("WavFile") << fullPath << " is missing its fmt or data chunk";
        return false;
    }

    bool isFloat = format == FORMAT_FLOAT;
    if (!(format == FORMAT_PCM || isFloat) ||
        (format == FORMAT_PCM && bits != 8 && bits != 16 && bits != 24 && bits != 32) ||
        (isFloat && bits != 32 && bits != 64)) {
        ofLogError("WavFile") << fullPath << ": unsupported format " << format << " / " << bits << " bits";
        return false;
    }

    size_t bytesPerSample = bits / 8;
    size_t numFrames = dataLen / (bytesPerSample * channels);
    size_t numSamples = numFrames * channels;

    buffer.allocate(numFrames, channels);
    buffer.setSampleRate(sampleRate);
    std::vector<float> & out = buffer.getBuffer();

    const unsigned char * p = &data[dataPos];
    for (size_t i = 0; i < numSamples; i++, p += bytesPerSample) {
        float v = 0;
        if (isFloat) {
            if (bits == 32) {
                uint32_t u = readU32(p);
                float f;
                std::memcpy(&f, &u, 4);
                v = f;
            } else {
                uint64_t u = uint64_t(readU32(p)) | (uint64_t(readU32(p + 4)) << 32);
                double d;
                std::memcpy(&d, &u, 8);
                v = float(d);
            }
        } else {
            switch (bits) {
                case 8:
                    v = (int(p[0]) - 128) / 128.0f;
                    break;
                case 16:
                    v = int16_t(readU16(p)) / 32768.0f;
                    break;
                case 24: {
                    int32_t s = int32_t(uint32_t(p[0]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 24) >> 8;
                    v = s / 8388608.0f;
                    break;
                }
                case 32:
                    v = int32_t(readU32(p)) / 2147483648.0f;
                    break;
            }
        }
        out[i] = v;
    }

    return true;
}

//--------------------------------------------------------------
bool WavFile::save(const std::string & path, const ofSoundBuffer & buffer, Format format) {
    std::string fullPath = ofToDataPath(path, true);
    ofFilePath::createEnclosingDirectory(fullPath);
    std::ofstream out(fullPath, std::ios::binary);
    if (!out) {
        ofLogError("WavFile") << "couldn't write " << fullPath;
        return false;
    }

    uint16_t channels = std::max<size_t>(1, buffer.getNumChannels());
    uint32_t sampleRate = buffer.getSampleRate() > 0 ? buffer.getSampleRate() : 44100;
    uint16_t bits = uint16_t(format);
    uint16_t bytesPerSample = bits / 8;
    const std::vector<float> & samples = buffer.getBuffer();
    uint32_t dataLen = uint32_t(samples.size() * bytesPerSample);

    out.write("RIFF", 4);
    writeU32(out, 36 + dataLen);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeU32(out, 16);
    writeU16(out, format == FLOAT_32 ? FORMAT_FLOAT : FORMAT_PCM);
    writeU16(out, channels);
    writeU32(out, sampleRate);
    writeU32(out, sampleRate * channels * bytesPerSample);
    writeU16(out, channels * bytesPerSample);
    writeU16(out, bits);
    out.write("data", 4);
    writeU32(out, dataLen);

    std::vector<char> bytes(dataLen);
    char * p = bytes.data();
    for (float s : samples) {
        if (format == FLOAT_32) {
            uint32_t u;
            std::memcpy(&u, &s, 4);
            for (int b = 0; b < 4; b++) *p++ = char((u >> (8 * b)) & 0xff);
        } else {
            float c = ofClamp(s, -1.0f, 1.0f);
            if (format == PCM_16) {
                int16_t v = int16_t(std::lround(c * 32767.0f));
                *p++ = char(v & 0xff);
                *p++ = char((v >> 8) & 0xff);
            } else {
                int32_t v = int32_t(std::lround(c * 8388607.0f));
                *p++ = char(v & 0xff);
                *p++ = char((v >> 8) & 0xff);
                *p++ = char((v >> 16) & 0xff);
            }
        }
    }
    out.write(bytes.data(), bytes.size());

    return bool(out);
}
