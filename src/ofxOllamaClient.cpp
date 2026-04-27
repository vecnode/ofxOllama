#include "ofxOllamaClient.h"

namespace {
std::string trimRightSlash(const std::string& input) {
    if (!input.empty() && input.back() == '/') {
        return input.substr(0, input.size() - 1);
    }
    return input;
}
}  // namespace

namespace ofxOllama {

Client::Client(std::string host)
: host(trimRightSlash(host)) {
}

void Client::setHost(const std::string& newHost) {
    host = trimRightSlash(newHost);
}

const std::string& Client::getHost() const {
    return host;
}

Result Client::generate(const std::string& prompt, const RequestOptions& options) const {
    ofJson body = {
        {"model", options.model},
        {"prompt", prompt},
        {"stream", options.stream}
    };

    if (!options.systemPrompt.empty()) {
        body["system"] = options.systemPrompt;
    }
    if (!options.options.empty()) {
        body["options"] = options.options;
    }

    Result result = postJson("/api/generate", body);
    if (result.success && result.raw.contains("response") && result.raw["response"].is_string()) {
        result.text = result.raw["response"].get<std::string>();
    }
    return result;
}

Result Client::chat(const std::vector<ChatMessage>& messages, const RequestOptions& options) const {
    ofJson jsonMessages = ofJson::array();
    for (const auto& message : messages) {
        jsonMessages.push_back(toJson(message));
    }

    if (!options.systemPrompt.empty()) {
        jsonMessages.insert(jsonMessages.begin(), ofJson{{"role", "system"}, {"content", options.systemPrompt}});
    }

    ofJson body = {
        {"model", options.model},
        {"messages", jsonMessages},
        {"stream", options.stream}
    };

    if (!options.options.empty()) {
        body["options"] = options.options;
    }

    Result result = postJson("/api/chat", body);
    if (result.success &&
        result.raw.contains("message") &&
        result.raw["message"].is_object() &&
        result.raw["message"].contains("content") &&
        result.raw["message"]["content"].is_string()) {
        result.text = result.raw["message"]["content"].get<std::string>();
    }
    return result;
}

Result Client::postJson(const std::string& endpoint, const ofJson& body) const {
    Result result;

    ofHttpRequest request;
    request.url = host + endpoint;
    request.method = ofHttpRequest::POST;
    request.name = "ofxOllama";
    request.body = body.dump();
    request.headers["Content-Type"] = "application/json";

    ofURLFileLoader loader;
    ofHttpResponse response = loader.handleRequest(request);

    result.statusCode = response.status;
    result.success = (response.status >= 200 && response.status < 300);

    const std::string payload = response.data.getText();
    if (!payload.empty()) {
        try {
            result.raw = ofJson::parse(payload);
        } catch (const std::exception& e) {
            result.error = "Failed to parse JSON response: " + std::string(e.what());
            if (!result.success) {
                result.error += " | Body: " + payload;
            }
        }
    }

    if (!result.success) {
        if (result.error.empty()) {
            if (result.raw.contains("error") && result.raw["error"].is_string()) {
                result.error = result.raw["error"].get<std::string>();
            } else {
                result.error = "HTTP request failed with status " + ofToString(response.status);
            }
        }
    }

    return result;
}

}  // namespace ofxOllama
