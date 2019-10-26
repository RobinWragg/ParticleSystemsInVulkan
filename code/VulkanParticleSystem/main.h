#pragma once

#include <cstdio>
#include <vector>
#include <fstream>
#include <windows.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

using namespace glm;
using namespace std;

double getTime();

namespace particles {
	struct Particle;
}

namespace graphics {
	void init(SDL_Window *window, VkVertexInputBindingDescription bindingDesc, vector<VkVertexInputAttributeDescription> attribDescs);
	void destroy();
	void render(vector<particles::Particle> particles);
}

namespace particles {
	struct Particle {
		vec3 position;
		float brightness;
	};

	void init(SDL_Window *window);
	void update(int particleCount, float deltaTime);
	void render();
}