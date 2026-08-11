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
 * \brief Indexed State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fIndexedStateQueryTests.hpp"
#include "es3fApiCase.hpp"
#include "glsStateQueryUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "glwEnums.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluContextInfo.hpp"
#include "deRandom.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

using namespace glw; // GLint and other GL types
using namespace gls::StateQueryUtil;

void checkIntEquals(tcu::TestContext &testCtx, GLint got, GLint expected)
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

void checkIntEquals(tcu::TestContext &testCtx, GLint64 got, GLint64 expected)
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

class TransformFeedbackCase : public ApiCase
{
public:
    TransformFeedbackCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    virtual void testTransformFeedback(void) = 0;

    void test(void)
    {
        static const char *transformFeedbackTestVertSource = "#version 300 es\n"
                                                             "out highp vec4 anotherOutput;\n"
                                                             "void main (void)\n"
                                                             "{\n"
                                                             "    gl_Position = vec4(0.0);\n"
                                                             "    anotherOutput = vec4(0.0);\n"
                                                             "}\n\0";
        static const char *transformFeedbackTestFragSource = "#version 300 es\n"
                                                             "layout(location = 0) out mediump vec4 fragColor;"
                                                             "void main (void)\n"
                                                             "{\n"
                                                             "    fragColor = vec4(0.0);\n"
                                                             "}\n\0";

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shaderVert, 1, &transformFeedbackTestVertSource, nullptr);
        glShaderSource(shaderFrag, 1, &transformFeedbackTestFragSource, nullptr);

        glCompileShader(shaderVert);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);

        GLuint shaderProg = glCreateProgram();
        glAttachShader(shaderProg, shaderVert);
        glAttachShader(shaderProg, shaderFrag);

        const char *transformFeedbackOutputs[] = {"gl_Position", "anotherOutput"};

        glTransformFeedbackVaryings(shaderProg, 2, transformFeedbackOutputs, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(shaderProg);
        expectError(GL_NO_ERROR);

        glGenTransformFeedbacks(2, transformFeedbacks);
        // Also store the default transform feedback in the array.
        transformFeedbacks[2] = 0;
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[0]);
        expectError(GL_NO_ERROR);

        testTransformFeedback();

        // cleanup

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

        glDeleteTransformFeedbacks(2, transformFeedbacks);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(shaderProg);
        expectError(GL_NO_ERROR);
    }

protected:
    GLuint transformFeedbacks[3];
};

class TransformFeedbackBufferBindingCase : public TransformFeedbackCase
{
public:
    TransformFeedbackBufferBindingCase(Context &context, const char *name, const char *description)
        : TransformFeedbackCase(context, name, description)
    {
    }

    void testTransformFeedback(void)
    {
        const int feedbackPositionIndex = 0;
        const int feedbackOutputIndex   = 1;
        const int feedbackIndex[2]      = {feedbackPositionIndex, feedbackOutputIndex};

        // bind bffers

        GLuint feedbackBuffers[2];
        glGenBuffers(2, feedbackBuffers);
        expectError(GL_NO_ERROR);

        for (int ndx = 0; ndx < 2; ++ndx)
        {
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffers[ndx]);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_DYNAMIC_READ);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackIndex[ndx], feedbackBuffers[ndx]);
            expectError(GL_NO_ERROR);
        }

        // test TRANSFORM_FEEDBACK_BUFFER_BINDING

        for (int ndx = 0; ndx < 2; ++ndx)
        {
            StateQueryMemoryWriteGuard<GLint> boundBuffer;
            glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, feedbackIndex[ndx], &boundBuffer);
            boundBuffer.verifyValidity(m_testCtx);
            checkIntEquals(m_testCtx, boundBuffer, feedbackBuffers[ndx]);
        }

        // cleanup

        glDeleteBuffers(2, feedbackBuffers);
    }
};

class TransformFeedbackBufferBufferCase : public TransformFeedbackCase
{
public:
    TransformFeedbackBufferBufferCase(Context &context, const char *name, const char *description)
        : TransformFeedbackCase(context, name, description)
    {
    }

