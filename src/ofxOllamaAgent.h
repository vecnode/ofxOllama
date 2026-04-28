#pragma once

#include <future>
#include <mutex>

#include "ofMain.h"
#include "ofxOllamaClient.h"

namespace ofxOllama {

class Agent {
public:
    explicit Agent(std::shared_ptr<Client> client = nullptr);

    void setClient(std::shared_ptr<Client> client);
    void setModel(const std::string& model);
    void setSystemPrompt(const std::string& prompt);
    void setRole(const std::string& prompt);

    void clearConversation();
    const std::vector<ChatMessage>& getConversation() const;

    Result ask(const std::string& userText);
    std::future<Result> askAsync(std::string userText);

private:
    std::shared_ptr<Client> client;
    mutable std::mutex mutex;
    std::vector<ChatMessage> conversation;
    RequestOptions requestOptions;
};

}  // namespace ofxOllama
