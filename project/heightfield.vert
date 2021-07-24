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
uniform float currentTime;
uniform bool water;

layout(binding = 13) uniform sampler2D heightField;

///////////////////////////////////////////////////////////////////////////////
// Output to fragment shader
///////////////////////////////////////////////////////////////////////////////
out vec2 texCoord;
out vec3 viewSpacePosition;
out vec3 viewSpaceNormal;

// Pseudo-RNG
float hash( float n ) { return fract(sin(n) * 753.5453123); } // fract(x) = x - floor(x);

// Pseudo-RNG
float noise(vec3 x )
{
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
	
    float n = p.x + p.y * 157.0 + 113.0 * p.z;
    return mix(mix(mix( hash(n +   0.0), hash(n +   1.0), f.x), // mix = linerp(a, b, t);
                   mix( hash(n + 157.0), hash(n + 158.0), f.x), f.y),
               mix(mix( hash(n + 113.0), hash(n + 114.0), f.x),
                   mix( hash(n + 270.0), hash(n + 271.0), f.x), f.y), f.z);
}

// Fractal Brownian Motion Noise
float fbm(vec3 pp){
    float f = 0.0;
    mat3 m = mat3( 0.00,  0.80,  0.60,
                  -0.80,  0.36, -0.48,
                  -0.60, -0.48,  0.64 ) * 2; // Rotate to reduce axial bias
    f  = 0.5000 * noise( pp ); pp = m*pp; // Octave 1
    f += 0.2500 * noise( pp ); pp = m*pp; // Octave 2, gain = 0.5
    f += 0.1250 * noise( pp ); pp = m*pp; // so on...
    f += 0.0625 * noise( pp ); pp = m*pp;
    f += 0.03125 * noise( pp ); pp = m*pp;
    f += 0.0150625 * noise( pp ); pp = m*pp;
    return f;
};

float waterHeight(float currentTime, vec2 uv)
{
	uv *= 50.0;

	//wave directions (not normalized)
	vec3 waveDir1 = vec3(1.0 / 1.5, 0.0, 1.0 / 8.0);
	vec2 waveDir2 = vec2(1.0 / 5.33, 0.911 / 5.33);
	vec2 waveDir3 = vec2(-1.0 / 1.79, 0.0);

	float f = 1.8 * fbm(currentTime * waveDir1 + vec3(uv / 4.0, 0)) - 0.5;
	float s0 = 0.5 * sin(currentTime * 1.7 + dot(waveDir2, uv));
	float s1 = 0.8 * sin(currentTime * 2.3 + dot(waveDir3, uv) + 3.3 );
	return (s0 + s1 + f) * 0.3;
}

void main()
{

	// heights
	float y;
	float topY;
	float botY;
	float leftY;
	float rightY;

	if (water) {
		y = waterHeight(currentTime, texCoordIn);
		topY = waterHeight(currentTime, texCoordIn + vec2(0.f, -uvSpacing));  // again with this -z
		botY = waterHeight(currentTime, texCoordIn + vec2(0.f, uvSpacing));
		leftY = waterHeight(currentTime, texCoordIn + vec2(-uvSpacing, 0.f));
		rightY = waterHeight(currentTime, texCoordIn + vec2(uvSpacing, 0.f));
	} else {
		y = texture(heightField, texCoordIn).r;
		topY = texture(heightField, texCoordIn + vec2(0.f, -uvSpacing)).r; // again with this -z
		botY = texture(heightField, texCoordIn + vec2(0.f, uvSpacing)).r;
		leftY = texture(heightField, texCoordIn + vec2(-uvSpacing, 0.f)).r;
		rightY = texture(heightField, texCoordIn + vec2(uvSpacing, 0.f)).r;
	}

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
