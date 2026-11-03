//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <array>
#include <cstring>
#include <sstream>

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{
enum Geometry
{
    Quad,
    Point,
    TriFan,
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

class InstancingTest : public ANGLETest<>
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

    void testTearDown() override
    {
        glDeleteBuffers(1, &mInstanceBuffer);
        glDeleteProgram(mProgram[0]);
        glDeleteProgram(mProgram[1]);
    }

    void testSetUp() override
    {
        for (unsigned i = 0; i < kMaxDrawn; ++i)
        {
            mInstanceData[i] = i * kDrawSize;
        }
        glGenBuffers(1, &mInstanceBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mInstanceBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(mInstanceData), mInstanceData.data(), GL_STATIC_DRAW);
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
                              storage == Buffer ? nullptr : mInstanceData.data());
        glEnableVertexAttribArray(instanceAttrib);
        if (vendor == Angle)
            glVertexAttribDivisorANGLE(instanceAttrib, divisor);
        else if (vendor == Ext)
            glVertexAttribDivisorEXT(instanceAttrib, divisor);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        const void *vertices;
        switch (geometry)
        {
            case Point:
                vertices = kPointVertices.data();
                break;
            case Quad:
                vertices = kQuadVertices.data();
                break;
            case TriFan:
                vertices = kTriFanVertices.data();
                break;
        }
        glVertexAttribPointer(positionAttrib, 2, GL_FLOAT, GL_FALSE, 0, vertices);
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
                    glDrawElementsInstancedANGLE(GL_POINTS, kPointIndices.size(), GL_UNSIGNED_SHORT,
                                                 kPointIndices.data(), numInstance);
                else
                    glDrawElementsInstancedEXT(GL_POINTS, kPointIndices.size(), GL_UNSIGNED_SHORT,
                                               kPointIndices.data(), numInstance);
            else if (vendor == Angle)
                glDrawArraysInstancedANGLE(GL_POINTS, offset, 4 /*vertices*/, numInstance);
            else
                glDrawArraysInstancedEXT(GL_POINTS, offset, 4 /*vertices*/, numInstance);
        }
        else if (geometry == Quad)
        {
            if (draw == Indexed)
                if (vendor == Angle)
                    glDrawElementsInstancedANGLE(GL_TRIANGLES, kQuadIndices.size(),
                                                 GL_UNSIGNED_SHORT, kQuadIndices.data(),
                                                 numInstance);
                else
                    glDrawElementsInstancedEXT(GL_TRIANGLES, kQuadIndices.size(), GL_UNSIGNED_SHORT,
                                               kQuadIndices.data(), numInstance);
            else if (vendor == Angle)
                glDrawArraysInstancedANGLE(GL_TRIANGLES, offset, 6 /*vertices*/, numInstance);
            else
                glDrawArraysInstancedEXT(GL_TRIANGLES, offset, 6 /*vertices*/, numInstance);
        }
        else if (geometry == TriFan)
        {
            if (draw == Indexed)
            {
                if (storage == Buffer)
                {
                    GLBuffer indexBuffer;
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kTriFanIndices),
                                 kTriFanIndices.data(), GL_STATIC_DRAW);

                    if (vendor == Angle)
                        glDrawElementsInstancedANGLE(GL_TRIANGLE_FAN, kTriFanIndices.size(),
                                                     GL_UNSIGNED_BYTE, 0, numInstance);
                    else
                        glDrawElementsInstancedEXT(GL_TRIANGLE_FAN, kTriFanIndices.size(),
                                                   GL_UNSIGNED_BYTE, 0, numInstance);
                }
                else
                {
                    if (vendor == Angle)
                        glDrawElementsInstancedANGLE(GL_TRIANGLE_FAN, kTriFanIndices.size(),
                                                     GL_UNSIGNED_BYTE, kTriFanIndices.data(),
                                                     numInstance);
                    else
                        glDrawElementsInstancedEXT(GL_TRIANGLE_FAN, kTriFanIndices.size(),
                                                   GL_UNSIGNED_BYTE, kTriFanIndices.data(),
                                                   numInstance);
                }
            }
            else if (vendor == Angle)
                glDrawArraysInstancedANGLE(GL_TRIANGLE_FAN, offset, 8 /*vertices*/, numInstance);
            else
                glDrawArraysInstancedEXT(GL_TRIANGLE_FAN, offset, 8 /*vertices*/, numInstance);
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

    std::array<GLuint, 2> mProgram;
    GLuint mInstanceBuffer;

    static constexpr unsigned kMaxDrawn = 16;
    static constexpr float kDrawSize    = 2.0 / kMaxDrawn;
    std::array<GLfloat, kMaxDrawn> mInstanceData;

    // clang-format off

    // Vertices 0-5 are two triangles that form a quad filling the first "slice" of the window.
    // See above about slices.  Vertices 4-9 are the same two triangles.
    static constexpr std::array<GLfloat, 20> kQuadVertices = {
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

    // Vertices 0-7 form a quad (triangle fan) filling the first "slice" of the window.
    // Vertices 8-15 are the same.
    static constexpr std::array<GLfloat, 32> kTriFanVertices = {
        -1, -1,
         1, -1,
        1, -1 + 0.5f * kDrawSize,
        1, -1 + kDrawSize,
        0.5f, -1 + kDrawSize,
        0, -1 + kDrawSize,
        -0.5f, -1 + kDrawSize,
        -1, -1 + kDrawSize,

        -1, -1,
         1, -1,
        1, -1 + 0.5f * kDrawSize,
        1, -1 + kDrawSize,
        0.5f, -1 + kDrawSize,
        0, -1 + kDrawSize,
        -0.5f, -1 + kDrawSize,
        -1, -1 + kDrawSize,
    };

    // Points 0-3 are spread across the first "slice."
    // Points 2-4 are the same.
    static constexpr std::array<GLfloat, 12> kPointVertices = {
        -0.6f, -1 + kDrawSize / 2.0,
        -0.2f, -1 + kDrawSize / 2.0,
         0.2f, -1 + kDrawSize / 2.0,
         0.6f, -1 + kDrawSize / 2.0,
        -0.2f, -1 + kDrawSize / 2.0,
        -0.6f, -1 + kDrawSize / 2.0,
    };
    // clang-format on

    // Same two triangles as described above.
    static constexpr std::array<GLushort, 6> kQuadIndices = {2, 9, 7, 5, 6, 4};

    // Same triangle fan as described above.
    static constexpr std::array<GLubyte, 8> kTriFanIndices = {0, 9, 10, 3, 4, 5, 14, 7};

    // Same four points as described above.
    static constexpr std::array<GLushort, 4> kPointIndices = {1, 5, 3, 2};
};

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

