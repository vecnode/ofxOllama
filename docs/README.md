# ofxOllama Docs

Practical API reference for using ofxOllama from openFrameworks apps.

## What This Library Exposes

- Global model configuration helpers.
- A low-level `Client` for direct Ollama HTTP calls.
- A higher-level `Agent` with conversation memory.
- Sync and async request methods.

## API

Total API entries documented below: 18.

### Globals

- `ofxOllama::getDefaultModel()` - Returns the current library-wide default model.
- `ofxOllama::setDefaultModel(const std::string& model)` - Sets the library-wide default model used by new request options.
- `ofxOllama::toJson(const ChatMessage& message)` - Converts a chat message into Ollama-compatible JSON.

### Client

- `ofxOllama::Client::Client(std::string host = "http://127.0.0.1:11434")` - Creates a client bound to an Ollama host URL.
- `ofxOllama::Client::setHost(const std::string& host)` - Updates the Ollama host URL for requests.
- `ofxOllama::Client::getHost() const` - Returns the current Ollama host URL.
- `ofxOllama::Client::generate(const std::string& prompt, const RequestOptions& options = RequestOptions()) const` - Sends a blocking `/api/generate` request.
- `ofxOllama::Client::chat(const std::vector<ChatMessage>& messages, const RequestOptions& options = RequestOptions()) const` - Sends a blocking `/api/chat` request.
- `ofxOllama::Client::generateAsync(std::string prompt, RequestOptions options = RequestOptions()) const` - Sends a non-blocking `/api/generate` request.
- `ofxOllama::Client::chatAsync(std::vector<ChatMessage> messages, RequestOptions options = RequestOptions()) const` - Sends a non-blocking `/api/chat` request.

### Agent

- `ofxOllama::Agent::Agent(std::shared_ptr<Client> client = nullptr)` - Creates an agent with optional custom client.
- `ofxOllama::Agent::setClient(std::shared_ptr<Client> client)` - Replaces the client used by the agent.
- `ofxOllama::Agent::setModel(const std::string& model)` - Sets the model for agent requests.
- `ofxOllama::Agent::setSystemPrompt(const std::string& prompt)` - Sets the system prompt prepended to chat context.
- `ofxOllama::Agent::clearConversation()` - Clears the agent conversation memory.
- `ofxOllama::Agent::getConversation() const` - Returns the current conversation history.
- `ofxOllama::Agent::ask(const std::string& userText)` - Sends a blocking user turn and appends assistant response on success.
- `ofxOllama::Agent::askAsync(std::string userText)` - Sends a non-blocking user turn and returns a future result.

## Notes

- Async APIs are available via `Client::generateAsync`, `Client::chatAsync`, and `Agent::askAsync`.
- Streaming is represented in request options but not yet implemented in the client loop.
- Good next iteration: async requests and token streaming callbacks.
