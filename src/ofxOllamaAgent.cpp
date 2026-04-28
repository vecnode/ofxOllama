#include "ofxOllamaAgent.h"

#include <thread>

namespace {
bool isMainThread() {
    return std::this_thread::get_id() == ofGetMainThreadId();
}
}

namespace ofxOllama {

Agent::Agent(std::shared_ptr<Client> client)
: client(std::move(client)) {
    if (!this->client) {
        this->client = std::make_shared<Client>();
    }

    ofAddListener(ofEvents().update, this, &Agent::dispatchQueuedEvents);
}

Agent::~Agent() {
    ofRemoveListener(ofEvents().update, this, &Agent::dispatchQueuedEvents);
}

void Agent::setClient(std::shared_ptr<Client> newClient) {
    client = std::move(newClient);
    if (!client) {
        client = std::make_shared<Client>();
    }
}

void Agent::setModel(const std::string& model) {
    ofxOllama::setModel(model);
    std::lock_guard<std::mutex> lock(mutex);
    requestOptions.model = model;
}

void Agent::setStream(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex);
    requestOptions.stream = enabled;
}

void Agent::setSystemPrompt(const std::string& prompt) {
    std::lock_guard<std::mutex> lock(mutex);
    requestOptions.systemPrompt = prompt;
}

void Agent::setRole(const std::string& prompt) {
    setSystemPrompt(prompt);
}

void Agent::clearConversation() {
    std::lock_guard<std::mutex> lock(mutex);
    conversation.clear();
}

const std::vector<ChatMessage>& Agent::getConversation() const {
    return conversation;
}

Result Agent::ask(const std::string& userText) {
    if (userText.empty()) {
        Result result;
        result.success = false;
        result.statusCode = -1;
        result.errorCode = ErrorCode::EmptyInput;
        result.error = "User text is empty";
        return result;
    }

    std::vector<ChatMessage> messagesForRequest;
    RequestOptions optionsForRequest;
    {
        std::lock_guard<std::mutex> lock(mutex);
        conversation.push_back(ChatMessage{"user", userText});
        messagesForRequest = conversation;
        optionsForRequest = requestOptions;
    }

    Result result = client->chat(messagesForRequest,
                                 optionsForRequest,
                                 [this](const std::string& token) {
                                     if (isMainThread()) {
                                         std::string tokenCopy = token;
                                         ofNotifyEvent(onToken, tokenCopy, this);
                                         return;
                                     }

                                     std::lock_guard<std::mutex> eventLock(eventMutex);
                                     queuedTokens.push_back(token);
                                 });
    if (result.success) {
        std::lock_guard<std::mutex> lock(mutex);
        conversation.push_back(ChatMessage{"assistant", result.text});
    }

    if (isMainThread()) {
        Result resultCopy = result;
        ofNotifyEvent(onResult, resultCopy, this);
    } else {
        std::lock_guard<std::mutex> eventLock(eventMutex);
        queuedResults.push_back(result);
    }

    return result;
}

void Agent::dispatchQueuedEvents(ofEventArgs& args) {
    (void)args;

    std::deque<std::string> tokens;
    std::deque<Result> results;
    {
        std::lock_guard<std::mutex> eventLock(eventMutex);
        tokens.swap(queuedTokens);
        results.swap(queuedResults);
    }

    for (auto& token : tokens) {
        ofNotifyEvent(onToken, token, this);
    }

    for (auto& result : results) {
        ofNotifyEvent(onResult, result, this);
    }
}

std::future<Result> Agent::askAsync(std::string userText) {
    return std::async(std::launch::async, [this, userText = std::move(userText)]() {
        return ask(userText);
    });
}

}  // namespace ofxOllama
