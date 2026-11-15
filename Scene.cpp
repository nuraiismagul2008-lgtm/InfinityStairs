// SCP-087 "Infinite Stairs" - Final Project
// A real-time first-person demo of an endless dark stairwell. The player has only a flashlight.
// Modern OpenGL 3.3 Core, GLFW + glad.


#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_easy_font.h"   // simple font for the text on the screen
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>
#ifdef _MSC_VER
#pragma comment(lib, "Winmm.lib")
#endif
#endif

const float PI = 3.14159265f;

// ---------------------------------------------------------------------------------------------
// Building constants. One "floor" is the module that is repeated again and again:
// north landing -> flight 1 (west side, goes south) -> south landing -> flight 2 (east side, goes north)
// The top of the north landing is y = 0, every next floor is FLOOR_HEIGHT lower
// ---------------------------------------------------------------------------------------------
const int LOOP_FLOORS = 6;               // after this many floors the world repeats and the player is moved back
const int STEPS_PER_FLIGHT = 12;         // 12 treads + the edge of the landing = 13 steps in every flight
const float STEP_DEPTH = 0.28f;
const float STEP_RISE = 0.19f;
const float STEP_THICKNESS = 0.5f;
const float FLIGHT_RUN = STEPS_PER_FLIGHT * STEP_DEPTH;
const float HALF_RUN = FLIGHT_RUN * 0.5f;
const float FLOOR_HEIGHT = 2.0f * (STEPS_PER_FLIGHT + 1) * STEP_RISE;
const float LANDING_DEPTH = 1.6f;
const float HALF_X = 1.9f;               // the inner half width of the stairwell
const float HALF_Z = HALF_RUN + LANDING_DEPTH;
const float WELL_HALF = 0.3f;            // half width of the open well between the two flights
const float WALL_THICKNESS = 0.3f;
const float LAMP_HEIGHT = 2.3f;          // height of the lamp above the south landing
// The wall texture repeats exactly 4 times on one floor, so there is no seam between two floors
const float TEXTURE_TILE = FLOOR_HEIGHT / 4.0f;

// How many floors around the player are drawn. The farthest ones are hidden by the fog and the darkness
const int FLOORS_ABOVE = 2;
const int FLOORS_BELOW = 3;
const int MAX_LAMPS = 4;

// Flashlight: it is held in the right hand, a bit lower than the eyes, so the shadows can be seen
const float FLASH_OFFSET_X = 0.22f;
const float FLASH_OFFSET_Y = -0.20f;
const float FLASH_OUTER_DEG = 26.0f;
const float FLASH_INNER_RATIO = 0.54f;   // the inner cone is this part of the outer cone
const float FLASH_FOLLOW_SPEED = 14.0f;  // how fast the hand follows the head (bigger = faster)
const float FLASH_COLOR[3] = { 1.0f, 0.94f, 0.80f };

const int TEXTURE_SIZE = 256;

// Dust in the air (particle system)
const int DUST_COUNT = 900;
const float DUST_RANGE_Y = 3.5f;         // the dust lives this far above and below the player

// Player
const float EYE_HEIGHT = 1.65f;
const float PLAYER_RADIUS = 0.25f;
const float STEP_UP = 0.55f;
const float WALK_SPEED = 2.2f;
const float RUN_SPEED = 3.6f;
const float MOUSE_SENSITIVITY = 0.1f;

//3D vector and the small functions that we need for it
struct Vec3 { float x, y, z; };

Vec3 makeVec(float x, float y, float z)
{
	Vec3 result = { x, y, z };
	return result;
}

