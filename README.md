# ofxOllama

`ofxOllama` is an openFrameworks addon to interface with Ollama.


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

![example_app](./docs/example1.png)