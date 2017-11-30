//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DrawElementsPerf:
//   Performance tests for ANGLE DrawElements call overhead.
//

#include <sstream>

#include "ANGLEPerfTest.h"
#include "DrawCallPerfParams.h"
#include "common/utilities.h"
#include "test_utils/draw_call_perf_utils.h"

namespace
{

GLuint CreateElementArrayBuffer(size_t count, GLenum type, GLenum usage)
{
    GLuint buffer = 0u;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(type), nullptr, usage);

    return buffer;
}

struct DrawElementsPerfParams final : public DrawCallPerfParams
{
    // Common default options
    DrawElementsPerfParams()
    {
        runTimeSeconds = 5.0;
        numTris        = 2;
    }

    std::string suffix() const override
    {
        std::stringstream strstr;

        strstr << DrawCallPerfParams::suffix();

        if (indexBufferChanged)
        {
            strstr << "_index_buffer_changed";
        }

        if (type == GL_UNSIGNED_SHORT)
        {
            strstr << "_ushort";
        }

        return strstr.str();
    }

    GLenum type             = GL_UNSIGNED_INT;
    bool indexBufferChanged = false;
};

std::ostream &operator<<(std::ostream &os, const DrawElementsPerfParams &params)
{
    os << params.suffix().substr(1);
    return os;
}

class DrawElementsPerfBenchmark : public ANGLERenderTest,
                                  public ::testing::WithParamInterface<DrawElementsPerfParams>
{
  public:
    DrawElementsPerfBenchmark();

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

  private:
    GLuint mProgram     = 0;
    GLuint mBuffer      = 0;
    GLuint mIndexBuffer = 0;
    GLuint mFBO         = 0;
    GLuint mTexture     = 0;
    GLsizei mBufferSize = 0;
    int mCount          = 3 * GetParam().numTris;
    std::vector<GLuint> mIntIndexData;
    std::vector<GLushort> mShortIndexData;
};

DrawElementsPerfBenchmark::DrawElementsPerfBenchmark()
    : ANGLERenderTest("DrawElementsPerf", GetParam())
{
    mRunTimeSeconds = GetParam().runTimeSeconds;
}

void DrawElementsPerfBenchmark::initializeBenchmark()
{
    const auto &params = GetParam();

    ASSERT_LT(0u, params.iterations);

    mProgram = SetupSimpleDrawProgram();
    ASSERT_NE(0u, mProgram);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    mBuffer      = Create2DTriangleBuffer(params.numTris, GL_STATIC_DRAW);
    mIndexBuffer = CreateElementArrayBuffer(mCount, params.type, GL_STATIC_DRAW);

    for (int i = 0; i < mCount; i++)
    {
        ASSERT_GE(std::numeric_limits<GLushort>::max(), mCount);
        mShortIndexData.push_back(static_cast<GLushort>(rand() % mCount));
        mIntIndexData.push_back(rand() % mCount);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);

    mBufferSize = gl::ElementTypeSize(params.type) * mCount;

    if (params.type == GL_UNSIGNED_INT)
    {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mBufferSize, mIntIndexData.data());
    }
    else
    {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mBufferSize, mShortIndexData.data());
    }

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    // Set the viewport
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    if (params.useFBO)
    {
        CreateColorFBO(getWindow()->getWidth(), getWindow()->getHeight(), &mTexture, &mFBO);
    }

    ASSERT_GL_NO_ERROR();
}

void DrawElementsPerfBenchmark::destroyBenchmark()
{
    glDeleteProgram(mProgram);
    glDeleteBuffers(1, &mBuffer);
    glDeleteBuffers(1, &mIndexBuffer);
    glDeleteTextures(1, &mTexture);
    glDeleteFramebuffers(1, &mFBO);
}

void DrawElementsPerfBenchmark::drawBenchmark()
{
    // This workaround fixes a huge queue of graphics commands accumulating on the GL
    // back-end. The GL back-end doesn't have a proper NULL device at the moment.
    // TODO(jmadill): Remove this when/if we ever get a proper OpenGL NULL device.
    const auto &eglParams = GetParam().eglParameters;
    if (eglParams.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE ||
        (eglParams.renderer != EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE &&
         eglParams.renderer != EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE))
    {
        glClear(GL_COLOR_BUFFER_BIT);
    }

    const auto &params = GetParam();

    for (unsigned int it = 0; it < params.iterations; it++)
    {
        if (params.indexBufferChanged)
        {
            if (params.type == GL_UNSIGNED_INT)
            {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mBufferSize, mIntIndexData.data());
            }
            else
            {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mBufferSize, mShortIndexData.data());
            }
        }
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mCount), params.type, 0);
    }

    ASSERT_GL_NO_ERROR();
}

DrawElementsPerfParams DrawElementsPerfD3D11Params(bool indexBufferChanged,
                                                   bool useNullDevice,
                                                   GLenum indexType)
{
    DrawElementsPerfParams params;
    params.eglParameters =
        useNullDevice ? angle::egl_platform::D3D11_NULL() : angle::egl_platform::D3D11();
    params.indexBufferChanged = indexBufferChanged;
    params.type               = indexType;
    return params;
}

DrawElementsPerfParams DrawElementsPerfD3D9Params(bool indexBufferChanged)
{
    DrawElementsPerfParams params;
    params.eglParameters      = angle::egl_platform::D3D9();
    params.indexBufferChanged = indexBufferChanged;
    return params;
}

DrawElementsPerfParams DrawElementsPerfOpenGLOrGLESParams(bool indexBufferChanged)
{
    DrawElementsPerfParams params;
    params.eglParameters      = angle::egl_platform::OPENGL_OR_GLES(false);
    params.indexBufferChanged = indexBufferChanged;
    return params;
}

TEST_P(DrawElementsPerfBenchmark, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(DrawElementsPerfBenchmark,
                       DrawElementsPerfD3D9Params(false),
                       DrawElementsPerfD3D9Params(true),
                       DrawElementsPerfD3D11Params(false, true, GL_UNSIGNED_INT),
                       DrawElementsPerfD3D11Params(true, true, GL_UNSIGNED_INT),
                       DrawElementsPerfD3D11Params(false, false, GL_UNSIGNED_INT),
                       DrawElementsPerfD3D11Params(true, false, GL_UNSIGNED_INT),
                       DrawElementsPerfD3D11Params(false, true, GL_UNSIGNED_SHORT),
                       DrawElementsPerfOpenGLOrGLESParams(false),
                       DrawElementsPerfOpenGLOrGLESParams(true));

}  // namespace
