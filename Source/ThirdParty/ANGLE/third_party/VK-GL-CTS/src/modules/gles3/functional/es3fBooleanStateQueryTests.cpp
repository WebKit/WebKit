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
 * \brief Boolean State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fBooleanStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"
#include "glwEnums.hpp"

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace BooleanStateQueryVerifiers
{

// StateVerifier

class StateVerifier : protected glu::CallLogWrapper
{
public:
    StateVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix);
    virtual ~StateVerifier(); // make GCC happy

    const char *getTestNamePostfix(void) const;

    virtual void verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference) = 0;
    virtual void verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1,
                                bool reference2, bool reference3)                      = 0;

private:
    const char *const m_testNamePostfix;
};

StateVerifier::StateVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix)
    : glu::CallLogWrapper(gl, log)
    , m_testNamePostfix(testNamePostfix)
{
    enableLogging(true);
}

StateVerifier::~StateVerifier()
{
}

const char *StateVerifier::getTestNamePostfix(void) const
{
    return m_testNamePostfix;
}

// IsEnabledVerifier

class IsEnabledVerifier : public StateVerifier
{
public:
    IsEnabledVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference);
    void verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1, bool reference2,
                        bool reference3);
};

IsEnabledVerifier::IsEnabledVerifier(const glw::Functions &gl, tcu::TestLog &log) : StateVerifier(gl, log, "_isenabled")
{
}

void IsEnabledVerifier::verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference)
{
    using tcu::TestLog;

    const GLboolean state           = glIsEnabled(name);
    const GLboolean expectedGLState = reference ? (GLboolean)GL_TRUE : (GLboolean)GL_FALSE;

    if (state != expectedGLState)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << (reference ? "GL_TRUE" : "GL_FALSE")
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void IsEnabledVerifier::verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1,
                                       bool reference2, bool reference3)
{
    DE_UNREF(testCtx);
    DE_UNREF(name);
    DE_UNREF(reference0);
    DE_UNREF(reference1);
    DE_UNREF(reference2);
    DE_UNREF(reference3);
    DE_ASSERT(false && "not supported");
}

// GetBooleanVerifier

class GetBooleanVerifier : public StateVerifier
{
public:
    GetBooleanVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference);
    void verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1, bool reference2,
                        bool reference3);
};

GetBooleanVerifier::GetBooleanVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getboolean")
{
}

void GetBooleanVerifier::verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLboolean expectedGLState = reference ? GL_TRUE : GL_FALSE;

    if (state != expectedGLState)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << (reference ? "GL_TRUE" : "GL_FALSE")
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void GetBooleanVerifier::verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1,
                                        bool reference2, bool reference3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean[4]> boolVector4;
    glGetBooleanv(name, boolVector4);

    if (!boolVector4.verifyValidity(testCtx))
        return;

    const GLboolean referenceAsGLBoolean[] = {
        reference0 ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
        reference1 ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
        reference2 ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
        reference3 ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
    };

    if (boolVector4[0] != referenceAsGLBoolean[0] || boolVector4[1] != referenceAsGLBoolean[1] ||
        boolVector4[2] != referenceAsGLBoolean[2] || boolVector4[3] != referenceAsGLBoolean[3])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected "
                         << (referenceAsGLBoolean[0] ? "GL_TRUE" : "GL_FALSE") << " "
                         << (referenceAsGLBoolean[1] ? "GL_TRUE" : "GL_FALSE") << " "
                         << (referenceAsGLBoolean[2] ? "GL_TRUE" : "GL_FALSE") << " "
                         << (referenceAsGLBoolean[3] ? "GL_TRUE" : "GL_FALSE") << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

//GetIntegerVerifier

class GetIntegerVerifier : public StateVerifier
{
public:
    GetIntegerVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference);
    void verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1, bool reference2,
                        bool reference3);
};

GetIntegerVerifier::GetIntegerVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getinteger")
{
}

void GetIntegerVerifier::verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint expectedGLState = reference ? 1 : 0;

    if (state != expectedGLState)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << expectedGLState << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1,
                                        bool reference2, bool reference3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[4]> boolVector4;
    glGetIntegerv(name, boolVector4);

    if (!boolVector4.verifyValidity(testCtx))
        return;

    const GLint referenceAsGLint[] = {
        reference0 ? 1 : 0,
        reference1 ? 1 : 0,
        reference2 ? 1 : 0,
        reference3 ? 1 : 0,
    };

    if (boolVector4[0] != referenceAsGLint[0] || boolVector4[1] != referenceAsGLint[1] ||
        boolVector4[2] != referenceAsGLint[2] || boolVector4[3] != referenceAsGLint[3])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << referenceAsGLint[0] << " "
                         << referenceAsGLint[1] << " " << referenceAsGLint[2] << " " << referenceAsGLint[3] << " "
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