    void testTransformFeedback(void)
    {
        const int feedbackPositionIndex = 0;
        const int feedbackOutputIndex   = 1;

        const int rangeBufferOffset = 4;
        const int rangeBufferSize   = 8;

        // bind buffers

        GLuint feedbackBuffers[2];
        glGenBuffers(2, feedbackBuffers);
        expectError(GL_NO_ERROR);

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffers[0]);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_DYNAMIC_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackPositionIndex, feedbackBuffers[0]);
        expectError(GL_NO_ERROR);

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffers[1]);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_DYNAMIC_READ);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackOutputIndex, feedbackBuffers[1], rangeBufferOffset,
                          rangeBufferSize);
        expectError(GL_NO_ERROR);

        // test TRANSFORM_FEEDBACK_BUFFER_START and TRANSFORM_FEEDBACK_BUFFER_SIZE

        const struct BufferRequirements
        {
            GLint index;
            GLenum pname;
            GLint64 value;
        } requirements[] = {{feedbackPositionIndex, GL_TRANSFORM_FEEDBACK_BUFFER_START, 0},
                            {feedbackPositionIndex, GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 0},
                            {feedbackOutputIndex, GL_TRANSFORM_FEEDBACK_BUFFER_START, rangeBufferOffset},
                            {feedbackOutputIndex, GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, rangeBufferSize}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(requirements); ++ndx)
        {
            StateQueryMemoryWriteGuard<GLint64> state;
            glGetInteger64i_v(requirements[ndx].pname, requirements[ndx].index, &state);

            if (state.verifyValidity(m_testCtx))
                checkIntEquals(m_testCtx, state, requirements[ndx].value);
        }

        // cleanup

        glDeleteBuffers(2, feedbackBuffers);
    }
};

class TransformFeedbackSwitchingBufferCase : public TransformFeedbackCase
{
public:
    TransformFeedbackSwitchingBufferCase(Context &context, const char *name, const char *description)
        : TransformFeedbackCase(context, name, description)
    {
    }

    void testTransformFeedback(void)
    {
        GLuint feedbackBuffers[3];
        glGenBuffers(3, feedbackBuffers);
        expectError(GL_NO_ERROR);

        for (int i = 0; i < 3; ++i)
        {
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[i]);
            expectError(GL_NO_ERROR);
            GLint value;
            glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &value);
            expectError(GL_NO_ERROR);
            checkIntEquals(m_testCtx, value, 0);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, feedbackBuffers[i]);
            expectError(GL_NO_ERROR);
            // glBindBufferBase should also set the generic binding point.
            glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &value);
            expectError(GL_NO_ERROR);
            checkIntEquals(m_testCtx, value, feedbackBuffers[i]);
        }

        for (int i = 0; i < 3; ++i)
        {
            // glBindTransformFeedback should change the indexed binding points, but
            // not the generic one.
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[i]);
            expectError(GL_NO_ERROR);
            GLint value;
            glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &value);
            expectError(GL_NO_ERROR);
            checkIntEquals(m_testCtx, value, feedbackBuffers[i]);
            glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &value);
            expectError(GL_NO_ERROR);
            // Should be unchanged.
            checkIntEquals(m_testCtx, value, feedbackBuffers[2]);
        }

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[0]);
        expectError(GL_NO_ERROR);
        glDeleteBuffers(3, feedbackBuffers);
        expectError(GL_NO_ERROR);

        // After deleting buffers the bound state should be changed but unbound
        // state should be unchanged.

        GLint value;
        glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &value);
        expectError(GL_NO_ERROR);
        checkIntEquals(m_testCtx, value, 0);
        glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &value);
        expectError(GL_NO_ERROR);
        checkIntEquals(m_testCtx, value, 0);

        for (int i = 1; i < 3; ++i)
        {
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[i]);
            expectError(GL_NO_ERROR);
            glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &value);
            expectError(GL_NO_ERROR);
            checkIntEquals(m_testCtx, value, feedbackBuffers[i]);
            glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &value);
            expectError(GL_NO_ERROR);
            checkIntEquals(m_testCtx, value, 0);
        }
    }
};

