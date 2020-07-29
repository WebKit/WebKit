//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// MultiDrawTest: Tests of GL_ANGLE_multi_draw

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

// Create a kWidth * kHeight canvas equally split into kCountX * kCountY tiles
// each containing a quad partially coverting each tile
constexpr uint32_t kWidth                  = 256;
constexpr uint32_t kHeight                 = 256;
constexpr uint32_t kCountX                 = 8;
constexpr uint32_t kCountY                 = 8;
constexpr uint32_t kQuadCount              = kCountX * kCountY;
constexpr uint32_t kTriCount               = kQuadCount * 2;
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

enum class DrawIDOption
{
    NoDrawID,
    UseDrawID,
};

enum class InstancingOption
{
    NoInstancing,
    UseInstancing,
};

enum class BufferDataUsageOption
{
    StaticDraw,
    DynamicDraw
};

using MultiDrawTestParams =
    std::tuple<angle::PlatformParameters, DrawIDOption, InstancingOption, BufferDataUsageOption>;

struct PrintToStringParamName
{
    std::string operator()(const ::testing::TestParamInfo<MultiDrawTestParams> &info) const
    {
        ::std::stringstream ss;
        ss << std::get<0>(info.param)
           << (std::get<3>(info.param) == BufferDataUsageOption::StaticDraw ? "__StaticDraw"
                                                                            : "__DynamicDraw")
           << (std::get<2>(info.param) == InstancingOption::UseInstancing ? "__Instanced" : "")
           << (std::get<1>(info.param) == DrawIDOption::UseDrawID ? "__DrawID" : "");
        return ss.str();
    }
};

