#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

Hershey single-stroke vector fonts, the text engine from XYscope.java
(which credits https://github.com/ixd-hof/HersheyFont).

Hershey glyphs are made of open strokes rather than filled outlines, so
they draw cleanly with a single beam. Fonts load from .jhf files: give
load() one of the names in getFontNames() to read
data/hershey_fonts/<name>.jhf, or a path to any .jhf file. The "futural"
font is compiled in, so text works even without the data folder.

Differences from the Java original:
* Glyphs are placed with their left and right bearings, the standard
  Hershey layout. XYscope centered each glyph on the pen and then advanced
  by its width, and its width had an operator precedence slip
  (right - left * factor), so spacing here is tighter and stays
  proportional at every text size.
* The .jhf parser counts the vertices each glyph declares, so it copes with
  glyphs that wrap onto several lines.
*/

#include "ofMain.h"

class HersheyFont {

    public:

        struct Glyph {
            int left = 0;
            int right = 0;
            // Each stroke is a run of points in Hershey units:
            // x right and y down, (0,0) near the middle of a capital letter.
            std::vector<std::vector<glm::vec2>> strokes;
        };

        HersheyFont();

        bool load(const std::string & nameOrPath);
        bool loadFromString(const std::string & jhf, const std::string & name = "");

        bool isLoaded() const { return !glyphs.empty(); }
        const std::string & getName() const { return name; }
        static const std::vector<std::string> & getFontNames();

        // Glyphs start at ASCII 32 (space).
        const Glyph * getGlyph(uint32_t codePoint) const;

        // Lay out a string (with \n for line breaks) as strokes in pixels.
        // size is the height of a capital letter, leading is the distance
        // from one baseline to the next.
        std::vector<ofPolyline> getStrokes(const std::string & text, float x, float y, float size, float leading,
                                           ofAlignHorz alignX = OF_ALIGN_HORZ_LEFT,
                                           ofAlignVert alignY = OF_ALIGN_VERT_TOP) const;

        // Width of the widest line, in pixels.
        float getWidth(const std::string & text, float size) const;

        // Hershey units from the top of a capital letter to the baseline.
        static constexpr float CAP_HEIGHT = 21.0f;

    private:

        float getLineWidth(const std::string & line, float factor) const;

        std::vector<Glyph> glyphs;
        std::string name;

};
