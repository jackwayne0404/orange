// Authors : Matteo Marcuzzo, Declan McGee, Meshal Marakkar

// Application by B00273607, B00269701, B00275050

// Controls: WASD movement, R and F to move vertically, < and > to rotate
// 3 and 4 to switch between skybox view and depth cubemap view as skybox
// 0 on numpad to switch between lights (to see different cube shadowmaps)
// N and M to switch on and off parallax mapping
// Z and X to switch between particle light mode and "light shooter" mode
// Briefly; demo displays multiple lights attached to particles that cast shadows on simple geometry and parallax mapped cubes with self shadowing.


// Windows specific: Uncomment the following line to open a console window for debug output
#if _DEBUG
#pragma comment(linker, "/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")
#endif
// testing git hub
#include "rt3d.h"
#include "rt3dObjLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stack>
#include "md2model.h"
#include "Bullet.h"
#include "particleArray.h"

using namespace std;

enum sky { NORMAL, DEPTHMAP };
sky showSkybox = NORMAL;

#define DEG_TO_RADIAN 0.017453293
#define NR_POINT_LIGHTS 4
#define STARTING_LIGHT 0

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// Globals
// Real programs don't use globals :-D

GLuint meshIndexCount = 0;
GLuint toonIndexCount = 0;
GLuint md2VertCount = 0;
GLuint meshObjects[3];

//Shader programs
GLuint shadowShaderProgram; //Main shader for colours and shadows
GLuint depthShaderProgram; //shader to create shadow cubemaps
GLuint skyboxProgram; //shader to render skyboxes / can be used to render shadow cubemaps as a skybox
GLuint multipleParallaxProgram; //shader used for parallax occlusion
GLuint particleProgram; //shader used for particles


glm::vec3 eye(-2.0f, 1.0f, 8.0f);
glm::vec3 at(0.0f, 1.0f, -1.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);

GLfloat pitch = 0.0f;
GLfloat yaw = 0.0f;
GLfloat roll = 0.0f;

stack<glm::mat4> mvStack; 

//////////////////
/// FBO globals
//////////////////
GLuint depthMapFBO[NR_POINT_LIGHTS]; // FBO
//GLuint depthMap;	// FBO texture
const GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
const GLuint screenWidth = 800, screenHeight = 600;

GLfloat aspect = (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT;
GLfloat near = 0.01f;
GLfloat far = 25.0f;

GLuint depthCubemap[NR_POINT_LIGHTS];

// TEXTURE STUFF
GLuint textures[8];
GLuint textures_other[4];
GLuint skybox[5];

// starting light positions (only for non particle lights)
glm::vec3 pointLightPositions[] = {
	glm::vec3(0.7f,  2.2f,  2.0f),
	glm::vec3(2.3f, 5.3f, -4.0f),
	glm::vec3(-4.0f,  2.0f, -12.0f),
	glm::vec3(-9.0f,  6.0f, -6.0f),
	glm::vec3(0.0f,  1.0f, -3.0f)
};

//glm::vec3 pointLightPositions_test[NR_POINT_LIGHTS]; // not used

// Movement and control variables
float theta = 0.0f;
float moveVar = 0.0f;
bool switchMov = false;
bool toggleDepthMap = false;
GLuint selectedLight = 0;

GLuint lastTime;
GLuint currentTime;
particleArray* particleSystem;
float fade = 3.0f;
bool reset = false; //reset particles to start position
int numOfParticles = NR_POINT_LIGHTS;

//#define BULLET_NO NR_POINT_LIGHTS
Bullet *bullet[NR_POINT_LIGHTS];
int numShotsFired = 0;
bool shotsFired = false;
float coolDownOfGun = 1.0; //wait between shots

bool gunMode = false;
bool particleMode = true;

bool parallax = false; //make cube parallax occluded or not
//mouse control variables
bool toggleMouse = false;
bool leftClick = false;

// md2 stuff
md2model tmpModel;
int currentAnim = 0;

// Set up rendering context
SDL_Window * setupRC(SDL_GLContext &context) {
	SDL_Window * window;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) // Initialize video
        rt3d::exitFatalError("Unable to initialize SDL"); 
	  
    // Request an OpenGL 3.0 context.
	
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); 

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);  // double buffering on
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8); // 8 bit alpha buffering
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // Turn on x4 multisampling anti-aliasing (MSAA)
 
    // Create 800x600 window
    window = SDL_CreateWindow("SDL/GLM/OpenGL Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN );
	if (!window) // Check window was created OK
        rt3d::exitFatalError("Unable to create window");
 
    context = SDL_GL_CreateContext(window); // Create opengl context and attach to window
    SDL_GL_SetSwapInterval(1); // set swap buffers to sync with monitor's vertical refresh rate
	return window;
}

