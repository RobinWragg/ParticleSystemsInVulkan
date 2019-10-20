#include "gfx.h"

#include <cstdio>
#include <vector>

using namespace std;

#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

namespace gfx {
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkInstance instance = VK_NULL_HANDLE;

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

	bool deviceSupportsAcceptableSwapchain(VkPhysicalDevice device, VkSurfaceKHR surface) {
		return true; // TODO
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());

		printf("\nAvailable swap buffer sizes: %i to %i\n", capabilities.minImageCount, capabilities.maxImageCount);

		VK_FORMAT_B8G8R8A8_UNORM;
		VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;




		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		vector<VkPresentModeKHR> presentModes(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());

		printf("\nPretending swapchain is not acceptable for now until checking is implemented.\n");
		return false;
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

	VkDeviceQueueCreateInfo buildQueueCreateInfo(VkPhysicalDevice device, VkQueueFlagBits requiredFlags, bool mustSupportSurface) {
		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

		int selectedIndex = -1;

		// Choose the first queue family that has the required support
		for (int index = 0; index < families.size(); index++) {
			if (!requiredFlags || (families[index].queueFlags & requiredFlags)) {
				if (mustSupportSurface) {
					VkBool32 familySupportsSurface;
					vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &familySupportsSurface);

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

	void printAvailableInstanceLayers() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		printf("\nAvailable layers:\n");
		for (const auto& layer : availableLayers) printf("\t%s\n", layer.layerName);
	}

	void printAvailableDeviceLayers(VkPhysicalDevice device) {
		uint32_t layerCount;
		vkEnumerateDeviceLayerProperties(device, &layerCount, nullptr);
		vector<VkLayerProperties> deviceLayers(layerCount);
		vkEnumerateDeviceLayerProperties(device, &layerCount, deviceLayers.data());

		printf("\nAvailable device layers (deprecated API section):\n");
		for (const auto& layer : deviceLayers) printf("\t%s\n", layer.layerName);
	}

	void init(SDL_Window *window) {
		printAvailableInstanceLayers();

		vector<const char*> requiredValidationLayers = { "VK_LAYER_KHRONOS_validation" };
		vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkPhysicalDeviceFeatures enabledDeviceFeatures = {};

		// Create VK instance
		{
			VkInstanceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			// Specify app info TODO: necessary?
			VkApplicationInfo appInfo = {};
			{
				appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
				appInfo.pApplicationName = "Vulkan Particle System";
				appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
				appInfo.pEngineName = "N/A";
				appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
				appInfo.apiVersion = VK_API_VERSION_1_0;

				createInfo.pApplicationInfo = &appInfo;
			}

			// Request the instance extensions that SDL requires
			vector<const char*> requiredExtensions;
			{
				unsigned int requiredExtensionCount;
				SDL_Vulkan_GetInstanceExtensions(window, &requiredExtensionCount, nullptr);
				requiredExtensions.resize(requiredExtensionCount);
				SDL_Vulkan_GetInstanceExtensions(window, &requiredExtensionCount, requiredExtensions.data());

				createInfo.enabledExtensionCount = (int)requiredExtensions.size();
				createInfo.ppEnabledExtensionNames = requiredExtensions.data();
			}

			// Enable instance validation layers
			createInfo.enabledLayerCount = (int)requiredValidationLayers.size();
			createInfo.ppEnabledLayerNames = requiredValidationLayers.data();

			auto creationResult = vkCreateInstance(&createInfo, nullptr, &instance);
			//SDL_assert( == VK_SUCCESS);
			printf("\nCreated Vulkan instance\n");
		}

		// Create surface
		SDL_assert(SDL_Vulkan_CreateSurface(window, instance, &surface));
		printf("\nCreated SDL+Vulkan surface\n");

		// Get physical device (GTX 1060 3GB)
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		{
			uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
			vector<VkPhysicalDevice> candidateDevices(deviceCount);
			vkEnumeratePhysicalDevices(instance, &deviceCount, candidateDevices.data());

			// This picks the first found device that meets my requirements (this resolves to the GTX 1060 3GB on my computer)
			for (auto &candidateDevice : candidateDevices) {
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(candidateDevice, &properties);

				if (VK_VERSION_MAJOR(properties.apiVersion) >= 1
					&& VK_VERSION_MINOR(properties.apiVersion) >= 1
					&& deviceHasExtensions(candidateDevice, requiredDeviceExtensions)
					&& deviceSupportsAcceptableSwapchain(candidateDevice, surface)) {
					physicalDevice = candidateDevice;
					break;
				}
			}

			SDL_assert(physicalDevice != VK_NULL_HANDLE);

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			printf("\nChosen device: %s\n", properties.deviceName);
		}

		// Create logical device with graphics and surface queues
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue surfaceQueue = VK_NULL_HANDLE;
		{
			auto graphicsQueueInfo = buildQueueCreateInfo(physicalDevice, VK_QUEUE_GRAPHICS_BIT, false);
			auto surfaceQueueInfo = buildQueueCreateInfo(physicalDevice, (VkQueueFlagBits)0, true);
			vector<VkDeviceQueueCreateInfo> queueInfos = { graphicsQueueInfo, surfaceQueueInfo };
			VkDeviceCreateInfo deviceCreateInfo = {};
			{
				deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

				deviceCreateInfo.pQueueCreateInfos = queueInfos.data();
				deviceCreateInfo.queueCreateInfoCount = (int)queueInfos.size();

				deviceCreateInfo.pEnabledFeatures = &enabledDeviceFeatures;

				// Enable extensions
				deviceCreateInfo.enabledExtensionCount = (int)requiredDeviceExtensions.size();
				deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
			}

			// Enable validation layers for the device, same as the instance (deprecated in Vulkan 1.1, but the API advises we do so) TODO: remove?
			{
				printAvailableDeviceLayers(physicalDevice);
				deviceCreateInfo.enabledLayerCount = (int)requiredValidationLayers.size();
				deviceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
			}

			SDL_assert(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) == VK_SUCCESS);
			SDL_assert(device != VK_NULL_HANDLE);
			printf("\nCreated logical device\n");

			// Get handles to the new queues
			int queueIndex = 0; // Only one queue per VkDeviceQueueCreateInfo was created, so this is 0.
			vkGetDeviceQueue(device, graphicsQueueInfo.queueFamilyIndex, queueIndex, &graphicsQueue);
			SDL_assert(graphicsQueue != VK_NULL_HANDLE);
			printf("\nCreated graphics queue at family index %i\n", graphicsQueueInfo.queueFamilyIndex);

			vkGetDeviceQueue(device, surfaceQueueInfo.queueFamilyIndex, queueIndex, &surfaceQueue);
			SDL_assert(surfaceQueue != VK_NULL_HANDLE);
			printf("\nCreated surface queue at family index %i\n", surfaceQueueInfo.queueFamilyIndex);
		}

		printf("\nInitialised Vulkan\n");
	}

	void destroy() {
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
	}
}



