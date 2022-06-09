//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include <array>
#include <cmath>
#include <sstream>

using namespace angle;

namespace
{

class SimpleUniformTest : public ANGLETest
{
  protected:
    SimpleUniformTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test that we can get and set a float uniform successfully.
TEST_P(SimpleUniformTest, FloatUniformStateQuery)
{
    constexpr char kFragShader[] = R"(precision mediump float;
uniform float uniF;
void main() {
    gl_FragColor = vec4(uniF, 0.0, 0.0, 0.0);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);
    GLint uniformLocation = glGetUniformLocation(program, "uniF");
    ASSERT_NE(uniformLocation, -1);

    GLfloat expected = 1.02f;
    glUniform1f(uniformLocation, expected);

    GLfloat f = 0.0f;
    glGetUniformfv(program, uniformLocation, &f);
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(f, expected);
}

// Test that we can get and set an int uniform successfully.
TEST_P(SimpleUniformTest, IntUniformStateQuery)
{
    constexpr char kFragShader[] = R"(uniform int uniI;
void main() {
    gl_FragColor = vec4(uniI, 0.0, 0.0, 0.0);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);

    GLint uniformLocation = glGetUniformLocation(program, "uniI");
    ASSERT_NE(uniformLocation, -1);

    GLint expected = 4;
    glUniform1i(uniformLocation, expected);

    GLint i = 0;
    glGetUniformiv(program, uniformLocation, &i);
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(i, expected);
}

// Test that we can get and set a vec2 uniform successfully.
TEST_P(SimpleUniformTest, FloatVec2UniformStateQuery)
{
    constexpr char kFragShader[] = R"(precision mediump float;
uniform vec2 uniVec2;
void main() {
    gl_FragColor = vec4(uniVec2, 0.0, 0.0);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);

    GLint uniformLocation = glGetUniformLocation(program, "uniVec2");
    ASSERT_NE(uniformLocation, -1);

    std::vector<GLfloat> expected = {{1.0f, 0.5f}};
    glUniform2fv(uniformLocation, 1, expected.data());

    std::vector<GLfloat> floats(2, 0);
    glGetUniformfv(program, uniformLocation, floats.data());
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(floats, expected);
}

// Test that we can get and set a vec3 uniform successfully.
TEST_P(SimpleUniformTest, FloatVec3UniformStateQuery)
{
    constexpr char kFragShader[] = R"(precision mediump float;
uniform vec3 uniVec3;
void main() {
    gl_FragColor = vec4(uniVec3, 0.0);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);

    GLint uniformLocation = glGetUniformLocation(program, "uniVec3");
    ASSERT_NE(uniformLocation, -1);

    std::vector<GLfloat> expected = {{1.0f, 0.5f, 0.2f}};
    glUniform3fv(uniformLocation, 1, expected.data());

    std::vector<GLfloat> floats(3, 0);
    glGetUniformfv(program, uniformLocation, floats.data());
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(floats, expected);
}

// Test that we can get and set a vec4 uniform successfully.
TEST_P(SimpleUniformTest, FloatVec4UniformStateQuery)
{
    constexpr char kFragShader[] = R"(precision mediump float;
uniform vec4 uniVec4;
void main() {
    gl_FragColor = uniVec4;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);

    GLint uniformLocation = glGetUniformLocation(program, "uniVec4");
    ASSERT_NE(uniformLocation, -1);

    std::vector<GLfloat> expected = {{1.0f, 0.5f, 0.2f, -0.8f}};
    glUniform4fv(uniformLocation, 1, expected.data());

    std::vector<GLfloat> floats(4, 0);
    glGetUniformfv(program, uniformLocation, floats.data());
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(floats, expected);
}

// Test that we can get and set a 2x2 float Matrix uniform successfully.
TEST_P(SimpleUniformTest, FloatMatrix2UniformStateQuery)
{
    constexpr char kFragShader[] = R"(precision mediump float;
uniform mat2 umat2;
void main() {
    gl_FragColor = vec4(umat2);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);

    GLint uniformLocation = glGetUniformLocation(program, "umat2");
    ASSERT_NE(uniformLocation, -1);

    std::vector<GLfloat> expected = {{1.0f, 0.5f, 0.2f, -0.8f}};
    glUniformMatrix2fv(uniformLocation, 1, false, expected.data());

    std::vector<GLfloat> floats(4, 0);
    glGetUniformfv(program, uniformLocation, floats.data());
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(floats, expected);
}

// Test that we can get and set a 3x3 float Matrix uniform successfully.
TEST_P(SimpleUniformTest, FloatMatrix3UniformStateQuery)
{
    constexpr char kFragShader[] = R"(precision mediump float;
uniform mat3 umat3;
void main() {
    gl_FragColor = vec4(umat3);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);

    GLint uniformLocation = glGetUniformLocation(program, "umat3");
    ASSERT_NE(uniformLocation, -1);

    std::vector<GLfloat> expected = {{1.0f, 0.5f, 0.2f, -0.8f, -0.2f, 0.1f, 0.1f, 0.2f, 0.7f}};
    glUniformMatrix3fv(uniformLocation, 1, false, expected.data());

    std::vector<GLfloat> floats(9, 0);
    glGetUniformfv(program, uniformLocation, floats.data());
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(floats, expected);
}

// Test that we can get and set a 4x4 float Matrix uniform successfully.
TEST_P(SimpleUniformTest, FloatMatrix4UniformStateQuery)
{
    constexpr char kFragShader[] = R"(precision mediump float;
uniform mat4 umat4;
void main() {
    gl_FragColor = umat4 * vec4(1.0, 1.0, 1.0, 1.0);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);

    GLint uniformLocation = glGetUniformLocation(program, "umat4");
    ASSERT_NE(uniformLocation, -1);

    std::vector<GLfloat> expected = {{1.0f, 0.5f, 0.2f, -0.8f, -0.2f, 0.1f, 0.1f, 0.2f, 0.7f, 0.1f,
                                      0.7f, 0.1f, 0.7f, 0.1f, 0.7f, 0.1f}};
    glUniformMatrix4fv(uniformLocation, 1, false, expected.data());

    std::vector<GLfloat> floats(16, 0);
    glGetUniformfv(program, uniformLocation, floats.data());
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(floats, expected);
}

// Test that we can get and set a float array of uniforms.
TEST_P(SimpleUniformTest, FloatArrayUniformStateQuery)
{

    constexpr char kFragShader[] = R"(
precision mediump float;
uniform float ufloats[4];
void main() {
    gl_FragColor = vec4(ufloats[0], ufloats[1], ufloats[2], ufloats[3]);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);
    std::vector<GLfloat> expected = {{0.1f, 0.2f, 0.3f, 0.4f}};

    for (size_t i = 0; i < expected.size(); i++)
    {
        std::string locationName = "ufloats[" + std::to_string(i) + "]";
        GLint uniformLocation    = glGetUniformLocation(program, locationName.c_str());
        glUniform1f(uniformLocation, expected[i]);
        ASSERT_GL_NO_ERROR();
        ASSERT_NE(uniformLocation, -1);

        GLfloat result = 0;
        glGetUniformfv(program, uniformLocation, &result);
        ASSERT_GL_NO_ERROR();
        ASSERT_EQ(result, expected[i]);
    }
}

// Test that we can get and set an array of matrices uniform.
TEST_P(SimpleUniformTest, ArrayOfMat3UniformStateQuery)
{
    constexpr char kFragShader[] = R"(
precision mediump float;
uniform mat3 umatarray[2];
void main() {
    gl_FragColor = vec4(umatarray[1]);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);
    std::vector<std::vector<GLfloat>> expected = {
        {1.0f, 0.5f, 0.2f, -0.8f, -0.2f, 0.1f, 0.1f, 0.2f, 0.7f},
        {0.9f, 0.4f, 0.1f, -0.9f, -0.3f, 0.0f, 0.0f, 0.1f, 0.6f}};

    for (size_t i = 0; i < expected.size(); i++)
    {
        std::string locationName = "umatarray[" + std::to_string(i) + "]";
        GLint uniformLocation    = glGetUniformLocation(program, locationName.c_str());
        glUniformMatrix3fv(uniformLocation, 1, false, expected[i].data());
        ASSERT_GL_NO_ERROR();
        ASSERT_NE(uniformLocation, -1);

        std::vector<GLfloat> results(9, 0);
        glGetUniformfv(program, uniformLocation, results.data());
        ASSERT_GL_NO_ERROR();
        ASSERT_EQ(results, expected[i]);
    }
}

// Test that we can get and set an int array of uniforms.
TEST_P(SimpleUniformTest, FloatIntUniformStateQuery)
{

    constexpr char kFragShader[] = R"(
precision mediump float;
uniform int uints[4];
void main() {
    gl_FragColor = vec4(uints[0], uints[1], uints[2], uints[3]);
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Zero(), kFragShader);
    glUseProgram(program);
    std::vector<GLint> expected = {{1, 2, 3, 4}};

    for (size_t i = 0; i < expected.size(); i++)
    {
        std::string locationName = "uints[" + std::to_string(i) + "]";
        GLint uniformLocation    = glGetUniformLocation(program, locationName.c_str());
        glUniform1i(uniformLocation, expected[i]);
        ASSERT_GL_NO_ERROR();
        ASSERT_NE(uniformLocation, -1);

        GLint result = 0;
        glGetUniformiv(program, uniformLocation, &result);
        ASSERT_GL_NO_ERROR();
        ASSERT_EQ(result, expected[i]);
    }
}

class UniformTest : public ANGLETest
{
  protected:
    UniformTest() : mProgram(0), mUniformFLocation(-1), mUniformILocation(-1), mUniformBLocation(-1)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        // TODO(anglebug.com/5505): asserting with latest direct-to-Metal compiler
        // changes. Must skip all tests explicitly.
        // if (IsMetal())
        //    return;