// D3D9 has a special codepath that rearranges the input layout sent to D3D,
// to ensure that slot/stream zero of the input layout doesn't contain per-instance data, so
// we test with attribute 0 being instanced, as will as attribute 1 being instanced.
//
// Tests with a non-zero 'offset' check that "first" parameter to glDrawArraysInstancedANGLE is only
// an offset into the non-instanced vertex attributes.
TEST_INDEXED(0, TriFan, Buffer, Angle)
TEST_INDEXED(0, TriFan, Memory, Angle)
TEST_INDEXED(1, TriFan, Buffer, Angle)
TEST_INDEXED(1, TriFan, Memory, Angle)
TEST_INDEXED(0, TriFan, Buffer, Ext)
TEST_INDEXED(0, TriFan, Memory, Ext)
TEST_INDEXED(1, TriFan, Buffer, Ext)
TEST_INDEXED(1, TriFan, Memory, Ext)
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

// offset should be 0 or 4 for quads, 0 or 8 for triangle fan
TEST_NONINDEXED(0, TriFan, Buffer, Angle, 0)
TEST_NONINDEXED(0, TriFan, Buffer, Angle, 8)
TEST_NONINDEXED(0, TriFan, Memory, Angle, 0)
TEST_NONINDEXED(0, TriFan, Memory, Angle, 8)
TEST_NONINDEXED(1, TriFan, Buffer, Angle, 0)
TEST_NONINDEXED(1, TriFan, Buffer, Angle, 8)
TEST_NONINDEXED(1, TriFan, Memory, Angle, 0)
TEST_NONINDEXED(1, TriFan, Memory, Angle, 8)
TEST_NONINDEXED(0, TriFan, Buffer, Ext, 0)
TEST_NONINDEXED(0, TriFan, Buffer, Ext, 8)
TEST_NONINDEXED(0, TriFan, Memory, Ext, 0)
TEST_NONINDEXED(0, TriFan, Memory, Ext, 8)
TEST_NONINDEXED(1, TriFan, Buffer, Ext, 0)
TEST_NONINDEXED(1, TriFan, Buffer, Ext, 8)
TEST_NONINDEXED(1, TriFan, Memory, Ext, 0)
TEST_NONINDEXED(1, TriFan, Memory, Ext, 8)
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

// Test line loop instanced draw
TEST_P(InstancingTest, LineLoop)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));

    constexpr char kVS[] = R"(
attribute vec2 a_position;
// x,y = offset, z = scale
attribute vec3 a_transform;

attribute vec4 a_color;

varying vec4 v_color;

invariant gl_Position;
void main()
{
    vec2 v_position = a_transform.z * a_position + a_transform.xy;
    gl_Position = vec4(v_position, 0.0, 1.0);

    v_color = a_color;
})";

    constexpr char kFS[] = R"(
precision highp float;
varying vec4 v_color;
void main()
{
    gl_FragColor = v_color;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_transform");
    glBindAttribLocation(program, 2, "a_color");
    glLinkProgram(program);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    constexpr std::array<GLfloat, 8> vertices = {
        0.1, 0.1, -0.1, 0.1, -0.1, -0.1, 0.1, -0.1,
    };

    constexpr std::array<GLfloat, 15> transform = {
        0, 0, 9, 0.2, 0.1, 2, 0.5, -0.2, 3, -0.8, -0.5, 1, -0.4, 0.4, 6,
    };

    constexpr GLsizei instances = transform.size() / 3;

    const std::array<GLfloat, instances * 3> colors = {
        1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0,
    };

    constexpr std::array<GLushort, 5> lineloopAsStripIndices = {0, 1, 2, 3, 0};

    std::vector<GLColor> expectedPixels(getWindowWidth() * getWindowHeight());

    // Draw in non-instanced way
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glVertexAttribDivisorANGLE(0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices.data());

    for (size_t i = 0; i < instances; ++i)
    {
        glVertexAttrib3fv(1, &transform[3 * i]);
        glVertexAttrib3fv(2, &colors[3 * i]);

        glDrawElements(GL_LINE_STRIP, lineloopAsStripIndices.size(), GL_UNSIGNED_SHORT,
                       lineloopAsStripIndices.data());
    }

    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 expectedPixels.data());
    ASSERT_GL_NO_ERROR();

    // Draw in instanced way:
    glClear(GL_COLOR_BUFFER_BIT);

    GLBuffer vertexBuffer[3];
    GLBuffer indexBuffer;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(lineloopAsStripIndices),
                 lineloopAsStripIndices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisorANGLE(0, 0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transform), transform.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisorANGLE(1, 1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisorANGLE(2, 1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArraysInstancedANGLE(GL_LINE_LOOP, 0, vertices.size() / 2, instances);

    std::vector<GLColor> actualPixels(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualPixels.data());
    EXPECT_EQ(expectedPixels, actualPixels);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElementsInstancedANGLE(GL_LINE_LOOP, lineloopAsStripIndices.size() - 1, GL_UNSIGNED_SHORT,
                                 0, instances);

    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actualPixels.data());
    EXPECT_EQ(expectedPixels, actualPixels);
}

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

