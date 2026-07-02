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
 * \brief State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fIntegerStateQueryTests.hpp"
#include "es3fApiCase.hpp"

#include "glsStateQueryUtil.hpp"

#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "gluStrUtil.hpp"

#include "tcuRenderTarget.hpp"

#include "deRandom.hpp"

#include "glwEnums.hpp"

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

#ifndef GL_SLUMINANCE_NV
#define GL_SLUMINANCE_NV 0x8C46
#endif
#ifndef GL_SLUMINANCE_ALPHA_NV
#define GL_SLUMINANCE_ALPHA_NV 0x8C44
#endif
#ifndef GL_BGR_NV
#define GL_BGR_NV 0x80E0
#endif

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace IntegerStateQueryVerifiers
{

// StateVerifier

class StateVerifier : protected glu::CallLogWrapper
{
public:
    StateVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix);
    virtual ~StateVerifier(); // make GCC happy

    const char *getTestNamePostfix(void) const;

    virtual void verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference)                        = 0;
    virtual void verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1,
                                GLint reference2, GLint reference3)                                            = 0;
    virtual void verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0,
                                    GLint reference1, bool enableRef1, GLint reference2, bool enableRef2,
                                    GLint reference3, bool enableRef3)                                         = 0;
    virtual void verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)          = 0;
    virtual void verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference) = 0;
    virtual void verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)             = 0;
    virtual void verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0,
                                              GLint reference1)                                                = 0;
    virtual void verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[],
                                    size_t referencesLength)                                                   = 0;
    virtual void verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits)             = 0;

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

// GetBooleanVerifier

class GetBooleanVerifier : public StateVerifier
{
public:
    GetBooleanVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1, GLint reference2,
                        GLint reference3);
    void verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0, GLint reference1,
                            bool enableRef1, GLint reference2, bool enableRef2, GLint reference3, bool enableRef3);
    void verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference);
    void verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1);
    void verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[], size_t referencesLength);
    void verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits);
};

GetBooleanVerifier::GetBooleanVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getboolean")
{
}

void GetBooleanVerifier::verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLboolean expectedGLState = reference ? GL_TRUE : GL_FALSE;

    if (state != expectedGLState)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected "
                         << (expectedGLState == GL_TRUE ? "GL_TRUE" : "GL_FALSE") << "; got "
                         << (state == GL_TRUE ? "GL_TRUE" : (state == GL_FALSE ? "GL_FALSE" : "non-boolean"))
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void GetBooleanVerifier::verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1,
                                        GLint reference2, GLint reference3)
{
    verifyInteger4Mask(testCtx, name, reference0, true, reference1, true, reference2, true, reference3, true);
}

void GetBooleanVerifier::verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0,
                                            GLint reference1, bool enableRef1, GLint reference2, bool enableRef2,
                                            GLint reference3, bool enableRef3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean[4]> boolVector4;
    glGetBooleanv(name, boolVector4);

    if (!boolVector4.verifyValidity(testCtx))
        return;

    const GLboolean referenceAsGLBoolean[] = {
        reference0 ? (GLboolean)GL_TRUE : (GLboolean)GL_FALSE,
        reference1 ? (GLboolean)GL_TRUE : (GLboolean)GL_FALSE,
        reference2 ? (GLboolean)GL_TRUE : (GLboolean)GL_FALSE,
        reference3 ? (GLboolean)GL_TRUE : (GLboolean)GL_FALSE,
    };

    if ((enableRef0 && (boolVector4[0] != referenceAsGLBoolean[0])) ||
        (enableRef1 && (boolVector4[1] != referenceAsGLBoolean[1])) ||
        (enableRef2 && (boolVector4[2] != referenceAsGLBoolean[2])) ||
        (enableRef3 && (boolVector4[3] != referenceAsGLBoolean[3])))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected "
                         << (enableRef0 ? (referenceAsGLBoolean[0] ? "GL_TRUE" : "GL_FALSE") : " - ") << ", "
                         << (enableRef1 ? (referenceAsGLBoolean[1] ? "GL_TRUE" : "GL_FALSE") : " - ") << ", "
                         << (enableRef2 ? (referenceAsGLBoolean[2] ? "GL_TRUE" : "GL_FALSE") : " - ") << ", "
                         << (enableRef3 ? (referenceAsGLBoolean[3] ? "GL_TRUE" : "GL_FALSE") : " - ")
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void GetBooleanVerifier::verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state == GL_TRUE) // state is non-zero, could be greater than reference (correct)
        return;

    if (state == GL_FALSE) // state is zero
    {
        if (reference > 0) // and reference is greater than zero?
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE" << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
        }
    }
    else
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE or GL_FALSE" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void GetBooleanVerifier::verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state == GL_TRUE) // state is non-zero, could be greater than reference (correct)
        return;

    if (state == GL_FALSE) // state is zero
    {
        if (reference > 0) // and reference is greater than zero?
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE" << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
        }
    }
    else
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE or GL_FALSE" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void GetBooleanVerifier::verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state == GL_TRUE) // state is non-zero, could be less than reference (correct)
        return;

    if (state == GL_FALSE) // state is zero
    {
        if (reference < 0) // and reference is less than zero?
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE" << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
        }
    }
    else
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE or GL_FALSE" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void GetBooleanVerifier::verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0,
                                                      GLint reference1)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean[2]> boolVector;
    glGetBooleanv(name, boolVector);

    if (!boolVector.verifyValidity(testCtx))
        return;

    const GLboolean referenceAsGLBoolean[2] = {reference0 ? (GLboolean)GL_TRUE : (GLboolean)GL_FALSE,
                                               reference1 ? (GLboolean)GL_TRUE : (GLboolean)GL_FALSE};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(referenceAsGLBoolean); ++ndx)
    {
        if (boolVector[ndx] == GL_TRUE) // state is non-zero, could be greater than any integer
        {
            continue;
        }
        else if (boolVector[ndx] == GL_FALSE) // state is zero
        {
            if (referenceAsGLBoolean[ndx] > 0) // and reference is greater than zero?
            {
                testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE" << TestLog::EndMessage;
                if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
            }
        }
        else
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE or GL_FALSE" << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
        }
    }
}

void GetBooleanVerifier::verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[],
                                            size_t referencesLength)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    for (size_t ndx = 0; ndx < referencesLength; ++ndx)
    {
        const GLboolean expectedGLState = references[ndx] ? GL_TRUE : GL_FALSE;

        if (state == expectedGLState)
            return;
    }

    testCtx.getLog() << TestLog::Message << "// ERROR: got " << (state == GL_TRUE ? "GL_TRUE" : "GL_FALSE")
                     << TestLog::EndMessage;
    if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
}

void GetBooleanVerifier::verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits)
{
    // if stencilBits == 0, the mask is allowed to be either GL_TRUE or GL_FALSE
    // otherwise it must be GL_TRUE
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (stencilBits > 0 && state != GL_TRUE)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected GL_TRUE" << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

//GetIntegerVerifier

class GetIntegerVerifier : public StateVerifier
{
public:
    GetIntegerVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1, GLint reference2,
                        GLint reference3);
    void verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0, GLint reference1,
                            bool enableRef1, GLint reference2, bool enableRef2, GLint reference3, bool enableRef3);
    void verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference);
    void verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1);
    void verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[], size_t referencesLength);
    void verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits);
};

GetIntegerVerifier::GetIntegerVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getinteger")
{
}

void GetIntegerVerifier::verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1,
                                        GLint reference2, GLint reference3)
{
    verifyInteger4Mask(testCtx, name, reference0, true, reference1, true, reference2, true, reference3, true);
}

void GetIntegerVerifier::verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0,
                                            GLint reference1, bool enableRef1, GLint reference2, bool enableRef2,
                                            GLint reference3, bool enableRef3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[4]> intVector4;
    glGetIntegerv(name, intVector4);

    if (!intVector4.verifyValidity(testCtx))
        return;

    if ((enableRef0 && (intVector4[0] != reference0)) || (enableRef1 && (intVector4[1] != reference1)) ||
        (enableRef2 && (intVector4[2] != reference2)) || (enableRef3 && (intVector4[3] != reference3)))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << (enableRef0 ? "" : "(") << reference0
                         << (enableRef0 ? "" : ")") << ", " << (enableRef1 ? "" : "(") << reference1
                         << (enableRef1 ? "" : ")") << ", " << (enableRef2 ? "" : "(") << reference2
                         << (enableRef2 ? "" : ")") << ", " << (enableRef3 ? "" : "(") << reference3
                         << (enableRef3 ? "" : ")") << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << reference << "; got "
                         << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (GLuint(state) < reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << reference << "; got "
                         << GLuint(state) << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state > reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected less or equal to " << reference << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0,
                                                      GLint reference1)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[2]> intVector2;
    glGetIntegerv(name, intVector2);

    if (!intVector2.verifyValidity(testCtx))
        return;

    if (intVector2[0] < reference0 || intVector2[1] < reference1)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << reference0 << ", "
                         << reference1 << "; got " << intVector2[0] << ", " << intVector2[0] << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[],
                                            size_t referencesLength)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    for (size_t ndx = 0; ndx < referencesLength; ++ndx)
    {
        const GLint expectedGLState = references[ndx];

        if (state == expectedGLState)
            return;
    }

    testCtx.getLog() << TestLog::Message << "// ERROR: got " << state << TestLog::EndMessage;
    if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
}

void GetIntegerVerifier::verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint reference = (1 << stencilBits) - 1;

    if ((state & reference) != reference) // the least significant stencilBits bits should be on
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected minimum mask of " << reference << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid mask value");
    }
}

//GetInteger64Verifier

class GetInteger64Verifier : public StateVerifier
{
public:
    GetInteger64Verifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1, GLint reference2,
                        GLint reference3);
    void verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0, GLint reference1,
                            bool enableRef1, GLint reference2, bool enableRef2, GLint reference3, bool enableRef3);
    void verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference);
    void verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1);
    void verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[], size_t referencesLength);
    void verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits);
};

GetInteger64Verifier::GetInteger64Verifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getinteger64")
{
}

void GetInteger64Verifier::verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != GLint64(reference))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1,
                                          GLint reference2, GLint reference3)
{
    verifyInteger4Mask(testCtx, name, reference0, true, reference1, true, reference2, true, reference3, true);
}

void GetInteger64Verifier::verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0,
                                              GLint reference1, bool enableRef1, GLint reference2, bool enableRef2,
                                              GLint reference3, bool enableRef3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64[4]> intVector4;
    glGetInteger64v(name, intVector4);

    if (!intVector4.verifyValidity(testCtx))
        return;

    if ((enableRef0 && (intVector4[0] != GLint64(reference0))) ||
        (enableRef1 && (intVector4[1] != GLint64(reference1))) ||
        (enableRef2 && (intVector4[2] != GLint64(reference2))) ||
        (enableRef3 && (intVector4[3] != GLint64(reference3))))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << (enableRef0 ? "" : "(") << reference0
                         << (enableRef0 ? "" : ")") << ", " << (enableRef1 ? "" : "(") << reference1
                         << (enableRef1 ? "" : ")") << ", " << (enableRef2 ? "" : "(") << reference2
                         << (enableRef2 ? "" : ")") << ", " << (enableRef3 ? "" : "(") << reference3
                         << (enableRef3 ? "" : ")") << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < GLint64(reference))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << GLint64(reference)
                         << "; got " << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (GLuint(state) < GLint64(reference))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << GLint64(reference)
                         << "; got " << GLuint(state) << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state > GLint64(reference))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected less or equal to " << GLint64(reference) << "; got "
                         << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0,
                                                        GLint reference1)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64[2]> intVector2;
    glGetInteger64v(name, intVector2);

    if (!intVector2.verifyValidity(testCtx))
        return;

    if (intVector2[0] < GLint64(reference0) || intVector2[1] < GLint64(reference1))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << GLint64(reference0) << ", "
                         << GLint64(reference1) << "; got " << intVector2[0] << ", " << intVector2[1]
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[],
                                              size_t referencesLength)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    for (size_t ndx = 0; ndx < referencesLength; ++ndx)
    {
        const GLint64 expectedGLState = GLint64(references[ndx]);

        if (state == expectedGLState)
            return;
    }

    testCtx.getLog() << TestLog::Message << "// ERROR: got " << state << TestLog::EndMessage;
    if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
}

