/* md2model.cpp - based on original:
* md2.c -- md2 model loader
* last modification: November 2010 by Daniel Livingstone
*
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


#include "rt3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "md2model.h"
#include <vector>

/* Table of precalculated normals */
md2vec3 anorms_table[162] = {
#include "anorms.h"
};

// Table of animation begin/end frames
static int animFrameList[40] = {
	0, 39, // stand
	40, 45, // run
	46, 53, // attack
	54, 57, //pain1
	58, 61, //pain2
	62, 65, //pain3
	66, 71, //jump
	72, 83, //flip
	84, 94, //salute
	95, 111, //taunt
	112, 122, //wave
	123, 134, //point
	135, 153, //crstnd
	154, 159, //crwalk
	160, 168, //crattak
	169, 172, //crpain
	173, 177, //crdeath
	178, 183, //death1
	184, 189, //death2
	190, 196 //death3
};

md2model::md2model()
{
	currentAnim = 0;
	currentFrame = 0;
	nextFrame = 1;
	interp = 0.0f;
}

md2model::md2model(const char *filename)
{
	ReadMD2Model(filename);
	currentAnim = 0;
	currentFrame = 0;
	nextFrame = 1;
	interp = 0.0f;
}

md2model::~md2model()
{
	FreeModel();
	int i;
	for (i=0;i<mdl.header.num_frames;++i) {
		delete [] vertData[i];
	}
	delete [] animVerts;
}

/**
* Load an MD2 model from file.
*
* Note: MD2 format stores model's data in little-endian ordering.  On
* big-endian machines, you'll have to perform proper conversions.
*/
GLuint md2model::ReadMD2Model (const char *filename)
{
	FILE *fp;
	int i;

	fp = fopen (filename, "rb");
	if (!fp)
	{
		fprintf (stderr, "Error: couldn't open \"%s\"!\n", filename);
		return 0;
	}

	/* Read header */
	fread (&mdl.header, 1, sizeof (struct md2_header_t), fp);

	if ((mdl.header.ident != 844121161) ||
		(mdl.header.version != 8))
	{
		/* Error! */
		fprintf (stderr, "Error: bad version or identifier\n");
		fclose (fp);
		return 0;
	}

	/* Memory allocations */
	mdl.skins = (struct md2_skin_t *)
		malloc (sizeof (struct md2_skin_t) * mdl.header.num_skins);
	mdl.texcoords = (struct md2_texCoord_t *)
		malloc (sizeof (struct md2_texCoord_t) * mdl.header.num_st);
	mdl.triangles = (struct md2_triangle_t *)
		malloc (sizeof (struct md2_triangle_t) * mdl.header.num_tris);
	mdl.frames = (struct md2_frame_t *)
		malloc (sizeof (struct md2_frame_t) * mdl.header.num_frames);
	mdl.glcmds = (int *)malloc (sizeof (int) * mdl.header.num_glcmds);

	/* Read model data */
	fseek (fp, mdl.header.offset_skins, SEEK_SET);
	fread (mdl.skins, sizeof (struct md2_skin_t),
		mdl.header.num_skins, fp);

	fseek (fp, mdl.header.offset_st, SEEK_SET);
	fread (mdl.texcoords, sizeof (struct md2_texCoord_t),
		mdl.header.num_st, fp);

	fseek (fp, mdl.header.offset_tris, SEEK_SET);
	fread (mdl.triangles, sizeof (struct md2_triangle_t),
		mdl.header.num_tris, fp);

	fseek (fp, mdl.header.offset_glcmds, SEEK_SET);
	fread (mdl.glcmds, sizeof (int), mdl.header.num_glcmds, fp);

	/* Read frames */
	fseek (fp, mdl.header.offset_frames, SEEK_SET);
	for (i = 0; i < mdl.header.num_frames; ++i)
	{
		/* Memory allocation for vertices of this frame */
		mdl.frames[i].verts = (struct md2_vertex_t *)
			malloc (sizeof (struct md2_vertex_t) * mdl.header.num_vertices);

		/* Read frame data */
		fread (mdl.frames[i].scale, sizeof (md2vec3), 1, fp);
		fread (mdl.frames[i].translate, sizeof (md2vec3), 1, fp);
		fread (mdl.frames[i].name, sizeof (char), 16, fp);
		fread (mdl.frames[i].verts, sizeof (struct md2_vertex_t),
			mdl.header.num_vertices, fp);
	}

	fclose (fp);

	// now generate VBO data and create mesh
	// then save the data we actually need and free all the stuff we no longer need
	// this is required to allow the correct generation of normals etc

	int j;
	GLfloat s, t;
	md2vec3 v, *norm;
	struct md2_frame_t *pframe;
	struct md2_vertex_t *pvert;

	//std::vector<GLfloat> verts;
	// these automatic variables will be created on stack and automatically deleted when this
	// function ends - no need to delete
	std::vector<GLfloat> tex_coords;
	std::vector<GLfloat> norms;
	std::vector<GLuint> indices;

	pframe = &mdl.frames[0]; // first frame
	// For each triangle 
	// The bad news for MD2 is that we need to expand the triangles to get the data
	// The number of tex coords need not match the number of vertices, and the number of normals
	// is different again. So we need to calculate each vertex
	// Is it possible to use indexing and remove duplicates though...
	// But I'll not bother doing that for this aged format that I'm definately dumping next year
	// (about 8 years overdue...! :-D )
	for (i = 0; i < mdl.header.num_tris; ++i)
	{
		// For each vertex 
		for (j = 0; j < 3; ++j)
		{
			// Get texture coordinates 
			tex_coords.push_back( (GLfloat)mdl.texcoords[mdl.triangles[i].st[j]].s / mdl.header.skinwidth );
			tex_coords.push_back( (GLfloat)mdl.texcoords[mdl.triangles[i].st[j]].t / mdl.header.skinheight );

			// get current vertex
			pvert = &pframe->verts[mdl.triangles[i].vertex[j]];

			// Get normals 
			//norm = anorms_table[pvert->normalIndex][0];
			norms.push_back(anorms_table[pvert->normalIndex][0]);
			norms.push_back(anorms_table[pvert->normalIndex][1]);
			norms.push_back(anorms_table[pvert->normalIndex][2]);

			// Vertices 
			// Doing these scaling operations *every* refresh is *very* wasteful
			// Should do all the scaling calculations once only, when loading the file
			//verts.push_back( pframe->scale[0] * pvert->v[0] + pframe->translate[0] );
			//verts.push_back( pframe->scale[1] * pvert->v[1] + pframe->translate[1] );
			//verts.push_back( pframe->scale[2] * pvert->v[2] + pframe->translate[2] );
			
		}
	}
	// now repeat for each frame...
	int k = 0;
	GLfloat *verts;
	vertDataSize = mdl.header.num_tris * 9;
	for (k=0;k<mdl.header.num_frames;++k) {
		verts = new GLfloat[vertDataSize];
		pframe = &mdl.frames[k]; // first frame
		for (i = 0; i < mdl.header.num_tris; ++i)
		{
			// For each vertex 
			for (j = 0; j < 3; ++j)
			{
				// get current vertex
				pvert = &pframe->verts[mdl.triangles[i].vertex[j]];
				verts[(i*3 + j)*3] = GLfloat(pframe->scale[0] * pvert->v[0] + pframe->translate[0]);
				verts[(i*3 + j)*3+1] = GLfloat(pframe->scale[1] * pvert->v[1] + pframe->translate[1]);
				verts[(i*3 + j)*3+2] = GLfloat(pframe->scale[2] * pvert->v[2] + pframe->translate[2]);
			}
		}
		vertData.push_back(verts);
	}

	// initialise animVerts with frame 0 data
	animVerts = new GLfloat[vertDataSize];
	memcpy(animVerts,vertData[0],vertDataSize*sizeof(float));


	GLuint VAO;
	VAO = rt3d::createMesh(mdl.header.num_tris * 3,vertData[0],nullptr,norms.data(),tex_coords.data());
	
	// actually have all the data we need, so call FreeModel
	this->FreeModel();

	return VAO;
}


