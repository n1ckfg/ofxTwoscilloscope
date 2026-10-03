#pragma once

/*
+ + +   ofxTwoscilloscope   + + +

A port of OsciMesh from Hansi Raber's Oscilloscope app, the renderer at the
heart of the audio-to-vector half of the addon.

Every pair of neighbouring samples becomes a quad around the line between
them, and a shader fills the quad with the light a gaussian electron beam
leaves as it sweeps along that line (the technique from m1el's woscope).
A beam that moves quickly between two samples spreads its light thinly, so
fast strokes come out dim and slow ones bright, the way they do on a real
CRT. Drawn additively into a slowly fading FBO (see Oscilloscope), this is
what gives the image its glow and persistence.

Differences from the original:
* The shader is built into the class and compiled for whichever renderer
  is running (GL 2, GL 3+ or GLES), instead of being hot-loaded from
  bin/data/shaders/osci.vert/.frag.
* Points are in scope units (-1..1). draw() uses the current
  openFrameworks matrices, where the original passed in its own view matrix.
* The segment data rides in the vertex normals instead of the colors.
*/

#include "ofMain.h"

class OsciMesh {

    public:

        OsciMesh();

        // Add many lines at once.
        // left: x coordinates (-1..1)
        // right: y coordinates (-1..1)
        // bright: brightness (0..1), or nullptr for full brightness
        // stride: step between samples in left and right (not bright)
        void addLines(const float * left, const float * right, const float * bright, int n, int stride = 1);

        // Add one line from a to b (-1..1), with brightness 0..1.
        void addLine(const glm::vec2 & a, const glm::vec2 & b, float bright);

        void draw();
        void clear();

        // the original's shader parameters
        float uSize = 0.01f;           // beam radius in scope units
        glm::vec3 uRgb = glm::vec3(1);  // beam color
        float uIntensity = 1;

        // it's a mesh. can you believe it?
        ofMesh mesh;

    private:

        bool loadShader();

        ofShader shader;
        bool shaderTried = false;
        glm::vec2 last = glm::vec2(0);

};
