#include "ofxOllamaAgent.h"

namespace ofxOllama {

Agent::Agent(std::shared_ptr<Client> client)
: client(std::move(client)) {
    if (!this->client) {
        this->client = std::make_shared<Client>();
    }
}

void Agent::setClient(std::shared_ptr<Client> newClient) {
    client = std::move(newClient);
    if (!client) {
        client = std::make_shared<Client>();
    }
}

void Agent::setModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex);
    requestOptions.model = model;
}

void Agent::setSystemPrompt(const std::string& prompt) {
    std::lock_guard<std::mutex> lock(mutex);
    requestOptions.systemPrompt = prompt;
}

void Agent::role(const std::string& prompt) {
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
        return Result{false, -1, "", "User text is empty", ofJson::object()};
    }

    std::vector<ChatMessage> messagesForRequest;
    RequestOptions optionsForRequest;
    {
        std::lock_guard<std::mutex> lock(mutex);
        conversation.push_back(ChatMessage{"user", userText});
        messagesForRequest = conversation;
        optionsForRequest = requestOptions;
    }

    Result result = client->chat(messagesForRequest, optionsForRequest);
    if (result.success) {
        std::lock_guard<std::mutex> lock(mutex);
        conversation.push_back(ChatMessage{"assistant", result.text});
    }

    return result;
}

std::future<Result> Agent::askAsync(std::string userText) {
    return std::async(std::launch::async, [this, userText = std::move(userText)]() {
        return ask(userText);
    });
}

}  // namespace ofxOllama
