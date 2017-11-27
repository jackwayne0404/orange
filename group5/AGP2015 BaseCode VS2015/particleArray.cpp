
#include "particleArray.h"
#include <glm/glm.hpp>
using namespace std;

particleArray::particleArray(const int n) : numParticles( n ) //intialisation of numParticles==n
{
	if ( numParticles <= 0 ) // trap invalid input
	return;
	positions = new glm::vec3[numParticles * 3];
	colours = new GLfloat[numParticles * 3];
	vel = new glm::vec3[numParticles * 3];
	fade = new GLfloat[numParticles * 3];
	life = new GLfloat[numParticles * 3];

	// lets initialise with some lovely random values!
	std::srand(std::time(0));
	for (int i = 0; i < numParticles * 3; i++) {
		positions[i] = glm::vec3(0.0f,0.0f,0.0f); // 3 position components per particle
		colours[i] = ( std::rand() % 100 ) / 100.0f;
		fade[i] = 0.01f;
		vel[i].x = (std::rand() % 100 - 50) / 10.0f;
		//vel[i].y = (std::rand() % 100 - 50) / 10.0f;
		vel[i].y = 1; //so it only goes +y
		vel[i].z = (std::rand() % 100 - 50) / 10.0f;
		life[i] = 3.0f;
	}

	// Initialise VAO and VBO in constructor, after initialising
	// the arrays:
	glGenVertexArrays(1,vao);//Important, we cannot do this, before generating the SDL context
	glGenBuffers(2, vbo1);
	glBindVertexArray(vao[0]); // bind VAO 0 as current object
	// Position data in attribute index 0, 3 floats per vertex
	glBindBuffer(GL_ARRAY_BUFFER, vbo1[0]); // bind VBO for positions
	glBufferData(GL_ARRAY_BUFFER, numParticles * 3 *sizeof(GLfloat), positions, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glEnableVertexAttribArray(0); // Enable attribute index 0
	// Colours data in attribute 1, 3 floats per vertex
	glBindBuffer(GL_ARRAY_BUFFER, vbo1[1]); // bind VBO for colours
	glBufferData(GL_ARRAY_BUFFER, numParticles * 3 * sizeof(GLfloat), colours, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1); // Enable attribute index 3
	glBindVertexArray(0);
}

particleArray::~particleArray(void)
{
	if ( numParticles <= 0 ) // trap invalid input
	return;
	delete positions;
	delete colours;
	delete vel;
	delete fade;
	delete life;
}

void particleArray::draw(void) {
	glBindVertexArray(vao[0]); // bind VAO 0 as current object
	// particle data may have been updated - so need to resend to GPU (at the beginning, only positions, not colours, nor velocities)
	glBindBuffer(GL_ARRAY_BUFFER, vbo1[0]); // bind VBO 0
	glBufferData(GL_ARRAY_BUFFER, numParticles * 3 *sizeof(GLfloat), positions, GL_DYNAMIC_DRAW);// Position data in attribute index 0, 3 floats per vertex
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0); // Enable attribute index 0

	// Now draw the particles... as easy as this!
	glDrawArrays(GL_POINTS, 0, numParticles );
	glBindVertexArray(0);
}

void particleArray::update(GLfloat dt, bool reset) {
	for (int i=0;i<numParticles * 3;i++) {
		positions[i].x += vel[i].x * dt;
		positions[i].y += vel[i].y * dt;
		positions[i].z += vel[i].z * dt;
		life[i] -= dt;

		if (reset)
		{	
			vel[i].x = (std::rand() % 100 - 50) / 10.0f;
			//vel[i].y = (std::rand() % 100 - 50) / 10.0f;
			vel[i].y = 1;
			vel[i].z = (std::rand() % 100 - 50) / 10.0f;
			life[i] = 3.0f;
			positions[i] = glm::vec3(0.0f, 0.0f, 0.0f);
			colours[i] = (std::rand() % 100) / 100.0f;
		}
	}
}