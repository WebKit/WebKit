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
 * \brief Integer64 State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fInteger64StateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"
#include "glwEnums.hpp"

#include <limits>

using namespace glw; // GLint and other
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace Integer64StateQueryVerifiers
{

// StateVerifier

class StateVerifier : protected glu::CallLogWrapper
{
public:
    StateVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix);
    virtual ~StateVerifier(); // make GCC happy

    const char *getTestNamePostfix(void) const;

    virtual void verifyUnsignedInteger64GreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint64 reference) = 0;

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
    void verifyUnsignedInteger64GreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint64 reference);
};

GetBooleanVerifier::GetBooleanVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getboolean")
{
}

void GetBooleanVerifier::verifyUnsignedInteger64GreaterOrEqual(tcu::TestContext &testCtx, GLenum name,
                                                               GLuint64 reference)
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
    void verifyUnsignedInteger64GreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint64 reference);
};

GetIntegerVerifier::GetIntegerVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : StateVerifier(gl, log, "_getinteger")
{
}

void GetIntegerVerifier::verifyUnsignedInteger64GreaterOrEqual(tcu::TestContext &testCtx, GLenum name,
                                                               GLuint64 reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetIntegerv(name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    // check that the converted value would be in the correct range, otherwise checking wont tell us anything
    if (reference > (GLuint64)std::numeric_limits<GLint>::max())
        return;

    if (GLuint(state) < reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected greater or equal to " << reference << "; got "
                         << state << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid integer value");
    }
}

//GetFloatVerifier

class GetFloatVerifier : public StateVerifier
{
public:
    GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log);
    void verifyUnsignedInteger64GreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint64 reference);
};

GetFloatVerifier::GetFloatVerifier(const glw::Functions &gl, tcu::TestLog &log) : StateVerifier(gl, log, "_getfloat")
{
}

void GetFloatVerifier::verifyUnsignedInteger64GreaterOrEqual(tcu::TestContext &testCtx, GLenum name, GLuint64 reference)
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

} // namespace Integer64StateQueryVerifiers

namespace
{

using namespace Integer64StateQueryVerifiers;

class ConstantMinimumValue64TestCase : public ApiCase
{
public:
    ConstantMinimumValue64TestCase(Context &context, StateVerifier *verifier, const char *name, const char *description,
                                   GLenum targetName, GLuint64 minValue)
        : ApiCase(context, name, description)
        , m_targetName(targetName)
        , m_minValue(minValue)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        m_verifier->verifyUnsignedInteger64GreaterOrEqual(m_testCtx, m_targetName, m_minValue);
        expectError(GL_NO_ERROR);
    }

private:
    GLenum m_targetName;
    GLuint64 m_minValue;
    StateVerifier *m_verifier;
};

class MaxCombinedStageUniformComponentsCase : public ApiCase
{
public:
    MaxCombinedStageUniformComponentsCase(Context &context, StateVerifier *verifier, const char *name,
                                          const char *description, GLenum targetName, GLenum targetMaxUniformBlocksName,
                                          GLenum targetMaxUniformComponentsName)
        : ApiCase(context, name, description)
        , m_targetName(targetName)
        , m_targetMaxUniformBlocksName(targetMaxUniformBlocksName)
        , m_targetMaxUniformComponentsName(targetMaxUniformComponentsName)
        , m_verifier(verifier)
    {
    }

    void test(void)
    {
        GLint uniformBlockSize = 0;
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &uniformBlockSize);
        expectError(GL_NO_ERROR);

        GLint maxUniformBlocks     = 0;
        GLint maxUniformComponents = 0;
        glGetIntegerv(m_targetMaxUniformBlocksName, &maxUniformBlocks);
        glGetIntegerv(m_targetMaxUniformComponentsName, &maxUniformComponents);
        expectError(GL_NO_ERROR);

        // MAX_stage_UNIFORM_BLOCKS * MAX_UNIFORM_BLOCK_SIZE / 4 + MAX_stage_UNIFORM_COMPONENTS
        const GLuint64 minCombinedUniformComponents =
            GLuint64(maxUniformBlocks) * uniformBlockSize / 4 + maxUniformComponents;

        m_verifier->verifyUnsignedInteger64GreaterOrEqual(m_testCtx, m_targetName, minCombinedUniformComponents);
        expectError(GL_NO_ERROR);
    }

private:
    GLenum m_targetName;
    GLenum m_targetMaxUniformBlocksName;
    GLenum m_targetMaxUniformComponentsName;
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

Integer64StateQueryTests::Integer64StateQueryTests(Context &context)
    : TestCaseGroup(context, "integers64", "Integer (64) Values")
    , m_verifierBoolean(nullptr)
    , m_verifierInteger(nullptr)
    , m_verifierFloat(nullptr)
{
}

Integer64StateQueryTests::~Integer64StateQueryTests(void)
{
    deinit();
}

void Integer64StateQueryTests::init(void)
{
    DE_ASSERT(m_verifierBoolean == nullptr);
    DE_ASSERT(m_verifierInteger == nullptr);
    DE_ASSERT(m_verifierFloat == nullptr);

    m_verifierBoolean =
        new GetBooleanVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierInteger =
        new GetIntegerVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierFloat =
        new GetFloatVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());

    const struct LimitedStateInteger64
    {
        const char *name;
        const char *description;
        GLenum targetName;
        GLuint64 minValue;
    } implementationLimits[] = {{"max_element_index", "MAX_ELEMENT_INDEX", GL_MAX_ELEMENT_INDEX, 0x00FFFFFF /*2^24-1*/},
                                {"max_server_wait_timeout", "MAX_SERVER_WAIT_TIMEOUT", GL_MAX_SERVER_WAIT_TIMEOUT, 0},
                                {"max_uniform_block_size", "MAX_UNIFORM_BLOCK_SIZE", GL_MAX_UNIFORM_BLOCK_SIZE, 16384}};

    // \note do not check the values with integer64 verifier as that has already been checked in implementation_limits
    StateVerifier *verifiers[] = {m_verifierBoolean, m_verifierInteger, m_verifierFloat};

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(implementationLimits); testNdx++)
        FOR_EACH_VERIFIER(
            verifiers, addChild(new ConstantMinimumValue64TestCase(
                           m_context, verifier,
                           (std::string(implementationLimits[testNdx].name) + verifier->getTestNamePostfix()).c_str(),
                           implementationLimits[testNdx].description, implementationLimits[testNdx].targetName,
                           implementationLimits[testNdx].minValue)));

    FOR_EACH_VERIFIER(
        verifiers, addChild(new MaxCombinedStageUniformComponentsCase(
                       m_context, verifier,
                       (std::string("max_combined_vertex_uniform_components") + verifier->getTestNamePostfix()).c_str(),
                       "MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS", GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
                       GL_MAX_VERTEX_UNIFORM_BLOCKS, GL_MAX_VERTEX_UNIFORM_COMPONENTS)));
    FOR_EACH_VERIFIER(
        verifiers,
        addChild(new MaxCombinedStageUniformComponentsCase(
            m_context, verifier,
            (std::string("max_combined_fragment_uniform_components") + verifier->getTestNamePostfix()).c_str(),
            "MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS", GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
            GL_MAX_FRAGMENT_UNIFORM_BLOCKS, GL_MAX_FRAGMENT_UNIFORM_COMPONENTS)));
}

void Integer64StateQueryTests::deinit(void)
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
