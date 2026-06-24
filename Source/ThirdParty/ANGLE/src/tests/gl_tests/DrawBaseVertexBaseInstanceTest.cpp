//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DrawBaseVertexBaseInstanceTest: Tests of GL_ANGLE_base_vertex_base_instance
// DrawBaseInstanceTest: Tests of GL_EXT_base_instance

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "gpu_info_util/SystemInfo.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include <numeric>

using namespace angle;

namespace
{

// Create a kWidth * kHeight canvas equally split into kCountX * kCountY tiles
// each containing a quad partially covering each tile
constexpr uint32_t kWidth                  = 256;
constexpr uint32_t kHeight                 = 256;
constexpr uint32_t kCountX                 = 8;
constexpr uint32_t kCountY                 = 8;
constexpr std::array<GLfloat, 2> kTileSize = {
    1.f / static_cast<GLfloat>(kCountX),
    1.f / static_cast<GLfloat>(kCountY),
};
constexpr std::array<uint32_t, 2> kTilePixelSize  = {kWidth / kCountX, kHeight / kCountY};
constexpr std::array<GLfloat, 2> kQuadRadius      = {0.25f * kTileSize[0], 0.25f * kTileSize[1]};
constexpr std::array<uint32_t, 2> kPixelCheckSize = {
    static_cast<uint32_t>(kQuadRadius[0] * kWidth),
    static_cast<uint32_t>(kQuadRadius[1] * kHeight)};

constexpr std::array<GLfloat, 2> getTileCenter(uint32_t x, uint32_t y)
{
    return {
        kTileSize[0] * (0.5f + static_cast<GLfloat>(x)),
        kTileSize[1] * (0.5f + static_cast<GLfloat>(y)),
    };
}
constexpr std::array<std::array<GLfloat, 3>, 4> getQuadVertices(uint32_t x, uint32_t y)
{
    const auto center = getTileCenter(x, y);
    return {
        std::array<GLfloat, 3>{center[0] - kQuadRadius[0], center[1] - kQuadRadius[1], 0.0f},
        std::array<GLfloat, 3>{center[0] + kQuadRadius[0], center[1] - kQuadRadius[1], 0.0f},
        std::array<GLfloat, 3>{center[0] + kQuadRadius[0], center[1] + kQuadRadius[1], 0.0f},
        std::array<GLfloat, 3>{center[0] - kQuadRadius[0], center[1] + kQuadRadius[1], 0.0f},
    };
}

enum class BaseVertexOption
{
    NoBaseVertex,
    UseBaseVertex
};

enum class BaseInstanceOption
{
    NoBaseInstance,
    UseBaseInstance
};

enum class BufferDataUsageOption
{
    StaticDraw,
    DynamicDraw
};

using DrawBaseVertexBaseInstanceTestParams = std::
    tuple<angle::PlatformParameters, BaseVertexOption, BaseInstanceOption, BufferDataUsageOption>;

struct PrintToStringParamName
{
    std::string operator()(
        const ::testing::TestParamInfo<DrawBaseVertexBaseInstanceTestParams> &info) const
    {
        ::std::stringstream ss;
        ss << std::get<0>(info.param) << "_"
           << (std::get<3>(info.param) == BufferDataUsageOption::StaticDraw ? "_StaticDraw"
                                                                            : "_DynamicDraw")
           << (std::get<2>(info.param) == BaseInstanceOption::UseBaseInstance ? "_UseBaseInstance"
                                                                              : "")
           << (std::get<1>(info.param) == BaseVertexOption::UseBaseVertex ? "_UseBaseVertex" : "");
        return ss.str();
    }
};

// These tests check correctness of the ANGLE_base_vertex_base_instance extension.
// An array of quads is drawn across the screen.
// gl_VertexID, gl_InstanceID, gl_BaseVertex, and gl_BaseInstance
// are checked by using them to select the color of the draw.
class DrawBaseVertexBaseInstanceTest
    : public ANGLETestBase,
      public ::testing::WithParamInterface<DrawBaseVertexBaseInstanceTestParams>
{
  protected:
    DrawBaseVertexBaseInstanceTest() : ANGLETestBase(std::get<0>(GetParam()))
    {
        setWindowWidth(kWidth);
        setWindowHeight(kHeight);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);

        // Rects in the same column are within a vertex array, testing gl_VertexID, gl_BaseVertex
        // Rects in the same row are drawn by instancing, testing gl_InstanceID, gl_BaseInstance

        mIndices = {0, 1, 2, 0, 2, 3};

        for (uint32_t y = 0; y < kCountY; ++y)
        {
            // v3 ---- v2
            // |       |
            // |       |
            // v0 ---- v1

            const auto vs = getQuadVertices(0, y);

            for (const auto &v : vs)
            {
                mVertices.insert(mVertices.end(), v.begin(), v.end());
            }

            for (GLushort i : mIndices)
            {
                mNonIndexedVertices.insert(mNonIndexedVertices.end(), vs[i].begin(), vs[i].end());
            }
        }

        mRegularIndices.resize(kCountY * mIndices.size());
        for (uint32_t i = 0; i < kCountY; i++)
        {
            uint32_t oi = 6 * i;
            uint32_t ov = 4 * i;
            for (uint32_t j = 0; j < 6; j++)
            {
                mRegularIndices[oi + j] = mIndices[j] + ov;
            }
        }

        std::iota(mInstancedArrayId.begin(), mInstancedArrayId.end(), 0.0f);
        std::reverse_copy(mInstancedArrayId.begin(), mInstancedArrayId.end(),
                          mInstancedArrayColorId.begin());
    }

    void SetUp() override { ANGLETestBase::ANGLETestSetUp(); }

    bool useBaseVertexBuiltin() const
    {
        return std::get<1>(GetParam()) == BaseVertexOption::UseBaseVertex;
    }

    bool useBaseInstanceBuiltin() const
    {
        return std::get<2>(GetParam()) == BaseInstanceOption::UseBaseInstance;
    }

    GLenum getBufferDataUsage() const
    {
        return std::get<3>(GetParam()) == BufferDataUsageOption::StaticDraw ? GL_STATIC_DRAW
                                                                            : GL_DYNAMIC_DRAW;
    }

    std::string vertexShaderSource300(bool isDrawArrays, bool isMultiDraw, bool divisorTest)
    {
        // Each color channel is to test the value of
        // R: gl_InstanceID and gl_BaseInstance
        // G: gl_VertexID and gl_BaseVertex
        // B: gl_BaseVertex

        std::stringstream shader;
        shader << ("#version 300 es\n")
               << (isMultiDraw ? "#extension GL_ANGLE_multi_draw : require\n" : "")
               << ("#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require\n")
               << "#define kCountX " << kCountX << "\n"
               << "#define kCountY " << kCountY << "\n"
               << R"(
in vec2 vPosition;
)" << (useBaseInstanceBuiltin() ? "" : "in float vInstanceID;\n")
               << (!divisorTest ? "" : "in float vInstanceColorID;\n") << R"(
out vec4 color;
void main()
{
    const float xStep = 1.0 / float(kCountX);
    const float yStep = 1.0 / float(kCountY);
    float x_id = )"
               << (useBaseInstanceBuiltin() ? " float(gl_InstanceID + gl_BaseInstance);"
                                            : "vInstanceID;")
               << "float x_color = "
               << (divisorTest ? "xStep * (vInstanceColorID + 1.0f);" : " 1.0 - xStep * x_id;")
               << R"(
    float y_id = float(gl_VertexID / )"
               << (isDrawArrays ? "6" : "4") << R"();