Vec3 add(Vec3 a, Vec3 b)
{
	return makeVec(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 subtract(Vec3 a, Vec3 b)
{
	return makeVec(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 scale(Vec3 a, float s)
{
	return makeVec(a.x * s, a.y * s, a.z * s);
}

float dot(Vec3 a, Vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b)
{
	return makeVec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float length(Vec3 a)
{
	return sqrtf(dot(a, a));
}

Vec3 normalize(Vec3 a)
{
	// A vector without length stays (0, 0, 0), so we never divide by zero
	float l = length(a);
	if (l < 0.000001f)
	{
		return makeVec(0.0f, 0.0f, 0.0f);
	}
	return scale(a, 1.0f / l);
}

float clampValue(float value, float minValue, float maxValue)
{
	if (value < minValue) return minValue;
	if (value > maxValue) return maxValue;
	return value;
}

float randomFloat()
{
	// A random number from 0 to 1
	return (float)rand() / (float)RAND_MAX;
}

float randomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * randomFloat();
}

// Matrices are arrays of 16 floats, column by column, so OpenGL can use them directly
void identityMatrix(float* m)
{
	for (int i = 0; i < 16; i++)
	{
		m[i] = 0.0f;
	}
	m[0] = 1.0f;
	m[5] = 1.0f;
	m[10] = 1.0f;
	m[15] = 1.0f;
}

void multiplyMatrices(const float* a, const float* b, float* result)
{
	// result = a * b. A temporary array is used, so the result can be the same array as a or b
	float temp[16];
	for (int column = 0; column < 4; column++)
	{
		for (int row = 0; row < 4; row++)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; k++)
			{
				sum += a[k * 4 + row] * b[column * 4 + k];
			}
			temp[column * 4 + row] = sum;
		}
	}
	for (int i = 0; i < 16; i++)
	{
		result[i] = temp[i];
	}
}

void translationMatrix(Vec3 t, float* m)
{
	identityMatrix(m);
	m[12] = t.x;
	m[13] = t.y;
	m[14] = t.z;
}

void lookAt(Vec3 eye, Vec3 target, Vec3 up, float* m)
{
	// The camera looks from eye to target. f is forward, s is right, u is up
	Vec3 f = normalize(subtract(target, eye));
	Vec3 s = normalize(cross(f, up));
	Vec3 u = cross(s, f);
	for (int i = 0; i < 16; i++)
	{
		m[i] = 0.0f;
	}
	m[0] = s.x;  m[4] = s.y;  m[8] = s.z;
	m[1] = u.x;  m[5] = u.y;  m[9] = u.z;
	m[2] = -f.x; m[6] = -f.y; m[10] = -f.z;
	m[12] = -dot(s, eye);
	m[13] = -dot(u, eye);
	m[14] = dot(f, eye);
	m[15] = 1.0f;
}

void perspectiveMatrix(float fovDegrees, float aspect, float nearPlane, float farPlane, float* m)
{
	float f = 1.0f / tanf(fovDegrees * PI / 360.0f);
	for (int i = 0; i < 16; i++)
	{
		m[i] = 0.0f;
	}
	m[0] = f / aspect;
	m[5] = f;
	m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
	m[11] = -1.0f;
	m[14] = 2.0f * farPlane * nearPlane / (nearPlane - farPlane);
}

Vec3 transformPoint(const float* m, Vec3 p)
{
	// Multiply the point (x, y, z, 1) by the matrix
	return makeVec(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
		m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
		m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
}

Vec3 transformDirection(const float* m, Vec3 d)
{
	// A direction is the vector (x, y, z, 0), so the translation of the matrix is not used
	return makeVec(m[0] * d.x + m[4] * d.y + m[8] * d.z,
		m[1] * d.x + m[5] * d.y + m[9] * d.z,
		m[2] * d.x + m[6] * d.y + m[10] * d.z);
}

// ---------------------------------------------------------------------------------------------
// The model of one floor. Every vertex has position (3), normal (3), texture coordinates (2) and material (1)
// Materials: 0 = concrete, 1 = metal, 2 = lamp (it shines by itself)
// ---------------------------------------------------------------------------------------------
std::vector<float> meshData;
int vertexCount = 0;

void addFace(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal, float material)
{
	// A face is a quad, it is cut into two triangles: a b c and a c d (counter-clockwise from the outside)
	Vec3 corners[6] = { a, b, c, a, c, d };
	for (int i = 0; i < 6; i++)
	{
		Vec3 p = corners[i];
		// The texture coordinates are the world coordinates of the plane of the face, divided by the tile size
		float u, v;
		if (fabsf(normal.x) > 0.5f)
		{
			u = p.z;
			v = p.y;
		}
		else if (fabsf(normal.y) > 0.5f)
		{
			u = p.x;
			v = p.z;
		}
		else
		{
			u = p.x;
			v = p.y;
		}
		meshData.push_back(p.x);
		meshData.push_back(p.y);
		meshData.push_back(p.z);
		meshData.push_back(normal.x);
		meshData.push_back(normal.y);
		meshData.push_back(normal.z);
		meshData.push_back(u / TEXTURE_TILE);
		meshData.push_back(v / TEXTURE_TILE);
		meshData.push_back(material);
		vertexCount++;
	}
}

void addBox(float x0, float y0, float z0, float x1, float y1, float z1, float material)
{
	// A box from the minimum corner (x0, y0, z0) to the maximum corner (x1, y1, z1), 6 faces with flat normals
	addFace(makeVec(x1, y0, z1), makeVec(x1, y0, z0), makeVec(x1, y1, z0), makeVec(x1, y1, z1), makeVec(1, 0, 0), material);
	addFace(makeVec(x0, y0, z0), makeVec(x0, y0, z1), makeVec(x0, y1, z1), makeVec(x0, y1, z0), makeVec(-1, 0, 0), material);
	addFace(makeVec(x0, y1, z1), makeVec(x1, y1, z1), makeVec(x1, y1, z0), makeVec(x0, y1, z0), makeVec(0, 1, 0), material);
	addFace(makeVec(x0, y0, z0), makeVec(x1, y0, z0), makeVec(x1, y0, z1), makeVec(x0, y0, z1), makeVec(0, -1, 0), material);
	addFace(makeVec(x0, y0, z1), makeVec(x1, y0, z1), makeVec(x1, y1, z1), makeVec(x0, y1, z1), makeVec(0, 0, 1), material);
	addFace(makeVec(x1, y0, z0), makeVec(x0, y0, z0), makeVec(x0, y1, z0), makeVec(x1, y1, z0), makeVec(0, 0, -1), material);
}

Vec3 lampWorldPosition(int floorIndex)
{
	// The position of the light of a lamp. Floor number j has its top at y = -j * FLOOR_HEIGHT
	return makeVec(0.0f, -floorIndex * FLOOR_HEIGHT - FLOOR_HEIGHT * 0.5f + LAMP_HEIGHT, HALF_Z - 0.35f);
}

void buildFloor()
{
	float halfHeight = FLOOR_HEIGHT * 0.5f;

	// Two landings (slabs)
	addBox(-HALF_X, -0.3f, -HALF_Z, HALF_X, 0.0f, -HALF_RUN, 0.0f);
	addBox(-HALF_X, -halfHeight - 0.3f, HALF_RUN, HALF_X, -halfHeight, HALF_Z, 0.0f);

	// Flight 1: west side, from the north landing (y = 0) down to the south landing
	// Flight 2: east side, from the south landing (y = -FLOOR_HEIGHT / 2) down to the north landing of the next floor
	for (int i = 0; i < STEPS_PER_FLIGHT; i++)
	{
		float top1 = -(i + 1) * STEP_RISE;
		float z1 = -HALF_RUN + i * STEP_DEPTH;
		addBox(-HALF_X, top1 - STEP_THICKNESS, z1, -WELL_HALF, top1, z1 + STEP_DEPTH, 0.0f);

		float top2 = -halfHeight - (i + 1) * STEP_RISE;
		float z2 = HALF_RUN - (i + 1) * STEP_DEPTH;
		addBox(WELL_HALF, top2 - STEP_THICKNESS, z2, HALF_X, top2, z2 + STEP_DEPTH, 0.0f);

		// Railings along the open well: a thin post on every step and a piece of the handrail between the posts
		float postCenter1 = z1 + STEP_DEPTH * 0.5f;
		addBox(-WELL_HALF - 0.05f, top1, postCenter1 - 0.025f, -WELL_HALF, top1 + 0.95f, postCenter1 + 0.025f, 1.0f);
		addBox(-WELL_HALF - 0.07f, top1 + 0.92f, z1, -WELL_HALF + 0.02f, top1 + 0.98f, z1 + STEP_DEPTH, 1.0f);

		float postCenter2 = z2 + STEP_DEPTH * 0.5f;
		addBox(WELL_HALF, top2, postCenter2 - 0.025f, WELL_HALF + 0.05f, top2 + 0.95f, postCenter2 + 0.025f, 1.0f);
		addBox(WELL_HALF - 0.02f, top2 + 0.92f, z2, WELL_HALF + 0.07f, top2 + 0.98f, z2 + STEP_DEPTH, 1.0f);
	}

	// The four walls of the stairwell, as tall as one floor
	addBox(-HALF_X - WALL_THICKNESS, -FLOOR_HEIGHT, -HALF_Z - WALL_THICKNESS, -HALF_X, 0.0f, HALF_Z + WALL_THICKNESS, 0.0f);
	addBox(HALF_X, -FLOOR_HEIGHT, -HALF_Z - WALL_THICKNESS, HALF_X + WALL_THICKNESS, 0.0f, HALF_Z + WALL_THICKNESS, 0.0f);
	addBox(-HALF_X, -FLOOR_HEIGHT, -HALF_Z - WALL_THICKNESS, HALF_X, 0.0f, -HALF_Z, 0.0f);
	addBox(-HALF_X, -FLOOR_HEIGHT, HALF_Z, HALF_X, 0.0f, HALF_Z + WALL_THICKNESS, 0.0f);

	// The body of the lamp on the south wall (the light itself is calculated in the shader)
	Vec3 lamp = lampWorldPosition(0);
	addBox(-0.2f, lamp.y - 0.08f, HALF_Z - 0.12f, 0.2f, lamp.y + 0.08f, HALF_Z, 2.0f);
}

// ---------------------------------------------------------------------------------------------
// Procedural textures. A value noise with a period is used, so the textures can be repeated without seams
// ---------------------------------------------------------------------------------------------
float hashLattice(int x, int y, int seed)
{
	// The same input always gives the same number from 0 to 1
	unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + (unsigned int)seed * 1274126177u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h = h ^ (h >> 16);
	return (float)(h & 0xFFFFFF) / 16777215.0f;
}

float smoothStep(float edge0, float edge1, float x)
{
	float t = clampValue((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

float valueNoise(float x, float y, int periodX, int periodY, int seed)
{
	// The noise is made from random values in the lattice points, the values between them are interpolated
	int xi = (int)floorf(x);
	int yi = (int)floorf(y);
	float fx = x - xi;
	float fy = y - yi;
	fx = fx * fx * (3.0f - 2.0f * fx);
	fy = fy * fy * (3.0f - 2.0f * fy);
	int x0 = ((xi % periodX) + periodX) % periodX;
	int y0 = ((yi % periodY) + periodY) % periodY;
	int x1 = (x0 + 1) % periodX;
	int y1 = (y0 + 1) % periodY;
	float a = hashLattice(x0, y0, seed);
	float b = hashLattice(x1, y0, seed);
	float c = hashLattice(x0, y1, seed);
	float d = hashLattice(x1, y1, seed);
	float top = a + (b - a) * fx;
	float bottom = c + (d - c) * fx;
	return top + (bottom - top) * fy;
}

float fractalNoise(float u, float v, int periodX, int periodY, int octaves, int seed)
{
	// Several noises with the double frequency and the half strength are added (fBm), u and v are from 0 to 1
	float sum = 0.0f;
	float amplitude = 0.5f;
	float total = 0.0f;
	for (int o = 0; o < octaves; o++)
	{
		sum += amplitude * valueNoise(u * periodX, v * periodY, periodX, periodY, seed + o);
		total += amplitude;
		periodX *= 2;
		periodY *= 2;
		amplitude *= 0.5f;
	}
	return sum / total;
}

unsigned char toByte(float value)
{
	return (unsigned char)(clampValue(value, 0.0f, 1.0f) * 255.0f);
}

void makeConcreteTexture(std::vector<unsigned char>& pixels)
{
	// Gray concrete: soft patches, fine grain and thin dark cracks
	pixels.resize(TEXTURE_SIZE * TEXTURE_SIZE * 3);
	for (int y = 0; y < TEXTURE_SIZE; y++)
	{
		for (int x = 0; x < TEXTURE_SIZE; x++)
		{
			float u = (float)x / TEXTURE_SIZE;
			float v = (float)y / TEXTURE_SIZE;
			float patches = fractalNoise(u, v, 4, 4, 4, 1);
			float grain = fractalNoise(u, v, 32, 32, 3, 2);
			float crackNoise = fractalNoise(u, v, 6, 6, 4, 3);
			float crack = 1.0f - smoothStep(0.0f, 0.025f, fabsf(crackNoise - 0.5f));
			float value = 0.38f + 0.30f * (patches - 0.5f) + 0.18f * (grain - 0.5f) - 0.22f * crack;
			int index = (y * TEXTURE_SIZE + x) * 3;
			pixels[index] = toByte(value * 1.00f);
			pixels[index + 1] = toByte(value * 0.99f);
			pixels[index + 2] = toByte(value * 0.95f);
		}
	}
}

void makeMetalTexture(std::vector<unsigned char>& pixels)
{
	// Dark painted metal: long horizontal scratches and brown spots of rust
	pixels.resize(TEXTURE_SIZE * TEXTURE_SIZE * 3);
	for (int y = 0; y < TEXTURE_SIZE; y++)
	{
		for (int x = 0; x < TEXTURE_SIZE; x++)
		{
			float u = (float)x / TEXTURE_SIZE;
			float v = (float)y / TEXTURE_SIZE;
			float scratches = fractalNoise(u, v, 2, 64, 3, 7);
			float rustNoise = fractalNoise(u, v, 5, 5, 4, 9);
			float rust = smoothStep(0.52f, 0.72f, rustNoise);
			float value = 0.20f + 0.16f * (scratches - 0.5f);
			int index = (y * TEXTURE_SIZE + x) * 3;
			pixels[index] = toByte(value * (1.0f - rust) + 0.42f * rust);
			pixels[index + 1] = toByte(value * (1.0f - rust) + 0.20f * rust);
			pixels[index + 2] = toByte(value * (1.0f - rust) + 0.08f * rust);
		}
	}
}

GLuint createTexture(const std::vector<unsigned char>& pixels)
{
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	// Mipmaps and repeating, so the far walls do not flicker
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return texture;
}

GLuint loadPNGTexture(const std::string& path)
{
	int width, height, channels;

	stbi_set_flip_vertically_on_load(true);

	unsigned char* image = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		std::cout << "Failed to load texture: " << path << std::endl;
		return 0;
	}

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(image);

	return texture;
}

// ---------------------------------------------------------------------------------------------
// Statistics of the frame (efficiency): the number of draw calls and triangles that were sent to the GPU
// ---------------------------------------------------------------------------------------------
int drawCalls = 0;
int drawnTriangles = 0;

// ---------------------------------------------------------------------------------------------
// LIGHTING AND SHADOWS - settings that can be changed while the program works
// ---------------------------------------------------------------------------------------------
struct LightingSettings
{
	// Flashlight (spotlight)
	bool flashlightOn;
	float intensity;          // brightness of the flashlight
	float outerDeg;           // angle of the outer cone (the light goes to 0 here)
	float spotExponent;       // the shape of the edge of the cone: 1 = linear, bigger = more narrow
	// Shadow mapping
	bool shadowsOn;
	int shadowSize;           // the depth texture is shadowSize x shadowSize
	int pcfRadius;            // 0 = hard shadow (1 lookup), 1 = 3 x 3 lookups, 2 = 5 x 5 lookups
	float shadowBias;         // constant part of the depth bias
	// Fog and visibility
	bool fogOn;
	int fogMode;              // 0 = linear, 1 = exponential, 2 = Gaussian
	bool fogScatter;          // the fog shines where the flashlight lights it
	// Other
	bool dustOn;
	bool multisample;
	int debugView;            // 0 = final image, 1 = shadows, 2 = light, 3 = fog
};

LightingSettings lighting;

const int SHADOW_SIZES[4] = { 512, 1024, 2048, 4096 };
const char* FOG_NAMES[3] = { "Linear", "Exp", "Gauss" };
const char* VIEW_NAMES[4] = { "Final", "Shadow", "Light", "Fog" };

void resetLighting()
{
	lighting.flashlightOn = true;
	lighting.intensity = 2.6f;
	lighting.outerDeg = FLASH_OUTER_DEG;
	lighting.spotExponent = 1.5f;
	lighting.shadowsOn = true;
	lighting.shadowSize = 2048;
	lighting.pcfRadius = 1;
	lighting.shadowBias = 0.0003f;
	lighting.fogOn = true;
	lighting.fogMode = 2;
	lighting.fogScatter = true;
	lighting.dustOn = true;
	lighting.multisample = true;
	lighting.debugView = 0;
}

// The atmosphere depends on the depth: the deeper the player, the thicker and colder the fog
struct Atmosphere
{
	float depthFactor;        // 0 = the top of the stairwell, 1 = very deep
	float fogDensity;
	float fogColor[3];
	float coldLightFactor;
};

Atmosphere computeAtmosphere(float depth)
{
	Atmosphere a;
	a.depthFactor = clampValue((depth - 20.0f) / 20.0f, 0.0f, 1.0f);
	a.fogDensity = 0.20f + 0.15f * a.depthFactor;
	a.fogColor[0] = 0.004f + 0.015f * a.depthFactor;
	a.fogColor[1] = 0.005f + 0.025f * a.depthFactor;
	a.fogColor[2] = 0.007f + 0.055f * a.depthFactor;
	a.coldLightFactor = clampValue((depth - 35.0f) / 15.0f, 0.0f, 1.0f);
	return a;
}

// Everything that the shaders need to know about the light in the current frame
struct FrameLight
{
	float lightVP[16];             // flashlight view and projection (world -> light clip space)
	Vec3 flashWorldPosition;
	Vec3 flashEyePosition;         // the same point and the direction in the camera (eye) coordinates
	Vec3 flashEyeDirection;
	float flashIntensity;
	float attenuation[3];          // a, b, c of the distance term 1 / (a + b * d + c * d * d)
	float shadowTexelScale;        // size of one shadow map texel at the distance 1 from the flashlight
	float fogDensity;
	float fogColor[3];
};

// ---------------------------------------------------------------------------------------------
// Shaders. All the lighting is calculated per fragment (Phong shading) in the camera (eye) coordinates
// ---------------------------------------------------------------------------------------------

// This code is used by the fragment shader of the scene and by the fragment shader of the dust:
// the spotlight, the shadow test and the fog are written only once
// GLSL code of this shader: shaders/lighting_shared.glsl

// Vertex Shader of the scene: model -> view -> projection, and the position in the light space for the shadow
// GLSL code of this shader: shaders/scene.vert

// Fragment Shader of the scene
// GLSL code of this shader: shaders/scene.frag

// The shadow pass only needs the depth, so its shaders are very small
// GLSL code of this shader: shaders/shadow.vert

// GLSL code of this shader: shaders/shadow.frag

// Dust: every particle is a point. It is visible only where the flashlight lights it (and the shadows are used too)
// GLSL code of this shader: shaders/dust.vert

// GLSL code of this shader: shaders/dust.frag

// The billboard of the face (it is drawn without the lighting)
// GLSL code of this shader: shaders/face.vert

// GLSL code of this shader: shaders/face.frag

// Reads a whole text file (a shader) into a string. The path is relative to the working directory.
// If the file cannot be opened, the error is printed to the console and the string is empty
std::string readTextFile(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		std::cout << "Failed to open shader file: " << path << std::endl;
		return "";
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

GLuint compileShader(GLenum type, const std::string& code)
{
	// The shader is the version line + the code of this stage
	std::string source = std::string("#version 330 core\n") + code;
	const char* sourcePointer = source.c_str();
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &sourcePointer, NULL);
	glCompileShader(shader);

	// Print the error if the shader does not compile
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char log[1024];
		glGetShaderInfoLog(shader, 1024, NULL, log);
		std::cout << "Shader error: " << log << std::endl;
	}
	return shader;
}

GLuint createShaderProgram(const std::string& vertexCode, const std::string& fragmentCode)
{
	// Compile both shaders and link them into one Shader Program
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexCode);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentCode);
	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char log[1024];
		glGetProgramInfoLog(program, 1024, NULL, log);
		std::cout << "Program error: " << log << std::endl;
	}

	// The shader objects are not needed after linking
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	return program;
}

// ---------------------------------------------------------------------------------------------
// Shadow map: a depth texture that is attached to a frame buffer object (FBO), the scene is rendered into it
// ---------------------------------------------------------------------------------------------
GLuint shadowFramebuffer = 0;
GLuint shadowTexture = 0;

void allocateShadowTexture(int size)
{
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
}

void createShadowMap()
{
	glGenTextures(1, &shadowTexture);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	allocateShadowTexture(lighting.shadowSize);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// Outside of the map everything is lit (the depth 1.0 is the far plane)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
	// The texture compares the depth itself, so the shader gets 0 (shadow) or 1 (light)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glGenFramebuffers(1, &shadowFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
	// There is no color buffer in this frame buffer
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "The shadow frame buffer is not complete" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void setShadowMapSize(int size)
{
	// The texture gets a new size, the frame buffer object keeps using the same texture
	lighting.shadowSize = size;
	allocateShadowTexture(size);
}

GLint uniformLocation(GLuint program, const char* name)
{
	return glGetUniformLocation(program, name);
}

// Sends the light, the shadow and the fog to a shader program (it is used for the scene and for the dust)
void uploadLightingUniforms(GLuint program, const FrameLight& frame, bool shadowActive)
{
	glUniformMatrix4fv(uniformLocation(program, "uLightVP"), 1, GL_FALSE, frame.lightVP);
	glUniform3f(uniformLocation(program, "uLightWorldPosition"), frame.flashWorldPosition.x, frame.flashWorldPosition.y, frame.flashWorldPosition.z);

	// Flashlight. The cones are sent as cosines, so the shader does not need the angles
	float inner = lighting.outerDeg * FLASH_INNER_RATIO;
	glUniform1i(uniformLocation(program, "uFlashlightOn"), lighting.flashlightOn ? 1 : 0);
	glUniform3f(uniformLocation(program, "uFlashPosition"), frame.flashEyePosition.x, frame.flashEyePosition.y, frame.flashEyePosition.z);
	glUniform3f(uniformLocation(program, "uFlashDirection"), frame.flashEyeDirection.x, frame.flashEyeDirection.y, frame.flashEyeDirection.z);
	glUniform1f(uniformLocation(program, "uFlashInner"), cosf(inner * PI / 180.0f));
	glUniform1f(uniformLocation(program, "uFlashOuter"), cosf(lighting.outerDeg * PI / 180.0f));
	glUniform1f(uniformLocation(program, "uFlashIntensity"), frame.flashIntensity);
	glUniform1f(uniformLocation(program, "uSpotExponent"), lighting.spotExponent);
	glUniform3f(uniformLocation(program, "uFlashAttenuation"), frame.attenuation[0], frame.attenuation[1], frame.attenuation[2]);
	glUniform3fv(uniformLocation(program, "uFlashColor"), 1, FLASH_COLOR);

	// Shadow map is on the texture unit 2. The normal offset is 1 texel + the radius of the PCF
	glUniform1i(uniformLocation(program, "uShadowMap"), 2);
	glUniform1i(uniformLocation(program, "uUseShadow"), shadowActive ? 1 : 0);
	glUniform1i(uniformLocation(program, "uPcfRadius"), lighting.pcfRadius);
	glUniform1f(uniformLocation(program, "uShadowBias"), lighting.shadowBias);
	glUniform1f(uniformLocation(program, "uNormalOffset"), 1.0f + (float)lighting.pcfRadius);
	glUniform1f(uniformLocation(program, "uShadowTexelScale"), frame.shadowTexelScale);

	// Fog
	glUniform1i(uniformLocation(program, "uUseFog"), lighting.fogOn ? 1 : 0);
	glUniform1i(uniformLocation(program, "uFogMode"), lighting.fogMode);
	glUniform1f(uniformLocation(program, "uFogDensity"), frame.fogDensity);
	glUniform3fv(uniformLocation(program, "uFogColor"), 1, frame.fogColor);
}

// ---------------------------------------------------------------------------------------------
// Dust. A particle system: every particle is born with random attributes, moves by the forces (a small
// random force, the air drag and a slow fall), and dies when it is too old or leaves the stairwell
// ---------------------------------------------------------------------------------------------
struct Dust
{
	Vec3 position;
	Vec3 velocity;
	float size;      // in meters
	float age;
	float life;
};

Dust dust[DUST_COUNT];
float dustData[DUST_COUNT * 5];   // for every particle: position (3), size and alpha (2)

void spawnDust(Dust& d, float centerY, bool randomAge)
{
	d.position = makeVec(randomRange(-HALF_X + 0.1f, HALF_X - 0.1f), centerY + randomRange(-DUST_RANGE_Y, DUST_RANGE_Y), randomRange(-HALF_Z + 0.1f, HALF_Z - 0.1f));
	d.velocity = makeVec(randomRange(-0.03f, 0.03f), randomRange(-0.06f, 0.0f), randomRange(-0.03f, 0.03f));
	d.size = randomRange(0.012f, 0.03f);
	d.life = randomRange(6.0f, 14.0f);
	d.age = randomAge ? randomRange(0.0f, d.life) : 0.0f;
}

void initDust(float centerY)
{
	for (int i = 0; i < DUST_COUNT; i++)
	{
		spawnDust(dust[i], centerY, true);
	}
}

void shiftDust(float dy)
{
	// When the player is moved by the loop, the dust is moved with him
	for (int i = 0; i < DUST_COUNT; i++)
	{
		dust[i].position.y += dy;
	}
}

void updateDust(float seconds, float centerY)
{
	for (int i = 0; i < DUST_COUNT; i++)
	{
		Dust& d = dust[i];
		d.age += seconds;

		// Force: a random push of the air, then the air drag (the velocity gets smaller)
		Vec3 push = makeVec(randomRange(-1.0f, 1.0f), randomRange(-1.0f, 1.0f), randomRange(-1.0f, 1.0f));
		d.velocity = add(d.velocity, scale(push, seconds));
		d.velocity = scale(d.velocity, 1.0f - clampValue(0.8f * seconds, 0.0f, 1.0f));
		d.position = add(d.position, scale(d.velocity, seconds));

		// Extinction: too old, or outside of the stairwell, or too far from the player
		bool outside = fabsf(d.position.x) > HALF_X || fabsf(d.position.z) > HALF_Z || fabsf(d.position.y - centerY) > DUST_RANGE_Y + 0.5f;
		if (d.age > d.life || outside)
		{
			spawnDust(d, centerY, false);
		}

		// The particle fades in and fades out during one second
		float fade = clampValue(fminf(d.age, d.life - d.age), 0.0f, 1.0f);
		dustData[i * 5] = d.position.x;
		dustData[i * 5 + 1] = d.position.y;
		dustData[i * 5 + 2] = d.position.z;
		dustData[i * 5 + 3] = d.size;
		dustData[i * 5 + 4] = fade;
	}
}

// ---------------------------------------------------------------------------------------------
// Light sources with a random flicker. Every light has a timer: it is steady for some seconds,
// then it flickers for a short time (the level jumps between dark and bright), then it is steady again
// ---------------------------------------------------------------------------------------------
struct Lamp
{
	bool exists;
	bool flickering;
	float level;        // 0 = dark, 1 = full light
	float timer;        // seconds until the state changes
	float stepTimer;    // seconds until the next jump inside a flicker
	float steadyMin;
	float steadyMax;
	float color[3];
};

bool flickerEnabled = true;
Lamp lamps[LOOP_FLOORS];
Lamp flashlight;

// The three colors of the lamps: warm, cold and orange
const float LAMP_COLORS[3][3] = { { 1.0f, 0.70f, 0.40f }, { 0.75f, 0.85f, 1.0f }, { 1.0f, 0.55f, 0.30f } };

void initLamp(Lamp& lamp, bool exists, float steadyMin, float steadyMax, const float* color)
{
	lamp.exists = exists;
	lamp.flickering = false;
	lamp.level = exists ? 1.0f : 0.0f;
	lamp.steadyMin = steadyMin;
	lamp.steadyMax = steadyMax;
	lamp.timer = randomRange(steadyMin, steadyMax);
	lamp.stepTimer = 0.0f;
	for (int i = 0; i < 3; i++)
	{
		lamp.color[i] = color[i];
	}
}

void updateLamp(Lamp& lamp, float seconds)
{
	if (!lamp.exists)
	{
		lamp.level = 0.0f;
		return;
	}
	if (!flickerEnabled)
	{
		lamp.level = 1.0f;
		lamp.flickering = false;
		return;
	}
	lamp.timer -= seconds;
	if (lamp.flickering)
	{
		lamp.stepTimer -= seconds;
		if (lamp.stepTimer <= 0.0f)
		{
			// A random jump: often completely dark, otherwise a random brightness
			lamp.level = (randomFloat() < 0.4f) ? 0.0f : randomRange(0.3f, 1.0f);
			lamp.stepTimer = randomRange(0.03f, 0.12f);
		}
		if (lamp.timer <= 0.0f)
		{
			lamp.flickering = false;
			lamp.level = 1.0f;
			lamp.timer = randomRange(lamp.steadyMin, lamp.steadyMax);
		}
	}
	else if (lamp.timer <= 0.0f)
	{
		lamp.flickering = true;
		lamp.timer = randomRange(0.3f, 1.2f);
		lamp.stepTimer = 0.0f;
	}
}

int variantOf(int floorIndex)
{
	// The world repeats every LOOP_FLOORS floors, so the floor j looks like the floor j + LOOP_FLOORS
	return ((floorIndex % LOOP_FLOORS) + LOOP_FLOORS) % LOOP_FLOORS;
}

void initLamps()
{
	// Floors 2 and 5 of the loop have a broken lamp
	for (int i = 0; i < LOOP_FLOORS; i++)
	{
		initLamp(lamps[i], (i % 3) != 2, 2.0f, 7.0f, LAMP_COLORS[i % 3]);
	}
	// The flashlight has its own timer, it flickers rarely
	initLamp(flashlight, true, 8.0f, 20.0f, FLASH_COLOR);
}


// Player, camera and the loop of the floors
GLFWwindow* window;
int windowWidth = 1280;
int windowHeight = 720;

float playerX, playerZ, feetY, cameraY;
float yaw, pitch;
float fieldOfView = 70.0f;
float totalDescent = 0.0f;   // meters the player gone down - doesnt reset with loop
bool depthEventTriggered = false;
float depthEventTimer = 0.0f;
bool redLightEventTriggered = false;
float redLightEventTimer = 0.0f;
// SCP face event
bool faceEventTriggered = false;
float faceEventTimer = 0.0f;
bool faceEventActive = false;
bool horrorSoundTriggered = false;

bool faceJumpScareActive = false;
float faceJumpScareTimer = 0.0f;
bool faceJumpScareTriggered = false;

const float FACE_ATTACK_DURATION = 2.3f;

Vec3 faceAttackStart;
Vec3 faceAttackDirection;

float faceAttackProgress = 0.0f;

const float FACE_DEPTH = 51.0f;
const float FACE_X = 0.0f;
const float FACE_Z = HALF_Z - 0.05f;
int wrapCount = 0;           // times player moved back by the loop
bool useTexture = true;
bool paused = false;         // Space: stops the animation, the mouse still turns the head
bool helpVisible = false;    // F1: the keys on the screen
bool freeFly = false;        // C: free camera, no collisions
const float FLY_SPEED = 4.0f;
const float GRAVITY = 14.0f;       // stronger than in real life, so the jump looks better
const float JUMP_HEIGHT = 0.55f;   // in meters, small, so the head does not hit the stairs above
bool jumpRequested = false;        // J: the key callback sets it, updatePlayer uses it once
bool airborne = false;
float verticalVelocity = 0.0f;
bool firstMouse = true;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

// The direction of the flashlight follows the direction of the head with a small delay
Vec3 flashDirection;

Vec3 viewDirection()
{
	return makeVec(cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw));
}

void resetPlayer()
{
	// The start: floor 0 - looks at the south
	playerX = -1.1f;
	playerZ = -2.4f;
	feetY = 0.0f;
	cameraY = EYE_HEIGHT;
	yaw = PI;
	pitch = -0.15f;
	totalDescent = 0.0f;
	wrapCount = 0;
	flashDirection = viewDirection();
	freeFly = false;
	airborne = false;
	verticalVelocity = 0.0f;
}

bool canStand(float x, float z)
{
	// The player cant go through the walls or into the open well
	if (fabsf(x) > HALF_X - PLAYER_RADIUS || fabsf(z) > HALF_Z - PLAYER_RADIUS)
	{
		return false;
	}
	if (fabsf(x) < WELL_HALF + PLAYER_RADIUS && z > -HALF_RUN && z < HALF_RUN)
	{
		return false;
	}
	return true;
}

// The height of the floor under the point (x, z). The floors repeat every FLOOR_HEIGHT, so the point has many
// possible heights. The highest one that the player can step on is taken
float groundHeight(float x, float z, float currentFeetY)
{
	float h0;
	if (z < -HALF_RUN)
	{
		h0 = 0.0f;
	}
	else if (z >= HALF_RUN)
	{
		h0 = -FLOOR_HEIGHT * 0.5f;
	}
	else if (x < 0.0f)
	{
		int i = (int)floorf((z + HALF_RUN) / STEP_DEPTH);
		i = (int)clampValue((float)i, 0.0f, (float)(STEPS_PER_FLIGHT - 1));
		h0 = -(i + 1) * STEP_RISE;
	}
	else
	{
		int j = (int)floorf((HALF_RUN - z) / STEP_DEPTH);
		j = (int)clampValue((float)j, 0.0f, (float)(STEPS_PER_FLIGHT - 1));
		h0 = -FLOOR_HEIGHT * 0.5f - (j + 1) * STEP_RISE;
	}
	float limit = currentFeetY + STEP_UP;
	float k = ceilf((h0 - limit) / FLOOR_HEIGHT);
	return h0 - k * FLOOR_HEIGHT;
}

void updatePlayer(float seconds)
{
	if (freeFly)
	{
		// Free camera: the player flies where he looks, E goes up and Q goes down, Shift is faster
		// There are no collisions, so we can look at the whole stairwell
		Vec3 look = viewDirection();
		Vec3 side = makeVec(cosf(yaw), 0.0f, sinf(yaw));
		Vec3 fly = makeVec(0.0f, 0.0f, 0.0f);
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) fly = add(fly, look);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) fly = subtract(fly, look);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) fly = add(fly, side);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) fly = subtract(fly, side);
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) fly = add(fly, makeVec(0.0f, 1.0f, 0.0f));
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) fly = subtract(fly, makeVec(0.0f, 1.0f, 0.0f));
		fly = normalize(fly);
		float flySpeed = FLY_SPEED;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		{
			flySpeed = FLY_SPEED * 3.0f;
		}
		playerX += fly.x * flySpeed * seconds;
		playerZ += fly.z * flySpeed * seconds;
		cameraY += fly.y * flySpeed * seconds;

		airborne = false;
		verticalVelocity = 0.0f;
		jumpRequested = false;

		// The feet stay under the camera, so the depth, fog and events still work
		float oldFeetY = feetY;
		feetY = cameraY - EYE_HEIGHT;
		totalDescent += oldFeetY - feetY;
	}
	else
	{
		// The walking directions on the floor, the pitch is ignored
		Vec3 forward = makeVec(sinf(yaw), 0.0f, -cosf(yaw));
		Vec3 right = makeVec(cosf(yaw), 0.0f, sinf(yaw));
		Vec3 wish = makeVec(0.0f, 0.0f, 0.0f);
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wish = add(wish, forward);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wish = subtract(wish, forward);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wish = add(wish, right);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wish = subtract(wish, right);
		wish = normalize(wish);
		float speed = WALK_SPEED;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		{
			speed = RUN_SPEED;
		}

		// X and Z are tried separately, so the player slides along the walls
		float newX = playerX + wish.x * speed * seconds;
		if (canStand(newX, playerZ)) playerX = newX;
		float newZ = playerZ + wish.z * speed * seconds;
		if (canStand(playerX, newZ)) playerZ = newZ;

		// Jump: only from the ground, in the pause it does nothing
		if (jumpRequested && !airborne && seconds > 0.0f)
		{
			airborne = true;
			verticalVelocity = sqrtf(2.0f * GRAVITY * JUMP_HEIGHT);
		}
		jumpRequested = false;

		float oldFeetY = feetY;
		float ground = groundHeight(playerX, playerZ, feetY);
		if (airborne)
		{
			// In the air the gravity moves the feet down, the ground can change when the player moves (next step),
			// so we ask for it again
			verticalVelocity -= GRAVITY * seconds;
			feetY += verticalVelocity * seconds;
			ground = groundHeight(playerX, playerZ, feetY);
			if (feetY <= ground)
			{
				feetY = ground;
				airborne = false;
				verticalVelocity = 0.0f;
			}
		}
		else
		{
			// On the ground the feet follow the steps
			feetY = ground;
		}
		totalDescent += oldFeetY - feetY;

		// The camera follows the feet a bit later, so the walk on the stairs is smooth, in the air it is faster
		float follow = airborne ? 30.0f : 12.0f;
		cameraY += (feetY + EYE_HEIGHT - cameraY) * clampValue(follow * seconds, 0.0f, 1.0f);
	}

	// The loop: the world has the same look every LOOP_FLOORS floors, so the player is moved by exactly
	// this height and nothing changes on the screen
	float loopLength = LOOP_FLOORS * FLOOR_HEIGHT;
	if (feetY < -loopLength)
	{
		feetY += loopLength;
		cameraY += loopLength;
		wrapCount++;
	}
	else if (feetY > 0.0f)
	{
		feetY -= loopLength;
		cameraY -= loopLength;
		wrapCount--;
	}
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------
void updateTitle(float fps)
{
	// The window title is the HUD: floor, depth, FPS, the efficiency numbers and the state of the effects
	int floorNumber = (int)floorf(totalDescent / FLOOR_HEIGHT);
	int kernel = 2 * lighting.pcfRadius + 1;
	char title[600];
	snprintf(title, sizeof(title),
		"Infinite Stairs: SCP-087 (Ismagul, Parunina, Bauyrzhan) | Floor %d | %.1f m | %.0f FPS %.1f ms | %d draws %d tris | Light %s %.0f deg x%.1f | Shadow %s %d px PCF %dx%d | Fog %s %s | Dust %s | MSAA %s | View %s",
		floorNumber, totalDescent, fps, fps > 0.0f ? 1000.0f / fps : 0.0f, drawCalls, drawnTriangles,
		lighting.flashlightOn ? "on" : "off", lighting.outerDeg, lighting.intensity,
		lighting.shadowsOn ? "on" : "off", lighting.shadowSize, kernel, kernel,
		lighting.fogOn ? "on" : "off", FOG_NAMES[lighting.fogMode], lighting.dustOn ? "on" : "off",
		lighting.multisample ? "on" : "off", VIEW_NAMES[lighting.debugView]);
	glfwSetWindowTitle(window, title);
}

