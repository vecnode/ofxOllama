#include "ofxOllamaTypes.h"

namespace ofxOllama {

namespace {
std::string& defaultModelStorage() {
    static std::string model = "gemma3:4b";
    return model;
}
}  // namespace

RequestOptions::RequestOptions()
: model(getDefaultModel()) {
}

const std::string& getDefaultModel() {
    return defaultModelStorage();
}

void setDefaultModel(const std::string& model) {
    if (!model.empty()) {
        defaultModelStorage() = model;
    }
}

}  // namespace ofxOllama
