#pragma once

#include <array>

#include "demo.h"

#include "opengl_headers.h"

#include "camera.h"

#include "tavern_scene.h"

class demo_postprocess : public demo
{
public:
    demo_postprocess(GL::cache& GLCache, GL::debug& GLDebug);
    virtual ~demo_postprocess();
    virtual void Update(const platform_io& IO);

    void RenderTavernFBO(const mat4& ProjectionMatrix, const mat4& ViewMatrix, const mat4& ModelMatrix);
    void RenderPingPong(const int iteration);

    void RenderScreen();

    void DisplayDebugUI();

private:
    GL::debug& GLDebug;

    // 3d camera
    camera Camera = {};

    // GL objects needed by the tavern
    GLuint TavernProgram = 0;
    GLuint TavernVAO = 0;

    tavern_scene TavernScene;

    bool Wireframe = false;

    GLuint RenderVAO = 0;
    GLuint RenderProgram = 0;

    GLuint FBO = 0;
    GLuint RawRenderTex = 0;
    GLuint RenderDepthMap = 0;
    unsigned int RenderResolution = 1024;

    //ping pong framebuffers
    bool pp = false;
    GLuint PostProcessProgram = 0;
    GLuint ppFBO[2];
    GLuint ppRenderTex[2];

    float Gamma = 2.2f;

    mat3 Kernel;
    int PostProcessCount = 1;
    float PostProcessOffset = 1 / 300.f;
};