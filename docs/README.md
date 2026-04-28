# ofxOllama Docs

Practical API reference for using ofxOllama from openFrameworks apps.

## What This Library Exposes

- Global model configuration helpers.
- A low-level `Client` for direct Ollama HTTP calls.
- A higher-level `Agent` with conversation memory.
- Sync and async request methods.
- Optional streaming with per-token callbacks and agent events.

## Summary

By default, `ofxOllama` checks installed models via `ollama list` on first use and selects the first available model, or `"none"` when no models are installed.

## API

Total API entries documented below: 21.

### Globals

- `ofxOllama::getModel()` - Returns the current library-wide model. On first use, it auto-selects the first model reported by `ollama list`, or `"none"` if no installed model is found.
- `ofxOllama::setModel(const std::string& model)` - Sets the library-wide model used by new request options.
- `ofxOllama::toJson(const ChatMessage& message)` - Converts a chat message into Ollama-compatible JSON.

### Client

- `ofxOllama::Client::Client(std::string host = "http://127.0.0.1:11434")` - Creates a client bound to an Ollama host URL.
- `ofxOllama::Client::setHost(const std::string& host)` - Updates the Ollama host URL for requests.
- `ofxOllama::Client::getHost() const` - Returns the current Ollama host URL.
- `ofxOllama::Client::generate(const std::string& prompt, const RequestOptions& options = RequestOptions(), std::function<void(const std::string& token)> onToken = nullptr) const` - Sends a blocking `/api/generate` request. When `options.stream` is `true`, invokes `onToken` for each streamed chunk.
- `ofxOllama::Client::chat(const std::vector<ChatMessage>& messages, const RequestOptions& options = RequestOptions(), std::function<void(const std::string& token)> onToken = nullptr) const` - Sends a blocking `/api/chat` request. When `options.stream` is `true`, invokes `onToken` for each streamed chunk.
- `ofxOllama::Client::generateAsync(std::string prompt, RequestOptions options = RequestOptions()) const` - Sends a non-blocking `/api/generate` request.
- `ofxOllama::Client::chatAsync(std::vector<ChatMessage> messages, RequestOptions options = RequestOptions()) const` - Sends a non-blocking `/api/chat` request.

### Agent

- `ofxOllama::Agent::Agent(std::shared_ptr<Client> client = nullptr)` - Creates an agent with optional custom client.
- `ofxOllama::Agent::setClient(std::shared_ptr<Client> client)` - Replaces the client used by the agent.
- `ofxOllama::Agent::setModel(const std::string& model)` - Sets the model for agent requests.
- `ofxOllama::Agent::setStream(bool enabled)` - Enables or disables streaming for subsequent `ask`/`askAsync` calls.
- `ofxOllama::Agent::setSystemPrompt(const std::string& prompt)` - Sets the system prompt prepended to chat context.
- `ofxOllama::Agent::setRole(const std::string& prompt)` - Convenience alias for `setSystemPrompt`.
- `ofxOllama::Agent::clearConversation()` - Clears the agent conversation memory.
- `ofxOllama::Agent::getConversation() const` - Returns the current conversation history.
- `ofxOllama::Agent::ask(const std::string& userText)` - Sends a blocking user turn and appends assistant response on success.
- `ofxOllama::Agent::askAsync(std::string userText)` - Sends a non-blocking user turn and returns a future result.
- `ofxOllama::Agent::onToken` - `ofEvent<std::string>` fired on each streamed token.
- `ofxOllama::Agent::onResult` - `ofEvent<Result>` fired when the full request is completed.

## Notes

- Async APIs are available via `Client::generateAsync`, `Client::chatAsync`, and `Agent::askAsync`.
- Streaming is opt-in using `RequestOptions::stream` (or `Agent::setStream(true)`).
- `simpleChatExample` demonstrates live streamed text rendering using `Agent::onToken`.
