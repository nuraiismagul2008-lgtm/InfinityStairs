layout (location = 0) in vec3 vPosition;
uniform mat4 uModel;
uniform mat4 uLightVP;
void main()
{
	gl_Position = uLightVP * uModel * vec4(vPosition, 1.0);
}
