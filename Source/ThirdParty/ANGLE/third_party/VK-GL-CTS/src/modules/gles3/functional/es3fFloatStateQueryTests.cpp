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
 * \brief Float State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fFloatStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuFormatUtil.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "glwEnums.hpp"

#include <limits>

using namespace glw; // GLint and other GL types
using namespace deqp::gls;
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace FloatStateQueryVerifiers
{
namespace
{

const int FLOAT_EXPANSION_E    = 0x03FF; // 10 bits error allowed, requires 22 accurate bits
const int FLOAT_EXPANSION_E_64 = 0x07FF;

GLint64 expandGLFloatToInteger(GLfloat f)
{
    const GLuint64 referenceValue = (GLint64)(f * 2147483647.0);
    return referenceValue;
}

GLint clampToGLint(GLint64 val)
{
    return (GLint)de::clamp<GLint64>(val, std::numeric_limits<GLint>::min(), std::numeric_limits<GLint>::max());
}

} // namespace

// StateVerifier

class StateVerifier : protected glu::CallLogWrapper
{
public:
    StateVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix);
    virtual ~StateVerifier(); // make GCC happy

    const char *getTestNamePostfix(void) const;

    virtual void verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference) = 0;

    // "Expanded" == Float to int conversion converts from [-1.0 to 1.0] -> [MIN_INT MAX_INT]
    virtual void verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference) = 0;
    virtual void verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                      GLfloat reference1)                                       = 0;
    virtual void verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1,
                                   GLfloat reference2, GLfloat reference3)                      = 0;

    // verify that the given range is completely whitin the GL state range
    virtual void verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max)   = 0;
    virtual void verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference) = 0;

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
    void verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1);
    void verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1,
                           GLfloat reference2, GLfloat reference3);
    void verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max);
    void verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
};

GetBooleanVerifier::GetBooleanVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getboolean")
{
}

void GetBooleanVerifier::verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean> state;
    glGetBooleanv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLboolean expectedGLState = reference != 0.0f ? GL_TRUE : GL_FALSE;

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

void GetBooleanVerifier::verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    DE_ASSERT(de::inRange(reference, -1.0f, 1.0f));
    verifyFloat(testCtx, name, reference);
}

void GetBooleanVerifier::verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                              GLfloat reference1)
{
    DE_ASSERT(de::inRange(reference0, -1.0f, 1.0f));
    DE_ASSERT(de::inRange(reference1, -1.0f, 1.0f));

    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean[2]> boolVector2;
    glGetBooleanv(name, boolVector2);

    if (!boolVector2.verifyValidity(testCtx))
        return;

    const GLboolean referenceAsGLBoolean[] = {
        reference0 != 0.0f ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
        reference1 != 0.0f ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
    };

    if (boolVector2[0] != referenceAsGLBoolean[0] || boolVector2[1] != referenceAsGLBoolean[1])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << (boolVector2[0] ? "GL_TRUE" : "GL_FALSE")
                         << " " << (boolVector2[1] ? "GL_TRUE" : "GL_FALSE") << " " << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void GetBooleanVerifier::verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                           GLfloat reference1, GLfloat reference2, GLfloat reference3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean[4]> boolVector4;
    glGetBooleanv(name, boolVector4);

    if (!boolVector4.verifyValidity(testCtx))
        return;

    const GLboolean referenceAsGLBoolean[] = {
        reference0 != 0.0f ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
        reference1 != 0.0f ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
        reference2 != 0.0f ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
        reference3 != 0.0f ? GLboolean(GL_TRUE) : GLboolean(GL_FALSE),
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

void GetBooleanVerifier::verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLboolean[2]> range;
    glGetBooleanv(name, range);

    if (!range.verifyValidity(testCtx))
        return;

    if (range[0] == GL_FALSE)
    {
        if (max < 0 || min < 0)
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: range [" << min << ", " << max << "] is not in range ["
                             << (range[0] == GL_TRUE ? "GL_TRUE" : (range[0] == GL_FALSE ? "GL_FALSE" : "non-boolean"))
                             << ", "
                             << (range[1] == GL_TRUE ? "GL_TRUE" : (range[1] == GL_FALSE ? "GL_FALSE" : "non-boolean"))
                             << "]" << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean range");
            return;
        }
    }
    if (range[1] == GL_FALSE)
    {
        if (max > 0 || min > 0)
        {
            testCtx.getLog() << TestLog::Message << "// ERROR: range [" << min << ", " << max << "] is not in range ["
                             << (range[0] == GL_TRUE ? "GL_TRUE" : (range[0] == GL_FALSE ? "GL_FALSE" : "non-boolean"))
                             << ", "
                             << (range[1] == GL_TRUE ? "GL_TRUE" : (range[1] == GL_FALSE ? "GL_FALSE" : "non-boolean"))
                             << "]" << TestLog::EndMessage;
            if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean range");
            return;
        }
    }
}

void GetBooleanVerifier::verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
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

//GetIntegerVerifier

class GetIntegerVerifier : public StateVerifier
{
public:
    GetIntegerVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1);
    void verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1,
                           GLfloat reference2, GLfloat reference3);
    void verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max);
    void verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
};