class RobustInstancingTestES3 : public InstancingTestES3
{
  public:
    RobustInstancingTestES3()
    {
        if (!IsMac() && !IsIOS())
        {
            setRobustAccess(true);
        }
    }
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices.data(), GL_STATIC_DRAW);

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
    glDrawElementsInstanced(GL_TRIANGLES, kQuadIndices.size(), GL_UNSIGNED_SHORT,
                            kQuadIndices.data(), numInstance);
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
    glDrawElementsInstanced(GL_TRIANGLES, kQuadIndices.size(), GL_UNSIGNED_SHORT,
                            kQuadIndices.data(), numInstance);
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

// Regression test for D3D11 streaming of GL_DYNAMIC_DRAW buffers not taking into account instanced
// attributes for buffer size calculations. http://crbug.com/1425606
TEST_P(InstancingTestES3, D3D11StreamingInstancedData)
{
    constexpr char kVS[] = R"(#version 300 es
in float scale;
void main()
{
    gl_Position = vec4(vec2(gl_VertexID % 2, gl_VertexID % 3) * scale, 0, 1);
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 color;
void main()
{
    color = vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    GLBuffer buffer;
    float data = 0.5f;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), &data, GL_DYNAMIC_DRAW);

    GLint loc = glGetAttribLocation(program, "scale");
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, 1, GL_FLOAT, false, 0, 0);
    glVertexAttribDivisor(loc, 1);

    // Both of these should succeed
    glDrawArraysInstanced(GL_TRIANGLES, 0, 3, 1);
    EXPECT_GL_NO_ERROR();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    EXPECT_GL_NO_ERROR();
}

// Regression test for D3D11 streaming of GL_DYNAMIC_DRAW buffers not taking into account instanced
// attributes for buffer size calculations. http://crbug.com/1425606
TEST_P(InstancingTestES3, D3D11StreamingInstancedData2)
{
    constexpr char kVS[] = R"(#version 300 es
in float scale;
void main()
{
    gl_Position = vec4(vec2(gl_VertexID % 2, gl_VertexID % 3) * scale, 0, 1);
    gl_PointSize = 1.0;
})";

    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 color;
void main()
{
    color = vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);

    GLint loc = glGetAttribLocation(program, "scale");
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, 1, GL_FLOAT, false, 0, 0);
    glVertexAttribDivisor(loc, 10001);

    for (size_t i = 0; i < 100; i++)
    {
        float data = 0.5f;
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), &data, GL_DYNAMIC_DRAW);

        glDrawArrays(GL_POINTS, 0, 10000);
        EXPECT_GL_NO_ERROR();
    }
}

// Regression test for out-of-bounds read during divisor emulation. http://crbug.com/500476886
TEST_P(RobustInstancingTestES3, IncompleteStrideForLastVertex)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_robust_buffer_access_behavior"));

    constexpr char kVS[] = R"(#version 300 es
    layout(location=0) in vec4 a_inst;
    layout(location=1) in float a_unused;
    out vec4 v;
    const vec2 coords[3] = vec2[](
    vec2(-3.0, -3.0),
    vec2( 3.0, -3.0),
    vec2( 0.0,  3.0)
    );
    void main() {
    v = a_inst + vec4(a_unused);
    if (gl_InstanceID == 0) {
      gl_Position = vec4(coords[gl_VertexID], 0.0, 1.0);
    } else {
      gl_Position = vec4(-2.0, -2.0, 0.0, 1.0);
    }
    })";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;
in vec4 v;
out vec4 o;
void main() { o = v; })";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Unused attrib 1
    GLBuffer unusedBuf;
    glBindBuffer(GL_ARRAY_BUFFER, unusedBuf);
    std::vector<GLfloat> unusedData(1, 0.0f);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * unusedData.size(), unusedData.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Instanced attrib 0
    GLBuffer instBuf;
    glBindBuffer(GL_ARRAY_BUFFER, instBuf);
    std::vector<GLfloat> instData(64, 0.0f);  // 256 bytes
    instData[59] = instData[60] = instData[61] = instData[62] = 1.0f;
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * instData.size(), instData.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    // Buffer size = 256, offset = 236.
    // bytes = 256 - 236 = 20. stride = 16, formatSize = 16.
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<const void *>(236));

    // Divisor > MaxVertexAttribDivisor will trigger emulation path.
    // mMaxVertexAttribDivisor is clamped to 255 in the Vulkan backend, so 256 guarantees emulation.
    // The previous bug resulted in GetVertexCountForRange returning an over-counted
    // number of vertices, leading to an OOB read in StreamVertexDataWithDivisor.
    GLuint divisor = 256;
    glVertexAttribDivisor(0, divisor);

    EXPECT_GL_NO_ERROR();

    // The draw call should not crash. We need divisor + 1 instances to reach the second vertex
    // and trigger the OOB read.
    glDrawArraysInstanced(GL_TRIANGLES, 0, 3, divisor + 1);

    // Check the center pixel, which corresponds to gl_InstanceID == 0.
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() / 2, getWindowHeight() / 2,
                            GLColor(255, 255, 255, 255), 1);

    EXPECT_GL_NO_ERROR();
}

// Draws instances of a quad, one in each cell of a grid, and checks that every vertex reads the
// values of its vertex from the per-vertex attributes and of its instance from the per-instance
// attributes. Attribute 0 is the vec2 corner of the quad (0 or 1 in x and y), and attribute 1 the
// vec2 cell of the instance, with a divisor of 1. The other attributes are floats. The value of
// per-vertex attribute i at a vertex is i + x + 2 * y, where (x, y) is the corner, and the value of
// element e of per-instance attribute i is i + 4 * e, so the vertices that read a wrong value draw
// red, and the cells of the instances that are not drawn stay blue.
class InstancedAttributeTest : public ANGLETest<>
{
  protected:
    static constexpr int kWindowSize = 64;

