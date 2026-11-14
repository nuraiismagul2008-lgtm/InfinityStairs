// SCP-087 "Infinite Stairs" - Final Project (beta)
// A real-time first-person demo of an endless dark stairwell. The player has only a flashlight.
// Modern OpenGL 3.3 Core, GLFW + glad.

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <Windows.h>
#include <mmsystem.h>

#pragma comment(lib, "Winmm.lib")

const float PI = 3.14159265f;

// Building constants. One "floor" is the module that is repeated again and again:
// north landing -> flight 1 (west side, goes south) -> south landing -> flight 2 (east side, goes north)
// The top of the north landing is y = 0, every next floor is FLOOR_HEIGHT lower

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

// player
const float EYE_HEIGHT = 1.65f;
const float PLAYER_RADIUS = 0.25f;
const float STEP_UP = 0.55f;
const float WALK_SPEED = 2.2f;
const float RUN_SPEED = 3.6f;
const float MOUSE_SENSITIVITY = 0.1f;

// Flashlight: it is held in the right hand, a bit lower than the eyes, so the shadows can be seen
const float FLASH_OFFSET_X = 0.22f;
const float FLASH_OFFSET_Y = -0.20f;
const float FLASH_INNER_DEG = 14.0f;
const float FLASH_OUTER_DEG = 26.0f;

const int SHADOW_SIZE = 2048;
const int TEXTURE_SIZE = 256;

// vector + matrix math - 3d vector

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
	// vector with no l = 0,0,0 - not divisible by zero
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
	// random num 0-1
	return (float)rand() / (float)RAND_MAX;
}

float randomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * randomFloat();
}

// Matrices = arrays of 16 floats

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
	// result = a * b - temporary array
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
	// camera looks from eye to target - f = forward, s = right, u = up
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
	// Multiply the points by matrix
	return makeVec(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
		m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
		m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
}


// The model of one floor. Every vertex has position (3), normal (3), texture coordinates (2) and material (1)
// Materials: 0 = concrete, 1 = metal, 2 = lamp (it shines by itself)

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

// Procedural textures. A value noise with a period is used, so the textures can be repeated without seams

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

	unsigned char* image = stbi_load(
		path.c_str(),
		&width,
		&height,
		&channels,
		STBI_rgb_alpha
	);

	if (!image)
	{
		std::cout << "Failed to load texture: "
			<< path << std::endl;
		return 0;
	}

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		width,
		height,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		image
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(image);

	return texture;
}

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

	glUniformMatrix4fv(
		glGetUniformLocation(program, "uView"),
		1, GL_FALSE, view
	);

	glUniformMatrix4fv(
		glGetUniformLocation(program, "uProjection"),
		1, GL_FALSE, projection
	);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glUniform1i(
		glGetUniformLocation(program, "uFaceTexture"),
		0
	);

	glUniform1f(
		glGetUniformLocation(program, "uFaceAlpha"),
		alpha
	);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glBufferSubData(
		GL_ARRAY_BUFFER,
		0,
		sizeof(vertices),
		vertices
	);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthMask(GL_FALSE);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glBindVertexArray(0);
}

void playHorrorSound()
{
	PlaySound(
		TEXT("assets/audio/gloomy_demonic_drone.wav"),
		NULL,
		SND_FILENAME | SND_ASYNC | SND_NODEFAULT
	);
}

// Shaders. All the lighting is calculated per fragment (Phong shading) in the camera (eye) coordinates

const char* faceVertexShaderCode = R"GLSL(
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vUV;

uniform mat4 uView;
uniform mat4 uProjection;

out vec2 fUV;

void main()
{
    fUV = vUV;
    gl_Position = uProjection * uView * vec4(vPosition, 1.0);
}
)GLSL";

const char* faceFragmentShaderCode = R"GLSL(
in vec2 fUV;
out vec4 FragColor;

uniform sampler2D uFaceTexture;

uniform float uFaceAlpha;