class UniformBufferCase : public ApiCase
{
public:
    UniformBufferCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_program(0)
    {
    }

    virtual void testUniformBuffers(void) = 0;

    void test(void)
    {
        static const char *testVertSource = "#version 300 es\n"
                                            "uniform highp vec4 input1;\n"
                                            "uniform highp vec4 input2;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = input1 + input2;\n"
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

        m_program = glCreateProgram();
        glAttachShader(m_program, shaderVert);
        glAttachShader(m_program, shaderFrag);
        glLinkProgram(m_program);
        glUseProgram(m_program);
        expectError(GL_NO_ERROR);

        testUniformBuffers();

        glUseProgram(0);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(m_program);
        expectError(GL_NO_ERROR);
    }

protected:
    GLuint m_program;
};

class UniformBufferBindingCase : public UniformBufferCase
{
public:
    UniformBufferBindingCase(Context &context, const char *name, const char *description)
        : UniformBufferCase(context, name, description)
    {
    }

    void testUniformBuffers(void)
    {
        const char *uniformNames[] = {"input1", "input2"};
        GLuint uniformIndices[2]   = {0};
        glGetUniformIndices(m_program, 2, uniformNames, uniformIndices);

        GLuint buffers[2];
        glGenBuffers(2, buffers);

        for (int ndx = 0; ndx < 2; ++ndx)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, buffers[ndx]);
            glBufferData(GL_UNIFORM_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, uniformIndices[ndx], buffers[ndx]);
            expectError(GL_NO_ERROR);
        }

        for (int ndx = 0; ndx < 2; ++ndx)
        {
            StateQueryMemoryWriteGuard<GLint> boundBuffer;
            glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, uniformIndices[ndx], &boundBuffer);

            if (boundBuffer.verifyValidity(m_testCtx))
                checkIntEquals(m_testCtx, boundBuffer, buffers[ndx]);
            expectError(GL_NO_ERROR);
        }

        glDeleteBuffers(2, buffers);
    }
};

class UniformBufferBufferCase : public UniformBufferCase
{
public:
    UniformBufferBufferCase(Context &context, const char *name, const char *description)
        : UniformBufferCase(context, name, description)
    {
    }

    void testUniformBuffers(void)
    {
        const char *uniformNames[] = {"input1", "input2"};
        GLuint uniformIndices[2]   = {0};
        glGetUniformIndices(m_program, 2, uniformNames, uniformIndices);

        const GLint alignment = GetAlignment();
        if (alignment == -1) // cannot continue without this
            return;

        m_testCtx.getLog() << tcu::TestLog::Message << "Alignment is " << alignment << tcu::TestLog::EndMessage;

        int rangeBufferOffset    = alignment;
        int rangeBufferSize      = alignment * 2;
        int rangeBufferTotalSize = rangeBufferOffset + rangeBufferSize +
                                   8; // + 8 has no special meaning, just to make it != with the size of the range

        GLuint buffers[2];
        glGenBuffers(2, buffers);

        glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);
        glBufferData(GL_UNIFORM_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, uniformIndices[0], buffers[0]);
        expectError(GL_NO_ERROR);

        glBindBuffer(GL_UNIFORM_BUFFER, buffers[1]);
        glBufferData(GL_UNIFORM_BUFFER, rangeBufferTotalSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferRange(GL_UNIFORM_BUFFER, uniformIndices[1], buffers[1], rangeBufferOffset, rangeBufferSize);
        expectError(GL_NO_ERROR);

        // test UNIFORM_BUFFER_START and UNIFORM_BUFFER_SIZE

        const struct BufferRequirements
        {
            GLuint index;
            GLenum pname;
            GLint64 value;
        } requirements[] = {{uniformIndices[0], GL_UNIFORM_BUFFER_START, 0},
                            {uniformIndices[0], GL_UNIFORM_BUFFER_SIZE, 0},
                            {uniformIndices[1], GL_UNIFORM_BUFFER_START, rangeBufferOffset},
                            {uniformIndices[1], GL_UNIFORM_BUFFER_SIZE, rangeBufferSize}};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(requirements); ++ndx)
        {
            StateQueryMemoryWriteGuard<GLint64> state;
            glGetInteger64i_v(requirements[ndx].pname, requirements[ndx].index, &state);

            if (state.verifyValidity(m_testCtx))
                checkIntEquals(m_testCtx, state, requirements[ndx].value);
            expectError(GL_NO_ERROR);
        }