//GetInteger64Verifier

class GetInteger64Verifier : public StateVerifier
{
public:
    GetInteger64Verifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference);
    void verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1, bool reference2,
                        bool reference3);
};

GetInteger64Verifier::GetInteger64Verifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getinteger64")
{
}

void GetInteger64Verifier::verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint64 expectedGLState = reference ? 1 : 0;

    if (state != expectedGLState)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << expectedGLState << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1,
                                          bool reference2, bool reference3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64[4]> boolVector4;
    glGetInteger64v(name, boolVector4);

    if (!boolVector4.verifyValidity(testCtx))
        return;

    const GLint64 referenceAsGLint64[] = {
        reference0 ? 1 : 0,
        reference1 ? 1 : 0,
        reference2 ? 1 : 0,
        reference3 ? 1 : 0,
    };

    if (boolVector4[0] != referenceAsGLint64[0] || boolVector4[1] != referenceAsGLint64[1] ||
        boolVector4[2] != referenceAsGLint64[2] || boolVector4[3] != referenceAsGLint64[3])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << referenceAsGLint64[0] << " "
                         << referenceAsGLint64[1] << " " << referenceAsGLint64[2] << " " << referenceAsGLint64[3] << " "
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

//GetFloatVerifier

class GetFloatVerifier : public StateVerifier
{
public:
    GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference);
    void verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1, bool reference2,
                        bool reference3);
};

GetFloatVerifier::GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log) : StateVerifier(gl, log, "_getfloat")
{
}

void GetFloatVerifier::verifyBoolean(tcu::TestContext &testCtx, GLenum name, bool reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLfloat expectedGLState = reference ? 1.0f : 0.0f;

    if (state != expectedGLState)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << expectedGLState << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyBoolean4(tcu::TestContext &testCtx, GLenum name, bool reference0, bool reference1,
                                      bool reference2, bool reference3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[4]> boolVector4;
    glGetFloatv(name, boolVector4);

    if (!boolVector4.verifyValidity(testCtx))
        return;

    const GLfloat referenceAsGLfloat[] = {
        reference0 ? 1.0f : 0.0f,
        reference1 ? 1.0f : 0.0f,
        reference2 ? 1.0f : 0.0f,
        reference3 ? 1.0f : 0.0f,
    };

    if (boolVector4[0] != referenceAsGLfloat[0] || boolVector4[1] != referenceAsGLfloat[1] ||
        boolVector4[2] != referenceAsGLfloat[2] || boolVector4[3] != referenceAsGLfloat[3])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << referenceAsGLfloat[0] << " "
                         << referenceAsGLfloat[1] << " " << referenceAsGLfloat[2] << " " << referenceAsGLfloat[3] << " "
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

} // namespace BooleanStateQueryVerifiers

namespace
{

using namespace BooleanStateQueryVerifiers;

static const char *transformFeedbackTestVertSource = "#version 300 es\n"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "    gl_Position = vec4(0.0);\n"
                                                     "}\n\0";
static const char *transformFeedbackTestFragSource = "#version 300 es\n"
                                                     "layout(location = 0) out mediump vec4 fragColor;"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "    fragColor = vec4(0.0);\n"
                                                     "}\n\0";

class IsEnabledStateTestCase : public ApiCase
{
public:
    IsEnabledStateTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                           GLenum targetName, bool initial)
        : ApiCase(context, name, description)
        , m_targetName(targetName)
        , m_initial(initial)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        // check inital value
        m_verifier->verifyBoolean(m_testCtx, m_targetName, m_initial);
        expectError(GL_NO_ERROR);

        // check toggle

        glEnable(m_targetName);
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, m_targetName, true);
        expectError(GL_NO_ERROR);

        glDisable(m_targetName);
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, m_targetName, false);
        expectError(GL_NO_ERROR);
    }

private:
    GLenum m_targetName;
    bool m_initial;
    StateVerifier *m_verifier;
};

