
#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "opengl_helpers_wireframe.h"

#include "color.h"
#include "maths.h"
#include "mesh.h"

#include "demo_postprocess.h"

const int LIGHT_BLOCK_BINDING_POINT = 0;

struct vertex
{
    v3 Position;
    v2 UV;
};

#pragma region tavern_shader
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

// Varyings
out vec2 vUV;
out vec3 vPos;    // Vertex position in view-space
out vec3 vNormal; // Vertex normal in view-space

void main()
{
    vUV = aUV;
    vec4 pos4 = (uModel * vec4(aPosition, 1.0));
    vPos = pos4.xyz / pos4.w;
    vNormal = (uModelNormalMatrix * vec4(aNormal, 0.0)).xyz;
    gl_Position = uProjection * uView * pos4;
})GLSL";

static const char* gFragmentShaderStr = R"GLSL(
// Varyings
in vec2 vUV;
in vec3 vPos;
in vec3 vNormal;

// Uniforms
uniform mat4 uProjection;
uniform vec3 uViewPosition;

uniform sampler2D uDiffuseTexture;
uniform sampler2D uEmissiveTexture;

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

void main()
{
    // Compute phong shading
    light_shade_result lightResult = get_lights_shading();
    
    vec3 diffuseColor  = gDefaultMaterial.diffuse * lightResult.diffuse * texture(uDiffuseTexture, vUV).rgb;
    vec3 ambientColor  = gDefaultMaterial.ambient * lightResult.ambient;
    vec3 specularColor = gDefaultMaterial.specular * lightResult.specular;
    vec3 emissiveColor = gDefaultMaterial.emission + texture(uEmissiveTexture, vUV).rgb;
    
    // Apply light color
    oColor = vec4((ambientColor + diffuseColor + specularColor + emissiveColor), 1.0);
})GLSL";
#pragma endregion tavern_shader

#pragma region postprocess_shader
static const char* gVertexPostProcessShaderStr = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = vec4(aPos, 1.0);
}
)GLSL";

static const char* gFragmentPostProcessShaderStr = R"GLSL(
out vec4 oColor;

in vec2 vUV;

uniform sampler2D uRenderTex;

uniform float uOffset;
uniform mat3 uKernel;

void main()
{
    vec2 offsets[9] = vec2[](
        vec2(-uOffset,  uOffset),
        vec2( 0.0f,     uOffset),
        vec2( uOffset,  uOffset),
        vec2(-uOffset,  0.0f),
        vec2( 0.0f,     0.0f),
        vec2( uOffset,  0.0f),
        vec2(-uOffset, -uOffset),
        vec2( 0.0f,    -uOffset),
        vec2( uOffset, -uOffset)
    );

    vec3 surroundingFrags[9];
    for (int i = 0; i < 9; ++i)
    {
        surroundingFrags[i] = vec3(texture(uRenderTex, vUV + offsets[i]));
    }

    vec3 blendedColor = vec3(0.0);
    for (int i = 0; i < 9; ++i)
    {
        blendedColor += surroundingFrags[i] * uKernel[i / 3][i % 3];
    }

    oColor = vec4(blendedColor, 1.0);
}
)GLSL";
#pragma endregion postprocess_shader

#pragma region render_shader
static const char* gVertexRenderShaderStr = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = vec4(aPos, 1.0);
}
)GLSL";

static const char* gFragmentRenderShaderStr = R"GLSL(
out vec4 oColor;

in vec2 vUV;

uniform float uGamma;
uniform sampler2D uRenderTex;

void main()
{
    oColor = texture(uRenderTex, vUV);

    oColor.rgb = pow(oColor.rgb, vec3(1.0 / uGamma));
}
)GLSL";
#pragma endregion render_shader

