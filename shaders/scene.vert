layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in float vMaterial;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightVP;
uniform vec3 uLightWorldPosition;
uniform float uNormalOffset;
uniform float uShadowTexelScale;
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

	// Normal offset: the point is moved a little along its normal before the shadow test, so the surface does not
	// shadow itself. The shadow texels get bigger with the distance, so the offset also gets bigger
	float lightDistance = length(worldPosition.xyz - uLightWorldPosition);
	vec3 shadowPoint = worldPosition.xyz + vNormal * (uNormalOffset * uShadowTexelScale * lightDistance);
	fShadowCoord = uLightVP * vec4(shadowPoint, 1.0);
	gl_Position = uProjection * eyePosition;
}
