// Includes and defs --------------------------

// openGL functionality
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// shader helper
#include "shader.h"
// math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
// image loading
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// Functions ---------------------------------

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// Global variables ---------------------------

// window
const int SCR_WIDTH = 1280;
const int SCR_HEIGHT = 720;

// camera
glm::vec3 cameraPos = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f, pitch = 0.0f;
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;

// time
float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame

// cloth
struct ClothPoint {
	glm::vec3 pos, vel, forces, norm, prevPos;
	glm::vec2 uv;
};
const int columns = 30;
const int rows = 30;
const int numPoints = rows * columns;
ClothPoint points[numPoints];

struct Spring {
	ClothPoint* point1;
	ClothPoint* point2;
	float restLen, k, vK;
};
//const int numSprings = (rows) * (columns-1);
const int numSprings = (rows) * (columns - 1) + (rows - 1) * (columns);// +(rows) * (columns) * 4;
Spring springs[numSprings];

// cloth physics
float clothK = 4.00f;
float dampK = 0.09f;
float crossClothK = 9.0f;
float crossDampK = 0.2f;
float clothMass = 0.244f / numPoints; //Actual mass of flag in kg/m2
float airDensity = 1.0f; // Actual density is 1.225 kg/m3 apparently
float clothDragCoef = 0.01f;


float timeInterval = 0.001;
glm::vec3 grav = glm::vec3(0.0f, -9.8f, 0.0f);

bool eularianIntegration = false;

// Sphere
const float sphereR = 2.0f;
glm::vec3 spherePos = glm::vec3(3.0f, sphereR, -3.0f);

