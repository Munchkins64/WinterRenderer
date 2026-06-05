#include "VulkanRenderer.hpp"

#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string modelPath = "model/dodo.glb";

    if (argc > 1) {
        modelPath = argv[1];
    }

    VulkanRenderer app;

    try {
        app.run(modelPath);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}