    color = vec4(
        x_color,
        1.0 - yStep * y_id,
        )" << (useBaseVertexBuiltin() ? "1.0 - yStep * float(gl_BaseVertex) / 4.0" : "1.0")
               << R"(,
        1);

    mat3 transform = mat3(1.0);
    transform[2][0] = x_id * xStep;

    gl_Position = vec4(transform * vec3(vPosition, 1.0) * 2.0 - 1.0, 1);
})";

        return shader.str();
    }

    std::string fragmentShaderSource300()
    {
        return
            R"(#version 300 es
precision mediump float;
in vec4 color;
out vec4 o_color;
void main()
{
    o_color = color;
})";
    }

    void setupProgram(GLProgram &program,
                      bool isDrawArrays  = true,
                      bool isMultiDraw   = false,
                      bool isDivisorTest = false)
    {
        program.makeRaster(vertexShaderSource300(isDrawArrays, isMultiDraw, isDivisorTest).c_str(),
                           fragmentShaderSource300().c_str());
        EXPECT_GL_NO_ERROR();
        ASSERT_TRUE(program.valid());
        glUseProgram(program);
        mPositionLoc = glGetAttribLocation(program, "vPosition");
        if (!useBaseInstanceBuiltin())
        {
            mInstanceIDLoc      = glGetAttribLocation(program, "vInstanceID");
            mInstanceColorIDLoc = glGetAttribLocation(program, "vInstanceColorID");
        }
    }

    void setupNonIndexedBuffers(GLBuffer &vertexBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mNonIndexedVertices.size(),
                     mNonIndexedVertices.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupIndexedBuffers(GLBuffer &vertexBuffer, GLBuffer &indexBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mVertices.size(), mVertices.data(),
                     getBufferDataUsage());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * mIndices.size(), mIndices.data(),
                     getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupInstanceIDBuffer(GLBuffer &instanceIDBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, instanceIDBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mInstancedArrayId.size(),
                     mInstancedArrayId.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupInstanceColorIDBuffer(GLBuffer &instanceIDBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, instanceIDBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mInstancedArrayColorId.size(),
                     mInstancedArrayColorId.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupRegularIndexedBuffer(GLBuffer &indexBuffer)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * mRegularIndices.size(),
                     mRegularIndices.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupPositionVertexAttribPointer()
    {
        glEnableVertexAttribArray(mPositionLoc);
        glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    void setupInstanceIDVertexAttribPointer(GLuint instanceIDLoc)
    {
        glEnableVertexAttribArray(instanceIDLoc);
        glVertexAttribPointer(instanceIDLoc, 1, GL_FLOAT, GL_FALSE, 0, 0);
        glVertexAttribDivisor(instanceIDLoc, 1);
    }

    void doDrawArraysInstancedBaseInstance()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const uint32_t countPerDraw = kCountY * 6;

        for (uint32_t i = 0; i < kCountX; i += 2)
        {
            glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLES, 0, countPerDraw, 2, i);
        }
    }

    void doMultiDrawArraysInstancedBaseInstance()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const uint32_t countPerDraw = kCountY * 6;

        const GLsizei drawCount = kCountX / 2;
        const std::vector<GLsizei> counts(drawCount, countPerDraw);
        const std::vector<GLsizei> firsts(drawCount, 0);
        const std::vector<GLsizei> instanceCounts(drawCount, 2);
        std::vector<GLuint> baseInstances(drawCount);
        for (size_t i = 0; i < drawCount; i++)
        {
            baseInstances[i] = i * 2;
        }
        glMultiDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLES, firsts.data(), counts.data(),
                                                    instanceCounts.data(), baseInstances.data(),
                                                    drawCount);
    }

    void doDrawElementsInstancedBaseVertexBaseInstance()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const uint32_t countPerDraw = 6;

        for (uint32_t v = 0; v < kCountY; v++)
        {
            for (uint32_t i = 0; i < kCountX; i += 2)
            {
                glDrawElementsInstancedBaseVertexBaseInstanceANGLE(
                    GL_TRIANGLES, countPerDraw, GL_UNSIGNED_SHORT,
                    reinterpret_cast<GLvoid *>(static_cast<uintptr_t>(0)), 2, v * 4, i);
            }
        }
    }

    // Call this after the *BaseVertexBaseInstance draw call to check if value of BaseVertex and
    // BaseInstance are reset to zero
    void doDrawArraysBaseInstanceReset()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6 * kCountY, 1);
    }

    void doDrawElementsBaseVertexBaseInstanceReset()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawElementsInstanced(GL_TRIANGLES, 6 * kCountY, GL_UNSIGNED_SHORT,
                                reinterpret_cast<GLvoid *>(static_cast<uintptr_t>(0)), 1);
    }

    void doMultiDrawElementsInstancedBaseVertexBaseInstance()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const GLsizei drawCount = kCountX * kCountY / 2;
        const std::vector<GLsizei> counts(drawCount, 6);
        const std::vector<GLsizei> instanceCounts(drawCount, 2);
        const std::vector<GLvoid *> indices(drawCount, 0);
        std::vector<GLint> baseVertices(drawCount);
        std::vector<GLuint> baseInstances(drawCount);

        GLsizei b = 0;
        for (uint32_t v = 0; v < kCountY; v++)
        {
            for (uint32_t i = 0; i < kCountX; i += 2)
            {
                baseVertices[b]  = v * 4;
                baseInstances[b] = i;
                b++;
            }
        }

        glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(
            GL_TRIANGLES, counts.data(), GL_UNSIGNED_SHORT, indices.data(), instanceCounts.data(),
            baseVertices.data(), baseInstances.data(), drawCount);
    }

    void checkDrawResult(bool hasBaseVertex, bool oneColumn = false)
    {
        uint32_t numColums = oneColumn ? 1 : kCountX;
        for (uint32_t y = 0; y < kCountY; ++y)
        {
            for (uint32_t x = 0; x < numColums; ++x)
            {
                uint32_t center_x = x * kTilePixelSize[0] + kTilePixelSize[0] / 2;
                uint32_t center_y = y * kTilePixelSize[1] + kTilePixelSize[1] / 2;

                EXPECT_PIXEL_NEAR(center_x - kPixelCheckSize[0] / 2,
                                  center_y - kPixelCheckSize[1] / 2,
                                  256.0 * (1.0 - (float)x / (float)kCountX),
                                  256.0 * (1.0 - (float)y / (float)kCountY),
                                  useBaseVertexBuiltin() && hasBaseVertex && !oneColumn
                                      ? 256.0 * (1.0 - (float)y / (float)kCountY)
                                      : 255,
                                  255, 3);
            }
        }
    }

    void TearDown() override { ANGLETestBase::ANGLETestTearDown(); }

    bool requestDrawBaseVertexBaseInstanceExtension()
    {
        if (IsGLExtensionRequestable("GL_ANGLE_base_vertex_base_instance"))
        {
            glRequestExtensionANGLE("GL_ANGLE_base_vertex_base_instance");
        }

        if (!IsGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance"))
        {
            return false;
        }

        if (IsGLExtensionRequestable("GL_ANGLE_base_vertex_base_instance_shader_builtin"))
        {
            glRequestExtensionANGLE("GL_ANGLE_base_vertex_base_instance_shader_builtin");
        }

        return true;
    }

    bool requestInstancedExtension()
    {
        if (IsGLExtensionRequestable("GL_ANGLE_instanced_arrays"))
        {
            glRequestExtensionANGLE("GL_ANGLE_instanced_arrays");
        }

        if (!IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"))
        {
            return false;
        }

        return true;
    }

    bool requestExtensions()
    {
        if (getClientMajorVersion() <= 2)
        {
            if (!requestInstancedExtension())
            {
                return false;
            }
        }
        return requestDrawBaseVertexBaseInstanceExtension();
    }

    // Used for base vertex base instance draw calls
    std::vector<GLushort> mIndices;
    std::vector<GLfloat> mVertices;
    std::vector<GLfloat> mNonIndexedVertices;
    // Used when gl_BaseInstance is not used
    std::array<GLfloat, kCountX> mInstancedArrayId      = {};
    std::array<GLfloat, kCountX> mInstancedArrayColorId = {};
    // Used for regular draw calls without base vertex base instance
    std::vector<GLushort> mRegularIndices;
    GLint mPositionLoc         = 0;
    GLuint mInstanceIDLoc      = 0;
    GLuint mInstanceColorIDLoc = 0;
};

using DrawBaseInstanceTestParams = std::
    tuple<angle::PlatformParameters, BaseVertexOption, BaseInstanceOption, BufferDataUsageOption>;

// The tests in DrawBaseInstanceTest check the correctness
// of the EXT_base_instance extension.
// gl_VertexID, gl_InstanceID, gl_BaseVertex, and gl_BaseInstance
// are checked by using them to select the color of the draw.
class DrawBaseInstanceTest : public ANGLETestBase,
                             public ::testing::WithParamInterface<DrawBaseInstanceTestParams>
{
  protected:
    DrawBaseInstanceTest() : ANGLETestBase(std::get<0>(GetParam()))
    {
        setWindowWidth(kWidth);
        setWindowHeight(kHeight);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);

        // Rects in the same column are within a vertex array, testing gl_VertexID, gl_BaseVertex
        // Rects in the same row are drawn by instancing, testing gl_InstanceID, gl_BaseInstance

        mIndices = {0, 1, 2, 0, 2, 3};

        for (uint32_t y = 0; y < kCountY; ++y)
        {
            // v3 ---- v2
            // |       |
            // |       |
            // v0 ---- v1

            const auto vs = getQuadVertices(0, y);

            for (const auto &v : vs)
            {
                mVertices.insert(mVertices.end(), v.begin(), v.end());
            }

            for (GLushort i : mIndices)
            {
                mNonIndexedVertices.insert(mNonIndexedVertices.end(), vs[i].begin(), vs[i].end());
            }
        }

        mRegularIndices.resize(kCountY * mIndices.size());
        for (uint32_t i = 0; i < kCountY; i++)
        {
            uint32_t oi = 6 * i;
            uint32_t ov = 4 * i;
            for (uint32_t j = 0; j < 6; j++)
            {
                mRegularIndices[oi + j] = mIndices[j] + ov;
            }
        }

        std::iota(mInstancedArrayId.begin(), mInstancedArrayId.end(), 0.0f);
        std::reverse_copy(mInstancedArrayId.begin(), mInstancedArrayId.end(),
                          mInstancedArrayColorId.begin());
    }

    void SetUp() override { ANGLETestBase::ANGLETestSetUp(); }

    bool useBaseVertexBuiltin() const
    {
        return std::get<1>(GetParam()) == BaseVertexOption::UseBaseVertex;
    }

    bool useBaseInstanceBuiltin() const
    {
        return std::get<2>(GetParam()) == BaseInstanceOption::UseBaseInstance;
    }

    GLenum getBufferDataUsage() const
    {
        return std::get<3>(GetParam()) == BufferDataUsageOption::StaticDraw ? GL_STATIC_DRAW
                                                                            : GL_DYNAMIC_DRAW;
    }

    std::string vertexShaderSource300(bool isDrawArrays, bool divisorTest)
    {
        // Each color channel is to test the value of
        // R: gl_InstanceID and gl_BaseInstance
        // G: gl_VertexID and gl_BaseVertex
        // B: gl_BaseVertex

        std::stringstream shader;
        shader << ("#version 300 es\n")
               << ("#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require\n")
               << "#define kCountX " << kCountX << "\n"
               << "#define kCountY " << kCountY << "\n"
               << R"(
in vec2 vPosition;
)" << (useBaseInstanceBuiltin() ? "" : "in float vInstanceID;\n")
               << (!divisorTest ? "" : "in float vInstanceColorID;\n") << R"(
// Use a mediump uniform, it can get sorted before the gl_BaseVertex/Instance emulated uniforms
uniform mediump vec2 zero;
out vec4 color;
void main()
{
    const float xStep = 1.0 / float(kCountX);
    const float yStep = 1.0 / float(kCountY);
    float x_id = )"
               << (useBaseInstanceBuiltin() ? " float(gl_InstanceID + gl_BaseInstance);"
                                            : "vInstanceID;")
               << "float x_color = "
               << (divisorTest ? "xStep * (vInstanceColorID + 1.0f);" : " 1.0 - xStep * x_id;")
               << R"(
    float y_id = float(gl_VertexID / )"
               << (isDrawArrays ? "6" : "4") << R"();

    color = vec4(
        x_color,
        1.0 - yStep * y_id,
        )" << (useBaseVertexBuiltin() ? "1.0 - yStep * float(gl_BaseVertex) / 4.0" : "1.0")
               << R"(,
        1);
    color.xy += zero;

    mat3 transform = mat3(1.0);
    transform[2][0] = x_id * xStep;

    gl_Position = vec4(transform * vec3(vPosition, 1.0) * 2.0 - 1.0, 1);
})";

        return shader.str();
    }

    std::string fragmentShaderSource300()
    {
        return
            R"(#version 300 es
precision mediump float;
in vec4 color;
out vec4 o_color;
void main()
{
    o_color = color;
})";
    }

    void setupProgram(GLProgram &program, bool isDrawArrays = true, bool isDivisorTest = false)
    {
        program.makeRaster(vertexShaderSource300(isDrawArrays, isDivisorTest).c_str(),
                           fragmentShaderSource300().c_str());
        EXPECT_GL_NO_ERROR();
        ASSERT_TRUE(program.valid());
        glUseProgram(program);
        mPositionLoc = glGetAttribLocation(program, "vPosition");
        if (!useBaseInstanceBuiltin())
        {
            mInstanceIDLoc      = glGetAttribLocation(program, "vInstanceID");
            mInstanceColorIDLoc = glGetAttribLocation(program, "vInstanceColorID");
        }
    }

    void setupNonIndexedBuffers(GLBuffer &vertexBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mNonIndexedVertices.size(),
                     mNonIndexedVertices.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupIndexedBuffers(GLBuffer &vertexBuffer, GLBuffer &indexBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mVertices.size(), mVertices.data(),
                     getBufferDataUsage());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * mIndices.size(), mIndices.data(),
                     getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupInstanceIDBuffer(GLBuffer &instanceIDBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, instanceIDBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mInstancedArrayId.size(),
                     mInstancedArrayId.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupInstanceColorIDBuffer(GLBuffer &instanceIDBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, instanceIDBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mInstancedArrayColorId.size(),
                     mInstancedArrayColorId.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupRegularIndexedBuffer(GLBuffer &indexBuffer)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * mRegularIndices.size(),
                     mRegularIndices.data(), getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void setupPositionVertexAttribPointer()
    {
        glEnableVertexAttribArray(mPositionLoc);
        glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    void setupInstanceIDVertexAttribPointer(GLuint instanceIDLoc)
    {
        glEnableVertexAttribArray(instanceIDLoc);
        glVertexAttribPointer(instanceIDLoc, 1, GL_FLOAT, GL_FALSE, 0, 0);
        glVertexAttribDivisor(instanceIDLoc, 1);
    }

    void doDrawArraysInstancedBaseInstance()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const uint32_t countPerDraw = kCountY * 6;

        for (uint32_t i = 0; i < kCountX; i += 2)
        {
            glDrawArraysInstancedBaseInstanceEXT(GL_TRIANGLES, 0, countPerDraw, 2, i);
        }
    }

    void doDrawElementsInstancedBaseInstance()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const uint32_t countPerDraw = 6;

        for (uint32_t v = 0; v < kCountY; v++)
        {
            for (uint32_t i = 0; i < kCountX; i += 2)
            {
                glDrawElementsInstancedBaseInstanceEXT(GL_TRIANGLES, countPerDraw,
                                                       GL_UNSIGNED_SHORT,
                                                       (void *)(v * 6 * sizeof(GLushort)), 2, i);
            }
        }
    }

    void doDrawElementsInstancedBaseVertexBaseInstance()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const uint32_t countPerDraw = 6;

        for (uint32_t v = 0; v < kCountY; v++)
        {
            for (uint32_t i = 0; i < kCountX; i += 2)
            {
                glDrawElementsInstancedBaseVertexBaseInstanceEXT(
                    GL_TRIANGLES, countPerDraw, GL_UNSIGNED_SHORT,
                    reinterpret_cast<GLvoid *>(static_cast<uintptr_t>(0)), 2, v * 4, i);
            }
        }
    }

    // Call this after the *BaseInstance draw call to check
    // if value of BaseInstance is reset to zero
    void doDrawArraysBaseInstanceReset()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6 * kCountY, 1);
    }

    void doDrawElementsInstancedBaseInstanceReset()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawElementsInstanced(GL_TRIANGLES, 6 * kCountY, GL_UNSIGNED_SHORT,
                                reinterpret_cast<GLvoid *>(static_cast<uintptr_t>(0)), 1);
    }

    void doDrawElementsBaseVertexBaseInstanceReset()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawElementsInstanced(GL_TRIANGLES, 6 * kCountY, GL_UNSIGNED_SHORT,
                                reinterpret_cast<GLvoid *>(static_cast<uintptr_t>(0)), 1);
    }

    void checkDrawResult(bool hasBaseVertex, bool oneColumn = false)
    {
        uint32_t numColums = oneColumn ? 1 : kCountX;
        for (uint32_t y = 0; y < kCountY; ++y)
        {
            for (uint32_t x = 0; x < numColums; ++x)
            {
                uint32_t center_x = x * kTilePixelSize[0] + kTilePixelSize[0] / 2;
                uint32_t center_y = y * kTilePixelSize[1] + kTilePixelSize[1] / 2;

                EXPECT_PIXEL_NEAR(center_x - kPixelCheckSize[0] / 2,
                                  center_y - kPixelCheckSize[1] / 2,
                                  256.0 * (1.0 - (float)x / (float)kCountX),
                                  256.0 * (1.0 - (float)y / (float)kCountY),
                                  useBaseVertexBuiltin() && hasBaseVertex && !oneColumn
                                      ? 256.0 * (1.0 - (float)y / (float)kCountY)
                                      : 255,
                                  255, 3);
            }
        }
    }

    void TearDown() override { ANGLETestBase::ANGLETestTearDown(); }

    bool requestExtensions()
    {
        if (IsGLExtensionRequestable("GL_ANGLE_base_vertex_base_instance_shader_builtin"))
        {
            glRequestExtensionANGLE("GL_ANGLE_base_vertex_base_instance_shader_builtin");
        }

        if (!IsGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance_shader_builtin"))
        {
            return false;
        }

        return EnsureGLExtensionEnabled("GL_EXT_base_instance");
    }

    // Used for base vertex base instance draw calls
    std::vector<GLushort> mIndices;
    std::vector<GLfloat> mVertices;
    std::vector<GLfloat> mNonIndexedVertices;
    // Used when gl_BaseInstance is not used
    std::array<GLfloat, kCountX> mInstancedArrayId      = {};
    std::array<GLfloat, kCountX> mInstancedArrayColorId = {};
    // Used for regular draw calls without base vertex base instance
    std::vector<GLushort> mRegularIndices;
    GLint mPositionLoc         = 0;
    GLuint mInstanceIDLoc      = 0;
    GLuint mInstanceColorIDLoc = 0;
};