void GetInteger64Verifier::verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint64 reference = (1ULL << stencilBits) - 1;

    if ((state & reference) != reference) // the least significant stencilBits bits should be on
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected mimimum mask of " << reference << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid mask value");
    }
}

//GetFloatVerifier

class GetFloatVerifier : public StateVerifier
{
public:
    GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1, GLint reference2,
                        GLint reference3);
    void verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0, GLint reference1,
                            bool enableRef1, GLint reference2, bool enableRef2, GLint reference3, bool enableRef3);
    void verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference);
    void verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference);
    void verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1);
    void verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[], size_t referencesLength);
    void verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits);
};

GetFloatVerifier::GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log) : StateVerifier(gl, log, "_getfloat")
{
}

void GetFloatVerifier::verifyInteger(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    const GLfloat referenceAsFloat = GLfloat(reference);
    DE_ASSERT(
        reference ==
        GLint(
            referenceAsFloat)); // reference integer must have 1:1 mapping to float for this to work. Reference value is always such value in these tests

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != referenceAsFloat)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << referenceAsFloat << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyInteger4(tcu::TestContext &testCtx, GLenum name, GLint reference0, GLint reference1,
                                      GLint reference2, GLint reference3)
{
    verifyInteger4Mask(testCtx, name, reference0, true, reference1, true, reference2, true, reference3, true);
}

void GetFloatVerifier::verifyInteger4Mask(tcu::TestContext &testCtx, GLenum name, GLint reference0, bool enableRef0,
                                          GLint reference1, bool enableRef1, GLint reference2, bool enableRef2,
                                          GLint reference3, bool enableRef3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[4]> floatVector4;
    glGetFloatv(name, floatVector4);

    if (!floatVector4.verifyValidity(testCtx))
        return;

    if ((enableRef0 && (floatVector4[0] != GLfloat(reference0))) ||
        (enableRef1 && (floatVector4[1] != GLfloat(reference1))) ||
        (enableRef2 && (floatVector4[2] != GLfloat(reference2))) ||
        (enableRef3 && (floatVector4[3] != GLfloat(reference3))))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << (enableRef0 ? "" : "(") << GLfloat(reference0)
                         << (enableRef0 ? "" : ")") << ", " << (enableRef1 ? "" : "(") << GLfloat(reference1)
                         << (enableRef1 ? "" : ")") << ", " << (enableRef2 ? "" : "(") << GLfloat(reference2)
                         << (enableRef2 ? "" : ")") << ", " << (enableRef3 ? "" : "(") << GLfloat(reference3)
                         << (enableRef3 ? "" : ")") << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < GLfloat(reference))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << GLfloat(reference)
                         << "; got " << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyUnsignedIntegerGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < GLfloat(reference))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << GLfloat(reference)
                         << "; got " << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyIntegerLessOrEqual(tcu::TestContext &testCtx, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state > GLfloat(reference))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected less or equal to " << GLfloat(reference) << "; got "
                         << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyIntegerGreaterOrEqual2(tcu::TestContext &testCtx, GLenum name, GLint reference0,
                                                    GLint reference1)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[2]> floatVector2;
    glGetFloatv(name, floatVector2);

    if (!floatVector2.verifyValidity(testCtx))
        return;

    if (floatVector2[0] < GLfloat(reference0) || floatVector2[1] < GLfloat(reference1))
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << GLfloat(reference0) << ", "
                         << GLfloat(reference1) << "; got " << floatVector2[0] << ", " << floatVector2[1]
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyIntegerAnyOf(tcu::TestContext &testCtx, GLenum name, const GLint references[],
                                          size_t referencesLength)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    for (size_t ndx = 0; ndx < referencesLength; ++ndx)
    {
        const GLfloat expectedGLState = GLfloat(references[ndx]);
        DE_ASSERT(
            references[ndx] ==
            GLint(
                expectedGLState)); // reference integer must have 1:1 mapping to float for this to work. Reference value is always such value in these tests

        if (state == expectedGLState)
            return;
    }

    testCtx.getLog() << TestLog::Message << "// ERROR: got " << state << TestLog::EndMessage;
    if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
}

void GetFloatVerifier::verifyStencilMaskInitial(tcu::TestContext &testCtx, GLenum name, int stencilBits)
{
    // checking the mask bits with float doesn't make much sense because of conversion errors
    // just verify that the value is greater or equal to the minimum value
    const GLint reference = (1 << stencilBits) - 1;
    verifyIntegerGreaterOrEqual(testCtx, name, reference);
}

} // namespace IntegerStateQueryVerifiers

namespace
{

using namespace IntegerStateQueryVerifiers;
using namespace deqp::gls::StateQueryUtil;

class ConstantMinimumValueTestCase : public ApiCase
{
public:
    ConstantMinimumValueTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                                 GLenum targetName, GLint minValue)
        : ApiCase(context, name, description)
        , m_targetName(targetName)
        , m_minValue(minValue)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyUnsignedIntegerGreaterOrEqual(m_testCtx, m_targetName, m_minValue);
        expectError(GL_NO_ERROR);
    }

private:
    GLenum m_targetName;
    GLint m_minValue;
    StateVerifier *m_verifier;
};

class ConstantMaximumValueTestCase : public ApiCase
{
public:
    ConstantMaximumValueTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                                 GLenum targetName, GLint minValue)
        : ApiCase(context, name, description)
        , m_targetName(targetName)
        , m_minValue(minValue)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyIntegerLessOrEqual(m_testCtx, m_targetName, m_minValue);
        expectError(GL_NO_ERROR);
    }

private:
    GLenum m_targetName;
    GLint m_minValue;
    StateVerifier *m_verifier;
};

class SampleBuffersTestCase : public ApiCase
{
public:
    SampleBuffersTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        const int expectedSampleBuffers = (m_context.getRenderTarget().getNumSamples() > 1) ? 1 : 0;

        m_log << tcu::TestLog::Message << "Sample count is " << (m_context.getRenderTarget().getNumSamples())
              << ", expecting GL_SAMPLE_BUFFERS to be " << expectedSampleBuffers << tcu::TestLog::EndMessage;

        m_verifier->verifyInteger(m_testCtx, GL_SAMPLE_BUFFERS, expectedSampleBuffers);
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class SamplesTestCase : public ApiCase
{
public:
    SamplesTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        // MSAA?
        if (m_context.getRenderTarget().getNumSamples() > 1)
        {
            m_log << tcu::TestLog::Message << "Sample count is " << (m_context.getRenderTarget().getNumSamples())
                  << tcu::TestLog::EndMessage;

            m_verifier->verifyInteger(m_testCtx, GL_SAMPLES, m_context.getRenderTarget().getNumSamples());
            expectError(GL_NO_ERROR);
        }
        else
        {
            const glw::GLint validSamples[] = {0, 1};

            m_log << tcu::TestLog::Message << "Expecting GL_SAMPLES to be 0 or 1" << tcu::TestLog::EndMessage;

            m_verifier->verifyIntegerAnyOf(m_testCtx, GL_SAMPLES, validSamples, DE_LENGTH_OF_ARRAY(validSamples));
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class HintTestCase : public ApiCase
{
public:
    HintTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                 GLenum targetName)
        : ApiCase(context, name, description)
        , m_targetName(targetName)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_targetName, GL_DONT_CARE);
        expectError(GL_NO_ERROR);

        glHint(m_targetName, GL_NICEST);
        m_verifier->verifyInteger(m_testCtx, m_targetName, GL_NICEST);
        expectError(GL_NO_ERROR);

        glHint(m_targetName, GL_FASTEST);
        m_verifier->verifyInteger(m_testCtx, m_targetName, GL_FASTEST);
        expectError(GL_NO_ERROR);

        glHint(m_targetName, GL_DONT_CARE);
        m_verifier->verifyInteger(m_testCtx, m_targetName, GL_DONT_CARE);
        expectError(GL_NO_ERROR);
    }

private:
    GLenum m_targetName;
    StateVerifier *m_verifier;
};

class DepthFuncTestCase : public ApiCase
{
public:
    DepthFuncTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, GL_DEPTH_FUNC, GL_LESS);
        expectError(GL_NO_ERROR);

        const GLenum depthFunctions[] = {GL_NEVER, GL_ALWAYS,  GL_LESS,   GL_LEQUAL,
                                         GL_EQUAL, GL_GREATER, GL_GEQUAL, GL_NOTEQUAL};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthFunctions); ndx++)
        {
            glDepthFunc(depthFunctions[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, GL_DEPTH_FUNC, depthFunctions[ndx]);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class CullFaceTestCase : public ApiCase
{
public:
    CullFaceTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, GL_CULL_FACE_MODE, GL_BACK);
        expectError(GL_NO_ERROR);

        const GLenum cullFaces[] = {GL_FRONT, GL_BACK, GL_FRONT_AND_BACK};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(cullFaces); ndx++)
        {
            glCullFace(cullFaces[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, GL_CULL_FACE_MODE, cullFaces[ndx]);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class FrontFaceTestCase : public ApiCase
{
public:
    FrontFaceTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, GL_FRONT_FACE, GL_CCW);
        expectError(GL_NO_ERROR);

        const GLenum frontFaces[] = {GL_CW, GL_CCW};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(frontFaces); ndx++)
        {
            glFrontFace(frontFaces[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, GL_FRONT_FACE, frontFaces[ndx]);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class ViewPortTestCase : public ApiCase
{
public:
    ViewPortTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        GLint maxViewportDimensions[2] = {0};
        GLfloat viewportBoundsRange[2] = {0.0f};
        GLboolean hasViewportArray     = false;
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxViewportDimensions);
        hasViewportArray = m_context.getContextInfo().isExtensionSupported("GL_OES_viewport_array") ||
                           m_context.getContextInfo().isExtensionSupported("GL_NV_viewport_array") ||
                           m_context.getContextInfo().isExtensionSupported("GL_ARB_viewport_array");
        if (hasViewportArray)
        {
            glGetFloatv(GL_VIEWPORT_BOUNDS_RANGE, viewportBoundsRange);
        }

        // verify initial value of first two values
        m_verifier->verifyInteger4(m_testCtx, GL_VIEWPORT, 0, 0, m_context.getRenderTarget().getWidth(),
                                   m_context.getRenderTarget().getHeight());
        expectError(GL_NO_ERROR);

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            GLint x        = rnd.getInt(-64000, 64000);
            GLint y        = rnd.getInt(-64000, 64000);
            GLsizei width  = rnd.getInt(0, maxViewportDimensions[0]);
            GLsizei height = rnd.getInt(0, maxViewportDimensions[1]);

            glViewport(x, y, width, height);

            if (hasViewportArray)
            {
                m_verifier->verifyInteger4(m_testCtx, GL_VIEWPORT,
                                           de::clamp(x, deFloorFloatToInt32(viewportBoundsRange[0]),
                                                     deFloorFloatToInt32(viewportBoundsRange[1])),
                                           de::clamp(y, deFloorFloatToInt32(viewportBoundsRange[0]),
                                                     deFloorFloatToInt32(viewportBoundsRange[1])),
                                           width, height);
            }
            else
            {
                m_verifier->verifyInteger4(m_testCtx, GL_VIEWPORT, x, y, width, height);
            }

            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class ScissorBoxTestCase : public ApiCase
{
public:
    ScissorBoxTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        // verify initial value of first two values
        m_verifier->verifyInteger4Mask(m_testCtx, GL_SCISSOR_BOX, 0, true, 0, true, 0, false, 0, false);
        expectError(GL_NO_ERROR);

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            GLint left     = rnd.getInt(-64000, 64000);
            GLint bottom   = rnd.getInt(-64000, 64000);
            GLsizei width  = rnd.getInt(0, 64000);
            GLsizei height = rnd.getInt(0, 64000);

            glScissor(left, bottom, width, height);
            m_verifier->verifyInteger4(m_testCtx, GL_SCISSOR_BOX, left, bottom, width, height);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class MaxViewportDimsTestCase : public ApiCase
{
public:
    MaxViewportDimsTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyIntegerGreaterOrEqual2(m_testCtx, GL_MAX_VIEWPORT_DIMS,
                                                 m_context.getRenderTarget().getWidth(),
                                                 m_context.getRenderTarget().getHeight());
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class StencilRefTestCase : public ApiCase
{
public:
    StencilRefTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                       GLenum testTargetName)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_testTargetName, 0);
        expectError(GL_NO_ERROR);

        const int stencilBits = m_context.getRenderTarget().getStencilBits();

        for (int stencilBit = 0; stencilBit < stencilBits; ++stencilBit)
        {
            const int ref = 1 << stencilBit;

            glStencilFunc(GL_ALWAYS, ref, 0); // mask should not affect the REF
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, ref);
            expectError(GL_NO_ERROR);

            glStencilFunc(GL_ALWAYS, ref, ref);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, ref);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
};

class StencilRefSeparateTestCase : public ApiCase
{
public:
    StencilRefSeparateTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                               GLenum testTargetName, GLenum stencilFuncTargetFace)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
        , m_stencilFuncTargetFace(stencilFuncTargetFace)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_testTargetName, 0);
        expectError(GL_NO_ERROR);

        const int stencilBits = m_context.getRenderTarget().getStencilBits();

        for (int stencilBit = 0; stencilBit < stencilBits; ++stencilBit)
        {
            const int ref = 1 << stencilBit;

            glStencilFuncSeparate(m_stencilFuncTargetFace, GL_ALWAYS, ref, 0);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, ref);
            expectError(GL_NO_ERROR);

            glStencilFuncSeparate(m_stencilFuncTargetFace, GL_ALWAYS, ref, ref);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, ref);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
    GLenum m_stencilFuncTargetFace;
};

