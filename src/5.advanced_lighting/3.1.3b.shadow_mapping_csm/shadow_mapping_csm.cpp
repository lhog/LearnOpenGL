#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <algorithm>

#include "Scene.cpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
void renderScene(const Shader &shader);

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
double lastX = (float)SCR_WIDTH / 2.0;
double lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

// renderQuad
#define RENDERIDX_MAX 2
int oldSpaceKeyState = GLFW_RELEASE;
int renderIdx = 0;

// timing
double deltaTime = 0.0f;
double lastTime = 0.0f;
double lastFPSTime = 0.0f;

// meshes
unsigned int planeVAO;

// for FPS
int frameCount = 0;

constexpr char* WinTitle = "LearnOpenGL";

#define NO_VSYNC 1

#define COMPUTED_CAMERAPROJECTION 1

#define SHADOW_RES 1024
#define SHADOWS_CSM 1
#if SHADOWS_CSM
	#define SHADOW_MAP_CASCADE_COUNT 4
	#define SHADOW_USE_GEOMETRY_SHADER 1
	#define COMPUTED_LIGHTPROJECTION 1
	#define SHADOW_WIDTH SHADOW_RES / SHADOW_MAP_CASCADE_COUNT
	#define SHADOW_HEIGHT SHADOW_RES / SHADOW_MAP_CASCADE_COUNT
#else
	#define SHADOW_WIDTH SHADOW_RES
	#define SHADOW_HEIGHT SHADOW_RES
#endif

#define MOVING_LIGHT 1

#define CAMERA_NEAR 0.1f 
#define CAMERA_FAR 100.0f





void displayFPS(GLFWwindow* win, const double fps)
{
	char winTitleFPS[255];
	snprintf(&winTitleFPS[0], sizeof(winTitleFPS), "%s - [FPS: %3.2f, avg. frame render time: %0.8f]", WinTitle, fps, 1000.0 / fps);

	glfwSetWindowTitle(win, winTitleFPS);
}

#if SHADOWS_CSM
constexpr float cascadeSplitLambda = 0.9f;
// Contains all resources required for a single shadow map cascade
struct Cascade {
	float splitDepth;
	glm::mat4 lsVPMat;
};
std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> cascades;