int main()
{
	// Before loop starts ---------------------
	// glfw init
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// glfw window creation
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "5611 HW2", NULL, NULL);
	glfwMakeContextCurrent(window);

	// register callbacks
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	//Register mouse movement callback
	glfwSetCursorPosCallback(window, mouse_callback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Initialize glad
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	// Enable openGL settings
	//glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	// Setup ----------------------------------

	// Cloth data
		// initialize points
	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < columns; j++)
		{
			// Inital cloth points
			float scale = 4.0f;
			//points[i * columns + j].pos = glm::vec3(3.0f + scale * 1.92f * ((float)i / rows), 6.0f - scale * 1.220f * ((float)j / columns), 3.0f);
			points[i * columns + j].pos = glm::vec3(3.0f + scale * 1.92f * ((float)i / rows), 6.0f, 3.0f - scale * 1.220f * ((float)j / columns));
			points[i * columns + j].prevPos = points[i * columns + j].pos;
			points[i * columns + j].vel = glm::vec3(0.0f);
			points[i * columns + j].forces = glm::vec3(0.0f);
			points[i * columns + j].uv = glm::vec2(i / (float)rows, j / (float)columns);
			points[i * columns + j].norm = glm::vec3(0.0f, 0.0f, 1.0f);
		}
	}
		// initialize springs
	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < columns; j++)
		{
			// Horizontal springs - there are (rows) * (columns-1) of these
			if (j < columns - 1)
			{
				Spring &s = springs[i * (columns - 1) + j];
				//std::cout << i * (columns - 1) + j << std::endl;
				s.point1 = &points[i * columns + j];
				s.point2 = &points[(i + 0) * columns + (j + 1)];
				s.restLen = glm::length(s.point1->pos - s.point2->pos);
				s.k = clothK;
				s.vK = dampK;
			}

			// Verticle springs - there are (rows-1) * (column) of these
			if (i < rows - 1)
			{
				Spring &s = springs[(rows) * (columns - 1) + i * columns + j];
				//std::cout << (rows) * (columns - 1) + i * columns + j << std::endl;
				s.point1 = &points[i * columns + j];
				s.point2 = &points[(i + 1) * columns + j];
				s.restLen = glm::length(s.point1->pos - s.point2->pos);
				s.k = clothK;
				s.vK = dampK;
			}

			// Cross springs
			/*
			if (i < rows - 2)// all but last 2 rows
			{
				if (i % 2 == 0 && j % 2 == 0) // every other point
				{
					Spring&s = springs[(rows) * (columns - 1) + (rows - 1) * (columns)+i * columns + j];
					if (j%columns - 2 >= 0) // down left spring - only if you can move two to your left
					{
						std::cout << (rows) * (columns - 1) + (rows - 1) * (columns)+i * columns + j << std::endl;
						s.point1 = &points[i * columns + j];
						s.point2 = &points[(i - 2) * columns + j + 2];
						s.restLen = glm::length(s.point1->pos - s.point2->pos);
						s.k = crossClothK;
						s.vK = crossDampK;
					}
					if (j % columns + 2 <= columns) // down right spring - only if you can move two to the right
					{
						std::cout << (rows) * (columns - 1) + (rows - 1) * (columns)+i * columns + j << std::endl;
						s.point1 = &points[i * columns + j];
						s.point2 = &points[(i + 2) * columns + j + 2];
						s.restLen = glm::length(s.point1->pos - s.point2->pos);
						s.k = crossClothK;
						s.vK = crossDampK;
					}
				}
			}
			//*/
		}
	}
	// Cross springs
	for (int i = 0; i < rows/2; i++)
	{
		for (int j = 0; j < columns/2; j++)
		{
			// There are (rows) * (columns - 1) + (rows - 1) * (columns) spings already
			// Spring&s = springs[(rows) * (columns - 1) + (rows - 1) * (columns) + (columns / 2) * i + j];
			//std::cout << (rows) * (columns - 1) + (rows - 1) * (columns)+(columns / 2) * i + j << std::endl;


		}
	}

	// Set up indices for rendering
	const int numFaces = (rows - 1) * (columns - 1) * 2;
	unsigned int clothIndices[numFaces * 3];
	for (int i = 0; i < rows-1; i++)
	{
		for (int j = 0; j < columns-1; j++)
		{
			// Wind counter-clockwise
			int index = (i * (columns-1) + j) * 6;

			clothIndices[index] = (i) * columns + (j);
			clothIndices[index+1] = (i+1) * columns + (j);
			clothIndices[index+2] = (i+1) * columns + (j+1);

			clothIndices[index+3] = (i)* columns + (j);
			clothIndices[index+4] = (i+1)* columns + (j+1);
			clothIndices[index+5] = (i)* columns + (j+1);
		}
	}

	// Cloth rendering
	Shader clothShader("cloth.vert", "cloth.frag");
	glm::vec3 clothVertices[numPoints];
	for (int i = 0; i < numPoints; i++)
	{
		clothVertices[i] = points[i].pos;
	}
	glm::vec2 clothUVs[numPoints];
	for (int i = 0; i < numPoints; i++)
	{
		clothUVs[i] = points[i].uv;
	}
	glm::vec3 clothNormals[numPoints];
	for (int i = 0; i < numPoints; i++)
	{
		clothNormals[i] = points[i].norm;
	}
	unsigned int clothVAO;
	glGenVertexArrays(1, &clothVAO);
	glBindVertexArray(clothVAO);

	unsigned int clothPosBuffer;
	glGenBuffers(1, &clothPosBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, clothPosBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(clothVertices), clothVertices, GL_STREAM_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glEnableVertexAttribArray(0);

	unsigned int clothNormBuffer;
	glGenBuffers(1, &clothNormBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, clothNormBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(clothNormals), clothNormals, GL_STREAM_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glEnableVertexAttribArray(1);

	unsigned int clothUVBuffer;
	glGenBuffers(1, &clothUVBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, clothUVBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(clothUVs), clothUVs, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glEnableVertexAttribArray(2);


	unsigned int clothElementBuffer;
	glGenBuffers(1, &clothElementBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, clothElementBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(clothIndices), clothIndices, GL_STATIC_DRAW);

	// Flag texture
	// Set up textures
	unsigned int flagTexture;
	glGenTextures(1, &flagTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, flagTexture);
	// load and generate the texture
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(false);
	unsigned char *data = stbi_load("Flag.png", &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);


	// Floor
	float floorVertices[] = {
		//x			y		z			nX		nY		nZ		t		s
		-20.0f,		0.0f,	-20.0f,		0.0f,	1.0f,	0.0f,	0.0f, 0.0f,
		-20.0f,		0.0f,	20.0f,		0.0f,	1.0f,	0.0f,	0.0f, 8.0f,
		 20.0f,		0.0f,	-20.0f,		0.0f,	1.0f,	0.0f,	8.0f, 0.0f,
		 20.0f,		0.0f,	20.0f,		0.0f,	1.0f,	0.0f,	8.0f, 8.0f
	};
	int floorIndices[] = {
		0, 1, 2,
		1, 3, 2
	};
	// Buffer stuff for floor
	unsigned int floorVAO, floorVBO, floorEBO;
	glGenVertexArrays(1, &floorVAO);
	glBindVertexArray(floorVAO);
	glGenBuffers(1, &floorVBO);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
	glGenBuffers(1, &floorEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floorIndices), floorIndices, GL_STATIC_DRAW);
	// Tell OpenGL how to use vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); //Uses whatever VBO is bound to GL_ARRAY_BUFFER
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	// Floor texture
	// Set up textures
	unsigned int gridTexture;
	glGenTextures(1, &gridTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gridTexture);
	// load and generate the texture
	stbi_set_flip_vertically_on_load(true);
	data = stbi_load("grid.png", &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);

	//Shader
	Shader texturedShader("textured.vert", "textured.frag");


	// Sphere
	const int xSegments = 20;
	const int ySegments = 20;
	float sphereVertices[xSegments * ySegments * 3];

	float PI = 3.14159265f;
	for (unsigned int y = 0; y < ySegments; y++)
	{
		for (unsigned int x = 0; x < xSegments; x++)
		{
			float xSegment = (float)x / (float)xSegments;
			float ySegment = (float)y / (float)ySegments;

			int index = (y*xSegments + x) * 3;
			sphereVertices[index] = std::cos(xSegment * PI*2.0f) * std::sin(ySegment * PI); // x pos
			sphereVertices[index + 1] = std::cos(ySegment * PI); // y pos
			sphereVertices[index + 2] = std::sin(xSegment * PI*2.0f) * std::sin(ySegment * PI); // z pos
		}
	}
	// Sphere indices
	int sphereIndices[(xSegments) * (ySegments) * 6]; // Make sure this is sized correctly
	for (unsigned int y = 0; y < (ySegments); y++) // Dont go below last row
	{
		for (unsigned int x = 0; x < (xSegments); x++) // Dont go after last column
		{
			int index = (y*xSegments + x) * 6;
			sphereIndices[index] = ((y)*xSegments) + x;				// 0
			sphereIndices[index + 1] = ((y + 1)*xSegments) + x;		// |
			sphereIndices[index + 2] = ((y + 1)*xSegments) + x + 1;	// 0--0

			sphereIndices[index + 3] = ((y)*xSegments) + x;			// 0--0
			sphereIndices[index + 4] = ((y)*xSegments) + x + 1;		//  \ |
			sphereIndices[index + 5] = ((y + 1)*xSegments) + x + 1;	//    0
		}
	}
	// Buffer stuff for sphere
	unsigned int sphereVAO, sphereVBO, sphereEBO;
	glGenVertexArrays(1, &sphereVAO);
	glBindVertexArray(sphereVAO);
	glGenBuffers(1, &sphereVBO);
	glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(sphereVertices), sphereVertices, GL_STATIC_DRAW);
	glGenBuffers(1, &sphereEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sphereIndices), sphereIndices, GL_STATIC_DRAW);
	// Tell OpenGL how to use vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); //Uses whatever VBO is bound to GL_ARRAY_BUFFER
	glEnableVertexAttribArray(0);

	// uncomment this call to draw in wireframe polygons.
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// render loop ----------------------------
	while (!glfwWindowShouldClose(window))
	{
		// Set deltaT
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		std::cout << 1.0f/deltaTime << std::endl;
		if (timeInterval != 0.0f)
			deltaTime = timeInterval;

		// input
		processInput(window);

		// processing
		// Process for each spring
			for (int i = 0; i < numSprings; i++)
			{
				// Apply a force to both points you are connected to
				Spring &s = springs[i];
				glm::vec3 displacement = s.point1->pos - s.point2->pos;
				glm::vec3 dir = glm::normalize(displacement);
				float len = glm::length(displacement);
				float sForce = (s.restLen - len) * clothK;
				s.point1->forces += sForce * dir;
				s.point2->forces -= sForce * dir;
				// Dampen velocities
				float v1 = glm::dot(s.point1->vel, dir);
				float v2 = glm::dot(s.point2->vel, dir);
				glm::vec3 dForce = dir * dampK * (v1 - v2);
				s.point1->forces -= dForce;
				s.point2->forces += dForce;
				
				/* // Old method
				glm::vec3 dForce2 = dampK / 1000.0f * (s.point1->vel - s.point2->vel);
				s.point1->forces -= dForce2;
				s.point2->forces += dForce2;
				//*/
			}
		// Process each face
			for (int i = 0; i < numFaces; i += 1)
			{
				// Get points for each point on face
				ClothPoint &p1 = points[clothIndices[i * 3]];
				ClothPoint &p2 = points[clothIndices[i * 3 + 1]];
				ClothPoint &p3 = points[clothIndices[i * 3 + 2]];
				// Drag
				// f = -1/2p*length(v)*DragCoef*area*normal
				glm::vec3 airVel = glm::vec3(0.0f, 0.0f, 0.001f);
				// v is velocity of face - velocity of the air
				glm::vec3 v = (p1.vel + p2.vel + p3.vel) / 3.0f + airVel;
				// use cross product and normalize to get n
				glm::vec3 cross = glm::cross((p1.pos - p2.pos), (p1.pos - p3.pos)); //Pull this out to reuse
				glm::vec3 n = glm::normalize(cross);
				// area of face is half of the area of parallelogram, dot this with velocity to get area exposed to flow
				float a = glm::dot((0.5f * cross), glm::normalize(v));
				// put all together to get drag
				glm::vec3 dragForce = -0.5f * airDensity * glm::length(v) * clothDragCoef * a * n;
				// Give each point on face 1/3 of force
				bool drag = true;
				if (drag)
				{	
					p1.forces += dragForce / 3.0f;
					p2.forces += dragForce / 3.0f;
					p3.forces += dragForce / 3.0f;
				}
			p1.norm += n;
			p2.norm += n;
			p3.norm += n;
		}
		// Process each non-fixed point
		for (int i = 0; i < numPoints; i++)
		{
			ClothPoint &p = points[i];
			if (i % columns != 0)
			{
				// Gravity
				p.forces += grav * clothMass;
				//p.forces -= 0.0001f * p.vel;

				// Now integrate forces
				glm::vec3 accel = p.forces / clothMass;
				// Integrate velocity
				if (eularianIntegration)
				{
					p.pos += p.vel * deltaTime;
					p.vel += accel * deltaTime;
				}
				else
				{
					p.vel += accel * deltaTime;
					glm::vec3 temp = p.prevPos;
					p.prevPos = p.pos;
					p.pos = 2.0f * p.pos - temp + accel * deltaTime*deltaTime;
				}

				// Collisions
				if (p.pos[1] < 0.01f)
				{
					p.pos[1] = 0.01f;
					p.vel[1] *= -0.95;
				}
				glm::vec3 off = (p.pos - spherePos);
				float buffer = 0.03f;
				if (glm::length(off) < (sphereR + buffer))
				{
					p.pos = spherePos + normalize(off)*(sphereR + buffer);
					p.vel = glm::reflect(p.vel, normalize(off));
				}

				p.forces = glm::vec3(0.0f);

			}

			// Calculate normals
			// normal should have been added from each face, so we just normalize
			p.norm += glm::vec3(0.0f, 0.0f, 0.01f);
			p.norm = p.norm / glm::length(p.norm);
			//std::cout << glm::length(p.norm) << std::endl;
			
		}

		for (int i = 0; i < numPoints; i++)
		{
			clothVertices[i] = points[i].pos;
			clothNormals[i] = points[i].norm;
		}
		glBindBuffer(GL_ARRAY_BUFFER, clothPosBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(clothVertices), clothVertices, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, clothNormBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(clothNormals), clothNormals, GL_STREAM_DRAW);


		// rendering commands here
		glClearColor(0.2f, 0.4f, 0.4f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		glm::mat4 projection = glm::mat4(1.0f);
		projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 model = glm::mat4(1.0f);

		glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, texture);
		texturedShader.use();
		texturedShader.setMat4("view", view);
		texturedShader.setMat4("projection", projection);
		texturedShader.setMat4("model", model);

		texturedShader.setVec3("viewPos", cameraPos);
		texturedShader.setVec3("light.direction", glm::vec3(0.0f, -1.0f, 1.0f));
		texturedShader.setVec3("light.ambient", glm::vec3(0.3f, 0.3f, 0.3f));
		texturedShader.setVec3("light.diffuse", glm::vec3(0.9f, 0.9f, 0.9f));
		texturedShader.setVec3("light.specular", glm::vec3(1.0f, 1.0f, 1.0f));

		texturedShader.setInt("material.diffuse",0);
		texturedShader.setVec3("material.specular", glm::vec3(0.5f, 0.5f, 0.5f));
		texturedShader.setFloat("material.shininess", 0.1f);
		glBindVertexArray(floorVAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		texturedShader.setInt("material.diffuse", 1);
		glBindVertexArray(clothVAO);
		glDrawElements(GL_TRIANGLES, (rows - 1) * (columns - 1) * 6, GL_UNSIGNED_INT, 0);
		
		clothShader.use();
		clothShader.setVec4("col", glm::vec4(0.9f, 0.8f, 0.6f, 1.0f));
		clothShader.setMat4("view", view);
		clothShader.setMat4("projection", projection);

		model = glm::translate(model, spherePos);
		model = glm::scale(model, glm::vec3(sphereR));
		clothShader.setMat4("model", model);
		glBindVertexArray(sphereVAO);
		glDrawElements(GL_TRIANGLES, (xSegments) * (ySegments) * 6, GL_UNSIGNED_INT, 0);

		model = glm::mat4(1.0f);
		clothShader.setMat4("model", model);

		// check and call events and swap the buffers
		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	glfwTerminate();

	//while (true) {} // Uncomment to see output after you close window

	return 0;
}

// This function is called whenever window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// Process all ketboard input here
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	float cameraSpeed = 4.0f * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraUp;
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraUp;

	float sphereSpeed = 2.0f * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		spherePos[0] += sphereSpeed;
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		spherePos[0] -= sphereSpeed;
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		spherePos[2] += sphereSpeed;
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		spherePos[2] -= sphereSpeed;

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

	float sensitivity = 0.2;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

	cameraFront = glm::normalize(front);
}