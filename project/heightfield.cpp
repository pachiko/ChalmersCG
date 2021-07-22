
#include "heightfield.h"

#include <iostream>
#include <stdint.h>
#include <vector>
#include <glm/glm.hpp>
#include <stb_image.h>

using namespace glm;
using std::string;

HeightField::HeightField(void)
    : m_meshResolution(0)
    , m_vao(UINT32_MAX)
    , m_positionBuffer(UINT32_MAX)
    , m_uvBuffer(UINT32_MAX)
    , m_indexBuffer(UINT32_MAX)
    , m_numIndices(0)
    , m_texid_hf(UINT32_MAX)
    , m_texid_diffuse(UINT32_MAX)
    , m_heightFieldPath("")
    , m_diffuseTexturePath("")
{
}

void HeightField::loadHeightField(const std::string& heigtFieldPath)
{
	int width, height, components;
	stbi_set_flip_vertically_on_load(true);
	float* data = stbi_loadf(heigtFieldPath.c_str(), &width, &height, &components, 1);
	if(data == nullptr)
	{
		std::cout << "Failed to load image: " << heigtFieldPath << ".\n";
		return;
	}

	if(m_texid_hf == UINT32_MAX)
	{
		glGenTextures(1, &m_texid_hf);
	}
	glBindTexture(GL_TEXTURE_2D, m_texid_hf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT,
	             data); // just one component (float)

	m_heightFieldPath = heigtFieldPath;
	std::cout << "Successfully loaded heigh field texture: " << heigtFieldPath << ".\n";
}

void HeightField::loadDiffuseTexture(const std::string& diffusePath)
{
	int width, height, components;
	stbi_set_flip_vertically_on_load(true);
	uint8_t* data = stbi_load(diffusePath.c_str(), &width, &height, &components, 3);
	if(data == nullptr)
	{
		std::cout << "Failed to load image: " << diffusePath << ".\n";
		return;
	}

	if(m_texid_diffuse == UINT32_MAX)
	{
		glGenTextures(1, &m_texid_diffuse);
	}

	glBindTexture(GL_TEXTURE_2D, m_texid_diffuse);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data); // plain RGB
	glGenerateMipmap(GL_TEXTURE_2D);

	std::cout << "Successfully loaded diffuse texture: " << diffusePath << ".\n";
}


void HeightField::generateMesh(int tesselation)
{
	// generate a mesh in range -1 to 1 in x and z
	// (y is 0 but will be altered in height field vertex shader)
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	m_meshResolution = tesselation;
	float spacing = 1.f / (tesselation / 2.f);
	int numVerts1D = (tesselation + 1);
	int numVerts = numVerts1D * numVerts1D;

	glGenBuffers(1, &m_positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);

	std::vector<float> coords; // vector on the stack, but elements on the heap
	for (float z = 1.f; z >= -1.f; z -= spacing) { // camera looks towards -z. OpenGL uses right-handed system
		for (float x = -1.f; x <= 1.f; x += spacing) {	
			coords.push_back(x);
			coords.push_back(0.f);
			coords.push_back(z);
		}
	}

	// sizeof(T*) = # of bytes of executeable (64 bit). fixed for all pointers
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 3 * numVerts, coords.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &m_uvBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_uvBuffer);
	float uvSpacing = spacing / 2.f;
	
	std::vector<float> uvs; // vector on the stack, but elements on the heap
	for (float v = 1.f; v >= 0.f; v -= uvSpacing) { // camera looks towards -z. OpenGL uses right-handed system
		for (float u = 0.f; u <= 1.f; u += uvSpacing) {
			uvs.push_back(u);
			uvs.push_back(v);
		}
	}

	glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 2 * numVerts, uvs.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	glEnableVertexAttribArray(2);


	// IMPORTANT for submitTriangles
	m_numIndices = 2*tesselation*tesselation*3;

	int* indices = new int[m_numIndices];
	for (int i = 0; i < tesselation; i++) {
		for (int j = 0; j < tesselation; j++) { // Draw per quad
			int row = i * (tesselation + 1) + j;
			int upperRow = (i + 1) * (tesselation + 1) + j;
			int idx = 6*(i * tesselation + j);

			indices[idx] = row;
			indices[idx + 1] = row + 1;
			indices[idx + 2] = upperRow;

			indices[idx + 3] = upperRow;
			indices[idx + 4] = row + 1;
			indices[idx + 5] = upperRow + 1;
		}
	}

	glGenBuffers(1, &m_indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)* m_numIndices, indices, GL_STATIC_DRAW);

	delete[] indices;
}

void HeightField::submitTriangles(void)
{
	if(m_vao == UINT32_MAX)
	{
		std::cout << "No vertex array is generated, cannot draw anything.\n";
		return;
	}

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, m_numIndices, GL_UNSIGNED_INT, 0); // already defined indices earlier
}