        glDeleteBuffers(2, buffers);
    }

    int GetAlignment()
    {
        StateQueryMemoryWriteGuard<GLint> state;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &state);

        if (!state.verifyValidity(m_testCtx))
            return -1;

        if (state <= 256)
            return state;

        m_testCtx.getLog() << tcu::TestLog::Message
                           << "// ERROR: UNIFORM_BUFFER_OFFSET_ALIGNMENT has a maximum value of 256."
                           << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "invalid UNIFORM_BUFFER_OFFSET_ALIGNMENT value");

        return -1;
    }
};

const char *getVerifierSuffix(QueryType type)
{
    switch (type)
    {
    case QUERY_INDEXED_INTEGER:
        return "getintegeri_v";
    case QUERY_INDEXED_INTEGER64:
        return "getinteger64i_v";
    case QUERY_INDEXED_INTEGER_VEC4:
        return "getintegeri_v";
    case QUERY_INDEXED_INTEGER64_VEC4:
        return "getinteger64i_v";
    case QUERY_INDEXED_ISENABLED:
        return "isenabledi";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

void isExtensionSupported(Context &context, std::string extensionName)
{
    if (contextSupports(context.getRenderContext().getType(), glu::ApiType::core(4, 5)))
        return;

    if (extensionName == "GL_EXT_draw_buffers_indexed" || extensionName == "GL_KHR_blend_equation_advanced")
    {
        if (!contextSupports(context.getRenderContext().getType(), glu::ApiType::es(3, 2)) &&
            !context.getContextInfo().isExtensionSupported(extensionName.c_str()))
            TCU_THROW(NotSupportedError,
                      (std::string("Extension ") + extensionName + std::string(" not supported.")).c_str());
    }
    else if (!context.getContextInfo().isExtensionSupported(extensionName.c_str()))
        TCU_THROW(NotSupportedError,
                  (std::string("Extension ") + extensionName + std::string(" not supported.")).c_str());
}

class EnableBlendCase : public TestCase
{
public:
    EnableBlendCase(Context &context, const char *name, const char *desc, QueryType verifierType);

    void init(void);

private:
    IterateResult iterate(void);

    const QueryType m_verifierType;
};

EnableBlendCase::EnableBlendCase(Context &context, const char *name, const char *desc, QueryType verifierType)
    : TestCase(context, name, desc)
    , m_verifierType(verifierType)
{
}

void EnableBlendCase::init(void)
{
    isExtensionSupported(m_context, "GL_EXT_draw_buffers_indexed");
}

EnableBlendCase::IterateResult EnableBlendCase::iterate(void)
{
    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
    int32_t maxDrawBuffers = 0;

    gl.enableLogging(true);

    gl.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBoolean(result, gl, GL_BLEND, ndx, false, m_verifierType);
    }
    {
        const tcu::ScopedLogSection superSection(m_testCtx.getLog(), "AfterSettingCommon", "After setting common");

        gl.glEnable(GL_BLEND);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBoolean(result, gl, GL_BLEND, ndx, true, m_verifierType);
    }
    {
        const tcu::ScopedLogSection superSection(m_testCtx.getLog(), "AfterSettingIndexed", "After setting indexed");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
        {
            if (ndx % 2 == 0)
                gl.glEnablei(GL_BLEND, ndx);
            else
                gl.glDisablei(GL_BLEND, ndx);
        }

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBoolean(result, gl, GL_BLEND, ndx, (ndx % 2 == 0), m_verifierType);
    }
    {
        const tcu::ScopedLogSection superSection(m_testCtx.getLog(), "AfterResettingIndexedWithCommon",
                                                 "After resetting indexed with common");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
        {
            if (ndx % 2 == 0)
                gl.glEnablei(GL_BLEND, ndx);
            else
                gl.glDisablei(GL_BLEND, ndx);
        }

        gl.glEnable(GL_BLEND);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBoolean(result, gl, GL_BLEND, ndx, true, m_verifierType);
    }

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class ColorMaskCase : public TestCase
{
public:
    ColorMaskCase(Context &context, const char *name, const char *desc, QueryType verifierType);

    void init(void);

private:
    IterateResult iterate(void);

    const QueryType m_verifierType;
};

ColorMaskCase::ColorMaskCase(Context &context, const char *name, const char *desc, QueryType verifierType)
    : TestCase(context, name, desc)
    , m_verifierType(verifierType)
{
}

void ColorMaskCase::init(void)
{
    isExtensionSupported(m_context, "GL_EXT_draw_buffers_indexed");
}

ColorMaskCase::IterateResult ColorMaskCase::iterate(void)
{
    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
    int32_t maxDrawBuffers = 0;

    gl.enableLogging(true);

    gl.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBooleanVec4(result, gl, GL_COLOR_WRITEMASK, ndx, tcu::BVec4(true), m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingCommon", "After setting common");

        gl.glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBooleanVec4(result, gl, GL_COLOR_WRITEMASK, ndx, tcu::BVec4(false, true, true, false),
                                          m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingIndexed", "After setting indexed");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glColorMaski(ndx, (ndx % 2 == 0 ? GL_TRUE : GL_FALSE), (ndx % 2 == 1 ? GL_TRUE : GL_FALSE),
                            (ndx % 2 == 0 ? GL_TRUE : GL_FALSE), (ndx % 2 == 1 ? GL_TRUE : GL_FALSE));

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBooleanVec4(
                result, gl, GL_COLOR_WRITEMASK, ndx,
                (ndx % 2 == 0 ? tcu::BVec4(true, false, true, false) : tcu::BVec4(false, true, false, true)),
                m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterResettingIndexedWithCommon",
                                            "After resetting indexed with common");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glColorMaski(ndx, (ndx % 2 == 0 ? GL_TRUE : GL_FALSE), (ndx % 2 == 1 ? GL_TRUE : GL_FALSE),
                            (ndx % 2 == 0 ? GL_TRUE : GL_FALSE), (ndx % 2 == 1 ? GL_TRUE : GL_FALSE));

        gl.glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedBooleanVec4(result, gl, GL_COLOR_WRITEMASK, ndx, tcu::BVec4(false, true, true, false),
                                          m_verifierType);
    }

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class BlendFuncCase : public TestCase
{
public:
    BlendFuncCase(Context &context, const char *name, const char *desc, QueryType verifierType);

    void init(void);

private:
    IterateResult iterate(void);

    const QueryType m_verifierType;
};

BlendFuncCase::BlendFuncCase(Context &context, const char *name, const char *desc, QueryType verifierType)
    : TestCase(context, name, desc)
    , m_verifierType(verifierType)
{
}

void BlendFuncCase::init(void)
{
    isExtensionSupported(m_context, "GL_EXT_draw_buffers_indexed");
}

BlendFuncCase::IterateResult BlendFuncCase::iterate(void)
{
    const uint32_t blendFuncs[] = {GL_ZERO,
                                   GL_ONE,
                                   GL_SRC_COLOR,
                                   GL_ONE_MINUS_SRC_COLOR,
                                   GL_DST_COLOR,
                                   GL_ONE_MINUS_DST_COLOR,
                                   GL_SRC_ALPHA,
                                   GL_ONE_MINUS_SRC_ALPHA,
                                   GL_DST_ALPHA,
                                   GL_ONE_MINUS_DST_ALPHA,
                                   GL_CONSTANT_COLOR,
                                   GL_ONE_MINUS_CONSTANT_COLOR,
                                   GL_CONSTANT_ALPHA,
                                   GL_ONE_MINUS_CONSTANT_ALPHA,
                                   GL_SRC_ALPHA_SATURATE};

    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
    int32_t maxDrawBuffers = 0;

    gl.enableLogging(true);

    gl.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_RGB, ndx, GL_ONE, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_RGB, ndx, GL_ZERO, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_ALPHA, ndx, GL_ONE, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_ALPHA, ndx, GL_ZERO, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingCommon", "After setting common");

        gl.glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_RGB, ndx, GL_SRC_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_RGB, ndx, GL_DST_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_ALPHA, ndx, GL_SRC_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_ALPHA, ndx, GL_DST_ALPHA, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingCommonSeparate",
                                            "After setting common separate");

        gl.glBlendFuncSeparate(GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_ALPHA);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_RGB, ndx, GL_SRC_COLOR, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_RGB, ndx, GL_ONE_MINUS_SRC_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_ALPHA, ndx, GL_DST_COLOR, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_ALPHA, ndx, GL_ONE_MINUS_DST_ALPHA, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingIndexed", "After setting indexed");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendFunci(ndx, blendFuncs[ndx % DE_LENGTH_OF_ARRAY(blendFuncs)],
                            blendFuncs[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendFuncs)]);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_RGB, ndx,
                                      blendFuncs[ndx % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_RGB, ndx,
                                      blendFuncs[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_ALPHA, ndx,
                                      blendFuncs[ndx % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_ALPHA, ndx,
                                      blendFuncs[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingIndexedSeparate",
                                            "After setting indexed separate");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendFuncSeparatei(ndx, blendFuncs[(ndx + 3) % DE_LENGTH_OF_ARRAY(blendFuncs)],
                                    blendFuncs[(ndx + 2) % DE_LENGTH_OF_ARRAY(blendFuncs)],
                                    blendFuncs[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendFuncs)],
                                    blendFuncs[(ndx + 0) % DE_LENGTH_OF_ARRAY(blendFuncs)]);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_RGB, ndx,
                                      blendFuncs[(ndx + 3) % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_RGB, ndx,
                                      blendFuncs[(ndx + 2) % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_ALPHA, ndx,
                                      blendFuncs[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_ALPHA, ndx,
                                      blendFuncs[(ndx + 0) % DE_LENGTH_OF_ARRAY(blendFuncs)], m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterResettingIndexedWithCommon",
                                            "After resetting indexed with common");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendFunci(ndx, blendFuncs[ndx % DE_LENGTH_OF_ARRAY(blendFuncs)],
                            blendFuncs[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendFuncs)]);

        gl.glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_RGB, ndx, GL_SRC_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_RGB, ndx, GL_DST_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_ALPHA, ndx, GL_SRC_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_ALPHA, ndx, GL_DST_ALPHA, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterResettingIndexedWithCommonSeparate",
                                            "After resetting indexed with common separate");

        gl.glBlendFuncSeparate(GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_ALPHA);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendFuncSeparatei(ndx, blendFuncs[(ndx + 3) % DE_LENGTH_OF_ARRAY(blendFuncs)],
                                    blendFuncs[(ndx + 2) % DE_LENGTH_OF_ARRAY(blendFuncs)],
                                    blendFuncs[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendFuncs)],
                                    blendFuncs[(ndx + 0) % DE_LENGTH_OF_ARRAY(blendFuncs)]);

        gl.glBlendFuncSeparate(GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_ALPHA);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_RGB, ndx, GL_SRC_COLOR, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_RGB, ndx, GL_ONE_MINUS_SRC_ALPHA, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_SRC_ALPHA, ndx, GL_DST_COLOR, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_DST_ALPHA, ndx, GL_ONE_MINUS_DST_ALPHA, m_verifierType);
    }

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class BlendEquationCase : public TestCase
{
public:
    BlendEquationCase(Context &context, const char *name, const char *desc, QueryType verifierType);

    void init(void);

private:
    IterateResult iterate(void);

    const QueryType m_verifierType;
};

