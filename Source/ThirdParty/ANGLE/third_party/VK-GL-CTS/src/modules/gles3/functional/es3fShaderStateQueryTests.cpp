/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Rbo state query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fShaderStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

static const char *commonTestVertSource = "#version 300 es\n"
                                          "void main (void)\n"
                                          "{\n"
                                          "    gl_Position = vec4(0.0);\n"
                                          "}\n\0";
static const char *commonTestFragSource = "#version 300 es\n"
                                          "layout(location = 0) out mediump vec4 fragColor;\n"
                                          "void main (void)\n"
                                          "{\n"
                                          "    fragColor = vec4(0.0);\n"
                                          "}\n\0";

static const char *brokenShader = "#version 300 es\n"
                                  "broken, this should not compile!\n"
                                  "\n\0";

// rounds x.1 to x+1
template <typename T>
T roundGLfloatToNearestIntegerUp(GLfloat val)
{
    return (T)(ceil(val));
}

// rounds x.9 to x
template <typename T>
T roundGLfloatToNearestIntegerDown(GLfloat val)
{
    return (T)(floor(val));
}

bool checkIntEquals(tcu::TestContext &testCtx, GLint got, GLint expected)
{
    using tcu::TestLog;

    if (got != expected)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << expected << "; got " << got
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
        return false;
    }
    return true;
}

void checkPointerEquals(tcu::TestContext &testCtx, const void *got, const void *expected)
{
    using tcu::TestLog;

    if (got != expected)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << expected << "; got " << got
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
    }
}

void verifyShaderParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint shader, GLenum pname,
                       GLenum reference)
{
    StateQueryMemoryWriteGuard<GLint> state;
    gl.glGetShaderiv(shader, pname, &state);

    if (state.verifyValidity(testCtx))
        checkIntEquals(testCtx, state, reference);
}

bool verifyProgramParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLenum pname,
                        GLenum reference)
{
    StateQueryMemoryWriteGuard<GLint> state;
    gl.glGetProgramiv(program, pname, &state);

    return state.verifyValidity(testCtx) && checkIntEquals(testCtx, state, reference);
}

void verifyActiveUniformParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLuint index,
                              GLenum pname, GLenum reference)
{
    StateQueryMemoryWriteGuard<GLint> state;
    gl.glGetActiveUniformsiv(program, 1, &index, pname, &state);

    if (state.verifyValidity(testCtx))
        checkIntEquals(testCtx, state, reference);
}

void verifyActiveUniformBlockParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program,
                                   GLuint blockIndex, GLenum pname, GLenum reference)
{
    StateQueryMemoryWriteGuard<GLint> state;
    gl.glGetActiveUniformBlockiv(program, blockIndex, pname, &state);

    if (state.verifyValidity(testCtx))
        checkIntEquals(testCtx, state, reference);
}

void verifyCurrentVertexAttribf(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLint index, GLfloat x, GLfloat y,
                                GLfloat z, GLfloat w)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[4]> attribValue;
    gl.glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, attribValue);

    attribValue.verifyValidity(testCtx);

    if (attribValue[0] != x || attribValue[1] != y || attribValue[2] != z || attribValue[3] != w)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected [" << x << "," << y << "," << z << "," << w << "];"
                         << "got [" << attribValue[0] << "," << attribValue[1] << "," << attribValue[2] << ","
                         << attribValue[3] << "]" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid attribute value");
    }
}

void verifyCurrentVertexAttribIi(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLint index, GLint x, GLint y,
                                 GLint z, GLint w)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[4]> attribValue;
    gl.glGetVertexAttribIiv(index, GL_CURRENT_VERTEX_ATTRIB, attribValue);

    attribValue.verifyValidity(testCtx);

    if (attribValue[0] != x || attribValue[1] != y || attribValue[2] != z || attribValue[3] != w)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected [" << x << "," << y << "," << z << "," << w << "];"
                         << "got [" << attribValue[0] << "," << attribValue[1] << "," << attribValue[2] << ","
                         << attribValue[3] << "]" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid attribute value");
    }
}

void verifyCurrentVertexAttribIui(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLint index, GLuint x, GLuint y,
                                  GLuint z, GLuint w)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLuint[4]> attribValue;
    gl.glGetVertexAttribIuiv(index, GL_CURRENT_VERTEX_ATTRIB, attribValue);

    attribValue.verifyValidity(testCtx);

    if (attribValue[0] != x || attribValue[1] != y || attribValue[2] != z || attribValue[3] != w)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected [" << x << "," << y << "," << z << "," << w << "];"
                         << "got [" << attribValue[0] << "," << attribValue[1] << "," << attribValue[2] << ","
                         << attribValue[3] << "]" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid attribute value");
    }
}

void verifyCurrentVertexAttribConversion(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLint index, GLfloat x,
                                         GLfloat y, GLfloat z, GLfloat w)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[4]> attribValue;
    gl.glGetVertexAttribiv(index, GL_CURRENT_VERTEX_ATTRIB, attribValue);

    attribValue.verifyValidity(testCtx);

    const GLint referenceAsGLintMin[] = {
        roundGLfloatToNearestIntegerDown<GLint>(x), roundGLfloatToNearestIntegerDown<GLint>(y),
        roundGLfloatToNearestIntegerDown<GLint>(z), roundGLfloatToNearestIntegerDown<GLint>(w)};
    const GLint referenceAsGLintMax[] = {
        roundGLfloatToNearestIntegerUp<GLint>(x), roundGLfloatToNearestIntegerUp<GLint>(y),
        roundGLfloatToNearestIntegerUp<GLint>(z), roundGLfloatToNearestIntegerUp<GLint>(w)};

    if (attribValue[0] < referenceAsGLintMin[0] || attribValue[0] > referenceAsGLintMax[0] ||
        attribValue[1] < referenceAsGLintMin[1] || attribValue[1] > referenceAsGLintMax[1] ||
        attribValue[2] < referenceAsGLintMin[2] || attribValue[2] > referenceAsGLintMax[2] ||
        attribValue[3] < referenceAsGLintMin[3] || attribValue[3] > referenceAsGLintMax[3])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in range "
                         << "[" << referenceAsGLintMin[0] << " " << referenceAsGLintMax[0] << "], "
                         << "[" << referenceAsGLintMin[1] << " " << referenceAsGLintMax[1] << "], "
                         << "[" << referenceAsGLintMin[2] << " " << referenceAsGLintMax[2] << "], "
                         << "[" << referenceAsGLintMin[3] << " " << referenceAsGLintMax[3] << "]"
                         << "; got " << attribValue[0] << ", " << attribValue[1] << ", " << attribValue[2] << ", "
                         << attribValue[3] << " "
                         << "; Input=" << x << "; " << y << "; " << z << "; " << w << " " << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid attribute value");
    }
}

void verifyVertexAttrib(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLint index, GLenum pname, GLenum reference)
{
    StateQueryMemoryWriteGuard<GLint> state;
    gl.glGetVertexAttribIiv(index, pname, &state);

    if (state.verifyValidity(testCtx))
        checkIntEquals(testCtx, state, reference);
}

void verifyUniformValue1f(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, float x)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[1]> state;
    gl.glGetUniformfv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << "]; got [" << state[0] << "]"
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue2f(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, float x,
                          float y)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[2]> state;
    gl.glGetUniformfv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << "]; got [" << state[0]
                         << ", " << state[1] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue3f(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, float x,
                          float y, float z)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[3]> state;
    gl.glGetUniformfv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y || state[2] != z)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << ", " << z << "]; got ["
                         << state[0] << ", " << state[1] << ", " << state[2] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue4f(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, float x,
                          float y, float z, float w)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[4]> state;
    gl.glGetUniformfv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y || state[2] != z || state[3] != w)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << ", " << z << ", " << w
                         << "]; got [" << state[0] << ", " << state[1] << ", " << state[2] << ", " << state[3] << "]"
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue1i(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLint x)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[1]> state;
    gl.glGetUniformiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << "]; got [" << state[0] << "]"
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue2i(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLint x,
                          GLint y)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[2]> state;
    gl.glGetUniformiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << "]; got [" << state[0]
                         << ", " << state[1] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue3i(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLint x,
                          GLint y, GLint z)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[3]> state;
    gl.glGetUniformiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y || state[2] != z)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << ", " << z << "]; got ["
                         << state[0] << ", " << state[1] << ", " << state[2] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue4i(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLint x,
                          GLint y, GLint z, GLint w)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[4]> state;
    gl.glGetUniformiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y || state[2] != z || state[3] != w)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << ", " << z << ", " << w
                         << "]; got [" << state[0] << ", " << state[1] << ", " << state[2] << ", " << state[3] << "]"
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue1ui(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLuint x)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLuint[1]> state;
    gl.glGetUniformuiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << "]; got [" << state[0] << "]"
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue2ui(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLuint x,
                           GLuint y)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLuint[2]> state;
    gl.glGetUniformuiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << "]; got [" << state[0]
                         << ", " << state[1] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue3ui(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLuint x,
                           GLuint y, GLuint z)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLuint[3]> state;
    gl.glGetUniformuiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y || state[2] != z)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << ", " << z << "]; got ["
                         << state[0] << ", " << state[1] << ", " << state[2] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

void verifyUniformValue4ui(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location, GLuint x,
                           GLuint y, GLuint z, GLuint w)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLuint[4]> state;
    gl.glGetUniformuiv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state[0] != x || state[1] != y || state[2] != z || state[3] != w)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected [" << x << ", " << y << ", " << z << ", " << w
                         << "]; got [" << state[0] << ", " << state[1] << ", " << state[2] << ", " << state[3] << "]"
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
    }
}

template <int Count>
void verifyUniformValues(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location,
                         const GLfloat *values)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[Count]> state;
    gl.glGetUniformfv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    for (int ndx = 0; ndx < Count; ++ndx)
    {
        if (values[ndx] != state[ndx])
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: at index " << ndx << " expected " << values[ndx]
                             << "; got " << state[ndx] << TestLog::EndMessage;

            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
        }
    }
}

template <int N>
void verifyUniformMatrixValues(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLuint program, GLint location,
                               const GLfloat *values, bool transpose)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[N * N]> state;
    gl.glGetUniformfv(program, location, state);

    if (!state.verifyValidity(testCtx))
        return;

    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
        {
            const int refIndex   = y * N + x;
            const int stateIndex = transpose ? (x * N + y) : (y * N + x);

            if (values[refIndex] != state[stateIndex])
            {
                testCtx.getLog() << TestLog::Message << "// ERROR: at index [" << y << "][" << x << "] expected "
                                 << values[refIndex] << "; got " << state[stateIndex] << TestLog::EndMessage;

                if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid uniform value");
            }
        }
}

class ShaderTypeCase : public ApiCase
{
public:
    ShaderTypeCase(Context &context, const char *name, const char *description) : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const GLenum shaderTypes[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(shaderTypes); ++ndx)
        {
            const GLuint shader = glCreateShader(shaderTypes[ndx]);
            verifyShaderParam(m_testCtx, *this, shader, GL_SHADER_TYPE, shaderTypes[ndx]);
            glDeleteShader(shader);
        }
    }
};

class ShaderCompileStatusCase : public ApiCase
{
public:
    ShaderCompileStatusCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        verifyShaderParam(m_testCtx, *this, shaderVert, GL_COMPILE_STATUS, GL_FALSE);
        verifyShaderParam(m_testCtx, *this, shaderFrag, GL_COMPILE_STATUS, GL_FALSE);

        glShaderSource(shaderVert, 1, &commonTestVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &commonTestFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        verifyShaderParam(m_testCtx, *this, shaderVert, GL_COMPILE_STATUS, GL_TRUE);
        verifyShaderParam(m_testCtx, *this, shaderFrag, GL_COMPILE_STATUS, GL_TRUE);

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        expectError(GL_NO_ERROR);
    }
};

