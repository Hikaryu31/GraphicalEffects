#pragma once

#include "demo.h"

#include "opengl_headers.h"

#include "camera.h"

class demo_skybox : public demo
{
public:
    demo_skybox();
    virtual ~demo_skybox();
    virtual void Update(const platform_io& IO);
    unsigned int loadCubemap(std::vector<std::string> faces);
    void DisplayDebugUI();

private:
    // 3d camera
    camera Camera = {};

    // GL objects needed by this demo
    GLuint Program = 0;
    GLuint SkyboxProgram = 0;
    GLuint Texture = 0;
    GLuint cubemapTexture;

    GLuint VAO = 0;
    GLuint skyboxVAO = 0;
    GLuint VertexBuffer = 0;
    int VertexCount = 0;
    bool refractive = false;
};