BlendEquationCase::BlendEquationCase(Context &context, const char *name, const char *desc, QueryType verifierType)
    : TestCase(context, name, desc)
    , m_verifierType(verifierType)
{
}

void BlendEquationCase::init(void)
{
    isExtensionSupported(m_context, "GL_EXT_draw_buffers_indexed");
}

BlendEquationCase::IterateResult BlendEquationCase::iterate(void)
{
    const uint32_t blendEquations[] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};

    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
    int32_t maxDrawBuffers = 0;

    gl.enableLogging(true);

    gl.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Initial", "Initial value");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_FUNC_ADD, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_FUNC_ADD, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingCommon", "After setting common");

        gl.glBlendEquation(GL_FUNC_SUBTRACT);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_FUNC_SUBTRACT, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_FUNC_SUBTRACT, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingCommonSeparate",
                                            "After setting common separate");

        gl.glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_FUNC_REVERSE_SUBTRACT, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_FUNC_SUBTRACT, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingIndexed", "After setting indexed");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendEquationi(ndx, blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)]);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx,
                                      blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx,
                                      blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)], m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingIndexedSeparate",
                                            "After setting indexed separate");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendEquationSeparatei(ndx, blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)],
                                        blendEquations[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendEquations)]);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx,
                                      blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)], m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx,
                                      blendEquations[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendEquations)], m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterResettingIndexedWithCommon",
                                            "After resetting indexed with common");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendEquationi(ndx, blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)]);

        gl.glBlendEquation(GL_FUNC_SUBTRACT);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_FUNC_SUBTRACT, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_FUNC_SUBTRACT, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterResettingIndexedWithCommonSeparate",
                                            "After resetting indexed with common separate");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendEquationSeparatei(ndx, blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)],
                                        blendEquations[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendEquations)]);

        gl.glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_FUNC_REVERSE_SUBTRACT, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_FUNC_SUBTRACT, m_verifierType);
    }

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class BlendEquationAdvancedCase : public TestCase
{
public:
    BlendEquationAdvancedCase(Context &context, const char *name, const char *desc, QueryType verifierType);

    void init(void);

private:
    IterateResult iterate(void);

    const QueryType m_verifierType;
};

