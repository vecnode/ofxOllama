#pragma once

#include "ofMain.h"
#include "ofxOllamaTypes.h"

namespace ofxOllama {

class Client {
public:
    explicit Client(std::string host = "http://127.0.0.1:11434");

    void setHost(const std::string& host);
    const std::string& getHost() const;

    Result generate(const std::string& prompt, const RequestOptions& options = RequestOptions()) const;
    Result chat(const std::vector<ChatMessage>& messages, const RequestOptions& options = RequestOptions()) const;

private:
    Result postJson(const std::string& endpoint, const ofJson& body) const;

    std::string host;
};

}  // namespace ofxOllama
