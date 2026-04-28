#pragma once

#include <future>
#include <mutex>

#include "ofMain.h"
#include "ofxOllama.h"

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;

private:
    void onAgentToken(std::string& token);
    void onAgentResult(ofxOllama::Result& result);
    void submitPrompt();
    std::string wrapText(const std::string& text, float maxWidth) const;

    ofxOllama::Agent agent;

    std::string model = "gemma3:4b";
    std::string inputBuffer;
    std::string statusLine;
    std::string responseText;
    std::mutex uiMutex;

    bool requestInFlight = false;
    std::future<ofxOllama::Result> pendingResult;
};