// ---------------------------------------------------------------------------------------------
// Text on the screen (HUD and help window). The font gives quads, we draw them as triangles
// ---------------------------------------------------------------------------------------------
// GLSL code of this shader: shaders/text.vert

// GLSL code of this shader: shaders/text.frag

GLuint textProgram = 0, textVAO = 0, textVBO = 0, textEBO = 0;
const int TEXT_MAX_QUADS = 1024;
float uiScale = 2.0f;

void initText()
{
	textProgram = createShaderProgram(readTextFile("shaders/text.vert"), readTextFile("shaders/text.frag"));
	glGenVertexArrays(1, &textVAO);
	glGenBuffers(1, &textVBO);
	glGenBuffers(1, &textEBO);
	glBindVertexArray(textVAO);
	glBindBuffer(GL_ARRAY_BUFFER, textVBO);
	glBufferData(GL_ARRAY_BUFFER, TEXT_MAX_QUADS * 4 * 16, NULL, GL_DYNAMIC_DRAW);
	// One vertex of the font is 16 bytes: x, y, z and 4 bytes of color (we dont use the color)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, (void*)0);
	glEnableVertexAttribArray(0);

	// The core profile has no GL_QUADS, so every quad is two triangles
	std::vector<unsigned int> indices;
	for (int q = 0; q < TEXT_MAX_QUADS; q++)
	{
		unsigned int b = q * 4;
		unsigned int quad[6] = { b, b + 1, b + 2, b, b + 2, b + 3 };
		indices.insert(indices.end(), quad, quad + 6);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);
}