    InstancedAttributeTest()
    {
        setWindowWidth(kWindowSize);
        setWindowHeight(kWindowSize);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testTearDown() override
    {
        for (GLuint program : mPrograms)
        {
            glDeleteProgram(program);
        }
    }

    // Where the data of an attribute is. The attributes whose buffer is kClientMemory each read
    // their own client memory.
    static constexpr size_t kClientMemory = std::numeric_limits<size_t>::max();
    struct AttribLayout
    {
        size_t buffer;
        size_t offset;
        // 0 for tightly packed.
        size_t stride;
        GLuint divisor;
    };

    enum class DrawMode
    {
        Arrays,
        ElementsInBuffer,
        ElementsInClientMemory,
        // Non-instanced draws, which draw instance 0.
        NonInstancedArrays,
        NonInstancedElements,
    };

    // The vertices before the quad, which a draw that ignores its first vertex reads.
    static constexpr size_t kPadVertexCount                                      = 2;
    static constexpr size_t kQuadVertexCount                                     = 6;
    static constexpr std::array<std::array<GLfloat, 2>, kQuadVertexCount> kCorners = {{
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, 1.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
    }};

    // Returns false when the context has no instanced arrays.
    bool initInstancing()
    {
        if (getClientMajorVersion() >= 3)
        {
            return true;
        }
        if (EnsureGLExtensionEnabled("GL_ANGLE_instanced_arrays"))
        {
            mUseANGLEEntryPoints = true;
            return true;
        }
        return EnsureGLExtensionEnabled("GL_EXT_instanced_arrays");
    }

    GLint getAttribCount()
    {
        GLint maxAttribs = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
        return std::min(maxAttribs, 16);
    }

    void vertexAttribDivisor(GLuint index, GLuint divisor)
    {
        if (getClientMajorVersion() >= 3)
        {
            glVertexAttribDivisor(index, divisor);
        }
        else if (mUseANGLEEntryPoints)
        {
            glVertexAttribDivisorANGLE(index, divisor);
        }
        else
        {
            glVertexAttribDivisorEXT(index, divisor);
        }
    }

    void drawArraysInstanced(GLint first, GLsizei count, GLsizei instanceCount)
    {
        if (getClientMajorVersion() >= 3)
        {
            glDrawArraysInstanced(GL_TRIANGLES, first, count, instanceCount);
        }
        else if (mUseANGLEEntryPoints)
        {
            glDrawArraysInstancedANGLE(GL_TRIANGLES, first, count, instanceCount);
        }
        else
        {
            glDrawArraysInstancedEXT(GL_TRIANGLES, first, count, instanceCount);
        }
    }

    void drawElementsInstanced(GLsizei count, const void *indices, GLsizei instanceCount)
    {
        if (getClientMajorVersion() >= 3)
        {
            glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices,
                                    instanceCount);
        }
        else if (mUseANGLEEntryPoints)
        {
            glDrawElementsInstancedANGLE(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices,
                                         instanceCount);
        }
        else
        {
            glDrawElementsInstancedEXT(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices,
                                       instanceCount);
        }
    }

    // The divisors of the attributes are uniforms, so that draws with other divisors can use the
    // same program. With checkInstanceID, the program also checks that gl_InstanceID is the
    // instance of the cell.
    GLuint getProgram(size_t attribCount, bool checkInstanceID)
    {
        const size_t key = attribCount * 2 + (checkInstanceID ? 1 : 0);
        if (mPrograms.size() <= key)
        {
            mPrograms.resize(key + 1, 0);
        }
        if (mPrograms[key] != 0)
        {
            return mPrograms[key];
        }

        const char *in = checkInstanceID ? "in" : "attribute";
        std::stringstream vs;
        if (checkInstanceID)
        {
            vs << "#version 300 es\n";
        }
        vs << in << " vec2 a0;\n" << in << " vec2 a1;\n";
        for (size_t attribIndex = 2; attribIndex < attribCount; ++attribIndex)
        {
            vs << in << " float a" << attribIndex << ";\n"
               << "uniform float u_divisor" << attribIndex << ";\n";
        }
        vs << "uniform float u_columns;\n"
           << (checkInstanceID ? "out" : "varying") << " vec4 v_color;\n"
           << "void main()\n"
              "{\n"
              "    float instance = a1.x + u_columns * a1.y;\n"
              "    gl_Position = vec4((a1 + a0) * (2.0 / u_columns) - 1.0, 0.0, 1.0);\n"
              "    float vertexValue = a0.x + 2.0 * a0.y;\n"
              "    bool ok = true;\n";
        for (size_t attribIndex = 2; attribIndex < attribCount; ++attribIndex)
        {
            const std::string divisor = "u_divisor" + std::to_string(attribIndex);
            vs << "    ok = ok && abs(a" << attribIndex << " - (" << attribIndex << ".0 + ("
               << divisor << " == 0.0 ? vertexValue : 4.0 * floor((instance + 0.5) / " << divisor
               << ")))) < 0.01;\n";
        }
        if (checkInstanceID)
        {
            vs << "    ok = ok && gl_InstanceID == int(instance);\n";
        }
        vs << "    v_color = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
              "}\n";

        const char *kFS =
            "precision mediump float;\n"
            "varying vec4 v_color;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = v_color;\n"
            "}\n";
        const char *kFS3 =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in vec4 v_color;\n"
            "out vec4 fragColor;\n"
            "void main()\n"
            "{\n"
            "    fragColor = v_color;\n"
            "}\n";

        mPrograms[key] =
            CompileProgram(vs.str().c_str(), checkInstanceID ? kFS3 : kFS, [&](GLuint program) {
                for (size_t attribIndex = 0; attribIndex < attribCount; ++attribIndex)
                {
                    const std::string name = "a" + std::to_string(attribIndex);
                    glBindAttribLocation(program, static_cast<GLuint>(attribIndex), name.c_str());
                }
            });
        return mPrograms[key];
    }

    static size_t GetAttribSize(size_t attribIndex) { return attribIndex < 2 ? 8 : 4; }