demo_postprocess::demo_postprocess(GL::cache& GLCache, GL::debug& GLDebug)
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
        this->PostProcessProgram = GL::CreateProgramEx(1, &gVertexPostProcessShaderStr, 1, &gFragmentPostProcessShaderStr, false);
        this->RenderProgram = GL::CreateProgramEx(1, &gVertexRenderShaderStr, 1, &gFragmentRenderShaderStr, false);
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
        glUniformBlockBinding(TavernProgram, glGetUniformBlockIndex(TavernProgram, "uLightBlock"), LIGHT_BLOCK_BINDING_POINT);
    }

    // create texture to be rendered as the screen
    {
        //direct scene render
        glGenTextures(1, &RawRenderTex);
        glBindTexture(GL_TEXTURE_2D, RawRenderTex);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, RenderResolution, RenderResolution, 0, GL_RGB, GL_UNSIGNED_BYTE, (void*)0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        //scene depth render
        glGenTextures(1, &RenderDepthMap);
        glBindTexture(GL_TEXTURE_2D, RenderDepthMap);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, RenderResolution, RenderResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, (void*)0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        //ping pong framebuffers
        glGenTextures(2, ppRenderTex);
        for (int i = 0; i < 2; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, ppRenderTex[i]);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, RenderResolution, RenderResolution, 0, GL_RGB, GL_UNSIGNED_BYTE, (void*)0);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        }
    }

    // create frame buffer
    {
        //direct scene render framebuffer
        glGenFramebuffers(1, &FBO);
        
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, RawRenderTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, RenderDepthMap, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        //ping pong framebuffer
        glGenFramebuffers(2, ppFBO);
        for (int i = 0; i < 2; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, ppFBO[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ppRenderTex[i], 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // create screen quad
    {
        // Create a descriptor based on the `struct vertex` format
        vertex_descriptor Descriptor = {};
        Descriptor.Stride = sizeof(vertex);
        Descriptor.HasUV = true;
        Descriptor.PositionOffset = OFFSETOF(vertex, Position);
        Descriptor.UVOffset = OFFSETOF(vertex, UV);

        // Create a cube in RAM
        vertex Quad[6];
        Mesh::BuildQuad(Quad, Quad + 6, Descriptor);

        for (int i = 0; i < 6; ++i)
        {
            Quad[i].Position *= 2.f;
        }

        GLuint VBO;
        // Upload cube to gpu (VRAM)
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(vertex), Quad, GL_STATIC_DRAW);

        glGenVertexArrays(1, &RenderVAO);
        glBindVertexArray(RenderVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)sizeof(v3));
        glBindVertexArray(0);

        glDeleteBuffers(1, &VBO);
    }

    //initializing kernels matrix
    {
        for (int i = 0; i < 9; ++i)
        {
            Kernel.e[i] = 0.f;
        }
        Kernel.c[1].e[1] = 1.f;
    }
}

demo_postprocess::~demo_postprocess()
{
    // Cleanup GL
    glDeleteVertexArrays(1, &TavernVAO);
    glDeleteVertexArrays(1, &RenderVAO);
    glDeleteProgram(TavernProgram);
    glDeleteProgram(PostProcessProgram);

    glDeleteFramebuffers(1, &FBO);
}

void demo_postprocess::Update(const platform_io& IO)
{
    const float AspectRatio = (float)IO.WindowWidth / (float)IO.WindowHeight;
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);

    Camera = CameraUpdateFreefly(Camera, IO.CameraInputs);

    mat4 ProjectionMatrix = Mat4::Perspective(Math::ToRadians(60.f), AspectRatio, 0.1f, 100.f);
    mat4 ViewMatrix = CameraGetInverseMatrix(Camera);
    mat4 ModelMatrix = Mat4::Translate({ 0.f, 0.f, 0.f });

    // Render tavern
    this->RenderTavernFBO(ProjectionMatrix, ViewMatrix, ModelMatrix);

    this->RenderPingPong(PostProcessCount);

    // Render screen
    glViewport(0, 0, IO.WindowWidth, IO.WindowHeight);
    this->RenderScreen();

    // Render tavern wireframe
    if (Wireframe)
    {
        GLDebug.Wireframe.BindBuffer(TavernScene.MeshBuffer, TavernScene.MeshDesc.Stride, TavernScene.MeshDesc.PositionOffset, TavernScene.MeshVertexCount);
        GLDebug.Wireframe.DrawArray(0, TavernScene.MeshVertexCount, ProjectionMatrix * ViewMatrix * ModelMatrix);
    }

    // Display debug UI
    this->DisplayDebugUI();
}

