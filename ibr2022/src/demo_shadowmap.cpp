
#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "opengl_helpers_wireframe.h"

#include "color.h"
#include "maths.h"
#include "mesh.h"

#include "demo_shadowmap.h"

const int LIGHT_BLOCK_BINDING_POINT = 0;

struct vertex
{
    v3 Position;
    v2 UV;
};

// shader used to render the tavern
#pragma region tavern
static const char* gVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;

// Uniforms
uniform mat4 uProjection;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uModelNormalMatrix;
uniform mat4 uLightSpaceMatrix;

// Varyings
out vec2 vUV;
out vec3 vPos;    // Vertex position in view-space
out vec3 vNormal; // Vertex normal in view-space
out vec4 vLightSpace;

void main()
{
    vUV = aUV;
    vec4 pos4 = (uModel * vec4(aPosition, 1.0));
    vPos = pos4.xyz / pos4.w;
    vNormal = (uModelNormalMatrix * vec4(aNormal, 0.0)).xyz;
    vLightSpace = uLightSpaceMatrix * pos4;

    gl_Position = uProjection * uView * pos4;
})GLSL";

static const char* gFragmentShaderStr = R"GLSL(
// Varyings
in vec2 vUV;
in vec3 vPos;
in vec3 vNormal;
in vec4 vLightSpace;

// Uniforms
uniform mat4 uProjection;
uniform vec3 uViewPosition;

uniform sampler2D uDiffuseTexture;
uniform sampler2D uEmissiveTexture;
uniform sampler2D uShadowMap;

// Uniform blocks
layout(std140) uniform uLightBlock
{
	light uLight[LIGHT_COUNT];
};

// Shader outputs
out vec4 oColor;

light_shade_result get_lights_shading()
{
    light_shade_result lightResult = light_shade_result(vec3(0.0), vec3(0.0), vec3(0.0));
	for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        light_shade_result light = light_shade(uLight[i], gDefaultMaterial.shininess, uViewPosition, vPos, normalize(vNormal));
        lightResult.ambient  += light.ambient;
        lightResult.diffuse  += light.diffuse;
        lightResult.specular += light.specular;
    }
    return lightResult;
}

float enlighten(vec4 lightSpace, float bias)
{
    vec3 perspective = lightSpace.xyz / lightSpace.w;
    perspective = perspective * 0.5 + 0.5;

    float currentDepth = perspective.z;

    if (currentDepth > 1.0)
        return 1.0;

    // is the fragment lit ?
    float lit = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int i = -1; i <= 1; ++i)
    {
        for (int j = -1; j <= 1; ++j)
        {
            float pcfDepth = texture(uShadowMap, perspective.xy + vec2(i, j) * texelSize).r;
            lit += currentDepth - bias > pcfDepth ? 0.0 : 1.0;
        }
    }

    return lit / 9.0;
}

void main()
{
    // Compute phong shading
    light_shade_result lightResult = get_lights_shading();
    
    vec3 diffuseColor  = gDefaultMaterial.diffuse * lightResult.diffuse * texture(uDiffuseTexture, vUV).rgb;
    vec3 ambientColor  = gDefaultMaterial.ambient * lightResult.ambient * texture(uDiffuseTexture, vUV).rgb;
    vec3 specularColor = gDefaultMaterial.specular * lightResult.specular;
    vec3 emissiveColor = gDefaultMaterial.emission + texture(uEmissiveTexture, vUV).rgb;

    float shadow = enlighten(vLightSpace, 0.005);
    
    // Apply light color
    oColor = vec4((ambientColor + shadow * (diffuseColor + specularColor) + emissiveColor), 1.0);
})GLSL";
#pragma endregion tavern

// shader used to render the depth map
#pragma region depth_map_shader
static const char* gVertexDepthShaderStr = R"GLSL(
layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;

void main()
{
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
)GLSL";

static const char* gFragmentDepthShaderStr = R"GLSL(
void main()
{
}
)GLSL";
#pragma endregion depth_map_shader

// shader used to render a texture on a quad, a screen
#pragma region render_shader
static const char* gVertexRenderShaderStr = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTex;

out vec2 vTex;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    vTex = aTex;
}
)GLSL";

static const char* gFragmentRenderShaderStr = R"GLSL(
out vec4 oColor;

in vec2 vTex;

uniform sampler2D renderTex;

void main()
{
    float r = texture(renderTex, vTex).r;
    oColor = vec4(vec3(r), 1.0);
}
)GLSL";
#pragma endregion render_shader

