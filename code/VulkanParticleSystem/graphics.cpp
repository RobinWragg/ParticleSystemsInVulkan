#include "graphics.h"
#include "main.h"

#include <cstdio>
#include <vector>
#include <fstream>
#include <direct.h>

using namespace std;

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

namespace graphics {
	const auto requiredSwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
	const auto requiredSwapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	const int requiredSwapchainImageCount = 2;
	bool vsync = true;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderCompletedSemaphore;

	VkQueue queue = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	vector<VkCommandBuffer> commandBuffers;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	vector<VkFramebuffer> framebuffers;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	vector<VkImage> swapchainImages;
	vector<VkImageView> swapchainViews;
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkInstance instance = VK_NULL_HANDLE;
	
	vector<uint8_t> loadBinaryFile(const char *filename) {
		ifstream file(filename, ios::ate | ios::binary);

		SDL_assert(file.is_open());

		vector<uint8_t> bytes(file.tellg());
		file.seekg(0);
		file.read((char*)bytes.data(), bytes.size());
		file.close();

		return bytes;
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

	VkPipelineShaderStageCreateInfo buildShaderStage(const char *spirVFilePath, VkShaderStageFlagBits stage) {
		auto spirV = loadBinaryFile(spirVFilePath);

		VkShaderModuleCreateInfo moduleInfo = {};
		moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleInfo.codeSize = spirV.size();
		moduleInfo.pCode = (uint32_t*)spirV.data();

		VkShaderModule module;
		SDL_assert(vkCreateShaderModule(device, &moduleInfo, nullptr, &module) == VK_SUCCESS);

		VkPipelineShaderStageCreateInfo stageInfo = {};
		stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

		stageInfo.stage = stage;

		stageInfo.module = module;
		stageInfo.pName = "main";

		return stageInfo;
	}

	void buildRenderPass() {

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;

		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = requiredSwapchainFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		SDL_assert(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS);
	}

	void buildPipeline(VkExtent2D extent) {
		
		vector<VkPipelineShaderStageCreateInfo> shaderStages = {
			buildShaderStage("basic_vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			buildShaderStage("basic_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0;
		viewport.maxDepth = 1;

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent = extent;

		VkPipelineViewportStateCreateInfo viewportInfo = {};
		viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportInfo.viewportCount = 1;
		viewportInfo.pViewports = &viewport;
		viewportInfo.scissorCount = 1;
		viewportInfo.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterInfo = {};
		rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterInfo.depthClampEnable = VK_FALSE;
		rasterInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterInfo.lineWidth = 1; // TODO: unnecessary?
		rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterInfo.depthBiasEnable = VK_FALSE;

		// TODO: implement basic antialiasing
		VkPipelineMultisampleStateCreateInfo multisamplingInfo = {};
		multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisamplingInfo.sampleShadingEnable = VK_FALSE;
		multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;

		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkPipelineLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		SDL_assert(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		pipelineInfo.stageCount = (int)shaderStages.size();
		pipelineInfo.pStages = shaderStages.data();

		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterInfo;
		pipelineInfo.pMultisampleState = &multisamplingInfo;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		SDL_assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS);

		for (auto &stage : shaderStages) vkDestroyShaderModule(device, stage.module, nullptr);
	}

	void buildFramebuffers(VkExtent2D extent) {
		framebuffers.resize(swapchainViews.size());

		for (int i = 0; i < swapchainViews.size(); i++) {
			VkImageView attachments[] = {
				swapchainViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = extent.width;
			framebufferInfo.height = extent.height;
			framebufferInfo.layers = 1;

			SDL_assert(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) == VK_SUCCESS);
		}
	}

	void buildCommandPoolAndBuffers(int queueFamilyIndex, VkExtent2D extent) {
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndex;
		poolInfo.flags = 0; // TODO: optimisation possible?
		SDL_assert(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS);

		commandBuffers.resize(framebuffers.size());

		VkCommandBufferAllocateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		bufferInfo.commandPool = commandPool;
		bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // TODO: optimise by reusing commands (secondary buffer level)?
		bufferInfo.commandBufferCount = (int)commandBuffers.size();
		SDL_assert(vkAllocateCommandBuffers(device, &bufferInfo, commandBuffers.data()) == VK_SUCCESS);

		for (int i = 0; i < commandBuffers.size(); i++) {
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0; // TODO: optimisation possible?
			beginInfo.pInheritanceInfo = nullptr; // TODO: for secondary buffers
			SDL_assert(vkBeginCommandBuffer(commandBuffers[i], &beginInfo) == VK_SUCCESS);

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = framebuffers[i];

			VkClearValue clearColor = { 0, 0, 0, 1 };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = extent;

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffers[i]);

			SDL_assert(vkEndCommandBuffer(commandBuffers[i]) == VK_SUCCESS);
		}
	}

	void buildSemaphores() {
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		SDL_assert(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) == VK_SUCCESS);
		SDL_assert(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderCompletedSemaphore) == VK_SUCCESS);
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

		// Create the logical device with a queue capable of graphics and surface presentation commands
		int queueFamilyIndex = -1;
		{
			vector<VkDeviceQueueCreateInfo> queueInfos = {
				buildQueueCreateInfo(physicalDevice, VK_QUEUE_GRAPHICS_BIT, true)
			};
			queueFamilyIndex = queueInfos[0].queueFamilyIndex;

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
				//printAvailableDeviceLayers(physicalDevice);
				//deviceCreateInfo.enabledLayerCount = (int)requiredValidationLayers.size();
				//deviceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
			}

			SDL_assert(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) == VK_SUCCESS);
			SDL_assert(device != VK_NULL_HANDLE);
			printf("\nCreated logical device\n");