// These tests check correctness of the ANGLE_multi_draw extension.
// An array of quads is drawn across the screen.
// gl_DrawID is checked by using it to select the color of the draw.
// MultiDraw*Instanced entrypoints use the existing instancing APIs which are
// more fully tested in InstancingTest.cpp.
// Correct interaction with the instancing APIs is tested here by using scaling
// and then instancing the array of quads over four quadrants on the screen.
class MultiDrawTest : public ANGLETestBase, public ::testing::TestWithParam<MultiDrawTestParams>
{
  protected:
    MultiDrawTest()
        : ANGLETestBase(std::get<0>(GetParam())),
          mNonIndexedVertexBuffer(0u),
          mVertexBuffer(0u),
          mIndexBuffer(0u),
          mInstanceBuffer(0u),
          mProgram(0u)
    {
        setWindowWidth(kWidth);
        setWindowHeight(kHeight);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override { ANGLETestBase::ANGLETestSetUp(); }

    bool IsDrawIDTest() const { return std::get<1>(GetParam()) == DrawIDOption::UseDrawID; }

    bool IsInstancedTest() const
    {
        return std::get<2>(GetParam()) == InstancingOption::UseInstancing;
    }

    GLenum getBufferDataUsage() const
    {
        return std::get<3>(GetParam()) == BufferDataUsageOption::StaticDraw ? GL_STATIC_DRAW
                                                                            : GL_DYNAMIC_DRAW;
    }

    std::string VertexShaderSource()
    {

        std::stringstream shader;
        shader << (IsDrawIDTest() ? "#extension GL_ANGLE_multi_draw : require\n" : "")
               << (IsInstancedTest() ? "attribute float vInstance;" : "") << R"(
attribute vec2 vPosition;
varying vec4 color;
void main()
{
    int id = )" << (IsDrawIDTest() ? "gl_DrawID" : "0")
               << ";"
               << R"(
    float quad_id = float(id / 2);
    float color_id = quad_id - (3.0 * floor(quad_id / 3.0));
    if (color_id == 0.0) {
      color = vec4(1, 0, 0, 1);
    } else if (color_id == 1.0) {
      color = vec4(0, 1, 0, 1);
    } else {
      color = vec4(0, 0, 1, 1);
    }

    mat3 transform = mat3(1.0);
)"
               << (IsInstancedTest() ? R"(
    transform[0][0] = 0.5;
    transform[1][1] = 0.5;
    if (vInstance == 0.0) {

    } else if (vInstance == 1.0) {
        transform[2][0] = 0.5;
    } else if (vInstance == 2.0) {
        transform[2][1] = 0.5;
    } else if (vInstance == 3.0) {
        transform[2][0] = 0.5;
        transform[2][1] = 0.5;
    }
)"
                                     : "")
               << R"(
    gl_Position = vec4(transform * vec3(vPosition, 1.0) * 2.0 - 1.0, 1);
})";

        return shader.str();
    }

    std::string FragmentShaderSource()
    {
        return
            R"(precision mediump float;
            varying vec4 color;
            void main()
            {
                gl_FragColor = color;
            })";
    }

    void SetupProgram()
    {
        mProgram = CompileProgram(VertexShaderSource().c_str(), FragmentShaderSource().c_str());
        EXPECT_GL_NO_ERROR();
        ASSERT_GE(mProgram, 1u);
        glUseProgram(mProgram);
        mPositionLoc = glGetAttribLocation(mProgram, "vPosition");
        mInstanceLoc = glGetAttribLocation(mProgram, "vInstance");
    }

    void SetupBuffers()
    {
        for (uint32_t y = 0; y < kCountY; ++y)
        {
            for (uint32_t x = 0; x < kCountX; ++x)
            {
                // v3 ---- v2
                // |       |
                // |       |
                // v0 ---- v1
                uint32_t quadIndex         = y * kCountX + x;
                GLushort starting_index    = static_cast<GLushort>(4 * quadIndex);
                std::array<GLushort, 6> is = {0, 1, 2, 0, 2, 3};
                const auto vs              = getQuadVertices(x, y);
                for (GLushort i : is)
                {
                    mIndices.push_back(starting_index + i);
                }

                for (const auto &v : vs)
                {
                    mVertices.insert(mVertices.end(), v.begin(), v.end());
                }

                for (GLushort i : is)
                {
                    mNonIndexedVertices.insert(mNonIndexedVertices.end(), vs[i].begin(),
                                               vs[i].end());
                }
            }
        }

        std::array<GLfloat, 4> instances{0, 1, 2, 3};

        glGenBuffers(1, &mNonIndexedVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mNonIndexedVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mNonIndexedVertices.size(),
                     mNonIndexedVertices.data(), getBufferDataUsage());

        glGenBuffers(1, &mVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * mVertices.size(), mVertices.data(),
                     getBufferDataUsage());

        glGenBuffers(1, &mIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * mIndices.size(), mIndices.data(),
                     getBufferDataUsage());

        glGenBuffers(1, &mInstanceBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mInstanceBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * instances.size(), instances.data(),
                     getBufferDataUsage());

        ASSERT_GL_NO_ERROR();
    }

    void DoVertexAttribDivisor(GLint location, GLuint divisor)
    {
        if (getClientMajorVersion() <= 2)
        {
            ASSERT_TRUE(IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));
            glVertexAttribDivisorANGLE(location, divisor);
        }
        else
        {
            glVertexAttribDivisor(location, divisor);
        }
    }

    void DoDrawArrays()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindBuffer(GL_ARRAY_BUFFER, mNonIndexedVertexBuffer);
        glEnableVertexAttribArray(mPositionLoc);
        glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);

        std::vector<GLint> firsts(kTriCount);
        std::vector<GLsizei> counts(kTriCount, 3);
        for (uint32_t i = 0; i < kTriCount; ++i)
            firsts[i] = i * 3;

        if (IsInstancedTest())
        {
            glBindBuffer(GL_ARRAY_BUFFER, mInstanceBuffer);
            glEnableVertexAttribArray(mInstanceLoc);
            glVertexAttribPointer(mInstanceLoc, 1, GL_FLOAT, GL_FALSE, 0, 0);
            DoVertexAttribDivisor(mInstanceLoc, 1);
            std::vector<GLsizei> instanceCounts(kTriCount, 4);
            glMultiDrawArraysInstancedANGLE(GL_TRIANGLES, firsts.data(), counts.data(),
                                            instanceCounts.data(), kTriCount);
        }
        else
        {
            glMultiDrawArraysANGLE(GL_TRIANGLES, firsts.data(), counts.data(), kTriCount);
        }
    }

    void DoDrawElements()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
        glEnableVertexAttribArray(mPositionLoc);
        glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);

        std::vector<GLsizei> counts(kTriCount, 3);
        std::vector<const GLvoid *> indices(kTriCount);
        for (uint32_t i = 0; i < kTriCount; ++i)
            indices[i] = reinterpret_cast<GLvoid *>(static_cast<uintptr_t>(i * 3 * 2));

        if (IsInstancedTest())
        {
            glBindBuffer(GL_ARRAY_BUFFER, mInstanceBuffer);
            glEnableVertexAttribArray(mInstanceLoc);
            glVertexAttribPointer(mInstanceLoc, 1, GL_FLOAT, GL_FALSE, 0, 0);
            DoVertexAttribDivisor(mInstanceLoc, 1);
            std::vector<GLsizei> instanceCounts(kTriCount, 4);
            glMultiDrawElementsInstancedANGLE(GL_TRIANGLES, counts.data(), GL_UNSIGNED_SHORT,
                                              indices.data(), instanceCounts.data(), kTriCount);
        }
        else
        {
            glMultiDrawElementsANGLE(GL_TRIANGLES, counts.data(), GL_UNSIGNED_SHORT, indices.data(),
                                     kTriCount);
        }
    }

    void CheckDrawResult()
    {
        for (uint32_t y = 0; y < kCountY; ++y)
        {
            for (uint32_t x = 0; x < kCountX; ++x)
            {
                uint32_t center_x             = x * kTilePixelSize[0] + kTilePixelSize[0] / 2;
                uint32_t center_y             = y * kTilePixelSize[1] + kTilePixelSize[1] / 2;
                uint32_t quadID               = IsDrawIDTest() ? y * kCountX + x : 0;
                uint32_t colorID              = quadID % 3u;
                std::array<GLColor, 3> colors = {GLColor(255, 0, 0, 255), GLColor(0, 255, 0, 255),
                                                 GLColor(0, 0, 255, 255)};
                GLColor expected              = colors[colorID];

                if (IsInstancedTest())
                {
                    EXPECT_PIXEL_RECT_EQ(center_x / 2 - kPixelCheckSize[0] / 4,
                                         center_y / 2 - kPixelCheckSize[1] / 4,
                                         kPixelCheckSize[0] / 2, kPixelCheckSize[1] / 2, expected);
                    EXPECT_PIXEL_RECT_EQ(center_x / 2 - kPixelCheckSize[0] / 4 + kWidth / 2,
                                         center_y / 2 - kPixelCheckSize[1] / 4,
                                         kPixelCheckSize[0] / 2, kPixelCheckSize[1] / 2, expected);
                    EXPECT_PIXEL_RECT_EQ(center_x / 2 - kPixelCheckSize[0] / 4,
                                         center_y / 2 - kPixelCheckSize[1] / 4 + kHeight / 2,
                                         kPixelCheckSize[0] / 2, kPixelCheckSize[1] / 2, expected);
                    EXPECT_PIXEL_RECT_EQ(center_x / 2 - kPixelCheckSize[0] / 4 + kWidth / 2,
                                         center_y / 2 - kPixelCheckSize[1] / 4 + kHeight / 2,
                                         kPixelCheckSize[0] / 2, kPixelCheckSize[1] / 2, expected);
                }
                else
                {
                    EXPECT_PIXEL_RECT_EQ(center_x - kPixelCheckSize[0] / 2,
                                         center_y - kPixelCheckSize[1] / 2, kPixelCheckSize[0],
                                         kPixelCheckSize[1], expected);
                }
            }
        }
    }

    void TearDown() override
    {
        if (mNonIndexedVertexBuffer != 0u)
        {
            glDeleteBuffers(1, &mNonIndexedVertexBuffer);
        }
        if (mVertexBuffer != 0u)
        {
            glDeleteBuffers(1, &mVertexBuffer);
        }
        if (mIndexBuffer != 0u)
        {
            glDeleteBuffers(1, &mIndexBuffer);
        }
        if (mInstanceBuffer != 0u)
        {
            glDeleteBuffers(1, &mInstanceBuffer);
        }
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
        }
        ANGLETestBase::ANGLETestTearDown();
    }

    bool requestMultiDrawExtension()
    {
        if (IsGLExtensionRequestable("GL_ANGLE_multi_draw"))
        {
            glRequestExtensionANGLE("GL_ANGLE_multi_draw");
        }

        if (!IsGLExtensionEnabled("GL_ANGLE_multi_draw"))
        {
            return false;
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
        if (IsInstancedTest() && getClientMajorVersion() <= 2)
        {
            if (!requestInstancedExtension())
            {
                return false;
            }
        }
        return requestMultiDrawExtension();
    }

    std::vector<GLushort> mIndices;
    std::vector<GLfloat> mVertices;
    std::vector<GLfloat> mNonIndexedVertices;
    GLuint mNonIndexedVertexBuffer;
    GLuint mVertexBuffer;
    GLuint mIndexBuffer;
    GLuint mInstanceBuffer;
    GLuint mProgram;
    GLint mPositionLoc;
    GLint mInstanceLoc;
};