class ShaderInfoLogCase : public ApiCase
{
public:
    ShaderInfoLogCase(Context &context, const char *name, const char *description) : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        // INFO_LOG_LENGTH is 0 by default and it includes null-terminator
        const GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        verifyShaderParam(m_testCtx, *this, shader, GL_INFO_LOG_LENGTH, 0);

        glShaderSource(shader, 1, &brokenShader, nullptr);
        glCompileShader(shader);
        expectError(GL_NO_ERROR);

        // check the log length
        StateQueryMemoryWriteGuard<GLint> logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (!logLength.verifyValidity(m_testCtx))
        {
            glDeleteShader(shader);
            return;
        }
        if (logLength == 0)
        {
            glDeleteShader(shader);
            return;
        }

        // check normal case
        {
            char buffer[2048] = {'x'}; // non-zero initialization

            GLint written = 0; // written does not include null terminator
            glGetShaderInfoLog(shader, DE_LENGTH_OF_ARRAY(buffer), &written, buffer);

            // check lengths are consistent
            if (logLength <= DE_LENGTH_OF_ARRAY(buffer))
            {
                if (written != logLength - 1)
                {
                    m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected length " << logLength - 1 << "; got "
                                       << written << TestLog::EndMessage;
                    if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log length");
                }
            }

            // check null-terminator, either at end of buffer or at buffer[written]
            const char *terminator = &buffer[DE_LENGTH_OF_ARRAY(buffer) - 1];
            if (logLength < DE_LENGTH_OF_ARRAY(buffer))
                terminator = &buffer[written];

            if (*terminator != '\0')
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected null terminator, got " << (int)*terminator
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log terminator");
            }
        }

        // check with too small buffer
        {
            char buffer[2048] = {'x'}; // non-zero initialization

            // check string always ends with \0, even with small buffers
            GLint written = 0;
            glGetShaderInfoLog(shader, 1, &written, buffer);
            if (written != 0)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected length 0; got " << written
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log length");
            }
            if (buffer[0] != '\0')
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected null terminator, got " << (int)buffer[0]
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log terminator");
            }
        }

        glDeleteShader(shader);
        expectError(GL_NO_ERROR);
    }
};

class ShaderSourceCase : public ApiCase
{
public:
    ShaderSourceCase(Context &context, const char *name, const char *description) : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        // SHADER_SOURCE_LENGTH does include 0-terminator
        const GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        verifyShaderParam(m_testCtx, *this, shader, GL_SHADER_SOURCE_LENGTH, 0);

        // check the SHADER_SOURCE_LENGTH
        {
            glShaderSource(shader, 1, &brokenShader, nullptr);
            expectError(GL_NO_ERROR);

            StateQueryMemoryWriteGuard<GLint> sourceLength;
            glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &sourceLength);

            sourceLength.verifyValidity(m_testCtx);

            const GLint referenceLength =
                (GLint)std::string(brokenShader).length() + 1; // including the null terminator
            if (sourceLength != referenceLength)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected length " << referenceLength << "; got "
                                   << sourceLength << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid source length");
            }
        }

        // check the concat source SHADER_SOURCE_LENGTH
        {
            const char *shaders[] = {brokenShader, brokenShader};
            glShaderSource(shader, 2, shaders, nullptr);
            expectError(GL_NO_ERROR);

            StateQueryMemoryWriteGuard<GLint> sourceLength;
            glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &sourceLength);

            sourceLength.verifyValidity(m_testCtx);

            const GLint referenceLength =
                2 * (GLint)std::string(brokenShader).length() + 1; // including the null terminator
            if (sourceLength != referenceLength)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected length " << referenceLength << "; got "
                                   << sourceLength << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid source length");
            }
        }

        // check the string length
        {
            char buffer[2048] = {'x'};
            DE_ASSERT(DE_LENGTH_OF_ARRAY(buffer) > 2 * (int)std::string(brokenShader).length());

            GLint written = 0; // not inluding null-terminator
            glGetShaderSource(shader, DE_LENGTH_OF_ARRAY(buffer), &written, buffer);

            const GLint referenceLength = 2 * (GLint)std::string(brokenShader).length();
            if (written != referenceLength)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected write length " << referenceLength
                                   << "; got " << written << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid source length");
            }
            // check null pointer at
            else
            {
                if (buffer[referenceLength] != '\0')
                {
                    m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected null terminator at "
                                       << referenceLength << TestLog::EndMessage;
                    if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "did not get a null terminator");
                }
            }
        }

        // check with small buffer
        {
            char buffer[2048] = {'x'};

            GLint written = 0;
            glGetShaderSource(shader, 1, &written, buffer);

            if (written != 0)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected write length 0; got " << written
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid source length");
            }
            if (buffer[0] != '\0')
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected null terminator; got=" << int(buffer[0])
                                   << ", char=" << buffer[0] << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid terminator");
            }
        }

        glDeleteShader(shader);
        expectError(GL_NO_ERROR);
    }
};

class DeleteStatusCase : public ApiCase
{
public:
    DeleteStatusCase(Context &context, const char *name, const char *description) : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &commonTestVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &commonTestFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        verifyShaderParam(m_testCtx, *this, shaderVert, GL_COMPILE_STATUS, GL_TRUE);
        verifyShaderParam(m_testCtx, *this, shaderFrag, GL_COMPILE_STATUS, GL_TRUE);

        GLuint shaderProg = glCreateProgram();
        glAttachShader(shaderProg, shaderVert);
        glAttachShader(shaderProg, shaderFrag);
        glLinkProgram(shaderProg);
        expectError(GL_NO_ERROR);

        verifyProgramParam(m_testCtx, *this, shaderProg, GL_LINK_STATUS, GL_TRUE);

        verifyShaderParam(m_testCtx, *this, shaderVert, GL_DELETE_STATUS, GL_FALSE);
        verifyShaderParam(m_testCtx, *this, shaderFrag, GL_DELETE_STATUS, GL_FALSE);
        verifyProgramParam(m_testCtx, *this, shaderProg, GL_DELETE_STATUS, GL_FALSE);
        expectError(GL_NO_ERROR);

        glUseProgram(shaderProg);

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(shaderProg);
        expectError(GL_NO_ERROR);

        verifyShaderParam(m_testCtx, *this, shaderVert, GL_DELETE_STATUS, GL_TRUE);
        verifyShaderParam(m_testCtx, *this, shaderFrag, GL_DELETE_STATUS, GL_TRUE);
        verifyProgramParam(m_testCtx, *this, shaderProg, GL_DELETE_STATUS, GL_TRUE);
        expectError(GL_NO_ERROR);

        glUseProgram(0);
        expectError(GL_NO_ERROR);
    }
};

class CurrentVertexAttribInitialCase : public ApiCase
{
public:
    CurrentVertexAttribInitialCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        int attribute_count = 16;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attribute_count);

        // initial

        for (int index = 0; index < attribute_count; ++index)
        {
            StateQueryMemoryWriteGuard<GLfloat[4]> attribValue;
            glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, attribValue);
            attribValue.verifyValidity(m_testCtx);

            if (attribValue[0] != 0.0f || attribValue[1] != 0.0f || attribValue[2] != 0.0f || attribValue[3] != 1.0f)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected [0, 0, 0, 1];"
                                   << "got [" << attribValue[0] << "," << attribValue[1] << "," << attribValue[2] << ","
                                   << attribValue[3] << "]" << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid attribute value");
            }
        }
    }
};

class CurrentVertexAttribFloatCase : public ApiCase
{
public:
    CurrentVertexAttribFloatCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        de::Random rnd(0xabcdef);

        int attribute_count = 16;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attribute_count);

        // test write float/read float

        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = rnd.getFloat(-64000, 64000);
            const GLfloat z = rnd.getFloat(-64000, 64000);
            const GLfloat w = rnd.getFloat(-64000, 64000);

            glVertexAttrib4f(index, x, y, z, w);
            verifyCurrentVertexAttribf(m_testCtx, *this, index, x, y, z, w);
        }
        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = rnd.getFloat(-64000, 64000);
            const GLfloat z = rnd.getFloat(-64000, 64000);
            const GLfloat w = 1.0f;

            glVertexAttrib3f(index, x, y, z);
            verifyCurrentVertexAttribf(m_testCtx, *this, index, x, y, z, w);
        }
        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = rnd.getFloat(-64000, 64000);
            const GLfloat z = 0.0f;
            const GLfloat w = 1.0f;

            glVertexAttrib2f(index, x, y);
            verifyCurrentVertexAttribf(m_testCtx, *this, index, x, y, z, w);
        }
        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = 0.0f;
            const GLfloat z = 0.0f;
            const GLfloat w = 1.0f;

            glVertexAttrib1f(index, x);
            verifyCurrentVertexAttribf(m_testCtx, *this, index, x, y, z, w);
        }
    }
};

class CurrentVertexAttribIntCase : public ApiCase
{
public:
    CurrentVertexAttribIntCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        de::Random rnd(0xabcdef);

        int attribute_count = 16;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attribute_count);

        // test write float/read float

        for (int index = 0; index < attribute_count; ++index)
        {
            const GLint x = rnd.getInt(-64000, 64000);
            const GLint y = rnd.getInt(-64000, 64000);
            const GLint z = rnd.getInt(-64000, 64000);
            const GLint w = rnd.getInt(-64000, 64000);

            glVertexAttribI4i(index, x, y, z, w);
            verifyCurrentVertexAttribIi(m_testCtx, *this, index, x, y, z, w);
        }
    }
};

class CurrentVertexAttribUintCase : public ApiCase
{
public:
    CurrentVertexAttribUintCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        de::Random rnd(0xabcdef);

        int attribute_count = 16;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attribute_count);

        // test write float/read float

        for (int index = 0; index < attribute_count; ++index)
        {
            const GLuint x = rnd.getInt(0, 64000);
            const GLuint y = rnd.getInt(0, 64000);
            const GLuint z = rnd.getInt(0, 64000);
            const GLuint w = rnd.getInt(0, 64000);

            glVertexAttribI4ui(index, x, y, z, w);
            verifyCurrentVertexAttribIui(m_testCtx, *this, index, x, y, z, w);
        }
    }
};

class CurrentVertexAttribConversionCase : public ApiCase
{
public:
    CurrentVertexAttribConversionCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        de::Random rnd(0xabcdef);

        int attribute_count = 16;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attribute_count);

        // test write float/read float

        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = rnd.getFloat(-64000, 64000);
            const GLfloat z = rnd.getFloat(-64000, 64000);
            const GLfloat w = rnd.getFloat(-64000, 64000);

            glVertexAttrib4f(index, x, y, z, w);
            verifyCurrentVertexAttribConversion(m_testCtx, *this, index, x, y, z, w);
        }
        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = rnd.getFloat(-64000, 64000);
            const GLfloat z = rnd.getFloat(-64000, 64000);
            const GLfloat w = 1.0f;

            glVertexAttrib3f(index, x, y, z);
            verifyCurrentVertexAttribConversion(m_testCtx, *this, index, x, y, z, w);
        }
        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = rnd.getFloat(-64000, 64000);
            const GLfloat z = 0.0f;
            const GLfloat w = 1.0f;

            glVertexAttrib2f(index, x, y);
            verifyCurrentVertexAttribConversion(m_testCtx, *this, index, x, y, z, w);
        }
        for (int index = 0; index < attribute_count; ++index)
        {
            const GLfloat x = rnd.getFloat(-64000, 64000);
            const GLfloat y = 0.0f;
            const GLfloat z = 0.0f;
            const GLfloat w = 1.0f;

            glVertexAttrib1f(index, x);
            verifyCurrentVertexAttribConversion(m_testCtx, *this, index, x, y, z, w);
        }
    }
};

class ProgramInfoLogCase : public ApiCase
{
public:
    enum BuildErrorType
    {
        BUILDERROR_COMPILE = 0,
        BUILDERROR_LINK
    };

    ProgramInfoLogCase(Context &context, const char *name, const char *description, BuildErrorType buildErrorType)
        : ApiCase(context, name, description)
        , m_buildErrorType(buildErrorType)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        enum
        {
            BUF_SIZE = 2048
        };

