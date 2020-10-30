#include <GL/glew.h>

#include <string>
#include <fstream>
#include <streambuf>
#include <cstdlib>
#include <SDL.h>
#include <labhelper.h>


#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>
// The window we'll be rendering to
SDL_Window* g_window = nullptr;

// `vertexArrayObject' holds the data for each vertex. Data for each vertex
// consists of positions (from positionBuffer) and color (from colorBuffer)
// in this example.
GLuint vertexArrayObject;

// The shaderProgram combines a vertex shader (vertexShader) and a
// fragment shader (fragmentShader) into a single GLSL program that can
// be activated (glUseProgram()).
GLuint shaderProgram;

float g_clearColor[3] = { 0.2f, 0.2f, 0.8f };

void initGL()
{
	//////////////////////////////////////////////////////////////////////////////
	// Vertex positions
	//////////////////////////////////////////////////////////////////////////////
	// Define the positions for each of the three vertices of the triangle
	const float positions[] = {
		//	 X      Y     Z
		0.0f,  0.5f,  1.0f, // v0
		-0.5f, -0.5f, 1.0f, // v1
		0.5f,  -0.5f, 1.0f  // v2
	};
	// Create a handle for the position vertex buffer object
	// See OpenGL Spec §2.9 Buffer Objects
	// - http://www.cse.chalmers.se/edu/course/TDA361/glspec30.20080923.pdf#page=54
	GLuint positionBuffer;
	glGenBuffers(1, &positionBuffer);
	// Set the newly created buffer as the current one
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	// Send the vertex position data to the current buffer
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

	//////////////////////////////////////////////////////////////////////////////
	// Vertex colors
	//
	// TASK 3: Change these colors to something more fun.
	//////////////////////////////////////////////////////////////////////////////
	// Define the colors for each of the three vertices of the triangle
	const float colors[] = {
		//   R     G     B
		1.0f, 1.0f, 1.0f, // White
		1.0f, 1.0f, 1.0f, // White
		1.0f, 1.0f, 1.0f  // White
	};
	// Create a handle for the vertex color buffer
	GLuint colorBuffer;
	glGenBuffers(1, &colorBuffer);
	// Set the newly created buffer as the current one
	glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
	// Send the vertex color data to the current buffer
	glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);

	//////////////////////////////////////////////////////////////////////////////
	// Create a vertex array object and connect the vertex buffer objects to it
	//
	// See OpenGL Spec §2.10
	// - http://www.cse.chalmers.se/edu/course/TDA361/glspec30.20080923.pdf#page=64
	//////////////////////////////////////////////////////////////////////////////
	glGenVertexArrays(1, &vertexArrayObject);
	// Bind the vertex array object
	// The following calls will affect this vertex array object.
	glBindVertexArray(vertexArrayObject);
	// Makes positionBuffer the current array buffer for subsequent calls.
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	// Attaches positionBuffer to vertexArrayObject, in the 0th attribute location
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	// Makes colorBuffer the current array buffer for subsequent calls.
	glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
	// Attaches colorBuffer to vertexArrayObject, in the 1st attribute location
	glVertexAttribPointer(1, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	glEnableVertexAttribArray(0); // Enable the vertex position attribute
	glEnableVertexAttribArray(1); // Enable the vertex color attribute

	//////////////////////////////////////////////////////////////////////////////
	// TASK 4: Add two new triangles. First by creating another vertex array
	//		   object, and then by adding a triangle to an existing VAO.
	//////////////////////////////////////////////////////////////////////////////



	///////////////////////////////////////////////////////////////////////////
	// Create shaders
	///////////////////////////////////////////////////////////////////////////

	// See OpenGL spec §2.20 http://www.cse.chalmers.se/edu/course/TDA361/glspec30.20080923.pdf#page=104&zoom=75
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load text files for vertex and fragment shaders.
	std::ifstream vs_file("../lab1-rasterization/simple.vert");
	std::string vs_src((std::istreambuf_iterator<char>(vs_file)), std::istreambuf_iterator<char>());

	std::ifstream fs_file("../lab1-rasterization/simple.frag");
	std::string fs_src((std::istreambuf_iterator<char>(fs_file)), std::istreambuf_iterator<char>());

	const char* vs = vs_src.c_str();
	const char* fs = fs_src.c_str();

	glShaderSource(vertexShader, 1, &vs, NULL);
	glShaderSource(fragmentShader, 1, &fs, NULL);

	// Compile the shader, translates into internal representation and checks for errors.
	glCompileShader(vertexShader);
	int compileOK;
	// check for compiler errors in vertex shader.
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compileOK);
	if(!compileOK)
	{
		std::string err = labhelper::GetShaderInfoLog(vertexShader);
		labhelper::fatal_error(err);
		return;
	}

	// Compile the shader, translates into internal representation and checks for errors.
	glCompileShader(fragmentShader);
	// check for compiler errors in fragment shader.
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compileOK);
	if(!compileOK)
	{
		std::string err = labhelper::GetShaderInfoLog(fragmentShader);
		labhelper::fatal_error(err);
		return;
	}

	// Create a program object and attach the two shaders we have compiled, the program object contains
	// both vertex and fragment shaders as well as information about uniforms and attributes common to both.
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, fragmentShader);
	glAttachShader(shaderProgram, vertexShader);

	// Now that the fragment and vertex shader has been attached, we no longer need these two separate objects and should delete them.
	// The attachment to the shader program will keep them alive, as long as we keep the shaderProgram.
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	// Link the different shaders that are bound to this program, this creates a final shader that
	// we can use to render geometry with.
	glLinkProgram(shaderProgram);

	// Check for linker errors, many errors, such as mismatched in and out variables between
	// vertex/fragment shaders,  do not appear before linking.
	{
		GLint linkOk = 0;
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkOk);
		if(!linkOk)
		{
			std::string err = labhelper::GetShaderInfoLog(shaderProgram);
			labhelper::fatal_error(err);
			return;
		}
	}
}


