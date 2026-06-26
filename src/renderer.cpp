#include "renderer.h"

void Renderer::initWindow() {

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(_width, _height, "WintrRenderer", nullptr, nullptr);
	if (!window) {
		std::cout << "GLFW window couldn't be created" << std::endl;
	}
}

void Renderer::initVulkan() {
	createInstance();
	createSurface();        
	pickPhysicalDevice();  
	createLogicalDevice();
}

void Renderer::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void Renderer::cleanup() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

// Vulkan functions
void Renderer::createInstance() {

	vkb::InstanceBuilder builder;
	auto vkb_inst_result = builder.set_app_name("wintrRenderer")
		.request_validation_layers(true) 
		.use_default_debug_messenger()   
		.require_api_version(1, 2, 0)    
		.build();

	if (!vkb_inst_result) {
		throw std::runtime_error("ERROR: "
			+ vkb_inst_result.error().message());
	}

	vkb::Instance vkb_instance = vkb_inst_result.value();

	instance = vk::raii::Instance(context, vkb_instance.instance);

	if (vkb_instance.debug_messenger != VK_NULL_HANDLE) {
		debugMessenger = vk::raii::DebugUtilsMessengerEXT(instance, vkb_instance.debug_messenger);
	}
}

void Renderer::createSurface() {

	VkSurfaceKHR rawSurface;
	if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}

	surface = vk::raii::SurfaceKHR(instance, rawSurface);
}

void Renderer::pickPhysicalDevice() {

	vkb::Instance vkb_instance;
	vkb_instance.instance = *instance;

	vkb::PhysicalDeviceSelector selector{ vkb_instance };
	auto phys_dev_ret = selector.set_surface(*surface)
		.set_minimum_version(1, 2)
		.select();

	if (!phys_dev_ret) {
		throw std::runtime_error("Failed to select physical device: " + phys_dev_ret.error().message());
	}

	physicalDevice = vk::raii::PhysicalDevice(instance, phys_dev_ret.value().physical_device);
}

void Renderer::createLogicalDevice() {

	vkb::PhysicalDevice vkb_phys_dev;
	vkb_phys_dev.physical_device = *physicalDevice;

	vkb::DeviceBuilder device_builder{ vkb_phys_dev };
	auto dev_ret = device_builder.build();

	if (!dev_ret) {
		throw std::runtime_error("Failed to create logical device: " + dev_ret.error().message());
	}

	vkb::Device vkb_device = dev_ret.value();

	device = vk::raii::Device(physicalDevice, vkb_device.device);

	auto queue_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	graphicsQueue = vk::raii::Queue(device, queue_index, 0);

}