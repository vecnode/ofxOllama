#pragma once

#include "ofMain.h"

namespace ofxOllama {

const std::string& getModel();
void setModel(const std::string& model);

enum class ErrorCode {
    None = 0,
    NotConnected,
    Timeout,
    ModelNotFound,
    ServerError,
    InvalidResponse,
    EmptyInput,
    Unknown
};

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
    int maxRetries = 0;
    int timeoutMs = 30000;
};

struct Result {
    bool success = false;
    int statusCode = -1;
    ErrorCode errorCode = ErrorCode::None;
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