// Tests that compile a program with the extension succeeds
TEST_P(DrawBaseVertexBaseInstanceTest, CanCompile)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());
    GLProgram p0;
    setupProgram(p0, true, false);
    GLProgram p1;
    setupProgram(p1, true, true);
    GLProgram p2;
    setupProgram(p2, false, false);
    GLProgram p3;
    setupProgram(p3, false, true);
}

// Tests that negative baseVertex works properly.
TEST_P(DrawBaseVertexBaseInstanceTest, NegativeBaseVertex)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program, false, false);

    GLBuffer vertexBuffer;
    GLBuffer indexBuffer;
    setupIndexedBuffers(vertexBuffer, indexBuffer);
    setupPositionVertexAttribPointer();

    // Create a new index buffer with shifted indices: {4, 5, 6, 4, 6, 7}
    // These indices point to the second quad (vertices 4, 5, 6, 7).
    std::vector<GLushort> shiftedIndices = {4, 5, 6, 4, 6, 7};
    GLBuffer shiftedIndexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shiftedIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * shiftedIndices.size(),
                 shiftedIndices.data(), GL_STATIC_DRAW);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw using baseVertex = -4.
    // This should shift the indices back to {0, 1, 2, 0, 2, 3}, drawing the first quad!
    glDrawElementsInstancedBaseVertexBaseInstanceANGLE(
        GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, reinterpret_cast<GLvoid *>(static_cast<uintptr_t>(0)),
        1, -4, 0);

    ASSERT_GL_NO_ERROR();

    // Check that the first quad was drawn as white.
    EXPECT_PIXEL_NEAR(16, 16, 255, 255, 255, 255, 3);
}

