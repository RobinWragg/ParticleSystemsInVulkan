#include "main.h"

namespace particles {
	struct Particle {
		vec3 position;
		float brightness;
	};

	vector<Particle> particles;

	void init() {
		VkVertexInputBindingDescription bindingDesc = {};
		bindingDesc.binding = 0;
		bindingDesc.stride = sizeof(Particle);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		particles.push_back({ { 0.0f, 0.0f, 0.0f }, 0.0f });
		particles.push_back({ { 1.0f, 0.0f, 0.0f }, 0.1f });
		particles.push_back({ { 0.0f, 1.0f, 0.0f }, 0.2f });
		particles.push_back({ { 0.0f, 0.0f, 0.0f }, 0.3f });
		particles.push_back({ { 0.0f, 1.0f, 0.0f }, 0.4f });
		particles.push_back({ { 0.0f, 0.0f, 1.0f }, 0.5f });
	}

	float cameraAngle = 0;

	void update(int particleCount, float deltaTime) {
		cameraAngle += deltaTime;
		while (cameraAngle >= 2 * M_PI) cameraAngle -= 2 * (float)M_PI;
	}

	void render() {
		graphics::render();
	}
}