        static const char *const linkErrorVtxSource = "#version 300 es\n"
                                                      "in highp vec4 a_pos;\n"
                                                      "uniform highp vec4 u_uniform;\n"
                                                      "void main ()\n"
                                                      "{\n"
                                                      "    gl_Position = a_pos + u_uniform;\n"
                                                      "}\n";
        static const char *const linkErrorFrgSource = "#version 300 es\n"
                                                      "in highp vec4 v_missingVar;\n"
                                                      "uniform highp int u_uniform;\n"
                                                      "layout(location = 0) out mediump vec4 fragColor;\n"
                                                      "void main ()\n"
                                                      "{\n"
                                                      "    fragColor = v_missingVar + vec4(float(u_uniform));\n"
                                                      "}\n";

        const char *vtxSource = (m_buildErrorType == BUILDERROR_COMPILE) ? (brokenShader) : (linkErrorVtxSource);
        const char *frgSource = (m_buildErrorType == BUILDERROR_COMPILE) ? (brokenShader) : (linkErrorFrgSource);

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &vtxSource, nullptr);
        glShaderSource(shaderFrag, 1, &frgSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);

        StateQueryMemoryWriteGuard<GLint> logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        logLength.verifyValidity(m_testCtx);

        // check INFO_LOG_LENGTH == GetProgramInfoLog len
        {
            const tcu::ScopedLogSection section(m_testCtx.getLog(), "QueryLarge", "Query to large buffer");
            char buffer[BUF_SIZE] = {'x'};
            GLint written         = 0;

            glGetProgramInfoLog(program, BUF_SIZE, &written, buffer);

            if (logLength != 0 && written + 1 != logLength) // INFO_LOG_LENGTH contains 0-terminator
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected INFO_LOG_LENGTH " << written + 1
                                   << "; got " << logLength << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log length");
            }
            else if (logLength != 0 && buffer[written] != '\0')
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected null terminator at index " << written
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "missing null terminator");
            }
        }

        // check query to just correct sized buffer
        if (BUF_SIZE > logLength)
        {
            const tcu::ScopedLogSection section(m_testCtx.getLog(), "QueryAll",
                                                "Query all to exactly right sized buffer");
            char buffer[BUF_SIZE] = {'x'};
            GLint written         = 0;

            glGetProgramInfoLog(program, logLength, &written, buffer);

            if (logLength != 0 && written + 1 != logLength) // INFO_LOG_LENGTH contains 0-terminator
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected INFO_LOG_LENGTH " << written + 1
                                   << "; got " << logLength << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log length");
            }
            else if (logLength != 0 && buffer[written] != '\0')
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected null terminator at index " << written
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "missing null terminator");
            }
        }

        // check GetProgramInfoLog works with too small buffer
        {
            const tcu::ScopedLogSection section(m_testCtx.getLog(), "QueryNone", "Query none");
            char buffer[BUF_SIZE] = {'x'};
            GLint written         = 0;

            glGetProgramInfoLog(program, 1, &written, buffer);

            if (written != 0)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected write length 0; got " << written
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log length");
            }
        }

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }

    const BuildErrorType m_buildErrorType;
};

class ProgramValidateStatusCase : public ApiCase
{
public:
    ProgramValidateStatusCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        // test validate ok
        {
            GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
            GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

            glShaderSource(shaderVert, 1, &commonTestVertSource, nullptr);
            glShaderSource(shaderFrag, 1, &commonTestFragSource, nullptr);

            glCompileShader(shaderVert);
            glCompileShader(shaderFrag);
            expectError(GL_NO_ERROR);

            GLuint program = glCreateProgram();
            glAttachShader(program, shaderVert);
            glAttachShader(program, shaderFrag);
            glLinkProgram(program);
            expectError(GL_NO_ERROR);

            verifyShaderParam(m_testCtx, *this, shaderVert, GL_COMPILE_STATUS, GL_TRUE);
            verifyShaderParam(m_testCtx, *this, shaderFrag, GL_COMPILE_STATUS, GL_TRUE);
            verifyProgramParam(m_testCtx, *this, program, GL_LINK_STATUS, GL_TRUE);

            glValidateProgram(program);
            verifyProgramParam(m_testCtx, *this, program, GL_VALIDATE_STATUS, GL_TRUE);

            glDeleteShader(shaderVert);
            glDeleteShader(shaderFrag);
            glDeleteProgram(program);
            expectError(GL_NO_ERROR);
        }

        // test with broken shader
        {
            GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
            GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

            glShaderSource(shaderVert, 1, &commonTestVertSource, nullptr);
            glShaderSource(shaderFrag, 1, &brokenShader, nullptr);

            glCompileShader(shaderVert);
            glCompileShader(shaderFrag);
            expectError(GL_NO_ERROR);

            GLuint program = glCreateProgram();
            glAttachShader(program, shaderVert);
            glAttachShader(program, shaderFrag);
            glLinkProgram(program);
            expectError(GL_NO_ERROR);

            verifyShaderParam(m_testCtx, *this, shaderVert, GL_COMPILE_STATUS, GL_TRUE);
            verifyShaderParam(m_testCtx, *this, shaderFrag, GL_COMPILE_STATUS, GL_FALSE);
            verifyProgramParam(m_testCtx, *this, program, GL_LINK_STATUS, GL_FALSE);

            glValidateProgram(program);
            verifyProgramParam(m_testCtx, *this, program, GL_VALIDATE_STATUS, GL_FALSE);

            glDeleteShader(shaderVert);
            glDeleteShader(shaderFrag);
            glDeleteProgram(program);
            expectError(GL_NO_ERROR);
        }
    }
};

class ProgramAttachedShadersCase : public ApiCase
{
public:
    ProgramAttachedShadersCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &commonTestVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &commonTestFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        // check ATTACHED_SHADERS

        GLuint program = glCreateProgram();
        verifyProgramParam(m_testCtx, *this, program, GL_ATTACHED_SHADERS, 0);
        expectError(GL_NO_ERROR);

        glAttachShader(program, shaderVert);
        verifyProgramParam(m_testCtx, *this, program, GL_ATTACHED_SHADERS, 1);
        expectError(GL_NO_ERROR);

        glAttachShader(program, shaderFrag);
        verifyProgramParam(m_testCtx, *this, program, GL_ATTACHED_SHADERS, 2);
        expectError(GL_NO_ERROR);