void display(void)
{
	// The viewport determines how many pixels we are rasterizing to
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	glViewport(0, 0, w, h); // Set viewport

	glClearColor(g_clearColor[0], g_clearColor[1], g_clearColor[2], 1.0); // Set clear color
	glClear(GL_BUFFER); // Clears the color buffer and the z-buffer
	                    // Instead of glClear(GL_BUFFER) the call should be glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

	// We disable backface culling for this tutorial, otherwise care must be taken with the winding order
	// of the vertices. It is however a lot faster to enable culling when drawing large scenes.
	glDisable(GL_CULL_FACE);

	// Shader Program
	glUseProgram(shaderProgram); // Set the shader program to use for this draw call
	// Bind the vertex array object that contains all the vertex data.
	glBindVertexArray(vertexArrayObject);
	// Submit triangles from currently bound vertex array object.
	glDrawArrays(GL_TRIANGLES, 0, 3); // Render 1 triangle


	glUseProgram(0); // "unsets" the current shader program. Not really necessary.
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	// ----------------- Set variables --------------------------
	ImGui::ColorEdit3("clear color", g_clearColor);
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------

	// Render the GUI.
	ImGui::Render();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Lab 1");

	initGL();


	// render-loop
	bool stopRendering = false;
	while(!stopRendering)
	{
		// First render our geometry.
		display();

		///////////////////////////////////////////////////////////////////////////
		// TASK 1: Uncomment the call to gui below to show the GUI
		///////////////////////////////////////////////////////////////////////////
		// Then render overlay GUI.
		// gui();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);

		// Check events (keyboard among other)
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			// Allow ImGui to capture events.
			ImGui_ImplSdlGL3_ProcessEvent(&event);

			// And do our own handling of events.
			if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
			{
				stopRendering = true;
			}
		}
	}

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