class StencilOpTestCase : public ApiCase
{
public:
    StencilOpTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                      GLenum stencilOpName)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_stencilOpName(stencilOpName)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_stencilOpName, GL_KEEP);
        expectError(GL_NO_ERROR);

        const GLenum stencilOpValues[] = {GL_KEEP, GL_ZERO,   GL_REPLACE,   GL_INCR,
                                          GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stencilOpValues); ++ndx)
        {
            SetStencilOp(stencilOpValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_stencilOpName, stencilOpValues[ndx]);
            expectError(GL_NO_ERROR);
        }
    }

protected:
    virtual void SetStencilOp(GLenum stencilOpValue)
    {
        switch (m_stencilOpName)
        {
        case GL_STENCIL_FAIL:
        case GL_STENCIL_BACK_FAIL:
            glStencilOp(stencilOpValue, GL_KEEP, GL_KEEP);
            break;

        case GL_STENCIL_PASS_DEPTH_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
            glStencilOp(GL_KEEP, stencilOpValue, GL_KEEP);
            break;

        case GL_STENCIL_PASS_DEPTH_PASS:
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:
            glStencilOp(GL_KEEP, GL_KEEP, stencilOpValue);
            break;

        default:
            DE_ASSERT(false && "should not happen");
            break;
        }
    }

    StateVerifier *m_verifier;
    GLenum m_stencilOpName;
};

class StencilOpSeparateTestCase : public StencilOpTestCase
{
public:
    StencilOpSeparateTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                              GLenum stencilOpName, GLenum stencilOpFace)
        : StencilOpTestCase(context, verifier, name, description, stencilOpName)
        , m_stencilOpFace(stencilOpFace)
    {
    }

private:
    void SetStencilOp(GLenum stencilOpValue)
    {
        switch (m_stencilOpName)
        {
        case GL_STENCIL_FAIL:
        case GL_STENCIL_BACK_FAIL:
            glStencilOpSeparate(m_stencilOpFace, stencilOpValue, GL_KEEP, GL_KEEP);
            break;

        case GL_STENCIL_PASS_DEPTH_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
            glStencilOpSeparate(m_stencilOpFace, GL_KEEP, stencilOpValue, GL_KEEP);
            break;

        case GL_STENCIL_PASS_DEPTH_PASS:
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:
            glStencilOpSeparate(m_stencilOpFace, GL_KEEP, GL_KEEP, stencilOpValue);
            break;

        default:
            DE_ASSERT(false && "should not happen");
            break;
        }
    }

    GLenum m_stencilOpFace;
};

class StencilFuncTestCase : public ApiCase
{
public:
    StencilFuncTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, GL_STENCIL_FUNC, GL_ALWAYS);
        expectError(GL_NO_ERROR);

        const GLenum stencilfuncValues[] = {GL_NEVER, GL_ALWAYS, GL_LESS,    GL_LEQUAL,
                                            GL_EQUAL, GL_GEQUAL, GL_GREATER, GL_NOTEQUAL};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stencilfuncValues); ++ndx)
        {
            glStencilFunc(stencilfuncValues[ndx], 0, 0);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, GL_STENCIL_FUNC, stencilfuncValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, GL_STENCIL_BACK_FUNC, stencilfuncValues[ndx]);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class StencilFuncSeparateTestCase : public ApiCase
{
public:
    StencilFuncSeparateTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                                GLenum stencilFuncName, GLenum stencilFuncFace)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_stencilFuncName(stencilFuncName)
        , m_stencilFuncFace(stencilFuncFace)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_stencilFuncName, GL_ALWAYS);
        expectError(GL_NO_ERROR);

        const GLenum stencilfuncValues[] = {GL_NEVER, GL_ALWAYS, GL_LESS,    GL_LEQUAL,
                                            GL_EQUAL, GL_GEQUAL, GL_GREATER, GL_NOTEQUAL};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stencilfuncValues); ++ndx)
        {
            glStencilFuncSeparate(m_stencilFuncFace, stencilfuncValues[ndx], 0, 0);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_stencilFuncName, stencilfuncValues[ndx]);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_stencilFuncName;
    GLenum m_stencilFuncFace;
};

class StencilMaskTestCase : public ApiCase
{
public:
    StencilMaskTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                        GLenum testTargetName)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
    {
    }

    void test(void)
    {
        const int stencilBits = m_context.getRenderTarget().getStencilBits();

        m_verifier->verifyStencilMaskInitial(m_testCtx, m_testTargetName, stencilBits);
        expectError(GL_NO_ERROR);

        for (int stencilBit = 0; stencilBit < stencilBits; ++stencilBit)
        {
            const int mask = 1 << stencilBit;

            glStencilFunc(GL_ALWAYS, 0, mask);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, mask);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
};

class StencilMaskSeparateTestCase : public ApiCase
{
public:
    StencilMaskSeparateTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                                GLenum testTargetName, GLenum stencilFuncTargetFace)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
        , m_stencilFuncTargetFace(stencilFuncTargetFace)
    {
    }

    void test(void)
    {
        const int stencilBits = m_context.getRenderTarget().getStencilBits();

        m_verifier->verifyStencilMaskInitial(m_testCtx, m_testTargetName, stencilBits);
        expectError(GL_NO_ERROR);

        for (int stencilBit = 0; stencilBit < stencilBits; ++stencilBit)
        {
            const int mask = 1 << stencilBit;

            glStencilFuncSeparate(m_stencilFuncTargetFace, GL_ALWAYS, 0, mask);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, mask);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
    GLenum m_stencilFuncTargetFace;
};

class StencilWriteMaskTestCase : public ApiCase
{
public:
    StencilWriteMaskTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                             GLenum testTargetName)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
    {
    }

    void test(void)
    {
        const int stencilBits = m_context.getRenderTarget().getStencilBits();

        for (int stencilBit = 0; stencilBit < stencilBits; ++stencilBit)
        {
            const int mask = 1 << stencilBit;

            glStencilMask(mask);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, mask);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
};

class StencilWriteMaskSeparateTestCase : public ApiCase
{
public:
    StencilWriteMaskSeparateTestCase(Context &context, StateVerifier *verifier, const char *name,
                                     const char *description, GLenum testTargetName, GLenum stencilTargetFace)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
        , m_stencilTargetFace(stencilTargetFace)
    {
    }

    void test(void)
    {
        const int stencilBits = m_context.getRenderTarget().getStencilBits();

        for (int stencilBit = 0; stencilBit < stencilBits; ++stencilBit)
        {
            const int mask = 1 << stencilBit;

            glStencilMaskSeparate(m_stencilTargetFace, mask);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, mask);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
    GLenum m_stencilTargetFace;
};

class PixelStoreTestCase : public ApiCase
{
public:
    PixelStoreTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                       GLenum testTargetName, int initialValue)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
        , m_initialValue(initialValue)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyInteger(m_testCtx, m_testTargetName, m_initialValue);
        expectError(GL_NO_ERROR);

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            const int referenceValue = rnd.getInt(0, 64000);

            glPixelStorei(m_testTargetName, referenceValue);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, referenceValue);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
    int m_initialValue;
};

class PixelStoreAlignTestCase : public ApiCase
{
public:
    PixelStoreAlignTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                            GLenum testTargetName)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_testTargetName, 4);
        expectError(GL_NO_ERROR);

        const int alignments[] = {1, 2, 4, 8};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(alignments); ++ndx)
        {
            const int referenceValue = alignments[ndx];

            glPixelStorei(m_testTargetName, referenceValue);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, referenceValue);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
};

class BlendFuncTestCase : public ApiCase
{
public:
    BlendFuncTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                      GLenum testTargetName, int initialValue)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
        , m_initialValue(initialValue)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_testTargetName, m_initialValue);
        expectError(GL_NO_ERROR);

        const GLenum blendFuncValues[] = {GL_ZERO,
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

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(blendFuncValues); ++ndx)
        {
            const GLenum referenceValue = blendFuncValues[ndx];

            SetBlendFunc(referenceValue);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, referenceValue);
            expectError(GL_NO_ERROR);
        }
    }

protected:
    virtual void SetBlendFunc(GLenum func)
    {
        switch (m_testTargetName)
        {
        case GL_BLEND_SRC_RGB:
        case GL_BLEND_SRC_ALPHA:
            glBlendFunc(func, GL_ZERO);
            break;

        case GL_BLEND_DST_RGB:
        case GL_BLEND_DST_ALPHA:
            glBlendFunc(GL_ZERO, func);
            break;

        default:
            DE_ASSERT(false && "should not happen");
            break;
        }
    }

    StateVerifier *m_verifier;
    GLenum m_testTargetName;
    int m_initialValue;
};

class BlendFuncSeparateTestCase : public BlendFuncTestCase
{
public:
    BlendFuncSeparateTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                              GLenum testTargetName, int initialValue)
        : BlendFuncTestCase(context, verifier, name, description, testTargetName, initialValue)
    {
    }

    void SetBlendFunc(GLenum func)
    {
        switch (m_testTargetName)
        {
        case GL_BLEND_SRC_RGB:
            glBlendFuncSeparate(func, GL_ZERO, GL_ZERO, GL_ZERO);
            break;

        case GL_BLEND_DST_RGB:
            glBlendFuncSeparate(GL_ZERO, func, GL_ZERO, GL_ZERO);
            break;

        case GL_BLEND_SRC_ALPHA:
            glBlendFuncSeparate(GL_ZERO, GL_ZERO, func, GL_ZERO);
            break;

        case GL_BLEND_DST_ALPHA:
            glBlendFuncSeparate(GL_ZERO, GL_ZERO, GL_ZERO, func);
            break;

        default:
            DE_ASSERT(false && "should not happen");
            break;
        }
    }
};

class BlendEquationTestCase : public ApiCase
{
public:
    BlendEquationTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                          GLenum testTargetName, int initialValue)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
        , m_initialValue(initialValue)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_testTargetName, m_initialValue);
        expectError(GL_NO_ERROR);

        const GLenum blendFuncValues[] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(blendFuncValues); ++ndx)
        {
            const GLenum referenceValue = blendFuncValues[ndx];

            SetBlendEquation(referenceValue);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_testTargetName, referenceValue);
            expectError(GL_NO_ERROR);
        }
    }

