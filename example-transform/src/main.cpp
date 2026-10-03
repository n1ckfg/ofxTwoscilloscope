#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main() {

#ifdef TARGET_OPENGLES
    ofGLESWindowSettings settings;
    settings.glesVersion = 2;
#else
    ofGLFWWindowSettings settings;
    #if defined(TARGET_LINUX) && (defined(__aarch64__) || defined(__arm__))
        settings.setGLVersion(3, 1); // the most a Raspberry Pi's V3D driver offers
    #else
        settings.setGLVersion(3, 2);
    #endif
#endif
    settings.setSize(1280, 760);
    settings.windowMode = OF_WINDOW;

    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();

}
