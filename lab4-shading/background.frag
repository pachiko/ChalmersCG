#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;
layout(binding = 6) uniform sampler2D environmentMap;
in vec2 texCoord; // We dont really need the texCoord here, more like the vertex position which is on near plane (both interpolated)
uniform mat4 inv_PV;
uniform vec3 camera_pos;
uniform float environment_multiplier;
#define PI 3.14159265359

void main()
{
	// Calculate the world-space position of this fragment on the near plane
	// xy from projected to camera (P-1), then world (V-1)
	// Same results using gl_FragCoord.xy. need to use viewport size (pass in as uniform) instead of environmentMap size
	// vec2 uv = gl_FragCoord.xy/vec2(1280, 720); // replace uv with texCoord
	vec4 pixel_world_pos = inv_PV * vec4(texCoord * 2.0 - 1.0, 1.0, 1.0); // [-1, 1] seems to work/give same result, even 0
	// Homogenize, always do this when using projection/inverse projection
	pixel_world_pos = (1.0 / pixel_world_pos.w) * pixel_world_pos;

	// Calculate the world-space direction from the camera to that position
	vec3 dir = normalize(pixel_world_pos.xyz - camera_pos);

	// Calculate the spherical coordinates of the direction
	float theta = acos(max(-1.0f, min(1.0f, dir.y))); // a smart way to avoid normalize using max, min
	float phi = atan(dir.z, dir.x); // start from x (pointing right), rotate towards z clockwise (facing yourself);
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}

	// Use these to lookup the color in the environment map
	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
	fragmentColor = environment_multiplier * texture(environmentMap, lookup);
}
