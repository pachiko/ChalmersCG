#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

#define MAX_SSAO 300
#define PI 3.1415926538

///////////////////////////////////////////////////////////////////////////////
// Textures
///////////////////////////////////////////////////////////////////////////////
// Don't use unit 0, used by shading.frag (colorMap)
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D depthMap;
layout(binding = 4) uniform sampler2D rotMap;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 inverseProjectionMatrix;
uniform mat4 projectionMatrix;
uniform vec3 ssaoSamples[MAX_SSAO];
uniform int numSSAO;
uniform float hemiR;
uniform bool useRot;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;

// Perspective Divide
vec3 homogenize(vec4 v) { return vec3((1.0 / v.w) * v); }


// Computes one vector in the plane perpendicular to v
vec3 perpendicular(vec3 v)
{
	vec3 av = abs(v); 
	if (av.x < av.y)
		if (av.x < av.z) return vec3(0.0f, -v.z, v.y);
		else return vec3(-v.y, v.x, 0.0f);
	else
		if (av.y < av.z) return vec3(-v.z, 0.0f, v.x);
		else return vec3(-v.y, v.x, 0.0f);
}

// Generate Quarternion for rotation
vec4 quarternion(vec3 axis, float angle) {
	float halfAngle = angle/2.0;
	float s = sin(halfAngle);
	float c = cos(halfAngle);
	return vec4(normalize(axis)*s, c);
}


vec3 applyQuarternion(vec4 q, vec3 v) {
	return v + 2.0*cross(cross(v, q.xyz ) + q.w*v, q.xyz);
}

void main() {
	vec2 texCoord = gl_FragCoord.xy/textureSize(depthMap, 0); // Both depth and normal maps have same dimensions
	float fragmentDepth = texture(depthMap, texCoord).x; // why x, why r? makes no difference!

	// Normalized Device Coordinates (clip space)
	vec4 ndc = vec4(texCoord.x * 2.0 - 1.0, texCoord.y * 2.0 - 1.0, 
					fragmentDepth * 2.0 - 1.0, 1.0);

	// Transform to view space
	vec3 vs_pos = homogenize(inverseProjectionMatrix * ndc);

	vec3 vs_normal = 2.0*texture(normalMap, texCoord).xyz - 1.0; // from [0, 1] to [-1, 1]
	vec3 vs_tangent = perpendicular(vs_normal);

	if (useRot) {
		float angle = 2.0*PI*texture(rotMap, gl_FragCoord.xy/textureSize(rotMap, 0)).x; // random rotation for each fragment
		vec4 q = quarternion(vs_normal, angle); // rotate about normal using quarternion
		vs_tangent = applyQuarternion(q, vs_tangent); // rotate tangent
	}

	vec3 vs_bitangent = cross(vs_normal, vs_tangent);

	mat3 tbn = mat3(vs_tangent, vs_bitangent, vs_normal); // local base

	int num_visible_samples = 0; 
	int num_valid_samples = 0; 
	for (int i = 0; i < numSSAO; i++) {
		// Project hemishere sample onto the local base
		vec3 s = tbn * ssaoSamples[i];

		// compute view-space position of sample
		vec3 vs_sample_position = vs_pos + s * hemiR ;

		// compute the ndc-coords of the sample
		vec3 sample_coords_ndc = homogenize(projectionMatrix * vec4(vs_sample_position, 1.0));

		// Sample the depth-buffer at a texture coord based on the ndc-coord of the sample
		vec2 sample_texCoord = (sample_coords_ndc.xy + 1.0)/2.0;
		float blocker_depth = texture(depthMap, sample_texCoord).x;

		// Find the view-space coord of the blocker
		vec3 vs_blocker_pos = homogenize(inverseProjectionMatrix * 
			 vec4(sample_coords_ndc.xy, blocker_depth * 2.0 - 1.0, 1.0));	

		// Check that the blocker is closer than hemisphere_radius to vs_pos
		// (otherwise skip this sample)
		if (distance(vs_blocker_pos, vs_pos) > hemiR) continue;

		// Check if the blocker pos is closer to the camera than our
		// fragment, otherwise, increase num_visible_samples
		if (length(vs_blocker_pos) + 1e-3 > length(vs_pos)) num_visible_samples += 1;

		num_valid_samples += 1;
	}

	float hemisphericalVisibility = float(num_visible_samples) / float(num_valid_samples);

	if (num_valid_samples == 0)
		hemisphericalVisibility = 1.0;

	fragmentColor = vec4(vec3(hemisphericalVisibility), 1.0);
	// fragmentColor = vec4(fragmentDepth); // debug depth
	// fragmentColor = vec4(vs_normal, 1.0); // debug normals
	// fragmentColor = vec4(vec3(angle), 1.0); // debug random rotations that are repeated
	return;
}