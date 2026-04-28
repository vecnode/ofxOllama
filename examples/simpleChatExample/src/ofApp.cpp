#include "ofApp.h"

void ofApp::setup() {
    ofSetWindowTitle("ofxOllama | simpleChatExample");
    ofSetFrameRate(60);
    ofBackground(16, 20, 28);

    ofxOllama::setModel("gemma3:4b");
    model = ofxOllama::getModel();

    agent.setModel(model);
    agent.setStream(true);
    agent.setSystemPrompt("You are a concise assistant for creative coding and openFrameworks.");
    ofAddListener(agent.onToken, this, &ofApp::onAgentToken);
    ofAddListener(agent.onResult, this, &ofApp::onAgentResult);

    statusLine = "Type prompt, press ENTER to stream response. BACKSPACE edits input.";
}

void ofApp::exit() {
    ofRemoveListener(agent.onToken, this, &ofApp::onAgentToken);
    ofRemoveListener(agent.onResult, this, &ofApp::onAgentResult);
}

void ofApp::update() {
    if (!requestInFlight) {
        return;
    }

    if (pendingResult.valid() && pendingResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        auto result = pendingResult.get();
        requestInFlight = false;

        std::lock_guard<std::mutex> lock(uiMutex);
        if (result.success && responseText.empty()) {
            responseText = result.text;
        }
        if (!result.success) {
            statusLine = "Request failed (status " + ofToString(result.statusCode) + "): " + result.error;
        }
    }
}

void ofApp::draw() {
    std::string status;
    std::string response;
    {
        std::lock_guard<std::mutex> lock(uiMutex);
        status = statusLine;
        response = responseText;
    }

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
    ofDrawBitmapString(wrapText(response.empty() ? "<no response yet>" : response, ofGetWidth() - 48), 24, 275);

    ofSetColor(245, 180, 90);
    ofDrawBitmapString(wrapText(status, ofGetWidth() - 48), 24, ofGetHeight() - 28);
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
        std::lock_guard<std::mutex> lock(uiMutex);
        statusLine = "A request is already running. Please wait...";
        return;
    }

    if (inputBuffer.empty()) {
        std::lock_guard<std::mutex> lock(uiMutex);
        statusLine = "Prompt is empty.";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(uiMutex);
        responseText.clear();
        statusLine = "Streaming response from Ollama...";
    }
    pendingResult = agent.askAsync(inputBuffer);
    requestInFlight = true;
}

void ofApp::onAgentToken(std::string& token) {
    std::lock_guard<std::mutex> lock(uiMutex);
    responseText += token;
}

void ofApp::onAgentResult(ofxOllama::Result& result) {
    std::lock_guard<std::mutex> lock(uiMutex);
    if (result.success) {
        statusLine = "Streaming complete.";
        inputBuffer.clear();
        return;
    }

    statusLine = "Request failed (status " + ofToString(result.statusCode) + "): " + result.error;
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
