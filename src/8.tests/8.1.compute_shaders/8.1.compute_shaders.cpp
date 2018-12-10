#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_s.h>

#ifdef _DEBUG
	#include <learnopengl/debugCallback.h>
#endif

#include <iostream>

#include <chrono>

using namespace std;
using namespace std::chrono;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1800;
const unsigned int SCR_HEIGHT = 900;
const GLuint GROUP_SIZE_X = 32;
const GLuint GROUP_SIZE_Y = 32;

const GLuint64 loopsToSwitch = 1000;


// FPS
double lastFPSTime = 0.0f;
int frameCount = 0;

constexpr char* WinTitle = "LearnOpenGL";

#define NO_VSYNC 1
#define CONSOLE_PERF 0

#define LINEAR_FILTERING 0


GLuint VAO, VBO, EBO;

GLuint inTexture;

GLuint outFBO[2];
GLuint outTexFBO[2];

int inTexWidth, inTexHeight, inTexNOfChannels;

//GLuint csTexture[2];

GLFWwindow* window;
unique_ptr<Shader> outputShader;
unique_ptr<Shader> blurFBOShader;
unique_ptr<Shader> blurCSShader;

void* zeros;

void init() {
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif


#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

	// glfw window creation
	// --------------------
	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		cout << "Failed to create GLFW window" << endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(window);
#if NO_VSYNC
	glfwSwapInterval(0);
#endif
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		cout << "Failed to initialize GLAD" << endl;
		exit(-1);
	}

#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(LearnOpenGL::debugCallback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
		GL_DEBUG_SEVERITY_NOTIFICATION, -1, "Start of debug log");
#endif

	// build and compile our shader program
	// ------------------------------------
	outputShader = make_unique<Shader>("8.1.compute_shaders.vert", "8.1.compute_shaders.frag");
	outputShader->activateWith([]() {
		outputShader->setInt("texture1", 0);
	});

	auto compShaderDefs = vector<string>();
	
	compShaderDefs.emplace_back("#version 430 core");
	compShaderDefs.emplace_back("#define GROUP_SIZE_X " + to_string(GROUP_SIZE_X));
	compShaderDefs.emplace_back("#define GROUP_SIZE_Y " + to_string(GROUP_SIZE_Y));
	blurCSShader = make_unique<Shader>("8.1.compute_shaders.comp", compShaderDefs);

	blurFBOShader = make_unique<Shader>("8.1.compute_shaders_blur.vert", "8.1.compute_shaders_blur.frag");
	blurFBOShader->activateWith([]() {
		blurFBOShader->setInt("texture1", 0);
	});


	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------
	const float vertices[] = {
		// positions          // colors           // texture coords
		 1.0f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
		 1.0f, -1.0f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
		-1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
		-1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // top left 
	};
	const unsigned int indices[] = {
		0, 1, 3, // first triangle
		1, 2, 3  // second triangle
	};
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// texture coord attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);


	// load and create a texture 
	// -------------------------
	glGenTextures(1, &inTexture);
	glBindTexture(GL_TEXTURE_2D, inTexture); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object

	/*
	// set the texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	*/

	// set texture filtering parameters
#if LINEAR_FILTERING
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif
	// load image, create texture and generate mipmaps
	
	// The FileSystem::getPath(...) is part of the GitHub repository so we can find files on any IDE/platform; replace it with your own image path.
	unsigned char *data = stbi_load(FileSystem::getPath("resources/textures/texture-diamond-plate.jpg").c_str(), &inTexWidth, &inTexHeight, &inTexNOfChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, inTexWidth, inTexHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glBindImageTexture(0, inTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
		//glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		cout << "Failed to load texture" << endl;
		exit(-1);
	}
	stbi_image_free(data);

	glBindTexture(GL_TEXTURE_2D, 0);

	// framebuffer configuration
	// -------------------------
	glGenFramebuffers(2, &outFBO[0]);

	for (int i = 0; i < 2; ++i) {
		glViewport(0, 0, inTexWidth, inTexHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, outFBO[i]);
		// create a color attachment texture
		glGenTextures(1, &outTexFBO[i]);
		glBindTexture(GL_TEXTURE_2D, outTexFBO[i]);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, inTexWidth, inTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, inTexWidth, inTexHeight);
		glBindImageTexture(i + 1, outTexFBO[i], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if LINEAR_FILTERING
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTexFBO[i], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << endl;
			exit(-1);
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	zeros = malloc(inTexWidth * inTexHeight * 4);
	memset(zeros, 0, inTexWidth * inTexHeight * 4);
}

void finalize() {

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);

	glDeleteTextures(1, &inTexture);
	
	glDeleteTextures(2, &outTexFBO[0]);
	glDeleteFramebuffers(2, &outFBO[0]);

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	free(zeros);

	glfwTerminate();
}