GetIntegerVerifier::GetIntegerVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getinteger")
{
}

void GetIntegerVerifier::verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    const GLint expectedGLStateMax = StateQueryUtil::roundGLfloatToNearestIntegerHalfUp<GLint>(reference);
    const GLint expectedGLStateMin = StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint>(reference);

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < expectedGLStateMin || state > expectedGLStateMax)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected rounding to the nearest integer, valid range ["
                         << expectedGLStateMin << "," << expectedGLStateMax << "]; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    DE_ASSERT(de::inRange(reference, -1.0f, 1.0f));

    using tcu::TestLog;
    using tcu::toHex;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint expectedGLStateMax = clampToGLint(expandGLFloatToInteger(reference) + FLOAT_EXPANSION_E);
    const GLint expectedGLStateMin = clampToGLint(expandGLFloatToInteger(reference) - FLOAT_EXPANSION_E);

    if (state < expectedGLStateMin || state > expectedGLStateMax)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in range [" << toHex(expectedGLStateMin) << ","
                         << toHex(expectedGLStateMax) << "]; got " << toHex((GLint)state) << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                              GLfloat reference1)
{
    DE_ASSERT(de::inRange(reference0, -1.0f, 1.0f));
    DE_ASSERT(de::inRange(reference1, -1.0f, 1.0f));

    using tcu::TestLog;
    using tcu::toHex;

    StateQueryMemoryWriteGuard<GLint[2]> floatVector2;
    glGetIntegerv(name, floatVector2);

    if (!floatVector2.verifyValidity(testCtx))
        return;

    const GLint referenceAsGLintMin[] = {clampToGLint(expandGLFloatToInteger(reference0) - FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference1) - FLOAT_EXPANSION_E)};
    const GLint referenceAsGLintMax[] = {clampToGLint(expandGLFloatToInteger(reference0) + FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference1) + FLOAT_EXPANSION_E)};

    if (floatVector2[0] < referenceAsGLintMin[0] || floatVector2[0] > referenceAsGLintMax[0] ||
        floatVector2[1] < referenceAsGLintMin[1] || floatVector2[1] > referenceAsGLintMax[1])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in ranges "
                         << "[" << toHex(referenceAsGLintMin[0]) << " " << toHex(referenceAsGLintMax[0]) << "], "
                         << "[" << toHex(referenceAsGLintMin[1]) << " " << toHex(referenceAsGLintMax[1]) << "]"
                         << "; got " << toHex(floatVector2[0]) << ", " << toHex(floatVector2[1]) << " "
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                           GLfloat reference1, GLfloat reference2, GLfloat reference3)
{
    using tcu::TestLog;
    using tcu::toHex;

    StateQueryMemoryWriteGuard<GLint[4]> floatVector4;
    glGetIntegerv(name, floatVector4);

    if (!floatVector4.verifyValidity(testCtx))
        return;

    const GLint referenceAsGLintMin[] = {clampToGLint(expandGLFloatToInteger(reference0) - FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference1) - FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference2) - FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference3) - FLOAT_EXPANSION_E)};
    const GLint referenceAsGLintMax[] = {clampToGLint(expandGLFloatToInteger(reference0) + FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference1) + FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference2) + FLOAT_EXPANSION_E),
                                         clampToGLint(expandGLFloatToInteger(reference3) + FLOAT_EXPANSION_E)};

    if (floatVector4[0] < referenceAsGLintMin[0] || floatVector4[0] > referenceAsGLintMax[0] ||
        floatVector4[1] < referenceAsGLintMin[1] || floatVector4[1] > referenceAsGLintMax[1] ||
        floatVector4[2] < referenceAsGLintMin[2] || floatVector4[2] > referenceAsGLintMax[2] ||
        floatVector4[3] < referenceAsGLintMin[3] || floatVector4[3] > referenceAsGLintMax[3])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in ranges "
                         << "[" << toHex(referenceAsGLintMin[0]) << " " << toHex(referenceAsGLintMax[0]) << "], "
                         << "[" << toHex(referenceAsGLintMin[1]) << " " << toHex(referenceAsGLintMax[1]) << "], "
                         << "[" << toHex(referenceAsGLintMin[2]) << " " << toHex(referenceAsGLintMax[2]) << "], "
                         << "[" << toHex(referenceAsGLintMin[3]) << " " << toHex(referenceAsGLintMax[3]) << "]"
                         << "; got " << toHex(floatVector4[0]) << ", " << toHex(floatVector4[1]) << ", "
                         << toHex(floatVector4[2]) << ", " << toHex(floatVector4[3]) << " " << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetIntegerVerifier::verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint[2]> range;
    glGetIntegerv(name, range);

    if (!range.verifyValidity(testCtx))
        return;

    const GLint testRangeAsGLint[] = {StateQueryUtil::roundGLfloatToNearestIntegerHalfUp<GLint>(min),
                                      StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint>(max)};

    // check if test range outside of gl state range
    if (testRangeAsGLint[0] < range[0] || testRangeAsGLint[1] > range[1])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: range [" << testRangeAsGLint[0] << ", "
                         << testRangeAsGLint[1] << "]"
                         << " is not in range [" << range[0] << ", " << range[1] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer range");
    }
}

