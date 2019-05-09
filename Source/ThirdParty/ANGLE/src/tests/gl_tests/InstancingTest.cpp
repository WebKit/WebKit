//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
enum Geometry
{
    Quad,
    Point
};
enum Storage
{
    Buffer,
    Memory
};
enum Draw
{
    Indexed,
    NonIndexed
};
enum Vendor
{
    Angle,
    Ext
};
}  // namespace

class InstancingTest : public ANGLETest
{
  protected:
    InstancingTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void TearDown() override
    {
        glDeleteBuffers(1, &mInstanceBuffer);
        glDeleteProgram(mProgram[0]);
        glDeleteProgram(mProgram[1]);

        ANGLETest::TearDown();
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        for (unsigned i = 0; i < kMaxDrawn; ++i)
        {
            mInstanceData[i] = i * kDrawSize;
        }
        glGenBuffers(1, &mInstanceBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mInstanceBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(mInstanceData), mInstanceData, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        const std::string inst = "attribute float a_instance;";
        const std::string pos  = "attribute vec2 a_position;";
        const std::string main = R"(
            void main()
            {
                gl_PointSize = 6.0;
                gl_Position = vec4(a_position.x, a_position.y + a_instance, 0, 1);
            }
        )";

        // attrib 0 is instanced
        const std::string inst0 = inst + pos + main;
        mProgram[0]             = CompileProgram(inst0.c_str(), essl1_shaders::fs::Red());
        ASSERT_NE(0u, mProgram[0]);
        ASSERT_EQ(0, glGetAttribLocation(mProgram[0], "a_instance"));
        ASSERT_EQ(1, glGetAttribLocation(mProgram[0], "a_position"));

        // attrib 1 is instanced
        const std::string inst1 = pos + inst + main;
        mProgram[1]             = CompileProgram(inst1.c_str(), essl1_shaders::fs::Red());
        ASSERT_NE(0u, mProgram[1]);
        ASSERT_EQ(1, glGetAttribLocation(mProgram[1], "a_instance"));
        ASSERT_EQ(0, glGetAttribLocation(mProgram[1], "a_position"));

        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    }