// A simple texture loading function
// lots of room for improvement - and better error checking!
GLuint loadBitmap(char *fname) {
	GLuint texID;
	glGenTextures(1, &texID); // generate texture ID

	// load file - using core SDL library
 	SDL_Surface *tmpSurface;
	tmpSurface = SDL_LoadBMP(fname);
	if (!tmpSurface) {
		std::cout << "Error loading bitmap" << std::endl;
	}

	// bind texture and set parameters
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SDL_PixelFormat *format = tmpSurface->format;
	
	GLuint externalFormat, internalFormat;
	if (format->Amask) {
		internalFormat = GL_RGBA;
		externalFormat = (format->Rmask < format-> Bmask) ? GL_RGBA : GL_BGRA;
	}
	else {
		internalFormat = GL_RGB;
		externalFormat = (format->Rmask < format-> Bmask) ? GL_RGB : GL_BGR;
	}

	glTexImage2D(GL_TEXTURE_2D,0,internalFormat,tmpSurface->w, tmpSurface->h, 0,
		externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);

	SDL_FreeSurface(tmpSurface); // texture loaded, free the temporary buffer
	return texID;	// return value of texture ID
}

GLuint loadCubeMap(const char *fname[6], GLuint *texID)
{
	glGenTextures(1, texID); // generate texture ID
	GLenum sides[6] = { GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y };
	SDL_Surface *tmpSurface;

	glBindTexture(GL_TEXTURE_CUBE_MAP, *texID); // bind texture and set parameters
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	GLuint externalFormat;
	for (int i = 0;i<6;i++)
	{
		// load file - using core SDL library
		tmpSurface = SDL_LoadBMP(fname[i]);
		if (!tmpSurface)
		{
			std::cout << "Error loading bitmap" << std::endl;
			return *texID;
		}

		// skybox textures should not have alpha (assuming this is true!)
		SDL_PixelFormat *format = tmpSurface->format;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGB : GL_BGR;

		glTexImage2D(sides[i], 0, GL_RGB, tmpSurface->w, tmpSurface->h, 0,
			externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
		// texture loaded, free the temporary buffer
		SDL_FreeSurface(tmpSurface);
	}
	return *texID;	// return value of texure ID, redundant really
}

void calculateTangents(vector<GLfloat> &tangents, vector<GLfloat> &verts, vector<GLfloat> &normals, vector<GLfloat> &tex_coords, vector<GLuint> &indices) {

	// Code taken from http://www.terathon.com/code/tangent.html and modified slightly to use vectors instead of arrays
	// Lengyel, Eric. “Computing Tangent Space Basis Vectors for an Arbitrary Mesh”. Terathon Software 3D Graphics Library, 2001. 

	// This is a little messy because my vectors are of type GLfloat:
	// should have made them glm::vec2 and glm::vec3 - life, would be much easier!

	vector<glm::vec3> tan1(verts.size() / 3, glm::vec3(0.0f));
	vector<glm::vec3> tan2(verts.size() / 3, glm::vec3(0.0f));
	int triCount = indices.size() / 3;
	for (int c = 0; c < indices.size(); c += 3)
	{
		int i1 = indices[c];
		int i2 = indices[c + 1];
		int i3 = indices[c + 2];

		glm::vec3 v1(verts[i1 * 3], verts[i1 * 3 + 1], verts[i1 * 3 + 2]);
		glm::vec3 v2(verts[i2 * 3], verts[i2 * 3 + 1], verts[i2 * 3 + 2]);
		glm::vec3 v3(verts[i3 * 3], verts[i3 * 3 + 1], verts[i3 * 3 + 2]);

		glm::vec2 w1(tex_coords[i1 * 2], tex_coords[i1 * 2 + 1]);
		glm::vec2 w2(tex_coords[i2 * 2], tex_coords[i2 * 2 + 1]);
		glm::vec2 w3(tex_coords[i3 * 2], tex_coords[i3 * 2 + 1]);

		float x1 = v2.x - v1.x;
		float x2 = v3.x - v1.x;
		float y1 = v2.y - v1.y;
		float y2 = v3.y - v1.y;
		float z1 = v2.z - v1.z;
		float z2 = v3.z - v1.z;

		float s1 = w2.x - w1.x;
		float s2 = w3.x - w1.x;
		float t1 = w2.y - w1.y;
		float t2 = w3.y - w1.y;

		float r = 1.0F / (s1 * t2 - s2 * t1);
		glm::vec3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
			(t2 * z1 - t1 * z2) * r);
		glm::vec3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
			(s1 * z2 - s2 * z1) * r);

		tan1[i1] += sdir;
		tan1[i2] += sdir;
		tan1[i3] += sdir;

		tan2[i1] += tdir;
		tan2[i2] += tdir;
		tan2[i3] += tdir;
	}

	for (int a = 0; a < verts.size(); a += 3)
	{
		glm::vec3 n(normals[a], normals[a + 1], normals[a + 2]);
		glm::vec3 t = tan1[a / 3];

		glm::vec3 tangent;
		tangent = (t - n * glm::normalize(glm::dot(n, t)));

		// handedness
		GLfloat w = (glm::dot(glm::cross(n, t), tan2[a / 3]) < 0.0f) ? -1.0f : 1.0f;

		tangents.push_back(tangent.x);
		tangents.push_back(tangent.y);
		tangents.push_back(tangent.z);
		tangents.push_back(w);
	}
}

