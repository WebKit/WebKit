//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TexturesPerf:
//   Performance test for setting texture state.
//

#include "ANGLEPerfTest.h"

#include <iostream>
#include <random>
#include <sstream>

#include "util/shader_utils.h"

namespace angle
{
constexpr unsigned int kIterationsPerStep = 256;

struct TexturesParams final : public RenderTestParams
{
    TexturesParams()
    {
        iterationsPerStep = kIterationsPerStep;

        // Common default params
        majorVersion = 2;
        minorVersion = 0;
        windowWidth  = 720;
        windowHeight = 720;

        numTextures                 = 8;
        textureRebindFrequency      = 5;
        textureStateUpdateFrequency = 3;
        textureMipCount             = 8;

        webgl = false;
    }

    std::string suffix() const override;
    size_t numTextures;
    size_t textureRebindFrequency;
    size_t textureStateUpdateFrequency;
    size_t textureMipCount;

    bool webgl;
};

std::ostream &operator<<(std::ostream &os, const TexturesParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

std::string TexturesParams::suffix() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::suffix();
    strstr << "_" << numTextures << "_textures";
    strstr << "_" << textureRebindFrequency << "_rebind";
    strstr << "_" << textureStateUpdateFrequency << "_state";
    strstr << "_" << textureMipCount << "_mips";

    if (webgl)
    {
        strstr << "_webgl";
    }

    return strstr.str();
}

class TexturesBenchmark : public ANGLERenderTest,
                          public ::testing::WithParamInterface<TexturesParams>
{
  public:
    TexturesBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  private:
    void initShaders();
    void initTextures();

    std::vector<GLuint> mTextures;

    GLuint mProgram;
    std::vector<GLuint> mUniformLocations;
};

TexturesBenchmark::TexturesBenchmark() : ANGLERenderTest("Textures", GetParam()), mProgram(0u)
{
    setWebGLCompatibilityEnabled(GetParam().webgl);
    setRobustResourceInit(GetParam().webgl);
}

void TexturesBenchmark::initializeBenchmark()
{
    const auto &params = GetParam();

    // Verify the uniform counts are within the limits
    GLint maxTextureUnits;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    if (params.numTextures > static_cast<size_t>(maxTextureUnits))
    {
        FAIL() << "Texture count (" << params.numTextures << ")"
               << " exceeds maximum texture unit count: " << maxTextureUnits << std::endl;
    }

    initShaders();
    initTextures();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    ASSERT_GL_NO_ERROR();
}

std::string GetUniformLocationName(size_t idx, bool vertexShader)
{
    std::stringstream strstr;
    strstr << (vertexShader ? "vs" : "fs") << "_u_" << idx;
    return strstr.str();
}

void TexturesBenchmark::initShaders()
{
    const auto &params = GetParam();

    std::string vs =
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(0, 0, 0, 0);\n"
        "}\n";

    std::stringstream fstrstr;
    for (size_t i = 0; i < params.numTextures; i++)
    {
        fstrstr << "uniform sampler2D tex" << i << ";";
    }
    fstrstr << "void main()\n"
               "{\n"
               "    gl_FragColor = vec4(0, 0, 0, 0)";
    for (size_t i = 0; i < params.numTextures; i++)
    {
        fstrstr << "+ texture2D(tex" << i << ", vec2(0, 0))";
    }
    fstrstr << ";\n"
               "}\n";

    mProgram = CompileProgram(vs.c_str(), fstrstr.str().c_str());
    ASSERT_NE(0u, mProgram);

    for (size_t i = 0; i < params.numTextures; ++i)
    {
        std::stringstream uniformName;
        uniformName << "tex" << i;

        GLint location = glGetUniformLocation(mProgram, uniformName.str().c_str());
        ASSERT_NE(-1, location);
        mUniformLocations.push_back(location);
    }

    // Use the program object
    glUseProgram(mProgram);
}

void TexturesBenchmark::initTextures()
{
    const auto &params = GetParam();

    size_t textureSize = static_cast<size_t>(1) << params.textureMipCount;
    std::vector<GLubyte> textureData(textureSize * textureSize * 4);
    for (auto &byte : textureData)
    {
        byte = rand() % 255u;
    }

    for (size_t texIndex = 0; texIndex < params.numTextures; texIndex++)
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);

        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + texIndex));
        glBindTexture(GL_TEXTURE_2D, tex);
        for (size_t mip = 0; mip < params.textureMipCount; mip++)
        {
            GLsizei levelSize = static_cast<GLsizei>(textureSize >> mip);
            glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(mip), GL_RGBA, levelSize, levelSize, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());
        }
        mTextures.push_back(tex);

        glUniform1i(mUniformLocations[texIndex], static_cast<GLint>(texIndex));
    }
}

