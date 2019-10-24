
#include "main.h"
#include "graphics.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

using namespace std;

double getTime() {
	static uint64_t startCount = SDL_GetPerformanceCounter();
	return (SDL_GetPerformanceCounter() - startCount) / (double)SDL_GetPerformanceFrequency();
}

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

int main(int argc, char* argv[]) {
	const char *appName = "Vulkan Particle System";

	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert(result == 0);

	char *path = SDL_GetBasePath();
	SDL_assert(SetCurrentDirectory(path));
	SDL_free(path);

	double appStartTime = getTime();

	// create a 4:3 SDL window
	int windowWidth = 1200;
	int windowHeight = 900;

	SDL_Window *window = SDL_CreateWindow(
		appName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_assert(window != NULL);

	graphics::init(window);

	int setupTimeMs = (int)((getTime() - appStartTime) * 1000);
	printf("\nSetup took %ims\n", setupTimeMs);
	
	bool running = true;
	while (running) {
		double deltaTime;
		{
			static double previousTime = 0;
			double timeNow = getTime();
			deltaTime = timeNow - previousTime;
			previousTime = timeNow;
		}
		
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT: running = false; break;
			}
		}
		
		// TODO: render
		graphics::render();
		printf("deltaTime: %.3lfms\n", deltaTime * 1000);
	}

	graphics::destroy();
	SDL_Quit();

	return 0;
}




