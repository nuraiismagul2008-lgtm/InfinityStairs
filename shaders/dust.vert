layout (location = 0) in vec3 pWorld;
layout (location = 1) in vec2 pData;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightVP;
uniform float uPointScale;
out vec3 fPositionEye;
out float fAlpha;
out vec4 fShadowCoord;
void main()
{
	vec4 eyePosition = uView * vec4(pWorld, 1.0);
	fPositionEye = eyePosition.xyz;
	fAlpha = pData.y;
	fShadowCoord = uLightVP * vec4(pWorld, 1.0);
	gl_Position = uProjection * eyePosition;
	// The size in pixels is the size in meters divided by the distance
	float dist = max(-eyePosition.z, 0.1);
	gl_PointSize = clamp(pData.x * uPointScale / dist, 1.0, 14.0);
}
