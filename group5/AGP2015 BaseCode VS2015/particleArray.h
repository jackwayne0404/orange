#pragma once
#include "rt3d.h"
#include <cstdlib>
#include <ctime>
#include <glm/glm.hpp>

class particleArray {
private:
	int numParticles;
	GLfloat* life;
	GLfloat* fade;
	glm::vec3* positions;
	GLfloat* colours;
	glm::vec3* vel;
	GLuint vao[1];
	GLuint vbo1[2];
public:
	particleArray(const int n);
	~particleArray();
	int getNumParticles(void) const { return numParticles; }
	glm::vec3* getPositions(void) const { return positions; }
	GLfloat* getColours(void) const { return colours; }
	glm::vec3* getVel(void) const { return vel; }
	GLfloat* getLife(void) const { return life; }
	void update(GLfloat dt, bool reset);
	void draw(void);
};