BlendEquationAdvancedCase::BlendEquationAdvancedCase(Context &context, const char *name, const char *desc,
                                                     QueryType verifierType)
    : TestCase(context, name, desc)
    , m_verifierType(verifierType)
{
}

void BlendEquationAdvancedCase::init(void)
{
    isExtensionSupported(m_context, "GL_EXT_draw_buffers_indexed");
    isExtensionSupported(m_context, "GL_KHR_blend_equation_advanced");
}

BlendEquationAdvancedCase::IterateResult BlendEquationAdvancedCase::iterate(void)
{
    const uint32_t blendEquations[] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};

    const uint32_t blendEquationAdvanced[] = {GL_MULTIPLY,       GL_SCREEN,     GL_OVERLAY,       GL_DARKEN,
                                              GL_LIGHTEN,        GL_COLORDODGE, GL_COLORBURN,     GL_HARDLIGHT,
                                              GL_SOFTLIGHT,      GL_DIFFERENCE, GL_EXCLUSION,     GL_HSL_HUE,
                                              GL_HSL_SATURATION, GL_HSL_COLOR,  GL_HSL_LUMINOSITY};

    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
    tcu::ResultCollector result(m_testCtx.getLog(), " // ERROR: ");
    int32_t maxDrawBuffers = 0;

    gl.enableLogging(true);

    gl.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetIntegerv");

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingCommon", "After setting common");

        gl.glBlendEquation(GL_SCREEN);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_SCREEN, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_SCREEN, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterSettingIndexed", "After setting indexed");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendEquationi(ndx, blendEquationAdvanced[ndx % DE_LENGTH_OF_ARRAY(blendEquationAdvanced)]);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx,
                                      blendEquationAdvanced[ndx % DE_LENGTH_OF_ARRAY(blendEquationAdvanced)],
                                      m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx,
                                      blendEquationAdvanced[ndx % DE_LENGTH_OF_ARRAY(blendEquationAdvanced)],
                                      m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterResettingIndexedWithCommon",
                                            "After resetting indexed with common");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendEquationi(ndx, blendEquationAdvanced[ndx % DE_LENGTH_OF_ARRAY(blendEquationAdvanced)]);

        gl.glBlendEquation(GL_MULTIPLY);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_MULTIPLY, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_MULTIPLY, m_verifierType);
    }
    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "AfterResettingIndexedSeparateWithCommon",
                                            "After resetting indexed separate with common");

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            gl.glBlendEquationSeparatei(ndx, blendEquations[ndx % DE_LENGTH_OF_ARRAY(blendEquations)],
                                        blendEquations[(ndx + 1) % DE_LENGTH_OF_ARRAY(blendEquations)]);

        gl.glBlendEquation(GL_LIGHTEN);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_RGB, ndx, GL_LIGHTEN, m_verifierType);

        for (int ndx = 0; ndx < maxDrawBuffers; ++ndx)
            verifyStateIndexedInteger(result, gl, GL_BLEND_EQUATION_ALPHA, ndx, GL_LIGHTEN, m_verifierType);
    }

    result.setTestContextResult(m_testCtx);
    return STOP;
}

} // namespace

