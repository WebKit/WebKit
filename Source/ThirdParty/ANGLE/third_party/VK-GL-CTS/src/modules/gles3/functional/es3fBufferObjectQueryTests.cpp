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
 * \brief Buffer Object Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fBufferObjectQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es3fApiCase.hpp"
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
namespace gles3
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

    virtual void verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference)     = 0;
    virtual void verifyInteger64(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint64 reference) = 0;

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
    void verifyInteger64(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint64 reference);
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

void GetBufferParameterIVerifier::verifyInteger64(tcu::TestContext &testCtx, GLenum target, GLenum name,
                                                  GLint64 reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetBufferParameteriv(target, name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    // check that the converted value would be in the correct range, otherwise checking wont tell us anything
    if (!de::inRange(reference, (GLint64)std::numeric_limits<GLint>::min(), (GLint64)std::numeric_limits<GLint>::max()))
        return;

    if (state != reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
    }
}

class GetBufferParameterI64Verifier : public BufferParamVerifier
{
public:
    GetBufferParameterI64Verifier(const glw::Functions &gl, tcu::TestLog &log);

    void verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference);
    void verifyInteger64(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint64 reference);
};

GetBufferParameterI64Verifier::GetBufferParameterI64Verifier(const glw::Functions &gl, tcu::TestLog &log)
    : BufferParamVerifier(gl, log, "_getbufferparameteri64")
{
}

void GetBufferParameterI64Verifier::verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name,
                                                  GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetBufferParameteri64v(target, name, &state);

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

void GetBufferParameterI64Verifier::verifyInteger64(tcu::TestContext &testCtx, GLenum target, GLenum name,
                                                    GLint64 reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint64> state;
    glGetBufferParameteri64v(target, name, &state);

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
        , m_testAllTargets(false)
    {
    }

    virtual void testBuffer(void) = 0;

    void test(void)
    {
        const GLenum bufferTargets[] = {
            GL_ARRAY_BUFFER,      GL_COPY_READ_BUFFER,     GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER,

            GL_COPY_WRITE_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_PIXEL_PACK_BUFFER,         GL_PIXEL_UNPACK_BUFFER};

        // most test need only to be run with a subset of targets
        const int targets = m_testAllTargets ? DE_LENGTH_OF_ARRAY(bufferTargets) : 4;

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
    bool m_testAllTargets;
};

class BufferSizeCase : public BufferCase
{
public:
    BufferSizeCase(Context &context, BufferParamVerifier *verifier, const char *name, const char *description)
        : BufferCase(context, verifier, name, description)
    {
        m_testAllTargets = true;
    }

    void testBuffer(void)
    {
        de::Random rnd(0xabcdef);

        m_verifier->verifyInteger64(m_testCtx, m_bufferTarget, GL_BUFFER_SIZE, 0);

        const int numIterations = 16;
        for (int i = 0; i < numIterations; ++i)
        {
            const GLint len = rnd.getInt(0, 1024);
            glBufferData(m_bufferTarget, len, nullptr, GL_STREAM_DRAW);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger64(m_testCtx, m_bufferTarget, GL_BUFFER_SIZE, len);
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
        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_USAGE, GL_STATIC_DRAW);

        const GLenum usages[] = {GL_STREAM_DRAW, GL_STREAM_READ,  GL_STREAM_COPY,  GL_STATIC_DRAW, GL_STATIC_READ,
                                 GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, GL_DYNAMIC_COPY};

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(usages); ++ndx)
        {
            glBufferData(m_bufferTarget, 16, nullptr, usages[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_USAGE, usages[ndx]);
            expectError(GL_NO_ERROR);
        }
    }
};

class BufferAccessFlagsCase : public BufferCase
{
public:
    BufferAccessFlagsCase(Context &context, BufferParamVerifier *verifier, const char *name, const char *description)
        : BufferCase(context, verifier, name, description)
    {
    }

    void testBuffer(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_ACCESS_FLAGS, 0);

        const GLenum accessFlags[] = {
            GL_MAP_READ_BIT,

            GL_MAP_WRITE_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,

            GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_INVALIDATE_RANGE_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,

            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,

            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_INVALIDATE_RANGE_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
                GL_MAP_INVALIDATE_BUFFER_BIT,

        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(accessFlags); ++ndx)
        {
            glBufferData(m_bufferTarget, 16, nullptr, GL_DYNAMIC_COPY);
            glMapBufferRange(m_bufferTarget, 0, 16, accessFlags[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_ACCESS_FLAGS, accessFlags[ndx]);
            expectError(GL_NO_ERROR);

            glUnmapBuffer(m_bufferTarget);
            expectError(GL_NO_ERROR);
        }
    }
};

class BufferMappedCase : public BufferCase
{
public:
    BufferMappedCase(Context &context, BufferParamVerifier *verifier, const char *name, const char *description)
        : BufferCase(context, verifier, name, description)
    {
    }

    void testBuffer(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_MAPPED, GL_FALSE);

        glBufferData(m_bufferTarget, 16, nullptr, GL_DYNAMIC_COPY);
        glMapBufferRange(m_bufferTarget, 0, 16, GL_MAP_WRITE_BIT);
        expectError(GL_NO_ERROR);

        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_MAPPED, GL_TRUE);
        expectError(GL_NO_ERROR);

        glUnmapBuffer(m_bufferTarget);
        expectError(GL_NO_ERROR);
    }
};