protected:
    virtual void SetBlendEquation(GLenum equation)
    {
        glBlendEquation(equation);
    }

    StateVerifier *m_verifier;
    GLenum m_testTargetName;
    int m_initialValue;
};
class BlendEquationSeparateTestCase : public BlendEquationTestCase
{
public:
    BlendEquationSeparateTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                                  GLenum testTargetName, int initialValue)
        : BlendEquationTestCase(context, verifier, name, description, testTargetName, initialValue)
    {
    }

protected:
    void SetBlendEquation(GLenum equation)
    {
        switch (m_testTargetName)
        {
        case GL_BLEND_EQUATION_RGB:
            glBlendEquationSeparate(equation, GL_FUNC_ADD);
            break;

        case GL_BLEND_EQUATION_ALPHA:
            glBlendEquationSeparate(GL_FUNC_ADD, equation);
            break;

        default:
            DE_ASSERT(false && "should not happen");
            break;
        }
    }
};

class ImplementationArrayTestCase : public ApiCase
{
public:
    ImplementationArrayTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                                GLenum testTargetName, GLenum testTargetLengthTargetName, int minValue)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
        , m_testTargetName(testTargetName)
        , m_testTargetLengthTargetName(testTargetLengthTargetName)
        , m_minValue(minValue)
    {
    }

    void test(void)
    {
        m_verifier->verifyIntegerGreaterOrEqual(m_testCtx, m_testTargetLengthTargetName, m_minValue);
        expectError(GL_NO_ERROR);

        GLint targetArrayLength = 0;
        glGetIntegerv(m_testTargetLengthTargetName, &targetArrayLength);
        expectError(GL_NO_ERROR);

        if (targetArrayLength)
        {
            std::vector<GLint> queryResult;
            queryResult.resize(targetArrayLength, 0);

            glGetIntegerv(m_testTargetName, &queryResult[0]);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
    GLenum m_testTargetName;
    GLenum m_testTargetLengthTargetName;
    int m_minValue;
};

class BindingTest : public TestCase
{
public:
    BindingTest(Context &context, const char *name, const char *desc, QueryType type);

    IterateResult iterate(void);

    virtual void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const = 0;

protected:
    const QueryType m_type;
};

BindingTest::BindingTest(Context &context, const char *name, const char *desc, QueryType type)
    : TestCase(context, name, desc)
    , m_type(type)
{
}

BindingTest::IterateResult BindingTest::iterate(void)
{
    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    tcu::ResultCollector result(m_context.getTestContext().getLog(), " // ERROR: ");

    gl.enableLogging(true);

    test(gl, result);

    result.setTestContextResult(m_testCtx);
    return STOP;
}

class TransformFeedbackBindingTestCase : public BindingTest
{
public:
    TransformFeedbackBindingTestCase(Context &context, QueryType type, const char *name)
        : BindingTest(context, name, "GL_TRANSFORM_FEEDBACK_BINDING", type)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
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

        GLuint shaderVert;
        GLuint shaderFrag;
        GLuint shaderProg;
        GLuint transformfeedback = 0;
        GLuint feedbackBufferId  = 0;

        {
            const tcu::ScopedLogSection section(gl.getLog(), "Initial", "Initial");
            verifyStateInteger(result, gl, GL_TRANSFORM_FEEDBACK_BINDING, 0, m_type);
        }

        gl.glGenTransformFeedbacks(1, &transformfeedback);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenTransformFeedbacks");

        {
            const tcu::ScopedLogSection section(gl.getLog(), "VertexShader", "Vertex Shader");

            GLint compileStatus = -1;

            shaderVert = gl.glCreateShader(GL_VERTEX_SHADER);
            gl.glShaderSource(shaderVert, 1, &transformFeedbackTestVertSource, nullptr);
            gl.glCompileShader(shaderVert);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glCompileShader");

            gl.glGetShaderiv(shaderVert, GL_COMPILE_STATUS, &compileStatus);
            if (compileStatus != GL_TRUE)
                result.fail("expected GL_TRUE");
        }
        {
            const tcu::ScopedLogSection section(gl.getLog(), "FragmentShader", "Fragment Shader");

            GLint compileStatus = -1;

            shaderFrag = gl.glCreateShader(GL_FRAGMENT_SHADER);
            gl.glShaderSource(shaderFrag, 1, &transformFeedbackTestFragSource, nullptr);
            gl.glCompileShader(shaderFrag);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glCompileShader");

            gl.glGetShaderiv(shaderFrag, GL_COMPILE_STATUS, &compileStatus);
            if (compileStatus != GL_TRUE)
                result.fail("expected GL_TRUE");
        }
        {
            const tcu::ScopedLogSection section(gl.getLog(), "Program", "Create and bind program");

            const char *transform_feedback_outputs = "gl_Position";
            GLint linkStatus                       = -1;

            shaderProg = gl.glCreateProgram();
            gl.glAttachShader(shaderProg, shaderVert);
            gl.glAttachShader(shaderProg, shaderFrag);
            gl.glTransformFeedbackVaryings(shaderProg, 1, &transform_feedback_outputs, GL_INTERLEAVED_ATTRIBS);
            gl.glLinkProgram(shaderProg);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glLinkProgram");

            gl.glGetProgramiv(shaderProg, GL_LINK_STATUS, &linkStatus);
            if (linkStatus != GL_TRUE)
                result.fail("expected GL_TRUE");
        }

        gl.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformfeedback);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindTransformFeedback");

        gl.glGenBuffers(1, &feedbackBufferId);
        gl.glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBufferId);
        gl.glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_DYNAMIC_READ);
        gl.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, feedbackBufferId);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "bind buffers");

        gl.glUseProgram(shaderProg);

        verifyStateInteger(result, gl, GL_TRANSFORM_FEEDBACK_BINDING, transformfeedback, m_type);

        gl.glUseProgram(0);
        gl.glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        gl.glDeleteTransformFeedbacks(1, &transformfeedback);

        verifyStateInteger(result, gl, GL_TRANSFORM_FEEDBACK_BINDING, 0, m_type);

        gl.glDeleteBuffers(1, &feedbackBufferId);
        gl.glDeleteShader(shaderVert);
        gl.glDeleteShader(shaderFrag);
        gl.glDeleteProgram(shaderProg);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteProgram");
    }
};

class CurrentProgramBindingTestCase : public BindingTest
{
public:
    CurrentProgramBindingTestCase(Context &context, QueryType type, const char *name, const char *description)
        : BindingTest(context, name, description, type)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        static const char *testVertSource = "#version 300 es\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(0.0);\n"
                                            "}\n\0";
        static const char *testFragSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(0.0);\n"
                                            "}\n\0";

        GLuint shaderVert;
        GLuint shaderFrag;
        GLuint shaderProg;

        {
            const tcu::ScopedLogSection section(gl.getLog(), "Initial", "Initial");

            verifyStateInteger(result, gl, GL_CURRENT_PROGRAM, 0, m_type);
        }
        {
            const tcu::ScopedLogSection section(gl.getLog(), "VertexShader", "Vertex Shader");

            GLint compileStatus = -1;

            shaderVert = gl.glCreateShader(GL_VERTEX_SHADER);
            gl.glShaderSource(shaderVert, 1, &testVertSource, nullptr);
            gl.glCompileShader(shaderVert);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glCompileShader");

            gl.glGetShaderiv(shaderVert, GL_COMPILE_STATUS, &compileStatus);
            if (compileStatus != GL_TRUE)
                result.fail("expected GL_TRUE");
        }
        {
            const tcu::ScopedLogSection section(gl.getLog(), "FragmentShader", "Fragment Shader");

            GLint compileStatus = -1;

            shaderFrag = gl.glCreateShader(GL_FRAGMENT_SHADER);
            gl.glShaderSource(shaderFrag, 1, &testFragSource, nullptr);
            gl.glCompileShader(shaderFrag);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glCompileShader");

            gl.glGetShaderiv(shaderFrag, GL_COMPILE_STATUS, &compileStatus);
            if (compileStatus != GL_TRUE)
                result.fail("expected GL_TRUE");
        }
        {
            const tcu::ScopedLogSection section(gl.getLog(), "Program", "Create and bind program");

            GLint linkStatus = -1;

            shaderProg = gl.glCreateProgram();
            gl.glAttachShader(shaderProg, shaderVert);
            gl.glAttachShader(shaderProg, shaderFrag);
            gl.glLinkProgram(shaderProg);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glLinkProgram");

            gl.glGetProgramiv(shaderProg, GL_LINK_STATUS, &linkStatus);
            if (linkStatus != GL_TRUE)
                result.fail("expected GL_TRUE");

            gl.glUseProgram(shaderProg);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glUseProgram");

            verifyStateInteger(result, gl, GL_CURRENT_PROGRAM, shaderProg, m_type);
        }
        {
            const tcu::ScopedLogSection section(gl.getLog(), "Delete", "Delete program while in use");

            gl.glDeleteShader(shaderVert);
            gl.glDeleteShader(shaderFrag);
            gl.glDeleteProgram(shaderProg);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteProgram");

            verifyStateInteger(result, gl, GL_CURRENT_PROGRAM, shaderProg, m_type);
        }
        {
            const tcu::ScopedLogSection section(gl.getLog(), "Unbind", "Unbind program");
            gl.glUseProgram(0);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glUseProgram");

            verifyStateInteger(result, gl, GL_CURRENT_PROGRAM, 0, m_type);
        }
    }
};

class VertexArrayBindingTestCase : public BindingTest
{
public:
    VertexArrayBindingTestCase(Context &context, QueryType type, const char *name, const char *description)
        : BindingTest(context, name, description, type)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        verifyStateInteger(result, gl, GL_VERTEX_ARRAY_BINDING, 0, m_type);

        GLuint vertexArrayObject = 0;
        gl.glGenVertexArrays(1, &vertexArrayObject);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenVertexArrays");

        gl.glBindVertexArray(vertexArrayObject);
        verifyStateInteger(result, gl, GL_VERTEX_ARRAY_BINDING, vertexArrayObject, m_type);

        gl.glDeleteVertexArrays(1, &vertexArrayObject);
        verifyStateInteger(result, gl, GL_VERTEX_ARRAY_BINDING, 0, m_type);
    }
};

class BufferBindingTestCase : public BindingTest
{
public:
    BufferBindingTestCase(Context &context, QueryType type, const char *name, const char *description,
                          GLenum bufferBindingName, GLenum bufferType)
        : BindingTest(context, name, description, type)
        , m_bufferBindingName(bufferBindingName)
        , m_bufferType(bufferType)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        verifyStateInteger(result, gl, m_bufferBindingName, 0, m_type);

        GLuint bufferObject = 0;
        gl.glGenBuffers(1, &bufferObject);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenBuffers");

        gl.glBindBuffer(m_bufferType, bufferObject);
        verifyStateInteger(result, gl, m_bufferBindingName, bufferObject, m_type);

        gl.glDeleteBuffers(1, &bufferObject);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteBuffers");

        verifyStateInteger(result, gl, m_bufferBindingName, 0, m_type);
    }

private:
    const GLenum m_bufferBindingName;
    const GLenum m_bufferType;
};

class ElementArrayBufferBindingTestCase : public BindingTest
{
public:
    ElementArrayBufferBindingTestCase(Context &context, QueryType type, const char *name)
        : BindingTest(context, name, "GL_ELEMENT_ARRAY_BUFFER_BINDING", type)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        // Test with default VAO
        {
            const tcu::ScopedLogSection section(gl.getLog(), "DefaultVAO", "Test with default VAO");

            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, m_type);

            GLuint bufferObject = 0;
            gl.glGenBuffers(1, &bufferObject);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenBuffers");

            gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObject);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, bufferObject, m_type);

            gl.glDeleteBuffers(1, &bufferObject);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, m_type);
        }

        // Test with multiple VAOs
        {
            const tcu::ScopedLogSection section(gl.getLog(), "WithVAO", "Test with VAO");

            GLuint vaos[2]    = {0};
            GLuint buffers[2] = {0};

            gl.glGenVertexArrays(2, vaos);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenVertexArrays");

            gl.glGenBuffers(2, buffers);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenBuffers");

            // initial
            gl.glBindVertexArray(vaos[0]);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, m_type);

            // after setting
            gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[0]);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, buffers[0], m_type);

            // initial of vao 2
            gl.glBindVertexArray(vaos[1]);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, m_type);

            // after setting to 2
            gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, buffers[1], m_type);

            // vao 1 still has buffer 1 bound?
            gl.glBindVertexArray(vaos[0]);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, buffers[0], m_type);

            // deleting clears from bound vaos ...
            gl.glDeleteBuffers(2, buffers);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, m_type);

            // ... but does not from non-bound vaos?
            gl.glBindVertexArray(vaos[1]);
            verifyStateInteger(result, gl, GL_ELEMENT_ARRAY_BUFFER_BINDING, buffers[1], m_type);

            gl.glDeleteVertexArrays(2, vaos);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteVertexArrays");
        }
    }
};

class StencilClearValueTestCase : public ApiCase
{
public:
    StencilClearValueTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, GL_STENCIL_CLEAR_VALUE, 0);
        expectError(GL_NO_ERROR);

        const int stencilBits = m_context.getRenderTarget().getStencilBits();

        for (int stencilBit = 0; stencilBit < stencilBits; ++stencilBit)
        {
            const int ref = 1 << stencilBit;

            glClearStencil(ref); // mask should not affect the REF
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, GL_STENCIL_CLEAR_VALUE, ref);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class ActiveTextureTestCase : public ApiCase
{
public:
    ActiveTextureTestCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyInteger(m_testCtx, GL_ACTIVE_TEXTURE, GL_TEXTURE0);
        expectError(GL_NO_ERROR);

        GLint textureUnits = 0;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &textureUnits);
        expectError(GL_NO_ERROR);

        for (int ndx = 0; ndx < textureUnits; ++ndx)
        {
            glActiveTexture(GL_TEXTURE0 + ndx);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, GL_ACTIVE_TEXTURE, GL_TEXTURE0 + ndx);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class RenderbufferBindingTestCase : public BindingTest
{
public:
    RenderbufferBindingTestCase(Context &context, QueryType type, const char *name, const char *description)
        : BindingTest(context, name, description, type)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        verifyStateInteger(result, gl, GL_RENDERBUFFER_BINDING, 0, m_type);

        GLuint renderBuffer = 0;
        gl.glGenRenderbuffers(1, &renderBuffer);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenRenderbuffers");

        gl.glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindRenderbuffer");

        verifyStateInteger(result, gl, GL_RENDERBUFFER_BINDING, renderBuffer, m_type);

        gl.glDeleteRenderbuffers(1, &renderBuffer);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteRenderbuffers");

        verifyStateInteger(result, gl, GL_RENDERBUFFER_BINDING, 0, m_type);
    }
};

class SamplerObjectBindingTestCase : public BindingTest
{
public:
    SamplerObjectBindingTestCase(Context &context, QueryType type, const char *name, const char *description)
        : BindingTest(context, name, description, type)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        verifyStateInteger(result, gl, GL_SAMPLER_BINDING, 0, m_type);

        {
            const tcu::ScopedLogSection section(gl.getLog(), "SingleUnit", "Single unit");

            GLuint sampler = 0;
            gl.glGenSamplers(1, &sampler);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenSamplers");

            gl.glBindSampler(0, sampler);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindSampler");

            verifyStateInteger(result, gl, GL_SAMPLER_BINDING, sampler, m_type);

            gl.glDeleteSamplers(1, &sampler);
            verifyStateInteger(result, gl, GL_SAMPLER_BINDING, 0, m_type);
        }

        {
            const tcu::ScopedLogSection section(gl.getLog(), "MultipleUnits", "Multiple units");

            GLuint samplerA = 0;
            GLuint samplerB = 0;
            gl.glGenSamplers(1, &samplerA);
            gl.glGenSamplers(1, &samplerB);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenSamplers");

            gl.glBindSampler(1, samplerA);
            gl.glBindSampler(2, samplerB);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindSampler");

            verifyStateInteger(result, gl, GL_SAMPLER_BINDING, 0, m_type);

            gl.glActiveTexture(GL_TEXTURE1);
            verifyStateInteger(result, gl, GL_SAMPLER_BINDING, samplerA, m_type);

            gl.glActiveTexture(GL_TEXTURE2);
            verifyStateInteger(result, gl, GL_SAMPLER_BINDING, samplerB, m_type);

            gl.glDeleteSamplers(1, &samplerB);
            gl.glDeleteSamplers(1, &samplerA);
            GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteSamplers");
        }
    }
};

class TextureBindingTestCase : public BindingTest
{
public:
    TextureBindingTestCase(Context &context, QueryType type, const char *name, const char *description,
                           GLenum testBindingName, GLenum textureType)
        : BindingTest(context, name, description, type)
        , m_testBindingName(testBindingName)
        , m_textureType(textureType)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        verifyStateInteger(result, gl, m_testBindingName, 0, m_type);

        GLuint texture = 0;
        gl.glGenTextures(1, &texture);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenTextures");

        gl.glBindTexture(m_textureType, texture);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glBindTexture");

        verifyStateInteger(result, gl, m_testBindingName, texture, m_type);

        gl.glDeleteTextures(1, &texture);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteTextures");

        verifyStateInteger(result, gl, m_testBindingName, 0, m_type);
    }

private:
    const GLenum m_testBindingName;
    const GLenum m_textureType;
};

class FrameBufferBindingTestCase : public BindingTest
{
public:
    FrameBufferBindingTestCase(Context &context, QueryType type, const char *name, const char *description)
        : BindingTest(context, name, description, type)
    {
    }

    void test(glu::CallLogWrapper &gl, tcu::ResultCollector &result) const
    {
        verifyStateInteger(result, gl, GL_DRAW_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_READ_FRAMEBUFFER_BINDING, 0, m_type);

        GLuint framebufferId = 0;
        gl.glGenFramebuffers(1, &framebufferId);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glGenFramebuffers");

        gl.glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "bind GL_FRAMEBUFFER");

        verifyStateInteger(result, gl, GL_DRAW_FRAMEBUFFER_BINDING, framebufferId, m_type);
        verifyStateInteger(result, gl, GL_FRAMEBUFFER_BINDING, framebufferId, m_type);
        verifyStateInteger(result, gl, GL_READ_FRAMEBUFFER_BINDING, framebufferId, m_type);

        gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "unbind GL_FRAMEBUFFER");

        verifyStateInteger(result, gl, GL_DRAW_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_READ_FRAMEBUFFER_BINDING, 0, m_type);

        gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferId);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "bind GL_READ_FRAMEBUFFER");

        verifyStateInteger(result, gl, GL_DRAW_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_READ_FRAMEBUFFER_BINDING, framebufferId, m_type);

        gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferId);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "bind GL_DRAW_FRAMEBUFFER");

        verifyStateInteger(result, gl, GL_DRAW_FRAMEBUFFER_BINDING, framebufferId, m_type);
        verifyStateInteger(result, gl, GL_FRAMEBUFFER_BINDING, framebufferId, m_type);
        verifyStateInteger(result, gl, GL_READ_FRAMEBUFFER_BINDING, framebufferId, m_type);

        gl.glDeleteFramebuffers(1, &framebufferId);
        GLS_COLLECT_GL_ERROR(result, gl.glGetError(), "glDeleteFramebuffers");

        verifyStateInteger(result, gl, GL_DRAW_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_FRAMEBUFFER_BINDING, 0, m_type);
        verifyStateInteger(result, gl, GL_READ_FRAMEBUFFER_BINDING, 0, m_type);
    }
};

class ImplementationColorReadTestCase : public ApiCase
{
public:
    ImplementationColorReadTestCase(Context &context, StateVerifier *verifier, const char *name,
                                    const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        const GLint defaultColorTypes[]   = {GL_UNSIGNED_BYTE,
                                             GL_BYTE,
                                             GL_UNSIGNED_SHORT,
                                             GL_SHORT,
                                             GL_UNSIGNED_INT,
                                             GL_INT,
                                             GL_HALF_FLOAT,
                                             GL_FLOAT,
                                             GL_UNSIGNED_SHORT_5_6_5,
                                             GL_UNSIGNED_SHORT_4_4_4_4,
                                             GL_UNSIGNED_SHORT_5_5_5_1,
                                             GL_UNSIGNED_INT_2_10_10_10_REV,
                                             GL_UNSIGNED_INT_10F_11F_11F_REV};
        const GLint defaultColorFormats[] = {GL_RGBA, GL_RGBA_INTEGER, GL_RGB, GL_RGB_INTEGER,
                                             GL_RG,   GL_RG_INTEGER,   GL_RED, GL_RED_INTEGER};

        std::vector<GLint> validColorTypes;
        std::vector<GLint> validColorFormats;

        // Defined by the spec

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(defaultColorTypes); ++ndx)
            validColorTypes.push_back(defaultColorTypes[ndx]);
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(defaultColorFormats); ++ndx)
            validColorFormats.push_back(defaultColorFormats[ndx]);

        // Extensions

        if (m_context.getContextInfo().isExtensionSupported("GL_EXT_texture_format_BGRA8888") ||
            m_context.getContextInfo().isExtensionSupported("GL_APPLE_texture_format_BGRA8888"))
            validColorFormats.push_back(GL_BGRA);

        if (m_context.getContextInfo().isExtensionSupported("GL_EXT_read_format_bgra"))
        {
            validColorFormats.push_back(GL_BGRA);
            validColorTypes.push_back(GL_UNSIGNED_SHORT_4_4_4_4_REV);
            validColorTypes.push_back(GL_UNSIGNED_SHORT_1_5_5_5_REV);
        }

        if (m_context.getContextInfo().isExtensionSupported("GL_IMG_read_format"))
        {
            validColorFormats.push_back(GL_BGRA);
            validColorTypes.push_back(GL_UNSIGNED_SHORT_4_4_4_4_REV);
        }

        if (m_context.getContextInfo().isExtensionSupported("GL_NV_sRGB_formats"))
        {
            validColorFormats.push_back(GL_SLUMINANCE_NV);
            validColorFormats.push_back(GL_SLUMINANCE_ALPHA_NV);
        }

        if (m_context.getContextInfo().isExtensionSupported("GL_NV_bgr"))
        {
            validColorFormats.push_back(GL_BGR_NV);
        }

        m_verifier->verifyIntegerAnyOf(m_testCtx, GL_IMPLEMENTATION_COLOR_READ_TYPE, &validColorTypes[0],
                                       validColorTypes.size());
        m_verifier->verifyIntegerAnyOf(m_testCtx, GL_IMPLEMENTATION_COLOR_READ_FORMAT, &validColorFormats[0],
                                       validColorFormats.size());
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class ReadBufferCase : public ApiCase
{
public:
    ReadBufferCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        const bool isGlCore45  = glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 5));
        GLenum colorAttachment = isGlCore45 ? GL_FRONT : GL_BACK;
        if (isGlCore45)
        {
            // Make sure GL_FRONT is available. If not, use GL_BACK instead.
            GLint objectType = GL_NONE;
            glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, colorAttachment,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objectType);
            if (objectType == GL_NONE)
            {
                colorAttachment = GL_BACK;
            }
        }
        const GLint validInitialValues[] = {(GLint)colorAttachment, GL_BACK, GL_NONE};
        m_verifier->verifyIntegerAnyOf(m_testCtx, GL_READ_BUFFER, validInitialValues,
                                       DE_LENGTH_OF_ARRAY(validInitialValues));
        expectError(GL_NO_ERROR);

        glReadBuffer(GL_NONE);
        m_verifier->verifyInteger(m_testCtx, GL_READ_BUFFER, GL_NONE);
        expectError(GL_NO_ERROR);

        glReadBuffer(colorAttachment);
        m_verifier->verifyInteger(m_testCtx, GL_READ_BUFFER, colorAttachment);
        expectError(GL_NO_ERROR);

        // test GL_READ_BUFFER with framebuffers

        GLuint framebufferId = 0;
        glGenFramebuffers(1, &framebufferId);
        expectError(GL_NO_ERROR);

        GLuint renderbuffer_id = 0;
        glGenRenderbuffers(1, &renderbuffer_id);
        expectError(GL_NO_ERROR);

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_id);
        expectError(GL_NO_ERROR);

        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 128, 128);
        expectError(GL_NO_ERROR);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferId);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer_id);
        expectError(GL_NO_ERROR);

        m_verifier->verifyInteger(m_testCtx, GL_READ_BUFFER, GL_COLOR_ATTACHMENT0);

        glDeleteFramebuffers(1, &framebufferId);
        glDeleteRenderbuffers(1, &renderbuffer_id);
        expectError(GL_NO_ERROR);

        m_verifier->verifyInteger(m_testCtx, GL_READ_BUFFER, colorAttachment);
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class DrawBufferCase : public ApiCase
{
public:
    DrawBufferCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        const GLint validInitialValues[] = {GL_FRONT, GL_BACK, GL_NONE};
        m_verifier->verifyIntegerAnyOf(m_testCtx, GL_DRAW_BUFFER0, validInitialValues,
                                       DE_LENGTH_OF_ARRAY(validInitialValues));
        expectError(GL_NO_ERROR);

        GLenum bufs = GL_NONE;
        glDrawBuffers(1, &bufs);
        m_verifier->verifyInteger(m_testCtx, GL_DRAW_BUFFER0, GL_NONE);
        expectError(GL_NO_ERROR);

        bufs = GL_BACK;
        glDrawBuffers(1, &bufs);
        const GLint validDraw0Values[] = {GL_FRONT_LEFT, GL_BACK};
        m_verifier->verifyIntegerAnyOf(m_testCtx, GL_DRAW_BUFFER0, validDraw0Values,
                                       DE_LENGTH_OF_ARRAY(validDraw0Values));
        expectError(GL_NO_ERROR);

        // test GL_DRAW_BUFFER with framebuffers

        GLuint framebufferId = 0;
        glGenFramebuffers(1, &framebufferId);
        expectError(GL_NO_ERROR);

        GLuint renderbuffer_ids[2] = {0};
        glGenRenderbuffers(2, renderbuffer_ids);
        expectError(GL_NO_ERROR);

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_ids[0]);
        expectError(GL_NO_ERROR);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 128, 128);
        expectError(GL_NO_ERROR);

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_ids[1]);
        expectError(GL_NO_ERROR);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 128, 128);
        expectError(GL_NO_ERROR);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferId);
        expectError(GL_NO_ERROR);

        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer_ids[0]);
        expectError(GL_NO_ERROR);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, renderbuffer_ids[1]);
        expectError(GL_NO_ERROR);

        // only the initial state the draw buffer for fragment color zero is defined
        m_verifier->verifyInteger(m_testCtx, GL_DRAW_BUFFER0, GL_COLOR_ATTACHMENT0);

        GLenum bufTargets[2] = {GL_NONE, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufTargets);
        m_verifier->verifyInteger(m_testCtx, GL_DRAW_BUFFER0, GL_NONE);
        m_verifier->verifyInteger(m_testCtx, GL_DRAW_BUFFER1, GL_COLOR_ATTACHMENT1);

        glDeleteFramebuffers(1, &framebufferId);
        glDeleteRenderbuffers(2, renderbuffer_ids);
        expectError(GL_NO_ERROR);

        m_verifier->verifyIntegerAnyOf(m_testCtx, GL_DRAW_BUFFER0, validDraw0Values,
                                       DE_LENGTH_OF_ARRAY(validDraw0Values));
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

