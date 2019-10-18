#include <cstdio>
#include <vector>
#include <SDL.h>
#include <SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

using namespace std;

vector<const char*> requiredValidationLayers = {
	// "VK_LAYER_KHRONOS_validation"
};

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

double getTime() {
	static unsigned long startCount = SDL_GetPerformanceCounter();
	return (SDL_GetPerformanceCounter() - startCount) / (double)SDL_GetPerformanceFrequency();
}

bool checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	printf("\nAvailable layers:\n");
	for (const auto& layer : availableLayers) {
		printf("\t%s\n", layer.layerName);
	}

	for (const char* layerName : requiredValidationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

void createVkInstance(const char* appName, SDL_Window *window, VkInstance *instanceOut) {
	SDL_assert(checkValidationLayerSupport());

	// App info. TODO: Much of this appInfo may be inessential
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	// VK instance creation info
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	unsigned int extensionCount;
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);

	vector<const char*> extensionNames(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensionNames.data();

	createInfo.enabledLayerCount = (int)requiredValidationLayers.size();
	createInfo.ppEnabledLayerNames = requiredValidationLayers.data();

	VkResult instanceCreationResult = vkCreateInstance(&createInfo, nullptr, instanceOut);
	SDL_assert(instanceCreationResult == VK_SUCCESS);

	// Query available Vulkan extensions
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	vector<VkExtensionProperties> extensions(extensionCount);

	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	printf("\nAvailable extensions:\n");
	for (const auto& extension : extensions) {
		printf("\t%s\n", extension.extensionName);
	}
}

void getVkPhysicalDevice(VkInstance instance, VkPhysicalDevice *deviceOut) {
	*deviceOut = VK_NULL_HANDLE;

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	SDL_assert(deviceCount > 0);

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	printf("\nAvailable devices (Vulkan v1.1):\n");

	// This picks any device that supports Vulkan 1.1 (this resolves to the GTX 1060 3GB on my testing hardware).
	for (auto &device : devices) {
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		if (VK_VERSION_MAJOR(properties.apiVersion) >= 1
			&& VK_VERSION_MINOR(properties.apiVersion) >= 1) {
			printf("\t%s\n", properties.deviceName);
			*deviceOut = device;
		}
	}
	
	SDL_assert(*deviceOut != VK_NULL_HANDLE);

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(*deviceOut, &properties);
	printf("\nChosen device: %s\n", properties.deviceName);

}

int getGraphicsQueueFamilyIndex(VkPhysicalDevice device) {
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int graphicsFamilyIndex = -1;

	printf("\nDevice has these queue families:\n");
	for (int i = 0; i < queueFamilies.size(); i++) {
		printf("\tNumber of queues: %i. Support: ", queueFamilies[i].queueCount);

		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			printf("graphics ");
			graphicsFamilyIndex = i;
		}
		
		if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) printf("compute ");
		if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) printf("transfer ");
		if (queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) printf("sparse_binding ");
		printf("\n");
	}

	return graphicsFamilyIndex;
}

void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice *logicalDeviceOut) {
	*logicalDeviceOut = VK_NULL_HANDLE;
	
	// Create one graphics queue
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = getGraphicsQueueFamilyIndex(physicalDevice);
	queueCreateInfo.queueCount = 1;

	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures enabledFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;

	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

	uint32_t layerCount;
	VkResult result = vkEnumerateDeviceLayerProperties(physicalDevice, &layerCount, nullptr);
	SDL_assert(result == VK_SUCCESS);
	vector<VkLayerProperties> deviceLayers(layerCount);
	result = vkEnumerateDeviceLayerProperties(physicalDevice, &layerCount, deviceLayers.data());
	SDL_assert(result == VK_SUCCESS);

	printf("\nAvailable device layers (deprecated API section):\n");
	for (const auto& layer : deviceLayers) {
		printf("\t%s\n", layer.layerName);
	}

	deviceCreateInfo.enabledLayerCount = (int)requiredValidationLayers.size();
	deviceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
}

int main(int argc, char* argv[]) {
	const char *appName = "Vulkan Particle System";

	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert(result == 0);

	// create a 4:3 SDL window
	int windowWidth = 1200;
	int windowHeight = 900;

	SDL_Window *window = SDL_CreateWindow(
		appName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_assert(window != NULL);

	VkInstance vkInstance;
	createVkInstance(appName, window, &vkInstance);

	VkPhysicalDevice physicalDevice;
	getVkPhysicalDevice(vkInstance, &physicalDevice);

	VkDevice device;
	createLogicalDevice(physicalDevice, &device);

	bool running = true;
	while (running) {
		static double previousTime = 0;
		double time = SDL_GetTicks() / 1000.0;
		double deltaTime = time - previousTime;
		previousTime = time;

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT: running = false; break;
			}
		}

		// TODO: render
	}

	//vkDestroyInstance(vkInstance, nullptr);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}




