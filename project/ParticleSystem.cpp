#include "ParticleSystem.h"

void ParticleSystem::kill(int id) {
	if (id < particles.size()) {
		particles[id] = particles.back();
		particles.pop_back();
	}
}

void ParticleSystem::spawn(Particle particle) {
	if (particles.size() < max_size) {
		particles.push_back(particle);
	}
}

void ParticleSystem::process_particles(float dt) {
	for (unsigned i = 0; i < particles.size(); ++i) {
		// Kill dead particles!
		Particle& p = particles[i];
		if (p.lifetime > p.life_length) {
			kill(i);
		}
	}
	for (unsigned i = 0; i < particles.size(); ++i) {
		// Update alive particles!
		Particle& p = particles[i];
		p.pos += p.velocity * dt;
		p.lifetime += dt;
	}
}