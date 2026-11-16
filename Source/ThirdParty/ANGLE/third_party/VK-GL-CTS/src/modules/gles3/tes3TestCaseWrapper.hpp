#ifndef _TES3TESTCASEWRAPPER_HPP
#define _TES3TESTCASEWRAPPER_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief OpenGL ES 3.0 Test Package that runs on GL4.5 context
 *//*--------------------------------------------------------------------*/

#include "tcuTestLog.hpp"
#include "tes3Context.hpp"
#include "tcuWaiverUtil.hpp"
#include "gluStateReset.hpp"

namespace deqp
{
namespace gles3
{

template <typename TEST_PACKAGE>
class TestCaseWrapper : public tcu::TestCaseExecutor
{
public:
    TestCaseWrapper(TEST_PACKAGE &package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism);
    ~TestCaseWrapper(void);

    void init(tcu::TestCase *testCase, const std::string &path);
    void deinit(tcu::TestCase *testCase);
    tcu::TestNode::IterateResult iterate(tcu::TestCase *testCase);

private:
    TEST_PACKAGE &m_testPackage;
    de::SharedPtr<tcu::WaiverUtil> m_waiverMechanism;
};

template <typename TEST_PACKAGE>
TestCaseWrapper<TEST_PACKAGE>::TestCaseWrapper(TEST_PACKAGE &package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism)
    : m_testPackage(package)
    , m_waiverMechanism(waiverMechanism)
{
}

template <typename TEST_PACKAGE>
TestCaseWrapper<TEST_PACKAGE>::~TestCaseWrapper(void)
{
}

template <typename TEST_PACKAGE>
void TestCaseWrapper<TEST_PACKAGE>::init(tcu::TestCase *testCase, const std::string &path)
{
    if (m_waiverMechanism->isOnWaiverList(path))
        throw tcu::TestException("Waived test", QP_TEST_RESULT_WAIVER);

    testCase->init();
}

template <typename TEST_PACKAGE>
void TestCaseWrapper<TEST_PACKAGE>::deinit(tcu::TestCase *testCase)
{
    testCase->deinit();

    DE_ASSERT(m_testPackage.getContext());
    glu::resetState(m_testPackage.getContext()->getRenderContext(), m_testPackage.getContext()->getContextInfo());
}

template <typename TEST_PACKAGE>
tcu::TestNode::IterateResult TestCaseWrapper<TEST_PACKAGE>::iterate(tcu::TestCase *testCase)
{
    tcu::TestContext &testCtx                 = m_testPackage.getContext()->getTestContext();
    const tcu::TestCase::IterateResult result = testCase->iterate();

    // Call implementation specific post-iterate routine (usually handles native events and swaps buffers)
    try
    {
        m_testPackage.getContext()->getRenderContext().postIterate();
        return result;
    }
    catch (const tcu::ResourceError &e)
    {
        testCtx.getLog() << e;
        testCtx.setTestResult(QP_TEST_RESULT_RESOURCE_ERROR, "Resource error in context post-iteration routine");
        testCtx.setTerminateAfter(true);
        return tcu::TestNode::STOP;
    }
    catch (const std::exception &e)
    {
        testCtx.getLog() << e;
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Error in context post-iteration routine");
        return tcu::TestNode::STOP;
    }
}

} // namespace gles3
} // namespace deqp

#endif // _TES3TESTCASEWRAPPER_HPP