        constexpr char kVS[] = "void main() { gl_Position = vec4(1); }";
        constexpr char kFS[] =
            "precision mediump float;\n"
            "uniform float uniF;\n"
            "uniform int uniI;\n"
            "uniform bool uniB;\n"
            "uniform bool uniBArr[4];\n"
            "void main() {\n"
            "  gl_FragColor = vec4(uniF + float(uniI));\n"
            "  gl_FragColor += vec4(uniB ? 1.0 : 0.0);\n"
            "  gl_FragColor += vec4(uniBArr[0] ? 1.0 : 0.0);\n"
            "  gl_FragColor += vec4(uniBArr[1] ? 1.0 : 0.0);\n"
            "  gl_FragColor += vec4(uniBArr[2] ? 1.0 : 0.0);\n"
            "  gl_FragColor += vec4(uniBArr[3] ? 1.0 : 0.0);\n"
            "}";

        mProgram = CompileProgram(kVS, kFS);
        ASSERT_NE(mProgram, 0u);

        mUniformFLocation = glGetUniformLocation(mProgram, "uniF");
        ASSERT_NE(mUniformFLocation, -1);

        mUniformILocation = glGetUniformLocation(mProgram, "uniI");
        ASSERT_NE(mUniformILocation, -1);

        mUniformBLocation = glGetUniformLocation(mProgram, "uniB");
        ASSERT_NE(mUniformBLocation, -1);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    GLuint mProgram;
    GLint mUniformFLocation;
    GLint mUniformILocation;
    GLint mUniformBLocation;
};

TEST_P(UniformTest, GetUniformNoCurrentProgram)
{

    glUseProgram(mProgram);
    glUniform1f(mUniformFLocation, 1.0f);
    glUniform1i(mUniformILocation, 1);
    glUseProgram(0);

    GLfloat f;
    glGetnUniformfvEXT(mProgram, mUniformFLocation, 4, &f);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1.0f, f);