static float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];
void updateCascades(float nearClip, float farClip, glm::mat4& invViewProj, glm::vec3& lightPos) {
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	float cascadeRange[2] = { 0.0, 0.0 };
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i) {
		cascadeRange[1] = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, -1.0f),
			glm::vec3(1.0f,  1.0f, -1.0f),
			glm::vec3(1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		int idx = 0;
		for (auto& frustumCorner : frustumCorners) {
			glm::vec4 frustumCornerWS = invViewProj * glm::vec4(frustumCorner, 1.0f);
			frustumCorners[idx] = glm::vec3(frustumCornerWS / frustumCornerWS.w);
			++idx;
		}


		for (uint32_t i = 0; i < 4; i++) {
			glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
			frustumCorners[i + 4] = frustumCorners[i] + (dist * cascadeRange[1]);
			frustumCorners[i] = frustumCorners[i] + (dist * cascadeRange[0]);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t i = 0; i < 8; i++) {
			frustumCenter += frustumCorners[i];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t i = 0; i < 8; i++) {
			float distance = glm::length(frustumCorners[i] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::vec3 lightDir = normalize(-lightPos);
		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

		cascades[i].splitDepth = -(nearClip + cascadeRange[0] * clipRange);
		cascades[i].lsVPMat = lightOrthoMatrix * lightViewMatrix;

		cascadeRange[0] = cascadeRange[1];
	}


}
#endif

static Scene scene;

void createSceneObjects() {

	const VertexAttribute position(3, 0, GL_FALSE);
	const VertexAttribute normal(3, 3, GL_FALSE);
	const VertexAttribute texCoord(2, 6, GL_FALSE);
	std::vector<VertexAttribute> sceneVA({ position, normal, texCoord });

	const std::vector<float> planeVertices({
		// positions            // normals         // texcoords
		 25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
		-25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
		-25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

		 25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
		-25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
		 25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
		});

	static ModelObject modelPlane(planeVertices, sceneVA, 8);
	static SceneObject scenePlane(modelPlane, position);
	scene.Add(0, scenePlane, false);

	const std::vector<float> cubeVertices({
		// positions            // normals       // texcoords
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		// right face
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
		// bottom face
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		// top face
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		 1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		});

	static ModelObject modelCube(cubeVertices, sceneVA, 8);
	{
		glm::mat4 modelMat = glm::mat4();
		modelMat = glm::translate(modelMat, glm::vec3(0.0f, 1.5f, 0.0));
		modelMat = glm::scale(modelMat, glm::vec3(0.5f));
		static SceneObject sceneCube1(modelCube, position, modelMat);
		scene.Add(1, sceneCube1);
	}

	{
		glm::mat4 modelMat = glm::mat4();
		modelMat = glm::translate(modelMat, glm::vec3(2.0f, 0.0f, 1.0));
		modelMat = glm::scale(modelMat, glm::vec3(0.5f));
		static SceneObject sceneCube2(modelCube, position, modelMat);
		scene.Add(2, sceneCube2);
	}

	{
		glm::mat4 modelMat = glm::mat4();
		modelMat = glm::translate(modelMat, glm::vec3(-1.0f, 0.0f, 2.0));
		modelMat = glm::rotate(modelMat, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
		modelMat = glm::scale(modelMat, glm::vec3(0.25));
		static SceneObject sceneCube3(modelCube, position, modelMat);
		scene.Add(3, sceneCube3);
	}
}


int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, WinTitle, NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
	#if NO_VSYNC
		glfwSwapInterval(0);
	#endif
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------
    Shader shader("3.1.3b.shadow_mapping.vs", "3.1.3b.shadow_mapping.fs");
	#if SHADOW_USE_GEOMETRY_SHADER
		Shader simpleDepthShader("3.1.3b.shadow_mapping_depth.vs", "3.1.3b.shadow_mapping_depth.fs", "3.1.3b.shadow_mapping_depth.gs");
	#else
		Shader simpleDepthShader("3.1.3b.shadow_mapping_depth.vs", "3.1.3b.shadow_mapping_depth.fs");
	#endif
    Shader debugDepthQuad("3.1.3b.debug_quad.vs", "3.1.3b.debug_quad_depth.fs");

	createSceneObjects();

    // load textures
    // -------------
    unsigned int woodTexture = loadTexture(FileSystem::getPath("resources/textures/wood.png").c_str());

    // configure depth map FBO
    // -----------------------
    unsigned int depthMapFBO[SHADOW_MAP_CASCADE_COUNT];
    glGenFramebuffers(SHADOW_MAP_CASCADE_COUNT, &depthMapFBO[0]);
    // create depth texture
    unsigned int depthMap[SHADOW_MAP_CASCADE_COUNT];    

	#if SHADOWS_CSM
		#if SHADOW_USE_GEOMETRY_SHADER
			glGenTextures(1, &depthMap[0]);
			glBindTexture(GL_TEXTURE_2D_ARRAY, depthMap[0]);
			//glTexStorage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, SHADOW_MAP_CASCADE_COUNT);
			glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, SHADOW_MAP_CASCADE_COUNT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
			glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
			// attach depth texture as FBO's depth buffer
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[0]);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMap[0], 0);
			//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_ARRAY, depthMap, 0);
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		#else
			glGenTextures(SHADOW_MAP_CASCADE_COUNT, &depthMap[0]);
		#endif
	#else
		glGenTextures(1, &depthMap[0]);
		glBindTexture(GL_TEXTURE_2D, depthMap[0]);
		//glTexStorage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
		// attach depth texture as FBO's depth buffer
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMap[0], 0);
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	#endif

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Failed to initialize depthMapFBO" << std::endl;
	}

    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // shader configuration
    // --------------------
    shader.use();
    shader.setInt("diffuseTexture", 0);
    shader.setInt("shadowMap", 1);
    debugDepthQuad.use();
    debugDepthQuad.setInt("depthMap", 0);

    // lighting info
    // -------------
    glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        double currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        

		// Measure speed
		++frameCount;
		// If a second has passed.
		if (currentTime - lastFPSTime >= 1.0)
		{
			// Display the frame count here any way you want.
			displayFPS(window, static_cast<double>(frameCount) / (currentTime - lastFPSTime));

			frameCount = 0;
			lastFPSTime = currentTime;
		}

		lastTime = currentTime;

        // input
        // -----
        processInput(window);

		#if MOVING_LIGHT
			// change light position over time
			lightPos.x = static_cast<float>(sin(glfwGetTime()) * 3.0f);
			lightPos.z = static_cast<float>(cos(glfwGetTime()) * 2.0f);
			lightPos.y = static_cast<float>(5.0 + cos(glfwGetTime()) * 1.0f);
		#endif

        // render
        // ------
		// 0. compute view and projection matrices
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 view = camera.GetViewMatrix();

		float camNear;
		float camFar;

		#if COMPUTED_CAMERAPROJECTION
			glm::vec4 cvMin, cvMax;
			BoundingSphere bsAll;
			bsAll.Add(scene.GetBoundingSphere());
			bsAll.Add(scene.Get(0).GetBoundingSphere());
			bsAll.GetMinMax(cvMin, cvMax, view);

			camNear = std::max(-cvMax.z, CAMERA_NEAR);
			camFar = -cvMin.z;
		#else
			camNear = CAMERA_NEAR;
			camFar = CAMERA_FAR;
		#endif

		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, camNear, camFar);
		glm::mat4 invViewProj = glm::inverse(projection * view);

        // 1. render depth of scene to texture (from light's perspective)
        // --------------------------------------------------------------
        
        //lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene
		#if SHADOWS_CSM
			updateCascades(camNear, camFar, invViewProj, lightPos);
		#endif

		const glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));

		float lightNear;
		float lightFar;
		#if COMPUTED_LIGHTPROJECTION
			//http://dev.theomader.com/cascaded-shadow-mapping-1/
			glm::vec4 lvMin, lvMax;
			scene.GetBoundingSphere().GetMinMax(lvMin, lvMax, lightView);
			lightNear = -lvMax.z;
			lightFar = -lvMin.z;
			const glm::mat4 lightProjection = glm::ortho(lvMin.x, lvMax.x, lvMin.y, lvMax.y, lightNear, lightFar);
		#else
			lightNear = 1.0f;
			lightFar = 7.5f;
			const glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, lightNear, lightFar);
		#endif

		const glm::mat4 lightSpaceMatrix = lightProjection * lightView;
        // render scene from light's point of view
        simpleDepthShader.use();
        simpleDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[0]);
            glClear(GL_DEPTH_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, woodTexture);
            renderScene(simpleDepthShader);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // reset viewport
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 2. render scene as normal using the generated depth/shadow map  
        // --------------------------------------------------------------
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        // set light uniforms
        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("lightPos", lightPos);
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap[0]);
        renderScene(shader);

        // render Depth map to quad for visual debugging
        // ---------------------------------------------
        debugDepthQuad.use();
        debugDepthQuad.setFloat("near_plane", lightNear);
        debugDepthQuad.setFloat("far_plane", lightFar);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap[0]);

		switch (renderIdx)
		{
		case 1:
			renderQuad();
			break;
		default:
			break;
		}
		

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// renders the 3D scene
// --------------------
void renderScene(const Shader &shader)
{
	auto lambdaUpdateShaderUniforms = [&](const int id, const SceneObject& so) {
		shader.setMat4("model", so.GetSceneMatrix());
	};
	scene.Render(lambdaUpdateShaderUniforms);
 }


// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, static_cast<float>(deltaTime));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, static_cast<float>(deltaTime));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, static_cast<float>(deltaTime));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, static_cast<float>(deltaTime));

	int spaceKeyState = glfwGetKey(window, GLFW_KEY_SPACE);
	if (spaceKeyState == GLFW_PRESS && spaceKeyState != oldSpaceKeyState) {
		renderIdx = ++renderIdx % RENDERIDX_MAX;
	}
	oldSpaceKeyState = spaceKeyState;
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

	float xoffset = static_cast<float>(xpos - lastX);
	float yoffset = static_cast<float>(lastY - ypos); // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