void main()
{
    vec4 face = texture(uFaceTexture, fUV);

    if (face.a < 0.05)
        discard;

    face.a *= uFaceAlpha;
	FragColor = face;
}
)GLSL";

// Vertex Shader of the scene: model -> view -> projection, and the position in the light space for the shadow
const char* sceneVertexShaderCode = R"GLSL(
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in float vMaterial;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightVP;
out vec3 fPositionEye;
out vec3 fNormalEye;
out vec2 fUV;
flat out float fMaterial;
out vec4 fShadowCoord;
void main()
{
	// The model matrix only moves the floor to its place, so the normal needs only the rotation of the view matrix
	vec4 worldPosition = uModel * vec4(vPosition, 1.0);
	vec4 eyePosition = uView * worldPosition;
	fPositionEye = eyePosition.xyz;
	fNormalEye = mat3(uView) * vNormal;
	fUV = vUV;
	fMaterial = vMaterial;
	fShadowCoord = uLightVP * worldPosition;
	gl_Position = uProjection * eyePosition;
}
)GLSL";

// Fragment Shader of the scene
const char* sceneFragmentShaderCode = R"GLSL(
in vec3 fPositionEye;
in vec3 fNormalEye;
in vec2 fUV;
flat in float fMaterial;
in vec4 fShadowCoord;
out vec4 FragColor;

uniform sampler2D uTexConcrete;
uniform sampler2D uTexMetal;
uniform sampler2DShadow uShadowMap;
uniform bool uUseTexture;
uniform bool uUseShadow;
uniform bool uUseFog;
uniform float uShadowBias;

// The flashlight is a spotlight, its position and direction are in the eye coordinates
uniform bool uFlashlightOn;
uniform vec3 uFlashPosition;
uniform vec3 uFlashDirection;
uniform float uFlashInner;
uniform float uFlashOuter;
uniform float uFlashIntensity;

// The lamps are point lights without shadows
uniform int uLampCount;
uniform vec3 uLampPosition[4];
uniform vec3 uLampColor[4];
uniform float uLampLevel[4];

// The glow of the body of the lamp of the floor that is drawn now
uniform float uGlowLevel;
uniform vec3 uGlowColor;

uniform float uFogDensity;
uniform vec3 uFogColor;

uniform float uSceneBrightness;

// Blinn-Phong: diffuse + specular with the halfway vector
vec3 blinnPhong(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 specularColor, float shininess)
{
	float diffuse = max(dot(N, L), 0.0);
	float specular = 0.0;
	if (diffuse > 0.0)
	{
		vec3 H = normalize(L + V);
		specular = pow(max(dot(N, H), 0.0), shininess);
	}
	return diffuse * albedo + specular * specularColor;
}

// Shadow mapping: compares the depth of the fragment with the depth that the flashlight has seen
// The 3x3 neighbour texels are used (PCF), every lookup is already filtered by the hardware
float shadowFactor(float cosTheta)
{
	if (!uUseShadow || fShadowCoord.w <= 0.0)
	{
		return 1.0;
	}
	vec3 p = fShadowCoord.xyz / fShadowCoord.w;
	p = p * 0.5 + 0.5;
	if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
	{
		return 1.0;
	}
	// The bias is bigger on the surfaces that are almost parallel to the light rays
	float bias = uShadowBias * (1.0 + 2.0 * (1.0 - cosTheta));
	vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
	float lit = 0.0;
	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			lit += textureLod(uShadowMap, vec3(p.xy + vec2(x, y) * texel, p.z - bias), 0.0);
		}
	}
	return lit / 9.0;
}

