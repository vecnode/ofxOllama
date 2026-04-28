#include "ofxOllamaClient.h"

#include <curl/curl.h>

#include <chrono>
#include <sstream>
#include <thread>

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

ofxOllama::ErrorCode classifyStatusCode(int statusCode) {
    if (statusCode == 404) {
        return ofxOllama::ErrorCode::ModelNotFound;
    }
    if (statusCode >= 500) {
        return ofxOllama::ErrorCode::ServerError;
    }
    if (statusCode >= 400) {
        return ofxOllama::ErrorCode::Unknown;
    }
    return ofxOllama::ErrorCode::None;
}

ofxOllama::ErrorCode classifyCurlCode(CURLcode code) {
    if (code == CURLE_OPERATION_TIMEDOUT) {
        return ofxOllama::ErrorCode::Timeout;
    }
    return ofxOllama::ErrorCode::NotConnected;
}

bool shouldRetry(const ofxOllama::Result& result, int attemptIndex, int maxRetries) {
    if (attemptIndex >= maxRetries) {
        return false;
    }
    return result.errorCode == ofxOllama::ErrorCode::NotConnected ||
           result.errorCode == ofxOllama::ErrorCode::ServerError;
}

int normalizedTimeoutMs(const ofxOllama::RequestOptions& options) {
    return std::max(1, options.timeoutMs);
}

int normalizedMaxRetries(const ofxOllama::RequestOptions& options) {
    return std::max(0, options.maxRetries);
}

size_t stringWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (userdata == nullptr || ptr == nullptr) {
        return 0;
    }

    auto* output = static_cast<std::string*>(userdata);
    const size_t bytes = size * nmemb;
    output->append(ptr, bytes);
    return bytes;
}

size_t discardWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    (void)ptr;
    (void)userdata;
    return size * nmemb;
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
        context.result->errorCode = ofxOllama::ErrorCode::InvalidResponse;
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

bool Client::isAvailable() const {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return false;
    }

    const std::string url = host + "/api/tags";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 2000L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardWriteCallback);

    CURLcode code = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    return code == CURLE_OK && httpCode >= 200 && httpCode < 300;
}

Result Client::generate(const std::string& prompt,
                        const RequestOptions& options,
                        std::function<void(const std::string& token)> onToken) const {
    if (prompt.empty()) {
        Result result;
        result.success = false;
        result.statusCode = -1;
        result.errorCode = ErrorCode::EmptyInput;
        result.error = "Prompt is empty";
        return result;
    }

    ofJson body = {
        {"model", options.model},
        {"prompt", prompt},
        {"stream", options.stream}
    };

    if (!options.images.empty()) {
        body["images"] = options.images;
    }

    if (!options.systemPrompt.empty()) {
        body["system"] = options.systemPrompt;
    }
    if (!options.options.empty()) {
        body["options"] = options.options;
    }

    if (options.stream) {
        return streamJson("/api/generate", body, options, std::move(onToken));
    }

    Result result = postJson("/api/generate", body, options);
    if (result.success && result.raw.contains("response") && result.raw["response"].is_string()) {
        result.text = result.raw["response"].get<std::string>();
    }
    return result;
}

