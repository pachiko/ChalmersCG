#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(binding = 0) uniform sampler2D colortexture;
uniform vec3 material_diffuse_color = vec3(0.0);
uniform vec3 material_emissive_color = vec3(0.0);
uniform int has_diffuse_texture = 1;

in vec2 texCoord;

layout(location = 0) out vec4 fragmentColor;

void main()
{
	//fragmentColor = texture(colortexture, texCoord.xy);
	fragmentColor = vec4(material_diffuse_color + material_emissive_color, 1.0);
	if(has_diffuse_texture == 1)
		fragmentColor = texture(colortexture, texCoord.xy);
}