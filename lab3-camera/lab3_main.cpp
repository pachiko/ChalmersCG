
#include <GL/glew.h>

#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>
#include <Model.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;
using namespace labhelper;

using std::min;
using std::max;

int old_w = 1280;
int old_h = 720;

struct PerspectiveParams
{
	float fov;
	int w;
	int h;
	float near;
	float far;
};

PerspectiveParams pp = { 45.0f, 1280, 720, 0.1f, 300.0f };

// The window we'll be rendering to
SDL_Window* g_window = nullptr;

GLuint shaderProgram;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

float currentTime = 0.0f;
float deltaTime = 0.0f;
bool showUI = false;

// Models
Model* cityModel = nullptr;
Model* carModel = nullptr;
Model* groundModel = nullptr;

// Self-driving Car
mat4 carModelMatrix(1.0f);
float angle = 0.1f;

vec3 worldUp = vec3(0.0f, 1.0f, 0.0f);

// Camera parameters
vec3 cameraPosition(15.0f, 15.0f, 15.0f);
vec3 cameraDirection(-1.0f, -1.0f, -1.0f);
vec3 cameraRight = normalize(cross(cameraDirection, worldUp));
vec3 cameraUp = normalize(cross(cameraRight, cameraDirection));

// Car
mat4 T(1.0f), R(1.0f);
bool driveCar = false;
mat4 carCam = translate(vec3(0.f, -2.f, -4.f));

void loadModels()
{
	///////////////////////////////////////////////////////////////////////////
	// Load models (both vertex buffers and textures).
	///////////////////////////////////////////////////////////////////////////
	cityModel = loadModelFromOBJ("../scenes/city.obj");
	carModel = loadModelFromOBJ("../scenes/car.obj");
	groundModel = loadModelFromOBJ("../scenes/ground_plane.obj");
}

void drawGround(mat4 mvpMatrix)
{
	mvpMatrix[3] += vec4(0, 0.0005, 0, 0);
	int loc = glGetUniformLocation(shaderProgram, "modelViewProjectionMatrix");
	glUniformMatrix4fv(loc, 1, false, &mvpMatrix[0].x);
	render(groundModel);
}