        // check GetAttachedShaders
        {
            GLuint shaders[2] = {0, 0};
            GLint count       = 0;
            glGetAttachedShaders(program, DE_LENGTH_OF_ARRAY(shaders), &count, shaders);

            if (count != 2)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected 2; got " << count << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong shader count");
            }
            // shaders are the attached shaders?
            if (!((shaders[0] == shaderVert && shaders[1] == shaderFrag) ||
                  (shaders[0] == shaderFrag && shaders[1] == shaderVert)))
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected {" << shaderVert << ", " << shaderFrag
                                   << "}; got {" << shaders[0] << ", " << shaders[1] << "}" << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong shader count");
            }
        }

        // check GetAttachedShaders with too small buffer
        {
            GLuint shaders[2] = {0, 0};
            GLint count       = 0;

            glGetAttachedShaders(program, 0, &count, shaders);
            if (count != 0)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected 0; got " << count << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong shader count");
            }

            count = 0;
            glGetAttachedShaders(program, 1, &count, shaders);
            if (count != 1)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected 1; got " << count << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong shader count");
            }
        }

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class ProgramActiveUniformNameCase : public ApiCase
{
public:
    ProgramActiveUniformNameCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        static const char *testVertSource = "#version 300 es\n"
                                            "uniform highp float uniformNameWithLength23;\n"
                                            "uniform highp vec2 uniformVec2;\n"
                                            "uniform highp mat4 uniformMat4;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(0.0) + vec4(uniformNameWithLength23) + "
                                            "vec4(uniformVec2.x) + vec4(uniformMat4[2][3]);\n"
                                            "}\n\0";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n\0";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        expectError(GL_NO_ERROR);

        verifyProgramParam(m_testCtx, *this, program, GL_ACTIVE_UNIFORMS, 3);
        verifyProgramParam(m_testCtx, *this, program, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                           (GLint)std::string("uniformNameWithLength23").length() + 1); // including a null terminator
        expectError(GL_NO_ERROR);

        const char *uniformNames[] = {"uniformNameWithLength23", "uniformVec2", "uniformMat4"};
        StateQueryMemoryWriteGuard<GLuint[DE_LENGTH_OF_ARRAY(uniformNames)]> uniformIndices;
        glGetUniformIndices(program, DE_LENGTH_OF_ARRAY(uniformNames), uniformNames, uniformIndices);
        uniformIndices.verifyValidity(m_testCtx);

        // check name lengths
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uniformNames); ++ndx)
        {
            const GLuint uniformIndex = uniformIndices[ndx];

            StateQueryMemoryWriteGuard<GLint> uniformNameLen;
            glGetActiveUniformsiv(program, 1, &uniformIndex, GL_UNIFORM_NAME_LENGTH, &uniformNameLen);

            uniformNameLen.verifyValidity(m_testCtx);

            const GLint referenceLength = (GLint)std::string(uniformNames[ndx]).length() + 1;
            if (referenceLength != uniformNameLen) // uniformNameLen is with null terminator
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << referenceLength << "got "
                                   << uniformNameLen << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong uniform name length");
            }
        }

        // check names
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uniformNames); ++ndx)
        {
            char buffer[2048] = {'x'};

            const GLuint uniformIndex = uniformIndices[ndx];

            GLint written = 0; // null terminator not included
            GLint size    = 0;
            GLenum type   = 0;
            glGetActiveUniform(program, uniformIndex, DE_LENGTH_OF_ARRAY(buffer), &written, &size, &type, buffer);

            const GLint referenceLength = (GLint)std::string(uniformNames[ndx]).length();
            if (referenceLength != written)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << referenceLength << "got " << written
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong uniform name length");
            }

            // and with too small buffer
            written = 0;
            glGetActiveUniform(program, uniformIndex, 1, &written, &size, &type, buffer);

            if (written != 0)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected 0 got " << written << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong uniform name length");
            }
        }

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class ProgramUniformCase : public ApiCase
{
public:
    ProgramUniformCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const struct UniformType
        {
            const char *declaration;
            const char *postDeclaration;
            const char *precision;
            const char *layout;
            const char *getter;
            GLenum type;
            GLint size;
            GLint isRowMajor;
        } uniformTypes[] = {
            {"float", "", "highp", "", "uniformValue", GL_FLOAT, 1, GL_FALSE},
            {"float[2]", "", "highp", "", "uniformValue[1]", GL_FLOAT, 2, GL_FALSE},
            {"vec2", "", "highp", "", "uniformValue.x", GL_FLOAT_VEC2, 1, GL_FALSE},
            {"vec3", "", "highp", "", "uniformValue.x", GL_FLOAT_VEC3, 1, GL_FALSE},
            {"vec4", "", "highp", "", "uniformValue.x", GL_FLOAT_VEC4, 1, GL_FALSE},
            {"int", "", "highp", "", "float(uniformValue)", GL_INT, 1, GL_FALSE},
            {"ivec2", "", "highp", "", "float(uniformValue.x)", GL_INT_VEC2, 1, GL_FALSE},
            {"ivec3", "", "highp", "", "float(uniformValue.x)", GL_INT_VEC3, 1, GL_FALSE},
            {"ivec4", "", "highp", "", "float(uniformValue.x)", GL_INT_VEC4, 1, GL_FALSE},
            {"uint", "", "highp", "", "float(uniformValue)", GL_UNSIGNED_INT, 1, GL_FALSE},
            {"uvec2", "", "highp", "", "float(uniformValue.x)", GL_UNSIGNED_INT_VEC2, 1, GL_FALSE},
            {"uvec3", "", "highp", "", "float(uniformValue.x)", GL_UNSIGNED_INT_VEC3, 1, GL_FALSE},
            {"uvec4", "", "highp", "", "float(uniformValue.x)", GL_UNSIGNED_INT_VEC4, 1, GL_FALSE},
            {"bool", "", "", "", "float(uniformValue)", GL_BOOL, 1, GL_FALSE},
            {"bvec2", "", "", "", "float(uniformValue.x)", GL_BOOL_VEC2, 1, GL_FALSE},
            {"bvec3", "", "", "", "float(uniformValue.x)", GL_BOOL_VEC3, 1, GL_FALSE},
            {"bvec4", "", "", "", "float(uniformValue.x)", GL_BOOL_VEC4, 1, GL_FALSE},
            {"mat2", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT2, 1, GL_FALSE},
            {"mat3", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT3, 1, GL_FALSE},
            {"mat4", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT4, 1, GL_FALSE},
            {"mat2x3", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT2x3, 1, GL_FALSE},
            {"mat2x4", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT2x4, 1, GL_FALSE},
            {"mat3x2", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT3x2, 1, GL_FALSE},
            {"mat3x4", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT3x4, 1, GL_FALSE},
            {"mat4x2", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT4x2, 1, GL_FALSE},
            {"mat4x3", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT4x3, 1, GL_FALSE},
            {"sampler2D", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_SAMPLER_2D, 1, GL_FALSE},
            {"sampler3D", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_SAMPLER_3D, 1, GL_FALSE},
            {"samplerCube", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_SAMPLER_CUBE, 1, GL_FALSE},
            {"sampler2DShadow", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_SAMPLER_2D_SHADOW, 1,
             GL_FALSE},
            {"sampler2DArray", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_SAMPLER_2D_ARRAY, 1,
             GL_FALSE},
            {"sampler2DArrayShadow", "", "highp", "", "float(textureSize(uniformValue,0).r)",
             GL_SAMPLER_2D_ARRAY_SHADOW, 1, GL_FALSE},
            {"samplerCubeShadow", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_SAMPLER_CUBE_SHADOW, 1,
             GL_FALSE},
            {"isampler2D", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_INT_SAMPLER_2D, 1, GL_FALSE},
            {"isampler3D", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_INT_SAMPLER_3D, 1, GL_FALSE},
            {"isamplerCube", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_INT_SAMPLER_CUBE, 1, GL_FALSE},
            {"isampler2DArray", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_INT_SAMPLER_2D_ARRAY, 1,
             GL_FALSE},
            {"usampler2D", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_UNSIGNED_INT_SAMPLER_2D, 1,
             GL_FALSE},
            {"usampler3D", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_UNSIGNED_INT_SAMPLER_3D, 1,
             GL_FALSE},
            {"usamplerCube", "", "highp", "", "float(textureSize(uniformValue,0).r)", GL_UNSIGNED_INT_SAMPLER_CUBE, 1,
             GL_FALSE},
            {"usampler2DArray", "", "highp", "", "float(textureSize(uniformValue,0).r)",
             GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, 1, GL_FALSE},
        };

        static const char *vertSource = "#version 300 es\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    gl_Position = vec4(0.0);\n"
                                        "}\n\0";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint program    = glCreateProgram();

        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);

        glShaderSource(shaderVert, 1, &vertSource, nullptr);
        glCompileShader(shaderVert);
        expectError(GL_NO_ERROR);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uniformTypes); ++ndx)
        {
            tcu::ScopedLogSection(m_log, uniformTypes[ndx].declaration,
                                  std::string("Verify type of ") + uniformTypes[ndx].declaration + " variable" +
                                      uniformTypes[ndx].postDeclaration);

            // gen fragment shader

            std::ostringstream frag;
            frag << "#version 300 es\n";
            frag << uniformTypes[ndx].layout << "uniform " << uniformTypes[ndx].precision << " "
                 << uniformTypes[ndx].declaration << " uniformValue" << uniformTypes[ndx].postDeclaration << ";\n";
            frag << "layout(location = 0) out mediump vec4 fragColor;\n";
            frag << "void main (void)\n";
            frag << "{\n";
            frag << "    fragColor = vec4(" << uniformTypes[ndx].getter << ");\n";
            frag << "}\n";

            {
                std::string fragmentSource     = frag.str();
                const char *fragmentSourceCStr = fragmentSource.c_str();
                glShaderSource(shaderFrag, 1, &fragmentSourceCStr, nullptr);
            }

            // compile & link

            glCompileShader(shaderFrag);
            glLinkProgram(program);

            // test
            if (verifyProgramParam(m_testCtx, *this, program, GL_LINK_STATUS, GL_TRUE))
            {
                const char *uniformNames[] = {"uniformValue"};
                StateQueryMemoryWriteGuard<GLuint> uniformIndex;
                glGetUniformIndices(program, 1, uniformNames, &uniformIndex);
                uniformIndex.verifyValidity(m_testCtx);

                verifyActiveUniformParam(m_testCtx, *this, program, uniformIndex, GL_UNIFORM_TYPE,
                                         uniformTypes[ndx].type);
                verifyActiveUniformParam(m_testCtx, *this, program, uniformIndex, GL_UNIFORM_SIZE,
                                         uniformTypes[ndx].size);
                verifyActiveUniformParam(m_testCtx, *this, program, uniformIndex, GL_UNIFORM_IS_ROW_MAJOR,
                                         uniformTypes[ndx].isRowMajor);
            }
        }

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class ProgramActiveUniformBlocksCase : public ApiCase
{
public:
    ProgramActiveUniformBlocksCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        static const char *testVertSource =
            "#version 300 es\n"
            "uniform longlongUniformBlockName {highp vec2 vector2;} longlongUniformInstanceName;\n"
            "uniform shortUniformBlockName {highp vec2 vector2;highp vec4 vector4;} shortUniformInstanceName;\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = shortUniformInstanceName.vector4 + vec4(longlongUniformInstanceName.vector2.x) + "
            "vec4(shortUniformInstanceName.vector2.x);\n"
            "}\n\0";
        static const char *testFragSource =
            "#version 300 es\n"
            "uniform longlongUniformBlockName {highp vec2 vector2;} longlongUniformInstanceName;\n"
            "layout(location = 0) out mediump vec4 fragColor;"
            "void main (void)\n"
            "{\n"
            "    fragColor = vec4(longlongUniformInstanceName.vector2.y);\n"
            "}\n\0";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        expectError(GL_NO_ERROR);

        verifyShaderParam(m_testCtx, *this, shaderVert, GL_COMPILE_STATUS, GL_TRUE);
        verifyShaderParam(m_testCtx, *this, shaderFrag, GL_COMPILE_STATUS, GL_TRUE);
        verifyProgramParam(m_testCtx, *this, program, GL_LINK_STATUS, GL_TRUE);

        verifyProgramParam(m_testCtx, *this, program, GL_ACTIVE_UNIFORM_BLOCKS, 2);
        verifyProgramParam(m_testCtx, *this, program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                           (GLint)std::string("longlongUniformBlockName").length() + 1); // including a null terminator
        expectError(GL_NO_ERROR);

        GLint longlongUniformBlockIndex = glGetUniformBlockIndex(program, "longlongUniformBlockName");
        GLint shortUniformBlockIndex    = glGetUniformBlockIndex(program, "shortUniformBlockName");

        const char *uniformNames[] = {"longlongUniformBlockName.vector2", "shortUniformBlockName.vector2",
                                      "shortUniformBlockName.vector4"};

        // test UNIFORM_BLOCK_INDEX

        DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(uniformNames) == 3);

        StateQueryMemoryWriteGuard<GLuint[DE_LENGTH_OF_ARRAY(uniformNames)]> uniformIndices;
        StateQueryMemoryWriteGuard<GLint[DE_LENGTH_OF_ARRAY(uniformNames)]> uniformsBlockIndices;

        glGetUniformIndices(program, DE_LENGTH_OF_ARRAY(uniformNames), uniformNames, uniformIndices);
        uniformIndices.verifyValidity(m_testCtx);
        expectError(GL_NO_ERROR);

        glGetActiveUniformsiv(program, DE_LENGTH_OF_ARRAY(uniformNames), uniformIndices, GL_UNIFORM_BLOCK_INDEX,
                              uniformsBlockIndices);
        uniformsBlockIndices.verifyValidity(m_testCtx);
        expectError(GL_NO_ERROR);

        if (uniformsBlockIndices[0] != longlongUniformBlockIndex || uniformsBlockIndices[1] != shortUniformBlockIndex ||
            uniformsBlockIndices[2] != shortUniformBlockIndex)
        {
            m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected [" << longlongUniformBlockIndex << ", "
                               << shortUniformBlockIndex << ", " << shortUniformBlockIndex << "];"
                               << "got [" << uniformsBlockIndices[0] << ", " << uniformsBlockIndices[1] << ", "
                               << uniformsBlockIndices[2] << "]" << TestLog::EndMessage;
            if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong uniform block index");
        }

        // test UNIFORM_BLOCK_NAME_LENGTH

        verifyActiveUniformBlockParam(
            m_testCtx, *this, program, longlongUniformBlockIndex, GL_UNIFORM_BLOCK_NAME_LENGTH,
            (GLint)std::string("longlongUniformBlockName").length() + 1); // including null-terminator
        verifyActiveUniformBlockParam(m_testCtx, *this, program, shortUniformBlockIndex, GL_UNIFORM_BLOCK_NAME_LENGTH,
                                      (GLint)std::string("shortUniformBlockName").length() +
                                          1); // including null-terminator
        expectError(GL_NO_ERROR);

        // test UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER & UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER

        verifyActiveUniformBlockParam(m_testCtx, *this, program, longlongUniformBlockIndex,
                                      GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, GL_TRUE);
        verifyActiveUniformBlockParam(m_testCtx, *this, program, longlongUniformBlockIndex,
                                      GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, GL_TRUE);
        verifyActiveUniformBlockParam(m_testCtx, *this, program, shortUniformBlockIndex,
                                      GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, GL_TRUE);
        verifyActiveUniformBlockParam(m_testCtx, *this, program, shortUniformBlockIndex,
                                      GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, GL_FALSE);
        expectError(GL_NO_ERROR);

        // test UNIFORM_BLOCK_ACTIVE_UNIFORMS

        verifyActiveUniformBlockParam(m_testCtx, *this, program, longlongUniformBlockIndex,
                                      GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, 1);
        verifyActiveUniformBlockParam(m_testCtx, *this, program, shortUniformBlockIndex,
                                      GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, 2);
        expectError(GL_NO_ERROR);

        // test UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES

        {
            StateQueryMemoryWriteGuard<GLint> longlongUniformBlockUniforms;
            glGetActiveUniformBlockiv(program, longlongUniformBlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
                                      &longlongUniformBlockUniforms);
            longlongUniformBlockUniforms.verifyValidity(m_testCtx);

            if (longlongUniformBlockUniforms == 2)
            {
                StateQueryMemoryWriteGuard<GLint[2]> longlongUniformBlockUniformIndices;
                glGetActiveUniformBlockiv(program, longlongUniformBlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                                          longlongUniformBlockUniformIndices);
                longlongUniformBlockUniformIndices.verifyValidity(m_testCtx);

                if ((GLuint(longlongUniformBlockUniformIndices[0]) != uniformIndices[0] ||
                     GLuint(longlongUniformBlockUniformIndices[1]) != uniformIndices[1]) &&
                    (GLuint(longlongUniformBlockUniformIndices[1]) != uniformIndices[0] ||
                     GLuint(longlongUniformBlockUniformIndices[0]) != uniformIndices[1]))
                {
                    m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected {" << uniformIndices[0] << ", "
                                       << uniformIndices[1] << "};"
                                       << "got {" << longlongUniformBlockUniformIndices[0] << ", "
                                       << longlongUniformBlockUniformIndices[1] << "}" << TestLog::EndMessage;

                    if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong uniform indices");
                }
            }
        }

        // check block names

        {
            char buffer[2048] = {'x'};
            GLint written     = 0;
            glGetActiveUniformBlockName(program, longlongUniformBlockIndex, DE_LENGTH_OF_ARRAY(buffer), &written,
                                        buffer);
            checkIntEquals(m_testCtx, written, (GLint)std::string("longlongUniformBlockName").length());

            written = 0;
            glGetActiveUniformBlockName(program, shortUniformBlockIndex, DE_LENGTH_OF_ARRAY(buffer), &written, buffer);
            checkIntEquals(m_testCtx, written, (GLint)std::string("shortUniformBlockName").length());

            // and one with too small buffer
            written = 0;
            glGetActiveUniformBlockName(program, longlongUniformBlockIndex, 1, &written, buffer);
            checkIntEquals(m_testCtx, written, 0);
        }

        expectError(GL_NO_ERROR);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class ProgramBinaryCase : public ApiCase
{
public:
    ProgramBinaryCase(Context &context, const char *name, const char *description) : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &commonTestVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &commonTestFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        expectError(GL_NO_ERROR);

        // test PROGRAM_BINARY_RETRIEVABLE_HINT
        verifyProgramParam(m_testCtx, *this, program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_FALSE);

        glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
        expectError(GL_NO_ERROR);

        glLinkProgram(program);
        expectError(GL_NO_ERROR);

        verifyProgramParam(m_testCtx, *this, program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

        // test PROGRAM_BINARY_LENGTH does something

        StateQueryMemoryWriteGuard<GLint> programLength;
        glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &programLength);
        expectError(GL_NO_ERROR);
        programLength.verifyValidity(m_testCtx);

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class TransformFeedbackCase : public ApiCase
{
public:
    TransformFeedbackCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        static const char *transformFeedbackTestVertSource = "#version 300 es\n"
                                                             "out highp vec4 tfOutput2withLongName;\n"
                                                             "void main (void)\n"
                                                             "{\n"
                                                             "    gl_Position = vec4(0.0);\n"
                                                             "    tfOutput2withLongName = vec4(0.0);\n"
                                                             "}\n";
        static const char *transformFeedbackTestFragSource = "#version 300 es\n"
                                                             "layout(location = 0) out highp vec4 fragColor;\n"
                                                             "void main (void)\n"
                                                             "{\n"
                                                             "    fragColor = vec4(0.0);\n"
                                                             "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint shaderProg = glCreateProgram();

        verifyProgramParam(m_testCtx, *this, shaderProg, GL_TRANSFORM_FEEDBACK_BUFFER_MODE, GL_INTERLEAVED_ATTRIBS);

        glShaderSource(shaderVert, 1, &transformFeedbackTestVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &transformFeedbackTestFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);

        verifyShaderParam(m_testCtx, *this, shaderVert, GL_COMPILE_STATUS, GL_TRUE);
        verifyShaderParam(m_testCtx, *this, shaderFrag, GL_COMPILE_STATUS, GL_TRUE);

        glAttachShader(shaderProg, shaderVert);
        glAttachShader(shaderProg, shaderFrag);

        // check TRANSFORM_FEEDBACK_BUFFER_MODE

        const char *transform_feedback_outputs[] = {"gl_Position", "tfOutput2withLongName"};
        const char *longest_output               = transform_feedback_outputs[1];
        const GLenum bufferModes[]               = {GL_SEPARATE_ATTRIBS, GL_INTERLEAVED_ATTRIBS};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(bufferModes); ++ndx)
        {
            glTransformFeedbackVaryings(shaderProg, DE_LENGTH_OF_ARRAY(transform_feedback_outputs),
                                        transform_feedback_outputs, bufferModes[ndx]);
            glLinkProgram(shaderProg);
            expectError(GL_NO_ERROR);

            verifyProgramParam(m_testCtx, *this, shaderProg, GL_LINK_STATUS, GL_TRUE);
            verifyProgramParam(m_testCtx, *this, shaderProg, GL_TRANSFORM_FEEDBACK_BUFFER_MODE, bufferModes[ndx]);
        }

        // TRANSFORM_FEEDBACK_VARYINGS
        verifyProgramParam(m_testCtx, *this, shaderProg, GL_TRANSFORM_FEEDBACK_VARYINGS, 2);

        // TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH
        {
            StateQueryMemoryWriteGuard<GLint> maxOutputLen;
            glGetProgramiv(shaderProg, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &maxOutputLen);

            maxOutputLen.verifyValidity(m_testCtx);

            const GLint referenceLength = (GLint)std::string(longest_output).length() + 1;
            checkIntEquals(m_testCtx, maxOutputLen, referenceLength);
        }

        // check varyings
        {
            StateQueryMemoryWriteGuard<GLint> varyings;
            glGetProgramiv(shaderProg, GL_TRANSFORM_FEEDBACK_VARYINGS, &varyings);

            if (!varyings.isUndefined())
                for (int index = 0; index < varyings; ++index)
                {
                    char buffer[2048] = {'x'};

                    GLint written = 0;
                    GLint size    = 0;
                    GLenum type   = 0;
                    glGetTransformFeedbackVarying(shaderProg, index, DE_LENGTH_OF_ARRAY(buffer), &written, &size, &type,
                                                  buffer);

                    if (written < DE_LENGTH_OF_ARRAY(buffer) && buffer[written] != '\0')
                    {
                        m_testCtx.getLog()
                            << TestLog::Message << "// ERROR: Expected null terminator" << TestLog::EndMessage;
                        if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid string terminator");
                    }

                    // check with too small buffer
                    written = 0;
                    glGetTransformFeedbackVarying(shaderProg, index, 1, &written, &size, &type, buffer);
                    if (written != 0)
                    {
                        m_testCtx.getLog()
                            << TestLog::Message << "// ERROR: Expected 0; got " << written << TestLog::EndMessage;
                        if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid write length");
                    }
                }
        }

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(shaderProg);
        expectError(GL_NO_ERROR);
    }
};

class ActiveAttributesCase : public ApiCase
{
public:
    ActiveAttributesCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        static const char *testVertSource = "#version 300 es\n"
                                            "in highp vec2 longInputAttributeName;\n"
                                            "in highp vec2 shortName;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = longInputAttributeName.yxxy + shortName.xyxy;\n"
                                            "}\n\0";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n\0";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        expectError(GL_NO_ERROR);

        verifyProgramParam(m_testCtx, *this, program, GL_ACTIVE_ATTRIBUTES, 2);
        verifyProgramParam(m_testCtx, *this, program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                           (GLint)std::string("longInputAttributeName").length() + 1); // does include null-terminator

        // check names
        for (int attributeNdx = 0; attributeNdx < 2; ++attributeNdx)
        {
            char buffer[2048] = {'x'};

            GLint written = 0;
            GLint size    = 0;
            GLenum type   = 0;
            glGetActiveAttrib(program, attributeNdx, DE_LENGTH_OF_ARRAY(buffer), &written, &size, &type, buffer);
            expectError(GL_NO_ERROR);

            if (deStringBeginsWith(buffer, "longInputAttributeName"))
            {
                checkIntEquals(
                    m_testCtx, written,
                    (GLint)std::string("longInputAttributeName").length()); // does NOT include null-terminator
            }
            else if (deStringBeginsWith(buffer, "shortName"))
            {
                checkIntEquals(m_testCtx, written,
                               (GLint)std::string("shortName").length()); // does NOT include null-terminator
            }
            else
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Got unexpected attribute name."
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got unexpected name");
            }
        }

        // and with too short buffer
        {
            char buffer[2048] = {'x'};

            GLint written = 0;
            GLint size    = 0;
            GLenum type   = 0;

            glGetActiveAttrib(program, 0, 1, &written, &size, &type, buffer);
            expectError(GL_NO_ERROR);
            checkIntEquals(m_testCtx, written, 0);
        }

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

struct PointerData
{
    GLint size;
    GLenum type;
    GLint stride;
    GLboolean normalized;
    const void *pointer;
};

class VertexAttributeSizeCase : public ApiCase
{
public:
    VertexAttributeSizeCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLfloat vertexData[4] = {0.0f}; // never accessed

        const PointerData pointers[] = {
            // size test
            {4, GL_FLOAT, 0, GL_FALSE, vertexData}, {3, GL_FLOAT, 0, GL_FALSE, vertexData},
            {2, GL_FLOAT, 0, GL_FALSE, vertexData}, {1, GL_FLOAT, 0, GL_FALSE, vertexData},
            {4, GL_INT, 0, GL_FALSE, vertexData},   {3, GL_INT, 0, GL_FALSE, vertexData},
            {2, GL_INT, 0, GL_FALSE, vertexData},   {1, GL_INT, 0, GL_FALSE, vertexData},
        };

        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
            {
                glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                      pointers[ndx].stride, pointers[ndx].pointer);
                expectError(GL_NO_ERROR);

                verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_SIZE, pointers[ndx].size);
            }
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint buf     = 0;
            GLuint vaos[2] = {0};

            glGenVertexArrays(2, vaos);
            glGenBuffers(1, &buf);
            glBindBuffer(GL_ARRAY_BUFFER, buf);
            expectError(GL_NO_ERROR);

            // initial
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_SIZE, 4);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glVertexAttribPointer(0, pointers[0].size, pointers[0].type, pointers[0].normalized, pointers[0].stride,
                                  nullptr);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glVertexAttribPointer(0, pointers[1].size, pointers[1].type, pointers[1].normalized, pointers[1].stride,
                                  nullptr);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_SIZE, pointers[1].size);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_SIZE, pointers[0].size);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            glDeleteBuffers(1, &buf);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributeTypeCase : public ApiCase
{
public:
    VertexAttributeTypeCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            const GLfloat vertexData[4] = {0.0f}; // never accessed

            // test VertexAttribPointer
            {
                const PointerData pointers[] = {
                    {1, GL_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_INT, 0, GL_FALSE, vertexData},
                    {1, GL_FIXED, 0, GL_FALSE, vertexData},
                    {1, GL_FLOAT, 0, GL_FALSE, vertexData},
                    {1, GL_HALF_FLOAT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_INT, 0, GL_FALSE, vertexData},
                    {4, GL_INT_2_10_10_10_REV, 0, GL_FALSE, vertexData},
                    {4, GL_UNSIGNED_INT_2_10_10_10_REV, 0, GL_FALSE, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                          pointers[ndx].stride, pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_TYPE, pointers[ndx].type);
                }
            }

            // test glVertexAttribIPointer
            {
                const PointerData pointers[] = {
                    {1, GL_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_INT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_INT, 0, GL_FALSE, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribIPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].stride,
                                           pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_TYPE, pointers[ndx].type);
                }
            }
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint buf     = 0;
            GLuint vaos[2] = {0};

            glGenVertexArrays(2, vaos);
            glGenBuffers(1, &buf);
            glBindBuffer(GL_ARRAY_BUFFER, buf);
            expectError(GL_NO_ERROR);

            // initial
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_TYPE, GL_FLOAT);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glVertexAttribPointer(0, 1, GL_SHORT, GL_FALSE, 0, nullptr);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_TYPE, GL_SHORT);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_TYPE, GL_FLOAT);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            glDeleteBuffers(1, &buf);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributeStrideCase : public ApiCase
{
public:
    VertexAttributeStrideCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            const GLfloat vertexData[4] = {0.0f}; // never accessed

            struct StridePointerData
            {
                GLint size;
                GLenum type;
                GLint stride;
                const void *pointer;
            };

            // test VertexAttribPointer
            {
                const StridePointerData pointers[] = {
                    {1, GL_FLOAT, 0, vertexData},      {1, GL_FLOAT, 1, vertexData},
                    {1, GL_FLOAT, 4, vertexData},      {1, GL_HALF_FLOAT, 0, vertexData},
                    {1, GL_HALF_FLOAT, 1, vertexData}, {1, GL_HALF_FLOAT, 4, vertexData},
                    {1, GL_FIXED, 0, vertexData},      {1, GL_FIXED, 1, vertexData},
                    {1, GL_FIXED, 4, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, GL_FALSE, pointers[ndx].stride,
                                          pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, pointers[ndx].stride);
                }
            }

            // test glVertexAttribIPointer
            {
                const StridePointerData pointers[] = {
                    {1, GL_INT, 0, vertexData},           {1, GL_INT, 1, vertexData},
                    {1, GL_INT, 4, vertexData},           {4, GL_UNSIGNED_BYTE, 0, vertexData},
                    {4, GL_UNSIGNED_BYTE, 1, vertexData}, {4, GL_UNSIGNED_BYTE, 4, vertexData},
                    {2, GL_SHORT, 0, vertexData},         {2, GL_SHORT, 1, vertexData},
                    {2, GL_SHORT, 4, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribIPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].stride,
                                           pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, pointers[ndx].stride);
                }
            }
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint buf     = 0;
            GLuint vaos[2] = {0};

            glGenVertexArrays(2, vaos);
            glGenBuffers(1, &buf);
            glBindBuffer(GL_ARRAY_BUFFER, buf);
            expectError(GL_NO_ERROR);

            // initial
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, 0);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 4, nullptr);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glVertexAttribPointer(0, 1, GL_SHORT, GL_FALSE, 8, nullptr);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, 8);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, 4);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            glDeleteBuffers(1, &buf);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributeNormalizedCase : public ApiCase
{
public:
    VertexAttributeNormalizedCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            const GLfloat vertexData[4] = {0.0f}; // never accessed

            // test VertexAttribPointer
            {
                const PointerData pointers[] = {
                    {1, GL_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_INT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_INT, 0, GL_FALSE, vertexData},
                    {4, GL_INT_2_10_10_10_REV, 0, GL_FALSE, vertexData},
                    {4, GL_UNSIGNED_INT_2_10_10_10_REV, 0, GL_FALSE, vertexData},
                    {1, GL_BYTE, 0, GL_TRUE, vertexData},
                    {1, GL_SHORT, 0, GL_TRUE, vertexData},
                    {1, GL_INT, 0, GL_TRUE, vertexData},
                    {1, GL_UNSIGNED_BYTE, 0, GL_TRUE, vertexData},
                    {1, GL_UNSIGNED_SHORT, 0, GL_TRUE, vertexData},
                    {1, GL_UNSIGNED_INT, 0, GL_TRUE, vertexData},
                    {4, GL_INT_2_10_10_10_REV, 0, GL_TRUE, vertexData},
                    {4, GL_UNSIGNED_INT_2_10_10_10_REV, 0, GL_TRUE, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                          pointers[ndx].stride, pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                                       pointers[ndx].normalized);
                }
            }

            // test glVertexAttribIPointer
            {
                const PointerData pointers[] = {
                    {1, GL_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_INT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_INT, 0, GL_FALSE, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribIPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].stride,
                                           pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, GL_FALSE);
                }
            }
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint buf     = 0;
            GLuint vaos[2] = {0};

            glGenVertexArrays(2, vaos);
            glGenBuffers(1, &buf);
            glBindBuffer(GL_ARRAY_BUFFER, buf);
            expectError(GL_NO_ERROR);

            // initial
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, GL_FALSE);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glVertexAttribPointer(0, 1, GL_INT, GL_TRUE, 0, nullptr);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glVertexAttribPointer(0, 1, GL_INT, GL_FALSE, 0, nullptr);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, GL_FALSE);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, GL_TRUE);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            glDeleteBuffers(1, &buf);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributeIntegerCase : public ApiCase
{
public:
    VertexAttributeIntegerCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            const GLfloat vertexData[4] = {0.0f}; // never accessed

            // test VertexAttribPointer
            {
                const PointerData pointers[] = {
                    {1, GL_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_INT, 0, GL_FALSE, vertexData},
                    {1, GL_FIXED, 0, GL_FALSE, vertexData},
                    {1, GL_FLOAT, 0, GL_FALSE, vertexData},
                    {1, GL_HALF_FLOAT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_INT, 0, GL_FALSE, vertexData},
                    {4, GL_INT_2_10_10_10_REV, 0, GL_FALSE, vertexData},
                    {4, GL_UNSIGNED_INT_2_10_10_10_REV, 0, GL_FALSE, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                          pointers[ndx].stride, pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_INTEGER, GL_FALSE);
                }
            }

            // test glVertexAttribIPointer
            {
                const PointerData pointers[] = {
                    {1, GL_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_INT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
                    {1, GL_UNSIGNED_INT, 0, GL_FALSE, vertexData},
                };

                for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
                {
                    glVertexAttribIPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].stride,
                                           pointers[ndx].pointer);
                    expectError(GL_NO_ERROR);

                    verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_INTEGER, GL_TRUE);
                }
            }
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint buf     = 0;
            GLuint vaos[2] = {0};

            glGenVertexArrays(2, vaos);
            glGenBuffers(1, &buf);
            glBindBuffer(GL_ARRAY_BUFFER, buf);
            expectError(GL_NO_ERROR);

            // initial
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_INTEGER, GL_FALSE);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glVertexAttribIPointer(0, 1, GL_INT, 0, nullptr);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_INTEGER, GL_FALSE);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_INTEGER, GL_TRUE);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            glDeleteBuffers(1, &buf);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributeEnabledCase : public ApiCase
{
public:
    VertexAttributeEnabledCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        // VERTEX_ATTRIB_ARRAY_ENABLED

        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_FALSE);
            glEnableVertexAttribArray(0);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_TRUE);
            glDisableVertexAttribArray(0);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_FALSE);
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint vaos[2] = {0};

            glGenVertexArrays(2, vaos);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glBindVertexArray(vaos[0]);
            glEnableVertexAttribArray(0);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glDisableVertexAttribArray(0);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_FALSE);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_TRUE);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributeDivisorCase : public ApiCase
{
public:
    VertexAttributeDivisorCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, 0);
            glVertexAttribDivisor(0, 1);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, 1);
            glVertexAttribDivisor(0, 5);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, 5);
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint vaos[2] = {0};

            glGenVertexArrays(2, vaos);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glBindVertexArray(vaos[0]);
            glVertexAttribDivisor(0, 1);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glVertexAttribDivisor(0, 5);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, 5);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, 1);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributeBufferBindingCase : public ApiCase
{
public:
    VertexAttributeBufferBindingCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            // initial
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, 0);

            GLuint bufferID;
            glGenBuffers(1, &bufferID);
            glBindBuffer(GL_ARRAY_BUFFER, bufferID);
            expectError(GL_NO_ERROR);

            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
            expectError(GL_NO_ERROR);

            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, bufferID);

            glDeleteBuffers(1, &bufferID);
            expectError(GL_NO_ERROR);
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint vaos[2] = {0};
            GLuint bufs[2] = {0};

            glGenBuffers(2, bufs);
            expectError(GL_NO_ERROR);

            glGenVertexArrays(2, vaos);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glBindVertexArray(vaos[0]);
            glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glBindBuffer(GL_ARRAY_BUFFER, bufs[1]);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, bufs[1]);
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, bufs[0]);
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            glDeleteBuffers(2, bufs);
            expectError(GL_NO_ERROR);
        }
    }
};