void drawTextureToFramebuffer(GLuint tex, unique_ptr<Shader>& shader) {
	// bind Texture
	glBindTexture(GL_TEXTURE_2D, tex);

	// render container
	//shader->use();
	shader->use();
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void displayFPS(GLFWwindow* win, const char* info, const double fps)
{
	char winTitleFPS[255];
	snprintf(&winTitleFPS[0], sizeof(winTitleFPS), "%s %s - [FPS: %3.2f, avg. frame render time: %0.8f]", WinTitle, info, fps, 1000.0 / fps);

	glfwSetWindowTitle(win, winTitleFPS);
}

#define DRAW_RESULT 1

int main()
{

	init();

	GLuint64 nLoops = 0;
	GLuint64 totalGPUTimeElapsed = 0;
	GLuint64 totalCPUTimeElapsed = 0;

	high_resolution_clock::time_point beginTime;

	GLuint timerQuery;
	glGenQueries(1, &timerQuery);

	GLuint renderingMethod = 0;
	const GLuint renderingMethodsCount = 2;


	const unordered_map<GLuint, string> renderingMethodNames = {
		{0, "Compute Shader"},
		{1, "   Framebuffer"},
	};

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

		++nLoops;

#if	CONSOLE_PERF
		beginTime = high_resolution_clock::now();
		glBeginQuery(GL_TIME_ELAPSED, timerQuery);
#endif
		glClearTexImage(outTexFBO[0], 0, GL_RGBA, GL_UNSIGNED_BYTE, zeros);
		if (renderingMethod == 0) { //CS
			blurCSShader->use();

			auto dispatchX = static_cast<GLuint>( ceil(static_cast<float>(inTexWidth) / static_cast<float>(GROUP_SIZE_X)) );
			auto dispatchY = static_cast<GLuint>( ceil(static_cast<float>(inTexHeight) / static_cast<float>(GROUP_SIZE_Y)) );

			glDispatchCompute(dispatchX, dispatchY, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
		else if (renderingMethod == 1) { //FBO
			glViewport(0, 0, inTexWidth, inTexHeight);
			glBindFramebuffer(GL_FRAMEBUFFER, outFBO[0]);
			drawTextureToFramebuffer(inTexture, blurFBOShader);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

#if	CONSOLE_PERF
		glEndQuery(GL_TIME_ELAPSED);

		auto timeElapsedCPU = duration_cast<nanoseconds>(high_resolution_clock::now() - beginTime).count();
		totalCPUTimeElapsed += timeElapsedCPU;


		GLuint64 timeElapsedGPU = 0;
		while (!timeElapsedGPU) {
			glGetQueryObjectui64v(timerQuery, GL_QUERY_RESULT_AVAILABLE, &timeElapsedGPU);
			if (!timeElapsedGPU)
				Sleep(0);
		}
		glGetQueryObjectui64v(timerQuery, GL_QUERY_RESULT, &timeElapsedGPU);
		totalGPUTimeElapsed += timeElapsedGPU;
#endif

		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
#if DRAW_RESULT
		drawTextureToFramebuffer(outTexFBO[0], outputShader);
#endif
		double currentTime = glfwGetTime();
		++frameCount;
		if (currentTime - lastFPSTime >= 0.5) {
			displayFPS(window, renderingMethodNames.at(renderingMethod).c_str(), static_cast<double>(frameCount) / (currentTime - lastFPSTime));
			lastFPSTime = currentTime;
			frameCount = 0;
		}

		if (nLoops % loopsToSwitch == 0) {
#if	CONSOLE_PERF
			cout << "Rendering Method: " << renderingMethodNames.at(renderingMethod) << ". Average processing time"
				<< " (GPU): " << static_cast<double>(totalGPUTimeElapsed) / static_cast<double>(nLoops) * 1.e-6
				<< " (CPU): " << static_cast<double>(totalCPUTimeElapsed) / static_cast<double>(nLoops) * 1.e-6
				<< " ms/frame" << endl;
#endif
			totalCPUTimeElapsed = 0;
			totalGPUTimeElapsed = 0;
			nLoops = 0;

			renderingMethod = ++renderingMethod % renderingMethodsCount;
		}
/*
#if	CONSOLE_PERF == 0
		if (nLoops == 0) {
			cout << "Rendering Method: " << renderingMethodNames.at(renderingMethod) << endl;
		}
#endif
*/


        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

	finalize();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}