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
    std::string encodeBase64(const ofBuffer& data) const;

    std::shared_ptr<ofxOllama::Client> client;
    std::string prompt = "Describe this image in one short paragraph.";
    std::string status = "Starting request...";
    std::string response;

    bool waiting = false;
    std::future<ofxOllama::Result> pending;
};
