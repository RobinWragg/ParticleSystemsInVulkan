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

bool enableValidationLayers = true;

vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
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

	for (const char* layerName : validationLayers) {
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
	SDL_assert(!enableValidationLayers || checkValidationLayerSupport());

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

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else createInfo.enabledLayerCount = 0;

	VkResult instanceCreationResult = vkCreateInstance(&createInfo, nullptr, instanceOut);
	SDL_assert(instanceCreationResult == VK_SUCCESS);

	// Query available Vulkan extensions
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	vector<VkExtensionProperties> extensions(extensionCount);

	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	for (const auto& extension : extensions) {
		printf("Available extension: %s\n", extension.extensionName);
	}
}

void getVkPhysicalDevice(VkInstance instance, VkPhysicalDevice *deviceOut) {
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
			&& VK_VERSION_MINOR(properties.apiVersion) >= 1) {
			*deviceOut = device;
		}
	}
	
	SDL_assert(*deviceOut != VK_NULL_HANDLE);
}

int main(int argc, char* argv[]) {
	const char *appName = "Vulkan Particle System";

	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert(result == 0);

	// create a 800x600 SDL2 window
	int windowWidth = 800;
	int windowHeight = 600;

	SDL_Window *window = SDL_CreateWindow(
		appName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_assert(window != NULL);

	VkInstance vkInstance;
	createVkInstance(appName, window, &vkInstance);

	VkPhysicalDevice vkPhysicalDevice;
	getVkPhysicalDevice(vkInstance, &vkPhysicalDevice);

	// Query queue families
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, nullptr);
	printf("%i queue families available\n", queueFamilyCount);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, queueFamilies.data());

	printf("Queue Families:\n");
	for (auto &family : queueFamilies) {
		printf("Number of queues: %i. Support: ", family.queueCount);

		if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) printf("graphics ");
		if (family.queueFlags & VK_QUEUE_COMPUTE_BIT) printf("compute ");
		if (family.queueFlags & VK_QUEUE_TRANSFER_BIT) printf("transfer ");
		if (family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) printf("sparse binding ");
		printf("\n");
	}
	
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