struct TextVertex
{
	float x, y, z;
	unsigned char color[4];
};

// Draws one line of text, x and y are in font pixels (uiScale makes them bigger on the screen)
void drawTextLine(float x, float y, const char* text, float r, float g, float b, float a)
{
	static char buffer[65536];
	int quads = stb_easy_font_print(x, y, (char*)text, NULL, buffer, sizeof(buffer));
	if (quads > TEXT_MAX_QUADS)
	{
		quads = TEXT_MAX_QUADS;
	}
	glUniform4f(glGetUniformLocation(textProgram, "uColor"), r, g, b, a);
	glBufferSubData(GL_ARRAY_BUFFER, 0, quads * 4 * 16, buffer);
	glDrawElements(GL_TRIANGLES, quads * 6, GL_UNSIGNED_INT, 0);
}

// Text with a dark shadow, so we can read it on any background
void drawShadowedLine(float x, float y, const char* text, float r, float g, float b)
{
	drawTextLine(x + 1.0f, y + 1.0f, text, 0.0f, 0.0f, 0.0f, 1.0f);
	drawTextLine(x, y, text, r, g, b, 1.0f);
}

void drawRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	TextVertex v[4];
	float xs[4] = { x, x + w, x + w, x };
	float ys[4] = { y, y, y + h, y + h };
	for (int i = 0; i < 4; i++)
	{
		v[i].x = xs[i];
		v[i].y = ys[i];
		v[i].z = 0.0f;
		v[i].color[0] = v[i].color[1] = v[i].color[2] = v[i].color[3] = 255;
	}
	glUniform4f(glGetUniformLocation(textProgram, "uColor"), r, g, b, a);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

