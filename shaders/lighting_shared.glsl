// Shadow map: a depth texture with the hardware depth compare
uniform sampler2DShadow uShadowMap;
uniform bool uUseShadow;
uniform int uPcfRadius;
uniform float uShadowBias;

// The flashlight is a spotlight, its position and direction are in the eye coordinates
uniform bool uFlashlightOn;
uniform vec3 uFlashPosition;
uniform vec3 uFlashDirection;
uniform float uFlashInner;
uniform float uFlashOuter;
uniform float uFlashIntensity;
uniform float uSpotExponent;
uniform vec3 uFlashAttenuation;
uniform vec3 uFlashColor;

// Fog
uniform bool uUseFog;
uniform int uFogMode;
uniform float uFogDensity;
uniform vec3 uFogColor;

// Spotlight: 0 outside the outer cone, 1 inside the inner cone, between them a ramp that is made sharper by the exponent
// L is the direction from the fragment to the light
float spotFactor(vec3 L)
{
	float theta = dot(-L, uFlashDirection);
	float ramp = clamp((theta - uFlashOuter) / (uFlashInner - uFlashOuter), 0.0, 1.0);
	return pow(ramp, uSpotExponent);
}

// Distance term 1 / (a + b * d + c * d * d)
float flashAttenuation(float d)
{
	return 1.0 / (uFlashAttenuation.x + uFlashAttenuation.y * d + uFlashAttenuation.z * d * d);
}

// Shadow mapping: compares the depth of the fragment with the depth that the flashlight has seen
// With PCF the neighbour texels are also used, every lookup is already filtered by the hardware
// The result is 1 in the light and 0 in the shadow
float shadowFactor(vec4 shadowCoord, float cosTheta)
{
	if (!uUseShadow || shadowCoord.w <= 0.0)
	{
		return 1.0;
	}
	vec3 p = shadowCoord.xyz / shadowCoord.w;
	p = p * 0.5 + 0.5;
	if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
	{
		return 1.0;
	}
	// The bias is bigger on the surfaces that are almost parallel to the light rays
	float bias = uShadowBias * (1.0 + 2.0 * (1.0 - cosTheta));
	vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
	float lit = 0.0;
	for (int x = -uPcfRadius; x <= uPcfRadius; x++)
	{
		for (int y = -uPcfRadius; y <= uPcfRadius; y++)
		{
			lit += textureLod(uShadowMap, vec3(p.xy + vec2(x, y) * texel, p.z - bias), 0.0);
		}
	}
	float taps = float((2 * uPcfRadius + 1) * (2 * uPcfRadius + 1));
	return lit / taps;
}

// Fog: the visibility is 1 near the camera and goes to 0 far away. The color of the pixel is
// visibility * color + (1 - visibility) * fog color
float fogVisibility(float d)
{
	if (uFogMode == 0)
	{
		// Linear (depth cueing)
		float fogEnd = 2.5 / uFogDensity;
		float fogStart = 0.15 / uFogDensity;
		return clamp((fogEnd - d) / (fogEnd - fogStart), 0.0, 1.0);
	}
	if (uFogMode == 1)
	{
		// Exponential
		return exp(-d * uFogDensity);
	}
	// Gaussian
	float x = d * uFogDensity;
	return exp(-x * x);
}
