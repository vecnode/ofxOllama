#include "ofApp.h"

namespace {
const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

void ofApp::setup() {
    ofSetWindowTitle("ofxOllama | imageExample");
    ofSetFrameRate(60);
    ofBackground(20, 24, 32);

    ofxOllama::setModel("gemma3:4b");
    client = std::make_shared<ofxOllama::Client>();

    if (!client->isAvailable()) {
        status = "Ollama is not reachable at " + client->getHost();
        return;
    }

    ofBuffer imageBuffer = ofBufferFromFile("img1.jpg");
    if (imageBuffer.size() == 0) {
        imageBuffer = ofBufferFromFile("../img1.jpg");
    }
    if (imageBuffer.size() == 0) {
        status = "Missing image file: examples/imageExample/img1.jpg";
        return;
    }

    ofxOllama::RequestOptions options;
    options.model = ofxOllama::getModel();
    options.images.push_back(encodeBase64(imageBuffer));

    pending = client->generateAsync(prompt, options);
    waiting = true;
    status = "Waiting for Ollama image response...";
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
            response.clear();
            status = "Error [" + ofToString(static_cast<int>(result.errorCode)) + "]: " + result.error;
        }
    }
}

void ofApp::draw() {
    ofSetColor(235);
    ofDrawBitmapStringHighlight("imageExample", 24, 34);

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

std::string ofApp::encodeBase64(const ofBuffer& data) const {
    const auto* input = reinterpret_cast<const unsigned char*>(data.getData());
    const std::size_t length = data.size();

    std::string encoded;
    encoded.reserve(((length + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 2 < length) {
        const unsigned int n = (static_cast<unsigned int>(input[i]) << 16) |
                               (static_cast<unsigned int>(input[i + 1]) << 8) |
                               static_cast<unsigned int>(input[i + 2]);
        encoded.push_back(kBase64Alphabet[(n >> 18) & 63]);
        encoded.push_back(kBase64Alphabet[(n >> 12) & 63]);
        encoded.push_back(kBase64Alphabet[(n >> 6) & 63]);
        encoded.push_back(kBase64Alphabet[n & 63]);
        i += 3;
    }

    if (i < length) {
        const unsigned int b0 = static_cast<unsigned int>(input[i]);
        const unsigned int b1 = (i + 1 < length) ? static_cast<unsigned int>(input[i + 1]) : 0U;
        const unsigned int n = (b0 << 16) | (b1 << 8);

        encoded.push_back(kBase64Alphabet[(n >> 18) & 63]);
        encoded.push_back(kBase64Alphabet[(n >> 12) & 63]);
        encoded.push_back((i + 1 < length) ? kBase64Alphabet[(n >> 6) & 63] : '=');
        encoded.push_back('=');
    }

    return encoded;
}