const char* onOff(bool value)
{
	return value ? "ON" : "OFF";
}

void drawHud(float fps)
{
	uiScale = fmaxf(1.5f, (float)windowHeight / 360.0f);
	float screenW = (float)windowWidth / uiScale;
	float screenH = (float)windowHeight / uiScale;

	// The text is drawn over everything, so no depth test and no culling
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(textProgram);
	glUniform2f(glGetUniformLocation(textProgram, "uScreen"), (float)windowWidth, (float)windowHeight);
	glUniform1f(glGetUniformLocation(textProgram, "uScale"), uiScale);
	glBindVertexArray(textVAO);
	glBindBuffer(GL_ARRAY_BUFFER, textVBO);

	char line[256];

	// Top left: the topic and the team
	drawShadowedLine(6, 5, "Infinite Stairs: SCP-087  |  Computer Graphics Fundamentals, IT-2505", 0.95f, 0.95f, 0.95f);
	drawShadowedLine(6, 16, "Team: Nuray Ismagul, Kira Parunina, Dilyara Bauyrzhan", 0.75f, 0.80f, 0.90f);

	// The floor, the depth, the FPS and the state of the shadows (the main technique)
	int floorNumber = (int)floorf(totalDescent / FLOOR_HEIGHT);
	snprintf(line, sizeof(line), "Floor %d   Depth %.1f m   %.0f FPS   Camera: %s", floorNumber, totalDescent, fps, freeFly ? "FREE-FLY" : "walk");
	drawShadowedLine(6, 31, line, 0.85f, 0.85f, 0.60f);
	snprintf(line, sizeof(line), "Shadows (H): %s   Flashlight (F): %s   Fog (G): %s   Dust (5): %s   PCF %dx%d",
		onOff(lighting.shadowsOn), onOff(lighting.flashlightOn), onOff(lighting.fogOn), onOff(lighting.dustOn),
		2 * lighting.pcfRadius + 1, 2 * lighting.pcfRadius + 1);
	drawShadowedLine(6, 42, line, lighting.shadowsOn ? 0.55f : 1.0f, lighting.shadowsOn ? 1.0f : 0.45f, 0.45f);

	if (paused)
	{
		drawShadowedLine(screenW * 0.5f - 24.0f, screenH * 0.5f - 40.0f, "PAUSED (Space)", 1.0f, 0.9f, 0.3f);
	}
	drawShadowedLine(6, screenH - 12.0f, "F1: help   Space: pause   C: free-fly   H: shadows on/off", 0.7f, 0.7f, 0.7f);

	// The help window with all the keys
	if (helpVisible)
	{
		const char* lines[] = {
			"CONTROLS  (F1 closes this window)",
			"",
			"Mouse: look around        Wheel: field of view",
			"W A S D: walk             Shift: run",
			"J: jump                  Space: PAUSE",
			"C: free-fly camera (no collisions)",
			"W A S D move    E up    Q down    Shift faster",
			"R: back to the start      Esc: exit",
			"",
			"H: SHADOW MAPPING on/off",
			"P: PCF 1 / 3x3 / 5x5      O: shadow map size",
			", .: shadow bias",
			"F: flashlight             [ ]: cone angle",
			"- = : brightness",
			"G: fog on/off       1 2 3: linear / exp / Gaussian fog",
			"4: light in the fog       5: dust        B: random flicker",
			"M: antialiasing           T: textures",
			"V: debug view (final / shadow / light / fog)",
			"I: numbers to the console",
		};
		int count = (int)(sizeof(lines) / sizeof(lines[0]));
		float boxW = 330.0f, boxH = 12.0f * count + 14.0f;
		float boxX = (screenW - boxW) * 0.5f, boxY = (screenH - boxH) * 0.5f;
		drawRect(boxX, boxY, boxW, boxH, 0.0f, 0.0f, 0.0f, 0.82f);
		for (int i = 0; i < count; i++)
		{
			drawTextLine(boxX + 8.0f, boxY + 8.0f + 12.0f * i, lines[i], i == 0 ? 1.0f : 0.9f, i == 0 ? 0.85f : 0.9f, i == 0 ? 0.3f : 0.9f, 1.0f);
		}
	}

	glBindVertexArray(0);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}