    void runTest(unsigned numInstance,
                 unsigned divisor,
                 const int instanceAttrib,  // which attrib is instanced: 0 or 1
                 Geometry geometry,
                 Draw draw,
                 Storage storage,
                 Vendor vendor,
                 unsigned offset)  // for NonIndexed/DrawArrays only
    {
        if (vendor == Angle)
        {
            ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));
        }
        else if (vendor == Ext)
        {
            ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_instanced_arrays"));
        }

        // TODO: Fix these.  http://anglebug.com/3129
        ANGLE_SKIP_TEST_IF(IsD3D9() && draw == Indexed && geometry == Point);
        ANGLE_SKIP_TEST_IF(IsD3D9() && IsAMD());

        // D3D11 FL9_3 has a special codepath that emulates instanced points rendering
        // but it has bugs and was only implemented for vertex positions in a buffer object,
        // not client memory as used in this test.
        ANGLE_SKIP_TEST_IF(IsD3D11_FL93() && geometry == Point);

        // Unknown problem.  FL9_3 is not officially supported anyway.
        ANGLE_SKIP_TEST_IF(IsD3D11_FL93() && geometry == Quad && draw == NonIndexed);

        // The window is divided into kMaxDrawn slices of size kDrawSize.
        // The slice drawn into is determined by the instance datum.
        // The instance data array selects all the slices in order.
        // 'lastDrawn' is the index (zero-based) of the last slice into which we draw.
        const unsigned lastDrawn = (numInstance - 1) / divisor;

        // Ensure the numInstance and divisor parameters are valid.
        ASSERT_TRUE(lastDrawn < kMaxDrawn);

        ASSERT_TRUE(instanceAttrib == 0 || instanceAttrib == 1);
        const int positionAttrib = 1 - instanceAttrib;

        glUseProgram(mProgram[instanceAttrib]);

        glBindBuffer(GL_ARRAY_BUFFER, storage == Buffer ? mInstanceBuffer : 0);
        glVertexAttribPointer(instanceAttrib, 1, GL_FLOAT, GL_FALSE, 0,
                              storage == Buffer ? nullptr : mInstanceData);
        glEnableVertexAttribArray(instanceAttrib);
        if (vendor == Angle)
            glVertexAttribDivisorANGLE(instanceAttrib, divisor);
        else if (vendor == Ext)
            glVertexAttribDivisorEXT(instanceAttrib, divisor);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(positionAttrib, 2, GL_FLOAT, GL_FALSE, 0,
                              geometry == Point ? kPointVertices : kQuadVertices);
        glEnableVertexAttribArray(positionAttrib);
        if (vendor == Angle)
            glVertexAttribDivisorANGLE(positionAttrib, 0);
        else if (vendor == Ext)
            glVertexAttribDivisorEXT(positionAttrib, 0);

        glClear(GL_COLOR_BUFFER_BIT);

        if (geometry == Point)
        {
            if (draw == Indexed)
                if (vendor == Angle)
                    glDrawElementsInstancedANGLE(GL_POINTS, ArraySize(kPointIndices),
                                                 GL_UNSIGNED_SHORT, kPointIndices, numInstance);
                else
                    glDrawElementsInstancedEXT(GL_POINTS, ArraySize(kPointIndices),
                                               GL_UNSIGNED_SHORT, kPointIndices, numInstance);
            else if (vendor == Angle)
                glDrawArraysInstancedANGLE(GL_POINTS, offset, 4 /*vertices*/, numInstance);
            else
                glDrawArraysInstancedEXT(GL_POINTS, offset, 4 /*vertices*/, numInstance);
        }
        else
        {
            if (draw == Indexed)
                if (vendor == Angle)
                    glDrawElementsInstancedANGLE(GL_TRIANGLES, ArraySize(kQuadIndices),
                                                 GL_UNSIGNED_SHORT, kQuadIndices, numInstance);
                else
                    glDrawElementsInstancedEXT(GL_TRIANGLES, ArraySize(kQuadIndices),
                                               GL_UNSIGNED_SHORT, kQuadIndices, numInstance);
            else if (vendor == Angle)
                glDrawArraysInstancedANGLE(GL_TRIANGLES, offset, 6 /*vertices*/, numInstance);
            else
                glDrawArraysInstancedEXT(GL_TRIANGLES, offset, 6 /*vertices*/, numInstance);
        }

        ASSERT_GL_NO_ERROR();
        checkDrawing(lastDrawn);
    }

    void checkDrawing(unsigned lastDrawn)
    {
        for (unsigned i = 0; i < kMaxDrawn; ++i)
        {
            float y =
                -1.0f + static_cast<float>(kDrawSize) / 2.0f + static_cast<float>(i * kDrawSize);
            int iy = static_cast<int>((y + 1.0f) / 2.0f * getWindowHeight());
            for (unsigned j = 0; j < 8; j += 2)
            {
                int ix = static_cast<int>((kPointVertices[j] + 1.0f) / 2.0f * getWindowWidth());
                EXPECT_PIXEL_COLOR_EQ(ix, iy, i <= lastDrawn ? GLColor::red : GLColor::blue)
                    << std::endl;
            }
        }
    }

    GLuint mProgram[2];
    GLuint mInstanceBuffer;

    static constexpr unsigned kMaxDrawn = 16;
    static constexpr float kDrawSize    = 2.0 / kMaxDrawn;
    GLfloat mInstanceData[kMaxDrawn];

    // clang-format off

    // Vertices 0-5 are two triangles that form a quad filling the first "slice" of the window.
    // See above about slices.  Vertices 4-9 are the same two triangles.
    static constexpr GLfloat kQuadVertices[] = {
        -1, -1,
         1, -1,
        -1, -1 + kDrawSize,
         1, -1,
         1, -1 + kDrawSize,
        -1, -1 + kDrawSize,
         1, -1,
         1, -1,
        -1, -1 + kDrawSize,
        -1, -1,
    };

    // Points 0-3 are spread across the first "slice."
    // Points 2-4 are the same.
    static constexpr GLfloat kPointVertices[] = {
        -0.6f, -1 + kDrawSize / 2.0,
        -0.2f, -1 + kDrawSize / 2.0,
         0.2f, -1 + kDrawSize / 2.0,
         0.6f, -1 + kDrawSize / 2.0,
        -0.2f, -1 + kDrawSize / 2.0,
        -0.6f, -1 + kDrawSize / 2.0,
    };
    // clang-format on

    // Same two triangles as described above.
    static constexpr GLushort kQuadIndices[] = {2, 9, 7, 5, 6, 4};

    // Same four points as described above.
    static constexpr GLushort kPointIndices[] = {1, 5, 3, 2};
};