// Function that initializes shaders, objects and so on
void init(void) {
	// Setting up the shaders
	shadowShaderProgram = rt3d::initShaders("pointShadows.vert", "pointShadows.frag");
	depthShaderProgram = rt3d::initShaders("simpleShadowMap.vert", "simpleShadowMap.frag", "simpleShadowMap.gs");
	skyboxProgram = rt3d::initShaders("cubeMap.vert", "cubeMap.frag");
	particleProgram = rt3d::initShaders("particle.vert", "particle.frag");
	multipleParallaxProgram = rt3d::initShaders("multipleParallaxLights.vert", "multipleParallaxLight.frag");

	vector<GLfloat> verts;
	vector<GLfloat> norms;
	vector<GLfloat> tex_coords;

	const char *cubeTexFiles[6] = {
		"Town-skybox/cloudtop_bk.bmp", "Town-skybox/cloudtop_ft.bmp", "Town-skybox/cloudtop_rt.bmp", "Town-skybox/cloudtop_lf.bmp", "Town-skybox/cloudtop_up.bmp", "Town-skybox/cloudtop_dn.bmp"
	};
	loadCubeMap(cubeTexFiles, &skybox[0]);
	
	vector<GLuint> indices;
	rt3d::loadObj("cube.obj", verts, norms, tex_coords, indices);
	meshIndexCount = indices.size();
	textures_other[0] = loadBitmap("fabric.bmp");
	meshObjects[0] = rt3d::createMesh(verts.size()/3, verts.data(), nullptr, norms.data(), tex_coords.data(), meshIndexCount, indices.data());

	// also need for normal mapping a VBO for the bitangents
	vector<GLfloat> tangents;
	calculateTangents(tangents, verts, norms, tex_coords, indices);
	
	textures_other[1] = loadBitmap("hobgoblin2.bmp");
	meshObjects[1] = tmpModel.ReadMD2Model("tris.MD2");
	md2VertCount = tmpModel.getVertDataCount();
	
	textures_other[2] = loadBitmap("studdedmetal.bmp");
	textures_other[3] = loadBitmap("tex3.bmp");
		
	verts.clear(); norms.clear();tex_coords.clear();indices.clear();
	rt3d::loadObj("bunny-5000.obj", verts, norms, tex_coords, indices);
	toonIndexCount = indices.size();
	meshObjects[2] = rt3d::createMesh(verts.size()/3, verts.data(), nullptr, norms.data(), nullptr, toonIndexCount, indices.data());

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);


	glBindVertexArray(meshObjects[0]);
	GLuint VBO;
	glGenBuffers(1, &VBO);
	// VBO for tangent data
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, tangents.size()*sizeof(GLfloat), tangents.data(), GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)5, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(5);
	glBindVertexArray(0);

	textures[0] = loadBitmap("diffuseMap.bmp");
	textures[1] = loadBitmap("heightMap.bmp");
	textures[2] = loadBitmap("normalMap.bmp");

	textures[6] = loadBitmap("spotLight.bmp");
	textures[7] = loadBitmap("particle08.bmp");

	particleSystem = new particleArray(NR_POINT_LIGHTS);
	glPointSize(30.0f);//Setting point size for the particle system
	glEnable(GL_POINT_SPRITE);

	////////////////////
	/// FBO for shadows
	/////////////////////
	for (int i = STARTING_LIGHT; i < NR_POINT_LIGHTS; i++) {
		glGenFramebuffers(1, &depthMapFBO[i]);
		// Create depth cubemap texture
	//	GLuint depthCubemap;
		glGenTextures(1, &depthCubemap[i]);
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[i]);
		for (GLuint i = 0; i < 6; ++i)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		// Attach cubemap as depth map FBO's color buffer
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[i]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap[i], 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "Framebuffer not complete!" << std::endl;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
		
}

