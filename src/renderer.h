#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>
#include <iostream>
#include "VkBootstrap.h"
#include <stdexcept>

constexpr uint32_t _width = 1000;
constexpr uint32_t _height = 650;

class Renderer {
public:

	void initRenderer() {
		initWindow();
		initVulkan();
		run();
		cleanup();
	}

private:

	void initWindow();
	void initVulkan();
	void run();
	void cleanup();

	void createInstance();
	void createSurface();  
	void pickPhysicalDevice();
	void createLogicalDevice();

	// variables
	GLFWwindow* window;

	vk::raii::Context                context;
	vk::raii::Instance               instance{ nullptr };
	vk::raii::DebugUtilsMessengerEXT debugMessenger{ nullptr };
	vk::raii::SurfaceKHR             surface{ nullptr };       
	vk::raii::PhysicalDevice         physicalDevice{ nullptr };
	vk::raii::Device                 device{ nullptr };         
	vk::raii::Queue                  graphicsQueue{ nullptr };
};