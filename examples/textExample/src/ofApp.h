#pragma once

#include <future>

#include "ofMain.h"
#include "ofxOllama.h"

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;

private:
    std::string wrapText(const std::string& text, float maxWidth) const;

    ofxOllama::Agent agent;
    std::string prompt = "In one sentence, describe what openFrameworks is for.";
    std::string status = "Starting request...";
    std::string response = "";

    bool waiting = false;
    std::future<ofxOllama::Result> pending;
};