// Functions used for camera movement
glm::vec3 moveForward(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d*std::sin(yaw*DEG_TO_RADIAN), pos.y, pos.z - d*std::cos(yaw*DEG_TO_RADIAN));
}

glm::vec3 moveRight(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d*std::cos(yaw*DEG_TO_RADIAN), pos.y, pos.z + d*std::sin(yaw*DEG_TO_RADIAN));
}

// mouse camera
void lockCamera()
{
	if (pitch > 70)
		pitch = 70;
	if (pitch < -70)
		pitch = -70;
	if (yaw < 0.0)
		yaw += 360.0;
	if (yaw > 360.0)
		yaw -= 360;
}
// camera function
void camera() {
	at = moveForward(eye, yaw, 1.0f);
	at.y -= pitch;
	mvStack.top() = glm::lookAt(eye, at, up);
}
// "light bullet"
void bulletCreation() {
	glm::vec3 bulletSpawn = eye;
	bulletSpawn = moveForward(eye, yaw, 0.5f);
	bullet[numShotsFired] = new Bullet(bulletSpawn, glm::vec3(0.5, 0.5, 0.5), yaw, pitch);
	numShotsFired++;
}

// mainly used for controls; also updates a downscaled vec3 light position vector
void update(SDL_Window * window, SDL_Event sdlEvent) {
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	if ( keys[SDL_SCANCODE_W] ) eye = moveForward(eye,yaw,0.1f);
	if ( keys[SDL_SCANCODE_S] ) eye = moveForward(eye,yaw,-0.1f);
	if ( keys[SDL_SCANCODE_A] ) eye = moveRight(eye,yaw,-0.1f);
	if ( keys[SDL_SCANCODE_D] ) eye = moveRight(eye,yaw,0.1f);
	if ( keys[SDL_SCANCODE_R] ) eye.y += 0.1;
	if ( keys[SDL_SCANCODE_F] ) eye.y -= 0.1;

	if (keys[SDL_SCANCODE_COMMA]) yaw -= 1.0f;
	else if (keys[SDL_SCANCODE_PERIOD]) yaw += 1.0f;


	if (keys[SDL_SCANCODE_KP_0]) selectedLight ++;
	if (selectedLight > NR_POINT_LIGHTS-1) selectedLight = 0;

	if (keys[SDL_SCANCODE_ESCAPE]) exit(0);

	if (keys[SDL_SCANCODE_KP_8]) pointLightPositions[selectedLight].z -= 0.1;
	else if (keys[SDL_SCANCODE_KP_5]) pointLightPositions[selectedLight].z += 0.1;
	if (keys[SDL_SCANCODE_KP_4]) pointLightPositions[selectedLight].x -= 0.1;
	else if (keys[SDL_SCANCODE_KP_6]) pointLightPositions[selectedLight].x += 0.1;
	if (keys[SDL_SCANCODE_KP_9]) pointLightPositions[selectedLight].y += 0.1;
	else if (keys[SDL_SCANCODE_KP_3]) pointLightPositions[selectedLight].y -= 0.1;

	if ( keys[SDL_SCANCODE_1] ) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);
	}
	if ( keys[SDL_SCANCODE_2] ) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);
	}
	// switch between skybox and depthmap view
	if (keys[SDL_SCANCODE_3]) showSkybox = NORMAL;
	if (keys[SDL_SCANCODE_4]) showSkybox = DEPTHMAP;


	if (keys[SDL_SCANCODE_N]) parallax = false;
	if (keys[SDL_SCANCODE_M]) parallax = true;

	if (keys[SDL_SCANCODE_Z]) {
		toggleMouse = true;
		gunMode = true;
		particleMode = false;
	}
	else if (keys[SDL_SCANCODE_X]) {
		toggleMouse = false;
		gunMode = false;
		particleMode = true;
		numShotsFired = 0;
	}
	if (keys[SDL_SCANCODE_P]) reset = true;
	if (toggleMouse)
	{
		int MidX = SCREEN_WIDTH / 2;
		int MidY = SCREEN_HEIGHT / 2;

		SDL_ShowCursor(SDL_DISABLE);
		int tmpx, tmpy;
		SDL_GetMouseState(&tmpx, &tmpy);
		yaw -= 0.1*(MidX - tmpx); //for y
		pitch -= 0.1*(MidY - tmpy) / 10; //for x
		lockCamera();

		//rotate the camera (move everything in the opposit direction)
		glRotatef(-pitch, 1.0, 0.0, 0.0); //basically glm::rotate
		glRotatef(-yaw, 0.0, 1.0, 0.0);
		SDL_WarpMouseInWindow(window, MidX, MidY);

		if (sdlEvent.type == SDL_MOUSEBUTTONDOWN)
			if (sdlEvent.button.button == SDL_BUTTON_LEFT)
				leftClick = true;

		if (leftClick == true) {
			if (numShotsFired >= NR_POINT_LIGHTS)
				cout << "no more ammo";
			else if (coolDownOfGun <= 0.0f) {
				shotsFired = true;
				bulletCreation();
				coolDownOfGun = 1.0f;
			}
		}
		if (sdlEvent.type == SDL_MOUSEBUTTONUP)
			leftClick = false;
	}
}