class VertexAttributePointerCase : public ApiCase
{
public:
    VertexAttributePointerCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        const glu::ContextType &contextType = m_context.getRenderContext().getType();
        const bool isCoreGL45               = glu::contextSupports(contextType, glu::ApiType::core(4, 5));

        // Test with default VAO
        if (!isCoreGL45)
        {
            const tcu::ScopedLogSection section(m_log, "DefaultVAO", "Test with default VAO");

            StateQueryMemoryWriteGuard<GLvoid *> initialState;
            glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &initialState);
            initialState.verifyValidity(m_testCtx);
            checkPointerEquals(m_testCtx, initialState, 0);

            const GLfloat vertexData[4]  = {0.0f}; // never accessed
            const PointerData pointers[] = {
                {1, GL_BYTE, 0, GL_FALSE, &vertexData[2]},       {1, GL_SHORT, 0, GL_FALSE, &vertexData[1]},
                {1, GL_INT, 0, GL_FALSE, &vertexData[2]},        {1, GL_FIXED, 0, GL_FALSE, &vertexData[2]},
                {1, GL_FIXED, 0, GL_FALSE, &vertexData[1]},      {1, GL_FLOAT, 0, GL_FALSE, &vertexData[0]},
                {1, GL_FLOAT, 0, GL_FALSE, &vertexData[3]},      {1, GL_FLOAT, 0, GL_FALSE, &vertexData[2]},
                {1, GL_HALF_FLOAT, 0, GL_FALSE, &vertexData[0]}, {4, GL_HALF_FLOAT, 0, GL_FALSE, &vertexData[1]},
                {4, GL_HALF_FLOAT, 0, GL_FALSE, &vertexData[2]},
            };

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
            {
                glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                      pointers[ndx].stride, pointers[ndx].pointer);
                expectError(GL_NO_ERROR);