// Tests if baseInstance works properly with instanced array with non-zero divisor
TEST_P(DrawBaseVertexBaseInstanceTest, BaseInstanceDivisor)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());
    ANGLE_SKIP_TEST_IF(useBaseInstanceBuiltin());

    GLProgram program;
    setupProgram(program, true, false, true);

    GLBuffer nonIndexedVertexBuffer;
    setupNonIndexedBuffers(nonIndexedVertexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    GLBuffer instanceColorIDBuffer;

    setupInstanceIDBuffer(instanceIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    setupInstanceColorIDBuffer(instanceColorIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceColorIDLoc);

    doDrawArraysInstancedBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false);

    doDrawArraysBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);

    GLProgram programIndexed;
    setupProgram(programIndexed, false, false, true);

    GLBuffer indexBuffer;
    GLBuffer vertexBuffer;
    setupIndexedBuffers(vertexBuffer, indexBuffer);
    setupPositionVertexAttribPointer();

    glBindBuffer(GL_ARRAY_BUFFER, instanceIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    glBindBuffer(GL_ARRAY_BUFFER, instanceColorIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceColorIDLoc);

    doDrawElementsInstancedBaseVertexBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true);

    setupRegularIndexedBuffer(indexBuffer);
    doDrawElementsBaseVertexBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);
}

// Tests basic functionality of glDrawArraysInstancedBaseInstanceANGLE
TEST_P(DrawBaseVertexBaseInstanceTest, DrawArraysInstancedBaseInstance)
{
    // TODO(shrekshao): Temporarily skip this test
    // before we could try updating win AMD bot driver version
    // after Lab team fixed issues with ssh into bot machines
    // Currently this test fail on certain Win7/Win2008Server AMD GPU
    // with driver version 23.20.185.235 when using OpenGL backend.
    // This failure couldn't be produced on local Win10 AMD machine with latest driver installed
    // Same for the MultiDrawArraysInstancedBaseInstance test
    if (IsAMD() && IsWindows() && IsDesktopOpenGL())
    {
        SystemInfo *systemInfo = GetTestSystemInfo();
        if (!systemInfo->gpus.empty())
        {
            ANGLE_SKIP_TEST_IF(0x6613 == systemInfo->gpus[systemInfo->activeGPUIndex].deviceId);
        }
    }

    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program, true);

    GLBuffer vertexBuffer;
    setupNonIndexedBuffers(vertexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    if (!useBaseInstanceBuiltin())
    {
        setupInstanceIDBuffer(instanceIDBuffer);
        setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    }

    doDrawArraysInstancedBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false);

    doDrawArraysBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);
}

// Tests basic drawcount validation
TEST_P(DrawBaseVertexBaseInstanceTest, MultiDrawValidation)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance"));
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_multi_draw"));

    const GLint first           = 0;
    const GLsizei count         = 0;
    const GLsizei instanceCount = 0;
    const GLint baseVertex      = 0;
    const GLuint baseInstance   = 0;
    const GLvoid *const indices[1]{nullptr};

    glMultiDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLES, &first, &count, &instanceCount,
                                                &baseInstance, -1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glMultiDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLES, &first, &count, &instanceCount,
                                                &baseInstance, 0);
    EXPECT_GL_NO_ERROR();

    glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(GL_TRIANGLES, &count, GL_UNSIGNED_SHORT,
                                                            indices, &instanceCount, &baseVertex,
                                                            &baseInstance, -1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(GL_TRIANGLES, &count, GL_UNSIGNED_SHORT,
                                                            indices, &instanceCount, &baseVertex,
                                                            &baseInstance, 0);
    EXPECT_GL_NO_ERROR();
}

// Tests basic functionality of glMultiDrawArraysInstancedBaseInstance
TEST_P(DrawBaseVertexBaseInstanceTest, MultiDrawArraysInstancedBaseInstance)
{
    if (IsAMD() && IsWindows() && IsDesktopOpenGL())
    {
        SystemInfo *systemInfo = GetTestSystemInfo();
        if (!(systemInfo->activeGPUIndex < 0 || systemInfo->gpus.empty()))
        {
            ANGLE_SKIP_TEST_IF(0x6613 == systemInfo->gpus[systemInfo->activeGPUIndex].deviceId);
        }
    }

    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program, true, true);

    GLBuffer vertexBuffer;
    setupNonIndexedBuffers(vertexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    if (!useBaseInstanceBuiltin())
    {
        setupInstanceIDBuffer(instanceIDBuffer);
        setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    }

    doMultiDrawArraysInstancedBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false);

    doDrawArraysBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);
}

// Tests basic functionality of glDrawElementsInstancedBaseVertexBaseInstanceANGLE
TEST_P(DrawBaseVertexBaseInstanceTest, DrawElementsInstancedBaseVertexBaseInstance)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program, false);

    GLBuffer indexBuffer;
    GLBuffer vertexBuffer;
    setupIndexedBuffers(vertexBuffer, indexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    if (!useBaseInstanceBuiltin())
    {
        setupInstanceIDBuffer(instanceIDBuffer);
        setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    }

    doDrawElementsInstancedBaseVertexBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true);

    setupRegularIndexedBuffer(indexBuffer);
    doDrawElementsBaseVertexBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true, true);
}

// Tests basic functionality of glMultiDrawElementsInstancedBaseVertexBaseInstance
TEST_P(DrawBaseVertexBaseInstanceTest, MultiDrawElementsInstancedBaseVertexBaseInstance)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program, false, true);

    GLBuffer indexBuffer;
    GLBuffer vertexBuffer;
    setupIndexedBuffers(vertexBuffer, indexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    if (!useBaseInstanceBuiltin())
    {
        setupInstanceIDBuffer(instanceIDBuffer);
        setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    }

    doMultiDrawElementsInstancedBaseVertexBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true);

    setupRegularIndexedBuffer(indexBuffer);
    doDrawElementsBaseVertexBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true, true);
}

// Tests if baseInstance works properly with instanced array with non-zero divisor
TEST_P(DrawBaseInstanceTest, BaseInstanceDivisor)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());
    ANGLE_SKIP_TEST_IF(useBaseInstanceBuiltin());

    GLProgram program;
    setupProgram(program, true, true);

    GLBuffer nonIndexedVertexBuffer;
    setupNonIndexedBuffers(nonIndexedVertexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    GLBuffer instanceColorIDBuffer;

    setupInstanceIDBuffer(instanceIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    setupInstanceColorIDBuffer(instanceColorIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceColorIDLoc);

    doDrawArraysInstancedBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false);

    doDrawArraysBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);

    GLProgram programIndexed;
    setupProgram(programIndexed, false, true);

    GLBuffer indexBuffer;
    GLBuffer vertexBuffer;
    setupIndexedBuffers(vertexBuffer, indexBuffer);
    setupPositionVertexAttribPointer();

    glBindBuffer(GL_ARRAY_BUFFER, instanceIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    glBindBuffer(GL_ARRAY_BUFFER, instanceColorIDBuffer);
    setupInstanceIDVertexAttribPointer(mInstanceColorIDLoc);

    doDrawElementsInstancedBaseVertexBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true);

    setupRegularIndexedBuffer(indexBuffer);
    doDrawElementsBaseVertexBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);
}

