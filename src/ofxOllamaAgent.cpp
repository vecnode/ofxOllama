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
    requestOptions.model = model;
}

void Agent::setSystemPrompt(const std::string& prompt) {
    requestOptions.systemPrompt = prompt;
}

void Agent::clearConversation() {
    conversation.clear();
}

const std::vector<ChatMessage>& Agent::getConversation() const {
    return conversation;
}

Result Agent::ask(const std::string& userText) {
    if (userText.empty()) {
        return Result{false, -1, "", "User text is empty", ofJson::object()};
    }

    conversation.push_back(ChatMessage{"user", userText});

    Result result = client->chat(conversation, requestOptions);
    if (result.success) {
        conversation.push_back(ChatMessage{"assistant", result.text});
    }

    return result;
}

}  // namespace ofxOllama
