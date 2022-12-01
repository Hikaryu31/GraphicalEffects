#pragma once

#include <array>

#include "demo.h"

#include "opengl_headers.h"

#include "camera.h"

#include "tavern_scene.h"

class demo_shadowmap : public demo
{
public:
    demo_shadowmap(GL::cache& GLCache, GL::debug& GLDebug);
    virtual ~demo_shadowmap();
    virtual void Update(const platform_io& IO);

    void RenderTavern(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix, const mat4& LightSpaceMatrix) const;
    void RenderTavernDepthMap(const mat4& ModelMatrix, const mat4& LightSpaceMatrix) const;
    void RenderDepthMap() const;

    void DisplayDebugUI();

private:
    GL::debug& GLDebug;

    // 3d camera
    camera Camera = {};

    // shaders
    GLuint TavernProgram = 0;
    GLuint DepthProgram = 0;
    GLuint RenderProgram = 0;

    GLuint TavernVAO = 0;
    GLuint RenderVAO = 0;

    // depth map frame buffer
    GLuint DepthFBO = 0;
    // depth map texture
    GLuint DepthMap = 0;
    unsigned int DepthMapResolution = 1024;
    float LightRange = 10.f;

    tavern_scene TavernScene;

    bool Wireframe = false;
};