// Tests basic functionality of glDrawArraysInstancedBaseInstanceEXT
TEST_P(DrawBaseInstanceTest, DrawArraysInstancedBaseInstance)
{
    // This test fails on certain Win7/Win2008Server AMD GPU
    // with driver version 23.20.185.235 when using OpenGL backend.
    // See comments under the ANGLE test of DrawArraysInstancedBaseInstance above.
    if (IsAMD() && IsWindows() && IsDesktopOpenGL())
    {
        SystemInfo *systemInfo = GetTestSystemInfo();
        if (!systemInfo->gpus.empty())
        {
            ANGLE_SKIP_TEST_IF(0x6613 == systemInfo->gpus[systemInfo->activeGPUIndex].deviceId);
        }
    }

    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program);

    GLBuffer vertexBuffer;
    setupNonIndexedBuffers(vertexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    if (!useBaseInstanceBuiltin())
    {
        setupInstanceIDBuffer(instanceIDBuffer);
        setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    }

    doDrawArraysInstancedBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false);

    doDrawArraysBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);
}

// Tests basic functionality of glDrawElementsInstancedBaseInstanceEXT
TEST_P(DrawBaseInstanceTest, DrawElementsInstancedBaseInstance)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program, false);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mVertices.size(), mVertices.data(),
                 getBufferDataUsage());
    EXPECT_GL_NO_ERROR();
    setupPositionVertexAttribPointer();

    GLBuffer indexBuffer;
    setupRegularIndexedBuffer(indexBuffer);

    GLBuffer instanceIDBuffer;
    if (!useBaseInstanceBuiltin())
    {
        setupInstanceIDBuffer(instanceIDBuffer);
        setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    }

    doDrawElementsInstancedBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false);

    doDrawElementsInstancedBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(false, true);
}

// Tests basic functionality of glDrawElementsInstancedBaseVertexBaseInstanceEXT
TEST_P(DrawBaseInstanceTest, DrawElementsInstancedBaseVertexBaseInstance)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());

    GLProgram program;
    setupProgram(program, false);

    GLBuffer indexBuffer;
    GLBuffer vertexBuffer;
    setupIndexedBuffers(vertexBuffer, indexBuffer);
    setupPositionVertexAttribPointer();

    GLBuffer instanceIDBuffer;
    if (!useBaseInstanceBuiltin())
    {
        setupInstanceIDBuffer(instanceIDBuffer);
        setupInstanceIDVertexAttribPointer(mInstanceIDLoc);
    }

    doDrawElementsInstancedBaseVertexBaseInstance();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true);

    setupRegularIndexedBuffer(indexBuffer);
    doDrawElementsBaseVertexBaseInstanceReset();
    EXPECT_GL_NO_ERROR();
    checkDrawResult(true, true);
}

class DrawBaseVertexBaseInstanceTest_ES3 : public ANGLETest<>
{
  public:
    DrawBaseVertexBaseInstanceTest_ES3()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void runTestNonZeroDivisor(std::function<void(void)> definePositionBuffer,
                               std::function<void(void)> defineColorBuffer,
                               std::function<void(void)> draw)
    {
        constexpr char kVS[] = R"(#version 300 es
precision mediump float;
in vec2 position;
in vec2 colorIn;
out vec2 color;
void main()
{
    gl_Position = vec4(position, 0, 1);
    if ((gl_InstanceID / 2) % 2 == 1)
    {
        gl_Position.y += 1.0;
    }
    if (gl_InstanceID % 2 == 1)
    {
        gl_Position.x += 1.0;
    }
    color = colorIn;
})";

        constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec2 color;
out vec4 colorOut;
void main()
{
    colorOut = vec4(color, 0, 1);
})";

        ANGLE_GL_PROGRAM(program, kVS, kFS);
        glUseProgram(program);

        const GLint posLoc = glGetAttribLocation(program, "position");
        const GLint colLoc = glGetAttribLocation(program, "colorIn");

        // Draw 4 squares.  With a divisor of 2 on color, 2 of them will be one color, two the
        // other.
        GLBuffer position;
        glBindBuffer(GL_ARRAY_BUFFER, position);
        definePositionBuffer();
        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        GLBuffer color;
        glBindBuffer(GL_ARRAY_BUFFER, color);
        defineColorBuffer();
        glEnableVertexAttribArray(colLoc);
        glVertexAttribPointer(colLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glVertexAttribDivisor(colLoc, 2);

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        draw();

        // Verify pixels.  The result looks like the following:
        //
        //      +-----------------------+
        //      |                       |
        //      |   +----+     +----+   |
        //      |   |    |     |    |   |  <-- two red squares
        //      |   |    |     |    |   |
        //      |   +----+     +----+   |
        //      |                       |
        //      |                       |
        //      |   +----+     +----+   |
        //      |   |    |     |    |   |  <-- two green squares
        //      |   |    |     |    |   |
        //      |   +----+     +----+   |
        //      |                       |
        //      +-----------------------+
        //            |
        //            V
        //  Black inside and outside the squares
        //
        const int w = getWindowWidth();
        const int h = getWindowHeight();
        // Don't check too close to the edges to account for precision issues.  This is the margin
        // from the edge.
        const int m = 2;
        const GLColor green(51, 153, 0, 255);

        // Left of squares
        EXPECT_PIXEL_RECT_EQ(0, 0, w / 8 - m, h, GLColor::black);
        // Between squares
        EXPECT_PIXEL_RECT_EQ(w / 2 - w / 8 + m, 0, w / 4 - m * 2, h, GLColor::black);
        // Right of squares
        EXPECT_PIXEL_RECT_EQ(w - w / 8 + m, 0, w / 8 - m, h, GLColor::black);

        // Above squares
        EXPECT_PIXEL_RECT_EQ(0, 0, w, h / 8 - m, GLColor::black);
        // Between squares
        EXPECT_PIXEL_RECT_EQ(0, h / 2 - h / 8 + m, w, h / 4 - m * 2, GLColor::black);
        // Below squares
        EXPECT_PIXEL_RECT_EQ(0, h - h / 8 + m, w, h / 8 - m, GLColor::black);

        // Squares
        EXPECT_PIXEL_RECT_EQ(w / 8 + m, h / 8 + m, w / 4 - 4, h / 4 - 4, GLColor::red);
        EXPECT_PIXEL_RECT_EQ(w / 2 + w / 8 + m, h / 8 + m, w / 4 - 4, h / 4 - 4, GLColor::red);
        EXPECT_PIXEL_RECT_EQ(w / 8 + m, h / 2 + h / 8 + m, w / 4 - 4, h / 4 - 4, green);
        EXPECT_PIXEL_RECT_EQ(w / 2 + w / 8 + m, h / 2 + h / 8 + m, w / 4 - 4, h / 4 - 4, green);
    }
};

// gl_BaseVertex and gl_BaseInstance are translated to angle_BaseVertex and angle_BaseInstance
// internally.  Check that a user-defined angle_BaseVertex or angle_BaseInstance is permitted
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, AllowsUserDefinedANGLEDrawID)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance"));
    ANGLE_SKIP_TEST_IF(
        !EnsureGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance_shader_builtin"));

    constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
in vec2 position;
uniform int angle_BaseVertex;
uniform int angle_BaseInstance;
out vec4 verified;

void main()
{
    // Expect gl_BaseVertex and gl_BaseInstance to be untouched when angle_BaseVertex and
    // angle_BaseInstance are not.
    verified = vec4(gl_BaseVertex == 2, gl_BaseInstance == 0,
                    angle_BaseVertex == 3, angle_BaseInstance == 5);
    gl_Position = vec4(position, 0, 1);
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec4 verified;
out vec4 color;

void main()
{
    color = verified;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "angle_BaseVertex"), 3);
    glUniform1i(glGetUniformLocation(program, "angle_BaseInstance"), 5);

    constexpr std::array<GLfloat, 5 * 2> kVertexData = {
        // Vertex 0, unused
        10000,
        10000,
        // Vertex 1, unused
        10000,
        10000,
        // Vertices 2, 3 and 4 define the triangle because base vertex is 2.
        -1,
        -1,
        3,
        -1,
        -1,
        3,
    };
    constexpr std::array<GLushort, 3> kIndexData = {
        0,
        1,
        2,
    };

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kVertexData), kVertexData.data(), GL_STATIC_DRAW);
    const GLint positionLoc = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(positionLoc);
    glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndexData), kIndexData.data(), GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    glDrawElementsInstancedBaseVertexBaseInstanceANGLE(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, nullptr,
                                                       1, 2, 0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
    ASSERT_GL_NO_ERROR();
}

