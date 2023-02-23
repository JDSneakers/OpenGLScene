#include <iostream>
#include <cstdlib>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "camera.h"

using namespace std;

//Shader program
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif


namespace
{
    // window title
    const char* const WINDOW_TITLE = "Final Project - John Austin";

    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    
    struct GLMesh
    {
        GLuint vao;
        GLuint vbo;
        GLuint nVertices;
    };

    // Main GLFW window
    GLFWwindow* gWindow = nullptr;
    GLMesh gMesh;

    // Texture
    GLuint gTextureIdBlack, gTextureIdScreen, gTextureIdWood, gTextureIdKeyboard, gTextureIdPhoto;
    glm::vec2 gUVScale(1.0f, 1.0f);
    GLint gTexWrapMode = GL_REPEAT;

    // Shader programs
    GLuint gObjectsProgramId;
    GLuint gLampProgramId;

    // camera
    Camera gCamera(glm::vec3(0.0f, 0.0f, 3.0f));
    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    bool gFirstMouse = true;

    // timing
    float gDeltaTime = 0.0f;
    float gLastFrame = 0.0f;

    // Subject position and scale
    glm::vec3 gObjectsPosition(0.0f, 0.0f, 0.0f);
    glm::vec3 gObjectsScale(1.0f);

    // Objects and light color
    glm::vec3 gObjectColor(1.f, 0.2f, 0.0f);
    glm::vec3 gLightColor(1.0f, 1.0f, 1.0f);

    // Light position and scale
    glm::vec3 gLightPosition(0.5f, 2.5f, 3.0f);
    glm::vec3 gLightScale(1.0f);

    // Lamp animation
    bool gIsLampOrbiting = false;
}

/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void UCreateMesh(GLMesh& mesh);
void UDestroyMesh(GLMesh& mesh);
bool UCreateTexture(const char* filename, GLuint& textureId);
void UDestroyTexture(GLuint textureId);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);


/* Obejcts Vertex Shader Source Code*/
const GLchar* objectsVertexShaderSource = GLSL(440,

    layout(location = 0) in vec3 position; // VAP position 0 for vertex position data
    layout(location = 1) in vec3 normal; // VAP position 1 for normals
    layout(location = 2) in vec2 textureCoordinate;
    
    out vec3 vertexNormal; // For outgoing normals to fragment shader
    out vec3 vertexFragmentPos; // For outgoing color to fragment shader
    out vec2 vertexTextureCoordinate;
    
    //Global variables for the  transform matrices
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0f); // transforms vertices into clip coordinates
    
        vertexFragmentPos = vec3(model * vec4(position, 1.0f)); // Gets fragment / pixel position in world space only
    
        vertexNormal = mat3(transpose(inverse(model))) * normal; // get normal vectors in world space only
        vertexTextureCoordinate = textureCoordinate;
    }
);


/* Objects Fragment Shader Source Code*/
const GLchar* objectsFragmentShaderSource = GLSL(440,

    in vec3 vertexNormal; // For incoming normals
    in vec3 vertexFragmentPos; // For incoming fragment position
    in vec2 vertexTextureCoordinate;
    
    out vec4 fragmentColor;
    
    // Global variables for object color, light color, light position, and camera/view position
    uniform vec3 objectColor;
    uniform vec3 lightColor;
    uniform vec3 lightPos;
    uniform vec3 viewPosition;
    uniform sampler2D uTexture;
    uniform vec2 uvScale;
    
    void main()
    {
    
        // Ambient lighting
        float ambientStrength = 0.5f; // Set ambient or global lighting strength
        vec3 ambient = ambientStrength * lightColor; // Generate ambient light color
    
        // Diffuse lighting
        vec3 norm = normalize(vertexNormal); // Normalize vectors to 1 unit
        vec3 lightDirection = normalize(lightPos - vertexFragmentPos); // Calculate distance (light direction) between light source and fragments/pixels on objects
        float impact = max(dot(norm, lightDirection), 0.0);// Calculate diffuse impact by generating dot product of normal and light
        vec3 diffuse = impact * lightColor; // Generate diffuse light color
    
        // Specular lighting
        float specularIntensity = 0.8f; 
        float highlightSize = 16.0f; 
        vec3 viewDir = normalize(viewPosition - vertexFragmentPos);
        vec3 reflectDir = reflect(-lightDirection, norm);
        float specularComponent = pow(max(dot(viewDir, reflectDir), 0.0), highlightSize);
        vec3 specular = specularIntensity * specularComponent * lightColor;
    
        // Texture holds the color to be used for all three components
        vec4 textureColor = texture(uTexture, vertexTextureCoordinate * uvScale);
    
        // Calculate phong result
        vec3 phong = (ambient + diffuse + specular) * textureColor.xyz;
    
        fragmentColor = vec4(phong, 1.0); // Send lighting results to GPU
    }
);


