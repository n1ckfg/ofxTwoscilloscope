#include "HersheyFont.h"
#include "HersheyFutural.h"

namespace {

    // Hershey coordinates are stored as characters, offset from 'R'.
    int hershey2coord(char c) {
        return int(c) - int('R');
    }

    // In Hershey units, the top of a capital is at y = -12 and the baseline at y = 9.
    const float HERSHEY_BASELINE = 9.0f;

    std::vector<std::string> splitLines(const std::string & text) {
        std::vector<std::string> lines;
        std::string line;
        for (char c : text) {
            if (c == '\n') {
                lines.push_back(line);
                line.clear();
            } else if (c != '\r') {
                line += c;
            }
        }
        lines.push_back(line);
        return lines;
    }

}

//--------------------------------------------------------------
HersheyFont::HersheyFont() {
    loadFromString(HERSHEY_FUTURAL_JHF, "futural");
}

//--------------------------------------------------------------
const std::vector<std::string> & HersheyFont::getFontNames() {
    static const std::vector<std::string> names = {
        "astrology", "cursive", "cyrilc_1", "cyrillic", "futural", "futuram", "gothgbt", "gothgrt",
        "gothiceng", "gothicger", "gothicita", "gothitt", "greek", "greekc", "greeks", "japanese",
        "markers", "mathlow", "mathupp", "meteorology", "music", "rowmand", "rowmans", "rowmant",
        "scriptc", "scripts", "symbolic", "timesg", "timesi", "timesib", "timesr", "timesrb"
    };
    return names;
}

//--------------------------------------------------------------
bool HersheyFont::load(const std::string & nameOrPath) {
    const auto & names = getFontNames();
    bool isName = std::find(names.begin(), names.end(), nameOrPath) != names.end();
    std::string path = isName ? "hershey_fonts/" + nameOrPath + ".jhf" : nameOrPath;

    ofFile file(ofToDataPath(path, true));
    if (!file.exists()) {
        if (nameOrPath == "futural") {
            return loadFromString(HERSHEY_FUTURAL_JHF, "futural");
        }
        ofLogError("HersheyFont") << "couldn't find " << file.getAbsolutePath()
            << " (copy the addon's data/hershey_fonts folder into bin/data)";
        return false;
    }

    ofBuffer buffer = file.readToBuffer();
    return loadFromString(buffer.getText(), isName ? nameOrPath : file.getBaseName());
}

//--------------------------------------------------------------
bool HersheyFont::loadFromString(const std::string & jhf, const std::string & _name) {
    std::vector<Glyph> parsed;

    // Each glyph starts with a 5 character id and a 3 character vertex count,
    // followed by that many coordinate pairs. The first pair holds the left and
    // right bearings, and " R" lifts the pen. A long glyph may wrap onto the
    // next line, so keep reading until all of its pairs have been collected.
    std::string data;
    int wanted = 0;

    auto finishGlyph = [&]() {
        Glyph glyph;
        glyph.left = hershey2coord(data[0]);
        glyph.right = hershey2coord(data[1]);
        std::vector<glm::vec2> stroke;
        for (size_t i = 2; i + 1 < data.size(); i += 2) {
            if (data[i] == ' ' && data[i + 1] == 'R') {
                if (stroke.size() > 1) glyph.strokes.push_back(stroke);
                stroke.clear();
            } else {
                stroke.emplace_back(hershey2coord(data[i]), hershey2coord(data[i + 1]));
            }
        }
        if (stroke.size() > 1) glyph.strokes.push_back(stroke);
        parsed.push_back(glyph);
        data.clear();
        wanted = 0;
    };

    for (std::string line : ofSplitString(jhf, "\n", false, false)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (wanted == 0) {
            if (line.size() < 8) continue;
            wanted = ofToInt(ofTrim(line.substr(5, 3))) * 2;
            if (wanted <= 0) {
                wanted = 0;
                continue;
            }
            line = line.substr(8);
        }

        data += line.substr(0, wanted - data.size());
        if (int(data.size()) >= wanted) finishGlyph();
    }

    if (parsed.empty()) {
        ofLogError("HersheyFont") << "no glyphs found in " << (_name.empty() ? "font data" : _name);
        return false;
    }

    glyphs = parsed;
    name = _name;
    return true;
}

//--------------------------------------------------------------
const HersheyFont::Glyph * HersheyFont::getGlyph(uint32_t codePoint) const {
    if (codePoint < 32) return nullptr;
    size_t index = codePoint - 32;
    return index < glyphs.size() ? &glyphs[index] : nullptr;
}

//--------------------------------------------------------------
float HersheyFont::getLineWidth(const std::string & line, float factor) const {
    float width = 0;
    const Glyph * space = getGlyph(' ');
    for (uint32_t c : ofUTF8Iterator(line)) {
        const Glyph * glyph = getGlyph(c);
        if (!glyph) glyph = space;
        if (glyph) width += (glyph->right - glyph->left) * factor;
    }
    return width;
}

//--------------------------------------------------------------
float HersheyFont::getWidth(const std::string & text, float size) const {
    float factor = size / CAP_HEIGHT;
    float width = 0;
    for (const auto & line : splitLines(text)) {
        width = std::max(width, getLineWidth(line, factor));
    }
    return width;
}

//--------------------------------------------------------------
std::vector<ofPolyline> HersheyFont::getStrokes(const std::string & text, float x, float y, float size, float leading,
                                                ofAlignHorz alignX, ofAlignVert alignY) const {
    std::vector<ofPolyline> result;
    float factor = size / CAP_HEIGHT;
    std::vector<std::string> lines = splitLines(text);
    float blockHeight = size + (lines.size() - 1) * leading;

    // baseline of the first line
    float baseline = y;
    switch (alignY) {
        case OF_ALIGN_VERT_TOP:
            baseline = y + size;
            break;
        case OF_ALIGN_VERT_CENTER:
            baseline = y - blockHeight / 2 + size;
            break;
        case OF_ALIGN_VERT_BOTTOM:
            baseline = y - (lines.size() - 1) * leading;
            break;
        default:
            break;
    }

    const Glyph * space = getGlyph(' ');
    for (const auto & line : lines) {
        float penX = x;
        if (alignX == OF_ALIGN_HORZ_CENTER) {
            penX -= getLineWidth(line, factor) / 2;
        } else if (alignX == OF_ALIGN_HORZ_RIGHT) {
            penX -= getLineWidth(line, factor);
        }

        for (uint32_t c : ofUTF8Iterator(line)) {
            const Glyph * glyph = getGlyph(c);
            if (!glyph) glyph = space;
            if (!glyph) continue;

            for (const auto & stroke : glyph->strokes) {
                ofPolyline polyline;
                for (const auto & p : stroke) {
                    polyline.addVertex(penX + (p.x - glyph->left) * factor,
                                       baseline + (p.y - HERSHEY_BASELINE) * factor);
                }
                result.push_back(polyline);
            }
            penX += (glyph->right - glyph->left) * factor;
        }
        baseline += leading;
    }

    return result;
}
