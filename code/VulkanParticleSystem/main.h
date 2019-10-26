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

namespace graphics {
	void init(SDL_Window *window);
	void destroy();
	void render();
}

namespace particles {
	void init();
	void update(int particleCount, float deltaTime);
	void render();
}