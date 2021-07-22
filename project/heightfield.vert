#version 420
///////////////////////////////////////////////////////////////////////////////
// Input vertex attributes
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) in vec3 position;
layout(location = 2) in vec2 texCoordIn;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////

uniform mat4 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform float uvSpacing;

layout(binding = 13) uniform sampler2D heightField;

///////////////////////////////////////////////////////////////////////////////
// Output to fragment shader
///////////////////////////////////////////////////////////////////////////////
out vec2 texCoord;
out vec3 viewSpacePosition;
out vec3 viewSpaceNormal;


void main()
{

	// heights
	float y = texture(heightField, texCoordIn).r;
	float topY = texture(heightField, texCoordIn + vec2(0.f, -uvSpacing)).r; // again with this -z
	float botY = texture(heightField, texCoordIn + vec2(0.f, uvSpacing)).r;
	float leftY = texture(heightField, texCoordIn + vec2(-uvSpacing, 0.f)).r;
	float rightY = texture(heightField, texCoordIn + vec2(uvSpacing, 0.f)).r;

	vec3 pos = position.xyz;
	pos.y = y; // cannot swizzle in variables (read-only)
	
	// positions
	vec3 topVert = normalize(vec3(0.f, topY, -uvSpacing));  // again with this -z
	vec3 botVert = normalize(vec3(0.f, botY, uvSpacing));
	vec3 leftVert = normalize(vec3(-uvSpacing, leftY, 0.f));
	vec3 rightVert = normalize(vec3(uvSpacing, rightY, 0.f));

	// normals of each quad surrounding vertex
	vec3 topLeftNorm = normalize(cross(topVert, leftVert));
	vec3 leftBotNorm = normalize(cross(leftVert, botVert));
	vec3 botRightNorm = normalize(cross(botVert, rightVert));
	vec3 rightTopNorm = normalize(cross(rightVert, topVert));

	vec3 normal = normalize(topLeftNorm + leftBotNorm + botRightNorm + rightTopNorm);

	viewSpacePosition = (modelViewMatrix * vec4(pos, 1.0)).xyz;
	viewSpaceNormal = (normalMatrix * vec4(normal, 0.0)).xyz;
	gl_Position = modelViewProjectionMatrix * vec4(pos, 1.0);
	texCoord = texCoordIn;
}