                StateQueryMemoryWriteGuard<GLvoid *> state;
                glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &state);
                state.verifyValidity(m_testCtx);
                checkPointerEquals(m_testCtx, state, pointers[ndx].pointer);
            }
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(m_log, "WithVAO", "Test with VAO");

            GLuint vaos[2] = {0};
            GLuint bufs[2] = {0};

            glGenBuffers(2, bufs);
            expectError(GL_NO_ERROR);

            glGenVertexArrays(2, vaos);
            expectError(GL_NO_ERROR);

            // set vao 0 to some value
            glBindVertexArray(vaos[0]);
            glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, glu::BufferOffsetAsPointer(8));
            expectError(GL_NO_ERROR);

            // set vao 1 to some other value
            glBindVertexArray(vaos[1]);
            glBindBuffer(GL_ARRAY_BUFFER, bufs[1]);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, glu::BufferOffsetAsPointer(4));
            expectError(GL_NO_ERROR);

            // verify vao 1 state
            {
                StateQueryMemoryWriteGuard<GLvoid *> state;
                glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &state);
                state.verifyValidity(m_testCtx);
                checkPointerEquals(m_testCtx, state, glu::BufferOffsetAsPointer(4));
            }
            expectError(GL_NO_ERROR);

            // verify vao 0 state
            glBindVertexArray(vaos[0]);
            {
                StateQueryMemoryWriteGuard<GLvoid *> state;
                glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &state);
                state.verifyValidity(m_testCtx);
                checkPointerEquals(m_testCtx, state, glu::BufferOffsetAsPointer(8));
            }
            expectError(GL_NO_ERROR);

            glDeleteVertexArrays(2, vaos);
            glDeleteBuffers(2, bufs);
            expectError(GL_NO_ERROR);
        }
    }
};

class UniformValueFloatCase : public ApiCase
{
public:
    UniformValueFloatCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        static const char *testVertSource =
            "#version 300 es\n"
            "uniform highp float floatUniform;\n"
            "uniform highp vec2 float2Uniform;\n"
            "uniform highp vec3 float3Uniform;\n"
            "uniform highp vec4 float4Uniform;\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(floatUniform + float2Uniform.x + float3Uniform.x + float4Uniform.x);\n"
            "}\n";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        glUseProgram(program);
        expectError(GL_NO_ERROR);

        GLint location;

        location = glGetUniformLocation(program, "floatUniform");
        glUniform1f(location, 1.0f);
        verifyUniformValue1f(m_testCtx, *this, program, location, 1.0f);

        location = glGetUniformLocation(program, "float2Uniform");
        glUniform2f(location, 1.0f, 2.0f);
        verifyUniformValue2f(m_testCtx, *this, program, location, 1.0f, 2.0f);

        location = glGetUniformLocation(program, "float3Uniform");
        glUniform3f(location, 1.0f, 2.0f, 3.0f);
        verifyUniformValue3f(m_testCtx, *this, program, location, 1.0f, 2.0f, 3.0f);

