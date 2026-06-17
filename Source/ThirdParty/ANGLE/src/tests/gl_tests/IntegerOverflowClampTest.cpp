//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IntegerOverflowClampTest: Verifies that ANGLE_int_clamp array-bounds guards
// survive integer UB operations in the Metal shader compiler's optimizer.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class IntegerOverflowClampTest : public ANGLETest<>
{
  protected:
    static constexpr uint32_t kIntMin = static_cast<uint32_t>(std::numeric_limits<int32_t>::min());

    IntegerOverflowClampTest()
    {
        setWindowWidth(4);
        setWindowHeight(1);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void runShader(const char *vs, const uint32_t *data, size_t dataSize, uint32_t expected)
    {
        constexpr char kFS[] = R"(#version 300 es
precision highp float;
void main() { }
)";

        std::vector<std::string> varyings = {"result"};
        GLuint program =
            CompileProgramWithTransformFeedback(vs, kFS, varyings, GL_SEPARATE_ATTRIBS);
        ASSERT_NE(0u, program);

        glUseProgram(program);

        GLBuffer ubo;
        glUniformBlockBinding(program, glGetUniformBlockIndex(program, "Data"), 0);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, dataSize, data, GL_STATIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

        GLBuffer transformFeedbackBuffer;
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, transformFeedbackBuffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(uint32_t), nullptr, GL_STATIC_DRAW);

        GLTransformFeedback transformFeedback;
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedback);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, transformFeedbackBuffer);

        glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, 1);
        glEndTransformFeedback();
        glDisable(GL_RASTERIZER_DISCARD);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        ASSERT_GL_NO_ERROR();

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, transformFeedbackBuffer);
        const void *mapped =
            glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(uint32_t), GL_MAP_READ_BIT);
        ASSERT_NE(nullptr, mapped);
        uint32_t actual = *static_cast<const uint32_t *>(mapped);
        glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);

        EXPECT_EQ(expected, actual)
            << "Got 0x" << std::hex << actual << ", expected 0x" << expected;

        glDeleteProgram(program);
    }
};

// Test that negating INT_MIN does not let the optimizer fold away array bounds clamping.
TEST_P(IntegerOverflowClampTest, NegateIntMin)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp int;
flat out uint result;
uniform Data { uvec4 values[2]; } ubo;
void main() {
    int value = int(ubo.values[0].x);
    int index = value;
    if (value < 0)
        index = -value;
    result = uint(index) ^ ubo.values[index].x;
})";

    std::vector<uint32_t> data(32, kIntMin);
    runShader(kVS, data.data(), data.size() * sizeof(uint32_t), 0u);
}

// Test that dividing INT_MIN by -1 does not let the optimizer fold away array bounds clamping.
TEST_P(IntegerOverflowClampTest, DivByMinusOne)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp int;
flat out uint result;
uniform Data { uvec4 values[2]; } ubo;
void main() {
    int value = int(ubo.values[0].x);
    int index = value;
    if (value < 0)
        index = value / -1;
    result = uint(index) ^ ubo.values[index].x;
})";

    std::vector<uint32_t> data(32, kIntMin);
    runShader(kVS, data.data(), data.size() * sizeof(uint32_t), 0u);
}

// Test that INT_MIN mod -1 does not let the optimizer fold away array bounds clamping.
TEST_P(IntegerOverflowClampTest, ModByMinusOne)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp int;
flat out uint result;
uniform Data { uvec4 values[2]; } ubo;
void main() {
    int value = int(ubo.values[0].x);
    int index = value % -1;
    result = uint(index) ^ ubo.values[index].x;
})";

    std::vector<uint32_t> data(32, kIntMin);
    runShader(kVS, data.data(), data.size() * sizeof(uint32_t), kIntMin);
}

// Test that unsigned divide-by-zero does not let the optimizer fold away array bounds clamping.
TEST_P(IntegerOverflowClampTest, UnsignedDivByZero)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp int;
flat out uint result;
uniform Data { uvec4 values[2]; } ubo;
void main() {
    uint value = ubo.values[0].x;
    uint index = 1u / (value ^ 0x80000000u);
    result = index ^ ubo.values[index].x;
})";

    std::vector<uint32_t> data(32, kIntMin);
    runShader(kVS, data.data(), data.size() * sizeof(uint32_t), kIntMin | 1u);
}

// Test that unsigned mod-by-zero does not let the optimizer fold away array bounds clamping.
TEST_P(IntegerOverflowClampTest, UnsignedModByZero)
{
    constexpr char kVS[] = R"(#version 300 es
precision highp int;
flat out uint result;
uniform Data { uvec4 values[2]; } ubo;
void main() {
    uint value = ubo.values[0].x;
    uint index = 7u % (value ^ 0x80000000u);
    result = index ^ ubo.values[index].x;
})";

    std::vector<uint32_t> data(32, kIntMin);
    runShader(kVS, data.data(), data.size() * sizeof(uint32_t), kIntMin);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IntegerOverflowClampTest);
ANGLE_INSTANTIATE_TEST(IntegerOverflowClampTest, ES3_METAL());

}  // namespace
