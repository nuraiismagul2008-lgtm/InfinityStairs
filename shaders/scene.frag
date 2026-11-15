in vec3 fPositionEye;
in vec3 fNormalEye;
in vec2 fUV;
flat in float fMaterial;
in vec4 fShadowCoord;
out vec4 FragColor;

uniform sampler2D uTexConcrete;
uniform sampler2D uTexMetal;
uniform bool uUseTexture;
uniform bool uFogScatter;
uniform int uDebugView;

// The lamps are point lights without shadows
uniform int uLampCount;
uniform vec3 uLampPosition[4];
uniform vec3 uLampColor[4];
uniform float uLampLevel[4];

// The glow of the body of the lamp of the floor that is drawn now
uniform float uGlowLevel;
uniform vec3 uGlowColor;

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

void main()
{
	// Both textures are always read (it is safe for the mipmap levels), the material chooses one of them
	vec3 texConcrete = texture(uTexConcrete, fUV).rgb;
	vec3 texMetal = texture(uTexMetal, fUV).rgb;
	vec3 N = normalize(fNormalEye);
	vec3 V = normalize(-fPositionEye);
	vec3 color;

	// Numbers for the debug views
	float debugShadow = 1.0;
	float debugLight = 0.0;
	float flashAmount = 0.0;

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
			float spot = spotFactor(L);
			float attenuation = flashAttenuation(d);
			float shadow = shadowFactor(fShadowCoord, max(dot(N, L), 0.0));
			flashAmount = uFlashIntensity * attenuation * spot;
			debugShadow = shadow;
			debugLight = flashAmount * max(dot(N, L), 0.0) * shadow;
			color += blinnPhong(N, V, L, albedo, specularColor, shininess) * uFlashColor * (flashAmount * shadow);
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

	// Fog: the farther the fragment, the more it gets the color of the fog
	float visibility = 1.0;
	if (uUseFog)
	{
		visibility = fogVisibility(length(fPositionEye));
		vec3 fogColor = uFogColor;
		// Light scattering: the fog shines a little where the cone of the flashlight goes through it
		if (uFogScatter && uFlashlightOn)
		{
			fogColor += uFlashColor * (0.12 * flashAmount);
		}
		color = mix(fogColor, color, visibility);
	}

	// Debug views: only one number is shown as gray
	if (uDebugView == 1)
	{
		float inCone = (flashAmount > 0.0) ? 1.0 : 0.15;
		FragColor = vec4(vec3(debugShadow * inCone), 1.0);
		return;
	}
	if (uDebugView == 2)
	{
		FragColor = vec4(vec3(clamp(debugLight, 0.0, 1.0)), 1.0);
		return;
	}
	if (uDebugView == 3)
	{
		FragColor = vec4(vec3(visibility), 1.0);
		return;
	}

	// Global brightness (it is used by the events), a too bright pixel is cut to 1
	color *= uSceneBrightness;
	color = min(color, vec3(1.0));

	// Back to the sRGB space
	color = pow(color, vec3(1.0 / 2.2));
	FragColor = vec4(color, 1.0);
}