			// Get a handle to the new queue
			int queueIndex = 0; // Only one queue per VkDeviceQueueCreateInfo was created, so this is 0.
			vkGetDeviceQueue(device, queueInfos[0].queueFamilyIndex, queueIndex, &queue);
			SDL_assert(queue != VK_NULL_HANDLE);
			printf("\nCreated queue at family index %i\n", queueInfos[0].queueFamilyIndex);
		}
		
		// Create the swapchain
		VkExtent2D imageExtent;

		{
			VkSurfaceCapabilitiesKHR capabilities;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
			imageExtent = capabilities.currentExtent;

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
			createInfo.imageExtent = imageExtent;
			createInfo.imageArrayLayers = 1; // 1 == not stereoscopic
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Suitable for VkFrameBuffer
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // the graphics and surface queues are the same, so no sharing is necessary.
			createInfo.preTransform = capabilities.currentTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Opaque window
			createInfo.presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
			createInfo.clipped = VK_FALSE; // Vulkan will always render all the pixels, even if some are osbscured by other windows.
			createInfo.oldSwapchain = VK_NULL_HANDLE; // I will not support swapchain recreation.

			SDL_assert(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) == VK_SUCCESS);
			printf("\nCreated swapchain with %i images\n", requiredSwapchainImageCount);
		}

		// Get handles to the swapchain images and create their views
		{
			uint32_t actualImageCount;
			vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, nullptr);
			SDL_assert(actualImageCount >= requiredSwapchainImageCount);
			swapchainImages.resize(actualImageCount);
			swapchainViews.resize(actualImageCount);
			SDL_assert(vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, swapchainImages.data()) == VK_SUCCESS);

			for (int i = 0; i < swapchainImages.size(); i++) {
				VkImageViewCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = swapchainImages[i];
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = requiredSwapchainFormat;
				
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;
				
				SDL_assert(vkCreateImageView(device, &createInfo, nullptr, &swapchainViews[i]) == VK_SUCCESS);
			}
		}

		printf("\nInitialised Vulkan\n");

		buildRenderPass();
		buildPipeline(imageExtent);
		buildFramebuffers(imageExtent);
		buildCommandPoolAndBuffers(queueFamilyIndex, imageExtent);
		buildSemaphores();
	}

	void render() {
		double submitStartT = getTime();

		// Submit commands
		uint32_t swapchainImageIndex;
		SDL_assert(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX /* no timeout */, imageAvailableSemaphore, VK_NULL_HANDLE, &swapchainImageIndex) == VK_SUCCESS);
		printf("Rendering to image %i...\n", swapchainImageIndex);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[swapchainImageIndex];

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.pWaitDstStageMask = &waitStage;

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderCompletedSemaphore;

		// The command buffer could be in a "pending" state (not finished executing), so we wait for everything to be finished before submission.
		vkQueueWaitIdle(queue); // TODO: Possible optimisation opportunity here

		SDL_assert(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS);

		double presentStartT = getTime();

		// Present
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderCompletedSemaphore;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &swapchainImageIndex;

		SDL_assert(vkQueuePresentKHR(queue, &presentInfo) == VK_SUCCESS);

		double presentEndT = getTime();

		printf("Submit took %.3lfms. Present tool %.3lfms.\n", (presentStartT - submitStartT) * 1000, (presentEndT - presentStartT) * 1000);
	}

	void destroy() {
		vkDeviceWaitIdle(device);

		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, renderCompletedSemaphore, nullptr);

		for (auto &buffer : framebuffers) vkDestroyFramebuffer(device, buffer, nullptr);
		vkDestroyCommandPool(device, commandPool, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);

		for (auto view : swapchainViews) vkDestroyImageView(device, view, nullptr);
		swapchainViews.resize(0);

		swapchainImages.resize(0);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
	}
}