/* Lamp Shader Source Code*/
const GLchar* lampVertexShaderSource = GLSL(440,

    layout(location = 0) in vec3 position; // VAP position 0 for vertex position data

    //Global variables for the  transform matrices
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0f); // Transforms vertices into clip coordinates
    }
);


/* Fragment Shader Source Code*/
const GLchar* lampFragmentShaderSource = GLSL(440,

    out vec4 fragmentColor;

    void main()
    {
        fragmentColor = vec4(1.0f);
    }
);


// flip images
void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
    for (int j = 0; j < height / 2; ++j)
    {
        int index1 = j * width * channels;
        int index2 = (height - 1 - j) * width * channels;

        for (int i = width * channels; i > 0; --i)
        {
            unsigned char tmp = image[index1];
            image[index1] = image[index2];
            image[index2] = tmp;
            ++index1;
            ++index2;
        }
    }
}


int main(int argc, char* argv[])
{
    if (!UInitialize(argc, argv, &gWindow))
        return EXIT_FAILURE;

    UCreateMesh(gMesh);

    // Create the shader programs
    if (!UCreateShaderProgram(objectsVertexShaderSource, objectsFragmentShaderSource, gObjectsProgramId))
        return EXIT_FAILURE;
    if (!UCreateShaderProgram(lampVertexShaderSource, lampFragmentShaderSource, gLampProgramId))
        return EXIT_FAILURE;

    // Computer Body texture
    const char* texFilename = "blackPlastic.jpg";
    if (!UCreateTexture(texFilename, gTextureIdBlack))
    {
        cout << "Failed to load texture " << texFilename << endl;
        return EXIT_FAILURE;
    }

    // Computer screen texture
    const char* texFilename2 = "screen.jpg";
    if (!UCreateTexture(texFilename2, gTextureIdScreen))
    {
        cout << "Failed to load texture " << texFilename2 << endl;
        return EXIT_FAILURE;
    }

    // Desk texture
    const char* texFilename3 = "wood.jpg";
    if (!UCreateTexture(texFilename3, gTextureIdWood))
    {
        cout << "Failed to load texture " << texFilename3 << endl;
        return EXIT_FAILURE;
    }

    // Keyboard texture
    const char* texFilename4 = "keyboard.jpg";
    if (!UCreateTexture(texFilename4, gTextureIdKeyboard))
    {
        cout << "Failed to load texture " << texFilename4 << endl;
        return EXIT_FAILURE;
    }

    // Glass Photo texture
    const char* texFilename5 = "photo.png";
    if (!UCreateTexture(texFilename5, gTextureIdPhoto))
    {
        cout << "Failed to load texture " << texFilename5 << endl;
        return EXIT_FAILURE;
    }

    // tell each sampler which texture unit it belongs to
    glUseProgram(gObjectsProgramId);
    glUniform1i(glGetUniformLocation(gObjectsProgramId, "uTexture"), 0);

    // Sets the background color of the window to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // render loop
    while (!glfwWindowShouldClose(gWindow))
    {
        // per-frame timing
        float currentFrame = glfwGetTime();
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        // input
        UProcessInput(gWindow);

        // Render this frame
        URender();
        glfwPollEvents();
    }

    // Release mesh data, textures, and shader program
    UDestroyMesh(gMesh);
    UDestroyTexture(gTextureIdBlack);
    UDestroyTexture(gTextureIdScreen);
    UDestroyTexture(gTextureIdWood);
    UDestroyTexture(gTextureIdKeyboard);
    UDestroyTexture(gTextureIdPhoto);
    UDestroyShaderProgram(gObjectsProgramId);
    UDestroyShaderProgram(gLampProgramId);

    exit(EXIT_SUCCESS); // Terminates the program
}


