#include "main.h"

namespace particles {
	
	vector<Particle> particles;
	vector<float> velocitiesX;
	vector<float> velocitiesY;
	vector<float> velocitiesZ;

	vector<thread> updaterThreads;
	HANDLE updateStartSemaphore = NULL;
	HANDLE updateEndSemaphore = NULL;
	void updaterThread(uint32_t startIndex, uint32_t endIndexExclusive);
	bool updaterThreadsShouldReturn = false;

	float randf() {
		static mt19937 randomGenerator((unsigned int)SDL_GetPerformanceCounter());
		return (mt19937::min() + randomGenerator()) / (float)mt19937::max();
	}

	void init(SDL_Window *window) {
		updateStartSemaphore = CreateSemaphore(NULL, 0, INT32_MAX, "particle_update_start");
		SDL_assert(updateStartSemaphore);

		updateEndSemaphore = CreateSemaphore(NULL, 0, INT32_MAX, "particle_update_end");
		SDL_assert(updateEndSemaphore);

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

		particles.resize(500000);

		velocitiesX.resize(particles.size());
		velocitiesY.resize(particles.size());
		velocitiesZ.resize(particles.size());
		for (int i = 0; i < particles.size(); i++) {
			particles[i].position = { 1.1, 0.85-randf()*10, 0 };
			velocitiesX[i] = 0;
			velocitiesY[i] = 0;
			velocitiesZ[i] = 0;
		}

		const uint32_t threadCount = thread::hardware_concurrency();

		for (uint32_t i = 0; i < threadCount; i++) {
			uint32_t rangeStartIndex = (i * (uint32_t)particles.size()) / threadCount;
			uint32_t rangeEndIndexExclusive = ((i + 1) * (uint32_t)particles.size()) / threadCount;
			updaterThreads.push_back(thread(updaterThread, rangeStartIndex, rangeEndIndexExclusive));
		}
	}

	const float gravity = 1.0f;
	const float airResistance = 0.1f;
	const float groundLevel = 1.0f;
	vec3 respawnPosition = { -0.8, -0.1, 0.95 };
	float stepSize = 0.0f;

	void respawn(int i) {
		particles[i].position = respawnPosition;
		particles[i].brightness = randf();

		vec3 baseVelocity = { 0.4, -1, -0.1 };
		const float velocityRandomnessAmount = 0.3f;
		vec3 velocityRandomness = { randf()-0.5f, randf()-0.5f, randf()-0.5 };
		velocityRandomness = normalize(velocityRandomness) * velocityRandomnessAmount * (randf()*0.95f+0.05f);

		velocitiesX[i] = baseVelocity.x + velocityRandomness.x;
		velocitiesY[i] = baseVelocity.y + velocityRandomness.y;
		velocitiesZ[i] = baseVelocity.z + velocityRandomness.z;
	}
	
	void updateRange(uint32_t startIndex, uint32_t endIndexExclusive) {
		float velocityMultiplier = 1 - stepSize * airResistance;

		for (uint32_t i = startIndex; i < endIndexExclusive; i++) velocitiesX[i] *= velocityMultiplier;
		for (uint32_t i = startIndex; i < endIndexExclusive; i++) velocitiesY[i] *= velocityMultiplier;
		for (uint32_t i = startIndex; i < endIndexExclusive; i++) velocitiesZ[i] *= velocityMultiplier;

		for (uint32_t i = startIndex; i < endIndexExclusive; i++) {
			velocitiesY[i] += gravity * stepSize;
			particles[i].position.x += velocitiesX[i] * stepSize;
			particles[i].position.y += velocitiesY[i] * stepSize;
			particles[i].position.z += velocitiesZ[i] * stepSize;

			if (particles[i].position.y > groundLevel) respawn(i);
		}
	}

	void updaterThread(uint32_t startIndex, uint32_t endIndexExclusive) {
		while (!updaterThreadsShouldReturn) {
			WaitForSingleObject(updateStartSemaphore, INFINITE);

			updateRange(startIndex, endIndexExclusive);

			ReleaseSemaphore(updateEndSemaphore, 1, nullptr);
		}
	}

	void update(int particleCount, float deltaTime) {
		static double totalTime = 0.0;

		// Update the stepSize for updateOneParticle()
		stepSize = deltaTime * 0.5f;

		// Move the respawn position for respawn()
		totalTime += deltaTime;
		respawnPosition.x = -0.8f + sinf((float)totalTime)*0.1f;

		// Notify the updater threads that updating should begin
		ReleaseSemaphore(updateStartSemaphore, (LONG)updaterThreads.size(), nullptr);

		// Wait until the updater threads are done
		for (int i = 0; i < updaterThreads.size(); i++) {
			WaitForSingleObject(updateEndSemaphore, INFINITE);
		}
	}

	void render() {
		graphics::render(particles);
	}

	void destroy() {
		updaterThreadsShouldReturn = true;
		ReleaseSemaphore(updateStartSemaphore, (LONG)updaterThreads.size(), nullptr);

		for (auto &thr : updaterThreads) thr.join();

		CloseHandle(updateStartSemaphore);
		CloseHandle(updateEndSemaphore);
	}
}

