/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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

#include "es2fShaderStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es2fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace
{

static const char *commonTestVertSource = "void main (void)\n"
                                          "{\n"
                                          "    gl_Position = vec4(0.0);\n"
                                          "}\n";
static const char *commonTestFragSource = "void main (void)\n"
                                          "{\n"
                                          "    gl_FragColor = vec4(0.0);\n"
                                          "}\n";

static const char *brokenShader = "broken, this should not compile!\n"
                                  "\n";

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

    if (state.verifyValidity(testCtx))
        return checkIntEquals(testCtx, state, reference);
    return false;
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
    gl.glGetVertexAttribiv(index, pname, &state);

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

void requireShaderCompiler(tcu::TestContext &testCtx, glu::CallLogWrapper &gl)
{
    StateQueryMemoryWriteGuard<GLboolean> state;
    gl.glGetBooleanv(GL_SHADER_COMPILER, &state);

    if (!state.verifyValidity(testCtx) || state != GL_TRUE)
        throw tcu::NotSupportedError("Test requires SHADER_COMPILER = TRUE");
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
        requireShaderCompiler(m_testCtx, *this);

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
        requireShaderCompiler(m_testCtx, *this);

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
        requireShaderCompiler(m_testCtx, *this);

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
    ProgramInfoLogCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        using tcu::TestLog;

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &brokenShader, nullptr);
        glShaderSource(shaderFrag, 1, &brokenShader, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint program = glCreateProgram();
        glAttachShader(program, shaderVert);
        glAttachShader(program, shaderFrag);
        glLinkProgram(program);

        // check INFO_LOG_LENGTH == GetProgramInfoLog len
        {
            char buffer[2048] = {'x'};

            GLint written = 0;
            glGetProgramInfoLog(program, DE_LENGTH_OF_ARRAY(buffer), &written, buffer);

            StateQueryMemoryWriteGuard<GLint> logLength;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            logLength.verifyValidity(m_testCtx);

            if (logLength != 0 && written + 1 != logLength) // INFO_LOG_LENGTH contains 0-terminator
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: Expected INFO_LOG_LENGTH " << written + 1
                                   << "; got " << logLength << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid log length");
            }
        }

        // check GetProgramInfoLog works with too small buffer
        {
            char buffer[2048] = {'x'};

            GLint written = 0;
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

        static const char *testVertSource = "uniform highp float uniformNameWithLength23;\n"
                                            "uniform highp vec2 uniformVec2;\n"
                                            "uniform highp mat4 uniformMat4;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(0.0) + vec4(uniformNameWithLength23) + "
                                            "vec4(uniformVec2.x) + vec4(uniformMat4[2][3]);\n"
                                            "}\n\0";
        static const char *testFragSource =

            "void main (void)\n"
            "{\n"
            "    gl_FragColor = vec4(0.0);\n"
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

        // check names
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uniformNames); ++ndx)
        {
            char buffer[2048] = {'x'};
            char *bufferEnd   = (buffer + 1);

            GLint written = 0; // null terminator not included
            GLint size    = 0;
            GLenum type   = 0;
            glGetActiveUniform(program, ndx, DE_LENGTH_OF_ARRAY(buffer), &written, &size, &type, buffer);

            if (written < DE_LENGTH_OF_ARRAY(buffer))
                bufferEnd = &buffer[written];

            // find matching uniform
            {
                const std::string uniformName(buffer, bufferEnd);
                bool found = false;

                for (int uniformNdx = 0; uniformNdx < DE_LENGTH_OF_ARRAY(uniformNames); ++uniformNdx)
                {
                    if (uniformName == uniformNames[uniformNdx])
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    m_testCtx.getLog() << TestLog::Message << "// ERROR: Got unknown uniform name: " << uniformName
                                       << TestLog::EndMessage;
                    if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got wrong uniform name");
                }
            }

            // and with too small buffer
            written = 0;
            glGetActiveUniform(program, ndx, 1, &written, &size, &type, buffer);

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
            {"float", "[2]", "highp", "", "uniformValue[1]", GL_FLOAT, 2, GL_FALSE},
            {"vec2", "", "highp", "", "uniformValue.x", GL_FLOAT_VEC2, 1, GL_FALSE},
            {"vec3", "", "highp", "", "uniformValue.x", GL_FLOAT_VEC3, 1, GL_FALSE},
            {"vec4", "", "highp", "", "uniformValue.x", GL_FLOAT_VEC4, 1, GL_FALSE},
            {"int", "", "highp", "", "float(uniformValue)", GL_INT, 1, GL_FALSE},
            {"ivec2", "", "highp", "", "float(uniformValue.x)", GL_INT_VEC2, 1, GL_FALSE},
            {"ivec3", "", "highp", "", "float(uniformValue.x)", GL_INT_VEC3, 1, GL_FALSE},
            {"ivec4", "", "highp", "", "float(uniformValue.x)", GL_INT_VEC4, 1, GL_FALSE},
            {"bool", "", "", "", "float(uniformValue)", GL_BOOL, 1, GL_FALSE},
            {"bvec2", "", "", "", "float(uniformValue.x)", GL_BOOL_VEC2, 1, GL_FALSE},
            {"bvec3", "", "", "", "float(uniformValue.x)", GL_BOOL_VEC3, 1, GL_FALSE},
            {"bvec4", "", "", "", "float(uniformValue.x)", GL_BOOL_VEC4, 1, GL_FALSE},
            {"mat2", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT2, 1, GL_FALSE},
            {"mat3", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT3, 1, GL_FALSE},
            {"mat4", "", "highp", "", "float(uniformValue[0][0])", GL_FLOAT_MAT4, 1, GL_FALSE},
            {"sampler2D", "", "highp", "", "float(texture2D(uniformValue, vec2(0.0, 0.0)).r)", GL_SAMPLER_2D, 1,
             GL_FALSE},
            {"samplerCube", "", "highp", "", "float(textureCube(uniformValue, vec3(0.0, 0.0, 0.0)).r)", GL_SAMPLER_CUBE,
             1, GL_FALSE},
        };

        static const char *vertSource = "void main (void)\n"
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
            frag << uniformTypes[ndx].layout << "uniform " << uniformTypes[ndx].precision << " "
                 << uniformTypes[ndx].declaration << " uniformValue" << uniformTypes[ndx].postDeclaration << ";\n";
            frag << "void main (void)\n";
            frag << "{\n";
            frag << "    gl_FragColor = vec4(" << uniformTypes[ndx].getter << ");\n";
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
                const GLint index = 0; // first and only active uniform

                char buffer[] = "not written to"; // not written to
                GLint written = 0;
                GLint size    = 0;
                GLenum type   = 0;
                glGetActiveUniform(program, index, 0, &written, &size, &type, buffer);

                checkIntEquals(m_testCtx, type, uniformTypes[ndx].type);
                checkIntEquals(m_testCtx, size, uniformTypes[ndx].size);
            }
        }

        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(program);
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

        static const char *testVertSource = "attribute highp vec2 longInputAttributeName;\n"
                                            "attribute highp vec2 shortName;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = longInputAttributeName.yxxy + shortName.xyxy;\n"
                                            "}\n\0";
        static const char *testFragSource = "void main (void)\n"
                                            "{\n"
                                            "    gl_FragColor = vec4(0.0);\n"
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
    void *pointer;
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

        // test VertexAttribPointer
        const PointerData pointers[] = {
            // size test
            {4, GL_FLOAT, 0, GL_FALSE, vertexData}, {3, GL_FLOAT, 0, GL_FALSE, vertexData},
            {2, GL_FLOAT, 0, GL_FALSE, vertexData}, {1, GL_FLOAT, 0, GL_FALSE, vertexData},
            {4, GL_SHORT, 0, GL_FALSE, vertexData}, {3, GL_SHORT, 0, GL_FALSE, vertexData},
            {2, GL_SHORT, 0, GL_FALSE, vertexData}, {1, GL_SHORT, 0, GL_FALSE, vertexData},
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
        {
            glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                  pointers[ndx].stride, pointers[ndx].pointer);
            expectError(GL_NO_ERROR);

            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_SIZE, pointers[ndx].size);
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
        GLfloat vertexData[4] = {0.0f}; // never accessed

        const PointerData pointers[] = {
            {1, GL_BYTE, 0, GL_FALSE, vertexData},  {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData},
            {1, GL_SHORT, 0, GL_FALSE, vertexData}, {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
            {1, GL_FIXED, 0, GL_FALSE, vertexData}, {1, GL_FLOAT, 0, GL_FALSE, vertexData},
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
        {
            glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                  pointers[ndx].stride, pointers[ndx].pointer);
            expectError(GL_NO_ERROR);

            verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_TYPE, pointers[ndx].type);
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
        GLfloat vertexData[4] = {0.0f}; // never accessed

        struct StridePointerData
        {
            GLint size;
            GLenum type;
            GLint stride;
            void *pointer;
        };

        // test VertexAttribPointer
        {
            const StridePointerData pointers[] = {
                {1, GL_FLOAT, 0, vertexData},          {1, GL_FLOAT, 1, vertexData},
                {1, GL_FLOAT, 4, vertexData},          {1, GL_SHORT, 0, vertexData},
                {1, GL_SHORT, 1, vertexData},          {1, GL_SHORT, 4, vertexData},
                {1, GL_FIXED, 0, vertexData},          {1, GL_FIXED, 1, vertexData},
                {1, GL_FIXED, 4, vertexData},          {1, GL_BYTE, 0, vertexData},
                {1, GL_UNSIGNED_SHORT, 1, vertexData}, {1, GL_UNSIGNED_SHORT, 4, vertexData},
                {4, GL_UNSIGNED_BYTE, 0, vertexData},  {4, GL_UNSIGNED_BYTE, 1, vertexData},
                {4, GL_UNSIGNED_BYTE, 4, vertexData},
            };

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
            {
                glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, GL_FALSE, pointers[ndx].stride,
                                      pointers[ndx].pointer);
                expectError(GL_NO_ERROR);

                verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, pointers[ndx].stride);
            }
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
        GLfloat vertexData[4] = {0.0f}; // never accessed

        // test VertexAttribPointer
        {
            const PointerData pointers[] = {
                {1, GL_BYTE, 0, GL_FALSE, vertexData},          {1, GL_SHORT, 0, GL_FALSE, vertexData},
                {1, GL_UNSIGNED_BYTE, 0, GL_FALSE, vertexData}, {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, vertexData},
                {1, GL_BYTE, 0, GL_TRUE, vertexData},           {1, GL_SHORT, 0, GL_TRUE, vertexData},
                {1, GL_UNSIGNED_BYTE, 0, GL_TRUE, vertexData},  {1, GL_UNSIGNED_SHORT, 0, GL_TRUE, vertexData},
            };

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pointers); ++ndx)
            {
                glVertexAttribPointer(0, pointers[ndx].size, pointers[ndx].type, pointers[ndx].normalized,
                                      pointers[ndx].stride, pointers[ndx].pointer);
                expectError(GL_NO_ERROR);

                verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, pointers[ndx].normalized);
            }
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

        verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_FALSE);
        glEnableVertexAttribArray(0);
        verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_TRUE);
        glDisableVertexAttribArray(0);
        verifyVertexAttrib(m_testCtx, *this, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, GL_FALSE);
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
        StateQueryMemoryWriteGuard<GLvoid *> initialState;
        glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &initialState);
        initialState.verifyValidity(m_testCtx);
        checkPointerEquals(m_testCtx, initialState, 0);

        GLfloat vertexData[4]        = {0.0f}; // never accessed
        const PointerData pointers[] = {
            {1, GL_BYTE, 0, GL_FALSE, &vertexData[2]},           {1, GL_SHORT, 0, GL_FALSE, &vertexData[1]},
            {1, GL_FIXED, 0, GL_FALSE, &vertexData[2]},          {1, GL_FIXED, 0, GL_FALSE, &vertexData[1]},
            {1, GL_FLOAT, 0, GL_FALSE, &vertexData[0]},          {1, GL_FLOAT, 0, GL_FALSE, &vertexData[3]},
            {1, GL_FLOAT, 0, GL_FALSE, &vertexData[2]},          {1, GL_UNSIGNED_SHORT, 0, GL_FALSE, &vertexData[0]},
            {4, GL_UNSIGNED_SHORT, 0, GL_FALSE, &vertexData[1]}, {4, GL_UNSIGNED_SHORT, 0, GL_FALSE, &vertexData[2]},
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
            "uniform highp float floatUniform;\n"
            "uniform highp vec2 float2Uniform;\n"
            "uniform highp vec3 float3Uniform;\n"
            "uniform highp vec4 float4Uniform;\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(floatUniform + float2Uniform.x + float3Uniform.x + float4Uniform.x);\n"
            "}\n";
        static const char *testFragSource =

            "void main (void)\n"
            "{\n"
            "    gl_FragColor = vec4(0.0);\n"
            "}\n";

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(testVertSource, testFragSource));
        if (!program.isOk())
        {
            m_log << program;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to compile shader");
            return;
        }

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        GLint location;

        location = glGetUniformLocation(program.getProgram(), "floatUniform");
        glUniform1f(location, 1.0f);
        verifyUniformValue1f(m_testCtx, *this, program.getProgram(), location, 1.0f);

        location = glGetUniformLocation(program.getProgram(), "float2Uniform");
        glUniform2f(location, 1.0f, 2.0f);
        verifyUniformValue2f(m_testCtx, *this, program.getProgram(), location, 1.0f, 2.0f);

        location = glGetUniformLocation(program.getProgram(), "float3Uniform");
        glUniform3f(location, 1.0f, 2.0f, 3.0f);
        verifyUniformValue3f(m_testCtx, *this, program.getProgram(), location, 1.0f, 2.0f, 3.0f);

        location = glGetUniformLocation(program.getProgram(), "float4Uniform");
        glUniform4f(location, 1.0f, 2.0f, 3.0f, 4.0f);
        verifyUniformValue4f(m_testCtx, *this, program.getProgram(), location, 1.0f, 2.0f, 3.0f, 4.0f);

        glUseProgram(0);
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
            "uniform highp int intUniform;\n"
            "uniform highp ivec2 int2Uniform;\n"
            "uniform highp ivec3 int3Uniform;\n"
            "uniform highp ivec4 int4Uniform;\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(float(intUniform + int2Uniform.x + int3Uniform.x + int4Uniform.x));\n"
            "}\n";
        static const char *testFragSource = "void main (void)\n"
                                            "{\n"
                                            "    gl_FragColor = vec4(0.0);\n"
                                            "}\n";

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(testVertSource, testFragSource));
        if (!program.isOk())
        {
            m_log << program;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to compile shader");
            return;
        }

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        GLint location;

        location = glGetUniformLocation(program.getProgram(), "intUniform");
        glUniform1i(location, 1);
        verifyUniformValue1i(m_testCtx, *this, program.getProgram(), location, 1);

        location = glGetUniformLocation(program.getProgram(), "int2Uniform");
        glUniform2i(location, 1, 2);
        verifyUniformValue2i(m_testCtx, *this, program.getProgram(), location, 1, 2);

        location = glGetUniformLocation(program.getProgram(), "int3Uniform");
        glUniform3i(location, 1, 2, 3);
        verifyUniformValue3i(m_testCtx, *this, program.getProgram(), location, 1, 2, 3);

        location = glGetUniformLocation(program.getProgram(), "int4Uniform");
        glUniform4i(location, 1, 2, 3, 4);
        verifyUniformValue4i(m_testCtx, *this, program.getProgram(), location, 1, 2, 3, 4);

        glUseProgram(0);
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
        static const char *testVertSource = "uniform bool boolUniform;\n"
                                            "uniform bvec2 bool2Uniform;\n"
                                            "uniform bvec3 bool3Uniform;\n"
                                            "uniform bvec4 bool4Uniform;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(float(boolUniform) + float(bool2Uniform.x) + "
                                            "float(bool3Uniform.x) + float(bool4Uniform.x));\n"
                                            "}\n";
        static const char *testFragSource = "void main (void)\n"
                                            "{\n"
                                            "    gl_FragColor = vec4(0.0);\n"
                                            "}\n";

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(testVertSource, testFragSource));
        if (!program.isOk())
        {
            m_log << program;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to compile shader");
            return;
        }

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        GLint location;

        // int conversion

        location = glGetUniformLocation(program.getProgram(), "boolUniform");
        glUniform1i(location, 1);
        verifyUniformValue1i(m_testCtx, *this, program.getProgram(), location, 1);

        location = glGetUniformLocation(program.getProgram(), "bool2Uniform");
        glUniform2i(location, 1, 2);
        verifyUniformValue2i(m_testCtx, *this, program.getProgram(), location, 1, 1);

        location = glGetUniformLocation(program.getProgram(), "bool3Uniform");
        glUniform3i(location, 0, 1, 2);
        verifyUniformValue3i(m_testCtx, *this, program.getProgram(), location, 0, 1, 1);

        location = glGetUniformLocation(program.getProgram(), "bool4Uniform");
        glUniform4i(location, 1, 0, 1, -1);
        verifyUniformValue4i(m_testCtx, *this, program.getProgram(), location, 1, 0, 1, 1);

        // float conversion

        location = glGetUniformLocation(program.getProgram(), "boolUniform");
        glUniform1f(location, 1.0f);
        verifyUniformValue1i(m_testCtx, *this, program.getProgram(), location, 1);

        location = glGetUniformLocation(program.getProgram(), "bool2Uniform");
        glUniform2f(location, 1.0f, 0.1f);
        verifyUniformValue2i(m_testCtx, *this, program.getProgram(), location, 1, 1);

        location = glGetUniformLocation(program.getProgram(), "bool3Uniform");
        glUniform3f(location, 0.0f, 0.1f, -0.1f);
        verifyUniformValue3i(m_testCtx, *this, program.getProgram(), location, 0, 1, 1);

        location = glGetUniformLocation(program.getProgram(), "bool4Uniform");
        glUniform4f(location, 1.0f, 0.0f, 0.1f, -0.9f);
        verifyUniformValue4i(m_testCtx, *this, program.getProgram(), location, 1, 0, 1, 1);

        glUseProgram(0);
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
        static const char *testVertSource = "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(0.0);\n"
                                            "}\n";
        static const char *testFragSource = "uniform highp sampler2D uniformSampler;\n"

                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_FragColor = vec4(texture2D(uniformSampler, vec2(0.0, 0.0)).x);\n"
                                            "}\n";

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(testVertSource, testFragSource));
        if (!program.isOk())
        {
            m_log << program;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to compile shader");
            return;
        }

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        GLint location;

        location = glGetUniformLocation(program.getProgram(), "uniformSampler");
        glUniform1i(location, 1);
        verifyUniformValue1i(m_testCtx, *this, program.getProgram(), location, 1);

        glUseProgram(0);
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
        static const char *testVertSource = "uniform highp float arrayUniform[5];"
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
        static const char *testFragSource =

            "void main (void)\n"
            "{\n"
            "    gl_FragColor = vec4(0.0);\n"
            "}\n";

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(testVertSource, testFragSource));
        if (!program.isOk())
        {
            m_log << program;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to compile shader");
            return;
        }

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        GLint location;

        float uniformValue[5 * 4] = {-1.0f, 0.1f,  4.0f,  800.0f, 13.0f, 55.0f, 12.0f, 91.0f, -55.1f, 1.1f,
                                     98.0f, 19.0f, 41.0f, 65.0f,  4.0f,  12.2f, 95.0f, 77.0f, 32.0f,  48.0f};

        location = glGetUniformLocation(program.getProgram(), "arrayUniform");
        glUniform1fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue1f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "arrayUniform[0]"), uniformValue[0]);
        verifyUniformValue1f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "arrayUniform[1]"), uniformValue[1]);
        verifyUniformValue1f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "arrayUniform[2]"), uniformValue[2]);
        verifyUniformValue1f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "arrayUniform[3]"), uniformValue[3]);
        verifyUniformValue1f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "arrayUniform[4]"), uniformValue[4]);
        expectError(GL_NO_ERROR);

        location = glGetUniformLocation(program.getProgram(), "array2Uniform");
        glUniform2fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue2f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array2Uniform[0]"), uniformValue[2 * 0],
                             uniformValue[(2 * 0) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array2Uniform[1]"), uniformValue[2 * 1],
                             uniformValue[(2 * 1) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array2Uniform[2]"), uniformValue[2 * 2],
                             uniformValue[(2 * 2) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array2Uniform[3]"), uniformValue[2 * 3],
                             uniformValue[(2 * 3) + 1]);
        verifyUniformValue2f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array2Uniform[4]"), uniformValue[2 * 4],
                             uniformValue[(2 * 4) + 1]);
        expectError(GL_NO_ERROR);

        location = glGetUniformLocation(program.getProgram(), "array3Uniform");
        glUniform3fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue3f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array3Uniform[0]"), uniformValue[3 * 0],
                             uniformValue[(3 * 0) + 1], uniformValue[(3 * 0) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array3Uniform[1]"), uniformValue[3 * 1],
                             uniformValue[(3 * 1) + 1], uniformValue[(3 * 1) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array3Uniform[2]"), uniformValue[3 * 2],
                             uniformValue[(3 * 2) + 1], uniformValue[(3 * 2) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array3Uniform[3]"), uniformValue[3 * 3],
                             uniformValue[(3 * 3) + 1], uniformValue[(3 * 3) + 2]);
        verifyUniformValue3f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array3Uniform[4]"), uniformValue[3 * 4],
                             uniformValue[(3 * 4) + 1], uniformValue[(3 * 4) + 2]);
        expectError(GL_NO_ERROR);

        location = glGetUniformLocation(program.getProgram(), "array4Uniform");
        glUniform4fv(location, 5, uniformValue);
        expectError(GL_NO_ERROR);

        verifyUniformValue4f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array4Uniform[0]"), uniformValue[4 * 0],
                             uniformValue[(4 * 0) + 1], uniformValue[(4 * 0) + 2], uniformValue[(4 * 0) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array4Uniform[1]"), uniformValue[4 * 1],
                             uniformValue[(4 * 1) + 1], uniformValue[(4 * 1) + 2], uniformValue[(4 * 1) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array4Uniform[2]"), uniformValue[4 * 2],
                             uniformValue[(4 * 2) + 1], uniformValue[(4 * 2) + 2], uniformValue[(4 * 2) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array4Uniform[3]"), uniformValue[4 * 3],
                             uniformValue[(4 * 3) + 1], uniformValue[(4 * 3) + 2], uniformValue[(4 * 3) + 3]);
        verifyUniformValue4f(m_testCtx, *this, program.getProgram(),
                             glGetUniformLocation(program.getProgram(), "array4Uniform[4]"), uniformValue[4 * 4],
                             uniformValue[(4 * 4) + 1], uniformValue[(4 * 4) + 2], uniformValue[(4 * 4) + 3]);
        expectError(GL_NO_ERROR);

        glUseProgram(0);
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
            "uniform highp mat2 mat2Uniform;"
            "uniform highp mat3 mat3Uniform;"
            "uniform highp mat4 mat4Uniform;"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(mat2Uniform[0][0] + mat3Uniform[0][0] + mat4Uniform[0][0]);\n"
            "}\n";
        static const char *testFragSource =

            "void main (void)\n"
            "{\n"
            "    gl_FragColor = vec4(0.0);\n"
            "}\n";

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(testVertSource, testFragSource));
        if (!program.isOk())
        {
            m_log << program;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to compile shader");
            return;
        }

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        GLint location;

        float matrixValues[4 * 4] = {
            -1.0f,  0.1f, 4.0f,  800.0f, 13.0f, 55.0f, 12.0f, 91.0f,
            -55.1f, 1.1f, 98.0f, 19.0f,  41.0f, 65.0f, 4.0f,  12.2f,
        };

        // the values of the matrix are returned in column major order but they can be given in either order

        location = glGetUniformLocation(program.getProgram(), "mat2Uniform");
        glUniformMatrix2fv(location, 1, GL_FALSE, matrixValues);
        verifyUniformMatrixValues<2>(m_testCtx, *this, program.getProgram(), location, matrixValues, false);

        location = glGetUniformLocation(program.getProgram(), "mat3Uniform");
        glUniformMatrix3fv(location, 1, GL_FALSE, matrixValues);
        verifyUniformMatrixValues<3>(m_testCtx, *this, program.getProgram(), location, matrixValues, false);

        location = glGetUniformLocation(program.getProgram(), "mat4Uniform");
        glUniformMatrix4fv(location, 1, GL_FALSE, matrixValues);
        verifyUniformMatrixValues<4>(m_testCtx, *this, program.getProgram(), location, matrixValues, false);

        glUseProgram(0);
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

        // requires SHADER_COMPILER = true
        glGetBooleanv(GL_SHADER_COMPILER, &shaderCompiler);
        expectError(GL_NO_ERROR);

        if (!shaderCompiler.verifyValidity(m_testCtx))
            return;
        if (shaderCompiler != GL_TRUE)
            throw tcu::NotSupportedError("SHADER_COMPILER = TRUE required");

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

        // special case for highp and fragment shader

        if (m_shaderType == GL_FRAGMENT_SHADER && (m_precisionType == GL_HIGH_FLOAT || m_precisionType == GL_HIGH_INT))
        {
            // not supported is a valid return value
            if (range[0] == 0 && range[1] == 0 && precision == 0)
                return;
        }

        // verify the returned values

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

        if (error)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid precision/range");
    }

    RequiredFormat getRequiredFormat(void) const
    {
        // Precisions for different types.
        // For example highp float: range: (-2^62, 2^62) => min = -2^62 + e, max = 2^62 - e
        const RequiredFormat requirements[] = {
            {0, 0, 8},    //!< lowp float
            {13, 13, 10}, //!< mediump float
            {61, 61, 16}, //!< highp float
            {7, 7, 0},    //!< lowp int
            {9, 9, 0},    //!< mediump int
            {15, 15, 0},  //!< highp int
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
    addChild(new CurrentVertexAttribConversionCase(m_context, "current_vertex_attrib_float_to_int",
                                                   "CURRENT_VERTEX_ATTRIB"));

    // program
    addChild(new ProgramInfoLogCase(m_context, "program_info_log_length", "INFO_LOG_LENGTH"));
    addChild(new ProgramValidateStatusCase(m_context, "program_validate_status", "VALIDATE_STATUS"));
    addChild(new ProgramAttachedShadersCase(m_context, "program_attached_shaders", "ATTACHED_SHADERS"));

    addChild(new ProgramActiveUniformNameCase(m_context, "program_active_uniform_name",
                                              "ACTIVE_UNIFORMS and ACTIVE_UNIFORM_MAX_LENGTH"));
    addChild(new ProgramUniformCase(m_context, "program_active_uniform_types", "UNIFORM_TYPE and UNIFORM_SIZE"));

    // attribute related
    addChild(
        new ActiveAttributesCase(m_context, "active_attributes", "ACTIVE_ATTRIBUTES and ACTIVE_ATTRIBUTE_MAX_LENGTH"));
    addChild(new VertexAttributeSizeCase(m_context, "vertex_attrib_size", "VERTEX_ATTRIB_ARRAY_SIZE"));
    addChild(new VertexAttributeTypeCase(m_context, "vertex_attrib_type", "VERTEX_ATTRIB_ARRAY_TYPE"));
    addChild(new VertexAttributeStrideCase(m_context, "vertex_attrib_stride", "VERTEX_ATTRIB_ARRAY_STRIDE"));
    addChild(
        new VertexAttributeNormalizedCase(m_context, "vertex_attrib_normalized", "VERTEX_ATTRIB_ARRAY_NORMALIZED"));
    addChild(new VertexAttributeEnabledCase(m_context, "vertex_attrib_array_enabled", "VERTEX_ATTRIB_ARRAY_ENABLED"));
    addChild(new VertexAttributeBufferBindingCase(m_context, "vertex_attrib_array_buffer_binding",
                                                  "VERTEX_ATTRIB_ARRAY_BUFFER_BINDING"));
    addChild(new VertexAttributePointerCase(m_context, "vertex_attrib_pointerv", "GetVertexAttribPointerv"));

    // uniform values
    addChild(new UniformValueFloatCase(m_context, "uniform_value_float", "GetUniform*"));
    addChild(new UniformValueIntCase(m_context, "uniform_value_int", "GetUniform*"));
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
} // namespace gles2
} // namespace deqp