    // The values of element e of an attribute.
    static std::array<GLfloat, 2> GetAttribValue(size_t attribIndex,
                                                 GLuint divisor,
                                                 size_t element,
                                                 size_t columns)
    {
        if (divisor != 0)
        {
            if (attribIndex == 1)
            {
                return {static_cast<GLfloat>(element % columns),
                        static_cast<GLfloat>(element / columns)};
            }
            return {static_cast<GLfloat>(attribIndex + 4 * element), 0.0f};
        }
        if (element < kPadVertexCount)
        {
            // Vertices that are not drawn: outside the window, and wrong values.
            return {1000.0f, 1000.0f};
        }
        const std::array<GLfloat, 2> &corner = kCorners[element - kPadVertexCount];
        if (attribIndex == 0)
        {
            return corner;
        }
        return {static_cast<GLfloat>(attribIndex) + corner[0] + 2.0f * corner[1], 0.0f};
    }

    void drawAndCheck(const std::vector<AttribLayout> &layouts,
                      GLsizei instanceCount,
                      DrawMode drawMode,
                      size_t columns = 16)
    {
        ASSERT_GE(layouts.size(), 2u);
        ASSERT_EQ(layouts[0].divisor, 0u);
        ASSERT_EQ(layouts[1].divisor, 1u);
        ASSERT_LE(static_cast<size_t>(instanceCount), columns * columns);
        ASSERT_EQ(kWindowSize % columns, 0u);

        GLuint program = getProgram(layouts.size(), mCheckInstanceID);
        ASSERT_NE(program, 0u);
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "u_columns"), static_cast<GLfloat>(columns));
        for (size_t attribIndex = 2; attribIndex < layouts.size(); ++attribIndex)
        {
            const std::string name = "u_divisor" + std::to_string(attribIndex);
            glUniform1f(glGetUniformLocation(program, name.c_str()),
                        static_cast<GLfloat>(layouts[attribIndex].divisor));
        }

        // Fill the buffers and client memory. The bytes that no attribute reads are garbage.
        std::vector<std::vector<uint8_t>> bufferData;
        std::vector<std::vector<bool>> bufferWritten;
        std::vector<std::vector<uint8_t>> clientData(layouts.size());
        for (size_t attribIndex = 0; attribIndex < layouts.size(); ++attribIndex)
        {
            const AttribLayout &layout = layouts[attribIndex];
            const size_t size          = GetAttribSize(attribIndex);
            const size_t stride        = layout.stride == 0 ? size : layout.stride;
            size_t elementCount =
                layout.divisor == 0
                    ? kPadVertexCount + kQuadVertexCount
                    : std::max<size_t>((instanceCount + layout.divisor - 1) / layout.divisor, 1);
            if (attribIndex + 1 == layouts.size() && mLastAttribElementCount != 0)
            {
                elementCount = mLastAttribElementCount;
            }
            const size_t end = layout.offset + (elementCount - 1) * stride + size;

            std::vector<uint8_t> *data = &clientData[attribIndex];
            std::vector<bool> written;
            std::vector<bool> *writtenData = &written;
            if (layout.buffer != kClientMemory)
            {
                if (bufferData.size() <= layout.buffer)
                {
                    bufferData.resize(layout.buffer + 1);
                    bufferWritten.resize(layout.buffer + 1);
                }
                data        = &bufferData[layout.buffer];
                writtenData = &bufferWritten[layout.buffer];
            }
            if (data->size() < ((end + 3) & ~size_t(3)))
            {
                const size_t oldSize = data->size();
                data->resize(((end + 3) & ~size_t(3)));
                writtenData->resize(data->size(), false);
                const GLfloat garbage = -1000.0f;
                for (size_t offset = oldSize; offset < data->size(); offset += 4)
                {
                    memcpy(data->data() + offset, &garbage, 4);
                }
            }
            for (size_t element = 0; element < elementCount; ++element)
            {
                const std::array<GLfloat, 2> value =
                    GetAttribValue(attribIndex, layout.divisor, element, columns);
                const size_t offset = layout.offset + element * stride;
                for (size_t byte = offset; byte < offset + size; ++byte)
                {
                    ASSERT_FALSE((*writtenData)[byte]) << "Overlapping attributes";
                    (*writtenData)[byte] = true;
                }
                memcpy(data->data() + offset, value.data(), size);
            }
        }

        std::vector<GLBuffer> buffers(bufferData.size());
        for (size_t buffer = 0; buffer < bufferData.size(); ++buffer)
        {
            glBindBuffer(GL_ARRAY_BUFFER, buffers[buffer]);
            glBufferData(GL_ARRAY_BUFFER, bufferData[buffer].size(), bufferData[buffer].data(),
                         GL_STATIC_DRAW);
        }

        for (size_t attribIndex = 0; attribIndex < layouts.size(); ++attribIndex)
        {
            const AttribLayout &layout = layouts[attribIndex];
            const GLuint location      = static_cast<GLuint>(attribIndex);
            const GLint components     = attribIndex < 2 ? 2 : 1;
            if (layout.buffer == kClientMemory)
            {
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE,
                                      static_cast<GLsizei>(layout.stride),
                                      clientData[attribIndex].data() + layout.offset);
            }
            else
            {
                glBindBuffer(GL_ARRAY_BUFFER, buffers[layout.buffer]);
                glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE,
                                      static_cast<GLsizei>(layout.stride),
                                      reinterpret_cast<const void *>(layout.offset));
            }
            glEnableVertexAttribArray(location);
            vertexAttribDivisor(location, layout.divisor);
        }
        for (size_t attribIndex = layouts.size(); attribIndex < mEnabledAttribCount; ++attribIndex)
        {
            glDisableVertexAttribArray(static_cast<GLuint>(attribIndex));
            vertexAttribDivisor(static_cast<GLuint>(attribIndex), 0);
        }
        mEnabledAttribCount = layouts.size();
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        ASSERT_GL_NO_ERROR();

        std::array<GLushort, kQuadVertexCount> indices;
        for (size_t index = 0; index < kQuadVertexCount; ++index)
        {
            indices[index] = static_cast<GLushort>(kPadVertexCount + kQuadVertexCount - 1 - index);
        }
        GLBuffer indexBuffer;
        const void *indicesPointer = indices.data();
        if (drawMode != DrawMode::ElementsInClientMemory)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(),
                         GL_STATIC_DRAW);
            indicesPointer = nullptr;
        }
        else
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        GLsizei drawnCount = instanceCount;
        switch (drawMode)
        {
            case DrawMode::Arrays:
                drawArraysInstanced(kPadVertexCount, kQuadVertexCount, instanceCount);
                break;
            case DrawMode::ElementsInBuffer:
            case DrawMode::ElementsInClientMemory:
                drawElementsInstanced(kQuadVertexCount, indicesPointer, instanceCount);
                break;
            case DrawMode::NonInstancedArrays:
                glDrawArrays(GL_TRIANGLES, kPadVertexCount, kQuadVertexCount);
                drawnCount = 1;
                break;
            case DrawMode::NonInstancedElements:
                glDrawElements(GL_TRIANGLES, kQuadVertexCount, GL_UNSIGNED_SHORT, indicesPointer);
                drawnCount = 1;
                break;
        }
        ASSERT_GL_NO_ERROR();

        std::vector<GLColor> pixels(kWindowSize * kWindowSize);
        glReadPixels(0, 0, kWindowSize, kWindowSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ASSERT_GL_NO_ERROR();
        // With mLastAttribElementCount, the instances that read the last attribute past the end of
        // its buffer read undefined values, so their cells can have any color, but they are drawn.
        size_t checkedCount = static_cast<size_t>(drawnCount);
        if (mLastAttribElementCount != 0)
        {
            checkedCount = std::min(checkedCount, mLastAttribElementCount * layouts.back().divisor);
        }
        const size_t cellSize = kWindowSize / columns;
        size_t wrongCells     = 0;
        std::stringstream firstWrongCells;
        for (size_t cell = 0; cell < columns * columns; ++cell)
        {
            const size_t x         = (cell % columns) * cellSize + cellSize / 2;
            const size_t y         = (cell / columns) * cellSize + cellSize / 2;
            const GLColor expected = cell < static_cast<size_t>(drawnCount) ? GLColor::green
                                                                             : GLColor::blue;
            const GLColor &actual  = pixels[y * kWindowSize + x];
            const bool isUndefined = cell >= checkedCount && cell < static_cast<size_t>(drawnCount);
            if (isUndefined ? actual.A != 255 || actual == GLColor::blue : actual != expected)
            {
                if (wrongCells < 8)
                {
                    firstWrongCells << " cell " << cell << ": " << actual << " instead of "
                                    << expected << ";";
                }
                ++wrongCells;
            }
        }
        EXPECT_EQ(wrongCells, 0u) << "Draw mode " << static_cast<int>(drawMode) << ", "
                                  << instanceCount << " instances:" << firstWrongCells.str();
    }

    // Draws with glDrawArraysInstanced, glDrawElementsInstanced with indices in a buffer and,
    // with client arrays, in client memory, and glDrawArrays and glDrawElements, which draw
    // instance 0.
    void drawWithAllModes(const std::vector<AttribLayout> &layouts,
                          GLsizei instanceCount,
                          size_t columns = 16)
    {
        for (DrawMode drawMode :
             {DrawMode::Arrays, DrawMode::ElementsInBuffer, DrawMode::ElementsInClientMemory,
              DrawMode::NonInstancedArrays, DrawMode::NonInstancedElements})
        {
            if (drawMode == DrawMode::ElementsInClientMemory && !mClientArraysEnabled)
            {
                continue;
            }
            drawAndCheck(layouts, instanceCount, drawMode, columns);
        }
    }

    // Per-instance attributes with distinct divisors 1 to attribCount - 2.
    std::vector<AttribLayout> distinctDivisorLayouts(GLint attribCount, bool clientMemory)
    {
        std::vector<AttribLayout> layouts;
        for (GLint attribIndex = 0; attribIndex < attribCount; ++attribIndex)
        {
            const size_t buffer =
                clientMemory ? kClientMemory : static_cast<size_t>(attribIndex);
            const GLuint divisor = attribIndex == 0 ? 0 : std::max<GLuint>(attribIndex - 1, 1);
            layouts.push_back({buffer, 0, 0, divisor});
        }
        return layouts;
    }

    // Attributes in one buffer, for 11 instances: the per-vertex attributes interleaved at the
    // start of the buffer, then two per-instance attributes with a divisor of 1 interleaved, then,
    // withDivisorTwo, a per-instance attribute with a divisor of 2.
    std::vector<AttribLayout> perVertexAndPerInstanceLayouts(bool withDivisorTwo)
    {
        constexpr size_t kInstanceStart   = (kPadVertexCount + kQuadVertexCount) * 12;
        constexpr size_t kDivisorTwoStart = kInstanceStart + 11 * 12;
        std::vector<AttribLayout> layouts = {{0, 0, 12, 0},
                                             {0, kInstanceStart, 12, 1},
                                             {0, 8, 12, 0},
                                             {0, kInstanceStart + 8, 12, 1}};
        if (withDivisorTwo)
        {
            layouts.push_back({0, kDivisorTwoStart, 0, 2});
        }
        return layouts;
    }

    // When not 0, the buffer or client memory of the last attribute has this many elements.
    size_t mLastAttribElementCount = 0;
    bool mClientArraysEnabled      = true;
    bool mCheckInstanceID          = false;
    bool mUseANGLEEntryPoints      = false;
    size_t mEnabledAttribCount     = 0;
    std::vector<GLuint> mPrograms;
};

