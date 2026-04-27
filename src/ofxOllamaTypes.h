#pragma once

#include "ofMain.h"

namespace ofxOllama {

const std::string& getDefaultModel();
void setDefaultModel(const std::string& model);

struct ChatMessage {
    std::string role;
    std::string content;
};

struct RequestOptions {
    RequestOptions();

    std::string model;
    bool stream = false;
    ofJson options = ofJson::object();
    std::string systemPrompt;
};

struct Result {
    bool success = false;
    int statusCode = -1;
    std::string text;
    std::string error;
    ofJson raw = ofJson::object();
};

inline ofJson toJson(const ChatMessage& message) {
    return ofJson{
        {"role", message.role},
        {"content", message.content}
    };
}

}  // namespace ofxOllama