// Initialize GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // GLFW: window creation
    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);
    glfwSetCursorPosCallback(*window, UMousePositionCallback);
    glfwSetScrollCallback(*window, UMouseScrollCallback);
    glfwSetMouseButtonCallback(*window, UMouseButtonCallback);

    // capture mouse movement
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLEW: initialize
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }

    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    return true;
}


// process input
void UProcessInput(GLFWwindow* window)
{
    static const float cameraSpeed = 2.0f;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        gCamera.ProcessKeyboard(FORWARD, gDeltaTime);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        gCamera.ProcessKeyboard(BACKWARD, gDeltaTime);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        gCamera.ProcessKeyboard(LEFT, gDeltaTime);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        gCamera.ProcessKeyboard(RIGHT, gDeltaTime);

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        gCamera.ProcessKeyboard(UP, gDeltaTime);

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        gCamera.ProcessKeyboard(DOWN, gDeltaTime);

    static bool isJKeyDown = false;
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS && !gIsLampOrbiting)
        gIsLampOrbiting = true;
    else if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS && gIsLampOrbiting)
        gIsLampOrbiting = false;

}


void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


// whenever the mouse moves, this callback is called
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    float xoffset = xpos - gLastX;
    float yoffset = gLastY - ypos;

    gLastX = xpos;
    gLastY = ypos;

    gCamera.ProcessMouseMovement(xoffset, yoffset);
}


// mouse scroll wheel
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    gCamera.ProcessMouseScroll(yoffset);
}

// handle mouse button events
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
    {
        if (action == GLFW_PRESS)
            cout << "Left mouse button pressed" << endl;
        else
            cout << "Left mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_MIDDLE:
    {
        if (action == GLFW_PRESS)
            cout << "Middle mouse button pressed" << endl;
        else
            cout << "Middle mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_RIGHT:
    {
        if (action == GLFW_PRESS)
            cout << "Right mouse button pressed" << endl;
        else
            cout << "Right mouse button released" << endl;
    }
    break;

    default:
        cout << "Unhandled mouse button event" << endl;
        break;
    }
}


// render function
void URender()
{
    
    const float angularVelocity = glm::radians(45.0f);

    if (gIsLampOrbiting)
    {
        glm::vec4 newPosition = glm::rotate(angularVelocity * gDeltaTime, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::vec4(gLightPosition, 1.0f);
        gLightPosition.x = newPosition.x;
        gLightPosition.y = newPosition.y;
        gLightPosition.z = newPosition.z;
    }

    glEnable(GL_DEPTH_TEST);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(gMesh.vao);

    glUseProgram(gObjectsProgramId);

    glm::mat4 model = glm::translate(gObjectsPosition) * glm::scale(gObjectsScale);
    glm::mat4 view = gCamera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(gCamera.Zoom), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);

    // passes transform matrices to the Shader program
    GLint modelLoc = glGetUniformLocation(gObjectsProgramId, "model");
    GLint viewLoc = glGetUniformLocation(gObjectsProgramId, "view");
    GLint projLoc = glGetUniformLocation(gObjectsProgramId, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // uniform location from the object color, light color, light position, and camera position
    GLint objectColorLoc = glGetUniformLocation(gObjectsProgramId, "objectColor");
    GLint lightColorLoc = glGetUniformLocation(gObjectsProgramId, "lightColor");
    GLint lightPositionLoc = glGetUniformLocation(gObjectsProgramId, "lightPos");
    GLint viewPositionLoc = glGetUniformLocation(gObjectsProgramId, "viewPosition");

    // Pass color, light, and camera data
    glUniform3f(objectColorLoc, gObjectColor.r, gObjectColor.g, gObjectColor.b);
    glUniform3f(lightColorLoc, gLightColor.r, gLightColor.g, gLightColor.b);
    glUniform3f(lightPositionLoc, gLightPosition.x, gLightPosition.y, gLightPosition.z);
    const glm::vec3 cameraPosition = gCamera.Position;
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    GLint UVScaleLoc = glGetUniformLocation(gObjectsProgramId, "uvScale");
    glUniform2fv(UVScaleLoc, 1, glm::value_ptr(gUVScale));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureIdBlack);
    glDrawArrays(GL_TRIANGLES, 0, 96);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureIdScreen);
    glDrawArrays(GL_TRIANGLES, 96, 6);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureIdWood);
    glDrawArrays(GL_TRIANGLES, 102, 6);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureIdBlack);
    glDrawArrays(GL_TRIANGLES, 108, 30);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureIdKeyboard);
    glDrawArrays(GL_TRIANGLES, 138, 6);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureIdPhoto);
    glDrawArrays(GL_TRIANGLES, 144, 36);
    
    // draw lamp
    glUseProgram(gLampProgramId);

    model = glm::translate(gLightPosition) * glm::scale(gLightScale);

    // uniforms from the lamp shader program
    modelLoc = glGetUniformLocation(gLampProgramId, "model");
    viewLoc = glGetUniformLocation(gLampProgramId, "view");
    projLoc = glGetUniformLocation(gLampProgramId, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Draws the triangles
    glDrawArrays(GL_TRIANGLES, 60, 36);

    glBindVertexArray(0);
    glUseProgram(0);
    
    glfwSwapBuffers(gWindow);
}


