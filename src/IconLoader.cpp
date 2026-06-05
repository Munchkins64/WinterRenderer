#include "IconLoader.hpp"
#include "../external/stb_image.h"
#include <iostream>
#include <vector>

bool setWindowIcon(GLFWwindow* window, const std::string& iconPath) {
    int width, height, channels;
    unsigned char* pixels = stbi_load(iconPath.c_str(), &width, &height, &channels, 4);

    if (!pixels) {
        std::cerr << "Failed to load icon: " << iconPath << std::endl;
        return false;
    }

    GLFWimage icon;
    icon.width = width;
    icon.height = height;
    icon.pixels = pixels;

    glfwSetWindowIcon(window, 1, &icon);

    stbi_image_free(pixels);

    std::cout << "Window icon set successfully" << std::endl;
    return true;
}

// Better version: Load multiple sizes
bool setWindowIconMultiple(GLFWwindow* window,
    const std::string& icon16,
    const std::string& icon32,
    const std::string& icon48) {
    std::vector<GLFWimage> icons;
    std::vector<unsigned char*> pixelData;

    auto loadIcon = [&](const std::string& path) -> bool {
        int width, height, channels;
        unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (pixels) {
            GLFWimage img;
            img.width = width;
            img.height = height;
            img.pixels = pixels;
            icons.push_back(img);
            pixelData.push_back(pixels);
            return true;
        }
        return false;
        };

    loadIcon(icon16);
    loadIcon(icon32);
    loadIcon(icon48);

    if (icons.empty()) {
        std::cerr << "Failed to load any icons!" << std::endl;
        return false;
    }

    glfwSetWindowIcon(window, static_cast<int>(icons.size()), icons.data());

    // Free all pixel data
    for (auto* p : pixelData) {
        stbi_image_free(p);
    }

    std::cout << "Window icons set successfully (" << icons.size() << " sizes)" << std::endl;
    return true;
}