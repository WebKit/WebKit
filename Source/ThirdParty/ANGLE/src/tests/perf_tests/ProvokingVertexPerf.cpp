//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Measures provoking vertex index conversion, reuse, and invalidation.

#include <ostream>
#include <string>
#include <vector>

#include "ANGLEPerfTest.h"
#include "common/string_utils.h"
#include "test_utils/draw_call_perf_utils.h"

using namespace angle;

namespace
{
struct ProvokingVertexPerfParams final : public RenderTestParams
{
    ProvokingVertexPerfParams()
    {
        majorVersion      = 3;
        minorVersion      = 0;
        windowWidth       = 128;
        windowHeight      = 128;
        surfaceType       = SurfaceType::Offscreen;
        iterationsPerStep = 256;
    }

    std::string story() const override
    {
        return RenderTestParams::story() + (flat ? "_flat" : "_smooth") +
               (firstVertex ? "_first" : "_last") + (update ? "_updated" : "_static") +
               (ranges > 1 ? "_" + std::to_string(ranges) + "_ranges" : "");
    }

    bool flat        = true;
    bool firstVertex = false;
    bool update      = false;
    size_t ranges    = 1;
};

std::ostream &operator<<(std::ostream &stream, const ProvokingVertexPerfParams &params)
{
    return stream << params.backendAndStory().substr(1);
}

class ProvokingVertexPerf : public ANGLERenderTest,
                            public ::testing::WithParamInterface<ProvokingVertexPerfParams>
{
  public:
    ProvokingVertexPerf() : ANGLERenderTest("ProvokingVertexPerf", GetParam())
    {
        disableTestHarnessSwap();
        if (GetParam().firstVertex)
        {
            addExtensionPrerequisite("GL_ANGLE_provoking_vertex");
        }
    }

    void initializeBenchmark() override
    {
        CreateColorFBO(GetParam().windowWidth, GetParam().windowHeight, &mColorTexture,
                       &mFramebuffer);
        constexpr char kVS[] = R"(#version 300 es
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
flat out highp vec3 v_color;

void main()
{
    gl_Position = vec4(position, 0, 1);
    v_color = color;
}
)";
        constexpr char kFS[] = R"(#version 300 es
precision highp float;
flat in highp vec3 v_color;
out vec4 result;

void main()
{
    result = vec4(v_color, 1);
}
)";
        std::string vs       = kVS;
        std::string fs       = kFS;
        if (!GetParam().flat)
        {
            ReplaceSubstring(&vs, "flat ", "");
            ReplaceSubstring(&fs, "flat ", "");
        }
        mProgram = CompileProgram(vs.c_str(), fs.c_str());
        ASSERT_NE(0u, mProgram);
        glUseProgram(mProgram);

        constexpr size_t kTriangles = 256;
        std::vector<GLfloat> vertices;
        for (size_t i = 0; i < kTriangles; ++i)
        {
            const GLfloat x = static_cast<GLfloat>(i % 16) / 8 - 1;
            const GLfloat y = static_cast<GLfloat>(i / 16) / 8 - 1;
            vertices.insert(vertices.end(),
                            {x, y, 0, 0, 1, x + 0.1f, y, 0, 0, 1, x, y + 0.1f, 0, 0, 1});
            mIndices.push_back(static_cast<GLushort>(i * 3));
            mIndices.push_back(static_cast<GLushort>(i * 3 + 1));
            mIndices.push_back(static_cast<GLushort>(i * 3 + 2));
        }
        mIndexCount                            = static_cast<GLsizei>(mIndices.size());
        const std::vector<GLushort> firstRange = mIndices;
        for (size_t i = 1; i < GetParam().ranges; ++i)
        {
            mIndices.insert(mIndices.end(), firstRange.begin(), firstRange.end());
        }
        glGenBuffers(1, &mVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(),
                     GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), nullptr);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                              reinterpret_cast<void *>(2 * sizeof(GLfloat)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glGenBuffers(1, &mIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(GLushort), mIndices.data(),
                     GL_STATIC_DRAW);
        glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        if (GetParam().firstVertex)
        {
            glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION_ANGLE);
        }
        glViewport(0, 0, 128, 128);
        ASSERT_GL_NO_ERROR();
    }

    void drawBenchmark() override
    {
        glClear(GL_COLOR_BUFFER_BIT);
        for (size_t i = 0; i < GetParam().iterationsPerStep; ++i)
        {
            if (GetParam().update)
            {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mIndices.size() * sizeof(GLushort),
                                mIndices.data());
            }
            size_t offset = (i % GetParam().ranges) * mIndexCount * sizeof(GLushort);
            glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_SHORT,
                           reinterpret_cast<void *>(offset));
        }
        // Complete each batch without window presentation throttling the measurements.
        glFinish();
        ASSERT_GL_NO_ERROR();
    }

    void destroyBenchmark() override
    {
        glDeleteProgram(mProgram);
        glDeleteBuffers(1, &mVertexBuffer);
        glDeleteBuffers(1, &mIndexBuffer);
        glDeleteFramebuffers(1, &mFramebuffer);
        glDeleteTextures(1, &mColorTexture);
    }

  private:
    GLuint mProgram      = 0;
    GLuint mVertexBuffer = 0;
    GLuint mIndexBuffer  = 0;
    GLuint mColorTexture = 0;
    GLuint mFramebuffer  = 0;
    GLsizei mIndexCount  = 0;
    std::vector<GLushort> mIndices;
};

ProvokingVertexPerfParams Params(const EGLPlatformParameters &platform,
                                 bool flat,
                                 bool firstVertex,
                                 bool update,
                                 size_t ranges)
{
    ProvokingVertexPerfParams params;
    params.eglParameters = platform;
    params.flat          = flat;
    params.firstVertex   = firstVertex;
    params.update        = update;
    params.ranges        = ranges;
    return params;
}

std::vector<ProvokingVertexPerfParams> GetTestParams()
{
    std::vector<ProvokingVertexPerfParams> params;
    for (const EGLPlatformParameters &platform :
         {egl_platform::METAL(), egl_platform::OPENGL_OR_GLES(), egl_platform::VULKAN()})
    {
        params.push_back(Params(platform, false, false, false, 1));
        params.push_back(Params(platform, true, false, false, 1));
        params.push_back(Params(platform, true, true, false, 1));
        params.push_back(Params(platform, true, false, true, 1));
        params.push_back(Params(platform, true, false, false, 8));
    }
    return params;
}

// Measure provoking vertex emulation with reusable and changing indices.
TEST_P(ProvokingVertexPerf, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST_ARRAY(ProvokingVertexPerf, GetTestParams());
}  // namespace
