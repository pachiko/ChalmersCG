#version 420
// This vertex shader simply outputs the input coordinates to the rasterizer. It only uses 2D coordinates.
layout(location = 0) in vec2 position; // [-1, 1] in x, y

out vec2 texCoord;  // what is really needed is the (interpolated) vertex position on near plane

void main()
{
	gl_Position = vec4(position, 0.0, 1.0);  // in clip-space, hence why we use MVP for model vertices (not homogenized yet)
	texCoord = 0.5 * (position + vec2(1, 1));
}