void GetIntegerVerifier::verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint referenceAsInt = StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint>(reference);

    if (state < referenceAsInt)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected expected greater or equal to " << referenceAsInt
                         << "; got " << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

//GetInteger64Verifier

class GetInteger64Verifier : public StateVerifier
{
public:
    GetInteger64Verifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1);
    void verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1,
                           GLfloat reference2, GLfloat reference3);
    void verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max);
    void verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
};

GetInteger64Verifier::GetInteger64Verifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getinteger64")
{
}

void GetInteger64Verifier::verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    const GLint64 expectedGLStateMax = StateQueryUtil::roundGLfloatToNearestIntegerHalfUp<GLint64>(reference);
    const GLint64 expectedGLStateMin = StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint64>(reference);

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < expectedGLStateMin || state > expectedGLStateMax)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected rounding to the nearest integer, valid range ["
                         << expectedGLStateMin << "," << expectedGLStateMax << "]; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    DE_ASSERT(de::inRange(reference, -1.0f, 1.0f));

    using tcu::TestLog;
    using tcu::toHex;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint64 expectedGLStateMax = expandGLFloatToInteger(reference) + FLOAT_EXPANSION_E_64;
    const GLint64 expectedGLStateMin = expandGLFloatToInteger(reference) - FLOAT_EXPANSION_E_64;

    if (state < expectedGLStateMin || state > expectedGLStateMax)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in range [" << toHex(expectedGLStateMin) << ","
                         << toHex(expectedGLStateMax) << "]; got " << toHex((GLint64)state) << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                                GLfloat reference1)
{
    DE_ASSERT(de::inRange(reference0, -1.0f, 1.0f));
    DE_ASSERT(de::inRange(reference1, -1.0f, 1.0f));

    using tcu::TestLog;
    using tcu::toHex;

    StateQueryMemoryWriteGuard<GLint64[2]> floatVector2;
    glGetInteger64v(name, floatVector2);

    if (!floatVector2.verifyValidity(testCtx))
        return;

    const GLint64 referenceAsGLintMin[] = {expandGLFloatToInteger(reference0) - FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference1) - FLOAT_EXPANSION_E_64};
    const GLint64 referenceAsGLintMax[] = {expandGLFloatToInteger(reference0) + FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference1) + FLOAT_EXPANSION_E_64};

    if (floatVector2[0] < referenceAsGLintMin[0] || floatVector2[0] > referenceAsGLintMax[0] ||
        floatVector2[1] < referenceAsGLintMin[1] || floatVector2[1] > referenceAsGLintMax[1])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in ranges "
                         << "[" << toHex(referenceAsGLintMin[0]) << " " << toHex(referenceAsGLintMax[0]) << "], "
                         << "[" << toHex(referenceAsGLintMin[1]) << " " << toHex(referenceAsGLintMax[1]) << "]"
                         << "; got " << toHex(floatVector2[0]) << ", " << toHex(floatVector2[1]) << " "
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                             GLfloat reference1, GLfloat reference2, GLfloat reference3)
{
    using tcu::TestLog;
    using tcu::toHex;

    StateQueryMemoryWriteGuard<GLint64[4]> floatVector4;
    glGetInteger64v(name, floatVector4);

    if (!floatVector4.verifyValidity(testCtx))
        return;

    const GLint64 referenceAsGLintMin[] = {expandGLFloatToInteger(reference0) - FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference1) - FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference2) - FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference3) - FLOAT_EXPANSION_E_64};
    const GLint64 referenceAsGLintMax[] = {expandGLFloatToInteger(reference0) + FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference1) + FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference2) + FLOAT_EXPANSION_E_64,
                                           expandGLFloatToInteger(reference3) + FLOAT_EXPANSION_E_64};

    if (floatVector4[0] < referenceAsGLintMin[0] || floatVector4[0] > referenceAsGLintMax[0] ||
        floatVector4[1] < referenceAsGLintMin[1] || floatVector4[1] > referenceAsGLintMax[1] ||
        floatVector4[2] < referenceAsGLintMin[2] || floatVector4[2] > referenceAsGLintMax[2] ||
        floatVector4[3] < referenceAsGLintMin[3] || floatVector4[3] > referenceAsGLintMax[3])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in ranges "
                         << "[" << toHex(referenceAsGLintMin[0]) << " " << toHex(referenceAsGLintMax[0]) << "], "
                         << "[" << toHex(referenceAsGLintMin[1]) << " " << toHex(referenceAsGLintMax[1]) << "], "
                         << "[" << toHex(referenceAsGLintMin[2]) << " " << toHex(referenceAsGLintMax[2]) << "], "
                         << "[" << toHex(referenceAsGLintMin[3]) << " " << toHex(referenceAsGLintMax[3]) << "]"
                         << "; got " << toHex(floatVector4[0]) << ", " << toHex(floatVector4[1]) << ", "
                         << toHex(floatVector4[2]) << ", " << toHex(floatVector4[3]) << " " << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