class DepthWriteMaskTestCase : public ApiCase
{
public:
    DepthWriteMaskTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyBoolean(m_testCtx, GL_DEPTH_WRITEMASK, true);
        expectError(GL_NO_ERROR);

        glDepthMask(GL_FALSE);
        m_verifier->verifyBoolean(m_testCtx, GL_DEPTH_WRITEMASK, false);
        expectError(GL_NO_ERROR);

        glDepthMask(GL_TRUE);
        m_verifier->verifyBoolean(m_testCtx, GL_DEPTH_WRITEMASK, true);
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class SampleCoverageInvertTestCase : public ApiCase
{
public:
    SampleCoverageInvertTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyBoolean(m_testCtx, GL_SAMPLE_COVERAGE_INVERT, false);
        expectError(GL_NO_ERROR);

        glSampleCoverage(1.0f, GL_TRUE);
        m_verifier->verifyBoolean(m_testCtx, GL_SAMPLE_COVERAGE_INVERT, true);
        expectError(GL_NO_ERROR);

        glSampleCoverage(1.0f, GL_FALSE);
        m_verifier->verifyBoolean(m_testCtx, GL_SAMPLE_COVERAGE_INVERT, false);
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class InitialBooleanTestCase : public ApiCase
{
public:
    InitialBooleanTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                           GLenum target, bool reference)
        : ApiCase(context, name, description)
        , m_target(target)
        , m_reference(reference)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyBoolean(m_testCtx, m_target, m_reference);
        expectError(GL_NO_ERROR);
    }

private:
    GLenum m_target;
    bool m_reference;
    StateVerifier *m_verifier;
};

class ColorMaskTestCase : public ApiCase
{
public:
    ColorMaskTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }
    void test(void)
    {
        m_verifier->verifyBoolean4(m_testCtx, GL_COLOR_WRITEMASK, true, true, true, true);
        expectError(GL_NO_ERROR);

        const struct ColorMask
        {
            GLboolean r, g, b, a;
        } testMasks[] = {
            {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE},    {GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE},
            {GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE},   {GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE},
            {GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE},   {GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE},
            {GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE},  {GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE},
            {GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE},   {GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE},
            {GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE},  {GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE},
            {GL_FALSE, GL_FALSE, GL_TRUE, GL_TRUE},  {GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE},
            {GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE}, {GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE},
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testMasks); ndx++)
        {
            glColorMask(testMasks[ndx].r, testMasks[ndx].g, testMasks[ndx].b, testMasks[ndx].a);
            m_verifier->verifyBoolean4(m_testCtx, GL_COLOR_WRITEMASK, testMasks[ndx].r == GL_TRUE,
                                       testMasks[ndx].g == GL_TRUE, testMasks[ndx].b == GL_TRUE,
                                       testMasks[ndx].a == GL_TRUE);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class TransformFeedbackTestCase : public ApiCase
{
public:
    TransformFeedbackTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_transformfeedback(0)
    {
    }

    void test(void)
    {
        glGenTransformFeedbacks(1, &m_transformfeedback);
        expectError(GL_NO_ERROR);

        GLuint shaderVert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shaderVert, 1, &transformFeedbackTestVertSource, nullptr);
        glCompileShader(shaderVert);
        expectError(GL_NO_ERROR);
        GLint compileStatus;
        glGetShaderiv(shaderVert, GL_COMPILE_STATUS, &compileStatus);
        checkBooleans(compileStatus, GL_TRUE);

        GLuint shaderFrag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(shaderFrag, 1, &transformFeedbackTestFragSource, nullptr);
        glCompileShader(shaderFrag);
        expectError(GL_NO_ERROR);
        glGetShaderiv(shaderFrag, GL_COMPILE_STATUS, &compileStatus);
        checkBooleans(compileStatus, GL_TRUE);

        GLuint shaderProg = glCreateProgram();
        glAttachShader(shaderProg, shaderVert);
        glAttachShader(shaderProg, shaderFrag);
        const char *transform_feedback_outputs = "gl_Position";
        glTransformFeedbackVaryings(shaderProg, 1, &transform_feedback_outputs, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(shaderProg);
        expectError(GL_NO_ERROR);
        GLint linkStatus;
        glGetProgramiv(shaderProg, GL_LINK_STATUS, &linkStatus);
        checkBooleans(linkStatus, GL_TRUE);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, m_transformfeedback);
        expectError(GL_NO_ERROR);

        GLuint feedbackBufferId;
        glGenBuffers(1, &feedbackBufferId);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBufferId);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_DYNAMIC_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, feedbackBufferId);
        expectError(GL_NO_ERROR);

        glUseProgram(shaderProg);

        testTransformFeedback();

        glUseProgram(0);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        glDeleteTransformFeedbacks(1, &m_transformfeedback);
        glDeleteBuffers(1, &feedbackBufferId);
        glDeleteShader(shaderVert);
        glDeleteShader(shaderFrag);
        glDeleteProgram(shaderProg);
        expectError(GL_NO_ERROR);
    }

    virtual void testTransformFeedback(void) = 0;