void printLighting()
{
	// All the numbers of the lighting part, for the experiments and for the report
	std::cout << "Flashlight: " << (lighting.flashlightOn ? "on" : "off") << ", cone " << lighting.outerDeg << " deg (inner "
		<< lighting.outerDeg * FLASH_INNER_RATIO << "), intensity " << lighting.intensity << ", exponent " << lighting.spotExponent << std::endl;
	std::cout << "Shadows: " << (lighting.shadowsOn ? "on" : "off") << ", map " << lighting.shadowSize << " x " << lighting.shadowSize
		<< ", PCF " << 2 * lighting.pcfRadius + 1 << " x " << 2 * lighting.pcfRadius + 1 << ", bias " << lighting.shadowBias << std::endl;
	std::cout << "Fog: " << (lighting.fogOn ? "on" : "off") << ", " << FOG_NAMES[lighting.fogMode] << ", scattering "
		<< (lighting.fogScatter ? "on" : "off") << ", depth " << totalDescent << " m" << std::endl;
}

void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods)
{
	if (action == GLFW_RELEASE)
	{
		return;
	}

	// These keys also work when they are held (a repeated action)
	if (key == GLFW_KEY_LEFT_BRACKET) lighting.outerDeg = clampValue(lighting.outerDeg - 1.0f, 12.0f, 45.0f);
	else if (key == GLFW_KEY_RIGHT_BRACKET) lighting.outerDeg = clampValue(lighting.outerDeg + 1.0f, 12.0f, 45.0f);
	else if (key == GLFW_KEY_MINUS) lighting.intensity = clampValue(lighting.intensity - 0.2f, 0.4f, 8.0f);
	else if (key == GLFW_KEY_EQUAL) lighting.intensity = clampValue(lighting.intensity + 0.2f, 0.4f, 8.0f);
	else if (key == GLFW_KEY_COMMA) lighting.shadowBias = clampValue(lighting.shadowBias - 0.0001f, 0.0f, 0.01f);
	else if (key == GLFW_KEY_PERIOD) lighting.shadowBias = clampValue(lighting.shadowBias + 0.0001f, 0.0f, 0.01f);

	// The other keys work once for every press
	if (action != GLFW_PRESS)
	{
		return;
	}

	// Pause, help, jump and free camera
	if (key == GLFW_KEY_F1)
	{
		helpVisible = !helpVisible;
		return;
	}
	if (key == GLFW_KEY_SPACE)
	{
		paused = !paused;
		return;
	}
	if (key == GLFW_KEY_J)
	{
		jumpRequested = true;
		return;
	}
	if (key == GLFW_KEY_C)
	{
		freeFly = !freeFly;
		if (!freeFly && !canStand(playerX, playerZ))
		{
			// The player cant stand inside a wall or in the well, so he goes back to the start
			resetPlayer();
		}
		return;
	}
	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
	else if (key == GLFW_KEY_F) lighting.flashlightOn = !lighting.flashlightOn;
	else if (key == GLFW_KEY_T) useTexture = !useTexture;
	else if (key == GLFW_KEY_H) lighting.shadowsOn = !lighting.shadowsOn;
	else if (key == GLFW_KEY_G) lighting.fogOn = !lighting.fogOn;
	else if (key == GLFW_KEY_B) flickerEnabled = !flickerEnabled;
	else if (key == GLFW_KEY_R) resetPlayer();
	else if (key == GLFW_KEY_1) lighting.fogMode = 0;
	else if (key == GLFW_KEY_2) lighting.fogMode = 1;
	else if (key == GLFW_KEY_3) lighting.fogMode = 2;
	else if (key == GLFW_KEY_4) lighting.fogScatter = !lighting.fogScatter;
	else if (key == GLFW_KEY_5) lighting.dustOn = !lighting.dustOn;
	else if (key == GLFW_KEY_M) lighting.multisample = !lighting.multisample;
	else if (key == GLFW_KEY_P) lighting.pcfRadius = (lighting.pcfRadius + 1) % 3;
	else if (key == GLFW_KEY_V) lighting.debugView = (lighting.debugView + 1) % 4;
	else if (key == GLFW_KEY_O)
	{
		// The next size of the shadow map
		int index = 0;
		for (int i = 0; i < 4; i++)
		{
			if (SHADOW_SIZES[i] == lighting.shadowSize) index = i;
		}
		setShadowMapSize(SHADOW_SIZES[(index + 1) % 4]);
	}
	else if (key != GLFW_KEY_I) return;
	printLighting();
}

void mouseCallback(GLFWwindow* w, double x, double y)
{
	// The first call only remembers the position, otherwise the view would jump
	if (firstMouse)
	{
		lastMouseX = x;
		lastMouseY = y;
		firstMouse = false;
	}
	yaw += (float)(x - lastMouseX) * MOUSE_SENSITIVITY * PI / 180.0f;
	pitch -= (float)(y - lastMouseY) * MOUSE_SENSITIVITY * PI / 180.0f;
	pitch = clampValue(pitch, -1.5f, 1.5f);
	lastMouseX = x;
	lastMouseY = y;
}

void scrollCallback(GLFWwindow* w, double xOffset, double yOffset)
{
	// The mouse wheel changes the field of view
	fieldOfView = clampValue(fieldOfView - (float)yOffset * 3.0f, 35.0f, 100.0f);
}

void framebufferSizeCallback(GLFWwindow* w, int width, int height)
{
	windowWidth = width;
	windowHeight = height;
}

// ---------------------------------------------------------------------------------------------
// The face (billboard) and the sound
// ---------------------------------------------------------------------------------------------
void drawFace(
	GLuint program,
	GLuint vao,
	GLuint vbo,
	GLuint texture,
	Vec3 eye,
	Vec3 worldPosition,
	const float* view,
	const float* projection,
	float width,
	float height,
	float alpha
)
{
	Vec3 center = worldPosition;

	Vec3 faceRight = makeVec(1.0f, 0.0f, 0.0f);
	Vec3 faceUp = makeVec(0.0f, 1.0f, 0.0f);

	Vec3 halfRight = scale(faceRight, width * 0.5f);
	Vec3 halfUp = scale(faceUp, height * 0.5f);

	Vec3 bottomLeft = add(add(center, scale(halfRight, -1.0f)), scale(halfUp, -1.0f));
	Vec3 bottomRight = add(add(center, halfRight), scale(halfUp, -1.0f));
	Vec3 topRight = add(add(center, halfRight), halfUp);
	Vec3 topLeft = add(add(center, scale(halfRight, -1.0f)), halfUp);

	float vertices[] =
	{
		bottomLeft.x, bottomLeft.y, bottomLeft.z, 0.0f, 0.0f,
		bottomRight.x, bottomRight.y, bottomRight.z, 1.0f, 0.0f,
		topRight.x, topRight.y, topRight.z, 1.0f, 1.0f,

		bottomLeft.x, bottomLeft.y, bottomLeft.z, 0.0f, 0.0f,
		topRight.x, topRight.y, topRight.z, 1.0f, 1.0f,
		topLeft.x, topLeft.y, topLeft.z, 0.0f, 1.0f
	};

	glUseProgram(program);

	glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, view);
	glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, projection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glUniform1i(glGetUniformLocation(program, "uFaceTexture"), 0);
	glUniform1f(glGetUniformLocation(program, "uFaceAlpha"), alpha);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	drawCalls++;
	drawnTriangles += 2;

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glBindVertexArray(0);
}

