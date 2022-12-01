
#include <vector>

#include <imgui.h>

#include "opengl_helpers.h"
#include "maths.h"
#include "mesh.h"
#include "color.h"

#include "demo_instancing.h"

#include "pg.h"

// Vertex format
// ==================================================
struct vertex
{
    v3 Position;
    v2 UV;
};

// Shaders
// ==================================================
static const char* gVertexShaderStr = R"GLSL(
// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in mat4 aInstanceModel;
layout(location = 6) in vec3 aColor;

// Uniforms
uniform mat4 uVP;

// Varyings (variables that are passed to fragment shader with perspective interpolation)
out vec2 vUV;
out vec3 vColor;

void main()
{
    vUV = aUV;
    vColor = aColor;

    gl_Position = uVP * aInstanceModel * vec4(aPosition, 1.0);
})GLSL";

static const char* gFragmentShaderStr = R"GLSL(
// Varyings
in vec2 vUV;
in vec3 vColor;

// Uniforms
uniform sampler2D uColorTexture;

// Shader outputs
out vec4 oColor;

void main()
{
    oColor = vec4(vColor, 1.0) * texture(uColorTexture, vUV);
})GLSL";

demo_instancing::demo_instancing()
{
    // Create render pipeline
    this->Program = GL::CreateProgram(gVertexShaderStr, gFragmentShaderStr);
    
    // Gen mesh
    {
        // Create a descriptor based on the `struct vertex` format
        vertex_descriptor Descriptor = {};
        Descriptor.Stride = sizeof(vertex);
        Descriptor.HasUV = true;
        Descriptor.PositionOffset = OFFSETOF(vertex, Position);
        Descriptor.UVOffset = OFFSETOF(vertex, UV);

        // Create a cube in RAM
        vertex obj[2880];
        this->VertexCount = 2880;
        Mesh::LoadObj(obj, obj + this->VertexCount, Descriptor, "media/sphere.obj", 1.f);

        // Upload cube to gpu (VRAM)
        glGenBuffers(1, &this->VertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, this->VertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, this->VertexCount * sizeof(vertex), obj, GL_STATIC_DRAW);
    }

    // Gen texture
    {
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        GL::UploadTexture("media/sphere.png");

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    
    // Create a vertex array
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, this->VertexBuffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, Position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)OFFSETOF(vertex, UV));

    glGenBuffers(1, &InstanceTransformVBO);
    glGenBuffers(1, &InstanceColorVBO);
    SetInstanceAttributes();
}

demo_instancing::~demo_instancing()
{
    // Cleanup GL
    glDeleteTextures(1, &Texture);
    glDeleteBuffers(1, &VertexBuffer);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(Program);

    glDeleteBuffers(1, &InstanceTransformVBO);
    glDeleteBuffers(1, &InstanceColorVBO);
    InstanceModel.clear();
    InstanceTransform.clear();
    InstanceColor.clear();
}

void demo_instancing::Draw(const GLuint& Program, const mat4& ViewProj) const
{
    glUniformMatrix4fv(glGetUniformLocation(Program, "uVP"), 1, GL_FALSE, ViewProj.e);
    glDrawArrays(GL_TRIANGLES, 0, VertexCount);
}

void demo_instancing::DrawInstanced(const GLuint& Program, const mat4& ViewProj, const GLsizei& instanceCount) const
{
    glUniformMatrix4fv(glGetUniformLocation(Program, "uVP"), 1, GL_FALSE, ViewProj.e);
    glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, instanceCount);
}

void demo_instancing::Update(const platform_io& IO)
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
    
    // Use shader and send data
    glUseProgram(Program);
    glUniform1f(glGetUniformLocation(Program, "uTime"), (float)IO.Time);
    
    glBindTexture(GL_TEXTURE_2D, Texture);
    glBindVertexArray(VAO);

    // Draw origin
    PG::DebugRenderer()->DrawAxisGizmo(Mat4::Translate({ 0.f, 0.f, 0.f }), true, false);

    DrawInstanced(Program, ProjectionMatrix * ViewMatrix, InstanceCount);
    
    DisplayDebugUI();
}

