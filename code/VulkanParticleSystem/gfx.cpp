#include "gfx.h"

#include <cstdio>
#include <vector>

using namespace std;

#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

namespace gfx {
	const auto requiredSwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
	const auto requiredSwapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	const int requiredSwapchainImageCount = 2;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
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
		
		// Check for required format
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());

		bool foundRequiredFormat = false;

		for (auto &format : formats) {
			if (format.format == requiredSwapchainFormat
				&& format.colorSpace == requiredSwapchainColorSpace) {
				foundRequiredFormat = true;
			}
		}

		return foundRequiredFormat;
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

		// Create logical device with a queue capable of graphics and surface presentation commands
		VkQueue queue = VK_NULL_HANDLE;
		{
			vector<VkDeviceQueueCreateInfo> queueInfos = {
				buildQueueCreateInfo(physicalDevice, VK_QUEUE_GRAPHICS_BIT, true)
			};
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

			// Get handle to the new queue
			int queueIndex = 0; // Only one queue per VkDeviceQueueCreateInfo was created, so this is 0.
			vkGetDeviceQueue(device, queueInfos[0].queueFamilyIndex, queueIndex, &queue);
			SDL_assert(queue != VK_NULL_HANDLE);
			printf("\nCreated queue at family index %i\n", queueInfos[0].queueFamilyIndex);
		}
		
		// Create the swapchain
		{
			VkSurfaceCapabilitiesKHR capabilities;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
			vector<VkPresentModeKHR> presentModes(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

			VkSwapchainCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = surface;
			createInfo.minImageCount = requiredSwapchainImageCount;
			createInfo.imageFormat = requiredSwapchainFormat;
			createInfo.imageColorSpace = requiredSwapchainColorSpace;
			createInfo.imageExtent = capabilities.currentExtent;
			createInfo.imageArrayLayers = 1; // 1 == not stereoscopic
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Suitable for VkFrameBuffer
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // the graphics and surface queues are the same, so no sharing is necessary.
			createInfo.preTransform = capabilities.currentTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Opaque window
			createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
			createInfo.clipped = VK_FALSE; // Vulkan will always render all the pixels, even if some are osbscured by other windows.
			createInfo.oldSwapchain = VK_NULL_HANDLE; // I will not support swapchain recreation.

			SDL_assert(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) == VK_SUCCESS);
		}

		printf("\nInitialised Vulkan\n");
	}

	void destroy() {
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
	}
}