void main()
{
	// Both textures are always read (it is safe for the mipmap levels), the material chooses one of them
	vec3 texConcrete = texture(uTexConcrete, fUV).rgb;
	vec3 texMetal = texture(uTexMetal, fUV).rgb;
	vec3 N = normalize(fNormalEye);
	vec3 V = normalize(-fPositionEye);
	vec3 color;

	if (fMaterial > 1.5)
	{
		// The body of the lamp: it shines by itself
		color = uGlowColor * uGlowLevel;
	}
	else
	{
		vec3 albedo = texConcrete;
		vec3 specularColor = vec3(0.08);
		float shininess = 10.0;
		if (fMaterial > 0.5)
		{
			albedo = texMetal;
			specularColor = vec3(0.6);
			shininess = 48.0;
		}
		if (!uUseTexture)
		{
			albedo = vec3(0.5);
		}
		// The textures are in the sRGB space, the lighting is calculated in the linear space
		albedo = pow(albedo, vec3(2.2));

		// Very weak ambient light, only to see that the world is not completely black
		color = vec3(0.012, 0.014, 0.020) * albedo;

		if (uFlashlightOn)
		{
			vec3 toLight = uFlashPosition - fPositionEye;
			float d = length(toLight);
			vec3 L = toLight / d;
			// Spotlight: full light inside the inner cone, a soft edge between the inner and the outer cone
			float theta = dot(-L, uFlashDirection);
			float spot = clamp((theta - uFlashOuter) / (uFlashInner - uFlashOuter), 0.0, 1.0);
			// Distance term 1 / (a + b * d + c * d * d)
			float attenuation = 1.0 / (1.0 + 0.09 * d + 0.032 * d * d);
			float shadow = shadowFactor(max(dot(N, L), 0.0));
			vec3 flashColor = vec3(1.0, 0.94, 0.80);
			color += blinnPhong(N, V, L, albedo, specularColor, shininess) * flashColor * (uFlashIntensity * attenuation * spot * shadow);
		}

		for (int i = 0; i < uLampCount; i++)
		{
			vec3 toLamp = uLampPosition[i] - fPositionEye;
			float d = length(toLamp);
			vec3 L = toLamp / d;
			float attenuation = 1.0 / (1.0 + 0.30 * d + 0.10 * d * d);
			color += blinnPhong(N, V, L, albedo, specularColor, shininess) * uLampColor[i] * (uLampLevel[i] * attenuation);
		}
	}

	// Gaussian fog: the farther the fragment, the more it gets the color of the fog
	if (uUseFog)
	{
		float x = length(fPositionEye) * uFogDensity;
		float fogAmount = exp(-x * x);
		color = mix(uFogColor, color, fogAmount);
	}

	// Apply global scene brightness
	color *= uSceneBrightness;

	// Back to the sRGB space
	color = pow(color, vec3(1.0 / 2.2));
	FragColor = vec4(color, 1.0);
}
)GLSL";

// The shadow pass only needs the depth, so its shaders are very small
const char* shadowVertexShaderCode = R"GLSL(
layout (location = 0) in vec3 vPosition;
uniform mat4 uModel;
uniform mat4 uLightVP;
void main()
{
	gl_Position = uLightVP * uModel * vec4(vPosition, 1.0);
}
)GLSL";

const char* shadowFragmentShaderCode = R"GLSL(
void main()
{
}
)GLSL";

//shader load
GLuint compileShader(GLenum type, const char* code)
{
	// shader = version line + code of stage
	std::string source = std::string("#version 330 core\n") + code;
	const char* sourcePointer = source.c_str();
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &sourcePointer, NULL);
	glCompileShader(shader);

	// error
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

GLuint createShaderProgram(const char* vertexCode, const char* fragmentCode)
{
	// Compile both shaders + link them into one Shader Program
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

	// shader objects arent needed after linking
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	return program;
}



// The shadow map is a depth texture that is attached to a frame buffer object (FBO), the scene is rendered into it
void createShadowMap(GLuint& framebuffer, GLuint& depthTexture)
{
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
	// There is no color buffer in this frame buffer
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "The shadow frame buffer is not complete" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Light sources with a random flicker. Every light has a timer: it is steady for some seconds,
// then it flickers for a short time (the level jumps between dark and bright), then it is steady again

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
	const float flashColor[3] = { 1.0f, 0.94f, 0.80f };
	initLamp(flashlight, true, 8.0f, 20.0f, flashColor);
}