demo_shadowmap::demo_shadowmap(GL::cache& GLCache, GL::debug& GLDebug)
    : GLDebug(GLDebug), TavernScene(GLCache)
{
    // Create shader
    {
        // Assemble fragment shader strings (defines + code)
        char FragmentShaderConfig[] = "#define LIGHT_COUNT %d\n";
        snprintf(FragmentShaderConfig, ARRAY_SIZE(FragmentShaderConfig), "#define LIGHT_COUNT %d\n", TavernScene.LightCount);
        const char* FragmentShaderStrs[2] = {
            FragmentShaderConfig,
            gFragmentShaderStr,
        };

        this->TavernProgram = GL::CreateProgramEx(1, &gVertexShaderStr, 2, FragmentShaderStrs, true);
        this->DepthProgram = GL::CreateProgramEx(1, &gVertexDepthShaderStr, 1, &gFragmentDepthShaderStr, true);
        this->RenderProgram = GL::CreateProgramEx(1, &gVertexRenderShaderStr, 1, &gFragmentRenderShaderStr, true);
    }

    // Create a vertex array and bind attribs onto the vertex buffer
    {
        glGenVertexArrays(1, &TavernVAO);
        glBindVertexArray(TavernVAO);

        glBindBuffer(GL_ARRAY_BUFFER, TavernScene.MeshBuffer);

        vertex_descriptor& Desc = TavernScene.MeshDesc;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, Desc.Stride, (void*)(size_t)Desc.PositionOffset);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, Desc.Stride, (void*)(size_t)Desc.UVOffset);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, Desc.Stride, (void*)(size_t)Desc.NormalOffset);
    }

    // Set uniforms that won't change
    {
        glUseProgram(TavernProgram);
        glUniform1i(glGetUniformLocation(TavernProgram, "uDiffuseTexture"), 0);
        glUniform1i(glGetUniformLocation(TavernProgram, "uEmissiveTexture"), 1);
        glUniform1i(glGetUniformLocation(TavernProgram, "uShadowMap"), 2);
        glUniformBlockBinding(TavernProgram, glGetUniformBlockIndex(TavernProgram, "uLightBlock"), LIGHT_BLOCK_BINDING_POINT);
    }

    // Initialize depth frame buffer
    {
        glGenFramebuffers(1, &DepthFBO);

        glGenTextures(1, &DepthMap);
        glBindTexture(GL_TEXTURE_2D, DepthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
            DepthMapResolution, DepthMapResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, (void*)0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // attaching texture to the framebuffer's depth buffer department
        glBindFramebuffer(GL_FRAMEBUFFER, DepthFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Initialize quad for frame buffer's rendering
    {
        // Create a cube in RAM
        vertex Quad[6];
        vertex_descriptor Desc = {};
        Desc.Stride = sizeof(vertex);
        Desc.HasUV = true;
        Desc.PositionOffset = OFFSETOF(vertex, Position);
        Desc.UVOffset = OFFSETOF(vertex, UV);
        Mesh::BuildQuad(Quad, Quad + 6, Desc);

        // Upload cube to gpu (VRAM)
        GLuint VBO;
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(vertex), Quad, GL_STATIC_DRAW);

        // Create a vertex array
        glGenVertexArrays(1, &RenderVAO);
        glBindVertexArray(RenderVAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Position));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, UV));
        glBindVertexArray(0);

        glDeleteBuffers(1, &VBO);
    }
}

demo_shadowmap::~demo_shadowmap()
{
    // Cleanup GL
    glDeleteVertexArrays(1, &TavernVAO);
    glDeleteVertexArrays(1, &RenderVAO);
    glDeleteFramebuffers(1, &DepthFBO);
    glDeleteProgram(TavernProgram);
    glDeleteProgram(DepthProgram);
    glDeleteProgram(RenderProgram);
}

