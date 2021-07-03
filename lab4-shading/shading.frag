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

uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

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

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;


vec3 calculateDirectIllumiunation(vec3 wo, vec3 n, vec3 base_color)
{
	// vec3 direct_illum = base_color;
	///////////////////////////////////////////////////////////////////////////
	// Task 1.2 - Calculate the radiance Li from the light, and the direction
	//            to the light. If the light is backfacing the triangle,
	//            return vec3(0);
	///////////////////////////////////////////////////////////////////////////
	
	vec3 wi = viewSpaceLightPosition - viewSpacePosition;
	float d = length(wi);
	wi /= d;
	float nDotWi = dot(wi, n);
	if (nDotWi <= 0.f) return vec3(0.f);
	
	vec3 Li = point_light_intensity_multiplier * point_light_color / d / d;
		///////////////////////////////////////////////////////////////////////////
		// Task 1.3 - Calculate the diffuse term and return that as the result
		///////////////////////////////////////////////////////////////////////////
		vec3 diffuse_term = base_color / PI * nDotWi * Li;
		// return diffuse_term;
	///////////////////////////////////////////////////////////////////////////
	// Task 2 - Calculate the Torrance Sparrow BRDF and return the light
	//          reflected from that instead
	///////////////////////////////////////////////////////////////////////////
	vec3 wh = normalize(wi + wo); // no need to divide 2, normalize() will make the magnitude right!
	float nDotWh = dot(n, wh);
	if (material_shininess == 0.5f && nDotWh <= 0.f) return vec3(0.f);
	float nDotWo = dot(n, wo);
	if (nDotWo <= 0.f) return vec3(0.f);
	float woDotWh = dot(wo, wh);
	if (woDotWh <= 0.f) return vec3(0.f);

	float F = material_fresnel + (1.f - material_fresnel)*pow(1.f - dot(wh, wi), 5.f);
	float D = (material_shininess + 2.f)/2.f/PI*pow(nDotWh, material_shininess); // sqrt can cause NaN, if n.wh < 0
	float G = min(1.f, min(2.f*nDotWh*nDotWo/woDotWh, 2.f*nDotWh*nDotWi/woDotWh)); // division by 0
	float brdf = F*D*G/(4.f*nDotWo*nDotWi); // division by 0
	// return brdf*nDotWi*Li;

	// NB: there are no colors because base_color is not used. Only light color is used.
	// Materials with shininess = 0 are just diffuse, D = (0 + 2)/2/pi*pow(n.wh, 0) = 1/pi
	// The reason dielectrics are pure black is due to a lack of fresnel, whereas metal has some
	// so it can reflect some light

	///////////////////////////////////////////////////////////////////////////
	// Task 3 - Make your shader respect the parameters of our material model.
	///////////////////////////////////////////////////////////////////////////
	vec3 dielectric_term = brdf*nDotWi*Li + (1.f - F)*diffuse_term; // refracted light is reflected diffusely
	vec3 metal_term = brdf*material_color*nDotWi*Li; // reflected light of metal follows material color
	vec3 microfacet_term = material_metalness*metal_term + (1.f - material_metalness)*dielectric_term; // blend with metalness
	
	return material_reflectivity*microfacet_term + (1.f - material_reflectivity)*diffuse_term;
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n, vec3 base_color)
{
	// vec3 indirect_illum = vec3(0.f);
	///////////////////////////////////////////////////////////////////////////
	// Task 5 - Lookup the irradiance from the irradiance map and calculate
	//          the diffuse reflection
	///////////////////////////////////////////////////////////////////////////
	vec3 worldSpaceNormal = (viewInverse*vec4(n.xyz, 0.f)).xyz;
	float theta = acos(max(-1.f, min(1.f, worldSpaceNormal.y)));
	float phi = atan(worldSpaceNormal.z, worldSpaceNormal.x);

	if (phi < 0.f) {
		phi += 2.f*PI;
	}

	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
	vec3 irradiance  = (environment_multiplier * texture(irradianceMap, lookup)).xyz;
	vec3 diffuse_term = (base_color / PI * irradiance);
	
	///////////////////////////////////////////////////////////////////////////
	// Task 6 - Look up in the reflection map from the perfect specular
	//          direction and calculate the dielectric and metal terms.
	///////////////////////////////////////////////////////////////////////////

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
	vec3 metal_term = F*base_color*Li;
	vec3 microfacet_term = material_metalness*metal_term + (1.f - material_metalness)*dielectric_term;

	return material_reflectivity*microfacet_term + (1.f - material_reflectivity)*diffuse_term;
}


void main()
{
	///////////////////////////////////////////////////////////////////////////
	// Task 1.1 - Fill in the outgoing direction, wo, and the normal, n. Both
	//            shall be normalized vectors in view-space.
	///////////////////////////////////////////////////////////////////////////
	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	vec3 base_color = material_color;
	if(has_color_texture == 1)
	{
		base_color *= texture(colorMap, texCoord).xyz;
	}

	vec3 direct_illumination_term = vec3(0.0);
	{ // Direct illumination
		direct_illumination_term = calculateDirectIllumiunation(wo, n, base_color);
	}

	vec3 indirect_illumination_term = vec3(0.0);
	{ // Indirect illumination
		indirect_illumination_term = calculateIndirectIllumination(wo, n, base_color);
	}

	///////////////////////////////////////////////////////////////////////////
	// Task 1.4 - Make glowy things glow!
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;

	vec3 final_color = direct_illumination_term + indirect_illumination_term + emission_term;

	// Check if we got invalid results in the operations
	if(any(isnan(final_color)))
	{
		final_color.xyz = vec3(1.f, 0.f, 1.f);
	}

	fragmentColor.xyz = final_color;
}
