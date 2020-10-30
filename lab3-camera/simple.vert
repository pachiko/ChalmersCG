#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoordIn; // incoming texcoord from the texcoord array

out vec2 texCoord; // outgoing interpolated texcoord to fragshader

uniform mat4 modelViewProjectionMatrix;

void main()
{
	gl_Position = modelViewProjectionMatrix * vec4(position, 1.0);
	texCoord = texCoordIn;
}