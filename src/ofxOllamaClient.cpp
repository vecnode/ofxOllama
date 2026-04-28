#include "ofxOllamaClient.h"

#include <curl/curl.h>

#include <sstream>

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

struct StreamContext {
    const std::string* endpoint = nullptr;
    std::function<void(const std::string& token)>* onToken = nullptr;
    ofxOllama::Result* result = nullptr;
    std::string pending;
};

void consumeStreamLine(const std::string& rawLine, StreamContext& context) {
    const std::string cleaned = trim(rawLine);
    if (cleaned.empty()) {
        return;
    }

    ofJson chunk;
    try {
        chunk = ofJson::parse(cleaned);
    } catch (const std::exception& e) {
        context.result->success = false;
        context.result->error = "Failed to parse streaming JSON chunk: " + std::string(e.what());
        return;
    }

    context.result->raw = chunk;

    const std::string token = tokenFromChunk(*context.endpoint, chunk);
    if (!token.empty()) {
        context.result->text += token;
        if (*context.onToken) {
            (*context.onToken)(token);
        }
    }

    if (chunk.contains("error") && chunk["error"].is_string()) {
        context.result->error = chunk["error"].get<std::string>();
    }
}

size_t streamingWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (userdata == nullptr || ptr == nullptr) {
        return 0;
    }

    StreamContext* context = static_cast<StreamContext*>(userdata);
    const size_t bytes = size * nmemb;
    context->pending.append(ptr, bytes);

    std::size_t newlinePos = std::string::npos;
    while ((newlinePos = context->pending.find('\n')) != std::string::npos) {
        const std::string line = context->pending.substr(0, newlinePos);
        context->pending.erase(0, newlinePos + 1);
        consumeStreamLine(line, *context);
    }

    return bytes;
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

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        result.success = false;
        result.statusCode = -1;
        result.error = "Failed to initialize libcurl";
        return result;
    }

    const std::string url = host + endpoint;
    const std::string payload = body.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    StreamContext context;
    context.endpoint = &endpoint;
    context.onToken = &onToken;
    context.result = &result;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streamingWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    CURLcode code = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (!context.pending.empty()) {
        consumeStreamLine(context.pending, context);
    }

    result.statusCode = static_cast<int>(httpCode);
    result.success = (code == CURLE_OK && result.statusCode >= 200 && result.statusCode < 300 && result.error.empty());
    if (!result.success && result.error.empty()) {
        if (code != CURLE_OK) {
            result.error = "Streaming request failed: " + std::string(curl_easy_strerror(code));
        } else {
            result.error = "HTTP request failed with status " + ofToString(result.statusCode);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

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