IndexedStateQueryTests::IndexedStateQueryTests(Context &context)
    : TestCaseGroup(context, "indexed", "Indexed Integer Values")
{
}

void IndexedStateQueryTests::init(void)
{
    // transform feedback
    addChild(new TransformFeedbackBufferBindingCase(m_context, "transform_feedback_buffer_binding",
                                                    "TRANSFORM_FEEDBACK_BUFFER_BINDING"));
    addChild(
        new TransformFeedbackBufferBufferCase(m_context, "transform_feedback_buffer_start_size",
                                              "TRANSFORM_FEEDBACK_BUFFER_START and TRANSFORM_FEEDBACK_BUFFER_SIZE"));
    addChild(new TransformFeedbackSwitchingBufferCase(
        m_context, "transform_feedback_switching_buffer",
        "TRANSFORM_FEEDBACK_BUFFER_BINDING while switching transform feedback objects"));

    // uniform buffers
    addChild(new UniformBufferBindingCase(m_context, "uniform_buffer_binding", "UNIFORM_BUFFER_BINDING"));
    addChild(new UniformBufferBufferCase(m_context, "uniform_buffer_start_size",
                                         "UNIFORM_BUFFER_START and UNIFORM_BUFFER_SIZE"));

    static const QueryType verifiers[]     = {QUERY_INDEXED_INTEGER, QUERY_INDEXED_INTEGER64};
    static const QueryType vec4Verifiers[] = {QUERY_INDEXED_INTEGER_VEC4, QUERY_INDEXED_INTEGER64_VEC4};

#define FOR_EACH_VERIFIER(X)                                                              \
    for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx) \
    {                                                                                     \
        const QueryType verifier   = verifiers[verifierNdx];                              \
        const char *verifierSuffix = getVerifierSuffix(verifier);                         \
        this->addChild(X);                                                                \
    }

