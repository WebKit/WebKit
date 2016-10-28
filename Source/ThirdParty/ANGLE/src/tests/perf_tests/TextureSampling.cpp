//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureSamplingBenchmark:
//   Performance test for texture sampling. The test generates a texture containing random data
//   and then blurs it in a fragment shader using nearest neighbor sampling. The test is
//   specifically designed to test overhead of GLSL's builtin texture*() functions that may result
//   from how ANGLE translates them on each backend.
//

#include "ANGLEPerfTest.h"

#include <iostream>
#include <random>
#include <sstream>

#include "shader_utils.h"

using namespace angle;

namespace
{

struct TextureSamplingParams final : public RenderTestParams
{
    TextureSamplingParams()
    {
        // Common default params
        majorVersion = 2;
        minorVersion = 0;
        windowWidth  = 720;
        windowHeight = 720;
        iterations   = 4;

        numSamplers = 2;
        textureSize = 32;
        kernelSize  = 3;
    }

    std::string suffix() const override;
    unsigned int numSamplers;
    unsigned int textureSize;
    unsigned int kernelSize;

    // static parameters
    unsigned int iterations;
};

std::ostream &operator<<(std::ostream &os, const TextureSamplingParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

std::string TextureSamplingParams::suffix() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::suffix() << "_" << numSamplers << "samplers";

    return strstr.str();
}

class TextureSamplingBenchmark : public ANGLERenderTest,
                                 public ::testing::WithParamInterface<TextureSamplingParams>
{
  public:
    TextureSamplingBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  private:
    void initShaders();
    void initVertexBuffer();
    void initTextures();

    GLuint mProgram;
    GLuint mBuffer;
    std::vector<GLuint> mTextures;
};

TextureSamplingBenchmark::TextureSamplingBenchmark()
    : ANGLERenderTest("TextureSampling", GetParam()), mProgram(0u), mBuffer(0u)
{
}

void TextureSamplingBenchmark::initializeBenchmark()
{
    const auto &params = GetParam();

    ASSERT_LT(0u, params.iterations);

    // Verify "numSamplers" is within MAX_TEXTURE_IMAGE_UNITS limit
    GLint maxTextureImageUnits;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);

    if (params.numSamplers > static_cast<unsigned int>(maxTextureImageUnits))
    {
        FAIL() << "Sampler count (" << params.numSamplers << ")"
               << " exceeds maximum texture count: " << maxTextureImageUnits << std::endl;
    }
    initShaders();
    initVertexBuffer();
    initTextures();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    ASSERT_GL_NO_ERROR();
}

void TextureSamplingBenchmark::initShaders()
{
    const auto &params = GetParam();

    std::stringstream vstrstr;
    vstrstr << "attribute vec2 aPosition;\n"
               "varying vec2 vTextureCoordinates;\n"
               "void main()\n"
               "{\n"
               "    vTextureCoordinates = (aPosition + vec2(1.0)) * 0.5;\n"
               "    gl_Position = vec4(aPosition, 0, 1.0);\n"
               "}";

    std::stringstream fstrstr;
    fstrstr << "precision mediump float;\n"
               "varying vec2 vTextureCoordinates;\n";
    for (unsigned int count = 0; count < params.numSamplers; count++)
    {
        fstrstr << "uniform sampler2D uSampler" << count << ";\n";
    }
    fstrstr << "void main()\n"
               "{\n"
               "    const float inverseTextureSize = 1.0 / "
            << params.textureSize << ".0;\n"
                                     "    vec4 colorOut = vec4(0.0, 0.0, 0.0, 1.0);\n";
    for (unsigned int count = 0; count < params.numSamplers; count++)
    {
        fstrstr << "    for (int x = 0; x < " << params.kernelSize << "; ++x)\n"
                   "    {\n"
                   "        for (int y = 0; y < " << params.kernelSize << "; ++y)\n"
                   "        {\n"
                   "            colorOut += texture2D(uSampler" << count
                   << ", vTextureCoordinates + vec2(x, y) * inverseTextureSize) * 0.1;\n"
                   "        }\n"
                   "    }\n";
    }
    fstrstr << "    gl_FragColor = colorOut;\n"
               "}\n";

    mProgram = CompileProgram(vstrstr.str(), fstrstr.str());
    ASSERT_NE(0u, mProgram);

    // Use the program object
    glUseProgram(mProgram);
}

