#pragma once

#include "demo.h"

#include "opengl_headers.h"

#include "camera.h"

#include <vector>

class demo_instancing : public demo
{
public:
    demo_instancing();
    virtual ~demo_instancing();
    virtual void Update(const platform_io& IO);

    void Draw(const GLuint& Program, const mat4& ViewProj) const;
    void DrawInstanced(const GLuint& Program, const mat4& ViewProj, const GLsizei& instanceCount) const;

    void SetInstanceAttributes();
    void UpdateInstanceAttributes();
    void AddInstanceAttributes();
    void DestroyInstanceAttributes();

    void DisplayDebugUI();

private:
    // 3d camera
    camera Camera = {};
    
    // GL objects needed by this demo
    GLuint Program = 0;
    GLuint Texture = 0;

    GLuint VAO = 0;
    GLuint VertexBuffer = 0;
    int VertexCount = 0;

    GLuint InstanceTransformVBO = 0;
    GLuint InstanceColorVBO = 0;
    unsigned int InstanceCount = 10;
    unsigned int InstanceIndex = 0;

    std::vector<mat4>       InstanceModel;
    std::vector<Transform>  InstanceTransform;
    std::vector<v3>         InstanceColor;

    Transform               InstanceAdditionalTransform;
    v3                      InstanceAdditionalColor;
};