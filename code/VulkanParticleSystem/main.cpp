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

vector<const char*> requiredDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

double getTime() {
	static uint64_t startCount = SDL_GetPerformanceCounter();
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

void createVkInstance(SDL_Window *window, VkInstance *instanceOut) {
	SDL_assert(checkValidationLayerSupport());

	// App info. TODO: Much of this appInfo may be inessential
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	// VK instance creation info
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Request the instance extensions that SDL requires
	unsigned int requiredExtensionCount;
	SDL_Vulkan_GetInstanceExtensions(window, &requiredExtensionCount, nullptr);
	vector<const char*> requiredExtensions(requiredExtensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &requiredExtensionCount, requiredExtensions.data());

	createInfo.enabledExtensionCount = requiredExtensionCount;
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	createInfo.enabledLayerCount = (int)requiredValidationLayers.size();
	createInfo.ppEnabledLayerNames = requiredValidationLayers.data();

	VkResult instanceCreationResult = vkCreateInstance(&createInfo, nullptr, instanceOut);
	SDL_assert(instanceCreationResult == VK_SUCCESS);
}

bool deviceHasExtensions(VkPhysicalDevice device, vector<const char*> requiredExtensions) {
	uint32_t availableExtensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionCount, availableExtensions.data());

	for (auto &requiredExt : requiredExtensions) {
		bool found = false;

		for (auto &availableExt : availableExtensions) {
			if (strcmp(requiredExt, availableExt.extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) return false;
	}

	return true;
}

void getPhysicalDevice(VkInstance instance, VkPhysicalDevice *deviceOut) {
	*deviceOut = VK_NULL_HANDLE;

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	SDL_assert(deviceCount > 0);

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	// This picks any device that supports Vulkan 1.1 (this resolves to the GTX 1060 3GB on my testing hardware).
	for (auto &device : devices) {
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		if (VK_VERSION_MAJOR(properties.apiVersion) >= 1
			&& VK_VERSION_MINOR(properties.apiVersion) >= 1
			&& deviceHasExtensions(device, requiredDeviceExtensions)) {
			
			*deviceOut = device;
			break;
		}
	}
	
	SDL_assert(*deviceOut != VK_NULL_HANDLE);

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(*deviceOut, &properties);
	printf("\nChosen device: %s\n", properties.deviceName);

}

void printQueueFamilies(VkPhysicalDevice device) {
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

	printf("\nDevice has these queue families:\n");
	for (uint32_t i = 0; i < familyCount; i++) {
		printf("\tNumber of queues: %i. Support: ", families[i].queueCount);

		if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) printf("graphics ");
		if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) printf("compute ");
		if (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) printf("transfer ");
		if (families[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) printf("sparse_binding ");
		printf("\n");
	}
}

VkDeviceQueueCreateInfo buildQueueCreateInfo(VkPhysicalDevice device, VkQueueFlagBits requiredFlags, VkSurfaceKHR *surfaceToSupport) {
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

	int selectedIndex = -1;

	// Choose the first queue family that has the required support
	for (int index = 0; index < families.size(); index++) {
		if (!requiredFlags || (families[index].queueFlags & requiredFlags)) {
			if (surfaceToSupport) {
				VkBool32 familySupportsSurface;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, index, *surfaceToSupport, &familySupportsSurface);

				// Family doesn't support the surface, so skip it
				if (familySupportsSurface == VK_FALSE) continue;
			}

			selectedIndex = index;
			break;
		}
	}

	SDL_assert(selectedIndex >= 0);

	VkDeviceQueueCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	info.queueFamilyIndex = selectedIndex;
	info.queueCount = 1;

	float queuePriority = 1.0f;
	info.pQueuePriorities = &queuePriority;

	return info;
}

void createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice *logicalDeviceOut, VkQueue *graphicsQueueOut, VkQueue *surfaceQueueOut) {
	*logicalDeviceOut = VK_NULL_HANDLE;
	
	VkDeviceQueueCreateInfo graphicsQueueInfo = buildQueueCreateInfo(physicalDevice, VK_QUEUE_GRAPHICS_BIT, nullptr);
	VkDeviceQueueCreateInfo surfaceQueueInfo = buildQueueCreateInfo(physicalDevice, (VkQueueFlagBits)0, &surface);
	
	vector<VkDeviceQueueCreateInfo> queueInfos = {
		graphicsQueueInfo,
		surfaceQueueInfo
	};

	VkPhysicalDeviceFeatures enabledFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	deviceCreateInfo.pQueueCreateInfos = queueInfos.data();
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();

	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

	// Enable extensions
	deviceCreateInfo.enabledExtensionCount = (int)requiredDeviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

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

	result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, logicalDeviceOut);
	SDL_assert(result == VK_SUCCESS);

	printf("\nCreated logical device\n");

	// Get handles to the new queues
	int queueIndex = 0; // Only one queue per VkDeviceQueueCreateInfo was created, so this is 0.
	vkGetDeviceQueue(*logicalDeviceOut, graphicsQueueInfo.queueFamilyIndex, queueIndex, graphicsQueueOut);
	SDL_assert(*graphicsQueueOut != VK_NULL_HANDLE);
	printf("\nCreated graphics queue at family index %i\n", graphicsQueueInfo.queueFamilyIndex);

	vkGetDeviceQueue(*logicalDeviceOut, surfaceQueueInfo.queueFamilyIndex, queueIndex, surfaceQueueOut);
	SDL_assert(*surfaceQueueOut!= VK_NULL_HANDLE);
	printf("\nCreated surface queue at family index %i\n", surfaceQueueInfo.queueFamilyIndex);
}

void initVulkan(SDL_Window *window, VkInstance *instanceOut, VkSurfaceKHR *surfaceOut, VkDevice *deviceOut) {
	createVkInstance(window, instanceOut);

	bool surfaceCreationResult = SDL_Vulkan_CreateSurface(window, *instanceOut, surfaceOut);
	SDL_assert(surfaceCreationResult);
	printf("\nCreated surface\n");

	VkPhysicalDevice physicalDevice;
	getPhysicalDevice(*instanceOut, &physicalDevice);

	VkQueue graphicsQueue;
	VkQueue surfaceQueue;
	createLogicalDevice(physicalDevice, *surfaceOut, deviceOut, &graphicsQueue, &surfaceQueue);

	printf("\nInitialised Vulkan\n");
}

int main(int argc, char* argv[]) {
	const char *appName = "Vulkan Particle System";

	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert(result == 0);

	double appStartTime = getTime();

	// create a 4:3 SDL window
	int windowWidth = 1200;
	int windowHeight = 900;

	SDL_Window *window = SDL_CreateWindow(
		appName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_assert(window != NULL);

	VkInstance vkInstance;
	VkSurfaceKHR surface;
	VkDevice device;
	initVulkan(window, &vkInstance, &surface, &device);

	int setupTimeMs = (int)((getTime() - appStartTime) * 1000);
	printf("\nSetup took %ims\n", setupTimeMs);
	
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

	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(vkInstance, surface, nullptr);
	vkDestroyInstance(vkInstance, nullptr);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}