void demo_shadowmap::Update(const platform_io& IO)
{
    const float AspectRatio = (float)IO.WindowWidth / (float)IO.WindowHeight;

    Camera = CameraUpdateFreefly(Camera, IO.CameraInputs);

    // Clear screen
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), AspectRatio, 0.1f, 100.f);
    mat4 ViewMatrix = CameraGetInverseMatrix(Camera);
    mat4 ModelMatrix = Mat4::Translate({ 0.f, 0.f, 0.f });

    v3 LightPos = TavernScene.GetLightPositionFromIndex(0);
    mat4 LightProjectionMatrix = Mat4::Orthographic(-LightRange, LightRange, -LightRange, LightRange, -LightRange, LightRange);
    mat4 LightViewMatrix = Mat4::LookAt(LightPos, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });
    mat4 LightSpaceMatrix = LightProjectionMatrix * LightViewMatrix;

    // Render depth map into a frame buffer
    this->RenderTavernDepthMap(ModelMatrix, LightSpaceMatrix);

    // Render tavern
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);
    this->RenderTavern(ProjectionMatrix, ViewMatrix, ModelMatrix, LightSpaceMatrix);

    // Render depth map on a quad
    //this->RenderDepthMap();

    // Render tavern wireframe
    if (Wireframe)
    {
        GLDebug.Wireframe.BindBuffer(TavernScene.MeshBuffer, TavernScene.MeshDesc.Stride, TavernScene.MeshDesc.PositionOffset, TavernScene.MeshVertexCount);
        GLDebug.Wireframe.DrawArray(0, TavernScene.MeshVertexCount, ProjectionMatrix * ViewMatrix * ModelMatrix);
    }

    // Display debug UI
    this->DisplayDebugUI();
}

void demo_shadowmap::DisplayDebugUI()
{
    if (ImGui::TreeNodeEx("demo_shadowmap", ImGuiTreeNodeFlags_Framed))
    {
        // Debug display
        ImGui::Checkbox("Wireframe", &Wireframe);
        if (ImGui::TreeNodeEx("Camera"))
        {
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", Camera.Position.x, Camera.Position.y, Camera.Position.z);
            ImGui::Text("Pitch: %.2f", Math::ToDegrees(Camera.Pitch));
            ImGui::Text("Yaw: %.2f", Math::ToDegrees(Camera.Yaw));
            ImGui::TreePop();
        }
        TavernScene.InspectLights();

        ImGui::TreePop();
    }

    if (ImGui::Begin("Depth map"))
    {
        ImGui::Image((ImTextureID)DepthMap, { DepthMapResolution * 0.4f, DepthMapResolution * 0.4f }, ImVec2(0, 1), ImVec2(1, 0));
    }

    ImGui::End();
}

void demo_shadowmap::RenderTavern(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix, const mat4& LightSpaceMatrix) const
{
    glEnable(GL_DEPTH_TEST);

    // Use shader and configure its uniforms
    glUseProgram(TavernProgram);

    // Set uniforms
    mat4 NormalMatrix = Mat4::Transpose(Mat4::Inverse(ModelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uProjection"), 1, GL_FALSE, ProjectionMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uModel"), 1, GL_FALSE, ModelMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uView"), 1, GL_FALSE, ViewMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uModelNormalMatrix"), 1, GL_FALSE, NormalMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uLightSpaceMatrix"), 1, GL_FALSE, LightSpaceMatrix.e);
    glUniform3fv(glGetUniformLocation(TavernProgram, "uViewPosition"), 1, Camera.Position.e);
    
    // Bind uniform buffer and textures
    glBindBufferBase(GL_UNIFORM_BUFFER, LIGHT_BLOCK_BINDING_POINT, TavernScene.LightsUniformBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TavernScene.DiffuseTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TavernScene.EmissiveTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, DepthMap);
    glActiveTexture(GL_TEXTURE0); // Reset active texture just in case
    
    // Draw mesh
    glBindVertexArray(TavernVAO);
    glDrawArrays(GL_TRIANGLES, 0, TavernScene.MeshVertexCount);
}

void demo_shadowmap::RenderTavernDepthMap(const mat4& ModelMatrix, const mat4& LightSpaceMatrix) const
{
    glViewport(0, 0, DepthMapResolution, DepthMapResolution);
    glBindFramebuffer(GL_FRAMEBUFFER, DepthFBO);
    //glCullFace(GL_FRONT);

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Use shader and configure its uniforms
    glUseProgram(DepthProgram);

    // Set uniforms
    glUniformMatrix4fv(glGetUniformLocation(DepthProgram, "uModel"), 1, GL_FALSE, ModelMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(DepthProgram, "uLightSpaceMatrix"), 1, GL_FALSE, LightSpaceMatrix.e);

    // Draw mesh
    glBindVertexArray(TavernVAO);
    glDrawArrays(GL_TRIANGLES, 0, TavernScene.MeshVertexCount);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //glCullFace(GL_BACK);
}

void demo_shadowmap::RenderDepthMap() const
{
    glUseProgram(RenderProgram);
    glBindVertexArray(RenderVAO);
    glDisable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, DepthMap);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}