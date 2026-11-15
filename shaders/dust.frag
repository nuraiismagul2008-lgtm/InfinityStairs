in vec3 fPositionEye;
in float fAlpha;
in vec4 fShadowCoord;
out vec4 FragColor;
uniform float uSceneBrightness;
void main()
{
	// A soft round point
	vec2 c = gl_PointCoord * 2.0 - 1.0;
	float r2 = dot(c, c);
	if (r2 > 1.0)
	{
		discard;
	}
	float brightness = 0.0;
	if (uFlashlightOn)
	{
		vec3 toLight = uFlashPosition - fPositionEye;
		float d = length(toLight);
		brightness = spotFactor(toLight / d) * flashAttenuation(d) * uFlashIntensity * shadowFactor(fShadowCoord, 1.0);
	}
	float distanceToEye = length(fPositionEye);
	float visibility = 1.0;
	if (uUseFog)
	{
		visibility = fogVisibility(distanceToEye);
	}
	// The dust very close to the eye fades out, otherwise it would be a big spot
	float nearFade = smoothstep(0.3, 1.0, distanceToEye);
	float alpha = (1.0 - r2) * fAlpha * nearFade * visibility * clamp(brightness, 0.0, 1.0);
	vec3 color = pow(uFlashColor * uSceneBrightness, vec3(1.0 / 2.2));
	FragColor = vec4(color, alpha);
}
