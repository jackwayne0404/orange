#ifndef BULLET
#define BULLET

#include <glm/glm.hpp>

using namespace std;

class Bullet {
private:
	glm::vec3 bulletPosition;
	glm::vec3 bulletScale;

	//initial pitch,yaw,angle, of object
	float bulletPitch;
	float bulletYaw;
protected:

public:
	// probably get rid of this constructor
	Bullet::Bullet(glm::vec3 position, glm::vec3 scale, float yaw, float pitch) {
		bulletPosition = position;
		bulletScale = scale;
		bulletYaw = yaw;
		bulletPitch = pitch;
	}
	//~Bullet() {}
	void Bullet::setPosition(glm::vec3 newPos) { bulletPosition = newPos; }
	glm::vec3 Bullet::getPosition() { return bulletPosition; }
	glm::vec3 Bullet::getScale() { return bulletScale; }

	//get and set angles at shot
	float getPitchAngle() { return bulletPitch; }
	float getYawAngle() { return bulletYaw; }
};
#endif