/**
* Free resources allocated for the model.
*/
void md2model::FreeModel ()
{
	int i;

	if (mdl.skins)
	{
		free (mdl.skins);
		mdl.skins = NULL;
	}

	if (mdl.texcoords)
	{
		free (mdl.texcoords);
		mdl.texcoords = NULL;
	}

	if (mdl.triangles)
	{
		free (mdl.triangles);
		mdl.triangles = NULL;
	}

	if (mdl.glcmds)
	{
		free (mdl.glcmds);
		mdl.glcmds = NULL;
	}

	if (mdl.frames)
	{
		for (i = 0; i < mdl.header.num_frames; ++i)
		{
			free (mdl.frames[i].verts);
			mdl.frames[i].verts = NULL;
		}

		free (mdl.frames);
		mdl.frames = NULL;
	}
}



/**
* Calculate the current frame in animation, based on
* selected animation and current frame. If current frame
* is not in selected animation sequence, new current frame
* is start of animation sequence. 
* If interpolation is past 1.0, move current frame to next
* and next frame = current frame + 1
*/

void md2model::Animate (int animation, float dt)
{
	int start = animFrameList[animation * 2];
	int end =  animFrameList[animation * 2 + 1];
	if ((currentFrame < start) || (currentFrame > end))
	{
		currentFrame = start;
		nextFrame = start + 1;
	}
	interp += dt;
	if (interp >= 1.0f)
	{

		// Move to next frame 
		interp = 0.0f;
		currentFrame = nextFrame;
		nextFrame++;

		if (nextFrame >= end+1)
			nextFrame = start;
	}
	if (interp == 0.0f)
		memcpy(animVerts,vertData[currentFrame],vertDataSize*sizeof(float));
	else {
		for (int i=0;i<vertDataSize;i++)
			animVerts[i] = vertData[currentFrame][i] + interp*(vertData[nextFrame][i] - vertData[currentFrame][i]);
	}
}

