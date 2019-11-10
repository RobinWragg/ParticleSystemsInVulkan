#include "main.h"

namespace particles {

	uint32_t particleCount = 500000;
	uint32_t m256Count;
	const uint32_t floatsPerM256 = 8;

	Particle * renderableParticles;

	vector<__m256> positionsX;
	vector<__m256> positionsY;
	vector<__m256> positionsZ;

	// TODO: Brightnesses are only updated on a per-float basis, so just make it a vector of floats?
	vector<__m256> brightnesses;

	vector<__m256> velocitiesX;
	vector<__m256> velocitiesY;
	vector<__m256> velocitiesZ;

	// Enables indexing into the arrays by particle index. Slow, so don't do it often.
#define M256s_TO_FLOATS(arrayName) ((float*)arrayName.data())

	vector<thread> updaterThreads;
	HANDLE updateStartSemaphore = NULL;
	HANDLE updateEndSemaphore = NULL;
	void updaterThread(uint32_t startIndex, uint32_t endIndexExclusive);
	bool updaterThreadsShouldReturn = false;

	float randf() {
		static mt19937 randomGenerator((unsigned int)SDL_GetPerformanceCounter());

		// Generate a number and remove the lower offset.
		float randomNumber = randomGen() - randomGen.min();

		// Calculate the range of possible values.
		float range = randomGen.max() - randomGen.min();

		// Return a random number in the range 0.0f-1.0f.
		return randomNumber / range;
	}

	void addComponentDescriptions(
		vector<VkVertexInputBindingDescription> *bindingDescs,
		vector<VkVertexInputAttributeDescription> *attribDescs) {

		// These happen to be the same because there is only one attribDesc per bindingDesc.
		int bindingIndex = bindingDescs->size();
		int location = bindingDescs->size();

		VkVertexInputBindingDescription bindingDesc = {};
		VkVertexInputAttributeDescription attribDesc = {};

		bindingDesc.binding = bindingIndex;
		bindingDesc.stride = sizeof(float);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		attribDesc.binding = bindingIndex;
		attribDesc.location = location;
		attribDesc.format = VK_FORMAT_R32_SFLOAT;
		attribDesc.offset = 0;
		
		bindingDescs->push_back(bindingDesc);
		attribDescs->push_back(attribDesc);
	}

	void setupGraphicsDescriptions(SDL_Window *window) {

		vector<VkVertexInputBindingDescription> bindingDescs;
		vector<VkVertexInputAttributeDescription> attribDescs;
		
		addComponentDescriptions(&bindingDescs, &attribDescs); // pos X
		addComponentDescriptions(&bindingDescs, &attribDescs); // pos Y
		addComponentDescriptions(&bindingDescs, &attribDescs); // pos Z
		addComponentDescriptions(&bindingDescs, &attribDescs); // brightness

		graphics::init(window, bindingDescs, attribDescs);
	}

	void init(SDL_Window *window) {
		
		updateStartSemaphore = CreateSemaphore(NULL, 0, INT32_MAX, "particle_update_start");
		SDL_assert(updateStartSemaphore);

		updateEndSemaphore = CreateSemaphore(NULL, 0, INT32_MAX, "particle_update_end");
		SDL_assert(updateEndSemaphore);

		setupGraphicsDescriptions(window);

		renderableParticles = new Particle[particleCount];

		uint32_t renderableFloatsPerParticle = 4; // x, y, z, brightness
		uint32_t totalRenderableFloats = renderableFloatsPerParticle * particleCount;
		
		// The number of particles must fit into a multiple of __m256 without overlap,
		// otherwise more complexity is required in the hot code path.
		SDL_assert_release(totalRenderableFloats % floatsPerM256 == 0);
		SDL_assert_release(particleCount % floatsPerM256 == 0);
		m256Count = particleCount / floatsPerM256;
			
		positionsX.resize(m256Count);
		positionsX.shrink_to_fit();
		positionsY.resize(m256Count);
		positionsY.shrink_to_fit();
		positionsZ.resize(m256Count);
		positionsZ.shrink_to_fit();
		brightnesses.resize(m256Count);
		brightnesses.shrink_to_fit();

		velocitiesX.resize(m256Count);
		velocitiesX.shrink_to_fit();
		velocitiesY.resize(m256Count);
		velocitiesY.shrink_to_fit();
		velocitiesZ.resize(m256Count);
		velocitiesZ.shrink_to_fit();

		// Initial state
		for (auto &x : positionsX) x = _mm256_set1_ps(1.1f);
		for (uint32_t i = 0; i < particleCount; i++) M256s_TO_FLOATS(positionsY)[i] = 0.8f - (i / (float)particleCount) * 7;
		for (auto &z : positionsZ) z = _mm256_set1_ps(0.0f);
		for (auto &x : velocitiesX) x = _mm256_set1_ps(0.0f);
		for (auto &y : velocitiesY) y = _mm256_set1_ps(0.0f);
		for (auto &z : velocitiesZ) z = _mm256_set1_ps(0.0f);

		const uint32_t threadCount = thread::hardware_concurrency();

		uint32_t rangeSum = 0;

		for (uint32_t i = 0; i < threadCount; i++) {
			uint32_t rangeStartIndex = (i * m256Count) / threadCount; // This will round down
			uint32_t rangeEndIndexExclusive = ((i + 1) * m256Count) / threadCount; // This will round down
			updaterThreads.push_back(thread(updaterThread, rangeStartIndex, rangeEndIndexExclusive));
			rangeSum += rangeEndIndexExclusive - rangeStartIndex;
		}

		SDL_assert_release(rangeSum == m256Count);
	}