void GetInteger64Verifier::verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64[2]> range;
    glGetInteger64v(name, range);

    if (!range.verifyValidity(testCtx))
        return;

    const GLint64 testRangeAsGLint[] = {StateQueryUtil::roundGLfloatToNearestIntegerHalfUp<GLint64>(min),
                                        StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint64>(max)};

    // check if test range outside of gl state range
    if (testRangeAsGLint[0] < range[0] || testRangeAsGLint[1] > range[1])
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: range [" << testRangeAsGLint[0] << ", "
                         << testRangeAsGLint[1] << "]"
                         << " is not in range [" << range[0] << ", " << range[1] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer range");
    }
}

void GetInteger64Verifier::verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetInteger64v(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    const GLint64 referenceAsInt = StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint64>(reference);

    if (state < referenceAsInt)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected expected greater or equal to " << referenceAsInt
                         << "; got " << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

//GetFloatVerifier

class GetFloatVerifier : public StateVerifier
{
public:
    GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
    void verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1);
    void verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1,
                           GLfloat reference2, GLfloat reference3);
    void verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max);
    void verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference);
};

GetFloatVerifier::GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log) : StateVerifier(gl, log, "_getfloat")
{
}

void GetFloatVerifier::verifyFloat(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyFloatExpanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    DE_ASSERT(de::inRange(reference, -1.0f, 1.0f));
    verifyFloat(testCtx, name, reference);
}

