#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform int has_emission_texture;
uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap; // Not used after lab 5.
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
layout(binding = 9) uniform sampler2D ssaoMap;
layout(binding = 10) uniform sampler2DShadow shadowMapTex;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;
uniform vec3 viewSpaceLightDir;
uniform float spotOuterAngle;
uniform float spotInnerAngle;
uniform bool useSpotLight;
uniform bool useSoftFalloff;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;
in vec4 shadowMapCoord;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;
uniform bool useSSAO;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;



vec3 calculateDirectIllumiunation(vec3 wo, vec3 n)
{
	// Copy-pasta
	vec3 wi = viewSpaceLightPosition - viewSpacePosition;
	float d = length(wi);
	wi /= d;
	float nDotWi = dot(wi, n);
	if (nDotWi <= 0.f) return vec3(0.f);

	vec3 Li = point_light_intensity_multiplier * point_light_color / d / d;
	vec3 diffuse_term = material_color / PI * nDotWi * Li;

	vec3 wh = normalize(wi + wo);
	float nDotWh = dot(n, wh);
	if (material_shininess == 0.5f && nDotWh <= 0.f) return vec3(0.f);
	float nDotWo = dot(n, wo);
	if (nDotWo <= 0.f) return vec3(0.f);
	float woDotWh = dot(wo, wh);
	if (woDotWh <= 0.f) return vec3(0.f);

	float F = material_fresnel + (1.f - material_fresnel)*pow(1.f - dot(wh, wi), 5.f);
	float D = (material_shininess + 2.f)/2.f/PI*pow(nDotWh, material_shininess);
	float G = min(1.f, min(2.f*nDotWh*nDotWo/woDotWh, 2.f*nDotWh*nDotWi/woDotWh));
	float brdf = F*D*G/(4.f*nDotWo*nDotWi);

	vec3 dielectric_term = brdf*nDotWi*Li + (1.f - F)*diffuse_term;
	vec3 metal_term = brdf*material_color*nDotWi*Li;
	vec3 microfacet_term = material_metalness*metal_term + (1.f - material_metalness)*dielectric_term;

	return material_reflectivity*microfacet_term + (1.f - material_reflectivity)*diffuse_term;
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	// Copy-pasta
	vec3 worldSpaceNormal = (viewInverse*vec4(n.xyz, 0.f)).xyz;
	float theta = acos(max(-1.f, min(1.f, worldSpaceNormal.y)));
	float phi = atan(worldSpaceNormal.z, worldSpaceNormal.x);

	if (phi < 0.f) {
		phi += 2.f*PI;
	}

	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
	vec3 irradiance  = (environment_multiplier * texture(irradianceMap, lookup)).xyz;
	vec3 diffuse_term = (material_color / PI * irradiance);
	if (useSSAO) {
		diffuse_term = diffuse_term * texture(ssaoMap, gl_FragCoord.xy/textureSize(ssaoMap, 0)).x; // SSAO
	}

	vec3 wi = -reflect(wo, n);
	wi = (viewInverse*vec4(wi, 0.f)).xyz;

	theta = acos(max(-1.f, min(1.f, wi.y)));
	phi = atan(wi.z, wi.x);

	if (phi < 0.f) {
		phi += 2.f*PI;
	}

	lookup = vec2(phi / (2.0 * PI), theta / PI);
	float roughness = sqrt(sqrt(2.f/(material_shininess + 2.f)));
	vec3 Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0).xyz;

	vec3 wh = normalize(wi + wo);
	float F = material_fresnel + (1.f - material_fresnel)*pow(1.f - dot(wh, wi), 5.f);
	vec3 dielectric_term = F*Li + (1.f - F)*diffuse_term; // approx brdf with fresnel. nDotWi = 1
	vec3 metal_term = F*material_color*Li;
	vec3 microfacet_term = material_metalness*metal_term + (1.f - material_metalness)*dielectric_term;

	return material_reflectivity*microfacet_term + (1.f - material_reflectivity)*diffuse_term;
}

void main()
{
	float visibility = 1.0;
	float attenuation = 1.0;

	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	visibility = textureProj( shadowMapTex, shadowMapCoord ); // texture lookup WITH projection

	// Use Spotlight
	if (useSpotLight) {
		vec3 posToLight = normalize(viewSpaceLightPosition - viewSpacePosition);
		float cosAngle = dot(posToLight, -viewSpaceLightDir);

		float spotAttenuation;
		if (useSoftFalloff) {
			spotAttenuation = smoothstep(spotOuterAngle, spotInnerAngle, cosAngle);
		} else { // Spotlight with hard border:
			spotAttenuation = (cosAngle > spotOuterAngle) ? 1.0 : 0.0;
		}
				 
		visibility *= spotAttenuation;
	}

	// Direct illumination
	vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;
	if(has_emission_texture == 1)
	{
		emission_term = texture(emissiveMap, texCoord).xyz;
	}

	vec3 shading = direct_illumination_term + indirect_illumination_term + emission_term;

	fragmentColor = vec4(shading, 1.0);
	// fragmentColor = vec4(1.0); // test reload shaders on-the-fly
	return;
}
