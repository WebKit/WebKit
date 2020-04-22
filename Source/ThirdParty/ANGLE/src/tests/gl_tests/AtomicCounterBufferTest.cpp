//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AtomicCounterBufferTest:
//   Various tests related for atomic counter buffers.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class AtomicCounterBufferTest : public ANGLETest
{
  protected:
    AtomicCounterBufferTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test GL_ATOMIC_COUNTER_BUFFER is not supported with version lower than ES31.
TEST_P(AtomicCounterBufferTest, AtomicCounterBufferBindings)
{
    ASSERT_EQ(3, getClientMajorVersion());
    GLBuffer atomicCounterBuffer;
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer.get());
    if (getClientMinorVersion() < 1)
    {
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }
    else
    {
        EXPECT_GL_NO_ERROR();
    }
}

class AtomicCounterBufferTest31 : public AtomicCounterBufferTest
{};

// Linking should fail if counters in vertex shader exceed gl_MaxVertexAtomicCounters.
TEST_P(AtomicCounterBufferTest31, ExceedMaxVertexAtomicCounters)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "layout(binding = 0) uniform atomic_uint foo[gl_MaxVertexAtomicCounters + 1];\n"
        "void main()\n"
        "{\n"
        "    atomicCounterIncrement(foo[0]);\n"
        "}\n";
    constexpr char kFS[] =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Counters matching across shader stages should fail if offsets aren't all specified.
// GLSL ES Spec 3.10.4, section 9.2.1.
TEST_P(AtomicCounterBufferTest31, OffsetNotAllSpecified)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "layout(binding = 0, offset = 4) uniform atomic_uint foo;\n"
        "void main()\n"
        "{\n"
        "    atomicCounterIncrement(foo);\n"
        "}\n";
    constexpr char kFS[] =
        "#version 310 es\n"
        "layout(binding = 0) uniform atomic_uint foo;\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Counters matching across shader stages should fail if offsets aren't all specified with same
// value.
TEST_P(AtomicCounterBufferTest31, OffsetNotAllSpecifiedWithSameValue)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "layout(binding = 0, offset = 4) uniform atomic_uint foo;\n"
        "void main()\n"
        "{\n"
        "    atomicCounterIncrement(foo);\n"
        "}\n";
    constexpr char kFS[] =
        "#version 310 es\n"
        "layout(binding = 0, offset = 8) uniform atomic_uint foo;\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Tests atomic counter reads using compute shaders. Used as a sanity check for the translator.
TEST_P(AtomicCounterBufferTest31, AtomicCounterReadCompute)
{
    // Skipping due to a bug on the Qualcomm Vulkan Android driver.
    // http://anglebug.com/3726
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    // Skipping due to a bug on the Adreno OpenGLES Android driver.
    // http://anglebug.com/2925
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    constexpr char kComputeShaderSource[] = R"(#version 310 es
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;

void atomicCounterInFunction(in atomic_uint counter[3]);

layout(binding = 0, offset = 8) uniform atomic_uint ac[3];

void atomicCounterInFunction(in atomic_uint counter[3])
{
    atomicCounter(counter[0]);
}

void main()
{
    atomicCounterInFunction(ac);
    atomicCounter(ac[gl_LocalInvocationIndex + 1u]);
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kComputeShaderSource);
    EXPECT_GL_NO_ERROR();
}

// Test atomic counter read.
TEST_P(AtomicCounterBufferTest31, AtomicCounterRead)
{
    // Skipping due to a bug on the Qualcomm Vulkan Android driver.
    // http://anglebug.com/3726
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    // Skipping test while we work on enabling atomic counter buffer support in th D3D renderer.
    // http://anglebug.com/1729
    ANGLE_SKIP_TEST_IF(IsD3D11());

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 0, offset = 4) uniform atomic_uint ac;\n"
        "out highp vec4 my_color;\n"
        "void main()\n"
        "{\n"
        "    my_color = vec4(0.0);\n"
        "    uint a1 = atomicCounter(ac);\n"
        "    if (a1 == 3u) my_color = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);

    glUseProgram(program.get());

    // The initial value of counter 'ac' is 3u.
    unsigned int bufferData[3] = {11u, 3u, 1u};
    GLBuffer atomicCounterBuffer;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(bufferData), bufferData, GL_STATIC_DRAW);

    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer);

    drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
}

