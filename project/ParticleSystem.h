#pragma once


#include <GL/glew.h>
#include <vector>
#include <glm/detail/type_vec3.hpp>
#include <glm/mat4x4.hpp>

struct Particle
{
	float lifetime;
	float life_length;
	glm::vec3 velocity;
	glm::vec3 pos;
};

class ParticleSystem
{
public:
	// Members
	std::vector<Particle> particles;
	int max_size;
	// Ctor/Dtor
	ParticleSystem() : max_size(0)
	{
	}
	explicit ParticleSystem(int size) : max_size(size)
	{
	}
	~ParticleSystem()
	{
	}
	// Methods
	void kill(int id);
	void spawn(Particle particle);
	void process_particles(float dt);
};
