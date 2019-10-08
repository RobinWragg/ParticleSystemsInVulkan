#include <SDL.h>
#include <unistd.h>
#include <vector>
#include <string>

using namespace std;

#include "linalg.h"
#include "gui.h"
#include "mcuLink.h"

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

float rotX = 0, rotY = 0, rotZ = 0;

Vec3 transformPoint(Vec3 point) {
	Mat33 masterMat;
	
	// make a 1:100 ratio of world coordinates to pixels and flip the Y axis so positive is up.
	masterMat.makeScaler(Vec3(100, -100, 100));
	
	Mat33 rotMat;
	
	rotMat.makeXRotator(rotX);
	masterMat *= rotMat;
	
	rotMat.makeYRotator(rotY);
	masterMat *= rotMat;
	
	rotMat.makeZRotator(rotZ);
	masterMat *= rotMat;
	
	point = masterMat * point;
	return point + Vec3(rendererWidth/2, rendererHeight/2, 0);
}

void renderLine(Vec3 lineStart, Vec3 lineEnd) {
	lineStart = transformPoint(lineStart);
	lineEnd = transformPoint(lineEnd);
	SDL_RenderDrawLine(renderer, lineStart.x, lineStart.y, lineEnd.x, lineEnd.y);
}

void renderTriangle(const Vec3 triangle[]) {
	renderLine(triangle[0], triangle[1]);
	renderLine(triangle[1], triangle[2]);
	renderLine(triangle[2], triangle[0]);
}

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
	
	gui::init(window, renderer);
		
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
		
		// camera
		rotX = M_PI/8;
		rotY = 0;
		rotZ = 0;
		// rotX = SDL_GetTicks() * 0.0002;
		// rotY = SDL_GetTicks() * 0.0002;
		// rotZ = SDL_GetTicks() * 0.0002;
		
		// render floor
		SDL_SetRenderDrawColor(renderer, 0x55, 0x55, 0x55, 0xff);
		for (int x = -10; x <= 10; x++) {
			renderLine(Vec3(x, 0, -10), Vec3(x, 0, 10));
		}
		
		for (int z = -10; z <= 10; z++) {
			renderLine(Vec3(-10, 0, z), Vec3(10, 0, z));
		}
		
		// render unit vectors
		SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0x00, 0xff);
		renderLine(Vec3(0, 0, 0), Vec3(1, 0, 0));
		SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
		renderLine(Vec3(0, 0, 0), Vec3(0, 1, 0));
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xff, 0xff);
		renderLine(Vec3(0, 0, 0), Vec3(0, 0, 1));
		
		
		
		
		
		
		
		
		
		
		
		
		
		Vec3 mag = gui::poseSet.tempPose.getMagLatest().value;
		float buf = mag.x;
		mag.x = -mag.y;
		mag.y = -buf;
		// mag.x *= -1;
		// mag.y *= -1;
		float heading;
		if (mag.y == 0) heading = (mag.x < 0) ? M_PI : 0;
		else heading = atan2(mag.y, mag.x);
		
		// float DECLINATION = 0.11;
		// heading -= DECLINATION * M_PI / 180;
		
		if (heading > M_PI) heading -= (2 * M_PI);
		else if (heading < -M_PI) heading += (2 * M_PI);
		else if (heading < 0) heading += 2 * M_PI;
		
		rotZ = heading;
		SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
		renderLine(Vec3(0, 0, 0), Vec3(1, 0, 0));
		renderLine(Vec3(0, 0, 0), Vec3(mag.x, mag.y, 0));
		
		
		
		
		
		
		
		
		gui::render();
		
		SDL_RenderPresent(renderer);
	}
	
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	
	if (mcuLink::connected()) mcuLink::disconnect();
	
	return 0;
}