// Test atomic counter increment and decrement.
TEST_P(AtomicCounterBufferTest31, AtomicCounterIncrementAndDecrement)
{
    // Skipping due to a bug on the Qualcomm Vulkan Android driver.
    // http://anglebug.com/3726
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kCS[] =
        "#version 310 es\n"
        "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
        "layout(binding = 0, offset = 4) uniform atomic_uint ac[2];\n"
        "void main()\n"
        "{\n"
        "    atomicCounterIncrement(ac[0]);\n"
        "    atomicCounterDecrement(ac[1]);\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);

    glUseProgram(program.get());

    // The initial value of 'ac[0]' is 3u, 'ac[1]' is 1u.
    unsigned int bufferData[3] = {11u, 3u, 1u};
    GLBuffer atomicCounterBuffer;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(bufferData), bufferData, GL_STATIC_DRAW);

    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer);

    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    void *mappedBuffer =
        glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 3, GL_MAP_READ_BIT);
    memcpy(bufferData, mappedBuffer, sizeof(bufferData));
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

    EXPECT_EQ(11u, bufferData[0]);
    EXPECT_EQ(4u, bufferData[1]);
    EXPECT_EQ(0u, bufferData[2]);
}

// Tests multiple atomic counter buffers.
TEST_P(AtomicCounterBufferTest31, AtomicCounterMultipleBuffers)
{
    // Skipping due to a bug on the Qualcomm Vulkan Android driver.
    // http://anglebug.com/3726
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    GLint maxAtomicCounterBuffers = 0;
    glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS, &maxAtomicCounterBuffers);
    constexpr unsigned int kBufferCount = 3;
    // ES 3.1 table 20.45 only guarantees 1 atomic counter buffer
    ANGLE_SKIP_TEST_IF(maxAtomicCounterBuffers < static_cast<int>(kBufferCount));

    constexpr char kComputeShaderSource[] = R"(#version 310 es
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(binding = 0) uniform atomic_uint ac1;
layout(binding = 1) uniform atomic_uint ac2;
layout(binding = 2) uniform atomic_uint ac3;

void main()
{
    atomicCounterIncrement(ac1);
    atomicCounterIncrement(ac2);
    atomicCounterIncrement(ac3);
})";

    ANGLE_GL_COMPUTE_PROGRAM(program, kComputeShaderSource);

    glUseProgram(program);

    GLBuffer atomicCounterBuffers[kBufferCount];

    for (unsigned int ii = 0; ii < kBufferCount; ++ii)
    {
        GLuint initialData[1] = {ii};
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffers[ii]);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(initialData), initialData, GL_STATIC_DRAW);

        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, ii, atomicCounterBuffers[ii]);
    }

    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    for (unsigned int ii = 0; ii < kBufferCount; ++ii)
    {
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffers[ii]);
        GLuint *mappedBuffer = static_cast<GLuint *>(
            glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT));
        EXPECT_EQ(ii + 1, mappedBuffer[0]);
        glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    }
}

// Test atomic counter array of array.
TEST_P(AtomicCounterBufferTest31, AtomicCounterArrayOfArray)
{
    // Fails on D3D.  Some counters are double-incremented while some are untouched, hinting at a
    // bug in index translation.  http://anglebug.com/3783
    ANGLE_SKIP_TEST_IF(IsD3D11());

    // Nvidia's OpenGL driver fails to compile the shader.  http://anglebug.com/3791
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsNVIDIA());

    // Intel's Windows OpenGL driver crashes in this test.  http://anglebug.com/3791
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsIntel() && IsWindows());

    // Skipping due to a bug on the Qualcomm Vulkan Android driver.
    // http://anglebug.com/3726
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    constexpr char kCS[] = R"(#version 310 es
layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(binding = 0) uniform atomic_uint ac[7][5][3];

void f0(in atomic_uint ac)
{
    atomicCounterIncrement(ac);
}

void f1(in atomic_uint ac[3])
{
    atomicCounterIncrement(ac[0]);
    f0(ac[1]);
    int index = 2;
    f0(ac[index]);
}

