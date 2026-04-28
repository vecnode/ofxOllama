#include "ofxOllamaClient.h"

#include <cstring>
#include <cstdio>
#include <sstream>
#include <cstdlib>

namespace {
std::string trimRightSlash(const std::string& input) {
    if (!input.empty() && input.back() == '/') {
        return input.substr(0, input.size() - 1);
    }
    return input;
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, (last - first) + 1);
}

std::string tokenFromChunk(const std::string& endpoint, const ofJson& chunk) {
    if (endpoint == "/api/generate") {
        if (chunk.contains("response") && chunk["response"].is_string()) {
            return chunk["response"].get<std::string>();
        }
        return "";
    }

    if (endpoint == "/api/chat") {
        if (chunk.contains("message") && chunk["message"].is_object() &&
            chunk["message"].contains("content") && chunk["message"]["content"].is_string()) {
            return chunk["message"]["content"].get<std::string>();
        }
        return "";
    }

    return "";
}

std::string shellQuote(const std::string& input) {
    std::string quoted = "'";
    for (const char c : input) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

bool hasCurl() {
    return std::system("command -v curl >/dev/null 2>&1") == 0;
}

void applyEndpointText(const std::string& endpoint, ofxOllama::Result& result) {
    if (!result.success) {
        return;
    }

    if (endpoint == "/api/generate") {
        if (result.raw.contains("response") && result.raw["response"].is_string()) {
            result.text = result.raw["response"].get<std::string>();
        }
        return;
    }

    if (endpoint == "/api/chat") {
        if (result.raw.contains("message") && result.raw["message"].is_object() &&
            result.raw["message"].contains("content") && result.raw["message"]["content"].is_string()) {
            result.text = result.raw["message"]["content"].get<std::string>();
        }
    }
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

Result Client::generate(const std::string& prompt,
                        const RequestOptions& options,
                        std::function<void(const std::string& token)> onToken) const {
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

    if (options.stream) {
        return streamJson("/api/generate", body, std::move(onToken));
    }

    Result result = postJson("/api/generate", body);
    if (result.success && result.raw.contains("response") && result.raw["response"].is_string()) {
        result.text = result.raw["response"].get<std::string>();
    }
    return result;
}

Result Client::chat(const std::vector<ChatMessage>& messages,
                    const RequestOptions& options,
                    std::function<void(const std::string& token)> onToken) const {
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

    if (options.stream) {
        return streamJson("/api/chat", body, std::move(onToken));
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

Result Client::streamJson(const std::string& endpoint,
                          const ofJson& body,
                          std::function<void(const std::string& token)> onToken) const {
    Result result;

    // Keep streaming optional at runtime: if curl is unavailable, preserve behavior using the regular path.
    if (!hasCurl()) {
        result = postJson(endpoint, body);
        applyEndpointText(endpoint, result);
        if (result.success && onToken && !result.text.empty()) {
            onToken(result.text);
        }
        return result;
    }

    const std::string payload = body.dump();
    const std::string command =
        "curl -sS -N -H 'Content-Type: application/json' -X POST " + shellQuote(host + endpoint) +
        " --data-binary " + shellQuote(payload) +
        " -w '\n__HTTP_STATUS__:%{http_code}\n' 2>/dev/null";

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        result = postJson(endpoint, body);
        applyEndpointText(endpoint, result);
        if (result.success && onToken && !result.text.empty()) {
            onToken(result.text);
        }
        return result;
    }

    char buffer[2048];
    std::string pending;

    auto consumeLine = [&](const std::string& rawLine) {
        const std::string cleaned = trim(rawLine);
        if (cleaned.empty()) {
            return;
        }

        constexpr const char* statusPrefix = "__HTTP_STATUS__:";
        if (cleaned.rfind(statusPrefix, 0) == 0) {
            result.statusCode = ofToInt(cleaned.substr(std::strlen(statusPrefix)));
            return;
        }

        ofJson chunk;
        try {
            chunk = ofJson::parse(cleaned);
        } catch (const std::exception& e) {
            result.success = false;
            result.error = "Failed to parse streaming JSON chunk: " + std::string(e.what());
            return;
        }

        result.raw = chunk;

        const std::string token = tokenFromChunk(endpoint, chunk);
        if (!token.empty()) {
            result.text += token;
            if (onToken) {
                onToken(token);
            }
        }

        if (chunk.contains("error") && chunk["error"].is_string()) {
            result.error = chunk["error"].get<std::string>();
        }
    };

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pending += buffer;

        std::size_t newlinePos = std::string::npos;
        while ((newlinePos = pending.find('\n')) != std::string::npos) {
            const std::string line = pending.substr(0, newlinePos);
            pending.erase(0, newlinePos + 1);
            consumeLine(line);
        }
    }

    if (!pending.empty()) {
        consumeLine(pending);
    }

    const int closeStatus = pclose(pipe);
    if (result.statusCode < 0) {
        result.statusCode = (closeStatus == 0) ? 200 : -1;
    }

    result.success = (result.statusCode >= 200 && result.statusCode < 300 && result.error.empty());
    if (!result.success && result.error.empty()) {
        result.error = "HTTP request failed with status " + ofToString(result.statusCode);
    }

    return result;
}

std::future<Result> Client::generateAsync(std::string prompt, RequestOptions options) const {
    return std::async(std::launch::async, [this, prompt = std::move(prompt), options = std::move(options)]() {
        return generate(prompt, options);
    });
}

std::future<Result> Client::chatAsync(std::vector<ChatMessage> messages, RequestOptions options) const {
    return std::async(std::launch::async, [this, messages = std::move(messages), options = std::move(options)]() {
        return chat(messages, options);
    });
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
