
#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "maths.h"
#include "mesh.h"
#include "color.h"

#include "demo_skybox.h"

#include "pg.h"

// Vertex format
// ==================================================
struct vertex
{
    v3 Position;
    v3 Normal;
};

// Shaders
// ==================================================
static const char* gVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

// Uniforms
uniform mat4 uViewProj;
uniform mat4 uModel;

// Varyings (variables that are passed to fragment shader with perspective interpolation)
out vec3 vNormal;
out vec3 vPos;

void main()
{
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vPos = vec3(uModel * vec4(aPosition, 1.0));
    gl_Position = uViewProj * uModel * vec4(aPosition, 1.0);
})GLSL";

static const char* gFragmentShaderStr = R"GLSL(
// Varyings
in vec3 vNormal;
in vec3 vPos;

// Uniforms
uniform samplerCube uSkybox;
uniform vec3 uCameraPos;
uniform bool uRefractive;

// Shader outputs
out vec4 oColor;

void main()
{
    vec3 I = normalize(vPos - uCameraPos);
    vec3 R;
    if (uRefractive)
    {
        float ratio = 1.00 / 2.42;
        R = refract(I, normalize(vNormal), ratio);
    }

    else
    {
        R = reflect(I, normalize(vNormal));
    }
    oColor = vec4(texture(uSkybox, R).rgb, 1.0);
})GLSL";

static const char* gVertexShaderSkybox = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;

// Uniforms
uniform mat4 uViewProj;

// Varyings (variables that are passed to fragment shader with perspective interpolation)
out vec3 vUV;

void main()
{
    vUV = aPosition;
    vec4 pos = uViewProj * vec4(aPosition, 1.0);
    gl_Position = pos.xyww;
})GLSL";

static const char* gFragmentShaderSkybox = R"GLSL(
// Varyings
in vec3 vUV;

// Uniforms
uniform samplerCube skybox;

// Shader outputs
out vec4 oColor;

void main()
{
    oColor = texture(skybox, vUV);
})GLSL";

demo_skybox::demo_skybox()
{
    // Create render pipeline
    this->Program = GL::CreateProgram(gVertexShaderStr, gFragmentShaderStr);
    this->SkyboxProgram = GL::CreateProgram(gVertexShaderSkybox, gFragmentShaderSkybox);

    // Gen mesh
    {
        // Create a descriptor based on the `struct vertex` format
        vertex_descriptor Descriptor = {};
        Descriptor.Stride = sizeof(vertex);
        Descriptor.HasNormal = true;
        Descriptor.PositionOffset = OFFSETOF(vertex, Position);
        Descriptor.NormalOffset = OFFSETOF(vertex, Normal);

        // Create a cube in RAM
        vertex Cube[36];
        this->VertexCount = 36;
        Mesh::BuildCube(Cube, Cube + this->VertexCount, Descriptor);

        // Upload cube to gpu (VRAM)
        glGenBuffers(1, &this->VertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, this->VertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, this->VertexCount * sizeof(vertex), Cube, GL_STATIC_DRAW);
    }

    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    std::vector<std::string> faces
    {
        "media/skybox/right.jpg",
        "media/skybox/left.jpg",
        "media/skybox/top.jpg",
        "media/skybox/bottom.jpg",
        "media/skybox/front.jpg",
        "media/skybox/back.jpg"
    };
    cubemapTexture = loadCubemap(faces);

    //// Gen texture
    //{
    //    glGenTextures(1, &Texture);
    //    glBindTexture(GL_TEXTURE_2D, Texture);
    //    GL::UploadCheckerboardTexture(64, 64, 8);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //}

    // Create a vertex array
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, this->VertexBuffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Normal));
}

demo_skybox::~demo_skybox()
{
    // Cleanup GL
    //glDeleteTextures(1, &Texture);
    glDeleteTextures(1, &cubemapTexture);
    glDeleteBuffers(1, &VertexBuffer);
    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteProgram(Program);
    glDeleteProgram(SkyboxProgram);
}

static void DrawCube(GLuint Program, mat4 ViewProj, mat4 Model, v3 cameraPos, bool refractive)
{
    glUniformMatrix4fv(glGetUniformLocation(Program, "uViewProj"), 1, GL_FALSE, ViewProj.e);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uModel"), 1, GL_FALSE, Model.e);
    glUniform3f(glGetUniformLocation(Program, "uCameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform1i(glGetUniformLocation(Program, "uRefractive"), refractive);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void DrawSkybox(GLuint Program, mat4 ViewProj)
{
    glDepthFunc(GL_LEQUAL);
    glUniformMatrix4fv(glGetUniformLocation(Program, "uViewProj"), 1, GL_FALSE, ViewProj.e);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthFunc(GL_LESS);
}

void demo_skybox::Update(const platform_io& IO)
{
    Camera = CameraUpdateFreefly(Camera, IO.CameraInputs);

    // Compute model-view-proj and send it to shader
    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), (float)IO.WindowWidth / (float)IO.WindowHeight, 0.1f, 100.f);
    mat4 ViewMatrix = CameraGetInverseMatrix(Camera);
    
    // Setup GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Clear screen
    glClearColor(0.2f, 0.2f, 0.2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 view = ViewMatrix;
    view.c[3].xyz = v3{ 0, 0, 0 };

    glUseProgram(SkyboxProgram);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glBindVertexArray(skyboxVAO);
    DrawSkybox(SkyboxProgram, ProjectionMatrix * view);
    
    // Use shader and send data
    glUseProgram(Program);
    glUniform1f(glGetUniformLocation(Program, "uTime"), (float)IO.Time);
    
    //glBindTexture(GL_TEXTURE_2D, Texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glBindVertexArray(VAO);

    // Draw origin
    PG::DebugRenderer()->DrawAxisGizmo(Mat4::Translate({ 0.f, 0.f, 0.f }), true, false);
    
    // Standard quad
    v3 ObjectPosition = { 0.f, 0.f, -3.f };
    {
        mat4 ModelMatrix = Mat4::Translate(ObjectPosition);
        DrawCube(Program, ProjectionMatrix * ViewMatrix, ModelMatrix, Camera.Position, refractive);
    }

    DisplayDebugUI();
}

unsigned int demo_skybox::loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    GL::UploadCubemapTexture(faces);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

void demo_skybox::DisplayDebugUI()
{
    if (ImGui::TreeNodeEx("demo_skybox", ImGuiTreeNodeFlags_Framed))
    {
        // Debug display
        ImGui::Checkbox("Refractive", &refractive);
        ImGui::TreePop();
    }
}