class WebGLInstancedAttributeTest : public InstancedAttributeTest
{
  protected:
    WebGLInstancedAttributeTest()
    {
        setWebGLCompatibilityEnabled(true);
        setClientArraysEnabled(false);
        mClientArraysEnabled = false;
    }
};

// Per-instance attributes with a divisor of 1 in buffers.
TEST_P(InstancedAttributeTest, DivisorOneInBuffers)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 1}}, 11);
}

// Per-instance attributes with a divisor of 1 in buffers that have fewer elements than the vertices
// that the draw reads.
TEST_P(InstancedAttributeTest, DivisorOneInBuffersWithFewerInstancesThanVertices)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 1}}, 3);
}

// Per-instance attributes with a divisor of 1 in client memory.
TEST_P(InstancedAttributeTest, DivisorOneInClientMemory)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{kClientMemory, 0, 0, 0}, {kClientMemory, 0, 0, 1}, {kClientMemory, 0, 0, 1}},
                     11);
}

// Per-instance attributes with a divisor of 1 in a buffer and in client memory, with the
// per-vertex attribute in a buffer.
TEST_P(InstancedAttributeTest, DivisorOneInBufferAndClientMemory)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {kClientMemory, 0, 0, 1}, {1, 0, 0, 1}}, 11);
}