void f2(in atomic_uint ac[5][3])
{
    // Increment all in ac[0], ac[1] and ac[2]
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            f0(ac[i][j]);
        }
        f0(ac[i][2]);
    }

    // Increment all in ac[3]
    f1(ac[3]);

    // Increment all in ac[4]
    for (int i = 0; i < 2; ++i)
    {
        atomicCounterIncrement(ac[4][i]);
    }
    f0(ac[4][2]);
}

void f3(in atomic_uint ac[7][5][3])
{
    // Increment all in ac[0], ac[1], ac[2] and ac[3]
    f2(ac[0]);
    for (int i = 1; i < 4; ++i)
    {
        f2(ac[i]);
    }

    // Increment all in ac[5][0], ac[5][1], ac[5][2] and ac[5][3]
    for (int i = 0; i < 4; ++i)
    {
        f1(ac[5][i]);
    }

    // Increment all in ac[5][4][0], ac[5][4][1] and ac[5][4][2]
    f0(ac[5][4][0]);
    for (int i = 1; i < 3; ++i)
    {
        f0(ac[5][4][i]);
    }

    // Increment all in ac[6]
    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            atomicCounterIncrement(ac[6][i][j]);
        }
        atomicCounterIncrement(ac[6][i][2]);
    }
}

void main()
{
    // Increment all in ac except ac[4]
    f3(ac);

    // Increment all in ac[4]
    f2(ac[4]);
})";

    constexpr uint32_t kAtomicCounterRows  = 7;
    constexpr uint32_t kAtomicCounterCols  = 5;
    constexpr uint32_t kAtomicCounterDepth = 3;
    constexpr uint32_t kAtomicCounterCount =
        kAtomicCounterRows * kAtomicCounterCols * kAtomicCounterDepth;

    GLint maxAtomicCounters = 0;
    glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTERS, &maxAtomicCounters);
    EXPECT_GL_NO_ERROR();

    // Required minimum is 8 by the spec
    EXPECT_GE(maxAtomicCounters, 8);
    ANGLE_SKIP_TEST_IF(static_cast<uint32_t>(maxAtomicCounters) < kAtomicCounterCount);

    ANGLE_GL_COMPUTE_PROGRAM(program, kCS);
    glUseProgram(program.get());

    // The initial value of atomic counters is 0, 1, 2, ...
    unsigned int bufferData[kAtomicCounterCount] = {};
    for (uint32_t index = 0; index < kAtomicCounterCount; ++index)
    {
        bufferData[index] = index;
    }

    GLBuffer atomicCounterBuffer;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(bufferData), bufferData, GL_STATIC_DRAW);

    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer);

    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    unsigned int result[kAtomicCounterCount] = {};
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    void *mappedBuffer =
        glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(bufferData), GL_MAP_READ_BIT);
    memcpy(result, mappedBuffer, sizeof(bufferData));
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

    for (uint32_t index = 0; index < kAtomicCounterCount; ++index)
    {
        EXPECT_EQ(result[index], bufferData[index] + 1) << "index " << index;
    }
}

// TODO(syoussefi): re-enable tests on Vulkan once http://anglebug.com/3738 is resolved.  The issue
// is with WGL where if a Vulkan test is run first in the shard, it causes crashes when an OpenGL
// test is run afterwards.  AtomicCounter* tests are alphabetically first, and having them not run
// on Vulkan makes every shard our bots currently make do have at least some OpenGL test run before
// any Vulkan test. When these tests can be enabled on Vulkan, can replace the current macros with
// the updated macros below that include Vulkan:
#if !defined(ANGLE_PLATFORM_WINDOWS)
ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(AtomicCounterBufferTest);
ANGLE_INSTANTIATE_TEST_ES31(AtomicCounterBufferTest31);
#else
ANGLE_INSTANTIATE_TEST(AtomicCounterBufferTest,
                       ES3_OPENGL(),
                       ES3_OPENGLES(),
                       ES31_OPENGL(),
                       ES31_OPENGLES(),
                       ES31_D3D11());
ANGLE_INSTANTIATE_TEST(AtomicCounterBufferTest31, ES31_OPENGL(), ES31_OPENGLES(), ES31_D3D11());
#endif

}  // namespace