// 3D mesh
void UCreateMesh(GLMesh& mesh)
{
    // Position and Color data
    GLfloat verts[] = {
        // Vertex Positions         //Normals               //Texture Coordinates
        // Monitor
        // Front face
         0.5f,  0.3f,  0.0f,        0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
         0.5f, -0.3f,  0.0f,        0.0f,  0.0f,  1.0f,     1.0f, 0.0f,
        -0.5f, -0.3f,  0.0f,        0.0f,  0.0f,  1.0f,     0.0f, 0.0f,

         0.5f,  0.3f,  0.0f,        0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
        -0.5f, -0.3f,  0.0f,        0.0f,  0.0f,  1.0f,     0.0f, 0.0f,
        -0.5f,  0.3f,  0.0f,        0.0f,  0.0f,  1.0f,     0.0f, 1.0f,

        // Back face
         0.5f,  0.3f, -0.05f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
         0.5f, -0.3f, -0.05f,       0.0f,  0.0f, -1.0f,     1.0f, 0.0f,
        -0.5f, -0.3f, -0.05f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,

         0.5f,  0.3f, -0.05f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
        -0.5f, -0.3f, -0.05f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,
        -0.5f,  0.3f, -0.05f,       0.0f,  0.0f, -1.0f,     0.0f, 1.0f,

        // Top face
         0.5f,  0.3f,  0.0f,        0.0f,  1.0f,  0.0f,     1.0f, 0.0f,
         0.5f,  0.3f, -0.05f,       0.0f,  1.0f,  0.0f,     1.0f, 1.0f,
        -0.5f,  0.3f,  0.0f,        0.0f,  1.0f,  0.0f,     0.0f, 0.0f,

         0.5f,  0.3f, -0.05f,       0.0f,  1.0f,  0.0f,     1.0f, 1.0f,
        -0.5f,  0.3f,  0.0f,        0.0f,  1.0f,  0.0f,     0.0f, 0.0f,
        -0.5f,  0.3f, -0.05f,       0.0f,  1.0f,  0.0f,     0.0f, 1.0f,

        // Left face
        -0.5f,  0.3f,  0.0f,       -1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
        -0.5f,  0.3f, -0.05f,      -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
        -0.5f, -0.3f,  0.0f,       -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,

        -0.5f,  0.3f, -0.05f,      -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
        -0.5f, -0.3f,  0.0f,       -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
        -0.5f, -0.3f, -0.05f,      -1.0f,  0.0f,  0.0f,     0.0f, 1.0f,

        // Right face 
         0.5f,  0.3f,  0.0f,        1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         0.5f,  0.3f, -0.05f,       1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         0.5f, -0.3f,  0.0f,        1.0f,  0.0f,  0.0f,     1.0f, 0.0f,

         0.5f,  0.3f, -0.05f,       1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         0.5f, -0.3f,  0.0f,        1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
         0.5f, -0.3f, -0.05f,       1.0f,  0.0f,  0.0f,     1.0f, 1.0f,

        // Bottom face
         0.5f, -0.3f,  0.0f,        0.0f, -1.0f,  0.0f,     1.0f, 0.0f,
         0.5f, -0.3f, -0.05f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
        -0.5f, -0.3f,  0.0f,        0.0f, -1.0f,  0.0f,     0.0f, 0.0f,

         0.5f, -0.3f, -0.05f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
        -0.5f, -0.3f,  0.0f,        0.0f, -1.0f,  0.0f,     0.0f, 0.0f,
        -0.5f, -0.3f, -0.05f,       0.0f, -1.0f,  0.0f,     0.0f, 1.0f,


        // Monitor Stand
        // Front face
         0.01f, -0.1f,  -0.05f,     0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
         0.01f, -0.5f,  -0.1f,      0.0f,  0.0f,  1.0f,     1.0f, 0.0f,
        -0.01f, -0.5f,  -0.1f,      0.0f,  0.0f,  1.0f,     0.0f, 0.0f,

         0.01f, -0.5f,  -0.1f,      0.0f,  0.0f,  1.0f,     1.0f, 0.0f,
        -0.01f, -0.5f,  -0.1f,      0.0f,  0.0f,  1.0f,     0.0f, 0.0f,
        -0.01f, -0.1f,  -0.05f,     0.0f,  0.0f,  1.0f,     0.0f, 1.0f,

        // Back face 
         0.01f, -0.1f,  -0.1f,      0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
         0.01f, -0.5f,  -0.15f,     0.0f,  0.0f, -1.0f,     1.0f, 0.0f,
        -0.01f, -0.5f,  -0.15f,     0.0f,  0.0f, -1.0f,     0.0f, 0.0f,

         0.01f, -0.5f,  -0.15f,     0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
        -0.01f, -0.5f,  -0.15f,     0.0f,  0.0f, -1.0f,     0.0f, 0.0f,
        -0.01f, -0.1f,  -0.1f,      0.0f,  0.0f, -1.0f,     0.0f, 1.0f,

        // Right face
         0.01f, -0.1f,  -0.05f,     1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         0.01f, -0.1f,  -0.1f,      1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.01f, -0.5f,  -0.1f,      1.0f,  0.0f,  0.0f,     0.0f, 0.0f,

         0.01f, -0.1f,  -0.1f,      1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.01f, -0.5f,  -0.1f,      1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         0.01f, -0.5f,  -0.15f,     1.0f,  0.0f,  0.0f,     0.0f, 1.0f,

         // Left face
         -0.01f, -0.1f,  -0.05f,    -1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         -0.01f, -0.1f,  -0.1f,     -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         -0.01f, -0.5f,  -0.1f,     -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,

         -0.01f, -0.1f,  -0.1f,     -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         -0.01f, -0.5f,  -0.1f,     -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         -0.01f, -0.5f,  -0.15f,    -1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         
        // Front face
         0.2f, -0.5f,   0.1f,       0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
         0.2f, -0.52f,  0.1f,       0.0f,  0.0f,  1.0f,     0.0f, 1.0f,
        -0.2f, -0.52f,  0.1f,       0.0f,  0.0f,  1.0f,     0.0f, 0.0f,
         
         0.2f, -0.5f,   0.1f,       0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
        -0.2f, -0.52f,  0.1f,       0.0f,  0.0f,  1.0f,     0.0f, 0.0f,
        -0.2f, -0.5f,   0.1f,       0.0f,  0.0f,  1.0f,     0.0f, 1.0f,

        // Back face
         0.2f, -0.5f,  -0.2f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
         0.2f, -0.52f, -0.2f,       0.0f,  0.0f, -1.0f,     0.0f, 1.0f,
        -0.2f, -0.52f, -0.2f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,

         0.2f, -0.5f,  -0.2f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
        -0.2f, -0.52f, -0.2f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,
        -0.2f, -0.5f,  -0.2f,       0.0f,  0.0f, -1.0f,     0.0f, 1.0f,

        // Top face
         0.2f, -0.5f,   0.1f,       0.0f,  1.0f,  0.0f,     0.0f, 1.0f,
         0.2f, -0.5f,  -0.2f,       0.0f,  1.0f,  0.0f,     1.0f, 1.0f,
        -0.2f, -0.5f,   0.1f,       0.0f,  1.0f,  0.0f,     0.0f, 0.0f,

         0.2f, -0.5f,  -0.2f,       0.0f,  1.0f,  0.0f,     1.0f, 1.0f,
        -0.2f, -0.5f,   0.1f,       0.0f,  1.0f,  0.0f,     0.0f, 0.0f,
        -0.2f, -0.5f,  -0.2f,       0.0f,  1.0f,  0.0f,     0.0f, 1.0f,

        // Bottom face
         0.2f, -0.52f,  0.1f,       0.0f, -1.0f,  0.0f,     0.0f, 1.0f,
         0.2f, -0.52f, -0.2f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
        -0.2f, -0.52f,  0.1f,       0.0f, -1.0f,  0.0f,     0.0f, 0.0f,

         0.2f, -0.52f, -0.2f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
        -0.2f, -0.52f,  0.1f,       0.0f, -1.0f,  0.0f,     0.0f, 0.0f,
        -0.2f, -0.52f, -0.2f,       0.0f, -1.0f,  0.0f,     0.0f, 1.0f,

        // Right face
         0.2f, -0.5f,   0.1f,       1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         0.2f, -0.52f,  0.1f,       1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         0.2f, -0.5f,  -0.2f,       1.0f,  0.0f,  0.0f,     1.0f, 1.0f,

         0.2f, -0.52f,  0.1f,       1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         0.2f, -0.5f,  -0.2f,       1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.2f, -0.52f, -0.2f,       1.0f,  0.0f,  0.0f,     1.0f, 0.0f,

        // Left face
        -0.2f, -0.5f,   0.1f,      -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
        -0.2f, -0.52f,  0.1f,      -1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
        -0.2f, -0.5f,  -0.2f,      -1.0f,  0.0f,  0.0f,     0.0f, 1.0f,

        -0.2f, -0.52f,  0.1f,      -1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
        -0.2f, -0.5f,  -0.2f,      -1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
        -0.2f, -0.52f, -0.2f,      -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,

        // Screen
         0.48f, 0.28f,  0.0001f,    0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
         0.48f, -0.28f, 0.0001f,    0.0f,  0.0f,  1.0f,     1.0f, 0.0f,
        -0.48f, -0.28f, 0.0001f,    0.0f,  0.0f,  1.0f,     0.0f, 0.0f,

         0.48f, 0.28f,  0.0001f,    0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
        -0.48f, -0.28f, 0.0001f,    0.0f,  0.0f,  1.0f,     0.0f, 0.0f,
        -0.48f, 0.28f,  0.0001f,    0.0f,  0.0f,  1.0f,     0.0f, 1.0f,

        // Desk
         1.6f, -0.53f, -0.4f,       0.0f,  1.0f,  0.0f,     1.0f, 1.0f,
         1.6f, -0.53f,  1.0f,       0.0f,  1.0f,  0.0f,     0.0f, 1.0f,
        -1.4f, -0.53f,  1.0f,       0.0f,  1.0f,  0.0f,     0.0f, 0.0f,

         1.6f, -0.53f, -0.4f,       0.0f,  1.0f,  0.0f,     1.0f, 1.0f,
        -1.4f, -0.53f,  1.0f,       0.0f,  1.0f,  0.0f,     0.0f, 0.0f,
        -1.4f, -0.53f, -0.4f,       0.0f,  1.0f,  0.0f,     0.0f, 1.0f,

        // Keyboard
        // Front face
         0.3f, -0.515f, 0.5f,       0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
         0.3f, -0.52f,  0.5f,       0.0f,  0.0f,  1.0f,     0.0f, 1.0f,
        -0.3f, -0.52f,  0.5f,       0.0f,  0.0f,  1.0f,     0.0f, 0.0f,

         0.3f, -0.515f, 0.5f,       0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
        -0.3f, -0.52f,  0.5f,       0.0f,  0.0f,  1.0f,     0.0f, 0.0f,
        -0.3f, -0.515f, 0.5f,       0.0f,  0.0f,  1.0f,     0.0f, 1.0f,

        // Back face
         0.3f, -0.5f,   0.3f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
         0.3f, -0.52f,  0.3f,       0.0f,  0.0f, -1.0f,     1.0f, 0.0f,
        -0.3f, -0.52f,  0.3f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,
                                           
         0.3f, -0.5f,   0.3f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
        -0.3f, -0.52f,  0.3f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,
        -0.3f, -0.5f,   0.3f,       0.0f,  0.0f, -1.0f,     0.0f, 1.0f,
             
        // Bottom face
         0.3f, -0.52f,  0.3f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
         0.3f, -0.52f,  0.5f,       0.0f, -1.0f,  0.0f,     1.0f, 0.0f,
        -0.3f, -0.52f,  0.5f,       0.0f, -1.0f,  0.0f,     0.0f, 0.0f,
                                                  
         0.3f, -0.52f,  0.3f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
        -0.3f, -0.52f,  0.5f,       0.0f, -1.0f,  0.0f,     0.0f, 0.0f,
        -0.3f, -0.52f,  0.3f,       0.0f, -1.0f,  0.0f,     0.0f, 1.0f,
             
        // Right face
         0.3f, -0.5f,   0.3f,       1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.3f, -0.52f,  0.3f,       1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
         0.3f, -0.52f,  0.5f,       1.0f,  0.0f,  0.0f,     0.0f, 0.0f,

         0.3f, -0.5f,   0.3f,       1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.3f, -0.52f,  0.5f,       1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         0.3f, -0.515f, 0.5f,       1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         
        // Left face
        -0.3f, -0.515f, 0.5f,      -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
        -0.3f, -0.52f,  0.5f,      -1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
        -0.3f, -0.52f,  0.3f,      -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
       
        -0.3f, -0.515f, 0.5f,      -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
        -0.3f, -0.52f,  0.3f,      -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
        -0.3f, -0.5f,   0.3f,      -1.0f,  0.0f,  0.0f,     0.0f, 1.0f,

        // Top face
         0.3f,  -0.5f,   0.3f,       0.0f, 1.0f, 0.0f,       1.0f, 1.0f,
         0.3f,  -0.515f, 0.5f,       0.0f, 1.0f, 0.0f,       1.0f, 0.0f,
        -0.3f,  -0.515f, 0.5f,       0.0f, 1.0f, 0.0f,       0.0f, 0.0f,

         0.3f,  -0.5f,   0.3f,       0.0f, 1.0f, 0.0f,       1.0f, 1.0f,
        -0.3f,  -0.515f, 0.5f,       0.0f, 1.0f, 0.0f,       0.0f, 0.0f,
        -0.3f,  -0.5f,   0.3f,       0.0f, 1.0f, 0.0f,       0.0f, 1.0f,


        // Acrylic Photo Frame
        // Front face
         0.9f, -0.2f,   0.2f,       0.0f,  0.0f,  1.0f,     1.0f, 1.0f, 
         0.9f, -0.52f,  0.2f,       0.0f,  0.0f,  1.0f,     1.0f, 0.0f,
         0.7f, -0.52f,  0.2f,       0.0f,  0.0f,  1.0f,     0.0f, 0.0f,

         0.9f, -0.2f,   0.2f,       0.0f,  0.0f,  1.0f,     1.0f, 1.0f,
         0.7f, -0.52f,  0.2f,       0.0f,  0.0f,  1.0f,     0.0f, 0.0f,
         0.7f, -0.2f,   0.2f,       0.0f,  0.0f,  1.0f,     0.0f, 1.0f,  
         
        // Back face
         0.7f, -0.2f,   0.15f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
         0.7f, -0.52f,  0.15f,       0.0f,  0.0f, -1.0f,     1.0f, 0.0f,
         0.9f, -0.52f,  0.15f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,
                                           
         0.7f, -0.2f,   0.15f,       0.0f,  0.0f, -1.0f,     1.0f, 1.0f,
         0.9f, -0.52f,  0.15f,       0.0f,  0.0f, -1.0f,     0.0f, 0.0f,
         0.9f, -0.2f,   0.15f,       0.0f,  0.0f, -1.0f,     0.0f, 1.0f,
             
        // Bottom face
         0.7f, -0.52f,  0.15f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
         0.7f, -0.52f,  0.2f,       0.0f, -1.0f,  0.0f,     1.0f, 0.0f,
         0.9f, -0.52f,  0.2f,       0.0f, -1.0f,  0.0f,     0.0f, 0.0f,
                                                  
         0.7f, -0.52f,  0.15f,       0.0f, -1.0f,  0.0f,     1.0f, 1.0f,
         0.9f, -0.52f,  0.2f,       0.0f, -1.0f,  0.0f,     0.0f, 0.0f,
         0.9f, -0.52f,  0.15f,       0.0f, -1.0f,  0.0f,     0.0f, 1.0f,
             
        // Right face
         0.7f, -0.2f,   0.15f,      -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.7f, -0.52f,  0.15f,      -1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
         0.7f, -0.52f,  0.2f,      -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,

         0.7f, -0.2f,   0.15f,      -1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.7f, -0.52f,  0.2f,      -1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         0.7f, -0.2f,   0.2f,      -1.0f,  0.0f,  0.0f,     0.0f, 1.0f,
         
        // Left face
         0.9f, -0.2f,   0.2f,       1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.9f, -0.52f,  0.2f,       1.0f,  0.0f,  0.0f,     1.0f, 0.0f,
         0.9f, -0.52f,  0.15f,       1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
       
         0.9f, -0.2f,   0.2f,       1.0f,  0.0f,  0.0f,     1.0f, 1.0f,
         0.9f, -0.52f,  0.15f,       1.0f,  0.0f,  0.0f,     0.0f, 0.0f,
         0.9f, -0.2f,   0.15f,       1.0f,  0.0f,  0.0f,     0.0f, 1.0f,

        // Top face
        0.7f,  -0.2f,   0.15f,       0.0f, 1.0f, 0.0f,       1.0f, 1.0f,
        0.7f,  -0.2f,   0.2f,       0.0f, 1.0f, 0.0f,       1.0f, 0.0f,
        0.9f,  -0.2f,   0.2f,       0.0f, 1.0f, 0.0f,       0.0f, 0.0f,

        0.7f,  -0.2f,   0.15f,       0.0f, 1.0f, 0.0f,       1.0f, 1.0f,
        0.9f,  -0.2f,   0.2f,       0.0f, 1.0f, 0.0f,       0.0f, 0.0f,
        0.9f,  -0.2f,   0.15f,       0.0f, 1.0f, 0.0f,       0.0f, 1.0f
    };

    const GLuint floatsPerVertex = 3;
    const GLuint floatsPerNormal = 3;
    const GLuint floatsPerUV = 2;

    mesh.nVertices = sizeof(verts) / (sizeof(verts[0]) * (floatsPerVertex + floatsPerNormal + floatsPerUV));

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    // Create 2 buffers, vertex data and one for the indices
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint stride = sizeof(float) * (floatsPerVertex + floatsPerNormal + floatsPerUV);

    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * floatsPerVertex));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * (floatsPerVertex + floatsPerNormal)));
    glEnableVertexAttribArray(2);
}


void UDestroyMesh(GLMesh& mesh)
{
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
}


// Generate and load textures
bool UCreateTexture(const char* filename, GLuint& textureId)
{
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (image)
    {
        flipImageVertically(image, width, height, channels);

        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (channels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (channels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
            cout << "Not implemented to handle image with " << channels << " channels" << endl;
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0);

        return true;
    }

    return false;
}


void UDestroyTexture(GLuint textureId)
{
    glGenTextures(1, &textureId);
}


// shader program
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{

    int success = 0;
    char infoLog[512];

    // Create a Shader program object.
    programId = glCreateProgram();

    // Create the vertex and fragment shader objects
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    // Retrieve the shader source for each shader
    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    // Compile the vertex shader
    glCompileShader(vertexShaderId);

    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    // compile the fragment shader
    glCompileShader(fragmentShaderId);

    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    // Attached compiled shaders to the shader program
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);

    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glUseProgram(programId);

    return true;
}


void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}
