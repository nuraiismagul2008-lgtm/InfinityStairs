layout (location = 0) in vec3 vPosition;

uniform vec2 uScreen;   // size of the window in pixels
uniform float uScale;   // size of one font pixel on the screen

void main()
{
	vec2 p = vPosition.xy * uScale;
	gl_Position = vec4(p.x / uScreen.x * 2.0 - 1.0, 1.0 - p.y / uScreen.y * 2.0, 0.0, 1.0);
}