void TextureSamplingBenchmark::initVertexBuffer()
{
    std::vector<float> vertexPositions(12);
    {
        // Bottom left triangle
        vertexPositions[0] = -1.0f;
        vertexPositions[1] = -1.0f;
        vertexPositions[2] = 1.0f;
        vertexPositions[3] = -1.0f;
        vertexPositions[4] = -1.0f;
        vertexPositions[5] = 1.0f;

        // Top right triangle
        vertexPositions[6]  = -1.0f;
        vertexPositions[7]  = 1.0f;
        vertexPositions[8]  = 1.0f;
        vertexPositions[9]  = -1.0f;
        vertexPositions[10] = 1.0f;
        vertexPositions[11] = 1.0f;
    }

    glGenBuffers(1, &mBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexPositions.size() * sizeof(float), &vertexPositions[0],
                 GL_STATIC_DRAW);

    GLint positionLocation = glGetAttribLocation(mProgram, "aPosition");
    ASSERT_NE(-1, positionLocation);

    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);
}

void TextureSamplingBenchmark::initTextures()
{
    const auto &params = GetParam();

    unsigned int dataSize = params.textureSize * params.textureSize;
    std::vector<unsigned int> randomTextureData;
    randomTextureData.resize(dataSize);

    unsigned int pseudoRandom = 1u;
    for (unsigned int i = 0; i < dataSize; ++i)
    {
        pseudoRandom         = pseudoRandom * 1664525u + 1013904223u;
        randomTextureData[i] = pseudoRandom;
    }

    mTextures.resize(params.numSamplers);
    glGenTextures(params.numSamplers, mTextures.data());
    for (unsigned int i = 0; i < params.numSamplers; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, mTextures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.textureSize, params.textureSize, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, randomTextureData.data());
    }

    for (unsigned int count = 0; count < params.numSamplers; count++)
    {
        std::stringstream samplerstrstr;
        samplerstrstr << "uSampler" << count;
        GLint samplerLocation = glGetUniformLocation(mProgram, samplerstrstr.str().c_str());
        ASSERT_NE(-1, samplerLocation);

        glUniform1i(samplerLocation, count);
    }
}

void TextureSamplingBenchmark::destroyBenchmark()
{
    const auto &params = GetParam();

    glDeleteProgram(mProgram);
    glDeleteBuffers(1, &mBuffer);
    if (!mTextures.empty())
    {
        glDeleteTextures(params.numSamplers, mTextures.data());
    }
}

void TextureSamplingBenchmark::drawBenchmark()
{
    glClear(GL_COLOR_BUFFER_BIT);

    const auto &params = GetParam();

    for (unsigned int it = 0; it < params.iterations; ++it)
    {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    ASSERT_GL_NO_ERROR();
}

TextureSamplingParams D3D11Params()
{
    TextureSamplingParams params;
    params.eglParameters = egl_platform::D3D11();
    return params;
}

TextureSamplingParams D3D9Params()
{
    TextureSamplingParams params;
    params.eglParameters = egl_platform::D3D9();
    return params;
}

TextureSamplingParams OpenGLParams()
{
    TextureSamplingParams params;
    params.eglParameters = egl_platform::OPENGL();
    return params;
}

}  // anonymous namespace

TEST_P(TextureSamplingBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(TextureSamplingBenchmark, D3D11Params(), D3D9Params(), OpenGLParams());