constexpr unsigned InstancingTest::kMaxDrawn;
constexpr float InstancingTest::kDrawSize;
constexpr GLfloat InstancingTest::kQuadVertices[];
constexpr GLfloat InstancingTest::kPointVertices[];
constexpr GLushort InstancingTest::kQuadIndices[];
constexpr GLushort InstancingTest::kPointIndices[];

#define TEST_INDEXED(attrib, geometry, storage, vendor)                      \
    TEST_P(InstancingTest, IndexedAttrib##attrib##geometry##storage##vendor) \
    {                                                                        \
        runTest(11, 2, attrib, geometry, Indexed, storage, vendor, 0);       \
    }

#define TEST_NONINDEXED(attrib, geometry, storage, vendor, offset)                              \
    TEST_P(InstancingTest, NonIndexedAttrib##attrib##geometry##storage##vendor##Offset##offset) \
    {                                                                                           \
        runTest(11, 2, attrib, geometry, NonIndexed, storage, vendor, offset);                  \
    }

#define TEST_DIVISOR(numInstance, divisor)                                    \
    TEST_P(InstancingTest, Instances##numInstance##Divisor##divisor)          \
    {                                                                         \
        runTest(numInstance, divisor, 1, Quad, NonIndexed, Buffer, Angle, 0); \
    }

// D3D9 and D3D11 FL9_3, have a special codepath that rearranges the input layout sent to D3D,
// to ensure that slot/stream zero of the input layout doesn't contain per-instance data, so
// we test with attribute 0 being instanced, as will as attribute 1 being instanced.
//
// Tests with a non-zero 'offset' check that "first" parameter to glDrawArraysInstancedANGLE is only
// an offset into the non-instanced vertex attributes.
TEST_INDEXED(0, Quad, Buffer, Angle)
TEST_INDEXED(0, Quad, Memory, Angle)
TEST_INDEXED(1, Quad, Buffer, Angle)
TEST_INDEXED(1, Quad, Memory, Angle)
TEST_INDEXED(0, Point, Buffer, Angle)
TEST_INDEXED(0, Point, Memory, Angle)
TEST_INDEXED(1, Point, Buffer, Angle)
TEST_INDEXED(1, Point, Memory, Angle)
TEST_INDEXED(0, Quad, Buffer, Ext)
TEST_INDEXED(0, Quad, Memory, Ext)
TEST_INDEXED(1, Quad, Buffer, Ext)
TEST_INDEXED(1, Quad, Memory, Ext)
TEST_INDEXED(0, Point, Buffer, Ext)
TEST_INDEXED(0, Point, Memory, Ext)
TEST_INDEXED(1, Point, Buffer, Ext)
TEST_INDEXED(1, Point, Memory, Ext)

// offset should be 0 or 4 for quads
TEST_NONINDEXED(0, Quad, Buffer, Angle, 0)
TEST_NONINDEXED(0, Quad, Buffer, Angle, 4)
TEST_NONINDEXED(0, Quad, Memory, Angle, 0)
TEST_NONINDEXED(0, Quad, Memory, Angle, 4)
TEST_NONINDEXED(1, Quad, Buffer, Angle, 0)
TEST_NONINDEXED(1, Quad, Buffer, Angle, 4)
TEST_NONINDEXED(1, Quad, Memory, Angle, 0)
TEST_NONINDEXED(1, Quad, Memory, Angle, 4)
TEST_NONINDEXED(0, Quad, Buffer, Ext, 0)
TEST_NONINDEXED(0, Quad, Buffer, Ext, 4)
TEST_NONINDEXED(0, Quad, Memory, Ext, 0)
TEST_NONINDEXED(0, Quad, Memory, Ext, 4)
TEST_NONINDEXED(1, Quad, Buffer, Ext, 0)
TEST_NONINDEXED(1, Quad, Buffer, Ext, 4)
TEST_NONINDEXED(1, Quad, Memory, Ext, 0)
TEST_NONINDEXED(1, Quad, Memory, Ext, 4)

// offset should be 0 or 2 for points
TEST_NONINDEXED(0, Point, Buffer, Angle, 0)
TEST_NONINDEXED(0, Point, Buffer, Angle, 2)
TEST_NONINDEXED(0, Point, Memory, Angle, 0)
TEST_NONINDEXED(0, Point, Memory, Angle, 2)
TEST_NONINDEXED(1, Point, Buffer, Angle, 0)
TEST_NONINDEXED(1, Point, Buffer, Angle, 2)
TEST_NONINDEXED(1, Point, Memory, Angle, 0)
TEST_NONINDEXED(1, Point, Memory, Angle, 2)
TEST_NONINDEXED(0, Point, Buffer, Ext, 0)
TEST_NONINDEXED(0, Point, Buffer, Ext, 2)
TEST_NONINDEXED(0, Point, Memory, Ext, 0)
TEST_NONINDEXED(0, Point, Memory, Ext, 2)
TEST_NONINDEXED(1, Point, Buffer, Ext, 0)
TEST_NONINDEXED(1, Point, Buffer, Ext, 2)
TEST_NONINDEXED(1, Point, Memory, Ext, 0)
TEST_NONINDEXED(1, Point, Memory, Ext, 2)

// The following tests produce each value of 'lastDrawn' in runTest() from 1 to kMaxDrawn, a few
// different ways.
TEST_DIVISOR(1, 1)
TEST_DIVISOR(1, 2)
TEST_DIVISOR(2, 1)
TEST_DIVISOR(3, 1)
TEST_DIVISOR(3, 2)
TEST_DIVISOR(4, 1)
TEST_DIVISOR(5, 1)
TEST_DIVISOR(5, 2)
TEST_DIVISOR(6, 1)
TEST_DIVISOR(6, 2)
TEST_DIVISOR(7, 1)
TEST_DIVISOR(7, 2)
TEST_DIVISOR(8, 1)
TEST_DIVISOR(8, 2)
TEST_DIVISOR(8, 4)
TEST_DIVISOR(9, 1)
TEST_DIVISOR(9, 2)
TEST_DIVISOR(10, 1)
TEST_DIVISOR(11, 1)
TEST_DIVISOR(11, 2)
TEST_DIVISOR(12, 1)
TEST_DIVISOR(12, 11)
TEST_DIVISOR(13, 1)
TEST_DIVISOR(13, 2)
TEST_DIVISOR(14, 1)
TEST_DIVISOR(15, 1)
TEST_DIVISOR(15, 2)
TEST_DIVISOR(16, 1)
TEST_DIVISOR(16, 3)
TEST_DIVISOR(16, 7)
TEST_DIVISOR(17, 2)
TEST_DIVISOR(20, 2)
TEST_DIVISOR(21, 2)
TEST_DIVISOR(23, 2)
TEST_DIVISOR(25, 5)
TEST_DIVISOR(25, 33)
TEST_DIVISOR(26, 2)
TEST_DIVISOR(26, 3)
TEST_DIVISOR(27, 2)
TEST_DIVISOR(27, 4)
TEST_DIVISOR(28, 3)
TEST_DIVISOR(29, 2)
TEST_DIVISOR(29, 11)
TEST_DIVISOR(30, 4)
TEST_DIVISOR(31, 6)
TEST_DIVISOR(32, 2)
TEST_DIVISOR(32, 3)
TEST_DIVISOR(32, 8)
TEST_DIVISOR(34, 3)
TEST_DIVISOR(34, 30)

class InstancingTestES3 : public InstancingTest
{
  public:
    InstancingTestES3() {}
};

class InstancingTestES31 : public InstancingTest
{
  public:
    InstancingTestES31() {}
};

// Verify that VertexAttribDivisor can update both binding divisor and attribBinding.
TEST_P(InstancingTestES31, UpdateAttribBindingByVertexAttribDivisor)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));

    glUseProgram(mProgram[0]);

    // Get the attribute locations
    GLint positionLoc    = glGetAttribLocation(mProgram[0], "a_position");
    GLint instancePosLoc = glGetAttribLocation(mProgram[0], "a_instance");
    ASSERT_NE(-1, positionLoc);
    ASSERT_NE(-1, instancePosLoc);
    ASSERT_GL_NO_ERROR();

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLBuffer quadBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, quadBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    const unsigned numInstance = 4;
    const unsigned divisor     = 1;
    const unsigned lastDrawn   = (numInstance - 1) / divisor;

    // Ensure the numInstance and divisor parameters are valid.
    ASSERT_TRUE(lastDrawn < kMaxDrawn);

    // Set the formats by VertexAttribFormat
    glVertexAttribFormat(positionLoc, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribFormat(instancePosLoc, 1, GL_FLOAT, GL_FALSE, 0);
    glEnableVertexAttribArray(positionLoc);
    glEnableVertexAttribArray(instancePosLoc);

    const GLint positionBinding = instancePosLoc;
    const GLint instanceBinding = positionLoc;

    // Load the vertex position into the binding indexed positionBinding (== instancePosLoc)
    // Load the instance position into the binding indexed instanceBinding (== positionLoc)
    glBindVertexBuffer(positionBinding, quadBuffer, 0, 2 * sizeof(kQuadVertices[0]));
    glBindVertexBuffer(instanceBinding, mInstanceBuffer, 0, sizeof(mInstanceData[0]));

    // The attribute indexed positionLoc is using the binding indexed positionBinding
    // The attribute indexed instancePosLoc is using the binding indexed instanceBinding
    glVertexAttribBinding(positionLoc, positionBinding);
    glVertexAttribBinding(instancePosLoc, instanceBinding);

    // Enable instancing on the binding indexed instanceBinding
    glVertexBindingDivisor(instanceBinding, divisor);

    // Do the first instanced draw
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElementsInstanced(GL_TRIANGLES, ArraySize(kQuadIndices), GL_UNSIGNED_SHORT, kQuadIndices,
                            numInstance);
    checkDrawing(lastDrawn);

    // Disable instancing.
    glVertexBindingDivisor(instanceBinding, 0);

    // Load the vertex position into the binding indexed positionLoc.
    // Load the instance position into the binding indexed instancePosLoc.
    glBindVertexBuffer(positionLoc, quadBuffer, 0, 2 * sizeof(kQuadVertices[0]));
    glBindVertexBuffer(instancePosLoc, mInstanceBuffer, 0, sizeof(mInstanceData[0]));

    // The attribute indexed positionLoc is using the binding indexed positionLoc.
    glVertexAttribBinding(positionLoc, positionLoc);

    // Call VertexAttribDivisor to both enable instancing on instancePosLoc and set the attribute
    // indexed instancePosLoc using the binding indexed instancePosLoc.
    glVertexAttribDivisor(instancePosLoc, divisor);

    // Do the second instanced draw
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElementsInstanced(GL_TRIANGLES, ArraySize(kQuadIndices), GL_UNSIGNED_SHORT, kQuadIndices,
                            numInstance);
    checkDrawing(lastDrawn);

    glDeleteVertexArrays(1, &vao);
}

// Verify that a large divisor that also changes doesn't cause issues and renders correctly.
TEST_P(InstancingTestES3, LargeDivisor)
{
    constexpr char kVS[] = R"(#version 300 es
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_color;
out vec4 v_color;
void main()
{
    gl_Position = a_position;
    gl_PointSize = 4.0f;
    v_color = a_color;
})";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
in vec4 v_color;
out vec4 my_FragColor;
void main()
{
    my_FragColor = v_color;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

    GLBuffer buf;
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    std::vector<GLfloat> vertices;
    for (size_t i = 0u; i < 4u; ++i)
    {
        vertices.push_back(0.0f + i * 0.25f);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
    }
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * 4u, vertices.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, false, 0, nullptr);
    ASSERT_GL_NO_ERROR();

    GLBuffer colorBuf;
    glBindBuffer(GL_ARRAY_BUFFER, colorBuf);

    std::array<GLColor, 4> ubyteColors = {GLColor::red, GLColor::green};
    std::vector<float> floatColors;
    for (const GLColor &color : ubyteColors)
    {
        floatColors.push_back(color.R / 255.0f);
        floatColors.push_back(color.G / 255.0f);
        floatColors.push_back(color.B / 255.0f);
        floatColors.push_back(color.A / 255.0f);
    }
    glBufferData(GL_ARRAY_BUFFER, floatColors.size() * 4u, floatColors.data(), GL_DYNAMIC_DRAW);

    const GLuint kColorDivisor = 65536u * 2u;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, false, 0, nullptr);
    glVertexAttribDivisor(1, kColorDivisor);

    std::array<GLuint, 1u> indices       = {0u};
    std::array<GLuint, 3u> divisorsToTry = {256u, 65536u, 65536u * 2u};

    for (GLuint divisorToTry : divisorsToTry)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glVertexAttribDivisor(0, divisorToTry);

        GLuint instanceCount        = divisorToTry + 1u;
        unsigned int pointsRendered = (instanceCount - 1u) / divisorToTry + 1u;

        glDrawElementsInstanced(GL_POINTS, indices.size(), GL_UNSIGNED_INT, indices.data(),
                                instanceCount);
        ASSERT_GL_NO_ERROR();

        // Check that the intended number of points has been rendered.
        for (unsigned int pointIndex = 0u; pointIndex < pointsRendered + 1u; ++pointIndex)
        {
            GLint pointx = static_cast<GLint>((pointIndex * 0.125f + 0.5f) * getWindowWidth());
            GLint pointy = static_cast<GLint>(0.5f * getWindowHeight());

            if (pointIndex < pointsRendered)
            {
                GLuint pointColorIndex = (pointIndex * divisorToTry) / kColorDivisor;
                EXPECT_PIXEL_COLOR_EQ(pointx, pointy, ubyteColors[pointColorIndex]);
            }
            else
            {
                // Clear color.
                EXPECT_PIXEL_COLOR_EQ(pointx, pointy, GLColor::blue);
            }
        }
    }
}

// This is a regression test. If VertexAttribDivisor was returned as a signed integer, it would be
// incorrectly clamped down to the maximum signed integer.
TEST_P(InstancingTestES3, LargestDivisor)
{
    constexpr GLuint kLargeDivisor = std::numeric_limits<GLuint>::max();
    glVertexAttribDivisor(0, kLargeDivisor);

    GLuint divisor = 0;
    glGetVertexAttribIuiv(0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
    EXPECT_EQ(kLargeDivisor, divisor)
        << "Vertex attrib divisor read was not the same that was passed in.";
}

ANGLE_INSTANTIATE_TEST(InstancingTestES3, ES3_OPENGL(), ES3_OPENGLES(), ES3_D3D11());

ANGLE_INSTANTIATE_TEST(InstancingTestES31, ES31_OPENGL(), ES31_OPENGLES(), ES31_D3D11());

ANGLE_INSTANTIATE_TEST(InstancingTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(3, 0),
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES2_VULKAN());
