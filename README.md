# ofxOllama

`ofxOllama` is an openFrameworks addon to interface with Ollama.


## Features (Backbone)

- HTTP client for Ollama REST endpoints with sync and async request APIs.
- Support for:
  - `/api/generate`
  - `/api/chat`
- Agent abstraction with conversation memory.
- Example app that sends prompts asynchronously and keeps the UI responsive.


## Requirements

1. openFrameworks installed.
2. Ollama installed and running locally.


```cpp
#include "ofxOllama.h"

ofxOllama::setDefaultModel("gemma3:4b");

ofxOllama::Agent agent;
auto result = agent.ask("Give me one creative coding idea.");

if(result.success){
    ofLogNotice() << result.text;
}else{
    ofLogError() << result.error;
}
```
