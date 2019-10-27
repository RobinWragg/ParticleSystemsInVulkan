#include "main.h"

double getTime() {
	static uint64_t startCount = SDL_GetPerformanceCounter();
	return (SDL_GetPerformanceCounter() - startCount) / (double)SDL_GetPerformanceFrequency();
}

void monitorFramerate(float deltaTime) {
	static vector<float> frameTimes;

	frameTimes.push_back(deltaTime);

	if (frameTimes.size() >= 100) {
		float worstTime = 0;
		for (auto time : frameTimes) if (time > worstTime) worstTime = time;
		frameTimes.resize(0);
		printf("Worst frame out of 100: %.2f ms (%.1f fps)\n", worstTime * 1000, 1 / worstTime);
	}
}

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

int main(int argc, char* argv[]) {
	const char *appName = "Vulkan Particle System";

	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert_release(result == 0);

	char *path = SDL_GetBasePath();
	SDL_assert_release(SetCurrentDirectory(path));
	SDL_free(path);

	double appStartTime = getTime();

	// create a 4:3 SDL window
	int windowWidth = 1200;
	int windowHeight = 900;

	SDL_Window *window = SDL_CreateWindow(
		appName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_assert_release(window != NULL);

	particles::init(window);

	int setupTimeMs = (int)((getTime() - appStartTime) * 1000);
	printf("\nSetup took %ims\n", setupTimeMs);

	
	bool running = true;
	while (running) {
		float deltaTime;
		{
			static double previousTime = 0;
			double timeNow = getTime();
			deltaTime = (float)(timeNow - previousTime);
			previousTime = timeNow;
		}
		
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT: running = false; break;
			}
		}
		
		particles::update(1000, deltaTime);
		particles::render();

		monitorFramerate(deltaTime);
	}

	graphics::destroy();
	SDL_Quit();

	return 0;
}