// зlayer + camera + the loop of the floors

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

float faceWorldY = -51.0f;
const float FACE_DEPTH = 51.0f;
const float FACE_X = 0.0f;
const float FACE_Z = HALF_Z - 0.05f;
int wrapCount = 0;           // times player moved back by the loop
bool flashlightOn = true;
bool useTexture = true;
bool useShadow = true;
bool useFog = true;
bool firstMouse = true;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

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
}

bool canStand(float x, float z)
{
	// player cant go through the walls or into the open well
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

// height of the floor under the point
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
	// walking directions on the floor + the pitch is ignored
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

	// X and Z are tried separately
	float newX = playerX + wish.x * speed * seconds;
	if (canStand(newX, playerZ)) playerX = newX;
	float newZ = playerZ + wish.z * speed * seconds;
	if (canStand(playerX, newZ)) playerZ = newZ;

	// feet follow the steps + camera follows the feet later
	float oldFeetY = feetY;
	feetY = groundHeight(playerX, playerZ, feetY);
	totalDescent += oldFeetY - feetY;
	cameraY += (feetY + EYE_HEIGHT - cameraY) * clampValue(12.0f * seconds, 0.0f, 1.0f);

	// The loop
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

// input

void updateTitle(float fps)
{
	// window title is the HUD -> floor, depth, loops, FPS and the state of the effects
	int floorNumber = (int)floorf(totalDescent / FLOOR_HEIGHT);
	char title[200];
	snprintf(title, sizeof(title), "SCP-087 | Floor %d | Depth %.1f m | Loops %d | %.0f FPS | Light %s | Tex %s | Shadow %s | Fog %s",
		floorNumber, totalDescent, wrapCount, fps, flashlightOn ? "on" : "off", useTexture ? "on" : "off", useShadow ? "on" : "off", useFog ? "on" : "off");
	glfwSetWindowTitle(window, title);
}

void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods)
{
	// keys work one per press
	if (action != GLFW_PRESS)
	{
		return;
	}
	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
	else if (key == GLFW_KEY_F) flashlightOn = !flashlightOn;
	else if (key == GLFW_KEY_T) useTexture = !useTexture;
	else if (key == GLFW_KEY_H) useShadow = !useShadow;
	else if (key == GLFW_KEY_G) useFog = !useFog;
	else if (key == GLFW_KEY_B) flickerEnabled = !flickerEnabled;
	else if (key == GLFW_KEY_R) resetPlayer();
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

// Draws same model floors around player
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
			float normalColor =
				lamp.color[c] * (1.0f - coldLightFactor) +
				coldColor[c] * coldLightFactor;

			colors[count * 3 + c] =
				normalColor * (1.0f - redLightFactor) +
				redColor[c] * redLightFactor;
		}
		levels[count] = (depthEventTimer > 0.0f) ? 0.0f : lamp.level;
		count++;
		ids[best] = ids[n - 1];
		distances[best] = distances[n - 1];
		n--;
	}
	return count;
}


//opengl setup
int main(int argc, char** argv)
{
	srand((unsigned int)time(NULL));

	// initialize GLFW
#ifdef _WIN32
	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);
