#include "ofxOllamaTypes.h"

#include <cstdio>
#include <sstream>

namespace ofxOllama {

namespace {
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, (last - first) + 1);
}

std::string firstInstalledModelFromOllamaList() {
    FILE* pipe = popen("ollama list 2>/dev/null", "r");
    if (pipe == nullptr) {
        return "none";
    }

    std::ostringstream output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output << buffer;
    }
    pclose(pipe);

    std::istringstream lines(output.str());
    std::string line;
    while (std::getline(lines, line)) {
        const auto cleaned = trim(line);
        if (cleaned.empty() || cleaned.rfind("NAME", 0) == 0) {
            continue;
        }

        const auto tokens = ofSplitString(cleaned, " ", true, true);
        if (!tokens.empty()) {
            return tokens.front();
        }
    }

    return "none";
}

std::string& modelStorage() {
    static std::string model = firstInstalledModelFromOllamaList();
    return model;
}
}  // namespace

RequestOptions::RequestOptions()
: model(getModel()) {
}

const std::string& getModel() {
    return modelStorage();
}

void setModel(const std::string& model) {
    if (!model.empty()) {
        modelStorage() = model;
    }
}

}  // namespace ofxOllama