Result Client::chat(const std::vector<ChatMessage>& messages,
                    const RequestOptions& options,
                    std::function<void(const std::string& token)> onToken) const {
    if (messages.empty()) {
        Result result;
        result.success = false;
        result.statusCode = -1;
        result.errorCode = ErrorCode::EmptyInput;
        result.error = "Messages are empty";
        return result;
    }

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
        return streamJson("/api/chat", body, options, std::move(onToken));
    }

    Result result = postJson("/api/chat", body, options);
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
                          const RequestOptions& options,
                          std::function<void(const std::string& token)> onToken) const {
    const std::string url = host + endpoint;
    const std::string payload = body.dump();
    const int maxRetries = normalizedMaxRetries(options);

    Result lastResult;
    for (int attempt = 0; attempt <= maxRetries; ++attempt) {
        Result result;

        CURL* curl = curl_easy_init();
        if (curl == nullptr) {
            result.success = false;
            result.statusCode = -1;
            result.errorCode = ErrorCode::NotConnected;
            result.error = "Failed to initialize libcurl";
            lastResult = result;
            break;
        }

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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(normalizedTimeoutMs(options)));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streamingWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

        CURLcode code = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (!context.pending.empty()) {
            consumeStreamLine(context.pending, context);
        }

        result.statusCode = (code == CURLE_OK) ? static_cast<int>(httpCode) : -1;
        result.success = (code == CURLE_OK && result.statusCode >= 200 && result.statusCode < 300 && result.error.empty());

        if (!result.success) {
            if (result.errorCode == ErrorCode::None) {
                if (code != CURLE_OK) {
                    result.errorCode = classifyCurlCode(code);
                } else {
                    result.errorCode = classifyStatusCode(result.statusCode);
                    if (result.errorCode == ErrorCode::None) {
                        result.errorCode = ErrorCode::Unknown;
                    }
                }
            }

            if (result.error.empty()) {
                if (code != CURLE_OK) {
                    result.error = "Streaming request failed: " + std::string(curl_easy_strerror(code));
                } else if (result.raw.contains("error") && result.raw["error"].is_string()) {
                    result.error = result.raw["error"].get<std::string>();
                } else {
                    result.error = "HTTP request failed with status " + ofToString(result.statusCode);
                }
            }
        } else {
            result.errorCode = ErrorCode::None;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        lastResult = result;
        if (!result.text.empty()) {
            return result;
        }
        if (!shouldRetry(result, attempt, maxRetries)) {
            return result;
        }

        const int backoffMs = 100 * (1 << attempt);
        std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
    }

    return lastResult;
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

Result Client::postJson(const std::string& endpoint, const ofJson& body, const RequestOptions& options) const {
    const std::string url = host + endpoint;
    const std::string payload = body.dump();
    const int maxRetries = normalizedMaxRetries(options);

    Result lastResult;
    for (int attempt = 0; attempt <= maxRetries; ++attempt) {
        Result result;
        std::string responseBody;

        CURL* curl = curl_easy_init();
        if (curl == nullptr) {
            result.success = false;
            result.statusCode = -1;
            result.errorCode = ErrorCode::NotConnected;
            result.error = "Failed to initialize libcurl";
            lastResult = result;
            break;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(normalizedTimeoutMs(options)));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

        CURLcode code = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        result.statusCode = (code == CURLE_OK) ? static_cast<int>(httpCode) : -1;
        result.success = (code == CURLE_OK && result.statusCode >= 200 && result.statusCode < 300);

        if (!responseBody.empty()) {
            try {
                result.raw = ofJson::parse(responseBody);
            } catch (const std::exception& e) {
                result.success = false;
                result.errorCode = ErrorCode::InvalidResponse;
                result.error = "Failed to parse JSON response: " + std::string(e.what());
            }
        }

        if (!result.success) {
            if (result.errorCode == ErrorCode::None) {
                if (code != CURLE_OK) {
                    result.errorCode = classifyCurlCode(code);
                } else {
                    result.errorCode = classifyStatusCode(result.statusCode);
                    if (result.errorCode == ErrorCode::None) {
                        result.errorCode = ErrorCode::Unknown;
                    }
                }
            }

            if (result.error.empty()) {
                if (code != CURLE_OK) {
                    result.error = "HTTP request failed: " + std::string(curl_easy_strerror(code));
                } else if (result.raw.contains("error") && result.raw["error"].is_string()) {
                    result.error = result.raw["error"].get<std::string>();
                } else {
                    result.error = "HTTP request failed with status " + ofToString(result.statusCode);
                }
            }
        } else {
            result.errorCode = ErrorCode::None;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        lastResult = result;
        if (!shouldRetry(result, attempt, maxRetries)) {
            return result;
        }

        const int backoffMs = 100 * (1 << attempt);
        std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
    }

    return lastResult;
}

}  // namespace ofxOllama