void GetFloatVerifier::verifyFloat2Expanded(tcu::TestContext &testCtx, GLenum name, GLfloat reference0,
                                            GLfloat reference1)
{
    DE_ASSERT(de::inRange(reference0, -1.0f, 1.0f));
    DE_ASSERT(de::inRange(reference1, -1.0f, 1.0f));

    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[2]> floatVector2;
    glGetFloatv(name, floatVector2);

    if (!floatVector2.verifyValidity(testCtx))
        return;

    if (floatVector2[0] != reference0 || floatVector2[1] != reference1)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference0 << ", " << reference1 << "; got "
                         << floatVector2[0] << " " << floatVector2[1] << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyFloat4Color(tcu::TestContext &testCtx, GLenum name, GLfloat reference0, GLfloat reference1,
                                         GLfloat reference2, GLfloat reference3)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[4]> floatVector4;
    glGetFloatv(name, floatVector4);

    if (!floatVector4.verifyValidity(testCtx))
        return;

    if (floatVector4[0] != reference0 || floatVector4[1] != reference1 || floatVector4[2] != reference2 ||
        floatVector4[3] != reference3)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference0 << ", " << reference1 << ", "
                         << reference2 << ", " << reference3 << "; got " << floatVector4[0] << ", " << floatVector4[1]
                         << ", " << floatVector4[2] << ", " << floatVector4[3] << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetFloatVerifier::verifyFloatRange(tcu::TestContext &testCtx, GLenum name, GLfloat min, GLfloat max)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat[2]> floatVector2;
    glGetFloatv(name, floatVector2);

    if (!floatVector2.verifyValidity(testCtx))
        return;

    if (floatVector2[0] > min || floatVector2[1] < max)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in range [" << min << ", " << max << "]; got ["
                         << floatVector2[0] << " " << floatVector2[1] << "]" << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float range");
    }
}

void GetFloatVerifier::verifyFloatGreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetFloatv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << reference << "; got "
                         << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

} // namespace FloatStateQueryVerifiers

namespace
{

using namespace FloatStateQueryVerifiers;

class DepthRangeCase : public ApiCase
{
public:
    DepthRangeCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyFloat2Expanded(m_testCtx, GL_DEPTH_RANGE, 0.0f, 1.0f);
        expectError(GL_NO_ERROR);

        const struct FixedTest
        {
            float n, f;
        } fixedTests[] = {{0.5f, 1.0f}, {0.0f, 0.5f}, {0.0f, 0.0f}, {1.0f, 1.0f}};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(fixedTests); ++ndx)
        {
            glDepthRangef(fixedTests[ndx].n, fixedTests[ndx].f);

            m_verifier->verifyFloat2Expanded(m_testCtx, GL_DEPTH_RANGE, fixedTests[ndx].n, fixedTests[ndx].f);
            expectError(GL_NO_ERROR);
        }

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            GLfloat n = rnd.getFloat(0, 1);
            GLfloat f = rnd.getFloat(0, 1);