    glGetUniformfv(mProgram, mUniformFLocation, &f);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1.0f, f);

    GLint i;
    glGetnUniformivEXT(mProgram, mUniformILocation, 4, &i);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, i);

    glGetUniformiv(mProgram, mUniformILocation, &i);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, i);
}

TEST_P(UniformTest, UniformArrayLocations)
{

    constexpr char kVS[] = R"(precision mediump float;
uniform float uPosition[4];
void main(void)
{
    gl_Position = vec4(uPosition[0], uPosition[1], uPosition[2], uPosition[3]);
})";

    constexpr char kFS[] = R"(precision mediump float;
uniform float uColor[4];
void main(void)
{
    gl_FragColor = vec4(uColor[0], uColor[1], uColor[2], uColor[3]);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    // Array index zero should be equivalent to the un-indexed uniform
    EXPECT_NE(-1, glGetUniformLocation(program, "uPosition"));
    EXPECT_EQ(glGetUniformLocation(program, "uPosition"),
              glGetUniformLocation(program, "uPosition[0]"));

    EXPECT_NE(-1, glGetUniformLocation(program, "uColor"));
    EXPECT_EQ(glGetUniformLocation(program, "uColor"), glGetUniformLocation(program, "uColor[0]"));

    // All array uniform locations should be unique
    GLint positionLocations[4] = {
        glGetUniformLocation(program, "uPosition[0]"),
        glGetUniformLocation(program, "uPosition[1]"),
        glGetUniformLocation(program, "uPosition[2]"),
        glGetUniformLocation(program, "uPosition[3]"),
    };

    GLint colorLocations[4] = {
        glGetUniformLocation(program, "uColor[0]"),
        glGetUniformLocation(program, "uColor[1]"),
        glGetUniformLocation(program, "uColor[2]"),
        glGetUniformLocation(program, "uColor[3]"),
    };

    for (size_t i = 0; i < 4; i++)
    {
        EXPECT_NE(-1, positionLocations[i]);
        EXPECT_NE(-1, colorLocations[i]);

        for (size_t j = i + 1; j < 4; j++)
        {
            EXPECT_NE(positionLocations[i], positionLocations[j]);
            EXPECT_NE(colorLocations[i], colorLocations[j]);
        }
    }

    glDeleteProgram(program);
}

// Test that float to integer GetUniform rounds values correctly.
TEST_P(UniformTest, FloatUniformStateQuery)
{

    std::vector<double> inValues;
    std::vector<GLfloat> expectedFValues;
    std::vector<GLint> expectedIValues;

    double intMaxD = static_cast<double>(std::numeric_limits<GLint>::max());
    double intMinD = static_cast<double>(std::numeric_limits<GLint>::min());

    // TODO(jmadill): Investigate rounding of .5
    inValues.push_back(-1.0);
    inValues.push_back(-0.6);
    // inValues.push_back(-0.5); // undefined behaviour?
    inValues.push_back(-0.4);
    inValues.push_back(0.0);
    inValues.push_back(0.4);
    // inValues.push_back(0.5); // undefined behaviour?
    inValues.push_back(0.6);
    inValues.push_back(1.0);
    inValues.push_back(999999.2);
    inValues.push_back(intMaxD * 2.0);
    inValues.push_back(intMaxD + 1.0);
    inValues.push_back(intMinD * 2.0);
    inValues.push_back(intMinD - 1.0);

    for (double value : inValues)
    {
        expectedFValues.push_back(static_cast<GLfloat>(value));

        double clampedValue = std::max(intMinD, std::min(intMaxD, value));
        double rounded      = round(clampedValue);
        expectedIValues.push_back(static_cast<GLint>(rounded));
    }

    glUseProgram(mProgram);
    ASSERT_GL_NO_ERROR();

    for (size_t index = 0; index < inValues.size(); ++index)
    {
        GLfloat inValue       = static_cast<GLfloat>(inValues[index]);
        GLfloat expectedValue = expectedFValues[index];

        glUniform1f(mUniformFLocation, inValue);
        GLfloat testValue;
        glGetUniformfv(mProgram, mUniformFLocation, &testValue);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(expectedValue, testValue);
    }

    for (size_t index = 0; index < inValues.size(); ++index)
    {
        GLfloat inValue     = static_cast<GLfloat>(inValues[index]);
        GLint expectedValue = expectedIValues[index];

        glUniform1f(mUniformFLocation, inValue);
        GLint testValue;
        glGetUniformiv(mProgram, mUniformFLocation, &testValue);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(expectedValue, testValue);
    }
}

// Test that integer to float GetUniform rounds values correctly.
TEST_P(UniformTest, IntUniformStateQuery)
{
    // Qualcomm seems to have a bug where integer uniforms are internally stored as float, and
    // large values are rounded to the nearest float representation of an integer.
    // TODO(jmadill): Lift this suppression when/if the bug is fixed.
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    std::vector<GLint> inValues;
    std::vector<GLint> expectedIValues;
    std::vector<GLfloat> expectedFValues;

    GLint intMax = std::numeric_limits<GLint>::max();
    GLint intMin = std::numeric_limits<GLint>::min();

    inValues.push_back(-1);
    inValues.push_back(0);
    inValues.push_back(1);
    inValues.push_back(999999);
    inValues.push_back(intMax);
    inValues.push_back(intMax - 1);
    inValues.push_back(intMin);
    inValues.push_back(intMin + 1);

    for (GLint value : inValues)
    {
        expectedIValues.push_back(value);
        expectedFValues.push_back(static_cast<GLfloat>(value));
    }

    glUseProgram(mProgram);
    ASSERT_GL_NO_ERROR();

    for (size_t index = 0; index < inValues.size(); ++index)
    {
        GLint inValue       = inValues[index];
        GLint expectedValue = expectedIValues[index];

        glUniform1i(mUniformILocation, inValue);
        GLint testValue = 1234567;
        glGetUniformiv(mProgram, mUniformILocation, &testValue);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(expectedValue, testValue) << " with glGetUniformiv";
    }

    for (size_t index = 0; index < inValues.size(); ++index)
    {
        GLint inValue         = inValues[index];
        GLfloat expectedValue = expectedFValues[index];

        glUniform1i(mUniformILocation, inValue);
        GLfloat testValue = 124567.0;
        glGetUniformfv(mProgram, mUniformILocation, &testValue);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(expectedValue, testValue) << " with glGetUniformfv";
    }
}

