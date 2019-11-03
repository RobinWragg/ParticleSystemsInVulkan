#include "main.h"

namespace particles {

	// Multiples of 10000 are divisible by 16
	uint32_t particleCount = 500000;

	Particle * renderableParticles;
	
	float * buf;

	float * positionsX;
	float * positionsY;
	float * positionsZ;

	float * brightnesses;

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

		renderableParticles = new Particle[particleCount];
		buf = new float[particleCount];

		// The positions* arrays and the brightnesses array all share one contiguous block of memory,
		// Necessary for passing to Vulkan efficiently.
		float * particlesBasePtr = (float*)malloc(sizeof(float) * 4 * particleCount);
		positionsX = particlesBasePtr;
		positionsY = &particlesBasePtr[particleCount];
		positionsZ = &particlesBasePtr[particleCount * 2];
		brightnesses = &particlesBasePtr[particleCount * 3];

		velocitiesX.resize(particleCount);
		velocitiesY.resize(particleCount);
		velocitiesZ.resize(particleCount);
		for (uint32_t i = 0; i < particleCount; i++) {
			positionsX[i] = 1.1f;
			positionsY[i] = 0.85f - randf() * 10;
			positionsZ[i] = 0;

			velocitiesX[i] = 0;
			velocitiesY[i] = 0;
			velocitiesZ[i] = 0;
		}

		const uint32_t threadCount = thread::hardware_concurrency();

		for (uint32_t i = 0; i < threadCount; i++) {
			uint32_t rangeStartIndex = (i * particleCount) / threadCount;
			uint32_t rangeEndIndexExclusive = ((i + 1) * particleCount) / threadCount;
			updaterThreads.push_back(thread(updaterThread, rangeStartIndex, rangeEndIndexExclusive));
		}
	}

	const float gravity = 1.0f;
	const float airResistance = 0.1f;
	const float groundLevel = 1.0f;
	vec3 respawnPosition = { -0.8, -0.1, 0.95 };
	float stepSize = 0.0f;

	void respawn(int i) {
		positionsX[i] = respawnPosition.x;
		positionsY[i] = respawnPosition.y;
		positionsZ[i] = respawnPosition.z;

		brightnesses[i] = randf();

		vec3 baseVelocity = { 0.4, -1, -0.1 };
		const float velocityRandomnessAmount = 0.3f;
		vec3 velocityRandomness = { randf()-0.5f, randf()-0.5f, randf()-0.5 };
		velocityRandomness = normalize(velocityRandomness) * velocityRandomnessAmount * (randf()*0.95f+0.05f);

		velocitiesX[i] = baseVelocity.x + velocityRandomness.x;
		velocitiesY[i] = baseVelocity.y + velocityRandomness.y;
		velocitiesZ[i] = baseVelocity.z + velocityRandomness.z;
	}

	void mul_ab_then_add_to_c(float *a, float b, float *c, uint32_t count) {
		__m256 b_vector = _mm256_set1_ps(b);

		for (uint32_t i = 0; i < count; i += 8) {
			__m256 *a_vector = (__m256*)&a[i];
			__m256 *c_vector = (__m256*)&c[i];
			*c_vector = _mm256_add_ps(*c_vector, _mm256_mul_ps(*a_vector, b_vector));
		}
	}

	void updateRange(uint32_t startIndex, uint32_t endIndexExclusive) {
		float velocityMultiplier = 1 - stepSize * airResistance;

		for (uint32_t i = startIndex; i < endIndexExclusive; i++) velocitiesX[i] *= velocityMultiplier;
		for (uint32_t i = startIndex; i < endIndexExclusive; i++) velocitiesY[i] *= velocityMultiplier;
		for (uint32_t i = startIndex; i < endIndexExclusive; i++) velocitiesZ[i] *= velocityMultiplier;

		float gravityStep = gravity * stepSize;
		for (uint32_t i = startIndex; i < endIndexExclusive; i++) velocitiesY[i] += gravityStep;





		uint32_t count = endIndexExclusive - startIndex;
		mul_ab_then_add_to_c(&velocitiesX[startIndex], stepSize, &positionsX[startIndex], count);
		mul_ab_then_add_to_c(&velocitiesY[startIndex], stepSize, &positionsY[startIndex], count);
		mul_ab_then_add_to_c(&velocitiesZ[startIndex], stepSize, &positionsZ[startIndex], count);
		
		//for (uint32_t i = startIndex; i < endIndexExclusive; i++) buf[i] = velocitiesX[i] * stepSize;
		//for (uint32_t i = startIndex; i < endIndexExclusive; i++) positionsX[i] += buf[i];





		//for (uint32_t i = startIndex; i < endIndexExclusive; i++) positionsX[i] += velocitiesX[i] * stepSize;
		//for (uint32_t i = startIndex; i < endIndexExclusive; i++) positionsY[i] += velocitiesY[i] * stepSize;
		//for (uint32_t i = startIndex; i < endIndexExclusive; i++) positionsZ[i] += velocitiesZ[i] * stepSize;

		for (uint32_t i = startIndex; i < endIndexExclusive; i++) {
			if (positionsY[i] > groundLevel) respawn(i);
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
		for (uint32_t i = 0; i < particleCount; i++) {
			renderableParticles[i].position.x = positionsX[i];
			renderableParticles[i].position.y = positionsY[i];
			renderableParticles[i].position.z = positionsZ[i];
			renderableParticles[i].brightness = brightnesses[i];
		}
		graphics::render(particleCount, renderableParticles);
	}

	void destroy() {
		updaterThreadsShouldReturn = true;
		ReleaseSemaphore(updateStartSemaphore, (LONG)updaterThreads.size(), nullptr);

		for (auto &thr : updaterThreads) thr.join();

		CloseHandle(updateStartSemaphore);
		CloseHandle(updateEndSemaphore);
	}
}