            glDepthRangef(n, f);
            m_verifier->verifyFloat2Expanded(m_testCtx, GL_DEPTH_RANGE, n, f);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class LineWidthCase : public ApiCase
{
public:
    LineWidthCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyFloat(m_testCtx, GL_LINE_WIDTH, 1.0f);
        expectError(GL_NO_ERROR);

        GLfloat range[2] = {1};
        glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
        expectError(GL_NO_ERROR);

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            const GLfloat reference = rnd.getFloat(range[0], range[1]);

            glLineWidth(reference);
            m_verifier->verifyFloat(m_testCtx, GL_LINE_WIDTH, reference);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class PolygonOffsetFactorCase : public ApiCase
{
public:
    PolygonOffsetFactorCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyFloat(m_testCtx, GL_POLYGON_OFFSET_FACTOR, 0.0f);
        expectError(GL_NO_ERROR);

        const float fixedTests[] = {0.0f, 0.5f, -0.5f, 1.5f};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(fixedTests); ++ndx)
        {
            glPolygonOffset(fixedTests[ndx], 0);
            m_verifier->verifyFloat(m_testCtx, GL_POLYGON_OFFSET_FACTOR, fixedTests[ndx]);
            expectError(GL_NO_ERROR);
        }

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            const GLfloat reference = rnd.getFloat(-64000, 64000);

            glPolygonOffset(reference, 0);
            m_verifier->verifyFloat(m_testCtx, GL_POLYGON_OFFSET_FACTOR, reference);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class PolygonOffsetUnitsCase : public ApiCase
{
public:
    PolygonOffsetUnitsCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyFloat(m_testCtx, GL_POLYGON_OFFSET_UNITS, 0.0f);
        expectError(GL_NO_ERROR);

        const float fixedTests[] = {0.0f, 0.5f, -0.5f, 1.5f};
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(fixedTests); ++ndx)
        {
            glPolygonOffset(0, fixedTests[ndx]);
            m_verifier->verifyFloat(m_testCtx, GL_POLYGON_OFFSET_UNITS, fixedTests[ndx]);
            expectError(GL_NO_ERROR);
        }

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            const GLfloat reference = rnd.getFloat(-64000, 64000);

            glPolygonOffset(0, reference);
            m_verifier->verifyFloat(m_testCtx, GL_POLYGON_OFFSET_UNITS, reference);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class SampleCoverageCase : public ApiCase
{
public:
    SampleCoverageCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyFloat(m_testCtx, GL_SAMPLE_COVERAGE_VALUE, 1.0f);
        expectError(GL_NO_ERROR);

        {
            const float fixedTests[] = {0.0f, 0.5f, 0.45f, 0.55f};
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(fixedTests); ++ndx)
            {
                glSampleCoverage(fixedTests[ndx], GL_FALSE);
                m_verifier->verifyFloat(m_testCtx, GL_SAMPLE_COVERAGE_VALUE, fixedTests[ndx]);
                expectError(GL_NO_ERROR);
            }
        }

        {
            const float clampTests[] = {-1.0f, -1.5f, 1.45f, 3.55f};
            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(clampTests); ++ndx)
            {
                glSampleCoverage(clampTests[ndx], GL_FALSE);
                m_verifier->verifyFloat(m_testCtx, GL_SAMPLE_COVERAGE_VALUE, de::clamp(clampTests[ndx], 0.0f, 1.0f));
                expectError(GL_NO_ERROR);
            }
        }

        {
            const int numIterations = 120;
            for (int i = 0; i < numIterations; ++i)
            {
                GLfloat reference = rnd.getFloat(0, 1);
                GLboolean invert  = rnd.getBool() ? GL_TRUE : GL_FALSE;

                glSampleCoverage(reference, invert);
                m_verifier->verifyFloat(m_testCtx, GL_SAMPLE_COVERAGE_VALUE, reference);
                expectError(GL_NO_ERROR);
            }
        }
    }