// Test that queries of boolean uniforms round correctly.
TEST_P(UniformTest, BooleanUniformStateQuery)
{

    glUseProgram(mProgram);
    GLint intValue     = 0;
    GLfloat floatValue = 0.0f;

    // Calling Uniform1i
    glUniform1i(mUniformBLocation, GL_FALSE);

    glGetUniformiv(mProgram, mUniformBLocation, &intValue);
    EXPECT_EQ(0, intValue);

    glGetUniformfv(mProgram, mUniformBLocation, &floatValue);
    EXPECT_EQ(0.0f, floatValue);

    glUniform1i(mUniformBLocation, GL_TRUE);

    glGetUniformiv(mProgram, mUniformBLocation, &intValue);
    EXPECT_EQ(1, intValue);

    glGetUniformfv(mProgram, mUniformBLocation, &floatValue);
    EXPECT_EQ(1.0f, floatValue);

    // Calling Uniform1f
    glUniform1f(mUniformBLocation, 0.0f);

    glGetUniformiv(mProgram, mUniformBLocation, &intValue);
    EXPECT_EQ(0, intValue);

    glGetUniformfv(mProgram, mUniformBLocation, &floatValue);
    EXPECT_EQ(0.0f, floatValue);

    glUniform1f(mUniformBLocation, 1.0f);

    glGetUniformiv(mProgram, mUniformBLocation, &intValue);
    EXPECT_EQ(1, intValue);

    glGetUniformfv(mProgram, mUniformBLocation, &floatValue);
    EXPECT_EQ(1.0f, floatValue);

    ASSERT_GL_NO_ERROR();
}

// Test queries for arrays of boolean uniforms.
TEST_P(UniformTest, BooleanArrayUniformStateQuery)
{

    glUseProgram(mProgram);
    GLint boolValuesi[4]   = {0, 1, 0, 1};
    GLfloat boolValuesf[4] = {0, 1, 0, 1};

    GLint locations[4] = {
        glGetUniformLocation(mProgram, "uniBArr"),
        glGetUniformLocation(mProgram, "uniBArr[1]"),
        glGetUniformLocation(mProgram, "uniBArr[2]"),
        glGetUniformLocation(mProgram, "uniBArr[3]"),
    };

    for (int i = 0; i < 4; ++i)
    {
        ASSERT_NE(-1, locations[i]) << " with i=" << i;
    }

    // Calling Uniform1iv
    glUniform1iv(locations[0], 4, boolValuesi);

    for (unsigned int idx = 0; idx < 4; ++idx)
    {
        int value = -1;
        glGetUniformiv(mProgram, locations[idx], &value);
        EXPECT_EQ(boolValuesi[idx], value) << " with Uniform1iv/GetUniformiv at " << idx;
    }

    for (unsigned int idx = 0; idx < 4; ++idx)
    {
        float value = -1.0f;
        glGetUniformfv(mProgram, locations[idx], &value);
        EXPECT_EQ(boolValuesf[idx], value) << " with Uniform1iv/GetUniformfv at " << idx;
    }

    // Calling Uniform1fv
    glUniform1fv(locations[0], 4, boolValuesf);

    for (unsigned int idx = 0; idx < 4; ++idx)
    {
        int value = -1;
        glGetUniformiv(mProgram, locations[idx], &value);
        EXPECT_EQ(boolValuesi[idx], value) << " with Uniform1fv/GetUniformiv at " << idx;
    }

    for (unsigned int idx = 0; idx < 4; ++idx)
    {
        float value = -1.0f;
        glGetUniformfv(mProgram, locations[idx], &value);
        EXPECT_EQ(boolValuesf[idx], value) << " with Uniform1fv/GetUniformfv at " << idx;
    }

    ASSERT_GL_NO_ERROR();
}

class UniformTestES3 : public ANGLETest
{
  protected:
    UniformTestES3() : mProgram(0) {}

    void testTearDown() override
    {
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
    }

    GLuint mProgram;
};