void demo_instancing::SetInstanceAttributes()
{
    InstanceModel.clear();
    InstanceTransform.clear();
    InstanceColor.clear();

    // Create vertex instance offset
    for (int i = 0; i < InstanceCount; ++i)
    {
        Transform tf;
        tf.t = { Rng(-10.f, 10.f), Rng(-10.f, 10.f), Rng(-10.f, 10.f) };
        tf.r = { 0.f, 0.f, 0.f };
        float s = Rng(0.1f, 1.5f);
        tf.s = { s, s, s };

        InstanceTransform.push_back(tf);
        InstanceModel.push_back(tf.GetModelMatrix());
        InstanceColor.push_back({ Rng(0.f, 1.f), Rng(0.f, 1.f), Rng(0.f, 1.f) });
    }

    // transform buffer
    glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * InstanceCount, InstanceModel.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void*)0);
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void*)sizeof(v4));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void*)(sizeof(v4) * 2));
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void*)(sizeof(v4) * 3));
    glVertexAttribDivisor(5, 1);

    // color buffer
    glBindBuffer(GL_ARRAY_BUFFER, InstanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v3) * InstanceCount, InstanceColor.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(v3), (void*)0);
    glVertexAttribDivisor(6, 1);
}

void demo_instancing::UpdateInstanceAttributes()
{
    InstanceModel[InstanceIndex] = InstanceTransform[InstanceIndex].GetModelMatrix();

    glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformVBO);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(mat4) * InstanceIndex, sizeof(mat4), InstanceModel[InstanceIndex].e);

    glBindBuffer(GL_ARRAY_BUFFER, InstanceColorVBO);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(v3) * InstanceIndex, sizeof(v3), InstanceColor[InstanceIndex].e);
}

void demo_instancing::AddInstanceAttributes()
{
    InstanceTransform.push_back(InstanceAdditionalTransform);
    InstanceModel.push_back(InstanceAdditionalTransform.GetModelMatrix());
    InstanceColor.push_back(InstanceAdditionalColor);

    ++InstanceCount;

    // transform buffer
    glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * InstanceCount, InstanceModel.data(), GL_STATIC_DRAW);

    // color buffer
    glBindBuffer(GL_ARRAY_BUFFER, InstanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v3) * InstanceCount, InstanceColor.data(), GL_STATIC_DRAW);
}

void demo_instancing::DestroyInstanceAttributes()
{
    InstanceTransform.erase(InstanceTransform.begin() + InstanceIndex);
    InstanceModel.erase(InstanceModel.begin() + InstanceIndex);
    InstanceColor.erase(InstanceColor.begin() + InstanceIndex);

    --InstanceCount;
    InstanceIndex = 0;

    // transform buffer
    glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * InstanceCount, InstanceModel.data(), GL_STATIC_DRAW);

    // color buffer
    glBindBuffer(GL_ARRAY_BUFFER, InstanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v3) * InstanceCount, InstanceColor.data(), GL_STATIC_DRAW);
}

void demo_instancing::DisplayDebugUI()
{
    if (ImGui::TreeNodeEx("demo_instancing", ImGuiTreeNodeFlags_Framed))
    {
        // Debug display
        if (ImGui::TreeNodeEx("Camera"))
        {
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", Camera.Position.x, Camera.Position.y, Camera.Position.z);
            ImGui::Text("Pitch: %.2f", Math::ToDegrees(Camera.Pitch));
            ImGui::Text("Yaw: %.2f", Math::ToDegrees(Camera.Yaw));

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Instancing"))
        {
            if (ImGui::DragInt("Instances", (int*)&InstanceCount, 1.f, 0, 9999))
            {
                InstanceIndex = 0;
                SetInstanceAttributes();
            }

            if (InstanceCount)
            {
                ImGui::SliderInt("Instance index", (int*)&InstanceIndex, 0, InstanceCount - 1);

                if (ImGui::DragFloat3("Instance position", InstanceTransform[InstanceIndex].t.e))
                    UpdateInstanceAttributes();
                if (ImGui::DragFloat3("Instance rotation", InstanceTransform[InstanceIndex].r.e))
                    UpdateInstanceAttributes();
                if (ImGui::DragFloat3("Instance scale", InstanceTransform[InstanceIndex].s.e))
                    UpdateInstanceAttributes();

                if (ImGui::DragFloat3("Instance color", InstanceColor[InstanceIndex].e, 1.f, 0.f, 1.f))
                    UpdateInstanceAttributes();

                if (ImGui::Button("Destroy"))
                    DestroyInstanceAttributes();
            }

            if (ImGui::TreeNodeEx("Add instance"))
            {
                ImGui::DragFloat3("Instance position", InstanceAdditionalTransform.t.e);
                ImGui::DragFloat3("Instance rotation", InstanceAdditionalTransform.r.e);
                ImGui::DragFloat3("Instance scale", InstanceAdditionalTransform.s.e);
                ImGui::DragFloat3("Instance color", InstanceAdditionalColor.e, 1.f, 0.f, 1.f);

                if (ImGui::Button("Add"))
                    AddInstanceAttributes();

                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}