private:
    StateVerifier *m_verifier;
};

class BlendColorCase : public ApiCase
{
public:
    BlendColorCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyFloat4Color(m_testCtx, GL_BLEND_COLOR, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);

        const struct FixedTest
        {
            float r, g, b, a;
        } fixedTests[] = {
            {0.5f, 1.0f, 0.5f, 1.0f},
            {0.0f, 0.5f, 0.0f, 0.5f},
            {0.0f, 0.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
        };
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(fixedTests); ++ndx)
        {
            glBlendColor(fixedTests[ndx].r, fixedTests[ndx].g, fixedTests[ndx].b, fixedTests[ndx].a);
            m_verifier->verifyFloat4Color(m_testCtx, GL_BLEND_COLOR, fixedTests[ndx].r, fixedTests[ndx].g,
                                          fixedTests[ndx].b, fixedTests[ndx].a);
            expectError(GL_NO_ERROR);
        }

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            const GLfloat r = rnd.getFloat(0, 1);
            const GLfloat g = rnd.getFloat(0, 1);
            const GLfloat b = rnd.getFloat(0, 1);
            const GLfloat a = rnd.getFloat(0, 1);

            glBlendColor(r, g, b, a);
            m_verifier->verifyFloat4Color(m_testCtx, GL_BLEND_COLOR, r, g, b, a);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class ColorClearCase : public ApiCase
{
public:
    ColorClearCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        // \note Initial color clear value check is temorarily removed. (until the framework does not alter it)
        //m_verifier->verifyFloat4Color(m_testCtx, GL_COLOR_CLEAR_VALUE, 0, 0, 0, 0);
        //expectError(GL_NO_ERROR);

        const struct FixedTest
        {
            float r, g, b, a;
        } fixedTests[] = {
            {0.5f, 1.0f, 0.5f, 1.0f},
            {0.0f, 0.5f, 0.0f, 0.5f},
            {0.0f, 0.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
        };
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(fixedTests); ++ndx)
        {
            glClearColor(fixedTests[ndx].r, fixedTests[ndx].g, fixedTests[ndx].b, fixedTests[ndx].a);
            m_verifier->verifyFloat4Color(m_testCtx, GL_COLOR_CLEAR_VALUE, fixedTests[ndx].r, fixedTests[ndx].g,
                                          fixedTests[ndx].b, fixedTests[ndx].a);
            expectError(GL_NO_ERROR);
        }

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            const GLfloat r = rnd.getFloat(0, 1);
            const GLfloat g = rnd.getFloat(0, 1);
            const GLfloat b = rnd.getFloat(0, 1);
            const GLfloat a = rnd.getFloat(0, 1);

            glClearColor(r, g, b, a);
            m_verifier->verifyFloat4Color(m_testCtx, GL_COLOR_CLEAR_VALUE, r, g, b, a);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class DepthClearCase : public ApiCase
{
public:
    DepthClearCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyFloatExpanded(m_testCtx, GL_DEPTH_CLEAR_VALUE, 1);
        expectError(GL_NO_ERROR);

        const int numIterations = 120;
        for (int i = 0; i < numIterations; ++i)
        {
            const GLfloat ref = rnd.getFloat(0, 1);

            glClearDepthf(ref);
            m_verifier->verifyFloatExpanded(m_testCtx, GL_DEPTH_CLEAR_VALUE, ref);
            expectError(GL_NO_ERROR);
        }
    }

private:
    StateVerifier *m_verifier;
};

class MaxTextureLODBiasCase : public ApiCase
{
public:
    MaxTextureLODBiasCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyFloatGreaterOrEqual(m_testCtx, GL_MAX_TEXTURE_LOD_BIAS, 2.0f);
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class AliasedPointSizeRangeCase : public ApiCase
{
public:
    AliasedPointSizeRangeCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyFloatRange(m_testCtx, GL_ALIASED_POINT_SIZE_RANGE, 1, 1);
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
};

class AliasedLineWidthRangeCase : public ApiCase
{
public:
    AliasedLineWidthRangeCase(Context &context, StateVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyFloatRange(m_testCtx, GL_ALIASED_LINE_WIDTH_RANGE, 1, 1);
        expectError(GL_NO_ERROR);
    }

private:
    StateVerifier *m_verifier;
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

FloatStateQueryTests::FloatStateQueryTests(Context &context)
    : TestCaseGroup(context, "floats", "Float Values")
    , m_verifierBoolean(nullptr)
    , m_verifierInteger(nullptr)
    , m_verifierInteger64(nullptr)
    , m_verifierFloat(nullptr)
{
}

FloatStateQueryTests::~FloatStateQueryTests(void)
{
    deinit();
}

void FloatStateQueryTests::init(void)
{
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

    StateVerifier *verifiers[] = {m_verifierBoolean, m_verifierInteger, m_verifierInteger64, m_verifierFloat};

    FOR_EACH_VERIFIER(verifiers,
                      addChild(new DepthRangeCase(m_context, verifier,
                                                  (std::string("depth_range") + verifier->getTestNamePostfix()).c_str(),
                                                  "DEPTH_RANGE")));
    FOR_EACH_VERIFIER(verifiers,
                      addChild(new LineWidthCase(m_context, verifier,
                                                 (std::string("line_width") + verifier->getTestNamePostfix()).c_str(),
                                                 "LINE_WIDTH")));
    FOR_EACH_VERIFIER(verifiers, addChild(new PolygonOffsetFactorCase(
                                     m_context, verifier,
                                     (std::string("polygon_offset_factor") + verifier->getTestNamePostfix()).c_str(),
                                     "POLYGON_OFFSET_FACTOR")));
    FOR_EACH_VERIFIER(verifiers, addChild(new PolygonOffsetUnitsCase(
                                     m_context, verifier,
                                     (std::string("polygon_offset_units") + verifier->getTestNamePostfix()).c_str(),
                                     "POLYGON_OFFSET_UNITS")));
    FOR_EACH_VERIFIER(verifiers, addChild(new SampleCoverageCase(
                                     m_context, verifier,
                                     (std::string("sample_coverage_value") + verifier->getTestNamePostfix()).c_str(),
                                     "SAMPLE_COVERAGE_VALUE")));
    FOR_EACH_VERIFIER(verifiers,
                      addChild(new BlendColorCase(m_context, verifier,
                                                  (std::string("blend_color") + verifier->getTestNamePostfix()).c_str(),
                                                  "BLEND_COLOR")));
    FOR_EACH_VERIFIER(
        verifiers, addChild(new ColorClearCase(
                       m_context, verifier, (std::string("color_clear_value") + verifier->getTestNamePostfix()).c_str(),
                       "COLOR_CLEAR_VALUE")));
    FOR_EACH_VERIFIER(
        verifiers, addChild(new DepthClearCase(
                       m_context, verifier, (std::string("depth_clear_value") + verifier->getTestNamePostfix()).c_str(),
                       "DEPTH_CLEAR_VALUE")));
    FOR_EACH_VERIFIER(verifiers, addChild(new MaxTextureLODBiasCase(
                                     m_context, verifier,
                                     (std::string("max_texture_lod_bias") + verifier->getTestNamePostfix()).c_str(),
                                     "MAX_TEXTURE_LOD_BIAS")));
    FOR_EACH_VERIFIER(verifiers, addChild(new AliasedPointSizeRangeCase(
                                     m_context, verifier,
                                     (std::string("aliased_point_size_range") + verifier->getTestNamePostfix()).c_str(),
                                     "ALIASED_POINT_SIZE_RANGE")));
    FOR_EACH_VERIFIER(verifiers, addChild(new AliasedLineWidthRangeCase(
                                     m_context, verifier,
                                     (std::string("aliased_line_width_range") + verifier->getTestNamePostfix()).c_str(),
                                     "ALIASED_LINE_WIDTH_RANGE")));
}

void FloatStateQueryTests::deinit(void)
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
