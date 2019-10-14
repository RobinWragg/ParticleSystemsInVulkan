#include <SDL.h>
#include <unistd.h>
#include <vector>
#include <string>

using namespace std;

#include "linalg.h"

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

double getTime() {
	static unsigned long startCount = SDL_GetPerformanceCounter();
	return (SDL_GetPerformanceCounter() - startCount) / (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char* argv[]) {
	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert(result == 0);
	chdir(SDL_GetBasePath());
	
	//
	// create window
	//
	int windowWidth = 800;
	int windowHeight = 600;
	bool highDpi = true;
	
	SDL_Window *window = SDL_CreateWindow(
		"sdl3d",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		windowWidth,
		windowHeight,
		highDpi ? SDL_WINDOW_ALLOW_HIGHDPI : 0);
	SDL_assert(window != NULL);
	
	//
	// create renderer
	//
	
	// Pick the Metal backend if available, as OpenGL is deprecated on macOS.
	int rendererIndex = -1;
	for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
		SDL_RendererInfo rendererInfo;
		SDL_GetRenderDriverInfo(i, &rendererInfo);
		
		if (strcmp(rendererInfo.name, "metal") == 0) {
			rendererIndex = i;
			break;
		}
	}
	
	Uint32 renderer_flags = SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED;
	renderer = SDL_CreateRenderer(window, rendererIndex, renderer_flags);
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
			gui::processSdlEvent(event);
			
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