static const char *getQueryTypeSuffix(QueryType type)
{
    switch (type)
    {
    case QUERY_BOOLEAN:
        return "_getboolean";
    case QUERY_INTEGER:
        return "_getinteger";
    case QUERY_INTEGER64:
        return "_getinteger64";
    case QUERY_FLOAT:
        return "_getfloat";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)                                                 \
    do                                                                                           \
    {                                                                                            \
        for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++) \
        {                                                                                        \
            StateVerifier *verifier = (VERIFIERS)[_verifierNdx];                                 \
            CODE_BLOCK;                                                                          \
        }                                                                                        \
    } while (0)

#define FOR_EACH_QUERYTYPE(QUERYTYPES, CODE_BLOCK)                                                   \
    do                                                                                               \
    {                                                                                                \
        for (int _queryTypeNdx = 0; _queryTypeNdx < DE_LENGTH_OF_ARRAY(QUERYTYPES); _queryTypeNdx++) \
        {                                                                                            \
            const QueryType queryType = (QUERYTYPES)[_queryTypeNdx];                                 \
            CODE_BLOCK;                                                                              \
        }                                                                                            \
    } while (0)

} // namespace

IntegerStateQueryTests::IntegerStateQueryTests(Context &context)
    : TestCaseGroup(context, "integers", "Integer Values")
    , m_verifierBoolean(nullptr)
    , m_verifierInteger(nullptr)
    , m_verifierInteger64(nullptr)
    , m_verifierFloat(nullptr)
{
}

IntegerStateQueryTests::~IntegerStateQueryTests(void)
{
    deinit();
}

