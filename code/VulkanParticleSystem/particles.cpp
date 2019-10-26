#include "main.h"

namespace particles {
	
	vector<Particle> particles;
	vector<vec3> velocities;

	float randf() {
		static bool initialised = false;
		if (!initialised) {
			srand(getTime() * 10000);
			initialised = true;
		}

		return rand() / (float)RAND_MAX;
	}

	void init(SDL_Window *window) {
		VkVertexInputBindingDescription bindingDesc = {};
		bindingDesc.binding = 0;
		bindingDesc.stride = sizeof(Particle);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription positionAttribDesc = {};
		positionAttribDesc.binding = 0;
		positionAttribDesc.location = 0;
		positionAttribDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
		positionAttribDesc.offset = offsetof(Particle, position);

		VkVertexInputAttributeDescription brightnessAttribDesc = {};
		brightnessAttribDesc.binding = 0;
		brightnessAttribDesc.location = 1;
		brightnessAttribDesc.format = VK_FORMAT_R32_SFLOAT;
		brightnessAttribDesc.offset = offsetof(Particle, brightness);

		graphics::init(window, bindingDesc, { positionAttribDesc, brightnessAttribDesc });




		particles.push_back({ { 0.0f, 0.0f, 0.0f }, randf() });
		particles.push_back({ { 1.0f, 0.0f, 0.0f }, randf() });
		particles.push_back({ { 0.0f, 1.0f, 0.0f }, randf() });
		particles.push_back({ { 0.0f, 0.0f, 0.0f }, randf() });
		particles.push_back({ { 0.0f, 1.0f, 0.0f }, randf() });
		particles.push_back({ { 0.0f, 0.0f, 1.0f }, randf() });
	}

	float cameraAngle = 0;

	void update(int particleCount, float deltaTime) {
		cameraAngle += deltaTime;
		while (cameraAngle >= 2 * M_PI) cameraAngle -= 2 * (float)M_PI;
		
		particles[0].position.x = randf();
	}

	void render() {
		graphics::render(particles);
	}
}

