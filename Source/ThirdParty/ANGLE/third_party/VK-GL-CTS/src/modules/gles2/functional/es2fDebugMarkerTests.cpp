/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief GL_EXT_debug_marker tests
 *//*--------------------------------------------------------------------*/

#include "es2fDebugMarkerTests.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

namespace
{

using std::vector;
using tcu::TestLog;

void checkSupport(const glu::ContextInfo &ctxInfo)
{
    if (!ctxInfo.isExtensionSupported("GL_EXT_debug_marker"))
    {
#if (DE_OS == DE_OS_ANDROID)
        TCU_THROW(TestError, "Support for GL_EXT_debug_marker is mandatory on Android");
#else
        TCU_THROW(NotSupportedError, "GL_EXT_debug_marker is not supported");
#endif
    }
    // else no exception thrown
}

class IsSupportedCase : public TestCase
{
public:
    IsSupportedCase(Context &context) : TestCase(context, "supported", "Is GL_EXT_debug_marker supported")
    {
    }

    IterateResult iterate(void)
    {
        checkSupport(m_context.getContextInfo());
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "GL_EXT_debug_marker is supported");
        return STOP;
    }
};

void getSimpleRndString(vector<char> &dst, de::Random &rnd, int maxLen)
{
    const char s_chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ -_";

    dst.resize(rnd.getInt(0, (int)maxLen));

    for (size_t ndx = 0; ndx < dst.size(); ndx++)
        dst[ndx] = rnd.choose<char>(DE_ARRAY_BEGIN(s_chars), DE_ARRAY_END(s_chars));
}

void getComplexRndString(vector<char> &dst, de::Random &rnd, int maxLen)
{
    dst.resize(rnd.getInt(0, (int)maxLen));

    for (size_t ndx = 0; ndx < dst.size(); ndx++)
        dst[ndx] = (char)rnd.getUint8();
}

enum CallType
{
    CALL_TYPE_PUSH_GROUP = 0,
    CALL_TYPE_POP_GROUP,
    CALL_TYPE_INSERT_MARKER,

    CALL_TYPE_LAST
};

class RandomCase : public TestCase
{
public:
    RandomCase(Context &context) : TestCase(context, "random", "Random GL_EXT_debug_marker usage")
    {
    }

    void init(void)
    {
        checkSupport(m_context.getContextInfo());
    }

    IterateResult iterate(void)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const int numIters       = 1000;
        const int maxMsgLen      = 4096;
        de::Random rnd(0xaf829c0);

        for (int iterNdx = 0; iterNdx < numIters; iterNdx++)
        {
            const CallType callType = CallType(rnd.getInt(0, CALL_TYPE_LAST - 1));

            if (callType == CALL_TYPE_PUSH_GROUP || callType == CALL_TYPE_INSERT_MARKER)
            {
                const bool nullTerminate = rnd.getBool();
                const bool passLength    = rnd.getBool();
                const bool complexMsg    = rnd.getBool();
                vector<char> message;

                if (complexMsg)
                    getComplexRndString(message, rnd, maxMsgLen);
                else
                    getSimpleRndString(message, rnd, maxMsgLen);

                // According to spec: https://registry.khronos.org/OpenGL/extensions/EXT/EXT_debug_marker.txt
                // "If <length> is 0 then <marker> is assumed to be null-terminated."
                if (nullTerminate || !passLength)
                    message.push_back(char(0));

                {
                    const glw::GLsizei length =
                        passLength ? glw::GLsizei(nullTerminate ? message.size() - 1 : message.size()) : 0;

                    if (callType == CALL_TYPE_PUSH_GROUP)
                        gl.pushGroupMarkerEXT(length, &message[0]);
                    else
                        gl.insertEventMarkerEXT(length, &message[0]);
                }
            }
            else
                gl.popGroupMarkerEXT();
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Debug marker calls must not set error state");

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "All calls passed");
        return STOP;
    }
};

class InvalidCase : public TestCase
{
public:
    InvalidCase(Context &context) : TestCase(context, "invalid", "Invalid GL_EXT_debug_marker usage")
    {
    }

    void init(void)
    {
        checkSupport(m_context.getContextInfo());
    }

    IterateResult iterate(void)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        m_testCtx.getLog()
            << TestLog::Message
            << "Note: GL_EXT_debug_marker calls must not report an error even if invalid arguments are supplied."
            << TestLog::EndMessage;

        gl.pushGroupMarkerEXT(-1, "foo");
        gl.insertEventMarkerEXT(-1, "foo");
        gl.pushGroupMarkerEXT(0, nullptr);
        gl.insertEventMarkerEXT(0, nullptr);
        gl.pushGroupMarkerEXT(-1, nullptr);
        gl.insertEventMarkerEXT(-1, nullptr);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Debug marker calls must not set error state");

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "All calls passed");
        return STOP;
    }
};

} // namespace

tcu::TestCaseGroup *createDebugMarkerTests(Context &context)
{
    de::MovePtr<tcu::TestCaseGroup> debugMarkerGroup(
        new tcu::TestCaseGroup(context.getTestContext(), "debug_marker", "GL_EXT_debug_marker tests"));

    debugMarkerGroup->addChild(new IsSupportedCase(context));
    debugMarkerGroup->addChild(new RandomCase(context));
    debugMarkerGroup->addChild(new InvalidCase(context));

    return debugMarkerGroup.release();
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
