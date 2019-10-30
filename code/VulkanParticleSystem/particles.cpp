#include "main.h"

namespace particles {
	
	vector<Particle> particles;
	vector<vec3> velocities;

	float randf() {
		static bool initialised = false;
		if (!initialised) {
			srand(SDL_GetPerformanceCounter());
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
	
	mutex particleMutex;
	int32_t nextParticleIndexToUpdate = 0;
	
	int32_t requestParticleIndex() {
		int32_t index;
		
		particleMutex.lock();
		
		if (nextParticleIndexToUpdate < particles.size()) index = nextParticleIndexToUpdate++;
		else index = -1;
		
		particleMutex.unlock();
		
		return index;
	}
	
	void updateOneParticle(int32_t i, float stepSize) {
		velocities[i] *= 1 - stepSize * airResistance;
		velocities[i].y += gravity * stepSize;
		particles[i].position += velocities[i] * stepSize;

		if (particles[i].position.y > groundLevel) respawn(&particles[i], &(velocities[i]));
	}
	
	void updaterThread(float stepSize) {
		while (true) {
			int32_t i = requestParticleIndex();
			
			if (i < 0) break;
			
			updateOneParticle(i, stepSize);
		}
	}

	void update(int particleCount, float deltaTime) {
		float stepSize = deltaTime * 0.5f;
		
		nextParticleIndexToUpdate = 0;
		
		vector<thread> threads;
		
		for (int i = 0; i < thread::hardware_concurrency(); i++) {
			threads.push_back(thread(updaterThread, stepSize));
		}
		
		printf("Updating particles using %i threads\n", thread::hardware_concurrency());
		
		for (auto &thr : threads) thr.join();
	}

	void render() {
		graphics::render(particles);
	}
}