        location = glGetUniformLocation(program, "float4Uniform");
        glUniform4f(location, 1.0f, 2.0f, 3.0f, 4.0f);
        verifyUniformValue4f(m_testCtx, *this, program, location, 1.0f, 2.0f, 3.0f, 4.0f);

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class UniformValueIntCase : public ApiCase
{
public:
    UniformValueIntCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        static const char *testVertSource =
            "#version 300 es\n"
            "uniform highp int intUniform;\n"
            "uniform highp ivec2 int2Uniform;\n"
            "uniform highp ivec3 int3Uniform;\n"
            "uniform highp ivec4 int4Uniform;\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(float(intUniform + int2Uniform.x + int3Uniform.x + int4Uniform.x));\n"
            "}\n";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        glUseProgram(program);
        expectError(GL_NO_ERROR);

        GLint location;

        location = glGetUniformLocation(program, "intUniform");
        glUniform1i(location, 1);
        verifyUniformValue1i(m_testCtx, *this, program, location, 1);

        location = glGetUniformLocation(program, "int2Uniform");
        glUniform2i(location, 1, 2);
        verifyUniformValue2i(m_testCtx, *this, program, location, 1, 2);

        location = glGetUniformLocation(program, "int3Uniform");
        glUniform3i(location, 1, 2, 3);
        verifyUniformValue3i(m_testCtx, *this, program, location, 1, 2, 3);

        location = glGetUniformLocation(program, "int4Uniform");
        glUniform4i(location, 1, 2, 3, 4);
        verifyUniformValue4i(m_testCtx, *this, program, location, 1, 2, 3, 4);

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class UniformValueUintCase : public ApiCase
{
public:
    UniformValueUintCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        static const char *testVertSource =
            "#version 300 es\n"
            "uniform highp uint uintUniform;\n"
            "uniform highp uvec2 uint2Uniform;\n"
            "uniform highp uvec3 uint3Uniform;\n"
            "uniform highp uvec4 uint4Uniform;\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(float(uintUniform + uint2Uniform.x + uint3Uniform.x + uint4Uniform.x));\n"
            "}\n";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        glUseProgram(program);
        expectError(GL_NO_ERROR);

        GLint location;

        location = glGetUniformLocation(program, "uintUniform");
        glUniform1ui(location, 1);
        verifyUniformValue1ui(m_testCtx, *this, program, location, 1);

        location = glGetUniformLocation(program, "uint2Uniform");
        glUniform2ui(location, 1, 2);
        verifyUniformValue2ui(m_testCtx, *this, program, location, 1, 2);

        location = glGetUniformLocation(program, "uint3Uniform");
        glUniform3ui(location, 1, 2, 3);
        verifyUniformValue3ui(m_testCtx, *this, program, location, 1, 2, 3);

        location = glGetUniformLocation(program, "uint4Uniform");
        glUniform4ui(location, 1, 2, 3, 4);
        verifyUniformValue4ui(m_testCtx, *this, program, location, 1, 2, 3, 4);

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class UniformValueBooleanCase : public ApiCase
{
public:
    UniformValueBooleanCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        static const char *testVertSource = "#version 300 es\n"
                                            "uniform bool boolUniform;\n"
                                            "uniform bvec2 bool2Uniform;\n"
                                            "uniform bvec3 bool3Uniform;\n"
                                            "uniform bvec4 bool4Uniform;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(float(boolUniform) + float(bool2Uniform.x) + "
                                            "float(bool3Uniform.x) + float(bool4Uniform.x));\n"
                                            "}\n";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        glUseProgram(program);
        expectError(GL_NO_ERROR);

        GLint location;

        // int conversion

        location = glGetUniformLocation(program, "boolUniform");
        glUniform1i(location, 1);
        verifyUniformValue1i(m_testCtx, *this, program, location, 1);

        location = glGetUniformLocation(program, "bool2Uniform");
        glUniform2i(location, 1, 2);
        verifyUniformValue2i(m_testCtx, *this, program, location, 1, 1);

        location = glGetUniformLocation(program, "bool3Uniform");
        glUniform3i(location, 0, 1, 2);
        verifyUniformValue3i(m_testCtx, *this, program, location, 0, 1, 1);

        location = glGetUniformLocation(program, "bool4Uniform");
        glUniform4i(location, 1, 0, 1, -1);
        verifyUniformValue4i(m_testCtx, *this, program, location, 1, 0, 1, 1);

        // float conversion

        location = glGetUniformLocation(program, "boolUniform");
        glUniform1f(location, 1.0f);
        verifyUniformValue1i(m_testCtx, *this, program, location, 1);

        location = glGetUniformLocation(program, "bool2Uniform");
        glUniform2f(location, 1.0f, 0.1f);
        verifyUniformValue2i(m_testCtx, *this, program, location, 1, 1);

        location = glGetUniformLocation(program, "bool3Uniform");
        glUniform3f(location, 0.0f, 0.1f, -0.1f);
        verifyUniformValue3i(m_testCtx, *this, program, location, 0, 1, 1);

        location = glGetUniformLocation(program, "bool4Uniform");
        glUniform4f(location, 1.0f, 0.0f, 0.1f, -0.9f);
        verifyUniformValue4i(m_testCtx, *this, program, location, 1, 0, 1, 1);

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class UniformValueSamplerCase : public ApiCase
{
public:
    UniformValueSamplerCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        static const char *testVertSource = "#version 300 es\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(0.0);\n"
                                            "}\n";
        static const char *testFragSource = "#version 300 es\n"
                                            "uniform highp sampler2D uniformSampler;\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(textureSize(uniformSampler, 0).x);\n"
                                            "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        glUseProgram(program);
        expectError(GL_NO_ERROR);

        GLint location;

        location = glGetUniformLocation(program, "uniformSampler");
        glUniform1i(location, 1);
        verifyUniformValue1i(m_testCtx, *this, program, location, 1);

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class UniformValueArrayCase : public ApiCase
{
public:
    UniformValueArrayCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        static const char *testVertSource = "#version 300 es\n"
                                            "uniform highp float arrayUniform[5];"
                                            "uniform highp vec2 array2Uniform[5];"
                                            "uniform highp vec3 array3Uniform[5];"
                                            "uniform highp vec4 array4Uniform[5];"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = \n"
                                            "        + vec4(arrayUniform[0]        + arrayUniform[1]        + "
                                            "arrayUniform[2]        + arrayUniform[3]        + arrayUniform[4])\n"
                                            "        + vec4(array2Uniform[0].x    + array2Uniform[1].x    + "
                                            "array2Uniform[2].x    + array2Uniform[3].x    + array2Uniform[4].x)\n"
                                            "        + vec4(array3Uniform[0].x    + array3Uniform[1].x    + "
                                            "array3Uniform[2].x    + array3Uniform[3].x    + array3Uniform[4].x)\n"
                                            "        + vec4(array4Uniform[0].x    + array4Uniform[1].x    + "
                                            "array4Uniform[2].x    + array4Uniform[3].x    + array4Uniform[4].x);\n"
                                            "}\n";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        glUseProgram(program);
        expectError(GL_NO_ERROR);

        GLint location;

        float uniformValue[5 * 4] = {-1.0f, 0.1f,  4.0f,  800.0f, 13.0f, 55.0f, 12.0f, 91.0f, -55.1f, 1.1f,
                                     98.0f, 19.0f, 41.0f, 65.0f,  4.0f,  12.2f, 95.0f, 77.0f, 32.0f,  48.0f};

        location = glGetUniformLocation(program, "arrayUniform");
        glUniform1fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue1f(m_testCtx, *this, program, glGetUniformLocation(program, "arrayUniform[0]"),
                             uniformValue[0]);
        verifyUniformValue1f(m_testCtx, *this, program, glGetUniformLocation(program, "arrayUniform[1]"),
                             uniformValue[1]);
        verifyUniformValue1f(m_testCtx, *this, program, glGetUniformLocation(program, "arrayUniform[2]"),
                             uniformValue[2]);
        verifyUniformValue1f(m_testCtx, *this, program, glGetUniformLocation(program, "arrayUniform[3]"),
                             uniformValue[3]);
        verifyUniformValue1f(m_testCtx, *this, program, glGetUniformLocation(program, "arrayUniform[4]"),
                             uniformValue[4]);
        expectError(GL_NO_ERROR);

        location = glGetUniformLocation(program, "array2Uniform");
        glUniform2fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue2f(m_testCtx, *this, program, glGetUniformLocation(program, "array2Uniform[0]"),
                             uniformValue[2 * 0], uniformValue[(2 * 0) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program, glGetUniformLocation(program, "array2Uniform[1]"),
                             uniformValue[2 * 1], uniformValue[(2 * 1) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program, glGetUniformLocation(program, "array2Uniform[2]"),
                             uniformValue[2 * 2], uniformValue[(2 * 2) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program, glGetUniformLocation(program, "array2Uniform[3]"),
                             uniformValue[2 * 3], uniformValue[(2 * 3) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program, glGetUniformLocation(program, "array2Uniform[4]"),
                             uniformValue[2 * 4], uniformValue[(2 * 4) + 1]);
        expectError(GL_NO_ERROR);

        location = glGetUniformLocation(program, "array3Uniform");
        glUniform3fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue3f(m_testCtx, *this, program, glGetUniformLocation(program, "array3Uniform[0]"),
                             uniformValue[3 * 0], uniformValue[(3 * 0) + 1], uniformValue[(3 * 0) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program, glGetUniformLocation(program, "array3Uniform[1]"),
                             uniformValue[3 * 1], uniformValue[(3 * 1) + 1], uniformValue[(3 * 1) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program, glGetUniformLocation(program, "array3Uniform[2]"),
                             uniformValue[3 * 2], uniformValue[(3 * 2) + 1], uniformValue[(3 * 2) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program, glGetUniformLocation(program, "array3Uniform[3]"),
                             uniformValue[3 * 3], uniformValue[(3 * 3) + 1], uniformValue[(3 * 3) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program, glGetUniformLocation(program, "array3Uniform[4]"),
                             uniformValue[3 * 4], uniformValue[(3 * 4) + 1], uniformValue[(3 * 4) + 2]);
        expectError(GL_NO_ERROR);

        location = glGetUniformLocation(program, "array4Uniform");
        glUniform4fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue4f(m_testCtx, *this, program, glGetUniformLocation(program, "array4Uniform[0]"),
                             uniformValue[4 * 0], uniformValue[(4 * 0) + 1], uniformValue[(4 * 0) + 2],
                             uniformValue[(4 * 0) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program, glGetUniformLocation(program, "array4Uniform[1]"),
                             uniformValue[4 * 1], uniformValue[(4 * 1) + 1], uniformValue[(4 * 1) + 2],
                             uniformValue[(4 * 1) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program, glGetUniformLocation(program, "array4Uniform[2]"),
                             uniformValue[4 * 2], uniformValue[(4 * 2) + 1], uniformValue[(4 * 2) + 2],
                             uniformValue[(4 * 2) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program, glGetUniformLocation(program, "array4Uniform[3]"),
                             uniformValue[4 * 3], uniformValue[(4 * 3) + 1], uniformValue[(4 * 3) + 2],
                             uniformValue[(4 * 3) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program, glGetUniformLocation(program, "array4Uniform[4]"),
                             uniformValue[4 * 4], uniformValue[(4 * 4) + 1], uniformValue[(4 * 4) + 2],
                             uniformValue[(4 * 4) + 3]);
        expectError(GL_NO_ERROR);

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class UniformValueMatrixCase : public ApiCase
{
public:
    UniformValueMatrixCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        static const char *testVertSource =
            "#version 300 es\n"
            "uniform highp mat2 mat2Uniform;"
            "uniform highp mat3 mat3Uniform;"
            "uniform highp mat4 mat4Uniform;"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(mat2Uniform[0][0] + mat3Uniform[0][0] + mat4Uniform[0][0]);\n"
            "}\n";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &testVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &testFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);
        glUseProgram(program);
        expectError(GL_NO_ERROR);

        GLint location;

        float matrixValues[4 * 4] = {
            -1.0f,  0.1f, 4.0f,  800.0f, 13.0f, 55.0f, 12.0f, 91.0f,
            -55.1f, 1.1f, 98.0f, 19.0f,  41.0f, 65.0f, 4.0f,  12.2f,
        };

        // the values of the matrix are returned in column major order but they can be given in either order

        location = glGetUniformLocation(program, "mat2Uniform");
        glUniformMatrix2fv(location, 1, GL_FALSE, matrixValues);
        verifyUniformMatrixValues<2>(m_testCtx, *this, program, location, matrixValues, false);
        glUniformMatrix2fv(location, 1, GL_TRUE, matrixValues);
        verifyUniformMatrixValues<2>(m_testCtx, *this, program, location, matrixValues, true);

        location = glGetUniformLocation(program, "mat3Uniform");
        glUniformMatrix3fv(location, 1, GL_FALSE, matrixValues);
        verifyUniformMatrixValues<3>(m_testCtx, *this, program, location, matrixValues, false);
        glUniformMatrix3fv(location, 1, GL_TRUE, matrixValues);
        verifyUniformMatrixValues<3>(m_testCtx, *this, program, location, matrixValues, true);

        location = glGetUniformLocation(program, "mat4Uniform");
        glUniformMatrix4fv(location, 1, GL_FALSE, matrixValues);
        verifyUniformMatrixValues<4>(m_testCtx, *this, program, location, matrixValues, false);
        glUniformMatrix4fv(location, 1, GL_TRUE, matrixValues);
        verifyUniformMatrixValues<4>(m_testCtx, *this, program, location, matrixValues, true);

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
        expectError(GL_NO_ERROR);
    }
};

class PrecisionFormatCase : public ApiCase
{
public:
    struct RequiredFormat
    {
        int negativeRange;
        int positiveRange;
        int precision;
    };

    PrecisionFormatCase(Context &context, const char *name, const char *description, glw::GLenum shaderType,
                        glw::GLenum precisionType)
        : ApiCase(context, name, description)
        , m_shaderType(shaderType)
        , m_precisionType(precisionType)
    {
    }

private:
    void test(void)
    {
        const RequiredFormat expected = getRequiredFormat();
        bool error                    = false;
        gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLboolean> shaderCompiler;
        gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint[2]> range;
        gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint> precision;

        // query values
        glGetShaderPrecisionFormat(m_shaderType, m_precisionType, range, &precision);
        expectError(GL_NO_ERROR);

        if (!range.verifyValidity(m_testCtx))
            return;
        if (!precision.verifyValidity(m_testCtx))
            return;

        m_log << tcu::TestLog::Message << "range[0] = " << range[0] << "\n"
              << "range[1] = " << range[1] << "\n"
              << "precision = " << precision << tcu::TestLog::EndMessage;

        // verify values

        if (m_precisionType == GL_HIGH_FLOAT)
        {
            // highp float must be IEEE 754 single

            if (range[0] != expected.negativeRange || range[1] != expected.positiveRange ||
                precision != expected.precision)
            {
                m_log << tcu::TestLog::Message << "// ERROR: Invalid precision format, expected:\n"
                      << "\trange[0] = " << expected.negativeRange << "\n"
                      << "\trange[1] = " << expected.positiveRange << "\n"
                      << "\tprecision = " << expected.precision << tcu::TestLog::EndMessage;
                error = true;
            }
        }
        else
        {
            if (range[0] < expected.negativeRange)
            {
                m_log << tcu::TestLog::Message << "// ERROR: Invalid range[0], expected greater or equal to "
                      << expected.negativeRange << tcu::TestLog::EndMessage;
                error = true;
            }

            if (range[1] < expected.positiveRange)
            {
                m_log << tcu::TestLog::Message << "// ERROR: Invalid range[1], expected greater or equal to "
                      << expected.positiveRange << tcu::TestLog::EndMessage;
                error = true;
            }

            if (precision < expected.precision)
            {
                m_log << tcu::TestLog::Message << "// ERROR: Invalid precision, expected greater or equal to "
                      << expected.precision << tcu::TestLog::EndMessage;
                error = true;
            }
        }

        if (error)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid precision/range");
    }

    RequiredFormat getRequiredFormat(void) const
    {
        // Precisions for different types.
        const RequiredFormat requirements[] = {
            {0, 0, 8},      //!< lowp float
            {13, 13, 10},   //!< mediump float
            {127, 127, 23}, //!< highp float
            {8, 7, 0},      //!< lowp int
            {15, 14, 0},    //!< mediump int
            {31, 30, 0},    //!< highp int
        };
        const int ndx = (int)m_precisionType - (int)GL_LOW_FLOAT;

        DE_ASSERT(ndx >= 0);
        DE_ASSERT(ndx < DE_LENGTH_OF_ARRAY(requirements));
        return requirements[ndx];
    }

    const glw::GLenum m_shaderType;
    const glw::GLenum m_precisionType;
};

} // namespace

ShaderStateQueryTests::ShaderStateQueryTests(Context &context)
    : TestCaseGroup(context, "shader", "Shader State Query tests")
{
}

void ShaderStateQueryTests::init(void)
{
    // shader
    addChild(new ShaderTypeCase(m_context, "shader_type", "SHADER_TYPE"));
    addChild(new ShaderCompileStatusCase(m_context, "shader_compile_status", "COMPILE_STATUS"));
    addChild(new ShaderInfoLogCase(m_context, "shader_info_log_length", "INFO_LOG_LENGTH"));
    addChild(new ShaderSourceCase(m_context, "shader_source_length", "SHADER_SOURCE_LENGTH"));

    // shader and program
    addChild(new DeleteStatusCase(m_context, "delete_status", "DELETE_STATUS"));

    // vertex-attrib
    addChild(new CurrentVertexAttribInitialCase(m_context, "current_vertex_attrib_initial", "CURRENT_VERTEX_ATTRIB"));
    addChild(new CurrentVertexAttribFloatCase(m_context, "current_vertex_attrib_float", "CURRENT_VERTEX_ATTRIB"));
    addChild(new CurrentVertexAttribIntCase(m_context, "current_vertex_attrib_int", "CURRENT_VERTEX_ATTRIB"));
    addChild(new CurrentVertexAttribUintCase(m_context, "current_vertex_attrib_uint", "CURRENT_VERTEX_ATTRIB"));
    addChild(new CurrentVertexAttribConversionCase(m_context, "current_vertex_attrib_float_to_int",
                                                   "CURRENT_VERTEX_ATTRIB"));

    // program
    addChild(new ProgramInfoLogCase(m_context, "program_info_log_length", "INFO_LOG_LENGTH",
                                    ProgramInfoLogCase::BUILDERROR_COMPILE));
    addChild(new ProgramInfoLogCase(m_context, "program_info_log_length_link_error", "INFO_LOG_LENGTH",
                                    ProgramInfoLogCase::BUILDERROR_LINK));
    addChild(new ProgramValidateStatusCase(m_context, "program_validate_status", "VALIDATE_STATUS"));
    addChild(new ProgramAttachedShadersCase(m_context, "program_attached_shaders", "ATTACHED_SHADERS"));

    addChild(new ProgramActiveUniformNameCase(m_context, "program_active_uniform_name",
                                              "ACTIVE_UNIFORMS and ACTIVE_UNIFORM_MAX_LENGTH"));
    addChild(new ProgramUniformCase(m_context, "program_active_uniform_types",
                                    "UNIFORM_TYPE, UNIFORM_SIZE, and UNIFORM_IS_ROW_MAJOR"));
    addChild(new ProgramActiveUniformBlocksCase(m_context, "program_active_uniform_blocks", "ACTIVE_UNIFORM_BLOCK_x"));
    addChild(new ProgramBinaryCase(m_context, "program_binary",
                                   "PROGRAM_BINARY_LENGTH and PROGRAM_BINARY_RETRIEVABLE_HINT"));

    // transform feedback
    addChild(new TransformFeedbackCase(
        m_context, "transform_feedback",
        "TRANSFORM_FEEDBACK_BUFFER_MODE, TRANSFORM_FEEDBACK_VARYINGS, TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH"));

    // attribute related
    addChild(
        new ActiveAttributesCase(m_context, "active_attributes", "ACTIVE_ATTRIBUTES and ACTIVE_ATTRIBUTE_MAX_LENGTH"));
    addChild(new VertexAttributeSizeCase(m_context, "vertex_attrib_size", "VERTEX_ATTRIB_ARRAY_SIZE"));
    addChild(new VertexAttributeTypeCase(m_context, "vertex_attrib_type", "VERTEX_ATTRIB_ARRAY_TYPE"));
    addChild(new VertexAttributeStrideCase(m_context, "vertex_attrib_stride", "VERTEX_ATTRIB_ARRAY_STRIDE"));
    addChild(
        new VertexAttributeNormalizedCase(m_context, "vertex_attrib_normalized", "VERTEX_ATTRIB_ARRAY_NORMALIZED"));
    addChild(new VertexAttributeIntegerCase(m_context, "vertex_attrib_integer", "VERTEX_ATTRIB_ARRAY_INTEGER"));
    addChild(new VertexAttributeEnabledCase(m_context, "vertex_attrib_array_enabled", "VERTEX_ATTRIB_ARRAY_ENABLED"));
    addChild(new VertexAttributeDivisorCase(m_context, "vertex_attrib_array_divisor", "VERTEX_ATTRIB_ARRAY_DIVISOR"));
    addChild(new VertexAttributeBufferBindingCase(m_context, "vertex_attrib_array_buffer_binding",
                                                  "VERTEX_ATTRIB_ARRAY_BUFFER_BINDING"));
    addChild(new VertexAttributePointerCase(m_context, "vertex_attrib_pointerv", "GetVertexAttribPointerv"));

    // uniform values
    addChild(new UniformValueFloatCase(m_context, "uniform_value_float", "GetUniform*"));
    addChild(new UniformValueIntCase(m_context, "uniform_value_int", "GetUniform*"));
    addChild(new UniformValueUintCase(m_context, "uniform_value_uint", "GetUniform*"));
    addChild(new UniformValueBooleanCase(m_context, "uniform_value_boolean", "GetUniform*"));
    addChild(new UniformValueSamplerCase(m_context, "uniform_value_sampler", "GetUniform*"));
    addChild(new UniformValueArrayCase(m_context, "uniform_value_array", "GetUniform*"));
    addChild(new UniformValueMatrixCase(m_context, "uniform_value_matrix", "GetUniform*"));

    // precision format query
    addChild(new PrecisionFormatCase(m_context, "precision_vertex_lowp_float", "GetShaderPrecisionFormat",
                                     GL_VERTEX_SHADER, GL_LOW_FLOAT));
    addChild(new PrecisionFormatCase(m_context, "precision_vertex_mediump_float", "GetShaderPrecisionFormat",
                                     GL_VERTEX_SHADER, GL_MEDIUM_FLOAT));
    addChild(new PrecisionFormatCase(m_context, "precision_vertex_highp_float", "GetShaderPrecisionFormat",
                                     GL_VERTEX_SHADER, GL_HIGH_FLOAT));
    addChild(new PrecisionFormatCase(m_context, "precision_vertex_lowp_int", "GetShaderPrecisionFormat",
                                     GL_VERTEX_SHADER, GL_LOW_INT));
    addChild(new PrecisionFormatCase(m_context, "precision_vertex_mediump_int", "GetShaderPrecisionFormat",
                                     GL_VERTEX_SHADER, GL_MEDIUM_INT));
    addChild(new PrecisionFormatCase(m_context, "precision_vertex_highp_int", "GetShaderPrecisionFormat",
                                     GL_VERTEX_SHADER, GL_HIGH_INT));
    addChild(new PrecisionFormatCase(m_context, "precision_fragment_lowp_float", "GetShaderPrecisionFormat",
                                     GL_FRAGMENT_SHADER, GL_LOW_FLOAT));
    addChild(new PrecisionFormatCase(m_context, "precision_fragment_mediump_float", "GetShaderPrecisionFormat",
                                     GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT));
    addChild(new PrecisionFormatCase(m_context, "precision_fragment_highp_float", "GetShaderPrecisionFormat",
                                     GL_FRAGMENT_SHADER, GL_HIGH_FLOAT));
    addChild(new PrecisionFormatCase(m_context, "precision_fragment_lowp_int", "GetShaderPrecisionFormat",
                                     GL_FRAGMENT_SHADER, GL_LOW_INT));
    addChild(new PrecisionFormatCase(m_context, "precision_fragment_mediump_int", "GetShaderPrecisionFormat",
                                     GL_FRAGMENT_SHADER, GL_MEDIUM_INT));
    addChild(new PrecisionFormatCase(m_context, "precision_fragment_highp_int", "GetShaderPrecisionFormat",
                                     GL_FRAGMENT_SHADER, GL_HIGH_INT));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
