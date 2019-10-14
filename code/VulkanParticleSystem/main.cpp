#include <cstdio>
#include <vector>
#include <SDL.h>
#include <SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

using namespace std;

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

double getTime() {
	static unsigned long startCount = SDL_GetPerformanceCounter();
	return (SDL_GetPerformanceCounter() - startCount) / (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char* argv[]) {
	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert(result == 0);

	//
	// create a 4:3 720p window
	//
	int windowWidth = 960;
	int windowHeight = 720;

	SDL_Window *window = SDL_CreateWindow(
		"Vulkan Particle System", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_assert(window != NULL);

	//
	// Get Vulkan instance extensions
	//
	unsigned int extensionCount;
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);

	vector<const char*> extensionNames(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

	for (auto &name : extensionNames) {
		printf("VK extension: %s\n", name);
	}


	//
	// create renderer
	//

	// Query renderers
	for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
		SDL_RendererInfo rendererInfo;
		SDL_GetRenderDriverInfo(i, &rendererInfo);
		printf("%s\n", rendererInfo.name);
	}

	Uint32 renderer_flags = SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED;
	renderer = SDL_CreateRenderer(window, 0, renderer_flags);
	SDL_assert(renderer != NULL);
	SDL_RendererInfo rendererInfo;
	SDL_GetRendererInfo(renderer, &rendererInfo);
	SDL_GetRendererOutputSize(renderer, &rendererWidth, &rendererHeight);
	printf("Renderer: %i x %i, %s\n", rendererWidth, rendererHeight, rendererInfo.name);

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

		// clear
		SDL_SetRenderDrawColor(renderer, 0x22, 0x22, 0x22, 0xff);
		SDL_RenderClear(renderer);

		SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);


		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}




