#include "ofApp.h"

void ofApp::setup() {
    ofSetWindowTitle("ofxOllama | simpleChatExample");
    ofSetFrameRate(60);
    ofBackground(16, 20, 28);

    ofxOllama::setDefaultModel("gemma3:4b");
    model = ofxOllama::getDefaultModel();
    agent.setModel(model);
    agent.setSystemPrompt("You are a concise assistant for creative coding and openFrameworks.");

    statusLine = "Type prompt, press ENTER to send. BACKSPACE edits input.";
}

void ofApp::update() {
    if (!requestInFlight) {
        return;
    }

    if (pendingResult.valid() && pendingResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        auto result = pendingResult.get();
        requestInFlight = false;

        if (result.success) {
            responseText = result.text;
            statusLine = "Response received.";
            inputBuffer.clear();
        } else {
            statusLine = "Request failed (status " + ofToString(result.statusCode) + "): " + result.error;
        }
    }
}

void ofApp::draw() {
    ofSetColor(230);
    ofDrawBitmapStringHighlight("ofxOllama example", 24, 34);

    ofSetColor(190);
    ofDrawBitmapString("Model: " + model, 24, 60);
    ofDrawBitmapString("Ollama host: http://127.0.0.1:11434", 24, 80);

    ofSetColor(255);
    ofDrawBitmapStringHighlight("Prompt", 24, 120);
    ofSetColor(210);
    ofDrawBitmapString(wrapText(inputBuffer.empty() ? "<empty>" : inputBuffer, ofGetWidth() - 48), 24, 145);

    ofSetColor(255);
    ofDrawBitmapStringHighlight("Response", 24, 250);
    ofSetColor(220);
    ofDrawBitmapString(wrapText(responseText.empty() ? "<no response yet>" : responseText, ofGetWidth() - 48), 24, 275);

    ofSetColor(245, 180, 90);
    ofDrawBitmapString(wrapText(statusLine, ofGetWidth() - 48), 24, ofGetHeight() - 28);
}

void ofApp::keyPressed(int key) {
    if (key == OF_KEY_BACKSPACE) {
        if (!inputBuffer.empty()) {
            inputBuffer.pop_back();
        }
        return;
    }

    if (key == OF_KEY_RETURN || key == '\n' || key == '\r') {
        submitPrompt();
        return;
    }

    if (key >= 32 && key <= 126) {
        inputBuffer.push_back(static_cast<char>(key));
    }
}

void ofApp::submitPrompt() {
    if (requestInFlight) {
        statusLine = "A request is already running. Please wait...";
        return;
    }

    if (inputBuffer.empty()) {
        statusLine = "Prompt is empty.";
        return;
    }

    statusLine = "Sending request to Ollama (async)...";
    pendingResult = agent.askAsync(inputBuffer);
    requestInFlight = true;
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
