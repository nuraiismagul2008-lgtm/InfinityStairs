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
