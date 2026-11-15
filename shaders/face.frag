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