class BufferOffsetLengthCase : public BufferCase
{
public:
    BufferOffsetLengthCase(Context &context, BufferParamVerifier *verifier, const char *name, const char *description)
        : BufferCase(context, verifier, name, description)
    {
    }

    void testBuffer(void)
    {
        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_MAP_OFFSET, 0);
        m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_MAP_LENGTH, 0);

        glBufferData(m_bufferTarget, 16, nullptr, GL_DYNAMIC_COPY);

        const struct BufferRange
        {
            int offset;
            int length;
        } ranges[] = {
            {0, 16},
            {4, 12},
            {0, 12},
            {8, 8},
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(ranges); ++ndx)
        {
            glMapBufferRange(m_bufferTarget, ranges[ndx].offset, ranges[ndx].length, GL_MAP_WRITE_BIT);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_MAP_OFFSET, ranges[ndx].offset);
            m_verifier->verifyInteger(m_testCtx, m_bufferTarget, GL_BUFFER_MAP_LENGTH, ranges[ndx].length);
            expectError(GL_NO_ERROR);

            glUnmapBuffer(m_bufferTarget);
            expectError(GL_NO_ERROR);
        }
    }
};

class BufferPointerCase : public ApiCase
{
public:
    BufferPointerCase(Context &context, const char *name, const char *description) : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint bufferId = 0;
        glGenBuffers(1, &bufferId);
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);
        expectError(GL_NO_ERROR);

        StateQueryMemoryWriteGuard<GLvoid *> initialState;
        glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &initialState);
        initialState.verifyValidity(m_testCtx);
        checkPointerEquals(m_testCtx, initialState, 0);

        glBufferData(GL_ARRAY_BUFFER, 8, nullptr, GL_DYNAMIC_COPY);
        GLvoid *mapPointer = glMapBufferRange(GL_ARRAY_BUFFER, 0, 8, GL_MAP_READ_BIT);
        expectError(GL_NO_ERROR);

        StateQueryMemoryWriteGuard<GLvoid *> mapPointerState;
        glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &mapPointerState);
        mapPointerState.verifyValidity(m_testCtx);
        checkPointerEquals(m_testCtx, mapPointerState, mapPointer);

        glDeleteBuffers(1, &bufferId);
        expectError(GL_NO_ERROR);
    }
};

} // namespace

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)                                                 \
    do                                                                                           \
    {                                                                                            \
        for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++) \
        {                                                                                        \
            BufferParamVerifier *verifier = (VERIFIERS)[_verifierNdx];                           \
            CODE_BLOCK;                                                                          \
        }                                                                                        \
    } while (0)

BufferObjectQueryTests::BufferObjectQueryTests(Context &context)
    : TestCaseGroup(context, "buffer_object", "Buffer Object Query tests")
    , m_verifierInt(nullptr)
    , m_verifierInt64(nullptr)
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
    DE_ASSERT(m_verifierInt64 == nullptr);

    m_verifierInt                    = new GetBufferParameterIVerifier(m_context.getRenderContext().getFunctions(),
                                                                       m_context.getTestContext().getLog());
    m_verifierInt64                  = new GetBufferParameterI64Verifier(m_context.getRenderContext().getFunctions(),
                                                                         m_context.getTestContext().getLog());
    BufferParamVerifier *verifiers[] = {m_verifierInt, m_verifierInt64};

    FOR_EACH_VERIFIER(verifiers,
                      addChild(new BufferSizeCase(m_context, verifier,
                                                  (std::string("buffer_size") + verifier->getTestNamePostfix()).c_str(),
                                                  "BUFFER_SIZE")));
    FOR_EACH_VERIFIER(
        verifiers, addChild(new BufferUsageCase(m_context, verifier,
                                                (std::string("buffer_usage") + verifier->getTestNamePostfix()).c_str(),
                                                "BUFFER_USAGE")));
    FOR_EACH_VERIFIER(verifiers, addChild(new BufferAccessFlagsCase(
                                     m_context, verifier,
                                     (std::string("buffer_access_flags") + verifier->getTestNamePostfix()).c_str(),
                                     "BUFFER_ACCESS_FLAGS")));
    FOR_EACH_VERIFIER(verifiers,
                      addChild(new BufferMappedCase(
                          m_context, verifier, (std::string("buffer_mapped") + verifier->getTestNamePostfix()).c_str(),
                          "BUFFER_MAPPED")));
    FOR_EACH_VERIFIER(verifiers, addChild(new BufferOffsetLengthCase(
                                     m_context, verifier,
                                     (std::string("buffer_map_offset_length") + verifier->getTestNamePostfix()).c_str(),
                                     "BUFFER_MAP_OFFSET and BUFFER_MAP_LENGTH")));

    addChild(new BufferPointerCase(m_context, "buffer_pointer", "GetBufferPointerv"));
}

void BufferObjectQueryTests::deinit(void)
{
    if (m_verifierInt)
    {
        delete m_verifierInt;
        m_verifierInt = NULL;
    }
    if (m_verifierInt64)
    {
        delete m_verifierInt64;
        m_verifierInt64 = NULL;
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
