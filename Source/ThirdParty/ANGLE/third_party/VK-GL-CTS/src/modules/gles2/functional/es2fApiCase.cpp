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
 * \brief API test case.
 *//*--------------------------------------------------------------------*/

#include "es2fApiCase.hpp"
#include "gluStrUtil.hpp"
#include "gluRenderContext.hpp"

#include <algorithm>

using std::string;
using std::vector;

namespace deqp
{
namespace gles2
{
namespace Functional
{

ApiCase::ApiCase(Context &context, const char *name, const char *description)
    : TestCase(context, name, description)
    , CallLogWrapper(context.getRenderContext().getFunctions(), context.getTestContext().getLog())
    , m_log(context.getTestContext().getLog())
{
}

ApiCase::~ApiCase(void)
{
}

ApiCase::IterateResult ApiCase::iterate(void)
{
    // Initialize result to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    // Enable call logging.
    enableLogging(true);

    // Run test.
    test();

    return STOP;
}

void ApiCase::expectError(uint32_t expected)
{
    uint32_t err = glGetError();
    if (err != expected)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: expected " << glu::getErrorStr(expected)
                           << tcu::TestLog::EndMessage;
        if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid error");
    }
}

void ApiCase::expectError(uint32_t expected0, uint32_t expected1)
{
    uint32_t err = glGetError();
    if (err != expected0 && err != expected1)
    {
        m_log << tcu::TestLog::Message << "// ERROR: expected " << glu::getErrorStr(expected0) << " or "
              << glu::getErrorStr(expected1) << tcu::TestLog::EndMessage;
        if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid error");
    }
}

void ApiCase::checkBooleans(uint8_t value, uint8_t expected)
{
    checkBooleans((int32_t)value, expected);
}

void ApiCase::checkBooleans(int32_t value, uint8_t expected)
{
    if (value != (int32_t)expected)
    {
        m_log << tcu::TestLog::Message << "// ERROR: expected " << (expected ? "GL_TRUE" : "GL_FALSE")
              << tcu::TestLog::EndMessage;
        if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid boolean value");
    }
}

void ApiCase::getSupportedExtensions(const uint32_t numSupportedValues, const uint32_t extension,
                                     std::vector<int> &values)
{
    int32_t numFormats;
    GLU_CHECK_CALL(glGetIntegerv(numSupportedValues, &numFormats));
    if (numFormats == 0)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "// No supported extensions available."
                           << tcu::TestLog::EndMessage;
        return;
    }
    values.resize(numFormats);
    GLU_CHECK_CALL(glGetIntegerv(extension, &values[0]));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