void display()
{
	// Set up
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);

	if (pp.w != old_w || pp.h != old_h)
	{
		SDL_SetWindowSize(g_window, pp.w, pp.h);
		w = pp.w;
		h = pp.h;
		old_w = pp.w;
		old_h = pp.h;
	}

	glViewport(0, 0, w, h);                             // Set viewport
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);               // Set clear color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clears the color buffer and the z-buffer
	glEnable(GL_DEPTH_TEST);                            // enable Z-buffering
	glDisable(GL_CULL_FACE);                            // disables not showing back faces of triangles

	// Set the shader program to use for this draw call
	glUseProgram(shaderProgram);

	// Set up model matrices
	mat4 cityModelMatrix(1.0f);

	// Set up the view matrix
	// The view matrix defines where the viewer is looking
	// Initially fixed, but will be replaced in the tutorial.
	//mat4 constantViewMatrix = mat4(0.707106769f, -0.408248276f, 1.00000000f, 0.000000000f, 0.000000000f,
	//	0.816496551f, 1.00000000f, 0.000000000f, -0.707106769f, -0.408248276f,
	//	1.00000000f, 0.000000000f, 0.000000000f, 0.000000000f, -30.0000000f,
	//	1.00000000f);

	mat4 viewMatrix;
	R[0] = normalize(R[0]);
	R[2] = vec4(cross(vec3(R[0]), vec3(R[1])), 0.0f);
	mat4 TR = T * R; // car's pose

	if (driveCar) {
		vec3 carDir = normalize(vec3(R[2])); // car faces it's z-axis
		vec3 carRight = normalize(cross(carDir, worldUp));
		vec3 carUp = normalize(cross(carRight, carDir));
		// The vectors above are left-handed; OpenGL needs them to be right-handed
		// Hence why z is always negated
		mat3 carBaseVectorsWorldSpace(carRight, carUp, -carDir);
		viewMatrix = carCam * mat4(transpose(carBaseVectorsWorldSpace)) * translate(-vec3(TR[3]));
	}
	else {
		//cameraRight = normalize(cross(cameraDirection, worldUp));
		//cameraUp = normalize(cross(cameraRight, cameraDirection));
		mat3 cameraBaseVectorsWorldSpace(cameraRight, cameraUp, -cameraDirection);
		viewMatrix = mat4(transpose(cameraBaseVectorsWorldSpace)) * translate(-cameraPosition);
	}


	// Setup the projection matrix
	if (w != old_w || h != old_h)
	{
		pp.h = h;
		pp.w = w;
		old_w = w;
		old_h = h;
	}
	mat4 projectionMatrix = perspective(radians(pp.fov), float(pp.w) / float(pp.h), pp.near, pp.far);

	// Concatenate the three matrices and pass the final transform to the vertex shader

	// City
	mat4 modelViewProjectionMatrix = projectionMatrix * viewMatrix * cityModelMatrix;
	int loc = glGetUniformLocation(shaderProgram, "modelViewProjectionMatrix");
	glUniformMatrix4fv(loc, 1, false, &modelViewProjectionMatrix[0].x);
	render(cityModel);

	// Ground
	// Task 5: Uncomment this
	drawGround(modelViewProjectionMatrix);

	// Car
	//T[3] = vec4(0.f, 0.f, 5.f, 1.0f);

	modelViewProjectionMatrix = projectionMatrix * viewMatrix * TR;
	glUniformMatrix4fv(loc, 1, false, &modelViewProjectionMatrix[0].x);
	render(carModel);

	// Self-driving car
	carModelMatrix = rotate(angle, vec3(0.f, 1.f, 0.f)); // self rotation
	carModelMatrix *= translate(vec3(5.f, 0.f, 0.f)); // radius from world center
	carModelMatrix *= rotate(angle / 100.f, vec3(0.f, 1.f, 0.f)); // rotation in world

	//carModelMatrix = translate(vec3(0.f, 0.f, 5.f));
	modelViewProjectionMatrix = projectionMatrix * viewMatrix * carModelMatrix;
	glUniformMatrix4fv(loc, 1, false, &modelViewProjectionMatrix[0].x);
	render(carModel);
	angle -= 0.01f;

	glUseProgram(0);
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	// ----------------- Set variables --------------------------
	ImGui::SliderFloat("Field Of View", &pp.fov, 1.0f, 180.0f, "%.0f");
	ImGui::SliderInt("Width", &pp.w, 256, 1920);
	ImGui::SliderInt("Height", &pp.h, 256, 1080);
	ImGui::Text("Aspect Ratio: %.2f", float(pp.w) / float(pp.h));
	ImGui::SliderFloat("Near Plane", &pp.near, 0.1f, 300.0f, "%.2f", 2.f);
	ImGui::SliderFloat("Far Plane", &pp.far, 0.1f, 300.0f, "%.2f", 2.f);
	if (ImGui::Button("Reset"))
	{
		pp.fov = 45.0f;
		pp.w = 1280;
		pp.h = 720;
		pp.near = 0.1f;
		pp.far = 300.0f;
	}
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
		ImGui::GetIO().Framerate);
	// ----------------------------------------------------------

	// Render the GUI.
	ImGui::Render();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Lab 3");

	// Load shader program
	shaderProgram = labhelper::loadShaderProgram("../lab3-camera/simple.vert", "../lab3-camera/simple.frag");

	// Load models
	loadModels();

	// render-loop
	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while (!stopRendering)
	{
		// update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		deltaTime = timeSinceStart.count() - currentTime;
		currentTime = timeSinceStart.count();

		// render to window
		display();

		// Render overlay GUI.
		if (showUI)
		{
			gui();
		}

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);

		// check new events (keyboard among other)
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			// Allow ImGui to capture events.
			ImGui_ImplSdlGL3_ProcessEvent(&event);

			// More info at https://wiki.libsdl.org/SDL_Event
			if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
			{
				stopRendering = true;
			}
			if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
			{
				showUI = !showUI;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
				&& (!showUI || !ImGui::GetIO().WantCaptureMouse))
			{
				g_isMouseDragging = true;
				int x;
				int y;
				SDL_GetMouseState(&x, &y);
				g_prevMouseCoords.x = x;
				g_prevMouseCoords.y = y;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
				driveCar = !driveCar;
			}

			if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
			{
				g_isMouseDragging = false;
			}

			if (event.type == SDL_MOUSEMOTION && g_isMouseDragging && !driveCar)
			{
				// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
				int delta_x = event.motion.x - g_prevMouseCoords.x;
				int delta_y = event.motion.y - g_prevMouseCoords.y;
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					float rotationSpeed = 0.005f;
					mat4 yaw = rotate(rotationSpeed * -delta_x, worldUp);
					mat4 pitch = rotate(rotationSpeed * -delta_y, normalize(cross(cameraDirection, worldUp)));
					cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));

					//printf("Mouse motion while left button down (%i, %i)\n", event.motion.x, event.motion.y);
				}
				g_prevMouseCoords.x = event.motion.x;
				g_prevMouseCoords.y = event.motion.y;
			}
		}

		// check keyboard state (which keys are still pressed)
		const uint8_t* state = SDL_GetKeyboardState(nullptr);

		const float speed = 2.f;
		const float rotateSpeed = 2.f;
		// implement car controls based on key states
		if (state[SDL_SCANCODE_UP])
		{
			T[3] += R * (speed * deltaTime * vec4(0.0f, 0.0f, 1.0f, 0.0f));
		}
		if (state[SDL_SCANCODE_DOWN])
		{
			T[3] -= R * (speed * deltaTime * vec4(0.0f, 0.0f, 1.0f, 0.0f));
		}
		if (state[SDL_SCANCODE_LEFT])
		{
			R[0] -= rotateSpeed * deltaTime * R[2];
		}
		if (state[SDL_SCANCODE_RIGHT])
		{
			R[0] += rotateSpeed * deltaTime * R[2];
		}
		
		if (!driveCar) {
			float zoomSpeed = 0.5f;
			if (event.type == SDL_MOUSEWHEEL)
			{ // Very buggy.
				if (event.wheel.y > 0) // scroll up
				{
					cameraPosition += zoomSpeed * cameraDirection;
				}
				else if (event.wheel.y < 0) // scroll down
				{
					cameraPosition -= zoomSpeed * cameraDirection;
				}
			}

			float panSpeed = 0.5f;
			// use camera direction as -z axis and compute the x (cameraRight) and y (cameraUp) base vectors
			cameraRight = normalize(cross(cameraDirection, worldUp));
			cameraUp = normalize(cross(cameraRight, cameraDirection));
			if (state[SDL_SCANCODE_W]) {
				cameraPosition += panSpeed * cameraUp;
			}
			if (state[SDL_SCANCODE_S]) {
				cameraPosition -= panSpeed * cameraUp;
			}
			if (state[SDL_SCANCODE_A]) {
				cameraPosition -= panSpeed * cameraRight;
			}
			if (state[SDL_SCANCODE_D]) {
				cameraPosition += panSpeed * cameraRight;
			}
		}
	}

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
