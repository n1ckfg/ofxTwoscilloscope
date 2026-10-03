//
//  OsciMesh.cpp
//  Oscilloscope
//
//  Created by Hansi on 12.01.17.
//  Ported to ofxTwoscilloscope.
//

#include "OsciMesh.h"

#define EPS 1E-6

namespace {

    // The beam: the light a gaussian spot leaves as it sweeps along one
    // segment, integrated analytically with erf (after m1el's woscope).
    // vUvl.x runs along the segment, vUvl.y across it, vUvl.z is its length.
    // Normalized so a beam that stands still peaks at 1.
    const std::string BEAM_FUNCTIONS = R"GLSL(
        #define SQRT2 1.4142135623730951
        #define TAUR 2.5066282746310002

        // approximates the error function, needed for the gaussian integral
        float erfApprox(float x) {
            float s = sign(x), a = abs(x);
            x = 1.0 + (0.278393 + (0.230389 + 0.000972 * a + 0.078108 * a * a) * a) * a;
            x *= x;
            return s - s / (x * x);
        }

        vec4 beam(vec3 uvl, float bright) {
            float len = uvl.z;
            vec2 xy = uvl.xy;
            float sigma = uSize / 3.0;
            float b;
            if (len < 1E-6) {
                // too short to integrate, the intensity at the position
                b = exp(-dot(xy, xy) / (2.0 * sigma * sigma));
            } else {
                b = erfApprox(xy.x / SQRT2 / sigma) - erfApprox((xy.x - len) / SQRT2 / sigma);
                b *= exp(-xy.y * xy.y / (2.0 * sigma * sigma)) * sigma * TAUR / (2.0 * len);
            }
            b *= bright * uIntensity;
            // where the beam is brightest it burns towards white
            vec3 col = uRgb * b + vec3(max(b - 1.0, 0.0) * 0.35);
            return vec4(col, 1.0);
        }
    )GLSL";

}

//--------------------------------------------------------------
OsciMesh::OsciMesh() {
    clear();
}

//--------------------------------------------------------------
bool OsciMesh::loadShader() {
    shaderTried = true;
    std::string vert, frag;
    std::string uniforms = "uniform float uSize;\nuniform float uIntensity;\nuniform vec3 uRgb;\n";

    if (ofIsGLProgrammableRenderer()) {
        auto renderer = ofGetGLRenderer();
#ifdef TARGET_OPENGLES
        bool es = true;
#else
        bool es = false;
#endif
        std::string version = es ? "" : "#version " + ofGLSLVersionFromGL(renderer->getGLVersionMajor(), renderer->getGLVersionMinor()) + "\n";
        bool modern = !es && ofToInt(ofGLSLVersionFromGL(renderer->getGLVersionMajor(), renderer->getGLVersionMinor())) >= 130;
        std::string precision = "#ifdef GL_ES\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n#endif\n";

        if (modern) {
            vert = version + R"GLSL(
                uniform mat4 modelViewProjectionMatrix;
                in vec4 position;
                in vec3 normal;
                out vec3 vUvl;
                out float vBright;
                void main() {
                    vUvl = normal;
                    vBright = position.z;
                    gl_Position = modelViewProjectionMatrix * vec4(position.xy, 0.0, 1.0);
                }
            )GLSL";
            frag = version + uniforms + "in vec3 vUvl;\nin float vBright;\nout vec4 fragColor;\n" + BEAM_FUNCTIONS +
                "void main() { fragColor = beam(vUvl, vBright); }\n";
        } else {
            vert = version + precision + R"GLSL(
                uniform mat4 modelViewProjectionMatrix;
                attribute vec4 position;
                attribute vec3 normal;
                varying vec3 vUvl;
                varying float vBright;
                void main() {
                    vUvl = normal;
                    vBright = position.z;
                    gl_Position = modelViewProjectionMatrix * vec4(position.xy, 0.0, 1.0);
                }
            )GLSL";
            frag = version + precision + uniforms + "varying vec3 vUvl;\nvarying float vBright;\n" + BEAM_FUNCTIONS +
                "void main() { gl_FragColor = beam(vUvl, vBright); }\n";
        }
    } else {
        // fixed function renderer (GL 2)
        vert = R"GLSL(#version 120
            varying vec3 vUvl;
            varying float vBright;
            void main() {
                vUvl = gl_Normal;
                vBright = gl_Vertex.z;
                gl_Position = gl_ModelViewProjectionMatrix * vec4(gl_Vertex.xy, 0.0, 1.0);
            }
        )GLSL";
        frag = "#version 120\n" + uniforms + "varying vec3 vUvl;\nvarying float vBright;\n" + BEAM_FUNCTIONS +
            "void main() { gl_FragColor = beam(vUvl, vBright); }\n";
    }

    bool ok = shader.setupShaderFromSource(GL_VERTEX_SHADER, vert) &&
              shader.setupShaderFromSource(GL_FRAGMENT_SHADER, frag);
    if (ok) {
        if (ofIsGLProgrammableRenderer()) shader.bindDefaults();
        ok = shader.linkProgram();
    }
    if (!ok) ofLogError("OsciMesh") << "couldn't compile the beam shader";
    return ok;
}

