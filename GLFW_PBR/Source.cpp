#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"

#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_glfw_gl3.h"

#include "Shader.h"
#include "Camera.h"

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include <iostream>
#include <map>

const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 695;

// Uniforms
glm::vec3 albedoF(0.5f, 0.0f, 0.0f);
glm::vec3 lightPos(0.0f, 0.0f, -20.0f);
float aoF = 1.0f;

// Camera
Camera camera(glm::vec3(0.0f, 0.5f, 5.0f));
float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;
bool mouseActive = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

GLFWwindow* InitWindow();
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path, bool gammaCorrection);
void renderSphere();
void renderCube();
void renderQuad();
void renderQuadNormal();

unsigned int sphereVAO = 0, indexCount;
unsigned int cubeVAO = 0, cubeVBO;
unsigned int quadVAO = 0, quadVBO;
unsigned int quadNormalVAO = 0, quadNormalVBO;

int main()
{
	GLFWwindow* window = InitWindow();
	if (!window)
		return -1;

	Shader shader("res/shaders/Basic.shader");
	Shader pbrShader("res/shaders/PBR.shader");
	Shader cubemapShader("res/shaders/Cubemap.shader");
	Shader irradianceShader("res/shaders/Irradiance.shader");
	Shader prefilterShader("res/shaders/Prefilter.shader");
	Shader brdfShader("res/shaders/BRDF.shader");
	Shader backgroundShader("res/shaders/Background.shader");

	// PBR: Setup Framebuffer
	unsigned int captureFBO, captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	// PBR: Load the HDR environment map
	stbi_set_flip_vertically_on_load(true);
	int width, height, nrComponents;
	float* data = stbi_loadf("res/skyboxes/Tropical_Beach/Tropical_Beach_3k.hdr", &width, &height, &nrComponents, 0);
	//float* data = stbi_loadf("res/skyboxes/Shiodome_Stairs/10-Shiodome_Stairs_3k.hdr", &width, &height, &nrComponents, 0);
	//float* data = stbi_loadf("res/skyboxes/Ridgecrest_Road/Ridgecrest_Road_Ref.hdr", &width, &height, &nrComponents, 0);
	//float* data = stbi_loadf("res/skyboxes/Barcelona_Rooftops/Barce_Rooftop_C_3k.hdr", &width, &height, &nrComponents, 0);
	unsigned int hdrTexture;
	if (data)
	{
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		stbi_image_free(data);
		std::cout << "Failed to load HDR image!" << std::endl;
	}

	// PBR: Setup cubemap to render and attach to framebuffer
	unsigned int envCubemap;
	glGenTextures(1, &envCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	for (unsigned int i = 0; i < 6; ++i)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// PBR: Setup matrices for capturing data onto the cubemap faces
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
	{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
	};

	// PBR: Convert HDR equirectangular environment map to cubemap equivalent
	cubemapShader.Bind();
	cubemapShader.SetUniform1i("equirectangularMap", 0);
	cubemapShader.SetUniformMatrix4fv("projection", captureProjection);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);
	glViewport(0, 0, 512, 512); // configure viewport to capture dimensions

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		for (unsigned int i = 0; i < 6; ++i)
		{
			cubemapShader.SetUniformMatrix4fv("view", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderCube();
		}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// let OpenGL generate mipmaps from first mip face
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// PBR: Create an irradiance cubemap
	unsigned int irradianceMap;
	glGenTextures(1, &irradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	for (unsigned int i = 0; i < 6; ++i)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

	// PBR: Solve diffuse integral by convolution to create an irradiance cubemap
	irradianceShader.Bind();
	irradianceShader.SetUniform1i("environmentMap", 0);
	irradianceShader.SetUniformMatrix4fv("projection", captureProjection);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	glViewport(0, 0, 32, 32);
	
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		for (unsigned int i = 0; i < 6; ++i)
		{
			irradianceShader.SetUniformMatrix4fv("view", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderCube();
		}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// PBR: Create pre-filter cubemap re-scaling captureFBO to pre-filter scale
	unsigned int prefilterMap;
	glGenTextures(1, &prefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	for (unsigned int i = 0; i < 6; ++i)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// PBR: run quasi monte-carlo simulation on environment lighting to create a prefilter cubemap
	prefilterShader.Bind();
	prefilterShader.SetUniform1i("environmentMap", 0);
	prefilterShader.SetUniformMatrix4fv("projection", captureProjection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	unsigned int maxMipLevels = 5;
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		// resize framebuffer according to mip-level size
		unsigned int mipWidth = 128 * std::pow(0.5, mip);
		unsigned int mipHeight = 128 * std::pow(0.5, mip);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0,  mipWidth, mipHeight);

		float roughness = (float)mip / (float)(maxMipLevels - 1);
		prefilterShader.SetUniform1f("roughness", roughness);
		for (unsigned int i = 0; i < 6; ++i)
		{
			prefilterShader.SetUniformMatrix4fv("view", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderCube();
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// PBR: Generate 2D LUT from BRDF equations used
	unsigned int brdfLUTTexture;
	glGenTextures(1, &brdfLUTTexture);
	// pre-allocate enough memory for the LUT texture
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// re-configure captureFBO and render screen-space quad wth BRDF shader
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

	glViewport(0, 0, 512, 512);
	brdfShader.Bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Load textures
	stbi_set_flip_vertically_on_load(false);

	shader.Bind();
	shader.SetUniform1i("diffuseMap", 0);
	shader.SetUniform1i("specularMap", 1);
	shader.SetUniform1i("normalMap", 2);
	
	unsigned int sandAlbedo = loadTexture("res/textures/sand/albedo.jpg", true);
	unsigned int sandSpecular = loadTexture("res/textures/sand/ao.jpg", true);
	unsigned int sandNormal = loadTexture("res/textures/sand/normal.jpg", true);
	
	pbrShader.Bind();
	pbrShader.SetUniform1i("irradianceMap", 0);
	pbrShader.SetUniform1i("prefilterMap", 1);
	pbrShader.SetUniform1i("brdfLUT", 2);

	pbrShader.SetUniform1i("albedoMap", 3);
	pbrShader.SetUniform1i("normalMap", 4);
	pbrShader.SetUniform1i("metallicMap", 5);
	pbrShader.SetUniform1i("roughnessMap", 6);
	pbrShader.SetUniform1i("aoMap", 7);

	pbrShader.SetUniform3f("albedoF", albedoF);
	pbrShader.SetUniform1f("aoF", aoF);

	unsigned int goldAlbedo = loadTexture("res/textures/gold/albedo.png", true);
	unsigned int goldNormal = loadTexture("res/textures/gold/normal.png", true);
	unsigned int goldMetallic = loadTexture("res/textures/gold/metallic.png", true);
	unsigned int goldRoughness = loadTexture("res/textures/gold/roughness.png", true);

	unsigned int alienAlbedo = loadTexture("res/textures/alien_metal/albedo.png", true);
	unsigned int alienNormal = loadTexture("res/textures/alien_metal/normal.png", true);
	unsigned int alienMetallic = loadTexture("res/textures/alien_metal/metallic.png", true);
	unsigned int alienRoughness = loadTexture("res/textures/alien_metal/roughness.png", true);
	unsigned int alienAo = loadTexture("res/textures/alien_metal/ao.png", true);

	unsigned int limestoneAlbedo = loadTexture("res/textures/limestone/albedo.png", true);
	unsigned int limestoneNormal = loadTexture("res/textures/limestone/normal.png", true);
	unsigned int limestoneMetallic = loadTexture("res/textures/limestone/metallic.png", true);
	unsigned int limestoneRoughness = loadTexture("res/textures/limestone/roughness.png", true);
	unsigned int limestoneAo = loadTexture("res/textures/limestone/ao.png", true);

	unsigned int woodAlbedo = loadTexture("res/textures/wood/albedo.png", true);
	unsigned int woodNormal = loadTexture("res/textures/wood/normal.png", true);
	unsigned int woodMetallic = loadTexture("res/textures/wood/metallic.png", true);
	unsigned int woodRoughness = loadTexture("res/textures/wood/roughness.png", true);
	unsigned int woodAo = loadTexture("res/textures/wood/ao.png", true);

	unsigned int graniteAlbedo = loadTexture("res/textures/granite/albedo.png", true);
	unsigned int graniteNormal = loadTexture("res/textures/granite/normal.png", true);
	unsigned int graniteMetallic = loadTexture("res/textures/granite/metallic.png", true);
	unsigned int graniteRoughness = loadTexture("res/textures/granite/roughness.png", true);
	unsigned int graniteAo = loadTexture("res/textures/granite/ao.png", true);

	unsigned int titaniumAlbedo = loadTexture("res/textures/titanium_scuffed/albedo.png", true);
	unsigned int titaniumNormal = loadTexture("res/textures/titanium_scuffed/normal.png", true);
	unsigned int titaniumMetallic = loadTexture("res/textures/titanium_scuffed/metallic.png", true);
	unsigned int titaniumRoughness = loadTexture("res/textures/titanium_scuffed/roughness.png", true);

	unsigned int pirateAlbedo = loadTexture("res/textures/pirate_gold/albedo.png", true);
	unsigned int pirateNormal = loadTexture("res/textures/pirate_gold/normal.png", true);
	unsigned int pirateMetallic = loadTexture("res/textures/pirate_gold/metallic.png", true);
	unsigned int pirateRoughness = loadTexture("res/textures/pirate_gold/roughness.png", true);
	unsigned int pirateAo = loadTexture("res/textures/pirate_gold/ao.png", true);

	unsigned int brickAlbedo = loadTexture("res/textures/bricks/albedo.png", true);
	unsigned int brickNormal = loadTexture("res/textures/bricks/normal.png", true);
	unsigned int brickMetallic = loadTexture("res/textures/bricks/metallic.psd", true);
	//unsigned int brickRoughness = loadTexture("res/textures/bricks/roughness.png", true);
	unsigned int brickAo = loadTexture("res/textures/bricks/ao.png", true);

	unsigned int dustyAlbedo = loadTexture("res/textures/dusty/albedo.png", true);
	unsigned int dustyNormal = loadTexture("res/textures/dusty/normal.png", true);
	unsigned int dustyMetallic = loadTexture("res/textures/dusty/metallic.png", true);
	unsigned int dustyRoughness = loadTexture("res/textures/dusty/roughness.png", true);
	unsigned int dustyAo = loadTexture("res/textures/dusty/ao.png", true);

	unsigned int grassAlbedo = loadTexture("res/textures/grass/albedo.png", true);
	unsigned int grassNormal = loadTexture("res/textures/grass/normal.png", true);
	unsigned int grassMetallic = loadTexture("res/textures/grass/metallic.png", true);
	unsigned int grassRoughness = loadTexture("res/textures/grass/roughness.png", true);
	unsigned int grassAo = loadTexture("res/textures/grass/ao.png", true);

	unsigned int ironAlbedo = loadTexture("res/textures/iron/albedo.png", true);
	unsigned int ironNormal = loadTexture("res/textures/iron/normal.png", true);
	unsigned int ironMetallic = loadTexture("res/textures/iron/metallic.png", true);
	unsigned int ironRoughness = loadTexture("res/textures/iron/roughness.png", true);

	unsigned int paperAlbedo = loadTexture("res/textures/paper/albedo.png", true);
	unsigned int paperNormal = loadTexture("res/textures/paper/normal.png", true);
	unsigned int paperMetallic = loadTexture("res/textures/paper/metallic.psd", true);
	//unsigned int paperRoughness = loadTexture("res/textures/paper/roughness.png", true);
	unsigned int paperAo = loadTexture("res/textures/paper/ao.png", true);

	unsigned int shoreAlbedo = loadTexture("res/textures/shoreline/albedo.png", true);
	unsigned int shoreNormal = loadTexture("res/textures/shoreline/normal.png", true);
	unsigned int shoreMetallic = loadTexture("res/textures/shoreline/metallic.png", true);
	unsigned int shoreRoughness = loadTexture("res/textures/shoreline/roughness.png", true);
	unsigned int shoreAo = loadTexture("res/textures/shoreline/ao.png", true);

	unsigned int steelAlbedo = loadTexture("res/textures/steel/albedo.png", true);
	unsigned int steelNormal = loadTexture("res/textures/steel/normal.png", true);
	unsigned int steelMetallic = loadTexture("res/textures/steel/metallic.psd", true);
	//unsigned int steelRoughness = loadTexture("res/textures/steel/roughness.png", true);
	unsigned int steelAo = loadTexture("res/textures/steel/ao.png", true);

	unsigned int barkAlbedo = loadTexture("res/textures/bark/albedo.png", true);
	unsigned int barkNormal = loadTexture("res/textures/bark/normal.png", true);
	unsigned int barkMetallic = loadTexture("res/textures/bark/metallic.png", true);
	unsigned int barkRoughness = loadTexture("res/textures/bark/roughness.png", true);
	unsigned int barkAo = loadTexture("res/textures/bark/ao.png", true);

	backgroundShader.Bind();
	backgroundShader.SetUniform1i("environmentMap", 0);

	glm::vec3 lightPositions[] = {
		glm::vec3(-7.5f,  7.5f, 7.5f),
		glm::vec3(7.5f,  7.5f, 7.5f),
		glm::vec3(-7.5f, -7.5f, 7.5f),
		glm::vec3(7.5f, -7.5f, 7.5f),
		glm::vec3(0.0f, 0.0f, -20.0f)
	};

	glm::vec3 lightColors[] = {
		glm::vec3(300.0f),
		glm::vec3(300.0f),
		glm::vec3(300.0f),
		glm::vec3(300.0f),
		glm::vec3(300.0f)
	};

	/*glm::vec3 lightPositions[] = {
		glm::vec3(0.0f, 0.0f, 10.0f),
	};

	glm::vec3 lightColors[] = {
		glm::vec3(150.0f),
	};*/

	int nrRows = 7;
	int nrColumns = 7;
	float spacing = 2.5;

	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
	shader.Bind();
	shader.SetUniformMatrix4fv("projection", projection);
	pbrShader.Bind();
	pbrShader.SetUniformMatrix4fv("projection", projection);
	backgroundShader.Bind();
	backgroundShader.SetUniformMatrix4fv("projection", projection);

	// Configure  the viewport to the original framebuffer's screen dimensions
	int scrWidth, scrHeight;
	glfwGetFramebufferSize(window, &scrWidth,  &scrHeight);
	glViewport(0, 0, scrWidth, scrHeight);

	ImGui::CreateContext();
	ImGui_ImplGlfwGL3_Init(window, true);
	ImGui::StyleColorsDark();

	glUseProgram(0);

	// Game Loop
	while (!glfwWindowShouldClose(window))
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processInput(window);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplGlfwGL3_NewFrame();

		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 model = glm::mat4(1.0f);

		// 0 - Render floor and point light
		shader.Bind();
		shader.SetUniformMatrix4fv("view", view);
		shader.SetUniform3f("viewPos", camera.Position);
		shader.SetUniform3f("lightPos", lightPos);

		glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sandAlbedo);
		glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, sandSpecular);
		glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, sandNormal);

		model = glm::translate(model, glm::vec3(0.0f, -10.0f, 0.0f));
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(200.0f, 200.0f, 200.0f));
		shader.SetUniformMatrix4fv("model", model);
		renderQuadNormal();

		// 0.5 - Setup uniforms and IBL textures
		pbrShader.Bind();
		pbrShader.SetUniformMatrix4fv("view", view);
		pbrShader.SetUniform3f("viewPos", camera.Position);
		pbrShader.SetUniform3f("albedoF", albedoF);
		pbrShader.SetUniform1f("aoF", aoF);
		pbrShader.SetUniform1i("lightSource", 0);

		glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
		glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
		glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

		// 1.0 - Render textured spheres
			// left wall
				// gold
		pbrShader.SetUniform1i("aoTexture", 0);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, goldAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, goldNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, goldMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, goldRoughness);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-10.0f, 0.0f, 2.5f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// alien metal
		pbrShader.SetUniform1i("aoTexture", 1);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, alienAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, alienNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, alienMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, alienRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, alienAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-10.0f, 0.0f, 5.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// limestone
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, limestoneAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, limestoneNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, limestoneMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, limestoneRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, limestoneAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-10.0f, 0.0f, 7.5f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// wood
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, woodAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, woodNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, woodMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, woodRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, woodAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-10.0f, 0.0f, 10.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// granite
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, graniteAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, graniteNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, graniteMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, graniteRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, graniteAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-10.0f, 0.0f, 12.5f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

			//back wall
				// titanium
		pbrShader.SetUniform1i("aoTexture", 0);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, titaniumAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, titaniumNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, titaniumMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, titaniumRoughness);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-5.0f, 0.0f, 15.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// pirate gold
		pbrShader.SetUniform1i("aoTexture", 1);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, pirateAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, pirateNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, pirateMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, pirateRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, pirateAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-2.5f, 0.0f, 15.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// bricks
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, brickAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, brickNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, brickMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, brickAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 15.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// dusty
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, dustyAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, dustyNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, dustyMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, dustyRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, dustyAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(2.5f, 0.0f, 15.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// grass
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, grassAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, grassNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, grassMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, grassRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, grassAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(5.0f, 0.0f, 15.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

			// right wall
				// iron
		pbrShader.SetUniform1i("aoTexture", 0);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, ironAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, ironNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, ironMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, ironRoughness);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(10.0f, 0.0f, 2.5f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// paper
		pbrShader.SetUniform1i("aoTexture", 1);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, paperAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, paperNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, paperMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, paperAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(10.0f, 0.0f, 5.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// shore
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, shoreAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, shoreNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, shoreMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, shoreRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, shoreAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(10.0f, 0.0f, 7.5f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// steel
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, steelAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, steelNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, steelMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, steelAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(10.0f, 0.0f, 10.0f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

				// bark
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, barkAlbedo);
		glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, barkNormal);
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, barkMetallic);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, barkRoughness);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, barkAo);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(10.0f, 0.0f, 12.5f));
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

		// 2.0 - render rows * columns of spheres
		pbrShader.SetUniform1i("texture_none", 1);
		for (int row = 0; row < nrRows; ++row)
		{	
			pbrShader.SetUniform1f("metallicF", (float)row / (float)nrRows);
			for (int col = 0; col < nrColumns; ++col)
			{
				pbrShader.SetUniform1f("roughnessF", glm::clamp((float)col / (float)nrColumns, 0.05f, 1.0f));

				model = glm::mat4(1.0f);
				model = glm::translate(model, glm::vec3(
					(float)(col - (nrColumns / 2)) * spacing,
					(float)(row - (nrRows / 2)) * spacing,
					-2.0f
				));
				pbrShader.SetUniformMatrix4fv("model", model);
				renderSphere();
			}
		}
		pbrShader.SetUniform1i("texture_none", 0);

		// 3.0 - render light sources
		pbrShader.SetUniform1i("lightSource", 1);
		for (unsigned int i = 0; i < sizeof(lightPositions) / sizeof(lightPositions[0]); ++i)
		{
			glm::vec3 newPos = lightPositions[i] + glm::vec3(sin(glfwGetTime() * 5.0) * 5.0, 0.0, 0.0);
			newPos = lightPositions[i];
			pbrShader.SetUniform3f("lightPositions[" + std::to_string(i) + "]", newPos);
			pbrShader.SetUniform3f("lightColors[" + std::to_string(i) + "]", lightColors[i]);

			model = glm::mat4(1.0f);
			model = glm::translate(model, newPos);
			model = glm::scale(model, glm::vec3(0.5f));
			pbrShader.SetUniformMatrix4fv("model", model);
			renderSphere();
		}

		model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		pbrShader.SetUniformMatrix4fv("model", model);
		renderSphere();

		// 4.0 - render cubemap
		backgroundShader.Bind();
		backgroundShader.SetUniformMatrix4fv("view", view);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
		//glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap); // display irradiance map
		//glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap); // display prefilter map
		renderCube();

		// render BRDF map to screen
		//brdfShader.Bind();
		//renderQuad();

		// ImGui Window
		ImGui::Begin("Main Window", NULL, ImGuiWindowFlags_AlwaysAutoResize);
		{
			if (ImGui::CollapsingHeader("Rendering"))
			{
				ImGui::SliderFloat3("Albedo", &albedoF.x, 0.0f, 1.0f, "%.1f", 1);
				ImGui::SliderFloat("AO", &aoF, 0.0f, 1.0f, "%.1f", 1);
			}
			
			if (ImGui::CollapsingHeader("Application Info"))
			{
				ImGui::Text("OpenGL Version: %s", glGetString(GL_VERSION));
				ImGui::Text("Shader Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
				ImGui::Text("Hardware: %s", glGetString(GL_RENDERER));
				ImGui::NewLine();
				ImGui::Text("Frametime: %.3f / Framerate: (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			}

			if (ImGui::CollapsingHeader("About"))
			{
				ImGui::Text("PBR & IBL Demo by Kyle Robinson");
				ImGui::NewLine();
				ImGui::Text("Email: kylerobinson456@outlook.com");
				ImGui::Text("Twitter: @KyleRobinson42");
			}
		}
		ImGui::End();
		ImGui::Render();
		ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());

		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	glDeleteFramebuffers(1, &captureFBO);
	glDeleteRenderbuffers(1, &captureRBO);

	glDeleteVertexArrays(1, &sphereVAO);
	glDeleteVertexArrays(1, &cubeVAO);
	glDeleteVertexArrays(1, &quadVAO);

	glDeleteBuffers(1, &cubeVBO);
	glDeleteBuffers(1, &quadVBO);

	glDeleteTextures(1, &hdrTexture);
	glDeleteTextures(1, &envCubemap);
	glDeleteTextures(1, &envCubemap);
	glDeleteTextures(1, &prefilterMap);
	glDeleteTextures(1, &brdfLUTTexture);

	ImGui_ImplGlfwGL3_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	return 0;
}

GLFWwindow* InitWindow()
{
	if (!glfwInit())
	{
		std::cout << "Failed to initialise GLFW!" << std::endl;
		return nullptr;
	}
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "PBR & Speuclar IBL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window." << std::endl;
		glfwTerminate();
		return nullptr;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowPos(window, 100, 100);

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialise GLAD." << std::endl;
		return nullptr;
	}

	std::cout << "Using GL Version: " << glGetString(GL_VERSION) << std::endl << std::endl;
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	return window;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	if (mouseActive)
		camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	// Camera Movement
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);

	// Polygon Mode
	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);

	// Mouse Control
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
	{
		mouseActive = true;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
	{
		mouseActive = false;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}

unsigned int loadTexture(const char* path, bool gammaCorrection)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrChannels;
	unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
	if (data)
	{
		GLenum internalFormat;
		GLenum dataFormat;
		if (nrChannels == 1)
			internalFormat = dataFormat = GL_RED;
		else if (nrChannels == 3)
		{
			internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
			dataFormat = GL_RGB;
		}
		else if (nrChannels == 4)
		{
			internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
			dataFormat = GL_RGBA;
		}

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, dataFormat == (GL_RGBA || GL_RGB) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, dataFormat == (GL_RGBA || GL_RGB) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Failed to load texture: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}

void renderSphere()
{
	if (sphereVAO == 0)
	{
		glGenVertexArrays(1, &sphereVAO);

		unsigned int vbo, ebo;
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> uv;
		std::vector<glm::vec3> normals;
		std::vector<unsigned int> indices;

		const unsigned int X_SEGMENTS = 128;
		const unsigned int Y_SEGMENTS = 128;
		const float PI = 3.14159265359;
		for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
		{
			for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
			{
				float xSegment = (float)x / (float)X_SEGMENTS;
				float ySegment = (float)y / (float)Y_SEGMENTS;
				float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
				float yPos = std::cos(ySegment * PI);
				float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

				positions.push_back(glm::vec3(xPos, yPos, zPos));
				uv.push_back(glm::vec2(xSegment, ySegment));
				normals.push_back(glm::vec3(xPos, yPos, zPos));
			}
		}

		bool oddRow = false;
		for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
		{
			if (!oddRow) // even rows
			{
				for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
				{
					indices.push_back(y * (X_SEGMENTS + 1) + x);
					indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
				}
			}
			else
			{
				for (int x = X_SEGMENTS; x >= 0; --x)
				{
					indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
					indices.push_back(y * (X_SEGMENTS + 1) + x);
				}
			}
			oddRow = !oddRow;
		}
		indexCount = indices.size();

		std::vector<float> data;
		for (unsigned int i = 0; i < positions.size(); ++i)
		{
			data.push_back(positions[i].x);
			data.push_back(positions[i].y);
			data.push_back(positions[i].z);
			if (uv.size() > 0)
			{
				data.push_back(uv[i].x);
				data.push_back(uv[i].y);
			}
			if (normals.size() > 0)
			{
				data.push_back(normals[i].x);
				data.push_back(normals[i].y);
				data.push_back(normals[i].z);
			}
		}

		glBindVertexArray(sphereVAO);
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		float stride = (3 + 2 + 3) * sizeof(float);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
		
		glBindVertexArray(0);
	}

	glBindVertexArray(sphereVAO);
	glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void renderCube()
{
	if (cubeVAO == 0)
	{
		float vertices[] = {
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
		};

		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

		glBindVertexArray(cubeVAO);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

		glBindVertexArray(0);
	}

	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

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

		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);

		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

		glBindVertexArray(quadVAO);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

		glBindVertexArray(0);
	}
	
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void renderQuadNormal()
{
	if (quadNormalVAO == 0)
	{
		// positions
		glm::vec3 pos1(-1.0, 1.0, 0.0);
		glm::vec3 pos2(-1.0, -1.0, 0.0);
		glm::vec3 pos3(1.0, -1.0, 0.0);
		glm::vec3 pos4(1.0, 1.0, 0.0);

		// texture coords
		glm::vec2 uv1(0.0, 20.0);
		glm::vec2 uv2(0.0, 0.0);
		glm::vec2 uv3(20.0, 0.0);
		glm::vec2 uv4(20.0, 20.0);

		// normal vector
		glm::vec3 nm(0.0, 0.0, 1.0);

		// tangent / bitangent
		glm::vec3 tangent1, bitangent1;
		glm::vec3 tangent2, bitangent2;

		// triangle 1
		glm::vec3 edge1 = pos2 - pos1;
		glm::vec3 edge2 = pos3 - pos1;
		glm::vec2 deltaUV1 = uv2 - uv1;
		glm::vec2 deltaUV2 = uv3 - uv1;

		float f = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

		tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
		tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
		tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

		bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
		bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
		bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

		// triangle 2
		edge1 = pos3 - pos1;
		edge2 = pos4 - pos1;
		deltaUV1 = uv3 - uv1;
		deltaUV2 = uv4 - uv1;

		f = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

		tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
		tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
		tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

		bitangent2.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
		bitangent2.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
		bitangent2.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

		float quadVertices[] = {
			// positions            // normal         // texcoords  // tangent                          // bitangent
			pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
			pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
			pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,

			pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
			pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
			pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z
		};

		glGenVertexArrays(1, &quadNormalVAO);
		glGenBuffers(1, &quadNormalVBO);

		glBindVertexArray(quadVAO);

		glBindBuffer(GL_ARRAY_BUFFER, quadNormalVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(6 * sizeof(float)));

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));

		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));

		glBindVertexArray(0);
	}

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}