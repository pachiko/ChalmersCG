

#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"


#define PI 3.14159265359
#define MAX_SSAO 300

using std::min;
using std::max;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
bool showUI = false;
int windowWidth, windowHeight;
// SSAO
bool useSSAO = true;
int numSSAO = 16;
float hemiR = 1.f;
std::vector<vec3> ssaoSamples(numSSAO);
int rotMapDimension = 64;
bool visSSAO = false;
bool blurSSAO = true;
bool useRot = true;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;
bool g_isMouseRightDragging = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
// SSAO
GLuint depthNormalProgram; // Shader used to generate depth and normals
GLuint ssaoProgram; // Shader to compute SSAO
GLuint hBlurProgram;
GLuint vBlurProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";
// SSAO
GLuint rotationMap;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
float lightRotation = 0.f;
bool lightManualOnly = false;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);
bool useSpotLight = true;
float innerSpotlightAngle = 17.5f;
float outerSpotlightAngle = 22.5f;
float point_light_intensity_multiplier = 10000.0f;


///////////////////////////////////////////////////////////////////////////////
// Shadow map
///////////////////////////////////////////////////////////////////////////////
enum ClampMode
{
	Edge = 1,
	Border = 2
};

FboInfo shadowMapFB(1);
int shadowMapResolution = 128;
int shadowMapClampMode = ClampMode::Edge;
bool shadowMapClampBorderShadowed = true;
bool usePolygonOffset = true;
bool useSoftFalloff = true;
bool useHardwarePCF = true;
float polygonOffset_factor = 1.0f;
float polygonOffset_units = 1.0f;

// Depth and Normal FrameBuffer for SSAO
FboInfo depthNormalFB(1);
// SSAO FrameBuffer
FboInfo ssaoFB(1);
// SSAO Blur FrameBuffers
FboInfo blurFB(1);


///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 10.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* fighterModel = nullptr;
labhelper::Model* landingpadModel = nullptr;
labhelper::Model* sphereModel = nullptr;

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;

void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag",
	                                             is_reload);
	if(shader != 0)
		simpleShaderProgram = shader;
	shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag",
	                                      is_reload);
	if(shader != 0)
		backgroundProgram = shader;
	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
		shaderProgram = shader;

	shader = labhelper::loadShaderProgram("../project/ssaoInput.vert", "../project/ssaoInput.frag");
	if (shader != 0)
		depthNormalProgram = shader;

	shader = labhelper::loadShaderProgram("../project/ssaoOutput.vert", "../project/ssaoOutput.frag");
	if (shader != 0)
		ssaoProgram = shader;

	shader = labhelper::loadShaderProgram("../project/ssaoOutput.vert", "../project/horizontal_blur.frag");
	if (shader != 0)
		hBlurProgram = shader;

	shader = labhelper::loadShaderProgram("../project/ssaoOutput.vert", "../project/vertical_blur.frag");
	if (shader != 0)
		vBlurProgram = shader;
}


void generateSamples() {
	// Generate uniform samples for SSAO
	ssaoSamples.clear();
	for (int i = 0; i < numSSAO; i++) {
		ssaoSamples.push_back(labhelper::randf() * labhelper::cosineSampleHemisphere());
	} // Remember to bind as uniform to the shader!
}


void generateRotMap() {
	// SSAO rotation map;
	int numAngles = rotMapDimension * rotMapDimension;
	float* angles = new float[numAngles];
	for (int i = 0; i < numAngles; i++) {
		angles[i] = float(labhelper::randf());
	}
	glGenTextures(1, &rotationMap);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, rotationMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, rotMapDimension, rotMapDimension, 0, GL_RED, GL_FLOAT, angles);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Don't forget these
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	delete[] angles;
}


void initGL()
{
	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	fighterModel = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");
	sphereModel = labhelper::loadModelFromOBJ("../scenes/sphere.obj");

	roomModelMatrix = mat4(1.0f);
	fighterModelMatrix = translate(15.0f * worldUp);
	landingPadModelMatrix = mat4(1.0f);

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	const int roughnesses = 8;
	std::vector<std::string> filenames;
	for(int i = 0; i < roughnesses; i++)
		filenames.push_back("../scenes/envmaps/" + envmap_base_name + "_dl_" + std::to_string(i) + ".hdr");

	reflectionMap = labhelper::loadHdrMipmapTexture(filenames);
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");
	irradianceMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");
	
	///////////////////////////////////////////////////////////////////////
	// SSAO
	///////////////////////////////////////////////////////////////////////
	generateSamples();
	generateRotMap();

	///////////////////////////////////////////////////////////////////////
	// Setup Framebuffer for shadow map rendering
	///////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE0);
	shadowMapFB.resize(shadowMapResolution, shadowMapResolution);
	glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL); // <=
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // PCF. Default: GL_NEAREST_MIPMAP_LINEAR. 2 mipmaps, nearest lookup, weighted average
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling
}