// Rendering functions; each of these renders a different part of the scene
// For the sake of simplicity and not causing confusion, we reset the model matrix instead of pushing an identity to the modelview stack
void renderBaseCube(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-10.0f, -0.1f, -10.0f));
	model = glm::scale(model, glm::vec3(20.0f, 0.1f, 20.0f));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
}

void renderSpinningCube(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-6.0f, 1.0f, -3.0f));
	model = glm::rotate(model, float(theta*DEG_TO_RADIAN), glm::vec3(1.0f, 1.0f, 1.0f));
	model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
}

void renderTallCubes(GLuint shader) {
	glm::mat4 model;
	for (int b = 0; b <5; b++) {
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(-10.0f + b * 2, 2.0f, -12.0f + b * 2));
		model = glm::scale(model, glm::vec3(0.5f, 1.0f + b/3, 0.5f));
		rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
		rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
	}
}

void renderBunny(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-7.0f, 0.5f, -2.0f));
	model = glm::scale(model, glm::vec3(10.0, 10.0, 10.0));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[2], toonIndexCount, GL_TRIANGLES);
}

void renderHobgoblin(GLuint shader) {
	// animation can adversely impact performace on some machine.
	// This is particularly true in the case of multiple lights / shadows, and has hence been commented out
	// In a real case scenario it would be sensible to animate it outside the rendering function, as calling this twice will make all animations twice as fast

	//tmpModel.Animate(currentAnim, 0.1);
	//rt3d::updateMesh(meshObjects[1], RT3D_VERTEX, tmpModel.getAnimVerts(), tmpModel.getVertDataSize());

	// draw the hobgoblin
	glCullFace(GL_FRONT); // md2 faces are defined clockwise, so cull front face
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures_other[1]);
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-8.0f, 1.2f, -6.0f));
	model = glm::rotate(model, float(90.0f*DEG_TO_RADIAN), glm::vec3(-1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(1.0*0.05, 1.0*0.05, 1.0*0.05));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawMesh(meshObjects[1], md2VertCount, GL_TRIANGLES);
	glCullFace(GL_BACK);

	// reset texture
	glBindTexture(GL_TEXTURE_2D, 0);
}

void renderMovingCube(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-7.0f+moveVar, 4.0f, -2.0f+moveVar));
	model = glm::rotate(model, float(theta*DEG_TO_RADIAN), glm::vec3(1.0f, 1.0f, 1.0f));
	model = glm::scale(model, glm::vec3(0.3f, 0.5f, 0.6f));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
}

// updates variables to move objects in the scene (for testing purposes)
void moveObjects() {
	theta += 2.0f;
	if (moveVar >= 3.0f)
		switchMov = false;
	if (moveVar <= -3.0f)
		switchMov = true;
	if (switchMov == true) {
		moveVar += 0.02f;
	}
	if (switchMov == false) {
		moveVar -= 0.02f;
	}
}

