#pragma once

#include <deque>
#include <future>
#include <mutex>

#include "ofMain.h"
#include "ofxOllamaClient.h"

namespace ofxOllama {

class Agent {
public:
    explicit Agent(std::shared_ptr<Client> client = nullptr);
    ~Agent();

    void setClient(std::shared_ptr<Client> client);
    void setModel(const std::string& model);
    void setStream(bool enabled);
    void setSystemPrompt(const std::string& prompt);
    void setRole(const std::string& prompt);

    void clearConversation();
    const std::vector<ChatMessage>& getConversation() const;

    Result ask(const std::string& userText);
    std::future<Result> askAsync(std::string userText);

    ofEvent<std::string> onToken;
    ofEvent<Result> onResult;

private:
    void dispatchQueuedEvents(ofEventArgs& args);

    std::shared_ptr<Client> client;
    mutable std::mutex mutex;
    std::vector<ChatMessage> conversation;
    RequestOptions requestOptions;

    std::mutex eventMutex;
    std::deque<std::string> queuedTokens;
    std::deque<Result> queuedResults;
};

}  // namespace ofxOllama
