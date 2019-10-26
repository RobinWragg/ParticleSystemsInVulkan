#include "main.h"

namespace particles {
	
	vector<Particle> particles;
	vector<vec3> velocities;

	float randf() {
		static bool initialised = false;
		if (!initialised) {
			srand((int)getTime() * 10000);
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

		particles.resize(10000);
		velocities.resize(particles.size());
		for (int i = 0; i < particles.size(); i++) {
			particles[i].position = { 1.1, 0.9-randf()*10, 0 };
			velocities[i] = { 0, 0, 0 };
		}
	}

	const float gravity = 1;
	const float airResistance = 0.1;
	const float groundLevel = 1;

	float getVelocityRandComponent() {
		float r = randf() - 0.5;
		return r * r * r + r*0.15;
	}

	void respawn(Particle *particle, vec3 *velocity) {
		particle->position = { -0.6, -0.2, 0 };
		particle->brightness = randf();

		vec3 spawnVelocity = { 0.4, -1, 0 };
		const vec3 velocityRandomnessAmount = {0.6, 0.6, 0.6};
		vec3 velocityRandomness = {
			getVelocityRandComponent() * velocityRandomnessAmount.x,
			getVelocityRandComponent() * velocityRandomnessAmount.y,
			getVelocityRandComponent() * velocityRandomnessAmount.z
		};

		spawnVelocity += velocityRandomness;
		*velocity = spawnVelocity;
	}

	void update(int particleCount, float deltaTime) {
		float stepSize = deltaTime * 0.5;

		for (int i = 0; i < particles.size(); i++) {
			velocities[i] *= 1 - stepSize * airResistance;
			velocities[i].y += gravity * stepSize;
			particles[i].position += velocities[i] * stepSize;

			if (particles[i].position.y > groundLevel) respawn(&particles[i], &(velocities[i]));
		}
	}

	void render() {
		graphics::render(particles);
	}
}