//function that passes all light positions and properties to the shader
void pointLights(GLuint shader) {

	int modeSpecificVariable;
	if (gunMode) modeSpecificVariable = numShotsFired;
	else if (particleMode) modeSpecificVariable = numOfParticles;

	GLuint uniformIndex = glGetUniformLocation(shader, "viewPos");
	glUniform3fv(uniformIndex, 1, glm::value_ptr(eye));
	uniformIndex = glGetUniformLocation(shader, "numShotsFired");
	glUniform1i(uniformIndex, modeSpecificVariable);

	for (int i = STARTING_LIGHT; i < NR_POINT_LIGHTS; i++) {
		string number = to_string(i);
		uniformIndex = glGetUniformLocation(shader, ("pointLights[" + number + "].position").c_str());
		glUniform3f(uniformIndex, pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);
		uniformIndex = glGetUniformLocation(shader, ("pointLights[" + number + "].ambient").c_str());
		glUniform3f(uniformIndex, 0.05f, 0.05f, 0.05f);
		uniformIndex = glGetUniformLocation(shader, ("pointLights[" + number + "].diffuse").c_str());
		glUniform3f(uniformIndex, 0.8f, 0.8f, 0.8f);
		uniformIndex = glGetUniformLocation(shader, ("pointLights[" + number + "].specular").c_str());
		glUniform3f(uniformIndex, 1.0f, 1.0f, 1.0f);
		uniformIndex = glGetUniformLocation(shader, ("pointLights[" + number + "].constant").c_str());
		glUniform1f(uniformIndex, 1.0f);
		uniformIndex = glGetUniformLocation(shader, ("pointLights[" + number + "].linear").c_str());
		glUniform1f(uniformIndex, 0.09);
		uniformIndex = glGetUniformLocation(shader, ("pointLights[" + number + "].quadratic").c_str());
		glUniform1f(uniformIndex, 0.032);
	}
}

// draw the parallax mapped cube
void drawMappedCube(GLuint shader, bool parallax, glm::vec3 translate, glm::mat4 projection) {
	glUseProgram(shader);
	pointLights(shader);

	GLuint uniformIndex = glGetUniformLocation(shader, "far_plane");
	glUniform1f(uniformIndex, far);	
	uniformIndex = glGetUniformLocation(shader, "viewPos");
	glUniform3fv(uniformIndex, 1, glm::value_ptr(eye));

	// pass in depthmaps for shadows
	for (int i = 0; i < NR_POINT_LIGHTS; i++) {
		uniformIndex = glGetUniformLocation(shader, ("depthMap[" + to_string(i) + "]").c_str());
		glUniform1i(uniformIndex, 5 + i);
		glActiveTexture(GL_TEXTURE5 + i);
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[i]);
	}
	// Now bind textures to texture units
	uniformIndex = glGetUniformLocation(shader, "diffuseMap");
	glUniform1i(uniformIndex, 10);
	uniformIndex = glGetUniformLocation(shader, "heightMap");
	glUniform1i(uniformIndex, 11);
	uniformIndex = glGetUniformLocation(shader, "normalMap");
	glUniform1i(uniformIndex, 12);
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, textures[2]);

	rt3d::setUniformMatrix4fv(shader, "view", glm::value_ptr(mvStack.top()));
	rt3d::setUniformMatrix4fv(shader, "projection", glm::value_ptr(projection));

	glm::mat4 model(1.0); // new
	model = glm::translate(model, translate);
	uniformIndex = glGetUniformLocation(shader, "cameraPos");
	glUniform3fv(uniformIndex, 1, glm::value_ptr(eye));
	glUniform1i(glGetUniformLocation(shader, "parallax"), parallax);
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));

	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
}

// Since we're generating a depth cubemap, we'll need a different view matrix per each of the 6 directions
// we can generate these with glm::lookat and pass them to the geometry shader
// these are the equivalent of the lightspace transform matrix used in conventional shadow mapping
void pointShadows(GLuint shader, int i) {
		glm::mat4 shadowProj = glm::perspective(float(90.0f*DEG_TO_RADIAN), aspect, near, far); //perspective projection is the best suited for this
		std::vector<glm::mat4> shadowTransforms;
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLightPositions[i], pointLightPositions[i] + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLightPositions[i], pointLightPositions[i] + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLightPositions[i], pointLightPositions[i] + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLightPositions[i], pointLightPositions[i] + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLightPositions[i], pointLightPositions[i] + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLightPositions[i], pointLightPositions[i] + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));

		for (int k = 0; k < 6; ++k)
			glUniformMatrix4fv(glGetUniformLocation(shader, ("shadowMatrices[" + std::to_string(k) + "]").c_str()), 1, GL_FALSE, glm::value_ptr(shadowTransforms[k]));
}