void playHorrorSound()
{
#ifdef _WIN32
	PlaySound(TEXT("assets/audio/gloomy_demonic_drone.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#endif
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------
// Draws all the floors around the player. Every floor is the same model, only the model matrix (a translation)
// is different. This is the instancing of the lecture about transformations
void drawFloors(GLuint program, int baseFloor, bool setGlow)
{
	GLint modelLocation = glGetUniformLocation(program, "uModel");
	GLint glowLevelLocation = glGetUniformLocation(program, "uGlowLevel");
	GLint glowColorLocation = glGetUniformLocation(program, "uGlowColor");
	for (int j = baseFloor - FLOORS_ABOVE; j <= baseFloor + FLOORS_BELOW; j++)
	{
		float model[16];
		translationMatrix(makeVec(0.0f, -j * FLOOR_HEIGHT, 0.0f), model);
		glUniformMatrix4fv(modelLocation, 1, GL_FALSE, model);
		if (setGlow)
		{
			const Lamp& lamp = lamps[variantOf(j)];
			glUniform1f(glowLevelLocation, lamp.level);
			glUniform3fv(glowColorLocation, 1, lamp.color);
		}
		glDrawArrays(GL_TRIANGLES, 0, vertexCount);
		drawCalls++;
		drawnTriangles += vertexCount / 3;
	}
}

// Takes the nearest lamps from the drawn floors and gives their data to the shader (positions in the eye coordinates)
int selectLamps(int baseFloor, Vec3 eye, const float* view,
	float* positions, float* colors, float* levels,
	float coldLightFactor, float redLightFactor)
{
	int ids[16];
	float distances[16];
	int n = 0;
	for (int j = baseFloor - FLOORS_ABOVE; j <= baseFloor + FLOORS_BELOW; j++)
	{
		if (!lamps[variantOf(j)].exists)
		{
			continue;
		}
		ids[n] = j;
		distances[n] = length(subtract(lampWorldPosition(j), eye));
		n++;
	}
	int count = 0;
	while (count < MAX_LAMPS && n > 0)
	{
		// Find the nearest of the rest, copy it and remove it from the list
		int best = 0;
		for (int i = 1; i < n; i++)
		{
			if (distances[i] < distances[best]) best = i;
		}
		const Lamp& lamp = lamps[variantOf(ids[best])];
		Vec3 p = transformPoint(view, lampWorldPosition(ids[best]));
		positions[count * 3] = p.x;
		positions[count * 3 + 1] = p.y;
		positions[count * 3 + 2] = p.z;

		// Blend the original lamp color with a cold blue tint or with a red tint
		const float coldColor[3] = { 0.55f, 0.72f, 1.0f };
		const float redColor[3] = { 1.0f, 0.08f, 0.03f };
		for (int c = 0; c < 3; c++)
		{
			float normalColor = lamp.color[c] * (1.0f - coldLightFactor) + coldColor[c] * coldLightFactor;
			colors[count * 3 + c] = normalColor * (1.0f - redLightFactor) + redColor[c] * redLightFactor;
		}
		levels[count] = (depthEventTimer > 0.0f) ? 0.0f : lamp.level;
		count++;
		ids[best] = ids[n - 1];
		distances[best] = distances[n - 1];
		n--;
	}
	return count;
}

int main(int argc, char** argv)
{
	srand((unsigned int)time(NULL));

	// Initialize GLFW. On Windows the visible Windows platform is chosen (GLFW 3.4 or newer)
#ifdef _WIN32
	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);
#endif
	if (!glfwInit())
	{
		return -1;
	}

	// Tell GLFW we are using OpenGL 3.3 with the CORE profile, with a depth buffer and 4 samples per pixel
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	glfwWindowHint(GLFW_SAMPLES, 4);

	window = glfwCreateWindow(windowWidth, windowHeight, "Infinite Stairs: SCP-087", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwSwapInterval(1);

	// The window can be bigger than 1280 x 720 on a screen with scaling, so the real size is asked
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

	// The mouse is hidden and captured, it only turns the head
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// The depth test makes the nearest triangle visible, the culling skips the back faces
	// The point size is set by the vertex shader (the dust)
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_PROGRAM_POINT_SIZE);

	resetLighting();

	// The fragment shaders of the scene and of the dust use the same code of the light, the shadow and the fog
	std::string shared = readTextFile("shaders/lighting_shared.glsl");
	GLuint sceneProgram = createShaderProgram(readTextFile("shaders/scene.vert"), shared + readTextFile("shaders/scene.frag"));
	GLuint shadowProgram = createShaderProgram(readTextFile("shaders/shadow.vert"), readTextFile("shaders/shadow.frag"));
	GLuint dustProgram = createShaderProgram(readTextFile("shaders/dust.vert"), shared + readTextFile("shaders/dust.frag"));
	GLuint faceProgram = createShaderProgram(readTextFile("shaders/face.vert"), readTextFile("shaders/face.frag"));
	initText();

	// Create the model of one floor, the VAO and the VBO
	buildFloor();
	GLuint VAO, VBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(float), meshData.data(), GL_STATIC_DRAW);

	// Every vertex has 9 floats: position, normal, texture coordinates and material
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	// The dust: a VAO and a dynamic VBO, the data of the particles is sent in every frame
	GLuint dustVAO, dustVBO;
	glGenVertexArrays(1, &dustVAO);
	glGenBuffers(1, &dustVBO);
	glBindVertexArray(dustVAO);
	glBindBuffer(GL_ARRAY_BUFFER, dustVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(dustData), NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// Face billboard geometry
	GLuint faceVAO, faceVBO;
	float faceVertices[30] = { 0.0f };
	glGenVertexArrays(1, &faceVAO);
	glGenBuffers(1, &faceVBO);
	glBindVertexArray(faceVAO);
	glBindBuffer(GL_ARRAY_BUFFER, faceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(faceVertices), faceVertices, GL_DYNAMIC_DRAW);
	// Position: 3 floats
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// Texture coordinates: 2 floats
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	// The textures are made by the program, the shadow map is an empty depth texture
	std::vector<unsigned char> pixels;
	makeConcreteTexture(pixels);
	GLuint concreteTexture = createTexture(pixels);
	makeMetalTexture(pixels);
	GLuint metalTexture = createTexture(pixels);
	createShadowMap();
	GLuint faceTexture = loadPNGTexture("assets/textures/scp_face.png");

	initLamps();
	resetPlayer();
	initDust(cameraY);
	int lastWrapCount = wrapCount;

	// Instructions for the user
	std::cout << "Mouse: look around, mouse wheel: field of view" << std::endl;
	std::cout << "W A S D: walk, Shift: run, R: back to the start, Esc: exit" << std::endl;
	std::cout << "F1: help on the screen, Space: pause, C: free-fly camera (E up, Q down)" << std::endl;
	std::cout << "F: flashlight, [ ]: cone angle, - =: flashlight brightness" << std::endl;
	std::cout << "H: shadows, P: PCF (hard / 3x3 / 5x5), O: shadow map size, , .: shadow bias" << std::endl;
	std::cout << "G: fog, 1 2 3: linear / exponential / Gaussian fog, 4: light in the fog" << std::endl;
	std::cout << "5: dust, M: antialiasing, T: textures, B: random flicker" << std::endl;
	std::cout << "V: debug view (final / shadow / light / fog), I: print the numbers" << std::endl;

	double lastTime = glfwGetTime();
	double startTime = lastTime;
	double fpsTime = lastTime;
	int frames = 0;
	long totalFrames = 0;
	float fps = 0.0f;
	float simClock = 0.0f;   // the time without the pauses

	// Main while loop
	while (!glfwWindowShouldClose(window))
	{
		// Time since the previous frame, a long frame (window moving) is cut, so the player does not jump
		double currentTime = glfwGetTime();
		float seconds = clampValue((float)(currentTime - lastTime), 0.0f, 0.1f);
		lastTime = currentTime;

		// In the pause simSeconds is 0, so the player, lamps, dust and events stop
		// The mouse and the flashlight sway use the real time, so the head still turns
		float simSeconds = paused ? 0.0f : seconds;
		if (!paused)
		{
			simClock += seconds;
		}

		// FPS is counted for half a second and is shown in the title
		frames++;
		totalFrames++;
		if (currentTime - fpsTime >= 0.5)
		{
			fps = (float)(frames / (currentTime - fpsTime));
			frames = 0;
			fpsTime = currentTime;
			updateTitle(fps);
		}
		drawCalls = 0;
		drawnTriangles = 0;

		// Update the player and the lights
		updatePlayer(simSeconds);
		for (int i = 0; i < LOOP_FLOORS; i++)
		{
			updateLamp(lamps[i], simSeconds);
		}
		updateLamp(flashlight, simSeconds);

		// Depth-based lighting event
		if (!depthEventTriggered && totalDescent >= 15.0f)
		{
			depthEventTriggered = true;
			depthEventTimer = 2.0f;
		}

		// Emergency red light event
		if (!redLightEventTriggered && totalDescent >= 48.0f)
		{
			redLightEventTriggered = true;
			redLightEventTimer = 4.0f;
		}

		// SCP face event: the scene gets dark for 1 second and then the light comes back
		if (!faceEventTriggered && totalDescent >= FACE_DEPTH)
		{
			faceEventTriggered = true;
			faceEventActive = true;
			faceEventTimer = 2.0f;
		}

		if (redLightEventTimer > 0.0f)
		{
			redLightEventTimer -= simSeconds;

			if (redLightEventTimer < 0.0f)
			{
				redLightEventTimer = 0.0f;
			}
		}

		if (depthEventTimer > 0.0f)
		{
			depthEventTimer -= simSeconds;
			if (depthEventTimer < 0.0f)
			{
				depthEventTimer = 0.0f;
			}
		}

		if (faceEventTimer > 0.0f)
		{
			faceEventTimer -= simSeconds;

			if (faceEventTimer <= 0.0f)
			{
				faceEventTimer = 0.0f;
				faceEventActive = false;
			}
		}

		// SCP face event brightness
		float sceneBrightness = 1.0f;

		if (faceEventActive)
		{
			if (faceEventTimer > 1.0f)
			{
				sceneBrightness = 0.05f;
			}
			else
			{
				sceneBrightness = 0.05f + 0.95f * (1.0f - faceEventTimer);
			}
		}

		if (faceJumpScareActive)
		{
			float t = clampValue(faceAttackProgress, 0.0f, 1.0f);

			if (t < 0.3f)
			{
				sceneBrightness = 1.0f - 0.90f * (t / 0.3f);
			}
			else
			{
				sceneBrightness = 0.10f;
			}
		}

		// Horror sound starts at 48 meters
		if (!horrorSoundTriggered && totalDescent >= 48.0f)
		{
			horrorSoundTriggered = true;
			playHorrorSound();
		}

		// Fog and the cold light depend on the depth
		Atmosphere atmosphere = computeAtmosphere(totalDescent);

		// Emergency red light pulse
		float redLightFactor = 0.0f;
		if (redLightEventTimer > 0.0f)
		{
			redLightFactor = 0.65f + 0.20f * sinf(simClock * 10.0f);
		}

		// The camera: yaw and pitch give the direction of the view, the position is the eye of the player
		Vec3 eye = makeVec(playerX, cameraY, playerZ);
		Vec3 forward = viewDirection();
		Vec3 right = normalize(cross(forward, makeVec(0.0f, 1.0f, 0.0f)));
		Vec3 cameraUp = cross(right, forward);
		float view[16], projection[16];
		lookAt(eye, add(eye, forward), makeVec(0.0f, 1.0f, 0.0f), view);
		float aspect = 1.0f;
		if (windowHeight > 0)
		{
			aspect = (float)windowWidth / (float)windowHeight;
		}
		perspectiveMatrix(fieldOfView, aspect, 0.05f, 80.0f, projection);

		// The flashlight is in the right hand. The hand follows the head with a small delay (sway)
		flashDirection = normalize(add(flashDirection, scale(subtract(forward, flashDirection), 1.0f - expf(-FLASH_FOLLOW_SPEED * seconds))));
		Vec3 flashPosition = add(eye, add(scale(right, FLASH_OFFSET_X), scale(cameraUp, FLASH_OFFSET_Y)));

		// Light of the current frame. The shadow map is rendered from the flashlight with a perspective projection
		// that is a little wider than the cone
		FrameLight frame;
		float shadowFov = 2.0f * lighting.outerDeg + 4.0f;
		float lightView[16], lightProjection[16];
		lookAt(flashPosition, add(flashPosition, flashDirection), cameraUp, lightView);
		perspectiveMatrix(shadowFov, 1.0f, 0.2f, 30.0f, lightProjection);
		multiplyMatrices(lightProjection, lightView, frame.lightVP);
		frame.flashWorldPosition = flashPosition;
		frame.flashEyePosition = makeVec(FLASH_OFFSET_X, FLASH_OFFSET_Y, 0.0f);
		frame.flashEyeDirection = normalize(transformDirection(view, flashDirection));
		frame.flashIntensity = lighting.intensity * flashlight.level;
		// Deeper in the stairwell the light reaches less far
		frame.attenuation[0] = 1.0f;
		frame.attenuation[1] = 0.09f;
		frame.attenuation[2] = 0.032f * (1.0f + 0.6f * atmosphere.depthFactor);
		frame.shadowTexelScale = 2.0f * tanf(shadowFov * PI / 360.0f) / (float)lighting.shadowSize;
		frame.fogDensity = atmosphere.fogDensity;
		for (int i = 0; i < 3; i++)
		{
			frame.fogColor[i] = atmosphere.fogColor[i];
		}

		// The dust moves with the loop and is simulated only when it is visible
		if (wrapCount != lastWrapCount)
		{
			shiftDust((wrapCount - lastWrapCount) * LOOP_FLOORS * FLOOR_HEIGHT);
			lastWrapCount = wrapCount;
		}
		if (lighting.dustOn)
		{
			updateDust(simSeconds, cameraY);
		}

		// The floor of the player and the floors around him
		int baseFloor = (int)floorf(-feetY / FLOOR_HEIGHT);
		bool shadowActive = lighting.shadowsOn && lighting.flashlightOn;

		// Pass 1: the scene is drawn from the flashlight into the shadow map, only the depth is saved
		glBindVertexArray(VAO);
		if (shadowActive)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
			glViewport(0, 0, lighting.shadowSize, lighting.shadowSize);
			glClear(GL_DEPTH_BUFFER_BIT);
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(2.0f, 4.0f);
			glUseProgram(shadowProgram);
			glUniformMatrix4fv(glGetUniformLocation(shadowProgram, "uLightVP"), 1, GL_FALSE, frame.lightVP);
			drawFloors(shadowProgram, baseFloor, false);
			glDisable(GL_POLYGON_OFFSET_FILL);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// Pass 2: the scene is drawn from the camera to the screen
		glViewport(0, 0, windowWidth, windowHeight);
		if (lighting.multisample)
		{
			glEnable(GL_MULTISAMPLE);
		}
		else
		{
			glDisable(GL_MULTISAMPLE);
		}
		glClearColor(powf(atmosphere.fogColor[0], 1.0f / 2.2f), powf(atmosphere.fogColor[1], 1.0f / 2.2f), powf(atmosphere.fogColor[2], 1.0f / 2.2f), 1.0f);
		if (lighting.debugView != 0)
		{
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(sceneProgram);
		glUniform1f(glGetUniformLocation(sceneProgram, "uSceneBrightness"), sceneBrightness);

		// Matrices
		glUniformMatrix4fv(glGetUniformLocation(sceneProgram, "uView"), 1, GL_FALSE, view);
		glUniformMatrix4fv(glGetUniformLocation(sceneProgram, "uProjection"), 1, GL_FALSE, projection);

		// Textures: unit 0 = concrete, unit 1 = metal, unit 2 = shadow map
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, concreteTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, metalTexture);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, shadowTexture);
		glUniform1i(glGetUniformLocation(sceneProgram, "uTexConcrete"), 0);
		glUniform1i(glGetUniformLocation(sceneProgram, "uTexMetal"), 1);

		// Switches, the flashlight, the shadows and the fog
		glUniform1i(glGetUniformLocation(sceneProgram, "uUseTexture"), useTexture ? 1 : 0);
		glUniform1i(glGetUniformLocation(sceneProgram, "uFogScatter"), lighting.fogScatter ? 1 : 0);
		glUniform1i(glGetUniformLocation(sceneProgram, "uDebugView"), lighting.debugView);
		uploadLightingUniforms(sceneProgram, frame, shadowActive);

		// The nearest lamps
		float lampPositions[MAX_LAMPS * 3];
		float lampColors[MAX_LAMPS * 3];
		float lampLevels[MAX_LAMPS];
		int lampCount = selectLamps(baseFloor, eye, view, lampPositions, lampColors, lampLevels, atmosphere.coldLightFactor, redLightFactor);
		glUniform1i(glGetUniformLocation(sceneProgram, "uLampCount"), lampCount);
		if (lampCount > 0)
		{
			glUniform3fv(glGetUniformLocation(sceneProgram, "uLampPosition"), lampCount, lampPositions);
			glUniform3fv(glGetUniformLocation(sceneProgram, "uLampColor"), lampCount, lampColors);
			glUniform1fv(glGetUniformLocation(sceneProgram, "uLampLevel"), lampCount, lampLevels);
		}

		drawFloors(sceneProgram, baseFloor, true);

		// The dust is drawn after the solid scene: the depth test hides it behind the walls, but it does not write
		// to the depth buffer. The additive blending adds the light of the particle to the pixel
		if (lighting.dustOn && lighting.debugView == 0)
		{
			glUseProgram(dustProgram);
			glUniformMatrix4fv(glGetUniformLocation(dustProgram, "uView"), 1, GL_FALSE, view);
			glUniformMatrix4fv(glGetUniformLocation(dustProgram, "uProjection"), 1, GL_FALSE, projection);
			glUniform1f(glGetUniformLocation(dustProgram, "uPointScale"), (float)windowHeight / (2.0f * tanf(fieldOfView * PI / 360.0f)));
			glUniform1f(glGetUniformLocation(dustProgram, "uSceneBrightness"), sceneBrightness);
			uploadLightingUniforms(dustProgram, frame, shadowActive);

			glBindVertexArray(dustVAO);
			glBindBuffer(GL_ARRAY_BUFFER, dustVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dustData), dustData);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glDepthMask(GL_FALSE);
			glDrawArrays(GL_POINTS, 0, DUST_COUNT);
			drawCalls++;
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glBindVertexArray(0);
		}

		// The face of the event
		float faceAlpha = 1.0f;
		float loopLength = LOOP_FLOORS * FLOOR_HEIGHT;
		float faceLocalY = -FACE_DEPTH + wrapCount * loopLength;
		Vec3 facePosition = makeVec(FACE_X, faceLocalY, FACE_Z);
		Vec3 animatedFacePosition = facePosition;
		float faceScale = 1.0f;

		if (faceJumpScareActive)
		{
			float t = clampValue(faceAttackProgress, 0.0f, 1.0f);

			animatedFacePosition = add(faceAttackStart, scale(faceAttackDirection, 3.0f * t));

			faceScale = 1.0f + 1.5f * t;

			if (t > 0.85f)
			{
				faceAlpha = 1.0f - (t - 0.85f) / 0.15f;
			}
		}

		float distanceToFace = length(subtract(eye, facePosition));

		if (!faceJumpScareTriggered &&
			!faceJumpScareActive &&
			faceEventTriggered &&
			distanceToFace < 4.0f)
		{
			faceJumpScareTriggered = true;
			faceJumpScareActive = true;
			faceJumpScareTimer = FACE_ATTACK_DURATION;

			faceAttackStart = facePosition;
			faceAttackDirection = makeVec(0.0f, 0.0f, -1.0f);

			faceAttackProgress = 0.0f;
		}

		if (faceJumpScareActive)
		{
			faceJumpScareTimer -= simSeconds;

			faceAttackProgress = 1.0f - faceJumpScareTimer / FACE_ATTACK_DURATION;

			if (faceJumpScareTimer <= 0.0f)
			{
				faceJumpScareTimer = 0.0f;
				faceJumpScareActive = false;
			}
		}

		if (faceTexture != 0 &&
			faceEventTriggered &&
			(!faceJumpScareTriggered || faceJumpScareActive))
		{
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);

			drawFace(faceProgram, faceVAO, faceVBO, faceTexture, eye, animatedFacePosition, view, projection,
				2.0f * faceScale, 2.4f * faceScale, faceAlpha);

			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
		}

		// The text is drawn last, over everything
		drawHud(fps);

		// Swap the back buffer with the front buffer
		glfwSwapBuffers(window);
		// Take care of all GLFW events
		glfwPollEvents();
	}

	// The average FPS of the whole run (for the report)
	double totalSeconds = glfwGetTime() - startTime;
	if (totalSeconds > 0.0)
	{
		std::cout << "Average FPS: " << totalFrames / totalSeconds << " (" << totalFrames << " frames)" << std::endl;
	}

	// Delete all the objects we've created
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &dustVAO);
	glDeleteBuffers(1, &dustVBO);
	glDeleteVertexArrays(1, &faceVAO);
	glDeleteBuffers(1, &faceVBO);
	glDeleteTextures(1, &concreteTexture);
	glDeleteTextures(1, &metalTexture);
	glDeleteTextures(1, &shadowTexture);
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteProgram(sceneProgram);
	glDeleteProgram(shadowProgram);
	glDeleteProgram(dustProgram);
	glDeleteProgram(faceProgram);
	glDeleteProgram(textProgram);
	glDeleteVertexArrays(1, &textVAO);
	glDeleteBuffers(1, &textVBO);
	glDeleteBuffers(1, &textEBO);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}