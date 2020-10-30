#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Task 4: Add an input variable for colors from the vertex shader, and set
//         the output color to be the incoming interpolated color.
///////////////////////////////////////////////////////////////////////////////

layout(location = 0) out vec4 fragmentColor;

void main()
{
	fragmentColor = vec4(1.0, 1.0, 1.0, 1.0);
}