//--------------------------------------------------------------
void OsciMesh::addLines(const float * left, const float * right, const float * bright, int n, int stride) {
    // no work? go home watch tv or something
    if (n <= 0 || stride <= 0) return;

    addLine(last, glm::vec2(left[0], right[0]), bright == nullptr ? 1 : bright[0]);
    int lastIndex = ((n - 1) / stride) * stride;
    last = glm::vec2(left[lastIndex], right[lastIndex]);

    mesh.getVertices().reserve(mesh.getNumVertices() + 6 * (n / stride + 1));
    mesh.getNormals().reserve(mesh.getNumNormals() + 6 * (n / stride + 1));

    for (int i = stride; i < n; i += stride) {
        glm::vec2 p0(left[i - stride], right[i - stride]);
        glm::vec2 p1(left[i], right[i]);
        addLine(p0, p1, bright == nullptr ? 1 : bright[i]);
    }
}

//--------------------------------------------------------------
void OsciMesh::addLine(const glm::vec2 & p0, const glm::vec2 & p1, float bright) {
    glm::vec2 dir = p1 - p0;
    float z = glm::length(dir);
    if (z > EPS) dir /= z;
    else dir = glm::vec2(1.0, 0.0);

    dir *= uSize;
    glm::vec2 norm(-dir.y, dir.x);

    auto add = [&](const glm::vec2 & p, float u, float v) {
        mesh.addVertex(glm::vec3(p, bright));
        mesh.addNormal(glm::vec3(u, v, z));
    };

    add(p0 - dir - norm, -uSize, -uSize);
    add(p0 - dir + norm, -uSize, uSize);
    add(p1 + dir - norm, z + uSize, -uSize);

    add(p0 - dir + norm, -uSize, uSize);
    add(p1 + dir - norm, z + uSize, -uSize);
    add(p1 + dir + norm, z + uSize, uSize);
}

//--------------------------------------------------------------
void OsciMesh::draw() {
    if (mesh.getNumVertices() == 0) return;
    if (!shaderTried) loadShader();
    if (!shader.isLoaded()) return;

    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    shader.begin();
    shader.setUniform3f("uRgb", uRgb);
    shader.setUniform1f("uSize", uSize);
    shader.setUniform1f("uIntensity", uIntensity);
    ofSetColor(255);
    mesh.draw();
    shader.end();
    ofPopStyle();
}

//--------------------------------------------------------------
void OsciMesh::clear() {
    mesh.clear();
    mesh.setMode(OF_PRIMITIVE_TRIANGLES);
    mesh.enableNormals();
}