	const float gravity = 1.0f;
	const float airResistance = 0.1f;
	const float groundLevel = 1.0f;
	vec3 respawnPosition = { -0.8, -0.1, 0.95 };
	float stepSize = 0.0f;

	void getRandomsForRespawn(__m256 &brightnesses, __m256 &velX, __m256 &velY, __m256 &velZ) {
		float bufferA[floatsPerM256];
		float bufferB[floatsPerM256];
		float bufferC[floatsPerM256];

		for (int i = 0; i < floatsPerM256; i++) bufferA[i] = randf();
		brightnesses = _mm256_load_ps(bufferA);

		vec3 baseVelocity = { 0.4, -1, -0.1 };
		const float velocityRandomnessAmount = 0.3f;

		for (int i = 0; i < floatsPerM256; i++) {
			vec3 velocityRandomness = { randf() - 0.5f, randf() - 0.5f, randf() - 0.5 };
			velocityRandomness = normalize(velocityRandomness) * velocityRandomnessAmount * (randf()*0.95f + 0.05f);
			vec3 velocity = baseVelocity + velocityRandomness;

			bufferA[i] = velocity.x;
			bufferB[i] = velocity.y;
			bufferC[i] = velocity.z;
		}

		velX = _mm256_load_ps(bufferA);
		velY = _mm256_load_ps(bufferB);
		velZ = _mm256_load_ps(bufferC);
	}

	void respawnParticleVectorAtIndex(uint32_t m256Index) {
		positionsX[m256Index] = _mm256_set1_ps(respawnPosition.x);
		positionsY[m256Index] = _mm256_set1_ps(respawnPosition.y);
		positionsZ[m256Index] = _mm256_set1_ps(respawnPosition.z);
		getRandomsForRespawn(brightnesses[m256Index], velocitiesX[m256Index], velocitiesY[m256Index], velocitiesZ[m256Index]);
	}

	void updateRange(uint32_t startIndex, uint32_t endIndexExclusive) {

		__m256 stepSizeVector = _mm256_set1_ps(stepSize);
		__m256 velocityMultiplierVector = _mm256_set1_ps(1 - stepSize * airResistance);
		__m256 gravityStepVector = _mm256_set1_ps(gravity * stepSize);
		__m256 groundLevelVector = _mm256_set1_ps(groundLevel);
		__m256 zeroVector = _mm256_set1_ps(0);

		for (uint32_t i = startIndex; i < endIndexExclusive; i++) {
			velocitiesX[i] = _mm256_mul_ps(velocitiesX[i], velocityMultiplierVector);
			positionsX[i] = _mm256_add_ps(positionsX[i], _mm256_mul_ps(velocitiesX[i], stepSizeVector));

			velocitiesY[i] = _mm256_add_ps(_mm256_mul_ps(velocitiesY[i], velocityMultiplierVector), gravityStepVector);
			positionsY[i] = _mm256_add_ps(positionsY[i], _mm256_mul_ps(velocitiesY[i], stepSizeVector));

			velocitiesZ[i] = _mm256_mul_ps(velocitiesZ[i], velocityMultiplierVector);
			positionsZ[i] = _mm256_add_ps(positionsZ[i], _mm256_mul_ps(velocitiesZ[i], stepSizeVector));

			// If all particles in the __m256 are below groundLevel (_CMP_LE_OQ == false), respawn them
			__m256 comparisonResult = _mm256_cmp_ps(positionsY[i], groundLevelVector, _CMP_LE_OQ);
			
			if (memcmp(&comparisonResult, &zeroVector, sizeof(comparisonResult)) == 0) {
				respawnParticleVectorAtIndex(i);
			}
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
		int componentCount = 4; // x, y, z, brightness
		float * componentPtrs[] = {
			(float*)positionsX.data(),
			(float*)positionsY.data(),
			(float*)positionsZ.data(),
			(float*)brightnesses.data()
		};
		
		graphics::render(particleCount, componentCount, componentPtrs);
	}

	void destroy() {
		updaterThreadsShouldReturn = true;
		ReleaseSemaphore(updateStartSemaphore, (LONG)updaterThreads.size(), nullptr);

		for (auto &thr : updaterThreads) thr.join();

		CloseHandle(updateStartSemaphore);
		CloseHandle(updateEndSemaphore);
	}
}