protected:
    StateVerifier *m_verifier;
    GLuint m_transformfeedback;
};

class TransformFeedbackBasicTestCase : public TransformFeedbackTestCase
{
public:
    TransformFeedbackBasicTestCase(Context &context, StateVerifier *verifier, const char *name)
        : TransformFeedbackTestCase(context, verifier, name,
                                    "Test TRANSFORM_FEEDBACK_ACTIVE and TRANSFORM_FEEDBACK_PAUSED")
    {
    }

    void testTransformFeedback(void)
    {
        glBeginTransformFeedback(GL_POINTS);
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_ACTIVE, true);
        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_PAUSED, false);
        expectError(GL_NO_ERROR);

        glPauseTransformFeedback();
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_ACTIVE, true);
        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_PAUSED, true);
        expectError(GL_NO_ERROR);

        glResumeTransformFeedback();
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_ACTIVE, true);
        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_PAUSED, false);
        expectError(GL_NO_ERROR);

        glEndTransformFeedback();
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_ACTIVE, false);
        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_PAUSED, false);
        expectError(GL_NO_ERROR);
    }
};

class TransformFeedbackImplicitResumeTestCase : public TransformFeedbackTestCase
{
public:
    TransformFeedbackImplicitResumeTestCase(Context &context, StateVerifier *verifier, const char *name)
        : TransformFeedbackTestCase(context, verifier, name,
                                    "EndTransformFeedback performs an implicit ResumeTransformFeedback.")
    {
    }

    void testTransformFeedback(void)
    {
        glBeginTransformFeedback(GL_POINTS);
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_ACTIVE, true);
        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_PAUSED, false);
        expectError(GL_NO_ERROR);

        glPauseTransformFeedback();
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_ACTIVE, true);
        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_PAUSED, true);
        expectError(GL_NO_ERROR);

        glEndTransformFeedback();
        expectError(GL_NO_ERROR);

        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_ACTIVE, false);
        m_verifier->verifyBoolean(m_testCtx, GL_TRANSFORM_FEEDBACK_PAUSED, false);
        expectError(GL_NO_ERROR);
    }
};

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)                                                 \
    do                                                                                           \
    {                                                                                            \
        for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++) \
        {                                                                                        \
            StateVerifier *verifier = (VERIFIERS)[_verifierNdx];                                 \
            CODE_BLOCK;                                                                          \
        }                                                                                        \
    } while (0)

} // namespace

BooleanStateQueryTests::BooleanStateQueryTests(Context &context)
    : TestCaseGroup(context, "boolean", "Boolean State Query tests")
    , m_verifierIsEnabled(nullptr)
    , m_verifierBoolean(nullptr)
    , m_verifierInteger(nullptr)
    , m_verifierInteger64(nullptr)
    , m_verifierFloat(nullptr)
{
}

BooleanStateQueryTests::~BooleanStateQueryTests(void)
{
    deinit();
}