class MultiDrawNoInstancingSupportTest : public MultiDrawTest
{
    void SetUp() override
    {
        ASSERT_LE(getClientMajorVersion(), 2);
        ASSERT_TRUE(IsInstancedTest());
        MultiDrawTest::SetUp();
    }
};

// glMultiDraw*ANGLE are emulated and should always be available
TEST_P(MultiDrawTest, RequestExtension)
{
    EXPECT_TRUE(requestMultiDrawExtension());
}

// Test that compile a program with the extension succeeds
TEST_P(MultiDrawTest, CanCompile)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());
    SetupProgram();
}

// Tests basic functionality of glMultiDrawArraysANGLE
TEST_P(MultiDrawTest, MultiDrawArrays)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());
    SetupBuffers();
    SetupProgram();
    DoDrawArrays();
    EXPECT_GL_NO_ERROR();
    CheckDrawResult();
}

// Tests basic functionality of glMultiDrawElementsANGLE
TEST_P(MultiDrawTest, MultiDrawElements)
{
    ANGLE_SKIP_TEST_IF(!requestExtensions());
    SetupBuffers();
    SetupProgram();
    DoDrawElements();
    EXPECT_GL_NO_ERROR();
    CheckDrawResult();
}

// Check that glMultiDraw*Instanced without instancing support results in GL_INVALID_OPERATION
TEST_P(MultiDrawNoInstancingSupportTest, InvalidOperation)
{
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));
    requestMultiDrawExtension();
    SetupBuffers();
    SetupProgram();

    GLint first       = 0;
    GLsizei count     = 3;
    GLvoid *indices   = 0;
    GLsizei instances = 1;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindBuffer(GL_ARRAY_BUFFER, mNonIndexedVertexBuffer);
    glEnableVertexAttribArray(mPositionLoc);
    glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glMultiDrawArraysInstancedANGLE(GL_TRIANGLES, &first, &count, &instances, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glEnableVertexAttribArray(mPositionLoc);
    glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glMultiDrawElementsInstancedANGLE(GL_TRIANGLES, &count, GL_UNSIGNED_SHORT, &indices, &instances,
                                      1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

const angle::PlatformParameters platforms[] = {
    ES2_D3D9(),  ES2_OPENGL(), ES2_OPENGLES(), ES2_VULKAN(),
    ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES(),
};

const angle::PlatformParameters es2_platforms[] = {
    ES2_D3D9(),
    ES2_OPENGL(),
    ES2_OPENGLES(),
    ES2_VULKAN(),
};

INSTANTIATE_TEST_SUITE_P(
    ,
    MultiDrawTest,
    testing::Combine(
        testing::ValuesIn(::angle::FilterTestParams(platforms, ArraySize(platforms))),
        testing::Values(DrawIDOption::NoDrawID, DrawIDOption::UseDrawID),
        testing::Values(InstancingOption::NoInstancing, InstancingOption::UseInstancing),
        testing::Values(BufferDataUsageOption::StaticDraw, BufferDataUsageOption::DynamicDraw)),
    PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(
    ,
    MultiDrawNoInstancingSupportTest,
    testing::Combine(
        testing::ValuesIn(::angle::FilterTestParams(es2_platforms, ArraySize(es2_platforms))),
        testing::Values(DrawIDOption::NoDrawID, DrawIDOption::UseDrawID),
        testing::Values(InstancingOption::UseInstancing),
        testing::Values(BufferDataUsageOption::StaticDraw, BufferDataUsageOption::DynamicDraw)),
    PrintToStringParamName());

}  // namespace
