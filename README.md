# ofxOllama

`ofxOllama` is an openFrameworks addon to interface with Ollama.


## Features (Backbone)

- Simple synchronous HTTP client for Ollama REST endpoints.
- Support for:
  - `/api/generate`
  - `/api/chat`
- Agent abstraction with conversation memory.
- Example app that sends prompts and renders responses.


## Requirements

1. openFrameworks installed.
2. Ollama installed and running locally.
3. A model pulled in Ollama, for example:

```bash
ollama pull gemma3:4b
```

## Quick Start

1. Place this addon in your openFrameworks addons path or keep it in your project and reference it from `addons.make`.
2. Start Ollama:

```bash
ollama serve
```

3. Build and run the example:

```bash
cd examples/simpleChatExample
make -j
make RunRelease
```

## Using in Your own openFrameworks App

Add this to your app `addons.make`:

```text
ofxOllama
```

Then use the API:

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

## Notes

- Current implementation is synchronous (blocking call).
- Streaming is represented in request options but not yet implemented in the client loop.
- Good next iteration: async requests and token streaming callbacks.