// Per-instance attributes with a divisor of 2 in a buffer, with the instance count a multiple of
// the divisor and not.
TEST_P(InstancedAttributeTest, DivisorTwoInBuffer)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 2}}, 11);
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 2}}, 12);
}

// Per-instance attributes with a divisor of 2 in client memory.
TEST_P(InstancedAttributeTest, DivisorTwoInClientMemory)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {kClientMemory, 0, 0, 2}}, 11);
}

// Per-instance attributes with a divisor of 3 in a buffer.
TEST_P(InstancedAttributeTest, DivisorThreeInBuffer)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 3}}, 11);
}

// Per-instance attributes with a divisor of 3 in client memory.
TEST_P(InstancedAttributeTest, DivisorThreeInClientMemory)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(
        {{kClientMemory, 0, 0, 0}, {kClientMemory, 0, 0, 1}, {kClientMemory, 0, 0, 3}}, 11);
}

// Per-instance attributes with the divisors 1, 2 and 3 in buffers and in client memory.
TEST_P(InstancedAttributeTest, DivisorsOneTwoAndThree)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0},
                      {1, 0, 0, 1},
                      {kClientMemory, 0, 0, 1},
                      {2, 0, 0, 2},
                      {kClientMemory, 0, 0, 2},
                      {3, 0, 0, 3},
                      {kClientMemory, 0, 0, 3}},
                     23);
}

// A divisor larger than the instance count: every instance reads the first element.
TEST_P(InstancedAttributeTest, DivisorLargerThanInstanceCount)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 16}, {kClientMemory, 0, 0, 100}}, 11);
}

// Per-vertex and per-instance attributes in one buffer: the per-vertex attributes interleaved at
// the start of the buffer, then two per-instance attributes interleaved.
TEST_P(InstancedAttributeTest, PerVertexAndPerInstanceAttributesInOneBuffer)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(perVertexAndPerInstanceLayouts(false), 11);
}

// As above, with a per-instance attribute with a divisor of 2 after the others in the buffer.
TEST_P(InstancedAttributeTest, PerVertexAndPerInstanceAttributesWithDivisorTwoInOneBuffer)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(perVertexAndPerInstanceLayouts(true), 11);
}

// A per-instance attribute in its own buffer and per-vertex attributes that share a buffer.
TEST_P(InstancedAttributeTest, PerInstanceAttributeAndSharedBufferPerVertexAttributes)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 16, 0}, {1, 0, 0, 1}, {0, 8, 16, 0}, {0, 12, 16, 0}}, 11);
}

// As above, with a per-instance attribute with a divisor of 2 in another buffer.
TEST_P(InstancedAttributeTest, DivisorTwoAndSharedBufferPerVertexAttributes)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 16, 0}, {1, 0, 0, 1}, {0, 8, 16, 0}, {0, 12, 16, 0}, {2, 0, 0, 2}},
                     11);
}

// Per-instance attributes with a divisor of 1 in a buffer and in client memory with a large
// instance count, a pixel for each instance.
TEST_P(InstancedAttributeTest, LargeInstanceCountWithDivisorOne)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {kClientMemory, 0, 0, 1}},
                     kWindowSize * kWindowSize - 5, kWindowSize);
}

// Per-instance attributes with the divisors 1, 3 and 7 in buffers and in client memory with a large
// instance count.
TEST_P(InstancedAttributeTest, LargeInstanceCount)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0},
                      {1, 0, 0, 1},
                      {kClientMemory, 0, 0, 1},
                      {2, 0, 0, 3},
                      {kClientMemory, 0, 0, 7}},
                     kWindowSize * kWindowSize - 5, kWindowSize);
}

// The divisor of an attribute in a buffer changes between 0 and 1 between draws with the same
// program.
TEST_P(InstancedAttributeTest, DivisorChangesBetweenZeroAndOne)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    for (GLuint divisor : {0u, 1u, 0u, 1u})
    {
        drawAndCheck({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, divisor}}, 11, DrawMode::Arrays);
    }
}

// The divisor of an attribute in a buffer changes between 0, 1 and larger divisors between draws
// with the same program.
TEST_P(InstancedAttributeTest, DivisorChangesBetweenDraws)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    for (GLuint divisor : {1u, 2u, 3u, 0u, 2u, 1u})
    {
        drawAndCheck({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, divisor}}, 11, DrawMode::Arrays);
    }
}

// A per-instance attribute with a divisor of 2 in a buffer that has elements for only some of the
// instances. The values of the attribute at the other instances are undefined in ES, but they are
// drawn, and the draw does not lose the other instances or the clear before it.
TEST_P(InstancedAttributeTest, DivisorTwoInBufferWithTooFewElements)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    // The reads past the end of the buffer are undefined without robust buffer access, and Metal's
    // shader validation aborts on them. WebGPU's vertex fetches are always in bounds.
    ANGLE_SKIP_TEST_IF(!IsWebGPU());
    mLastAttribElementCount = 3;
    drawAndCheck({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 2}}, 11, DrawMode::Arrays);
    drawAndCheck({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 2}}, 11, DrawMode::ElementsInBuffer);
}