void demo_postprocess::DisplayDebugUI()
{
    if (ImGui::TreeNodeEx("demo_postprocess", ImGuiTreeNodeFlags_Framed))
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

        if (ImGui::TreeNodeEx("Gamma correction"))
        {
            ImGui::DragFloat("Gamma", &Gamma, 0.1f, 0.6f, 3.f);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Post processing"))
        {
            ImGui::DragFloat("Processing offset", &PostProcessOffset, 0.001f);
            ImGui::DragInt("Processing count", &PostProcessCount, 0.1f);

            ImGui::Text("Kernels matrix");
            ImGui::DragFloat3("0", Kernel.c[0].e);
            ImGui::DragFloat3("1", Kernel.c[1].e);
            ImGui::DragFloat3("2", Kernel.c[2].e);

            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}

void demo_postprocess::RenderTavernFBO(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix)
{
    glViewport(0, 0, RenderResolution, RenderResolution);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    // Clear screen
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    // Use shader and configure its uniforms
    glUseProgram(TavernProgram);

    // Set uniforms
    mat4 NormalMatrix = Mat4::Transpose(Mat4::Inverse(ModelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uProjection"), 1, GL_FALSE, ProjectionMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uModel"), 1, GL_FALSE, ModelMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uView"), 1, GL_FALSE, ViewMatrix.e);
    glUniformMatrix4fv(glGetUniformLocation(TavernProgram, "uModelNormalMatrix"), 1, GL_FALSE, NormalMatrix.e);
    glUniform3fv(glGetUniformLocation(TavernProgram, "uViewPosition"), 1, Camera.Position.e);
    
    // Bind uniform buffer and textures
    glBindBufferBase(GL_UNIFORM_BUFFER, LIGHT_BLOCK_BINDING_POINT, TavernScene.LightsUniformBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TavernScene.DiffuseTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TavernScene.EmissiveTexture);
    glActiveTexture(GL_TEXTURE0); // Reset active texture just in case
    
    // Draw mesh
    glBindVertexArray(TavernVAO);
    glDrawArrays(GL_TRIANGLES, 0, TavernScene.MeshVertexCount);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void demo_postprocess::RenderPingPong(const int iteration)
{
    glUseProgram(PostProcessProgram);
    glUniform1f(glGetUniformLocation(PostProcessProgram, "uOffset"), PostProcessOffset);
    glUniformMatrix3fv(glGetUniformLocation(PostProcessProgram, "uKernel"), 1, GL_FALSE, Kernel.e);

    for (int i = 0; i < iteration; ++i)
    {
        glViewport(0, 0, RenderResolution, RenderResolution);
        glBindFramebuffer(GL_FRAMEBUFFER, ppFBO[pp]);

        // Clear screen
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(RenderVAO);
        glBindTexture(GL_TEXTURE_2D, i == 0 ? RawRenderTex : ppRenderTex[!pp]);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        pp = !pp;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void demo_postprocess::RenderScreen()
{
    // Clear screen
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(RenderProgram);
    glUniform1f(glGetUniformLocation(RenderProgram, "uGamma"), Gamma);

    glBindVertexArray(RenderVAO);
    //is postprocessing active ?
    glBindTexture(GL_TEXTURE_2D, PostProcessCount > 0 ? ppRenderTex[!pp] : RawRenderTex);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}