//render cubes at light position, mainly used for debugging
void renderlightCubes(GLuint shader) {
	for (int i = STARTING_LIGHT; i < NR_POINT_LIGHTS; i++)
	{
		glm::mat4 model;
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z));
		model = glm::scale(model, glm::vec3(0.05f, 0.05f, 0.05f));
		rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
		rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
	}
}

//render skybox; can be used to display the shadow cubemaps as the skybox for a visual representation of what the light sees
void renderSkybox(glm::mat4 projection) {
	glUseProgram(skyboxProgram);
	rt3d::setUniformMatrix4fv(skyboxProgram, "projection", glm::value_ptr(projection));

	glDepthMask(GL_FALSE); // make sure writing to update depth test is off
	glm::mat3 mvRotOnlyMat3 = glm::mat3(mvStack.top());
	mvStack.push(glm::mat4(mvRotOnlyMat3));

	glCullFace(GL_FRONT); // drawing inside of cube!
	if (showSkybox == NORMAL)
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox[0]);
	else if (showSkybox == DEPTHMAP)
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[selectedLight]);
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(1.5f, 1.5f, 1.5f));
	rt3d::setUniformMatrix4fv(skyboxProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
	mvStack.pop();
	glCullFace(GL_BACK); // drawing inside of cube!

						 // back to remainder of rendering
	glDepthMask(GL_TRUE); // make sure depth test is on
}

// render particle-lights
void renderParticleSystem(GLuint shader, GLfloat dt, glm::mat4 projection) {
	//fade -= dt;

	GLuint uniformIndex = glGetUniformLocation(shader, "ex_alpha");
	glUniform1f(uniformIndex, fade);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(0.0f, 0.0f, 0.0f));
	glm::mat4 mvp = projection*mvStack.top();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[7]);
	rt3d::setUniformMatrix4fv(shader, "MVP", glm::value_ptr(mvp));
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glEnable(GL_BLEND);
	glDepthMask(0);
	particleSystem->draw();
	for (int i = 0; i < NR_POINT_LIGHTS; i++)
		pointLightPositions[i] = particleSystem->getPositions()[i];
	glDepthMask(1);
	glDisable(GL_BLEND);
	particleSystem->update(dt, reset);
	reset = false;
	lastTime = currentTime;
	mvStack.pop();
}


void renderBullet(GLuint shader) {
	glBindTexture(GL_TEXTURE_2D, textures[6]);

	//rt3d::setUniformMatrix4fv(shader, "modelview", glm::value_ptr(viewMatrix));
	for (int i = 0; i < numShotsFired; i++) {
		glm::mat4 model;
		model = glm::translate(model, bullet[i]->getPosition());
		pointLightPositions[i] = bullet[i]->getPosition();


		glm::vec3 bulletPos = bullet[i]->getPosition();
		float angleAtShot = bullet[i]->getYawAngle();
		glm::vec3 newPos = glm::vec3(bulletPos.x + 0.05f*std::sin(angleAtShot*DEG_TO_RADIAN),
			bulletPos.y - 0.05f*sin(std::atan(bullet[i]->getPitchAngle())),
			bulletPos.z - 0.05f*std::cos(angleAtShot*DEG_TO_RADIAN));
		bullet[i]->setPosition(newPos);
	}
}