void TexturesBenchmark::destroyBenchmark()
{
    glDeleteProgram(mProgram);
}

void TexturesBenchmark::drawBenchmark()
{
    const auto &params = GetParam();

    for (size_t it = 0; it < params.iterationsPerStep; ++it)
    {
        if (it % params.textureRebindFrequency == 0)
        {
            // Swap two textures
            size_t swapTexture = (it / params.textureRebindFrequency) % (params.numTextures - 1);

            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + swapTexture));
            glBindTexture(GL_TEXTURE_2D, mTextures[swapTexture]);
            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + swapTexture + 1));
            glBindTexture(GL_TEXTURE_2D, mTextures[swapTexture + 1]);
            std::swap(mTextures[swapTexture], mTextures[swapTexture + 1]);
        }

        if (it % params.textureStateUpdateFrequency == 0)
        {
            // Update a texture's state
            size_t stateUpdateCount = it / params.textureStateUpdateFrequency;

            const size_t numUpdateTextures = 4;
            ASSERT_LE(numUpdateTextures, params.numTextures);

            size_t firstTexture = stateUpdateCount % (params.numTextures - numUpdateTextures);

            for (size_t updateTextureIdx = 0; updateTextureIdx < numUpdateTextures;
                 updateTextureIdx++)
            {
                size_t updateTexture = firstTexture + updateTextureIdx;
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + updateTexture));

                const GLenum minFilters[] = {
                    GL_NEAREST,
                    GL_LINEAR,
                    GL_NEAREST_MIPMAP_NEAREST,
                    GL_LINEAR_MIPMAP_NEAREST,
                    GL_NEAREST_MIPMAP_LINEAR,
                    GL_LINEAR_MIPMAP_LINEAR,
                };
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                minFilters[stateUpdateCount % ArraySize(minFilters)]);

                const GLenum magFilters[] = {
                    GL_NEAREST,
                    GL_LINEAR,
                };
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                magFilters[stateUpdateCount % ArraySize(magFilters)]);

                const GLenum wrapParameters[] = {
                    GL_CLAMP_TO_EDGE,
                    GL_REPEAT,
                    GL_MIRRORED_REPEAT,
                };
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                wrapParameters[stateUpdateCount % ArraySize(wrapParameters)]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                wrapParameters[stateUpdateCount % ArraySize(wrapParameters)]);
            }
        }

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    ASSERT_GL_NO_ERROR();
}

TexturesParams D3D11Params(bool webglCompat)
{
    TexturesParams params;
    params.eglParameters = egl_platform::D3D11_NULL();
    params.webgl         = webglCompat;
    return params;
}

TexturesParams D3D9Params(bool webglCompat)
{
    TexturesParams params;
    params.eglParameters = egl_platform::D3D9_NULL();
    params.webgl         = webglCompat;
    return params;
}

TexturesParams OpenGLOrGLESParams(bool webglCompat)
{
    TexturesParams params;
    params.eglParameters = egl_platform::OPENGL_OR_GLES(true);
    params.webgl         = webglCompat;
    return params;
}

TEST_P(TexturesBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(TexturesBenchmark,
                       D3D11Params(false),
                       D3D11Params(true),
                       D3D9Params(true),
                       OpenGLOrGLESParams(false),
                       OpenGLOrGLESParams(true));
}  // namespace angle