#define FOR_EACH_VEC4_VERIFIER(X)                                                             \
    for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(vec4Verifiers); ++verifierNdx) \
    {                                                                                         \
        const QueryType verifier   = vec4Verifiers[verifierNdx];                              \
        const char *verifierSuffix = getVerifierSuffix(verifier);                             \
        this->addChild(X);                                                                    \
    }

    addChild(new EnableBlendCase(m_context, "blend_isenabledi", "BLEND", QUERY_INDEXED_ISENABLED));
    FOR_EACH_VEC4_VERIFIER(new ColorMaskCase(m_context, (std::string() + "color_mask_" + verifierSuffix).c_str(),
                                             "COLOR_WRITEMASK", verifier))
    FOR_EACH_VERIFIER(new BlendFuncCase(m_context, (std::string() + "blend_func_" + verifierSuffix).c_str(),
                                        "BLEND_SRC and BLEND_DST", verifier))
    FOR_EACH_VERIFIER(new BlendEquationCase(m_context, (std::string() + "blend_equation_" + verifierSuffix).c_str(),
                                            "BLEND_EQUATION_RGB and BLEND_DST", verifier))
    FOR_EACH_VERIFIER(
        new BlendEquationAdvancedCase(m_context, (std::string() + "blend_equation_advanced_" + verifierSuffix).c_str(),
                                      "BLEND_EQUATION_RGB and BLEND_DST", verifier))
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