// Draws with an instance count so large that the expanded per-instance attribute with a divisor
// of 2, in a buffer and in client memory, does not fit in a buffer, which fails with
// GL_OUT_OF_MEMORY, and then draws normally unless the GL_OUT_OF_MEMORY lost the context.
TEST_P(InstancedAttributeTest, HugeInstanceCount)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    // Only the WebGPU backend expands the per-instance attributes to an element for each instance.
    // The other backends would draw all the instances.
    ANGLE_SKIP_TEST_IF(!IsWebGPU());

    GLuint program = getProgram(3, false);
    ASSERT_NE(program, 0u);
    glUseProgram(program);

    std::array<std::array<GLfloat, 2>, kPadVertexCount + kQuadVertexCount> corners = {};
    std::copy(kCorners.begin(), kCorners.end(), corners.begin() + kPadVertexCount);
    GLBuffer cornerBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, cornerBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    const std::array<GLfloat, 8> cells = {};
    GLBuffer cellBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, cellBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cells), cells.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);
    vertexAttribDivisor(1, 1);

    const std::array<GLfloat, 16> values = {};
    GLBuffer valueBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, valueBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(values), values.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    vertexAttribDivisor(2, 2);
    ASSERT_GL_NO_ERROR();

    bool outOfMemoryReported = false;
    for (bool clientMemory : {false, true})
    {
        SCOPED_TRACE(testing::Message() << "client memory: " << clientMemory);
        glBindBuffer(GL_ARRAY_BUFFER, clientMemory ? 0 : valueBuffer.get());
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, clientMemory ? values.data() : nullptr);
        drawArraysInstanced(kPadVertexCount, kQuadVertexCount,
                            std::numeric_limits<GLsizei>::max());
        const GLenum error = glGetError();
        // Contexts that are lost on backend errors, such as WebGL contexts, report
        // GL_CONTEXT_LOST for the draws after the first GL_OUT_OF_MEMORY.
        if (error == GL_CONTEXT_LOST && outOfMemoryReported)
        {
            return;
        }
        EXPECT_GLENUM_EQ(GL_OUT_OF_MEMORY, error);
        outOfMemoryReported = outOfMemoryReported || error == GL_OUT_OF_MEMORY;
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    drawAndCheck({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 2}}, 11, DrawMode::Arrays);
}

// Per-instance attributes in client memory with more distinct divisors than WebGPU has vertex
// buffers.
TEST_P(InstancedAttributeTest, ManyDistinctDivisorsInClientMemory)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(distinctDivisorLayouts(getAttribCount(), true), 25);
}

// Per-instance attributes in separate buffers with more distinct divisors than WebGPU has vertex
// buffers.
TEST_P(InstancedAttributeTest, ManyDistinctDivisorsInBuffers)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(distinctDivisorLayouts(getAttribCount(), false), 25);
}

// gl_InstanceID with per-instance attributes with a divisor of 1.
TEST_P(InstancedAttributeTest, InstanceIDWithDivisorOne)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    mCheckInstanceID = true;
    for (DrawMode drawMode :
         {DrawMode::Arrays, DrawMode::ElementsInBuffer, DrawMode::ElementsInClientMemory})
    {
        drawAndCheck({{0, 0, 0, 0}, {1, 0, 0, 1}, {kClientMemory, 0, 0, 1}}, 23, drawMode);
    }
}

// gl_InstanceID with per-instance attributes with the divisors 1, 2 and 3.
TEST_P(InstancedAttributeTest, InstanceID)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    mCheckInstanceID = true;
    for (DrawMode drawMode :
         {DrawMode::Arrays, DrawMode::ElementsInBuffer, DrawMode::ElementsInClientMemory})
    {
        drawAndCheck({{0, 0, 0, 0},
                      {1, 0, 0, 1},
                      {kClientMemory, 0, 0, 1},
                      {2, 0, 0, 2},
                      {kClientMemory, 0, 0, 3}},
                     23, drawMode);
    }
}

// Per-instance attributes with a divisor of 1 in WebGL, which has no client arrays.
TEST_P(WebGLInstancedAttributeTest, DivisorOne)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 1}}, 11);
}

// Per-instance attributes with a divisor of 1 in buffers that have fewer elements than the vertices
// that the draw reads, in WebGL.
TEST_P(WebGLInstancedAttributeTest, DivisorOneWithFewerInstancesThanVertices)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 1}}, 3);
}

// Per-instance attributes with the divisors 1, 2 and 3 in WebGL.
TEST_P(WebGLInstancedAttributeTest, DivisorsOneTwoAndThree)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 1}, {3, 0, 0, 2}, {4, 0, 0, 3}}, 11);
}

// Per-vertex and per-instance attributes in one buffer in WebGL.
TEST_P(WebGLInstancedAttributeTest, PerVertexAndPerInstanceAttributesInOneBuffer)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(perVertexAndPerInstanceLayouts(false), 11);
}

// Per-vertex and per-instance attributes with a divisor of 2 in one buffer in WebGL.
TEST_P(WebGLInstancedAttributeTest, PerVertexAndPerInstanceAttributesWithDivisorTwoInOneBuffer)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(perVertexAndPerInstanceLayouts(true), 11);
}

// Per-instance attributes with a large instance count in WebGL.
TEST_P(WebGLInstancedAttributeTest, LargeInstanceCount)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes({{0, 0, 0, 0}, {1, 0, 0, 1}, {2, 0, 0, 3}}, kWindowSize * kWindowSize - 5,
                     kWindowSize);
}

// Per-instance attributes in separate buffers with more distinct divisors than WebGPU has vertex
// buffers, in WebGL.
TEST_P(WebGLInstancedAttributeTest, ManyDistinctDivisors)
{
    ANGLE_SKIP_TEST_IF(!initInstancing());
    drawWithAllModes(distinctDivisorLayouts(getAttribCount(), false), 25);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(InstancingTestES3);
ANGLE_INSTANTIATE_TEST_ES3(InstancingTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(RobustInstancingTestES3);
ANGLE_INSTANTIATE_TEST_ES3(RobustInstancingTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(InstancingTestES31);
ANGLE_INSTANTIATE_TEST_ES31(InstancingTestES31);

ANGLE_INSTANTIATE_TEST_ES2(InstancingTest);

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(InstancedAttributeTest, ES3_WEBGPU());
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(WebGLInstancedAttributeTest, ES3_WEBGPU());