// Test that we can get and set an array of matrices uniform.
TEST_P(UniformTestES3, MatrixArrayUniformStateQuery)
{
    constexpr char kFragShader[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform mat3x4 uniMat3x4[5];\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(uniMat3x4[0]);\n"
        "    fragColor += vec4(uniMat3x4[1]);\n"
        "    fragColor += vec4(uniMat3x4[2]);\n"
        "    fragColor += vec4(uniMat3x4[3]);\n"
        "    fragColor += vec4(uniMat3x4[4]);\n"
        "}\n";
    constexpr unsigned int kArrayCount   = 5;
    constexpr unsigned int kMatrixStride = 3 * 4;

    mProgram = CompileProgram(essl3_shaders::vs::Zero(), kFragShader);
    ASSERT_NE(mProgram, 0u);

    glUseProgram(mProgram);
    GLfloat expected[kArrayCount][kMatrixStride] = {
        {0.6f, -0.4f, 0.6f, 0.9f, -0.6f, 0.3f, -0.3f, -0.1f, -0.4f, -0.3f, 0.7f, 0.1f},
        {-0.4f, -0.4f, -0.5f, -0.7f, 0.1f, -0.5f, 0.0f, -0.9f, -0.4f, 0.8f, -0.6f, 0.9f},
        {0.4f, 0.1f, -0.9f, 1.0f, -0.8f, 0.4f, -0.2f, 0.4f, -0.0f, 0.2f, 0.9f, -0.3f},
        {0.5f, 0.7f, -0.0f, 1.0f, 0.7f, 0.7f, 0.7f, -0.7f, -0.8f, 0.6f, 0.5f, -0.2f},
        {-1.0f, 0.8f, 1.0f, -0.4f, 0.7f, 0.5f, 0.5f, 0.8f, 0.6f, 0.1f, 0.4f, -0.9f}};

    GLint baseLocation = glGetUniformLocation(mProgram, "uniMat3x4");
    ASSERT_NE(-1, baseLocation);

    glUniformMatrix3x4fv(baseLocation, kArrayCount, GL_FALSE, &expected[0][0]);

    for (size_t i = 0; i < kArrayCount; i++)
    {
        std::stringstream nameStr;
        nameStr << "uniMat3x4[" << i << "]";
        std::string name = nameStr.str();
        GLint location   = glGetUniformLocation(mProgram, name.c_str());
        ASSERT_GL_NO_ERROR();
        ASSERT_NE(-1, location);

        std::vector<GLfloat> results(12, 0);
        glGetUniformfv(mProgram, location, results.data());
        ASSERT_GL_NO_ERROR();

        for (size_t compIdx = 0; compIdx < kMatrixStride; compIdx++)
        {
            EXPECT_EQ(results[compIdx], expected[i][compIdx]);
        }
    }
}

// Test queries for transposed arrays of non-square matrix uniforms.
TEST_P(UniformTestES3, TransposedMatrixArrayUniformStateQuery)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform mat3x2 uniMat3x2[5];\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(uniMat3x2[0][0][0]);\n"
        "  color += vec4(uniMat3x2[1][0][0]);\n"
        "  color += vec4(uniMat3x2[2][0][0]);\n"
        "  color += vec4(uniMat3x2[3][0][0]);\n"
        "  color += vec4(uniMat3x2[4][0][0]);\n"
        "}";

    mProgram = CompileProgram(essl3_shaders::vs::Zero(), kFS);
    ASSERT_NE(mProgram, 0u);

    glUseProgram(mProgram);

    std::vector<GLfloat> transposedValues;

    for (size_t arrayElement = 0; arrayElement < 5; ++arrayElement)
    {
        transposedValues.push_back(1.0f + arrayElement);
        transposedValues.push_back(3.0f + arrayElement);
        transposedValues.push_back(5.0f + arrayElement);
        transposedValues.push_back(2.0f + arrayElement);
        transposedValues.push_back(4.0f + arrayElement);
        transposedValues.push_back(6.0f + arrayElement);
    }

    // Setting as a clump
    GLint baseLocation = glGetUniformLocation(mProgram, "uniMat3x2");
    ASSERT_NE(-1, baseLocation);

    glUniformMatrix3x2fv(baseLocation, 5, GL_TRUE, &transposedValues[0]);

    for (size_t arrayElement = 0; arrayElement < 5; ++arrayElement)
    {
        std::stringstream nameStr;
        nameStr << "uniMat3x2[" << arrayElement << "]";
        std::string name = nameStr.str();
        GLint location   = glGetUniformLocation(mProgram, name.c_str());
        ASSERT_NE(-1, location);

        std::vector<GLfloat> sequentialValues(6, 0);
        glGetUniformfv(mProgram, location, &sequentialValues[0]);

        ASSERT_GL_NO_ERROR();

        for (size_t comp = 0; comp < 6; ++comp)
        {
            EXPECT_EQ(static_cast<GLfloat>(comp + 1 + arrayElement), sequentialValues[comp]);
        }
    }
}

// Check that trying setting too many elements of an array doesn't overflow
TEST_P(UniformTestES3, OverflowArray)
{

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform float uniF[5];\n"
        "uniform mat3x2 uniMat3x2[5];\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = vec4(uniMat3x2[0][0][0] + uniF[0]);\n"
        "  color = vec4(uniMat3x2[1][0][0] + uniF[1]);\n"
        "  color = vec4(uniMat3x2[2][0][0] + uniF[2]);\n"
        "  color = vec4(uniMat3x2[3][0][0] + uniF[3]);\n"
        "  color = vec4(uniMat3x2[4][0][0] + uniF[4]);\n"
        "}";

    mProgram = CompileProgram(essl3_shaders::vs::Zero(), kFS);
    ASSERT_NE(mProgram, 0u);

    glUseProgram(mProgram);

    const size_t kOverflowSize = 10000;
    std::vector<GLfloat> values(10000 * 6);

    // Setting as a clump
    GLint floatLocation = glGetUniformLocation(mProgram, "uniF");
    ASSERT_NE(-1, floatLocation);
    GLint matLocation = glGetUniformLocation(mProgram, "uniMat3x2");
    ASSERT_NE(-1, matLocation);

    // Set too many float uniforms
    glUniform1fv(floatLocation, kOverflowSize, &values[0]);

    // Set too many matrix uniforms, transposed or not
    glUniformMatrix3x2fv(matLocation, kOverflowSize, GL_FALSE, &values[0]);
    glUniformMatrix3x2fv(matLocation, kOverflowSize, GL_TRUE, &values[0]);

    // Same checks but with offsets
    GLint floatLocationOffset = glGetUniformLocation(mProgram, "uniF[3]");
    ASSERT_NE(-1, floatLocationOffset);
    GLint matLocationOffset = glGetUniformLocation(mProgram, "uniMat3x2[3]");
    ASSERT_NE(-1, matLocationOffset);

    glUniform1fv(floatLocationOffset, kOverflowSize, &values[0]);
    glUniformMatrix3x2fv(matLocationOffset, kOverflowSize, GL_FALSE, &values[0]);
    glUniformMatrix3x2fv(matLocationOffset, kOverflowSize, GL_TRUE, &values[0]);
}

