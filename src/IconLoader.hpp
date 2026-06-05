#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

bool setWindowIcon(GLFWwindow* window, const std::string& iconPath);

bool setWindowIconMultiple(GLFWwindow* window,
    const std::string& icon16,
    const std::string& icon32,
    const std::string& icon48);