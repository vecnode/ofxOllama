# ofxOllama

`ofxOllama` is an openFrameworks addon to interface with Ollama.


## Requirements

1. openFrameworks installed.
2. Ollama installed and running locally.


```cpp
#include "ofxOllama.h"

ofxOllama::setModel("gemma3:4b");

auto client = std::make_shared<ofxOllama::Client>();
if (!client->isAvailable()) {
    ofLogError() << "Ollama is not reachable at " << client->getHost();
    return;
}

ofxOllama::Agent agent(client);
agent.setRole("You are a professional assistant that answers the questions in 2 or 3 sentences.");

auto result = agent.ask("Give me one creative coding idea.");

if(result.success){
    ofLogNotice() << result.text;
}else{
    switch (result.errorCode) {
        case ofxOllama::ErrorCode::NotConnected:
            ofLogError() << "Connection failed: " << result.error;
            break;
        case ofxOllama::ErrorCode::Timeout:
            ofLogError() << "Request timed out: " << result.error;
            break;
        case ofxOllama::ErrorCode::ModelNotFound:
            ofLogError() << "Model not found: " << result.error;
            break;
        default:
            ofLogError() << "Request failed: " << result.error;
            break;
    }
}
```

![example_app](./docs/example1.png)