// Test glDrawArraysInstancedBaseInstance with a non-zero divisor.
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, NonZeroDivisorBaseInstance)
{
    const bool hasEXT   = IsGLExtensionEnabled("GL_EXT_base_instance");
    const bool hasANGLE = IsGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance");
    ANGLE_SKIP_TEST_IF(!hasEXT && !hasANGLE);

    const int w        = getWindowWidth();
    const int h        = getWindowHeight();
    const float left   = static_cast<float>(w / 8) / (w - 1) * 2.0 - 1.0;
    const float right  = static_cast<float>(w / 2 - w / 8) / (w - 1) * 2.0 - 1.0;
    const float top    = static_cast<float>(h / 8) / (h - 1) * 2.0 - 1.0;
    const float bottom = static_cast<float>(h / 2 - h / 8) / (h - 1) * 2.0 - 1.0;

    const GLfloat kPositions[] = {
        // Top left.  Instances 1 and 3 shift this square to the right.  Instances 2 and 3 shift it
        // down.
        left, top, left, bottom, right, top, right, bottom,
    };
    constexpr GLfloat kColors[] = {
        // 11 unused attributes, skipped by base instance
        // clang-format off
        0.1f, 0.2f,
        0.3f, 0.4f,
        0.5f, 0.6f,
        0.7f, 0.8f,
        0.9f, 0.95f,
        0.85f, 0.75f,
        0.65f, 0.55f,
        0.45f, 0.35f,
        0.25f, 0.15f,
        0.05f, 0.15f,
        0.25f, 0.35f,
        // Top
        1.0f, 0.0f,
        // Bottom
        0.2f, 0.6f,
        // clang-format on
    };
    runTestNonZeroDivisor(
        [&kPositions]() {
            glBufferData(GL_ARRAY_BUFFER, sizeof(kPositions), kPositions, GL_STATIC_DRAW);
        },
        [&kColors]() { glBufferData(GL_ARRAY_BUFFER, sizeof(kColors), kColors, GL_STATIC_DRAW); },
        [hasEXT]() {
            if (hasEXT)
            {
                glDrawArraysInstancedBaseInstanceEXT(GL_TRIANGLE_STRIP, 0, 4, 4, 11);
            }
            else
            {
                glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLE_STRIP, 0, 4, 4, 11);
            }
        });
}

// Test glDrawElementsInstancedBaseVertexBaseInstance with a non-zero divisor.
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, NonZeroDivisorBaseVertexBaseInstance)
{
    const bool hasEXT   = IsGLExtensionEnabled("GL_EXT_base_instance");
    const bool hasANGLE = IsGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance");
    ANGLE_SKIP_TEST_IF(!hasEXT && !hasANGLE);

    const int w        = getWindowWidth();
    const int h        = getWindowHeight();
    const float left   = static_cast<float>(w / 8) / (w - 1) * 2.0 - 1.0;
    const float right  = static_cast<float>(w / 2 - w / 8) / (w - 1) * 2.0 - 1.0;
    const float top    = static_cast<float>(h / 8) / (h - 1) * 2.0 - 1.0;
    const float bottom = static_cast<float>(h / 2 - h / 8) / (h - 1) * 2.0 - 1.0;

    const GLfloat kPositions[] = {
        // 5 unused vertices, skipped by base vertex
        // clang-format off
        0.1f, 0.4f,
        0.3f, 0.6f,
        0.5f, 0.8f,
        0.7f, 0.6f,
        0.9f, 0.4f,
        // Top left.  Instances 1 and 3 shift this square to the right.  Instances 2 and 3 shift it
        // down.  The vertices are out of order, but are reordered by the index buffer.
        right, bottom, left, bottom, left, top, right, top,
        // clang-format on
    };
    constexpr GLfloat kColors[] = {
        // 7 unused attributes, skipped by base instance.
        // clang-format off
        0.1f, 0.2f,
        0.3f, 0.4f,
        0.5f, 0.6f,
        0.7f, 0.8f,
        0.9f, 0.95f,
        0.85f, 0.75f,
        0.65f, 0.55f,
        // Top
        1.0f, 0.0f,
        // Bottom
        0.2f, 0.6f,
        // clang-format on
    };
    constexpr GLint kIndices[] = {
        // The square
        2,
        1,
        3,
        0,
    };

    GLBuffer index;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_STATIC_DRAW);

    runTestNonZeroDivisor(
        [&kPositions]() {
            glBufferData(GL_ARRAY_BUFFER, sizeof(kPositions), kPositions, GL_STATIC_DRAW);
        },
        [&kColors]() { glBufferData(GL_ARRAY_BUFFER, sizeof(kColors), kColors, GL_STATIC_DRAW); },
        [hasEXT]() {
            if (hasEXT)
            {
                glDrawElementsInstancedBaseVertexBaseInstanceEXT(GL_TRIANGLE_STRIP, 4,
                                                                 GL_UNSIGNED_INT, nullptr, 4, 5, 7);
            }
            else
            {
                glDrawElementsInstancedBaseVertexBaseInstanceANGLE(
                    GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, nullptr, 4, 5, 7);
            }
        });
}