// Check setting a sampler uniform
TEST_P(UniformTest, Sampler)
{
    constexpr char kVS[] =
        "uniform sampler2D tex2D;\n"
        "void main() {\n"
        "  gl_Position = vec4(0, 0, 0, 1);\n"
        "}";

    constexpr char kFS[] =
        "precision mediump float;\n"
        "uniform sampler2D tex2D;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(tex2D, vec2(0, 0));\n"
        "}";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    GLint location = glGetUniformLocation(program.get(), "tex2D");
    ASSERT_NE(-1, location);

    const GLint sampler[] = {0, 0, 0, 0};

    // before UseProgram
    glUniform1i(location, sampler[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glUseProgram(program.get());

    // Uniform1i
    glUniform1i(location, sampler[0]);
    glUniform1iv(location, 1, sampler);
    EXPECT_GL_NO_ERROR();

    // Uniform{234}i
    glUniform2i(location, sampler[0], sampler[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform3i(location, sampler[0], sampler[0], sampler[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform4i(location, sampler[0], sampler[0], sampler[0], sampler[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform2iv(location, 1, sampler);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform3iv(location, 1, sampler);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform4iv(location, 1, sampler);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Uniform{1234}f
    const GLfloat f[] = {0, 0, 0, 0};
    glUniform1f(location, f[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform2f(location, f[0], f[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform3f(location, f[0], f[0], f[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform4f(location, f[0], f[0], f[0], f[0]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform1fv(location, 1, f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform2fv(location, 1, f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform3fv(location, 1, f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glUniform4fv(location, 1, f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // < 0 or >= max
    GLint tooHigh;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &tooHigh);
    constexpr GLint tooLow[] = {-1};
    glUniform1i(location, tooLow[0]);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glUniform1iv(location, 1, tooLow);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glUniform1i(location, tooHigh);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glUniform1iv(location, 1, &tooHigh);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Check that sampler uniforms only show up one time in the list
TEST_P(UniformTest, SamplerUniformsAppearOnce)
{
    int maxVertexTextureImageUnits = 0;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTextureImageUnits);

    // Renderer doesn't support vertex texture fetch, skipping test.
    ANGLE_SKIP_TEST_IF(!maxVertexTextureImageUnits);

    constexpr char kVS[] =
        "attribute vec2 position;\n"
        "uniform sampler2D tex2D;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  color = texture2D(tex2D, vec2(0));\n"
        "}";

    constexpr char kFS[] =
        "precision mediump float;\n"
        "varying vec4 color;\n"
        "uniform sampler2D tex2D;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(tex2D, vec2(0)) + color;\n"
        "}";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    GLint activeUniformsCount = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniformsCount);
    ASSERT_EQ(1, activeUniformsCount);

    GLint size       = 0;
    GLenum type      = GL_NONE;
    GLchar name[120] = {0};
    glGetActiveUniform(program, 0, 100, nullptr, &size, &type, name);
    EXPECT_EQ(1, size);
    EXPECT_GLENUM_EQ(GL_SAMPLER_2D, type);
    EXPECT_STREQ("tex2D", name);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

template <typename T, typename GetUniformV>
void CheckOneElement(GetUniformV getUniformv,
                     GLuint program,
                     const std::string &name,
                     int components,
                     T canary)
{
    // The buffer getting the results has three chunks
    //  - A chunk to see underflows
    //  - A chunk that will hold the result
    //  - A chunk to see overflows for when components = kChunkSize
    static const size_t kChunkSize = 4;
    std::array<T, 3 * kChunkSize> buffer;
    buffer.fill(canary);

    GLint location = glGetUniformLocation(program, name.c_str());
    ASSERT_NE(location, -1);

    getUniformv(program, location, &buffer[kChunkSize]);
    for (size_t i = 0; i < kChunkSize; i++)
    {
        ASSERT_EQ(canary, buffer[i]);
    }
    for (size_t i = kChunkSize + components; i < buffer.size(); i++)
    {
        ASSERT_EQ(canary, buffer[i]);
    }
}

// Check that getting an element array doesn't return the whole array.
TEST_P(UniformTestES3, ReturnsOnlyOneArrayElement)
{

    static const size_t kArraySize = 4;
    struct UniformArrayInfo
    {
        UniformArrayInfo(std::string type, std::string name, int components)
            : type(type), name(name), components(components)
        {}
        std::string type;
        std::string name;
        int components;
    };

    // Check for various number of components and types
    std::vector<UniformArrayInfo> uniformArrays;
    uniformArrays.emplace_back("bool", "uBool", 1);
    uniformArrays.emplace_back("vec2", "uFloat", 2);
    uniformArrays.emplace_back("ivec3", "uInt", 3);
    uniformArrays.emplace_back("uvec4", "uUint", 4);

    std::ostringstream uniformStream;
    std::ostringstream additionStream;
    for (const auto &array : uniformArrays)
    {
        uniformStream << "uniform " << array.type << " " << array.name << "["
                      << ToString(kArraySize) << "];\n";

        // We need to make use of the uniforms or they get compiled out.
        for (int i = 0; i < 4; i++)
        {
            if (array.components == 1)
            {
                additionStream << " + float(" << array.name << "[" << i << "])";
            }
            else
            {
                for (int component = 0; component < array.components; component++)
                {
                    additionStream << " + float(" << array.name << "[" << i << "][" << component
                                   << "])";
                }
            }
        }
    }

    const std::string vertexShader = "#version 300 es\n" + uniformStream.str() +
                                     "void main()\n"
                                     "{\n"
                                     "    gl_Position = vec4(1.0" +
                                     additionStream.str() +
                                     ");\n"
                                     "}";

    constexpr char kFS[] =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "void main ()\n"
        "{\n"
        "    color = vec4(1, 0, 0, 1);\n"
        "}";

    mProgram = CompileProgram(vertexShader.c_str(), kFS);
    ASSERT_NE(0u, mProgram);

    glUseProgram(mProgram);

    for (const auto &uniformArray : uniformArrays)
    {
        for (size_t index = 0; index < kArraySize; index++)
        {
            std::string strIndex = "[" + ToString(index) + "]";
            // Check all the different glGetUniformv functions
            CheckOneElement<float>(glGetUniformfv, mProgram, uniformArray.name + strIndex,
                                   uniformArray.components, 42.4242f);
            CheckOneElement<int>(glGetUniformiv, mProgram, uniformArray.name + strIndex,
                                 uniformArray.components, 0x7BADBED5);
            CheckOneElement<unsigned int>(glGetUniformuiv, mProgram, uniformArray.name + strIndex,
                                          uniformArray.components, 0xDEADBEEF);
        }
    }
}

// This test reproduces a regression when Intel windows driver upgrades to 4944. In some situation,
// when a boolean uniform with false value is used as the if and for condtions, the bug will be
// triggered. It seems that the shader doesn't get a right 'false' value from the uniform.
TEST_P(UniformTestES3, BooleanUniformAsIfAndForCondition)
{
    const char kFragShader[] =
        R"(#version 300 es
        precision mediump float;
        uniform bool u;
        out vec4 result;
        int sideEffectCounter;

        bool foo() {
          ++sideEffectCounter;
          return true;
        }

        void main() {
          sideEffectCounter = 0;
          bool condition = u;
          if (condition)
          {
            condition = foo();
          }
          for(int iterations = 0; condition;) {
            ++iterations;
            if (iterations >= 10) {
              break;
            }

            if (condition)
            {
              condition = foo();
            }
          }

          bool success = (!u && sideEffectCounter == 0);
          result = (success) ? vec4(0, 1.0, 0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
        })";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFragShader);

    glUseProgram(program.get());

    GLint uniformLocation = glGetUniformLocation(program, "u");
    ASSERT_NE(uniformLocation, -1);

    glUniform1i(uniformLocation, GL_FALSE);

    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

class UniformTestES31 : public ANGLETest
{
  protected:
    UniformTestES31() : mProgram(0) {}

    void testTearDown() override
    {
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
    }

    GLuint mProgram;
};

// Test that uniform locations get set correctly for structure members.
// ESSL 3.10.4 section 4.4.3.
TEST_P(UniformTestES31, StructLocationLayoutQualifier)
{
    constexpr char kFS[] =
        "#version 310 es\n"
        "out highp vec4 my_FragColor;\n"
        "struct S\n"
        "{\n"
        "    highp float f;\n"
        "    highp float f2;\n"
        "};\n"
        "uniform layout(location=12) S uS;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(uS.f, uS.f2, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Zero(), kFS);

    EXPECT_EQ(12, glGetUniformLocation(program.get(), "uS.f"));
    EXPECT_EQ(13, glGetUniformLocation(program.get(), "uS.f2"));
}

// Set uniform location with a layout qualifier in the fragment shader. The same uniform exists in
// the vertex shader, but doesn't have a location specified there.
TEST_P(UniformTestES31, UniformLocationInFragmentShader)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "uniform highp sampler2D tex2D;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = texture(tex2D, vec2(0));\n"
        "}";

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision mediump float;\n"
        "out vec4 my_FragColor;\n"
        "uniform layout(location=12) highp sampler2D tex2D;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = texture(tex2D, vec2(0));\n"
        "}";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    EXPECT_EQ(12, glGetUniformLocation(program.get(), "tex2D"));
}

// Test two unused uniforms that have the same location.
// ESSL 3.10.4 section 4.4.3: "No two default-block uniform variables in the program can have the
// same location, even if they are unused, otherwise a compiler or linker error will be generated."
TEST_P(UniformTestES31, UnusedUniformsConflictingLocation)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "uniform layout(location=12) highp sampler2D texA;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(0);\n"
        "}";

    constexpr char kFS[] =
        "#version 310 es\n"
        "out highp vec4 my_FragColor;\n"
        "uniform layout(location=12) highp sampler2D texB;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(0);\n"
        "}";

    mProgram = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, mProgram);
}

// Test two unused uniforms that have overlapping locations once all array elements are taken into
// account.
// ESSL 3.10.4 section 4.4.3: "No two default-block uniform variables in the program can have the
// same location, even if they are unused, otherwise a compiler or linker error will be generated."
TEST_P(UniformTestES31, UnusedUniformArraysConflictingLocation)
{
    constexpr char kVS[] =
        "#version 310 es\n"
        "uniform layout(location=11) highp vec4 uA[2];\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(0);\n"
        "}";

    constexpr char kFS[] =
        "#version 310 es\n"
        "out highp vec4 my_FragColor;\n"
        "uniform layout(location=12) highp vec4 uB;\n"
        "void main()\n"
        "{\n"
        "    my_FragColor = vec4(0);\n"
        "}";

    mProgram = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, mProgram);
}

// Test a uniform struct containing a non-square matrix and a boolean.
// Minimal test case for a bug revealed by dEQP tests.
TEST_P(UniformTestES3, StructWithNonSquareMatrixAndBool)
{
    constexpr char kFS[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "out highp vec4 my_color;\n"
        "struct S\n"
        "{\n"
        "    mat2x4 m;\n"
        "    bool b;\n"
        "};\n"
        "uniform S uni;\n"
        "void main()\n"
        "{\n"
        "    my_color = vec4(1.0);\n"
        "    if (!uni.b) { my_color.g = 0.0; }"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);

    glUseProgram(program.get());

    GLint location = glGetUniformLocation(program.get(), "uni.b");
    ASSERT_NE(-1, location);
    glUniform1i(location, 1);

    drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.0f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
}

// Test that matrix uniform upload is correct.
TEST_P(UniformTestES3, MatrixUniformUpload)
{
    constexpr size_t kMinDims = 2;
    constexpr size_t kMaxDims = 4;

    GLfloat matrixValues[kMaxDims * kMaxDims];

    for (size_t i = 0; i < kMaxDims * kMaxDims; ++i)
    {
        matrixValues[i] = static_cast<GLfloat>(i);
    }

    using UniformMatrixCxRfv = decltype(glUniformMatrix2fv);
    UniformMatrixCxRfv uniformMatrixCxRfv[kMaxDims + 1][kMaxDims + 1] = {
        {nullptr, nullptr, nullptr, nullptr, nullptr},
        {nullptr, nullptr, nullptr, nullptr, nullptr},
        {nullptr, nullptr, glUniformMatrix2fv, glUniformMatrix2x3fv, glUniformMatrix2x4fv},
        {nullptr, nullptr, glUniformMatrix3x2fv, glUniformMatrix3fv, glUniformMatrix3x4fv},
        {nullptr, nullptr, glUniformMatrix4x2fv, glUniformMatrix4x3fv, glUniformMatrix4fv},
    };

    for (int transpose = 0; transpose < 2; ++transpose)
    {
        for (size_t cols = kMinDims; cols <= kMaxDims; ++cols)
        {
            for (size_t rows = kMinDims; rows <= kMaxDims; ++rows)
            {
                std::ostringstream shader;
                shader << "#version 300 es\n"
                          "precision highp float;\n"
                          "out highp vec4 colorOut;\n"
                          "uniform mat"
                       << cols << 'x' << rows
                       << " unused;\n"
                          "uniform mat"
                       << cols << 'x' << rows
                       << " m;\n"
                          "void main()\n"
                          "{\n"
                          "  bool isCorrect =";

                for (size_t col = 0; col < cols; ++col)
                {
                    for (size_t row = 0; row < rows; ++row)
                    {
                        size_t value;
                        if (!transpose)
                        {
                            // Matrix data is uploaded column-major.
                            value = col * rows + row;
                        }
                        else
                        {
                            // Matrix data is uploaded row-major.
                            value = row * cols + col;
                        }

                        if (value != 0)
                        {
                            shader << "&&\n    ";
                        }

                        shader << "(m[" << col << "][" << row << "] == " << value << ".0)";
                    }
                }

                shader << ";\n  colorOut = vec4(isCorrect);\n"
                          "}\n";

                ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), shader.str().c_str());

                glUseProgram(program.get());

                GLint location = glGetUniformLocation(program.get(), "m");
                ASSERT_NE(-1, location);

                uniformMatrixCxRfv[cols][rows](location, 1, transpose != 0, matrixValues);
                ASSERT_GL_NO_ERROR();

                drawQuad(program.get(), essl3_shaders::PositionAttrib(), 0.0f);

                ASSERT_GL_NO_ERROR();
                EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
            }
        }
    }
}

// Test that uniforms with reserved OpenGL names that aren't reserved in GL ES 2 work correctly.
TEST_P(UniformTest, UniformWithReservedOpenGLName)
{
    constexpr char kFS[] =
        "precision mediump float;\n"
        "uniform float buffer;"
        "void main() {\n"
        "    gl_FragColor = vec4(buffer);\n"
        "}";

    mProgram = CompileProgram(essl1_shaders::vs::Simple(), kFS);
    ASSERT_NE(mProgram, 0u);

    GLint location = glGetUniformLocation(mProgram, "buffer");
    ASSERT_NE(-1, location);

    glUseProgram(mProgram);
    glUniform1f(location, 1.0f);

    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.0f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
}

// Test that unused sampler array elements do not corrupt used sampler array elements. Checks for a
// bug where unused samplers in an array would mark the whole array unused.
TEST_P(UniformTest, UnusedUniformsInSamplerArray)
{
    constexpr char kVS[] = R"(precision highp float;
attribute vec4 position;
varying vec2 texcoord;
void main()
{
    gl_Position = position;
    texcoord = (position.xy * 0.5) + 0.5;
})";
    constexpr char kFS[] = R"(precision highp float;
uniform sampler2D tex[3];
varying vec2 texcoord;
void main()
{
    gl_FragColor = texture2D(tex[0], texcoord);
})";

    mProgram = CompileProgram(kVS, kFS);

    ASSERT_NE(mProgram, 0u);
    GLint texLocation = glGetUniformLocation(mProgram, "tex[0]");
    ASSERT_NE(-1, texLocation);
    glUseProgram(mProgram);
    glUniform1i(texLocation, 0);
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    constexpr GLsizei kTextureSize = 2;
    std::vector<GLColor> textureData(kTextureSize * kTextureSize, GLColor::green);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTextureSize, kTextureSize, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, textureData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

TEST_P(UniformTest, UnusedStructInlineUniform)
{
    constexpr char kVS[] = R"(precision highp float;
attribute vec4 position;
void main()
{
    gl_Position = position;
})";

    constexpr char kFS[] = R"(precision highp float;
uniform struct {
  vec3  aVec3;
  vec2 aVec2;
}aUniform;
varying vec2 texcoord;
void main()
{
    gl_FragColor = vec4(0,1,0,1);
})";

    mProgram = CompileProgram(kVS, kFS);
    ASSERT_NE(mProgram, 0u);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

TEST_P(UniformTest, UnusedStructInlineUniformWithSampler)
{
    constexpr char kVS[] = R"(precision highp float;
attribute vec4 position;
void main()
{
    gl_Position = position;
})";

    constexpr char kFS[] = R"(precision highp float;
uniform struct {
  sampler2D  aSampler;
  vec3 aVec3;
}aUniform;
varying vec2 texcoord;
void main()
{
    gl_FragColor = vec4(0,1,0,1);
})";

    mProgram = CompileProgram(kVS, kFS);
    ASSERT_NE(mProgram, 0u);
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(SimpleUniformTest);

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(UniformTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(UniformTestES3);
ANGLE_INSTANTIATE_TEST_ES3(UniformTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(UniformTestES31);
ANGLE_INSTANTIATE_TEST_ES31(UniformTestES31);

}  // namespace
