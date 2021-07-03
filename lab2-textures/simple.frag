#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;
in vec3 outColor;
in vec2 texCoord;

// >>> @task 3.4
layout(binding = 0) uniform sampler2D colortexture; // texture unit 0
layout(location = 0) out vec4 fragmentColor;

void main()
{
	// >>> @task 3.5
	fragmentColor = texture2D(colortexture, texCoord.xy);
	// fragmentColor = vec4(texCoord.x, texCoord.y, 0.0, 0.0); 
	// fragmentColor = vec4(1.0, 0.5, 0.25, 1.0);
}