// Test that gl_InstanceID does not include base instance.
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, InstanceIDDoesNotIncludeBaseInstance)
{
    const bool hasEXT   = IsGLExtensionEnabled("GL_EXT_base_instance");
    const bool hasANGLE = IsGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance");
    ANGLE_SKIP_TEST_IF(!hasEXT && !hasANGLE);

    // Draw 4 triangles to cover the left half of the screen, each triangle is the result of a
    // different instance ID.  If instance ID is larger than expected (because it includes base
    // instance), draw red to the right half of the screen.
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;
out vec2 color;
void main()
{
    // Take the following points:
    //
    //    P0     P1
    //     +------+------+
    //     |    _-|      |
    //     |  _-  |      |
    //     |_-    |      |
    //  P2 +------+ P3   |
    //     |    _-|      |
    //     |  _-  |      |
    //     |_-    |      |
    //     +------+------+
    //    P4     P5
    //
    // Instances generate:
    //
    // * Instance 0: P0, P1, P2
    // * Instance 1: P1, P2, P3
    // * Instance 2: P2, P3, P4
    // * Instance 3: P3, P4, P5
    //
    // So effectively the output position is P[gl_VertexID + gl_InstanceID].
    //
    // To make sure there are no seams, the first vertex is always offset by -(0, epsilon) and the
    // third vertex is offset by (0, epsilon).
    vec2 P[6] = vec2[6](
        vec2(-1.0, -1.0),
        vec2( 0.0, -1.0),
        vec2(-1.0,  0.0),
        vec2( 0.0,  0.0),
        vec2(-1.0,  1.0),
        vec2( 0.0,  1.0)
    );

    if (gl_InstanceID < 4)
    {
        gl_Position = vec4(P[gl_VertexID + gl_InstanceID], 0, 1);
        if (gl_VertexID == 0)
            gl_Position.y -= 0.01;
        else if (gl_VertexID == 2)
            gl_Position.y += 0.01;
        color = vec2(0, 1);
    }
    else
    {
        switch (gl_VertexID) {
            case 0: gl_Position = vec4(0, -2, 0, 1); break;
            case 1: gl_Position = vec4(2, 0, 0, 1); break;
            case 2: gl_Position = vec4(0, 2, 0, 1); break;
        };
        color = vec2(1, 0);
    }
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec2 color;
out vec4 colorOut;
void main()
{
    colorOut = vec4(color, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (hasEXT)
    {
        glDrawArraysInstancedBaseInstanceEXT(GL_TRIANGLE_STRIP, 0, 3, 4, 15);
    }
    else
    {
        glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLE_STRIP, 0, 3, 4, 15);
    }

    // Left half should be all green, right half should be all black.
    const int w = getWindowWidth();
    const int h = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(0, 0, w / 2 - 1, h, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(w / 2 + 1, 0, w - (w / 2 + 1), h, GLColor::black);
}

// Test base instance in combination with divisor
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, BaseInstanceDivisorIndexing)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_base_instance") ||
                       !EnsureGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance"));

    // Vertex shader: pass per-instance integer value through a flat varying.
    constexpr char kVS[] = R"(#version 300 es
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
in vec4 a_position;
in int a_instValue;
flat out int v_instValue;
flat out int v_instanceID;
flat out int v_baseInstance;
void main()
{
    v_instValue = a_instValue;
    v_instanceID = gl_InstanceID;
    v_baseInstance = gl_BaseInstance;
    gl_Position = a_position;
})";

    // Fragment shader: output the instanced value as an integer.
    constexpr char kFS[] = R"(#version 300 es
precision highp int;
flat in int v_instValue;
flat in int v_instanceID;
flat in int v_baseInstance;
out ivec4 o_color;
void main()
{
    o_color = ivec4(v_instValue, v_instanceID, v_baseInstance, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    GLint posLoc  = glGetAttribLocation(program, "a_position");
    GLint instLoc = glGetAttribLocation(program, "a_instValue");
    ASSERT_NE(-1, posLoc);
    ASSERT_NE(-1, instLoc);

    // Render to a GL_RGBA32I texture via FBO for exact integer readback.
    GLTexture intTex;
    glBindTexture(GL_TEXTURE_2D, intTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, getWindowWidth(), getWindowHeight(), 0,
                 GL_RGBA_INTEGER, GL_INT, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, intTex, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Position: a full-viewport triangle strip (4 vertices).
    const float positions[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
    };
    GLBuffer posBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Per-instance attribute: 8 distinct integer values, divisor = 3.
    // Each value is a unique identifier for its buffer index.
    const GLint instValues[] = {0, 1, 2, 3, 4, 5, 6, 7};
    GLBuffer instBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, instBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(instValues), instValues, GL_STATIC_DRAW);
    glEnableVertexAttribArray(instLoc);
    glVertexAttribIPointer(instLoc, 1, GL_INT, 0, nullptr);
    glVertexAttribDivisor(instLoc, 3);
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    struct Subcase
    {
        GLsizei instanceCount;
        GLuint baseInstance;
    } subcases[]{
        {.instanceCount = 1, .baseInstance = 0},
        {.instanceCount = 1, .baseInstance = 4},
        {.instanceCount = 1, .baseInstance = 7},
        {.instanceCount = 6, .baseInstance = 3},
    };

    for (auto const &subcase : subcases)
    {
        SCOPED_TRACE(testing::Message()
                     << "Subcase subcase{.instanceCount = " << subcase.instanceCount
                     << ", .baseInstance = " << subcase.baseInstance << "}");
        const GLint clearValue[] = {0, 0, 0, 0};
        glClearBufferiv(GL_COLOR, 0, clearValue);
        glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLE_STRIP, 0, 4, subcase.instanceCount,
                                               subcase.baseInstance);
        ASSERT_GL_NO_ERROR();
        GLint lastInstanceAttrIndex = subcase.baseInstance + (subcase.instanceCount - 1) / 3;
        // SAFETY: Values chosen in the test cases, max value within 0..7.
        GLint lastInstValue  = ANGLE_UNSAFE_BUFFERS(instValues[lastInstanceAttrIndex]);
        GLint lastInstanceID = subcase.instanceCount - 1;
        // R == instValue (per-instance attribute), G == gl_InstanceID, B == gl_BaseInstance.
        GLColor32I expected(lastInstValue, lastInstanceID, static_cast<GLint>(subcase.baseInstance),
                            1);
        EXPECT_PIXEL_32I_COLOR(0, 0, expected);
    }
}

// Test base instance with divisor >= 256
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, BaseInstanceLargeDivisor)
{
    const bool hasEXT   = IsGLExtensionEnabled("GL_EXT_base_instance");
    const bool hasANGLE = IsGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance");
    ANGLE_SKIP_TEST_IF(!hasEXT && !hasANGLE);

    constexpr uint32_t kBaseInstance = 3;
    constexpr uint32_t kRegionCount  = 4;
    constexpr uint32_t kDivisor      = 256;

    // Create an attribute that will be instanced.
    const std::array<GLColor, kBaseInstance + kRegionCount> kData = {
        GLColor(10, 20, 30, 40), GLColor(11, 21, 31, 41), GLColor(12, 22, 32, 42), GLColor::red,
        GLColor::green,          GLColor::blue,           GLColor::yellow,
    };

    GLBuffer instancedData;
    glBindBuffer(GL_ARRAY_BUFFER, instancedData);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kData), kData.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_UNSIGNED_BYTE, true, 4, nullptr);
    glVertexAttribDivisor(0, kDivisor);

    // The divisor of the data should be high (256) to trigger a specific path in the Vulkan
    // backend.  The test renders 256*4 instances with a base instance of 3.  To simplify
    // validation, the output is divided into four corners, where every 256 instances overdraw the
    // same corner.
    //
    //                      P0
    //                      +
    //                    / | \
    //                  /   |   \
    //                /     |     \
    //              /       |       \
    //             +--------+--------+
    //           / |        |        | \
    //         /   |        |        |   \
    //       /     |        | O      |     \
    //   P1 +------+--------+--------+------+ P3
    //       \     |        |        |     /
    //         \   |        |        |   /
    //           \ |        |        | /
    //             +--------+--------+
    //              \       |       /
    //                \     |     /
    //                  \   |   /
    //                    \ | /
    //                      +
    //                     P2
    //
    // Instances generate:
    //
    // * Instances [0 * divisor, 1 * divisor): O, P0, P1
    // * Instances [1 * divisor, 2 * divisor): O, P1, P2
    // * Instances [2 * divisor, 3 * divisor): O, P2, P3
    // * Instances [3 * divisor, 4 * divisor): O, P3, P0
    //
    // So the vertices are always as follows, given i = gl_InstanceID / divisor:
    //
    // * gl_VertexID 0: P[0]
    // * gl_VertexID 1: P[i]
    // * gl_VertexID 2: P[(i + 1) % 4]
    //
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;
in vec4 instanceData;
out vec4 color;
void main()
{
    vec2 P[4] = vec2[4](
        vec2( 0.0, -2.0),
        vec2(-2.0,  0.0),
        vec2( 0.0,  2.0),
        vec2( 2.0,  0.0)
    );

    int index = gl_InstanceID / 256;
    switch (gl_VertexID % 3)
    {
    case 0:
        gl_Position = vec4(0, 0, 0, 1);
        break;
    case 1:
        gl_Position = vec4(P[index], 0, 1);
        break;
    default:
        gl_Position = vec4(P[(index + 1) % 4], 0, 1);
        break;
    }

    color = instanceData;
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec4 color;
out vec4 colorOut;
void main()
{
    colorOut = color;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (hasEXT)
    {
        glDrawArraysInstancedBaseInstanceEXT(GL_TRIANGLE_STRIP, 0, 3, kDivisor * kRegionCount,
                                             kBaseInstance);
    }
    else
    {
        glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLE_STRIP, 0, 3, kDivisor * kRegionCount,
                                               kBaseInstance);
    }

    const int w = getWindowWidth();
    const int h = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(0, 0, w / 2 - 1, h / 2 - 1, kData[kBaseInstance]);
    EXPECT_PIXEL_RECT_EQ(0, h / 2 + 1, w / 2 - 1, h - (h / 2 + 1), kData[kBaseInstance + 1]);
    EXPECT_PIXEL_RECT_EQ(w / 2 + 1, h / 2 + 1, w - (w / 2 + 1), h - (h / 2 + 1),
                         kData[kBaseInstance + 2]);
    EXPECT_PIXEL_RECT_EQ(w / 2 + 1, 0, w - (w / 2 + 1), h / 2, kData[kBaseInstance + 3]);
}

