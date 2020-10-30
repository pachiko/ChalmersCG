#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
///////////////////////////////////////////////////////////////////////////////
// Task 3: Add an output variable for colors for the fragment shader, and set
//         it to the vertex color
///////////////////////////////////////////////////////////////////////////////

void main()
{
	gl_Position = vec4(position, 1.0);
}