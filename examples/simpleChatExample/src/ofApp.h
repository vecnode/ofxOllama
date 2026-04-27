#pragma once

#include "ofMain.h"
#include "ofxOllama.h"

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void submitPrompt();
    std::string wrapText(const std::string& text, float maxWidth) const;

    ofxOllama::Agent agent;

    std::string model = "gemma3:4b";
    std::string inputBuffer;
    std::string statusLine;
    std::string responseText;
};
