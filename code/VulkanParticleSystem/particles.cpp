#include "main.h"

namespace particles {
	
	vector<Particle> particles;
	vector<vec3> velocities;

	float randf() {
		static mt19937 randomGenerator(SDL_GetPerformanceCounter());
		return (randomGenerator.min() + randomGenerator()) / (float)randomGenerator.max();
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

		particles.resize(100000);

		velocities.resize(particles.size());
		for (int i = 0; i < particles.size(); i++) {
			particles[i].position = { 1.1, 0.85-randf()*10, 0 };
			velocities[i] = { 0, 0, 0 };
		}
	}

	const float gravity = 1.0f;
	const float airResistance = 0.1f;
	const float groundLevel = 1.0f;

	void respawn(Particle *particle, vec3 *velocity) {
		particle->position = { -0.8, -0.1, 0.5 };
		particle->brightness = randf();

		vec3 baseVelocity = { 0.4, -1, 0 };
		const float velocityRandomnessAmount = 0.3f;
		vec3 velocityRandomness = { randf()-0.5f, randf()-0.5f, (randf()-0.5f)*0.5 };
		velocityRandomness = normalize(velocityRandomness) * velocityRandomnessAmount * (randf()*0.95f+0.05f);

		*velocity = baseVelocity + velocityRandomness;
	}
	
	void updateOneParticle(int32_t i, float stepSize) {
		velocities[i] *= 1 - stepSize * airResistance;
		velocities[i].y += gravity * stepSize;
		particles[i].position += velocities[i] * stepSize;

		if (particles[i].position.y > groundLevel) respawn(&particles[i], &(velocities[i]));
	}
	
	void updateRange(int startIndex, int endIndexExclusive, float stepSize) {
		for (uint32_t i = startIndex; i < endIndexExclusive; i++) {
			updateOneParticle(i, stepSize);
		}
	}

	void update(int particleCount, float deltaTime) {
		float stepSize = deltaTime * 0.5f;
		
		vector<thread> threads;
		
		const int threadCount = thread::hardware_concurrency();

		for (int i = 0; i < threadCount; i++) {
			uint32_t rangeStartIndex = (i * particles.size()) / threadCount;
			uint32_t rangeEndIndexExclusive = ((i+1) * particles.size()) / threadCount;
			threads.push_back(thread(updateRange, rangeStartIndex, rangeEndIndexExclusive, stepSize));
		}
		
		for (auto &thr : threads) thr.join();

		//printf("done\n");
	}

	void render() {
		graphics::render(particles);
	}
}