void IntegerStateQueryTests::init(void)
{
    static const QueryType queryTypes[] = {
        QUERY_BOOLEAN,
        QUERY_INTEGER,
        QUERY_INTEGER64,
        QUERY_FLOAT,
    };

    DE_ASSERT(m_verifierBoolean == nullptr);
    DE_ASSERT(m_verifierInteger == nullptr);
    DE_ASSERT(m_verifierInteger64 == nullptr);
    DE_ASSERT(m_verifierFloat == nullptr);

    m_verifierBoolean =
        new GetBooleanVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierInteger =
        new GetIntegerVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierInteger64 =
        new GetInteger64Verifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierFloat =
        new GetFloatVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());

    const struct LimitedStateInteger
    {
        const char *name;
        const char *description;
        GLenum targetName;
        GLint value;
        bool skipForGl;
    } implementationMinLimits[] = {
        {"subpixel_bits", "SUBPIXEL_BITS has minimum value of 4", GL_SUBPIXEL_BITS, 4, false},
        {"max_3d_texture_size", "MAX_3D_TEXTURE_SIZE has minimum value of 256", GL_MAX_3D_TEXTURE_SIZE, 256, false},
        {"max_texture_size", "MAX_TEXTURE_SIZE has minimum value of 2048", GL_MAX_TEXTURE_SIZE, 2048, false},
        {"max_array_texture_layers", "MAX_ARRAY_TEXTURE_LAYERS has minimum value of 256", GL_MAX_ARRAY_TEXTURE_LAYERS,
         256, false},
        {"max_cube_map_texture_size", "MAX_CUBE_MAP_TEXTURE_SIZE has minimum value of 2048",
         GL_MAX_CUBE_MAP_TEXTURE_SIZE, 2048, false},
        {"max_renderbuffer_size", "MAX_RENDERBUFFER_SIZE has minimum value of 2048", GL_MAX_RENDERBUFFER_SIZE, 2048,
         false},
        {"max_draw_buffers", "MAX_DRAW_BUFFERS has minimum value of 4", GL_MAX_DRAW_BUFFERS, 4, false},
        {"max_color_attachments", "MAX_COLOR_ATTACHMENTS has minimum value of 4", GL_MAX_COLOR_ATTACHMENTS, 4, false},
        {"max_elements_indices", "MAX_ELEMENTS_INDICES has minimum value of 0", GL_MAX_ELEMENTS_INDICES, 0, false},
        {"max_elements_vertices", "MAX_ELEMENTS_VERTICES has minimum value of 0", GL_MAX_ELEMENTS_VERTICES, 0, false},
        {"num_extensions", "NUM_EXTENSIONS has minimum value of 0", GL_NUM_EXTENSIONS, 0, false},
        {"major_version", "MAJOR_VERSION has minimum value of 3", GL_MAJOR_VERSION, 3, false},
        {"minor_version", "MINOR_VERSION has minimum value of 0", GL_MINOR_VERSION, 0, false},
        {"max_vertex_attribs", "MAX_VERTEX_ATTRIBS has minimum value of 16", GL_MAX_VERTEX_ATTRIBS, 16, false},
        {"max_vertex_uniform_components", "MAX_VERTEX_UNIFORM_COMPONENTS has minimum value of 1024",
         GL_MAX_VERTEX_UNIFORM_COMPONENTS, 1024, false},
        {"max_vertex_uniform_vectors", "MAX_VERTEX_UNIFORM_VECTORS has minimum value of 256",
         GL_MAX_VERTEX_UNIFORM_VECTORS, 256, false},
        {"max_vertex_uniform_blocks", "MAX_VERTEX_UNIFORM_BLOCKS has minimum value of 12", GL_MAX_VERTEX_UNIFORM_BLOCKS,
         12, false},
        {"max_vertex_output_components", "MAX_VERTEX_OUTPUT_COMPONENTS has minimum value of 64",
         GL_MAX_VERTEX_OUTPUT_COMPONENTS, 64, false},
        {"max_vertex_texture_image_units", "MAX_VERTEX_TEXTURE_IMAGE_UNITS has minimum value of 16",
         GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, 16, false},
        {"max_fragment_uniform_components", "MAX_FRAGMENT_UNIFORM_COMPONENTS has minimum value of 896",
         GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, 896, false},
        {"max_fragment_uniform_vectors", "MAX_FRAGMENT_UNIFORM_VECTORS has minimum value of 224",
         GL_MAX_FRAGMENT_UNIFORM_VECTORS, 224, false},
        {"max_fragment_uniform_blocks", "MAX_FRAGMENT_UNIFORM_BLOCKS has minimum value of 12",
         GL_MAX_FRAGMENT_UNIFORM_BLOCKS, 12, false},
        {"max_fragment_input_components", "MAX_FRAGMENT_INPUT_COMPONENTS has minimum value of 60",
         GL_MAX_FRAGMENT_INPUT_COMPONENTS, 60, false},
        {"max_texture_image_units", "MAX_TEXTURE_IMAGE_UNITS has minimum value of 16", GL_MAX_TEXTURE_IMAGE_UNITS, 16,
         false},
        {"max_program_texel_offset", "MAX_PROGRAM_TEXEL_OFFSET has minimum value of 7", GL_MAX_PROGRAM_TEXEL_OFFSET, 7,
         false},
        {"max_uniform_buffer_bindings", "MAX_UNIFORM_BUFFER_BINDINGS has minimum value of 24",
         GL_MAX_UNIFORM_BUFFER_BINDINGS, 24, false},
        {"max_combined_uniform_blocks", "MAX_COMBINED_UNIFORM_BLOCKS has minimum value of 24",
         GL_MAX_COMBINED_UNIFORM_BLOCKS, 24, false},
        {"max_varying_components", "MAX_VARYING_COMPONENTS has minimum value of 60", GL_MAX_VARYING_COMPONENTS, 60,
         false},
        {"max_varying_vectors", "MAX_VARYING_VECTORS has minimum value of 15", GL_MAX_VARYING_VECTORS, 15, false},
        {"max_combined_texture_image_units", "MAX_COMBINED_TEXTURE_IMAGE_UNITS has minimum value of 32",
         GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 32, false},
        {"max_transform_feedback_interleaved_components",
         "MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS has minimum value of 64",
         GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, 64, false},
        {"max_transform_feedback_separate_attribs", "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS has minimum value of 4",
         GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, 4, false},
        {"max_transform_feedback_separate_components",
         "MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS has minimum value of 4",
         GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, 4, false},
        {"max_samples", "MAX_SAMPLES has minimum value of 4", GL_MAX_SAMPLES, 4, false},
        {"red_bits", "RED_BITS has minimum value of 0", GL_RED_BITS, 0, true},
        {"green_bits", "GREEN_BITS has minimum value of 0", GL_GREEN_BITS, 0, true},
        {"blue_bits", "BLUE_BITS has minimum value of 0", GL_BLUE_BITS, 0, true},
        {"alpha_bits", "ALPHA_BITS has minimum value of 0", GL_ALPHA_BITS, 0, true},
        {"depth_bits", "DEPTH_BITS has minimum value of 0", GL_DEPTH_BITS, 0, true},
        {"stencil_bits", "STENCIL_BITS has minimum value of 0", GL_STENCIL_BITS, 0, true},
    };
    const LimitedStateInteger implementationMaxLimits[] = {
        {"min_program_texel_offset", "MIN_PROGRAM_TEXEL_OFFSET has maximum value of -8", GL_MIN_PROGRAM_TEXEL_OFFSET,
         -8, false},
        {"uniform_buffer_offset_alignment", "UNIFORM_BUFFER_OFFSET_ALIGNMENT has minimum value of 1",
         GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, 256, false},
    };

    // \note implementation defined limits have their own tests so just check the conversions to boolean, int64 and float
    StateVerifier *implementationLimitVerifiers[] = {m_verifierBoolean, m_verifierInteger64, m_verifierFloat};

    const bool isGlCore45 = glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::core(4, 5));
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(implementationMinLimits); testNdx++)
    {
        if (implementationMinLimits[testNdx].skipForGl && isGlCore45)
            continue;
        FOR_EACH_VERIFIER(
            implementationLimitVerifiers,
            addChild(new ConstantMinimumValueTestCase(
                m_context, verifier,
                (std::string(implementationMinLimits[testNdx].name) + verifier->getTestNamePostfix()).c_str(),
                implementationMinLimits[testNdx].description, implementationMinLimits[testNdx].targetName,
                implementationMinLimits[testNdx].value)));
    }
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(implementationMaxLimits); testNdx++)
        FOR_EACH_VERIFIER(
            implementationLimitVerifiers,
            addChild(new ConstantMaximumValueTestCase(
                m_context, verifier,
                (std::string(implementationMaxLimits[testNdx].name) + verifier->getTestNamePostfix()).c_str(),
                implementationMaxLimits[testNdx].description, implementationMaxLimits[testNdx].targetName,
                implementationMaxLimits[testNdx].value)));

    StateVerifier *normalVerifiers[] = {m_verifierBoolean, m_verifierInteger, m_verifierInteger64, m_verifierFloat};

    FOR_EACH_VERIFIER(implementationLimitVerifiers,
                      addChild(new SampleBuffersTestCase(
                          m_context, verifier, (std::string("sample_buffers") + verifier->getTestNamePostfix()).c_str(),
                          "SAMPLE_BUFFERS")));

    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new SamplesTestCase(m_context, verifier,
                                     (std::string("samples") + verifier->getTestNamePostfix()).c_str(), "SAMPLES")));
    if (!isGlCore45)
        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new HintTestCase(m_context, verifier,
                                      (std::string("generate_mipmap_hint") + verifier->getTestNamePostfix()).c_str(),
                                      "GENERATE_MIPMAP_HINT", GL_GENERATE_MIPMAP_HINT)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new HintTestCase(
                          m_context, verifier,
                          (std::string("fragment_shader_derivative_hint") + verifier->getTestNamePostfix()).c_str(),
                          "FRAGMENT_SHADER_DERIVATIVE_HINT", GL_FRAGMENT_SHADER_DERIVATIVE_HINT)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new DepthFuncTestCase(
            m_context, verifier, (std::string("depth_func") + verifier->getTestNamePostfix()).c_str(), "DEPTH_FUNC")));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new CullFaceTestCase(
                          m_context, verifier, (std::string("cull_face_mode") + verifier->getTestNamePostfix()).c_str(),
                          "CULL_FACE_MODE")));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new FrontFaceTestCase(
                          m_context, verifier,
                          (std::string("front_face_mode") + verifier->getTestNamePostfix()).c_str(), "FRONT_FACE")));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new ViewPortTestCase(m_context, verifier,
                                      (std::string("viewport") + verifier->getTestNamePostfix()).c_str(), "VIEWPORT")));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new ScissorBoxTestCase(
                          m_context, verifier, (std::string("scissor_box") + verifier->getTestNamePostfix()).c_str(),
                          "SCISSOR_BOX")));
    FOR_EACH_VERIFIER(normalVerifiers, addChild(new MaxViewportDimsTestCase(
                                           m_context, verifier,
                                           (std::string("max_viewport_dims") + verifier->getTestNamePostfix()).c_str(),
                                           "MAX_VIEWPORT_DIMS")));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilRefTestCase(
                          m_context, verifier, (std::string("stencil_ref") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_REF", GL_STENCIL_REF)));
    FOR_EACH_VERIFIER(normalVerifiers, addChild(new StencilRefTestCase(
                                           m_context, verifier,
                                           (std::string("stencil_back_ref") + verifier->getTestNamePostfix()).c_str(),
                                           "STENCIL_BACK_REF", GL_STENCIL_BACK_REF)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilRefSeparateTestCase(
            m_context, verifier, (std::string("stencil_ref_separate") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_REF (separate)", GL_STENCIL_REF, GL_FRONT)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilRefSeparateTestCase(
            m_context, verifier, (std::string("stencil_ref_separate_both") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_REF (separate)", GL_STENCIL_REF, GL_FRONT_AND_BACK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilRefSeparateTestCase(
            m_context, verifier, (std::string("stencil_back_ref_separate") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_BACK_REF (separate)", GL_STENCIL_BACK_REF, GL_BACK)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilRefSeparateTestCase(
                          m_context, verifier,
                          (std::string("stencil_back_ref_separate_both") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_BACK_REF (separate)", GL_STENCIL_BACK_REF, GL_FRONT_AND_BACK)));

    const struct NamedStencilOp
    {
        const char *name;

        const char *frontDescription;
        GLenum frontTarget;
        const char *backDescription;
        GLenum backTarget;
    } stencilOps[] = {{"fail", "STENCIL_FAIL", GL_STENCIL_FAIL, "STENCIL_BACK_FAIL", GL_STENCIL_BACK_FAIL},
                      {"depth_fail", "STENCIL_PASS_DEPTH_FAIL", GL_STENCIL_PASS_DEPTH_FAIL,
                       "STENCIL_BACK_PASS_DEPTH_FAIL", GL_STENCIL_BACK_PASS_DEPTH_FAIL},
                      {"depth_pass", "STENCIL_PASS_DEPTH_PASS", GL_STENCIL_PASS_DEPTH_PASS,
                       "STENCIL_BACK_PASS_DEPTH_PASS", GL_STENCIL_BACK_PASS_DEPTH_PASS}};

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(stencilOps); testNdx++)
    {
        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new StencilOpTestCase(
                m_context, verifier,
                (std::string("stencil_") + stencilOps[testNdx].name + verifier->getTestNamePostfix()).c_str(),
                stencilOps[testNdx].frontDescription, stencilOps[testNdx].frontTarget)));
        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new StencilOpTestCase(
                m_context, verifier,
                (std::string("stencil_back_") + stencilOps[testNdx].name + verifier->getTestNamePostfix()).c_str(),
                stencilOps[testNdx].backDescription, stencilOps[testNdx].backTarget)));

        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new StencilOpSeparateTestCase(
                m_context, verifier,
                (std::string("stencil_") + stencilOps[testNdx].name + "_separate_both" + verifier->getTestNamePostfix())
                    .c_str(),
                stencilOps[testNdx].frontDescription, stencilOps[testNdx].frontTarget, GL_FRONT_AND_BACK)));
        FOR_EACH_VERIFIER(normalVerifiers,
                          addChild(new StencilOpSeparateTestCase(
                              m_context, verifier,
                              (std::string("stencil_back_") + stencilOps[testNdx].name + "_separate_both" +
                               verifier->getTestNamePostfix())
                                  .c_str(),
                              stencilOps[testNdx].backDescription, stencilOps[testNdx].backTarget, GL_FRONT_AND_BACK)));

        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new StencilOpSeparateTestCase(
                m_context, verifier,
                (std::string("stencil_") + stencilOps[testNdx].name + "_separate" + verifier->getTestNamePostfix())
                    .c_str(),
                stencilOps[testNdx].frontDescription, stencilOps[testNdx].frontTarget, GL_FRONT)));
        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new StencilOpSeparateTestCase(
                m_context, verifier,
                (std::string("stencil_back_") + stencilOps[testNdx].name + "_separate" + verifier->getTestNamePostfix())
                    .c_str(),
                stencilOps[testNdx].backDescription, stencilOps[testNdx].backTarget, GL_BACK)));
    }

    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilFuncTestCase(
                          m_context, verifier, (std::string("stencil_func") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_FUNC")));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilFuncSeparateTestCase(
            m_context, verifier, (std::string("stencil_func_separate") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_FUNC (separate)", GL_STENCIL_FUNC, GL_FRONT)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilFuncSeparateTestCase(
            m_context, verifier, (std::string("stencil_func_separate_both") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_FUNC (separate)", GL_STENCIL_FUNC, GL_FRONT_AND_BACK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilFuncSeparateTestCase(
            m_context, verifier, (std::string("stencil_back_func_separate") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_FUNC (separate)", GL_STENCIL_BACK_FUNC, GL_BACK)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilFuncSeparateTestCase(
                          m_context, verifier,
                          (std::string("stencil_back_func_separate_both") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_FUNC (separate)", GL_STENCIL_BACK_FUNC, GL_FRONT_AND_BACK)));
    FOR_EACH_VERIFIER(normalVerifiers, addChild(new StencilMaskTestCase(
                                           m_context, verifier,
                                           (std::string("stencil_value_mask") + verifier->getTestNamePostfix()).c_str(),
                                           "STENCIL_VALUE_MASK", GL_STENCIL_VALUE_MASK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilMaskTestCase(
            m_context, verifier, (std::string("stencil_back_value_mask") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_BACK_VALUE_MASK", GL_STENCIL_BACK_VALUE_MASK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilMaskSeparateTestCase(
            m_context, verifier, (std::string("stencil_value_mask_separate") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_VALUE_MASK (separate)", GL_STENCIL_VALUE_MASK, GL_FRONT)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilMaskSeparateTestCase(
                          m_context, verifier,
                          (std::string("stencil_value_mask_separate_both") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_VALUE_MASK (separate)", GL_STENCIL_VALUE_MASK, GL_FRONT_AND_BACK)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilMaskSeparateTestCase(
                          m_context, verifier,
                          (std::string("stencil_back_value_mask_separate") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_BACK_VALUE_MASK (separate)", GL_STENCIL_BACK_VALUE_MASK, GL_BACK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilMaskSeparateTestCase(
            m_context, verifier,
            (std::string("stencil_back_value_mask_separate_both") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_BACK_VALUE_MASK (separate)", GL_STENCIL_BACK_VALUE_MASK, GL_FRONT_AND_BACK)));
    FOR_EACH_VERIFIER(normalVerifiers, addChild(new StencilWriteMaskTestCase(
                                           m_context, verifier,
                                           (std::string("stencil_writemask") + verifier->getTestNamePostfix()).c_str(),
                                           "STENCIL_WRITEMASK", GL_STENCIL_WRITEMASK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilWriteMaskTestCase(
            m_context, verifier, (std::string("stencil_back_writemask") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_BACK_WRITEMASK", GL_STENCIL_BACK_WRITEMASK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilWriteMaskSeparateTestCase(
            m_context, verifier, (std::string("stencil_writemask_separate") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_WRITEMASK (separate)", GL_STENCIL_WRITEMASK, GL_FRONT)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilWriteMaskSeparateTestCase(
                          m_context, verifier,
                          (std::string("stencil_writemask_separate_both") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_WRITEMASK (separate)", GL_STENCIL_WRITEMASK, GL_FRONT_AND_BACK)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new StencilWriteMaskSeparateTestCase(
                          m_context, verifier,
                          (std::string("stencil_back_writemask_separate") + verifier->getTestNamePostfix()).c_str(),
                          "STENCIL_BACK_WRITEMASK (separate)", GL_STENCIL_BACK_WRITEMASK, GL_BACK)));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilWriteMaskSeparateTestCase(
            m_context, verifier,
            (std::string("stencil_back_writemask_separate_both") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_BACK_WRITEMASK (separate)", GL_STENCIL_BACK_WRITEMASK, GL_FRONT_AND_BACK)));

    const struct PixelStoreState
    {
        const char *name;
        const char *description;
        GLenum target;
        int initialValue;
    } pixelStoreStates[] = {{"unpack_image_height", "UNPACK_IMAGE_HEIGHT", GL_UNPACK_IMAGE_HEIGHT, 0},
                            {"unpack_skip_images", "UNPACK_SKIP_IMAGES", GL_UNPACK_SKIP_IMAGES, 0},
                            {"unpack_row_length", "UNPACK_ROW_LENGTH", GL_UNPACK_ROW_LENGTH, 0},
                            {"unpack_skip_rows", "UNPACK_SKIP_ROWS", GL_UNPACK_SKIP_ROWS, 0},
                            {"unpack_skip_pixels", "UNPACK_SKIP_PIXELS", GL_UNPACK_SKIP_PIXELS, 0},
                            {"pack_row_length", "PACK_ROW_LENGTH", GL_PACK_ROW_LENGTH, 0},
                            {"pack_skip_rows", "PACK_SKIP_ROWS", GL_PACK_SKIP_ROWS, 0},
                            {"pack_skip_pixels", "PACK_SKIP_PIXELS", GL_PACK_SKIP_PIXELS, 0}};
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(pixelStoreStates); testNdx++)
    {
        FOR_EACH_VERIFIER(normalVerifiers,
                          addChild(new PixelStoreTestCase(
                              m_context, verifier,
                              (std::string(pixelStoreStates[testNdx].name) + verifier->getTestNamePostfix()).c_str(),
                              pixelStoreStates[testNdx].description, pixelStoreStates[testNdx].target,
                              pixelStoreStates[testNdx].initialValue)));
    }

    FOR_EACH_VERIFIER(normalVerifiers, addChild(new PixelStoreAlignTestCase(
                                           m_context, verifier,
                                           (std::string("unpack_alignment") + verifier->getTestNamePostfix()).c_str(),
                                           "UNPACK_ALIGNMENT", GL_UNPACK_ALIGNMENT)));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new PixelStoreAlignTestCase(
                          m_context, verifier, (std::string("pack_alignment") + verifier->getTestNamePostfix()).c_str(),
                          "PACK_ALIGNMENT", GL_PACK_ALIGNMENT)));

    const struct BlendColorState
    {
        const char *name;
        const char *description;
        GLenum target;
        int initialValue;
    } blendColorStates[] = {{"blend_src_rgb", "BLEND_SRC_RGB", GL_BLEND_SRC_RGB, GL_ONE},
                            {"blend_src_alpha", "BLEND_SRC_ALPHA", GL_BLEND_SRC_ALPHA, GL_ONE},
                            {"blend_dst_rgb", "BLEND_DST_RGB", GL_BLEND_DST_RGB, GL_ZERO},
                            {"blend_dst_alpha", "BLEND_DST_ALPHA", GL_BLEND_DST_ALPHA, GL_ZERO}};
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(blendColorStates); testNdx++)
    {
        FOR_EACH_VERIFIER(normalVerifiers,
                          addChild(new BlendFuncTestCase(
                              m_context, verifier,
                              (std::string(blendColorStates[testNdx].name) + verifier->getTestNamePostfix()).c_str(),
                              blendColorStates[testNdx].description, blendColorStates[testNdx].target,
                              blendColorStates[testNdx].initialValue)));
        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new BlendFuncSeparateTestCase(
                m_context, verifier,
                (std::string(blendColorStates[testNdx].name) + "_separate" + verifier->getTestNamePostfix()).c_str(),
                blendColorStates[testNdx].description, blendColorStates[testNdx].target,
                blendColorStates[testNdx].initialValue)));
    }

    const struct BlendEquationState
    {
        const char *name;
        const char *description;
        GLenum target;
        int initialValue;
    } blendEquationStates[] = {{"blend_equation_rgb", "BLEND_EQUATION_RGB", GL_BLEND_EQUATION_RGB, GL_FUNC_ADD},
                               {"blend_equation_alpha", "BLEND_EQUATION_ALPHA", GL_BLEND_EQUATION_ALPHA, GL_FUNC_ADD}};
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(blendEquationStates); testNdx++)
    {
        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new BlendEquationTestCase(
                m_context, verifier,
                (std::string(blendEquationStates[testNdx].name) + +verifier->getTestNamePostfix()).c_str(),
                blendEquationStates[testNdx].description, blendEquationStates[testNdx].target,
                blendEquationStates[testNdx].initialValue)));
        FOR_EACH_VERIFIER(
            normalVerifiers,
            addChild(new BlendEquationSeparateTestCase(
                m_context, verifier,
                (std::string(blendEquationStates[testNdx].name) + "_separate" + verifier->getTestNamePostfix()).c_str(),
                blendEquationStates[testNdx].description, blendEquationStates[testNdx].target,
                blendEquationStates[testNdx].initialValue)));
    }

    const struct ImplementationArrayReturningState
    {
        const char *name;
        const char *description;
        GLenum target;
        GLenum targetLengthTarget;
        int minLength;
    } implementationArrayReturningStates[] = {
        {"compressed_texture_formats", "COMPRESSED_TEXTURE_FORMATS", GL_COMPRESSED_TEXTURE_FORMATS,
         GL_NUM_COMPRESSED_TEXTURE_FORMATS, 10},
        {"program_binary_formats", "PROGRAM_BINARY_FORMATS", GL_PROGRAM_BINARY_FORMATS, GL_NUM_PROGRAM_BINARY_FORMATS,
         0},
        {"shader_binary_formats", "SHADER_BINARY_FORMATS", GL_SHADER_BINARY_FORMATS, GL_NUM_SHADER_BINARY_FORMATS, 0}};
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(implementationArrayReturningStates); testNdx++)
    {
        FOR_EACH_VERIFIER(normalVerifiers, addChild(new ImplementationArrayTestCase(
                                               m_context, verifier,
                                               (std::string(implementationArrayReturningStates[testNdx].name) +
                                                verifier->getTestNamePostfix())
                                                   .c_str(),
                                               implementationArrayReturningStates[testNdx].description,
                                               implementationArrayReturningStates[testNdx].target,
                                               implementationArrayReturningStates[testNdx].targetLengthTarget,
                                               implementationArrayReturningStates[testNdx].minLength)));
    }

    const struct BufferBindingState
    {
        const char *name;
        const char *description;
        GLenum target;
        GLenum type;
    } bufferBindingStates[] = {
        {"array_buffer_binding", "ARRAY_BUFFER_BINDING", GL_ARRAY_BUFFER_BINDING, GL_ARRAY_BUFFER},
        {"uniform_buffer_binding", "UNIFORM_BUFFER_BINDING", GL_UNIFORM_BUFFER_BINDING, GL_UNIFORM_BUFFER},
        {"pixel_pack_buffer_binding", "PIXEL_PACK_BUFFER_BINDING", GL_PIXEL_PACK_BUFFER_BINDING, GL_PIXEL_PACK_BUFFER},
        {"pixel_unpack_buffer_binding", "PIXEL_UNPACK_BUFFER_BINDING", GL_PIXEL_UNPACK_BUFFER_BINDING,
         GL_PIXEL_UNPACK_BUFFER},
        {"transform_feedback_buffer_binding", "TRANSFORM_FEEDBACK_BUFFER_BINDING", GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
         GL_TRANSFORM_FEEDBACK_BUFFER},
        {"copy_read_buffer_binding", "COPY_READ_BUFFER_BINDING", GL_COPY_READ_BUFFER_BINDING, GL_COPY_READ_BUFFER},
        {"copy_write_buffer_binding", "COPY_WRITE_BUFFER_BINDING", GL_COPY_WRITE_BUFFER_BINDING, GL_COPY_WRITE_BUFFER}};
    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(bufferBindingStates); testNdx++)
    {
        FOR_EACH_QUERYTYPE(queryTypes,
                           addChild(new BufferBindingTestCase(
                               m_context, queryType,
                               (std::string(bufferBindingStates[testNdx].name) + getQueryTypeSuffix(queryType)).c_str(),
                               bufferBindingStates[testNdx].description, bufferBindingStates[testNdx].target,
                               bufferBindingStates[testNdx].type)));
    }

    FOR_EACH_QUERYTYPE(queryTypes,
                       addChild(new ElementArrayBufferBindingTestCase(
                           m_context, queryType,
                           (std::string("element_array_buffer_binding") + getQueryTypeSuffix(queryType)).c_str())));
    FOR_EACH_QUERYTYPE(queryTypes,
                       addChild(new TransformFeedbackBindingTestCase(
                           m_context, queryType,
                           (std::string("transform_feedback_binding") + getQueryTypeSuffix(queryType)).c_str())));
    FOR_EACH_QUERYTYPE(queryTypes, addChild(new CurrentProgramBindingTestCase(
                                       m_context, queryType,
                                       (std::string("current_program_binding") + getQueryTypeSuffix(queryType)).c_str(),
                                       "CURRENT_PROGRAM")));
    FOR_EACH_QUERYTYPE(queryTypes, addChild(new VertexArrayBindingTestCase(
                                       m_context, queryType,
                                       (std::string("vertex_array_binding") + getQueryTypeSuffix(queryType)).c_str(),
                                       "VERTEX_ARRAY_BINDING")));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new StencilClearValueTestCase(
            m_context, verifier, (std::string("stencil_clear_value") + verifier->getTestNamePostfix()).c_str(),
            "STENCIL_CLEAR_VALUE")));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new ActiveTextureTestCase(
                          m_context, verifier, (std::string("active_texture") + verifier->getTestNamePostfix()).c_str(),
                          "ACTIVE_TEXTURE")));
    FOR_EACH_QUERYTYPE(queryTypes, addChild(new RenderbufferBindingTestCase(
                                       m_context, queryType,
                                       (std::string("renderbuffer_binding") + getQueryTypeSuffix(queryType)).c_str(),
                                       "RENDERBUFFER_BINDING")));
    FOR_EACH_QUERYTYPE(
        queryTypes, addChild(new SamplerObjectBindingTestCase(
                        m_context, queryType, (std::string("sampler_binding") + getQueryTypeSuffix(queryType)).c_str(),
                        "SAMPLER_BINDING")));

    const struct TextureBinding
    {
        const char *name;
        const char *description;
        GLenum target;
        GLenum type;
    } textureBindings[] = {
        {"texture_binding_2d", "TEXTURE_BINDING_2D", GL_TEXTURE_BINDING_2D, GL_TEXTURE_2D},
        {"texture_binding_3d", "TEXTURE_BINDING_3D", GL_TEXTURE_BINDING_3D, GL_TEXTURE_3D},
        {"texture_binding_2d_array", "TEXTURE_BINDING_2D_ARRAY", GL_TEXTURE_BINDING_2D_ARRAY, GL_TEXTURE_2D_ARRAY},
        {"texture_binding_cube_map", "TEXTURE_BINDING_CUBE_MAP", GL_TEXTURE_BINDING_CUBE_MAP, GL_TEXTURE_CUBE_MAP}};

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(textureBindings); testNdx++)
    {
        FOR_EACH_QUERYTYPE(
            queryTypes,
            addChild(new TextureBindingTestCase(
                m_context, queryType,
                (std::string(textureBindings[testNdx].name) + getQueryTypeSuffix(queryType)).c_str(),
                textureBindings[testNdx].description, textureBindings[testNdx].target, textureBindings[testNdx].type)));
    }

    FOR_EACH_QUERYTYPE(queryTypes, addChild(new FrameBufferBindingTestCase(
                                       m_context, queryType,
                                       (std::string("framebuffer_binding") + getQueryTypeSuffix(queryType)).c_str(),
                                       "DRAW_FRAMEBUFFER_BINDING and READ_FRAMEBUFFER_BINDING")));
    FOR_EACH_VERIFIER(
        normalVerifiers,
        addChild(new ImplementationColorReadTestCase(
            m_context, verifier, (std::string("implementation_color_read") + verifier->getTestNamePostfix()).c_str(),
            "IMPLEMENTATION_COLOR_READ_TYPE and IMPLEMENTATION_COLOR_READ_FORMAT")));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new ReadBufferCase(m_context, verifier,
                                                  (std::string("read_buffer") + verifier->getTestNamePostfix()).c_str(),
                                                  "READ_BUFFER")));
    FOR_EACH_VERIFIER(normalVerifiers,
                      addChild(new DrawBufferCase(m_context, verifier,
                                                  (std::string("draw_buffer") + verifier->getTestNamePostfix()).c_str(),
                                                  "DRAW_BUFFER")));
}

void IntegerStateQueryTests::deinit(void)
{
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