void debugDrawLight(const glm::mat4& viewMatrix,
                    const glm::mat4& projectionMatrix,
                    const glm::vec3& worldSpaceLightPos)
{
	mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * modelMatrix);
	labhelper::render(sphereModel);
}


void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}


void drawScene(GLuint currentShaderProgram,
               const mat4& viewMatrix,
               const mat4& projectionMatrix,
               const mat4& lightViewMatrix,
               const mat4& lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);
	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
	                          point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
	                          normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));
	labhelper::setUniformSlow(currentShaderProgram, "useSpotLight", useSpotLight);
	labhelper::setUniformSlow(currentShaderProgram, "useSoftFalloff", useSoftFalloff);
	labhelper::setUniformSlow(currentShaderProgram, "spotOuterAngle", std::cos(radians(outerSpotlightAngle)));
	labhelper::setUniformSlow(currentShaderProgram, "spotInnerAngle", std::cos(radians(innerSpotlightAngle)));
	mat4 lightMatrix = translate(vec3(0.5f)) * scale(vec3(0.5f)) *
		lightProjectionMatrix * lightViewMatrix * inverse(viewMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "lightMatrix", lightMatrix);

	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "useSSAO", useSSAO);
	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	// landing pad
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * landingPadModelMatrix)));

	labhelper::render(landingpadModel);

	// Fighter
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * fighterModelMatrix)));

	labhelper::render(fighterModel);
}


void display(void)
{
	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}


	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 2000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	float light_rotation_speed = 1.f;
	if (!lightManualOnly && !g_isMouseRightDragging)
	{
		lightRotation += deltaTime * light_rotation_speed;
	}
	lightPosition = vec3(rotate(lightRotation, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);


	///////////////////////////////////////////////////////////////////////////
	// SSAO
	///////////////////////////////////////////////////////////////////////////

	if (useSSAO) {
		// Draw depth and normal map for SSAO
		if (depthNormalFB.width != windowWidth || depthNormalFB.height != windowHeight) {
			depthNormalFB.resize(windowWidth, windowHeight);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, depthNormalFB.framebufferId);
		glViewport(0, 0, windowWidth, windowHeight);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		drawScene(depthNormalProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);

		// Compute SSAO
		if (ssaoFB.width != windowWidth || ssaoFB.height != windowHeight) {
			ssaoFB.resize(windowWidth, windowHeight);
		}

		// Init
		if (visSSAO && !blurSSAO) glBindFramebuffer(GL_FRAMEBUFFER, 0); // visualize SSAO
		else glBindFramebuffer(GL_FRAMEBUFFER, ssaoFB.framebufferId);
		glViewport(0, 0, windowWidth, windowHeight);
		glClearColor(0.2, 0.2, 0.8, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Shader
		glUseProgram(ssaoProgram);
		labhelper::setUniformSlow(ssaoProgram, "numSSAO", numSSAO); // number of SSAO samples
		labhelper::setUniformSlow(ssaoProgram, "ssaoSamples", numSSAO, &ssaoSamples[0]); // SSAO samples in vec3 array
		labhelper::setUniformSlow(ssaoProgram, "inverseProjectionMatrix", inverse(projMatrix));
		labhelper::setUniformSlow(ssaoProgram, "projectionMatrix", projMatrix);
		labhelper::setUniformSlow(ssaoProgram, "hemiR", hemiR);
		labhelper::setUniformSlow(ssaoProgram, "useRot", useRot);

		// Textures
		glActiveTexture(GL_TEXTURE2); // Normal map
		glBindTexture(GL_TEXTURE_2D, depthNormalFB.colorTextureTargets[0]); // Will these go out-of-bounds? no, since it's windowSize
		glActiveTexture(GL_TEXTURE3); // Depth Buffer
		glBindTexture(GL_TEXTURE_2D, depthNormalFB.depthBuffer);
		glActiveTexture(GL_TEXTURE4); // Rotation Map
		glBindTexture(GL_TEXTURE_2D, rotationMap);

		labhelper::drawFullScreenQuad(); // Post-process

		if (blurSSAO) {
			if (blurFB.width != windowWidth || blurFB.height != windowHeight) {
				blurFB.resize(windowWidth, windowHeight);
			}

			// horizontal blur using SSAO texture. Blur shader uses unit 11 as input
			glActiveTexture(GL_TEXTURE11);
			glBindTexture(GL_TEXTURE_2D, ssaoFB.colorTextureTargets[0]);
			// Don't write to the same buffer!
			glBindFramebuffer(GL_FRAMEBUFFER, blurFB.framebufferId);
			glViewport(0, 0, windowWidth, windowHeight);
			glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glUseProgram(hBlurProgram);
			labhelper::drawFullScreenQuad();
			// vertical blur using horizontal blur texture. Unit 11 as input again.
			glUseProgram(vBlurProgram);
			glActiveTexture(GL_TEXTURE11);
			glBindTexture(GL_TEXTURE_2D, blurFB.colorTextureTargets[0]);
			// Write back to SSAO buffer
			if (visSSAO) glBindFramebuffer(GL_FRAMEBUFFER, 0); // visualize SSAO
			else glBindFramebuffer(GL_FRAMEBUFFER, ssaoFB.framebufferId);
			labhelper::drawFullScreenQuad();
		}

		if (visSSAO) return;
	}


	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, irradianceMap);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, reflectionMap);
	if (useSSAO) {
		// Warnings will be given using if-statement and useSSAO=false at first
		// OpenGL does not like unused samplers.
		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, ssaoFB.colorTextureTargets[0]);
	}
	glActiveTexture(GL_TEXTURE0);

	///////////////////////////////////////////////////////////////////////////
	// Set up shadow map parameters
	///////////////////////////////////////////////////////////////////////////
	if (shadowMapFB.width != shadowMapResolution || shadowMapFB.height != shadowMapResolution) {
		shadowMapFB.resize(shadowMapResolution, shadowMapResolution);
	}
	// It is NOT the monitor's shadow, since the monitor is never seen by the light
	if (shadowMapClampMode == ClampMode::Edge) {
		glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if (shadowMapClampMode == ClampMode::Border) {
		glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		vec4 border(shadowMapClampBorderShadowed ? 0.f : 1.f);
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &border.x);
	}

	///////////////////////////////////////////////////////////////////////////
	// Draw Shadow Map
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFB.framebufferId);
	glViewport(0, 0, shadowMapResolution, shadowMapResolution);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (usePolygonOffset) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(polygonOffset_factor, polygonOffset_units);
	}
	drawScene(simpleShaderProgram, lightViewMatrix, lightProjMatrix, lightViewMatrix, lightProjMatrix);
	//labhelper::Material& screen = landingpadModel->m_materials[8]; // Show light's view on landing pad's screen
	//screen.m_emission_texture.gl_id = shadowMapFB.colorTextureTargets[0];
	glActiveTexture(GL_TEXTURE10); // Bind Shadow Map Texture
	glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
	if (usePolygonOffset) {
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawBackground(viewMatrix, projMatrix);
	drawScene(shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);
	debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));
}

bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			showUI = !showUI;
		}
		if(event.type == SDL_MOUSEBUTTONDOWN  
			&& (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)
			&& (!showUI || !ImGui::GetIO().WantCaptureMouse)
			&& !(g_isMouseDragging || g_isMouseRightDragging))
		{
			if (event.button.button == SDL_BUTTON_LEFT)
			{
				g_isMouseDragging = true;
			}
			else if (event.button.button == SDL_BUTTON_RIGHT)
			{
				g_isMouseRightDragging = true;
			}
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}
		if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)))
		{
			g_isMouseRightDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			if (g_isMouseDragging)
			{
				float rotation_speed = 0.005f;
				mat4 yaw = rotate(rotation_speed * -delta_x, worldUp);
				mat4 pitch = rotate(rotation_speed * -delta_y, normalize(cross(cameraDirection, worldUp)));
				cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			}
			else if (g_isMouseRightDragging)
			{
				const float rotation_speed = 0.01f;
				lightRotation += delta_x * rotation_speed;
			}
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);

	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if(state[SDL_SCANCODE_E])
	{
		cameraPosition += cameraSpeed * deltaTime * worldUp;
	}
	return quitEvent;
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	// ----------------- Set variables --------------------------
	ImGui::SliderFloat("Inner Deg.", &innerSpotlightAngle, 0.0f, 90.0f);
	ImGui::SliderFloat("Outer Deg.", &outerSpotlightAngle, 0.0f, 90.0f);
	ImGui::Checkbox("Manual light only (right-click drag to move)", &lightManualOnly);
	ImGui::Checkbox("Use SSAO", &useSSAO);
	if (useSSAO) {
		bool changed = ImGui::SliderInt("Number of SSAO Samples", &numSSAO, 10, MAX_SSAO);
		changed = changed || ImGui::SliderFloat("Hemisphere Radius", &hemiR, 1.f, 20.f);
		if (changed) {
			generateSamples();
		}
		ImGui::Checkbox("Use Rotation Map", &useRot);
		ImGui::Checkbox("Bilateral-Blur SSAO", &blurSSAO);
		ImGui::Checkbox("Visualise SSAO", &visSSAO);
	}
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
		ImGui::GetIO().Framerate);
	// ----------------------------------------------------------

	///////////////////////////////////////////////////////////////////////////
	// A button for reloading the shaders
	///////////////////////////////////////////////////////////////////////////
	if (ImGui::Button("Reload Shaders"))
	{
		loadShaders(true);
	}


	// Render the GUI.
	ImGui::Render();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initGL();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;
		// render to window
		display();

		// Render overlay GUI.
		if(showUI)
		{
			gui();
		}

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);

		// check events (keyboard among other)
		stopRendering = handleEvents();
	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(sphereModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