#endif
	if (!glfwInit())
	{
		return -1;
	}

	// Tell GLFW were using OpenGL 3.3 + CORE profile, with a depth buffer
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);

	window = glfwCreateWindow(windowWidth, windowHeight, "SCP-087", NULL, NULL);
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

	// The window size is asked
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

	// the mouse is hidden + captured, it only turns the head
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);


	// The depth test makes the nearest triangle visible, the culling skips the back faces
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	GLuint sceneProgram = createShaderProgram(sceneVertexShaderCode, sceneFragmentShaderCode);
	GLuint shadowProgram = createShaderProgram(shadowVertexShaderCode, shadowFragmentShaderCode);
	GLuint faceProgram = createShaderProgram(
		faceVertexShaderCode,
		faceFragmentShaderCode
	);

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
	// Face billboard geometry
	GLuint faceVAO, faceVBO;

	float faceVertices[30] = { 0.0f };

	glGenVertexArrays(1, &faceVAO);
	glGenBuffers(1, &faceVBO);

	glBindVertexArray(faceVAO);
	glBindBuffer(GL_ARRAY_BUFFER, faceVBO);

	glBufferData(
		GL_ARRAY_BUFFER,
		sizeof(faceVertices),
		faceVertices,
		GL_DYNAMIC_DRAW
	);

	// Position: 3 floats
	glVertexAttribPointer(
		0, 3, GL_FLOAT, GL_FALSE,
		5 * sizeof(float), (void*)0
	);
	glEnableVertexAttribArray(0);

	// Texture coordinates: 2 floats
	glVertexAttribPointer(
		1, 2, GL_FLOAT, GL_FALSE,
		5 * sizeof(float), (void*)(3 * sizeof(float))
	);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	// The textures are made by the program, the shadow map is an empty depth texture
	std::vector<unsigned char> pixels;
	makeConcreteTexture(pixels);
	GLuint concreteTexture = createTexture(pixels);
	makeMetalTexture(pixels);
	GLuint metalTexture = createTexture(pixels);
	GLuint shadowFramebuffer, shadowTexture;
	createShadowMap(shadowFramebuffer, shadowTexture);
	GLuint faceTexture = loadPNGTexture(
		"assets/textures/scp_face.png"
	);

	initLamps();
	resetPlayer();

	// Fog color in the linear space, the clear color is the same, but in the sRGB space
	const float fogColor[3] = { 0.004f, 0.005f, 0.007f };
	float depthFogFactor = 0.0f;
	float clearColor[3];
	for (int i = 0; i < 3; i++)
	{
		clearColor[i] = powf(fogColor[i], 1.0f / 2.2f);
	}

	// Instructions for the user
	std::cout << "Mouse: look around, mouse wheel: field of view" << std::endl;
	std::cout << "W A S D: walk, Shift: run" << std::endl;
	std::cout << "F: flashlight, T: textures, H: shadows, G: fog, B: random flicker" << std::endl;
	std::cout << "R: back to the start, Esc: exit" << std::endl;

	double lastTime = glfwGetTime();
	double fpsTime = lastTime;
	int frames = 0;
	float fps = 0.0f;

	// Main while loop
	while (!glfwWindowShouldClose(window))
	{
		// Time since the previous frame, a long frame (window moving) is cut, so the player does not jump
		double currentTime = glfwGetTime();
		float seconds = clampValue((float)(currentTime - lastTime), 0.0f, 0.1f);
		lastTime = currentTime;

		// FPS is counted for half a second and is shown in the title
		frames++;
		if (currentTime - fpsTime >= 0.5)
		{
			fps = (float)(frames / (currentTime - fpsTime));
			frames = 0;
			fpsTime = currentTime;
			updateTitle(fps);
		}

		// Update the player and the lights
		updatePlayer(seconds);
		for (int i = 0; i < LOOP_FLOORS; i++)
		{
			updateLamp(lamps[i], seconds);
		}
		updateLamp(flashlight, seconds);

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

		// SCP face event
		if (!faceEventTriggered && totalDescent >= FACE_DEPTH)
		{
			faceEventTriggered = true;
			faceEventActive = true;
		}

		if (redLightEventTimer > 0.0f)
		{
			redLightEventTimer -= seconds;

			if (redLightEventTimer < 0.0f)
			{
				redLightEventTimer = 0.0f;
			}
		}

		if (depthEventTimer > 0.0f)
		{
			depthEventTimer -= seconds;
			if (depthEventTimer < 0.0f)
			{
				depthEventTimer = 0.0f;
			}
		}

		if (faceEventTimer > 0.0f)
		{
			faceEventTimer -= seconds;

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

		// Horror sound starts at 50 meters
		if (!horrorSoundTriggered && totalDescent >= 48.0f)
		{
			horrorSoundTriggered = true;
			playHorrorSound();
		}

		// Depth-based fog intensity
		depthFogFactor = clampValue(
			(totalDescent - 20.0f) / 20.0f,
			0.0f,
			1.0f
		);

		// Depth-based cold lighting
		float coldLightFactor = clampValue(
			(totalDescent - 35.0f) / 15.0f,
			0.0f,
			1.0f
		);

		// Emergency red light pulse
		float redLightFactor = 0.0f;

		if (redLightEventTimer > 0.0f)
		{
			redLightFactor = 0.65f + 0.20f * sinf(currentTime * 10.0f);
		}

		// camera
		Vec3 eye = makeVec(playerX, cameraY, playerZ);
		Vec3 forward = makeVec(cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw));
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

		// flashlight is in the right hand
		Vec3 flashPosition = add(eye, add(scale(right, FLASH_OFFSET_X), scale(cameraUp, FLASH_OFFSET_Y)));
		float lightView[16], lightProjection[16], lightVP[16];
		lookAt(flashPosition, add(flashPosition, forward), cameraUp, lightView);
		perspectiveMatrix(2.0f * FLASH_OUTER_DEG + 4.0f, 1.0f, 0.2f, 30.0f, lightProjection);
		multiplyMatrices(lightProjection, lightView, lightVP);


		// The floor of the player and the floors around him
		int baseFloor = (int)floorf(-feetY / FLOOR_HEIGHT);
		bool shadowActive = useShadow && flashlightOn;

		// Pass 1: the scene is drawn from the flashlight into the shadow map, only the depth is saved
		glBindVertexArray(VAO);
		if (shadowActive)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
			glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
			glClear(GL_DEPTH_BUFFER_BIT);
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(2.0f, 4.0f);
			glUseProgram(shadowProgram);
			glUniformMatrix4fv(glGetUniformLocation(shadowProgram, "uLightVP"), 1, GL_FALSE, lightVP);
			drawFloors(shadowProgram, baseFloor, false);
			glDisable(GL_POLYGON_OFFSET_FILL);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// Pass 2: the scene is drawn from the camera to the screen
		glViewport(0, 0, windowWidth, windowHeight);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(sceneProgram);
		glUniform1f(
			glGetUniformLocation(sceneProgram, "uSceneBrightness"),
			sceneBrightness
		);

		// Matrices
		glUniformMatrix4fv(glGetUniformLocation(sceneProgram, "uView"), 1, GL_FALSE, view);
		glUniformMatrix4fv(glGetUniformLocation(sceneProgram, "uProjection"), 1, GL_FALSE, projection);
		glUniformMatrix4fv(glGetUniformLocation(sceneProgram, "uLightVP"), 1, GL_FALSE, lightVP);

		// Textures: unit 0 = concrete, unit 1 = metal, unit 2 = shadow map
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, concreteTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, metalTexture);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, shadowTexture);
		glUniform1i(glGetUniformLocation(sceneProgram, "uTexConcrete"), 0);
		glUniform1i(glGetUniformLocation(sceneProgram, "uTexMetal"), 1);
		glUniform1i(glGetUniformLocation(sceneProgram, "uShadowMap"), 2);

		// Switches
		glUniform1i(glGetUniformLocation(sceneProgram, "uUseTexture"), useTexture ? 1 : 0);
		glUniform1i(glGetUniformLocation(sceneProgram, "uUseShadow"), shadowActive ? 1 : 0);
		glUniform1i(glGetUniformLocation(sceneProgram, "uUseFog"), useFog ? 1 : 0);
		glUniform1f(glGetUniformLocation(sceneProgram, "uShadowBias"), 0.0004f);
		
		// Depth-based fog
		float currentFogDensity = 0.20f + 0.15f * depthFogFactor;

		float currentFogColor[3] =
		{
			fogColor[0] + 0.015f * depthFogFactor,
			fogColor[1] + 0.025f * depthFogFactor,
			fogColor[2] + 0.055f * depthFogFactor
		};

		glUniform1f(glGetUniformLocation(sceneProgram, "uFogDensity"), currentFogDensity);
		glUniform3fv(glGetUniformLocation(sceneProgram, "uFogColor"), 1, currentFogColor);

		// Flashlight: in the eye coordinates the camera is at (0, 0, 0) and looks to -z
		glUniform1i(glGetUniformLocation(sceneProgram, "uFlashlightOn"), flashlightOn ? 1 : 0);
		glUniform3f(glGetUniformLocation(sceneProgram, "uFlashPosition"), FLASH_OFFSET_X, FLASH_OFFSET_Y, 0.0f);
		glUniform3f(glGetUniformLocation(sceneProgram, "uFlashDirection"), 0.0f, 0.0f, -1.0f);
		glUniform1f(glGetUniformLocation(sceneProgram, "uFlashInner"), cosf(FLASH_INNER_DEG * PI / 180.0f));
		glUniform1f(glGetUniformLocation(sceneProgram, "uFlashOuter"), cosf(FLASH_OUTER_DEG * PI / 180.0f));
		glUniform1f(glGetUniformLocation(sceneProgram, "uFlashIntensity"), 2.6f * flashlight.level);

		// The nearest lamps
		float lampPositions[MAX_LAMPS * 3];
		float lampColors[MAX_LAMPS * 3];
		float lampLevels[MAX_LAMPS];
		int lampCount = selectLamps(
			baseFloor, eye, view,
			lampPositions, lampColors, lampLevels,
			coldLightFactor, redLightFactor
		);
		glUniform1i(glGetUniformLocation(sceneProgram, "uLampCount"), lampCount);
		if (lampCount > 0)
		{
			glUniform3fv(glGetUniformLocation(sceneProgram, "uLampPosition"), lampCount, lampPositions);
			glUniform3fv(glGetUniformLocation(sceneProgram, "uLampColor"), lampCount, lampColors);
			glUniform1fv(glGetUniformLocation(sceneProgram, "uLampLevel"), lampCount, lampLevels);
		}

		drawFloors(sceneProgram, baseFloor, true);

		float faceAlpha = 1.0f;

		float loopLength = LOOP_FLOORS * FLOOR_HEIGHT;

		float faceLocalY = -FACE_DEPTH + wrapCount * loopLength;

		Vec3 facePosition = makeVec(
			FACE_X,
			faceLocalY,
			FACE_Z
		);

		Vec3 animatedFacePosition = facePosition;

		float faceScale = 1.0f;

		if (faceJumpScareActive)
		{
			float t = clampValue(faceAttackProgress, 0.0f, 1.0f);

			animatedFacePosition = add(
				faceAttackStart,
				scale(faceAttackDirection, 3.0f * t)
			);

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
			faceJumpScareTimer -= seconds;

			faceAttackProgress =
				1.0f - faceJumpScareTimer / FACE_ATTACK_DURATION;

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

			drawFace(
				faceProgram,
				faceVAO,
				faceVBO,
				faceTexture,
				eye,
				animatedFacePosition,
				view,
				projection,
				2.0f * faceScale,
				2.4f * faceScale,
				faceAlpha
			);

			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
		}

		// Swap the back buffer with the front buffer
		glfwSwapBuffers(window);
		// Take care of all GLFW events
		glfwPollEvents();
	}

	// Delete all the objects we've created
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteTextures(1, &concreteTexture);
	glDeleteTextures(1, &metalTexture);
	glDeleteTextures(1, &shadowTexture);
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteProgram(sceneProgram);
	glDeleteProgram(shadowProgram);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}