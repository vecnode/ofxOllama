// AI Generated

#include "ofApp.h"

void ofApp::setup() {
    ofSetWindowTitle("ofxOllama | oneLineAskExample");
    ofSetFrameRate(60);
    ofBackground(20, 24, 32);

    ofxOllama::setModel("gemma3:4b");
    agent.setModel(ofxOllama::getModel());

    pending = agent.askAsync(prompt);
    waiting = true;
    status = "Waiting for Ollama response...";
}

void ofApp::update() {
    if (!waiting || !pending.valid()) {
        return;
    }

    if (pending.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const auto result = pending.get();
        waiting = false;

        if (result.success) {
            response = result.text;
            status = "Done.";
        } else {
            response = "";
            status = "Error (" + ofToString(result.statusCode) + "): " + result.error;
        }
    }
}

void ofApp::draw() {
    ofSetColor(235);
    ofDrawBitmapStringHighlight("oneLineAskExample", 24, 34);

    ofSetColor(200);
    ofDrawBitmapString("Model: " + ofxOllama::getModel(), 24, 60);

    ofSetColor(255);
    ofDrawBitmapStringHighlight("Prompt", 24, 100);
    ofSetColor(220);
    ofDrawBitmapString(wrapText(prompt, ofGetWidth() - 48), 24, 125);

    ofSetColor(255);
    ofDrawBitmapStringHighlight("Response", 24, 210);
    ofSetColor(230);
    ofDrawBitmapString(wrapText(response.empty() ? "<waiting>" : response, ofGetWidth() - 48), 24, 235);

    ofSetColor(245, 190, 110);
    ofDrawBitmapString(wrapText(status, ofGetWidth() - 48), 24, ofGetHeight() - 24);
}

std::string ofApp::wrapText(const std::string& text, float maxWidth) const {
    const auto words = ofSplitString(text, " ", true, true);
    std::string out;
    std::string line;
    const std::size_t maxCharsPerLine = std::max<std::size_t>(20, static_cast<std::size_t>(maxWidth / 8.0f));

    for (const auto& word : words) {
        const std::string candidate = line.empty() ? word : (line + " " + word);
        if (candidate.size() > maxCharsPerLine) {
            if (!out.empty()) {
                out += "\n";
            }
            out += line;
            line = word;
        } else {
            line = candidate;
        }
    }

    if (!line.empty()) {
        if (!out.empty()) {
            out += "\n";
        }
        out += line;
    }

    return out;
}
