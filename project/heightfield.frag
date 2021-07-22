#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
layout(binding = 12) uniform sampler2D diffuseMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform float terrain_reflectivity;
uniform float terrain_metalness;
uniform float terrain_fresnel;
uniform float terrain_shininess;
uniform bool dbgTerrainNormal;
uniform bool dbgTerrainMesh;

///////////////////////////////////////////////////////////////////////////////
// From vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpacePosition;
in vec3 viewSpaceNormal;

///////////////////////////////////////////////////////////////////////////////
// Outputs
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;


vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	vec3 worldSpaceNormal = (viewInverse*vec4(n.xyz, 0.f)).xyz;
	float theta = acos(max(-1.f, min(1.f, worldSpaceNormal.y)));
	float phi = atan(worldSpaceNormal.z, worldSpaceNormal.x);

	if (phi < 0.f) {
		phi += 2.f*PI;
	}

	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
	vec3 irradiance  = (environment_multiplier * texture(irradianceMap, lookup)).xyz;
	vec3 terrain_color = texture(diffuseMap, texCoord).xyz;
	vec3 diffuse_term = (terrain_color / PI * irradiance);


	vec3 wi = -reflect(wo, n);
	wi = (viewInverse*vec4(wi, 0.f)).xyz;

	theta = acos(max(-1.f, min(1.f, wi.y)));
	phi = atan(wi.z, wi.x);

	if (phi < 0.f) {
		phi += 2.f*PI;
	}

	lookup = vec2(phi / (2.0 * PI), theta / PI);
	float roughness = sqrt(sqrt(2.f/(terrain_shininess + 2.f)));
	vec3 Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0).xyz;

	vec3 wh = normalize(wi + wo);
	float F = terrain_fresnel + (1.f - terrain_fresnel)*pow(1.f - dot(wh, wi), 5.f);
	vec3 dielectric_term = F*Li + (1.f - F)*diffuse_term; // approx brdf with fresnel. nDotWi = 1
	vec3 metal_term = F*terrain_color*Li;
	vec3 microfacet_term = terrain_metalness*metal_term + (1.f - terrain_metalness)*dielectric_term;

	return terrain_reflectivity*microfacet_term + (1.f - terrain_reflectivity)*diffuse_term;
}

void main()
{
	if (dbgTerrainNormal) {
		fragmentColor = vec4(viewSpaceNormal, 1.0);
	} else if (dbgTerrainMesh) {
		fragmentColor = vec4(1.0);
	} else {
		vec3 wo = -normalize(viewSpacePosition);
		vec3 n = normalize(viewSpaceNormal);

		fragmentColor = vec4(calculateIndirectIllumination(wo, n), 1.f);
	}
}