// Test base instance with divisor < 256 where vertex attribute data is sourced from client memory.
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, BaseInstanceSmallDivisorClientMemory)
{
    const bool hasEXT   = IsGLExtensionEnabled("GL_EXT_base_instance");
    const bool hasANGLE = IsGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance");
    ANGLE_SKIP_TEST_IF(!hasEXT && !hasANGLE);

    constexpr uint32_t kBaseInstance = 3;
    constexpr uint32_t kRegionCount  = 4;
    constexpr uint32_t kDivisor      = 11;

    // Create an attribute that will be instanced.
    const std::array<GLColor, kBaseInstance + kRegionCount> kData = {
        GLColor(10, 20, 30, 40), GLColor(11, 21, 31, 41), GLColor(12, 22, 32, 42), GLColor::red,
        GLColor::green,          GLColor::blue,           GLColor::yellow,
    };

    // Data needs to be sourced from a client buffer to trigger a specific path in the Vulkan
    // backend.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_UNSIGNED_BYTE, true, 4, kData.data());
    glVertexAttribDivisor(0, kDivisor);

    // The divisor of the data should be low (<256) to trigger a specific path in the Vulkan
    // backend.  The test renders 11*4 instances with a base instance of 3.  To simplify
    // validation, the output is divided as such:
    //
    //                      P0
    //                      +
    //                    / | \
    //                  /   |   \
    //                /     |     \
    //              /       |       \
    //             +--------+--------+
    //           / |        |        | \
    //         /   |        |        |   \
    //       /     |        | O      |     \
    //   P1 +------+--------+--------+------+ P3
    //       \     |        |        |     /
    //         \   |        |        |   /
    //           \ |        |        | /
    //             +--------+--------+
    //              \       |       /
    //                \     |     /
    //                  \   |   /
    //                    \ | /
    //                      +
    //                     P2
    //
    // Instances generate:
    //
    // * Instances [0 * divisor, 1 * divisor): O, P0, P1
    // * Instances [1 * divisor, 2 * divisor): O, P1, P2
    // * Instances [2 * divisor, 3 * divisor): O, P2, P3
    // * Instances [3 * divisor, 4 * divisor): O, P3, P0
    //
    // So the vertices are always as follows, given i = gl_InstanceID / divisor:
    //
    // * gl_VertexID 0: P[0]
    // * gl_VertexID 1: P[i]
    // * gl_VertexID 2: P[(i + 1) % 4]
    //
    constexpr char kVS[] = R"(#version 300 es
precision mediump float;
in vec4 instanceData;
out vec4 color;
void main()
{
    vec2 P[4] = vec2[4](
        vec2( 0.0, -2.0),
        vec2(-2.0,  0.0),
        vec2( 0.0,  2.0),
        vec2( 2.0,  0.0)
    );

    int index = gl_InstanceID / 11;
    switch (gl_VertexID % 3)
    {
    case 0:
        gl_Position = vec4(0, 0, 0, 1);
        break;
    case 1:
        gl_Position = vec4(P[index], 0, 1);
        break;
    default:
        gl_Position = vec4(P[(index + 1) % 4], 0, 1);
        break;
    }

    color = instanceData;
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec4 color;
out vec4 colorOut;
void main()
{
    colorOut = color;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (hasEXT)
    {
        glDrawArraysInstancedBaseInstanceEXT(GL_TRIANGLE_STRIP, 0, 3, kDivisor * kRegionCount,
                                             kBaseInstance);
    }
    else
    {
        glDrawArraysInstancedBaseInstanceANGLE(GL_TRIANGLE_STRIP, 0, 3, kDivisor * kRegionCount,
                                               kBaseInstance);
    }

    const int w = getWindowWidth();
    const int h = getWindowHeight();
    EXPECT_PIXEL_RECT_EQ(0, 0, w / 2 - 1, h / 2 - 1, kData[kBaseInstance]);
    EXPECT_PIXEL_RECT_EQ(0, h / 2 + 1, w / 2 - 1, h - (h / 2 + 1), kData[kBaseInstance + 1]);
    EXPECT_PIXEL_RECT_EQ(w / 2 + 1, h / 2 + 1, w - (w / 2 + 1), h - (h / 2 + 1),
                         kData[kBaseInstance + 2]);
    EXPECT_PIXEL_RECT_EQ(w / 2 + 1, 0, w - (w / 2 + 1), h / 2, kData[kBaseInstance + 3]);
}

// Test that drawing with baseInstance and divisor where (instances mod divisor != 0) and
// (baseInstance mod divisor != 0) works.
TEST_P(DrawBaseVertexBaseInstanceTest_ES3, DivisorDoesNotDivideBaseInstance)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_base_vertex_base_instance"));

    // The test divides the screen horizontally into 4 columns/strips for the first 4 instances
    // Draw within viewport only for instances 0, 1, 2, 3, other instances are discarded.
    // Each of the 4 instances is drawn as a vertical strip in the viewport.
    constexpr char kVS[] = R"(#version 300 es
in vec4 a_position;
in vec4 a_color;
out vec4 v_color;
void main()
{
    if (gl_InstanceID >= 4)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    }
    else
    {
        gl_Position = a_position;
        gl_Position.x = (float(gl_InstanceID) * 0.5) - 0.75 + a_position.x * 0.25;
    }
    v_color = a_color;
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
in vec4 v_color;
out vec4 o_color;
void main()
{
    o_color = v_color;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    GLint posLoc = glGetAttribLocation(program, "a_position");
    GLint colLoc = glGetAttribLocation(program, "a_color");
    ASSERT_NE(-1, posLoc);
    ASSERT_NE(-1, colLoc);

    const float positions[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
    };
    GLBuffer posBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Use GL_FLOAT 4-components (16 bytes per element).
    // Both kInstances and kBaseInstance are chosen to be odd numbers so
    // they are not multiples of the divisor
    // kInstances is kept small to prevent tsan timeouts on slow software renderers (SwiftShader).
    // kBaseInstance is chosen to be large to force the D3D11 backend to resize the buffer,
    constexpr GLsizei kInstances   = 5;
    constexpr GLuint kBaseInstance = 131071;
    constexpr GLuint kDivisor      = 2;
    constexpr size_t kCopyCount =
        kBaseInstance + rx::UnsignedCeilDivide(static_cast<unsigned int>(kInstances), kDivisor);
    constexpr size_t kComponents = 4;

    std::vector<float> colors(kCopyCount * kComponents);
    for (size_t i = 0; i < kCopyCount; ++i)
    {
        colors[i * kComponents + 0] = 1.0f;  // R
        colors[i * kComponents + 1] = 1.0f;  // G
        colors[i * kComponents + 2] = 1.0f;  // B
        colors[i * kComponents + 3] = 1.0f;  // A
    }
    // Index kBaseInstance: Yellow
    colors[kBaseInstance * kComponents + 0] = 1.0f;
    colors[kBaseInstance * kComponents + 1] = 1.0f;
    colors[kBaseInstance * kComponents + 2] = 0.0f;
    colors[kBaseInstance * kComponents + 3] = 1.0f;
    // Index kBaseInstance + 1: Cyan
    colors[(kBaseInstance + 1) * kComponents + 0] = 0.0f;
    colors[(kBaseInstance + 1) * kComponents + 1] = 1.0f;
    colors[(kBaseInstance + 1) * kComponents + 2] = 1.0f;
    colors[(kBaseInstance + 1) * kComponents + 3] = 1.0f;

    GLBuffer colBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, colBuffer);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(colLoc);
    glVertexAttribPointer(colLoc, kComponents, GL_FLOAT, GL_FALSE, 0, nullptr);
    glVertexAttribDivisor(colLoc, kDivisor);

    const GLushort indices[] = {0, 1, 2, 1, 2, 3};
    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawElementsInstancedBaseVertexBaseInstanceANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr,
                                                       kInstances, 0, kBaseInstance);
    ASSERT_GL_NO_ERROR();

    const int w        = getWindowWidth();
    const int h        = getWindowHeight();
    const int y        = h / 2;
    const int colWidth = w / 8;

    // Verify each of the 4 columns
    // Column 0 (Instance 0): index = 3 + 0/2 = 3 (Yellow)
    EXPECT_PIXEL_COLOR_EQ(colWidth, y, GLColor::yellow);
    // Column 1 (Instance 1): index = 3 + 1/2 = 3 (Yellow)
    EXPECT_PIXEL_COLOR_EQ(3 * colWidth, y, GLColor::yellow);
    // Column 2 (Instance 2): index = 3 + 2/2 = 4 (Cyan)
    EXPECT_PIXEL_COLOR_EQ(5 * colWidth, y, GLColor::cyan);
    // Column 3 (Instance 3): index = 3 + 3/2 = 4 (Cyan)
    EXPECT_PIXEL_COLOR_EQ(7 * colWidth, y, GLColor::cyan);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DrawBaseVertexBaseInstanceTest);

#define ANGLE_ALL_BASEVERTEXBASEINTANCE_TEST_PLATFORMS_ES3                                 \
    ES3_D3D11().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),                  \
        ES3_OPENGL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),             \
        ES3_OPENGLES().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),           \
        ES3_VULKAN().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions),             \
        ES3_VULKAN().disable(Feature::SupportsVertexInputDynamicState),                    \
        ES3_VULKAN_SWIFTSHADER().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions), \
        ES3_METAL().enable(Feature::AlwaysEnableEmulatedMultidrawExtensions)

ANGLE_INSTANTIATE_TEST_COMBINE_3(
    DrawBaseVertexBaseInstanceTest,
    PrintToStringParamName(),
    testing::Values(BaseVertexOption::NoBaseVertex, BaseVertexOption::UseBaseVertex),
    testing::Values(BaseInstanceOption::NoBaseInstance, BaseInstanceOption::UseBaseInstance),
    testing::Values(BufferDataUsageOption::StaticDraw, BufferDataUsageOption::DynamicDraw),
    ANGLE_ALL_BASEVERTEXBASEINTANCE_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_3(
    DrawBaseInstanceTest,
    PrintToStringParamName(),
    testing::Values(BaseVertexOption::NoBaseVertex, BaseVertexOption::UseBaseVertex),
    testing::Values(BaseInstanceOption::NoBaseInstance, BaseInstanceOption::UseBaseInstance),
    testing::Values(BufferDataUsageOption::StaticDraw, BufferDataUsageOption::DynamicDraw),
    ANGLE_ALL_BASEVERTEXBASEINTANCE_TEST_PLATFORMS_ES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DrawBaseVertexBaseInstanceTest_ES3);
ANGLE_INSTANTIATE_TEST(DrawBaseVertexBaseInstanceTest_ES3,
                       ANGLE_ALL_BASEVERTEXBASEINTANCE_TEST_PLATFORMS_ES3);

}  // namespace
