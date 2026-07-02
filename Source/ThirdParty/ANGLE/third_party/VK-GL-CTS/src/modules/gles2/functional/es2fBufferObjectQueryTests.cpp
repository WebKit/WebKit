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
 * \brief Buffer Object Query tests.
 *//*--------------------------------------------------------------------*/

#include "es2fBufferObjectQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es2fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deRandom.hpp"
#include "deMath.h"

#include <limits>

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace BufferParamVerifiers
{

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

class BufferParamVerifier : protected glu::CallLogWrapper
{
public:
    BufferParamVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix);
    virtual ~BufferParamVerifier(); // make GCC happy

    const char *getTestNamePostfix(void) const;

    virtual void verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference) = 0;

private:
    const char *const m_testNamePostfix;
};

BufferParamVerifier::BufferParamVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix)
    : glu::CallLogWrapper(gl, log)
    , m_testNamePostfix(testNamePostfix)
{
    enableLogging(true);
}

BufferParamVerifier::~BufferParamVerifier()
{
}

const char *BufferParamVerifier::getTestNamePostfix(void) const
{
    return m_testNamePostfix;
}

class GetBufferParameterIVerifier : public BufferParamVerifier
{
public:
    GetBufferParameterIVerifier(const glw::Functions &gl, tcu::TestLog &log);

    void verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference);
};

GetBufferParameterIVerifier::GetBufferParameterIVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : BufferParamVerifier(gl, log, "_getbufferparameteri")
{
}

void GetBufferParameterIVerifier::verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetBufferParameteriv(target, name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
    }
}

} // namespace BufferParamVerifiers

namespace
{

using namespace BufferParamVerifiers;

// Tests

class BufferCase : public ApiCase
{
public:
    BufferCase(Context &context, BufferParamVerifier *verifier, const char *name, const char *description)
        : ApiCase(context, name, description)
        , m_bufferTarget(0)
        , m_verifier(verifier)
    {
    }

    virtual void testBuffer(void) = 0;

    void test(void)
    {
        const GLenum bufferTargets[] = {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER};
        const int targets            = DE_LENGTH_OF_ARRAY(bufferTargets);

        for (int ndx = 0; ndx < targets; ++ndx)
        {
            m_bufferTarget = bufferTargets[ndx];

            GLuint bufferId = 0;
            glGenBuffers(1, &bufferId);
            glBindBuffer(m_bufferTarget, bufferId);
            expectError(GL_NO_ERROR);

            testBuffer();

            glDeleteBuffers(1, &bufferId);
            expectError(GL_NO_ERROR);
        }
    }

protected:
    GLenum m_bufferTarget;
    BufferParamVerifier *m_verifier;
};

class BufferSizeCase : public BufferCase
{
public:
    BufferSizeCase(Context &context, BufferParamVerifier *verifier, const char *name, const char *description)
        : BufferCase(context, verifier, name, description)
    {
    }

    void testBuffer(void)
    {
        const int numIteration = 16;
        de::Random rnd(0xabcdef);

        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_SIZE, 0);

        for (int i = 0; i < numIteration; ++i)
        {
            const GLint len = rnd.getInt(0, 1024);
            glBufferData(m_bufferTarget, len, nullptr, GL_STREAM_DRAW);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_SIZE, len);
            expectError(GL_NO_ERROR);
        }
    }
};

class BufferUsageCase : public BufferCase
{
public:
    BufferUsageCase(Context &context, BufferParamVerifier *verifier, const char *name, const char *description)
        : BufferCase(context, verifier, name, description)
    {
    }

    void testBuffer(void)
    {
        const GLenum usages[] = {GL_STATIC_DRAW, GL_DYNAMIC_DRAW, GL_STREAM_DRAW};

        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_USAGE, GL_STATIC_DRAW);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(usages); ++ndx)
        {
            glBufferData(m_bufferTarget, 16, nullptr, usages[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_USAGE, usages[ndx]);
            expectError(GL_NO_ERROR);
        }
    }
};

} // namespace

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)                                             \
    for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++) \
    {                                                                                        \
        BufferParamVerifier *verifier = (VERIFIERS)[_verifierNdx];                           \
        CODE_BLOCK;                                                                          \
    }

BufferObjectQueryTests::BufferObjectQueryTests(Context &context)
    : TestCaseGroup(context, "buffer_object", "Buffer Object Query tests")
    , m_verifierInt(nullptr)
{
}

BufferObjectQueryTests::~BufferObjectQueryTests(void)
{
    deinit();
}

void BufferObjectQueryTests::init(void)
{
    using namespace BufferParamVerifiers;

    DE_ASSERT(m_verifierInt == nullptr);

    m_verifierInt                    = new GetBufferParameterIVerifier(m_context.getRenderContext().getFunctions(),
                                                                       m_context.getTestContext().getLog());
    BufferParamVerifier *verifiers[] = {m_verifierInt};

    FOR_EACH_VERIFIER(verifiers,
                      addChild(new BufferSizeCase(m_context, verifier,
                                                  (std::string("buffer_size") + verifier->getTestNamePostfix()).c_str(),
                                                  "BUFFER_SIZE")))
    FOR_EACH_VERIFIER(
        verifiers, addChild(new BufferUsageCase(m_context, verifier,
                                                (std::string("buffer_usage") + verifier->getTestNamePostfix()).c_str(),
                                                "BUFFER_USAGE")))
}

void BufferObjectQueryTests::deinit(void)
{
    if (m_verifierInt)
    {
        delete m_verifierInt;
        m_verifierInt = NULL;
    }

    this->TestCaseGroup::deinit();
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