void BooleanStateQueryTests::init(void)
{
    DE_ASSERT(m_verifierIsEnabled == nullptr);
    DE_ASSERT(m_verifierBoolean == nullptr);
    DE_ASSERT(m_verifierInteger == nullptr);
    DE_ASSERT(m_verifierInteger64 == nullptr);
    DE_ASSERT(m_verifierFloat == nullptr);

    m_verifierIsEnabled =
        new IsEnabledVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierBoolean =
        new GetBooleanVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierInteger =
        new GetIntegerVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierInteger64 =
        new GetInteger64Verifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierFloat =
        new GetFloatVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());

    StateVerifier *isEnabledVerifiers[] = {m_verifierIsEnabled, m_verifierBoolean, m_verifierInteger,
                                           m_verifierInteger64, m_verifierFloat};
    StateVerifier *normalVerifiers[]    = {m_verifierBoolean, m_verifierInteger, m_verifierInteger64, m_verifierFloat};

    struct StateBoolean
    {
        const char *name;
        const char *description;
        GLenum targetName;
        bool value;
    };
    const StateBoolean isEnableds[] = {
        {"primitive_restart_fixed_index", "PRIMITIVE_RESTART_FIXED_INDEX", GL_PRIMITIVE_RESTART_FIXED_INDEX, false},
        {"rasterizer_discard", "RASTERIZER_DISCARD", GL_RASTERIZER_DISCARD, false},
        {"cull_face", "CULL_FACE", GL_CULL_FACE, false},
        {"polygon_offset_fill", "POLYGON_OFFSET_FILL", GL_POLYGON_OFFSET_FILL, false},
        {"sample_alpha_to_coverage", "SAMPLE_ALPHA_TO_COVERAGE", GL_SAMPLE_ALPHA_TO_COVERAGE, false},
        {"sample_coverage", "SAMPLE_COVERAGE", GL_SAMPLE_COVERAGE, false},
        {"scissor_test", "SCISSOR_TEST", GL_SCISSOR_TEST, false},
        {"stencil_test", "STENCIL_TEST", GL_STENCIL_TEST, false},
        {"depth_test", "DEPTH_TEST", GL_DEPTH_TEST, false},
        {"blend", "BLEND", GL_BLEND, false},
        {"dither", "DITHER", GL_DITHER, true},
    };
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(isEnableds); testNdx++)
    {
        FOR_EACH_VERIFIER(
            isEnabledVerifiers,
            addChild(new IsEnabledStateTestCase(
                m_context, verifier, (std::string(isEnableds[testNdx].name) + verifier->getTestNamePostfix()).c_str(),
                isEnableds[testNdx].description, isEnableds[testNdx].targetName, isEnableds[testNdx].value)));
    }

    FOR_EACH_VERIFIER(normalVerifiers, addChild(new ColorMaskTestCase(
                                           m_context, verifier,
                                           (std::string("color_writemask") + verifier->getTestNamePostfix()).c_str(),
                                           "COLOR_WRITEMASK")));
    FOR_EACH_VERIFIER(normalVerifiers, addChild(new DepthWriteMaskTestCase(
                                           m_context, verifier,
                                           (std::string("depth_writemask") + verifier->getTestNamePostfix()).c_str(),
                                           "DEPTH_WRITEMASK")));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new SampleCoverageInvertTestCase(
            m_context, verifier, (std::string("sample_coverage_invert") + verifier->getTestNamePostfix()).c_str(),
            "SAMPLE_COVERAGE_INVERT")));
    FOR_EACH_VERIFIER(normalVerifiers, addChild(new InitialBooleanTestCase(
                                           m_context, verifier,
                                           (std::string("shader_compiler") + verifier->getTestNamePostfix()).c_str(),
                                           "SHADER_COMPILER", GL_SHADER_COMPILER, true)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new InitialBooleanTestCase(
                          m_context, verifier,
                          (std::string("transform_feedback_active_initial") + verifier->getTestNamePostfix()).c_str(),
                          "initial TRANSFORM_FEEDBACK_ACTIVE", GL_TRANSFORM_FEEDBACK_ACTIVE, false)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new InitialBooleanTestCase(
                          m_context, verifier,
                          (std::string("transform_feedback_paused_initial") + verifier->getTestNamePostfix()).c_str(),
                          "initial TRANSFORM_FEEDBACK_PAUSED", GL_TRANSFORM_FEEDBACK_PAUSED, false)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new TransformFeedbackBasicTestCase(
            m_context, verifier, (std::string("transform_feedback") + verifier->getTestNamePostfix()).c_str())));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new TransformFeedbackImplicitResumeTestCase(
            m_context, verifier,
            (std::string("transform_feedback_implicit_resume") + verifier->getTestNamePostfix()).c_str())));
}

void BooleanStateQueryTests::deinit(void)
{
    if (m_verifierIsEnabled)
    {
        delete m_verifierIsEnabled;
        m_verifierIsEnabled = nullptr;
    }
    if (m_verifierBoolean)
    {
        delete m_verifierBoolean;
        m_verifierBoolean = nullptr;
    }
    if (m_verifierInteger)
    {
        delete m_verifierInteger;
        m_verifierInteger = nullptr;
    }
    if (m_verifierInteger64)
    {
        delete m_verifierInteger64;
        m_verifierInteger64 = nullptr;
    }
    if (m_verifierFloat)
    {
        delete m_verifierFloat;
        m_verifierFloat = nullptr;
    }

    this->TestCaseGroup::deinit();
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
