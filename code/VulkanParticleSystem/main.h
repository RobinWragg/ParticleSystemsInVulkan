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

using namespace std;

#include "graphics.h"
#include "particles.h"

double getTime();