// main render function, sets up the shaders and then calls all other functions
void RenderShadowScene(glm::mat4 projection, glm::mat4 viewMatrix, GLuint shader, bool cubemap, int shadowPass) {

	GLuint uniformIndex;
	glUseProgram(shader);
	pointLights(shader);
	// if cubemap translates into "if rendering to the depthmap"
	if (cubemap) {
		glUniform1i(glGetUniformLocation(depthShaderProgram, "currentLight"), shadowPass);
		pointShadows(shader, shadowPass);
	}
	
	uniformIndex = glGetUniformLocation(shader, "far_plane");
	glUniform1f(uniformIndex, far);

	//similarly, if (!cubemap) refers to normal rendering
	if(!cubemap){
		rt3d::setUniformMatrix4fv(shader, "projection", glm::value_ptr(projection));
		rt3d::setUniformMatrix4fv(shader, "view", glm::value_ptr(viewMatrix));
		// material properties in this case roughly translate to textures
		uniformIndex = glGetUniformLocation(shader, "material.diffuse");
		glUniform1i(uniformIndex, 0);
		uniformIndex = glGetUniformLocation(shader, "material.specular");
		glUniform1i(uniformIndex, 0);
		glUniform1f(glGetUniformLocation(shader, "material.shininess"), 32.0f); //??
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures_other[3]);

		for (int i = 0; i < NR_POINT_LIGHTS; i++) {
			// pass in the shadowmaps, each in a different texture unit
			uniformIndex = glGetUniformLocation(shader, ("depthMap[" + to_string(i) + "]").c_str());
			glUniform1i(uniformIndex, 1+i);
			glActiveTexture(GL_TEXTURE1+i);
			glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[i]);
		}
	}
		//draw normal scene
		renderBaseCube(shader);
		renderSpinningCube(shader);
		renderTallCubes(shader);
		renderMovingCube(shader);
		renderBunny(shader);
		renderHobgoblin(shader);
		
		// if drawing to shadowmap, draw mapped cube
		if(cubemap) drawMappedCube(shader, parallax, glm::vec3(1.0f, 2.0f, -5.0f), projection);

		if (!cubemap) {
			if (gunMode) {
				glUseProgram(shader);
				//render small cubes at light positions when shooting
				renderlightCubes(shader);
			}
			drawMappedCube(multipleParallaxProgram, parallax, glm::vec3(1.0f, 2.0f, -5.0f), projection);
		
			currentTime = SDL_GetTicks();
			GLfloat dt;
			dt = (currentTime - lastTime) / 1000.0;
			lastTime = currentTime;

			if (gunMode) {
				renderBullet(shader);
				coolDownOfGun -= dt;
			}
			if (particleMode) {
				glUseProgram(particleProgram);
				renderParticleSystem(particleProgram, dt, projection);
			}

		}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

}



// draw function called in the main loop
void draw(SDL_Window * window) {

	// clear the screen
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection(1.0);
	projection = glm::perspective(float(60.0f*DEG_TO_RADIAN), 800.0f / 600.0f, 1.0f, 150.0f);
	GLfloat scale(1.0f); // just to allow easy scaling of complete scene

	glm::mat4 modelview(1.0); // set base position for scene
	mvStack.push(modelview);

	// render in two passes
	// first shadow to FBO
	// then scene using depthmap data
	moveObjects();


	for (int pass = 0; pass < 2; pass++) {
		camera();

		if (pass == 0) {
			// need to render once per light
			for (int i = STARTING_LIGHT; i < NR_POINT_LIGHTS; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[i]);
				glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap[i], 0);
				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
				glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[i]);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear FBO
				glClear(GL_DEPTH_BUFFER_BIT);

				RenderShadowScene(projection, mvStack.top(), depthShaderProgram, true, i); // render using light's point of view and simpler shader program

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}

		} else {
		
			//Render to frame buffer
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glViewport(0, 0, screenWidth, screenHeight);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear window
																// clear the screen
			glEnable(GL_CULL_FACE);
			glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	
			renderSkybox(projection);		
			// normal rendering
			RenderShadowScene(projection, mvStack.top(), shadowShaderProgram, false, 0); // render normal scene from normal point of view
		}
		glDepthMask(GL_TRUE);
	}
	mvStack.pop();
	SDL_GL_SwapWindow(window); // swap buffers

}

// Program entry point - SDL manages the actual WinMain entry point for us
int main(int argc, char *argv[]) {
    SDL_Window * hWindow; // window handle
    SDL_GLContext glContext; // OpenGL context handle
    hWindow = setupRC(glContext); // Create window and render context 

	// Required on Windows *only* init GLEW to access OpenGL beyond 1.1d
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err) { // glewInit failed, something is seriously wrong
		std::cout << "glewInit failed, aborting." << endl;
		exit (1);
	}
	cout << glGetString(GL_VERSION) << endl;

	init();

	bool running = true; // set running to true
	SDL_Event sdlEvent;  // variable to detect SDL events
	while (running)	{	// the event loop
		while (SDL_PollEvent(&sdlEvent)) {
			if (sdlEvent.type == SDL_QUIT)
				running = false;
		}
		update(hWindow, sdlEvent);
		draw(hWindow); // call the draw function
	}

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(hWindow);
    SDL_Quit();
    return 0;
}