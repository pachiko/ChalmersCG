#version 420

layout(location = 0) in vec3 position;
uniform mat4 projectionMatrix;
uniform vec3 cameraPosition;

// >>> @task 3.2

void main()
{
	vec4 pos = vec4(position.xyz - cameraPosition.xyz, 1);
	gl_Position = projectionMatrix * pos;

	// >>> @task 3.3
}