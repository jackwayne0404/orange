/* md2model.h - based on original:
 * md2.c -- md2 model loader

 * Modified: March 2013 by Daniel Livingstone
 * to remove use of GLTools, replacing with calls to GLM and rt3d library
 * and generally changed around to make life easier with VBOs
 * Still a quick hack overall.
 * To do:
 *   generate normals for each frame
 *   use index lists: need to remove duplicate vertices  

 * Modified: November 2010, by Daniel Livingstone
 * Modified to add very simple OO structure & some changes to
 * make it easy to import and use into existing code

 * Copyright (c) 2005-2007 David HENRY
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * gcc -Wall -ansi -lGL -lGLU -lglut md2.c -o md2
 */
#define WIN32
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "rt3d.h"
#include <vector>

// Animation List
// This contains the standard list of MD2 animations
// All Quake 2 models will conform to this list - but MD2 models can potentially
// store additional animations and frames. Such extensions are not be supported
// here.
#define MD2_STAND	0
#define MD2_RUN		1
#define MD2_ATTACK	2
#define MD2_PAIN1	3
#define MD2_PAIN2	4
#define MD2_PAIN3	5
#define MD2_JUMP	6
#define MD2_FLIP	7
#define MD2_SALUTE	8
#define MD2_TAUNT	9
#define MD2_WAVE	10
#define MD2_POINT	11
#define MD2_CRSTAND	12
#define MD2_CRWALK	13
#define MD2_CRATTACK	14
#define MD2_CRPAIN	15
#define MD2_CRDEATH	16
#define MD2_DEATH1	17
#define MD2_DEATH2	18
#define MD2_DEATH3	19


typedef GLfloat md2vec3[3];

/* MD2 header */
struct md2_header_t
{
  int ident;
  int version;

  int skinwidth;
  int skinheight;

  int framesize;

  int num_skins;
  int num_vertices;
  int num_st;
  int num_tris;
  int num_glcmds;
  int num_frames;

  int offset_skins;
  int offset_st;
  int offset_tris;
  int offset_frames;
  int offset_glcmds;
  int offset_end;
};

/* Texture name */
struct md2_skin_t
{
  char name[64];
};

/* Texture coords */
struct md2_texCoord_t
{
  short s;
  short t;
};

/* Triangle info */
struct md2_triangle_t
{
  unsigned short vertex[3];
  unsigned short st[3];
};

/* Compressed vertex */
struct md2_vertex_t
{
  unsigned char v[3];
  unsigned char normalIndex;
};

/* Model frame */
struct md2_frame_t
{
  md2vec3 scale;
  md2vec3 translate;
  char name[16];
  struct md2_vertex_t *verts;
};

/* GL command packet */
struct md2_glcmd_t
{
  float s;
  float t;
  int index;
};

/* MD2 model structure */
struct md2_model_t
{
  struct md2_header_t header;

  struct md2_skin_t *skins;
  struct md2_texCoord_t *texcoords;
  struct md2_triangle_t *triangles;
  struct md2_frame_t *frames;
  int *glcmds;

  GLuint tex_id;
};

/*** An MD2 model ***/
//struct md2_model_t md2file;

// md2model class
class md2model
{
public:
	md2model();
	md2model(const char *filename);
	~md2model();
	GLuint ReadMD2Model(const char *filename);
	void FreeModel();
	void Animate(int animation, float dt);
	void Animate(float dt) { Animate(currentAnim, dt); }
	int setCurrentAnim(int n);
private:
	md2_model_t mdl;
	int currentAnim;
	int currentFrame;
	int nextFrame;
	float interp;
	std::vector<GLfloat *> vertData;
	GLuint vertDataSize;
	GLfloat *animVerts;
public:
	GLfloat* getAnimVerts() { return animVerts; }
	GLuint getVertDataSize() { return vertDataSize; }
	GLuint getVertDataCount() { return vertDataSize/3; }
	int getCurrentAnim() {return currentAnim;}
};