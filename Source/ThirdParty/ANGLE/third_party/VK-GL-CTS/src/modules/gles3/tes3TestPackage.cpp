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
 * \brief OpenGL ES 3.0 Test Package
 *//*--------------------------------------------------------------------*/

#include "tes3TestPackage.hpp"
#include "tes3InfoTests.hpp"
#include "es3fFunctionalTests.hpp"
#include "es3aAccuracyTests.hpp"
#include "es3sStressTests.hpp"
#include "es3pPerformanceTests.hpp"
#include "tcuTestLog.hpp"
#include "tcuWaiverUtil.hpp"
#include "tcuCommandLine.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "gluStateReset.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{

class TestCaseWrapper : public tcu::TestCaseExecutor
{
public:
    TestCaseWrapper(TestPackage &package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism);
    ~TestCaseWrapper(void);

    void init(tcu::TestCase *testCase, const std::string &path);
    void deinit(tcu::TestCase *testCase);
    tcu::TestNode::IterateResult iterate(tcu::TestCase *testCase);

private:
    TestPackage &m_testPackage;
    de::SharedPtr<tcu::WaiverUtil> m_waiverMechanism;
};

TestCaseWrapper::TestCaseWrapper(TestPackage &package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism)
    : m_testPackage(package)
    , m_waiverMechanism(waiverMechanism)
{
}

TestCaseWrapper::~TestCaseWrapper(void)
{
}

void TestCaseWrapper::init(tcu::TestCase *testCase, const std::string &path)
{
    if (m_waiverMechanism->isOnWaiverList(path))
        throw tcu::TestException("Waived test", QP_TEST_RESULT_WAIVER);

    testCase->init();
}

void TestCaseWrapper::deinit(tcu::TestCase *testCase)
{
    testCase->deinit();

    DE_ASSERT(m_testPackage.getContext());
    glu::resetState(m_testPackage.getContext()->getRenderContext(), m_testPackage.getContext()->getContextInfo());
}

tcu::TestNode::IterateResult TestCaseWrapper::iterate(tcu::TestCase *testCase)
{
    tcu::TestContext &testCtx     = m_testPackage.getContext()->getTestContext();
    glu::RenderContext &renderCtx = m_testPackage.getContext()->getRenderContext();
    tcu::TestCase::IterateResult result;

    // Clear to surrender-blue
    {
        const glw::Functions &gl = renderCtx.getFunctions();
        gl.clearColor(0.125f, 0.25f, 0.5f, 1.f);
        gl.clear(GL_COLOR_BUFFER_BIT);
    }

    result = testCase->iterate();

    // Call implementation specific post-iterate routine (usually handles native events and swaps buffers)
    try
    {
        renderCtx.postIterate();
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

TestPackage::TestPackage(tcu::TestContext &testCtx)
    : tcu::TestPackage(testCtx, "dEQP-GLES3", "dEQP OpenGL ES 3.0 Tests")
    , m_archive(testCtx.getRootArchive(), "gles3/")
    , m_context(nullptr)
    , m_waiverMechanism(new tcu::WaiverUtil)
{
}

TestPackage::~TestPackage(void)
{
    // Destroy children first since destructors may access context.
    TestNode::deinit();
    delete m_context;
}

void TestPackage::init(void)
{
    try
    {
        // Create context
        m_context = new Context(m_testCtx);

        // Setup waiver mechanism
        if (m_testCtx.getCommandLine().getRunMode() == tcu::RUNMODE_EXECUTE)
        {
            const glu::ContextInfo &contextInfo = m_context->getContextInfo();
            std::string vendor                  = contextInfo.getString(GL_VENDOR);
            std::string renderer                = contextInfo.getString(GL_RENDERER);
            const tcu::CommandLine &commandLine = m_context->getTestContext().getCommandLine();
            tcu::SessionInfo sessionInfo(vendor, renderer, commandLine.getInitialCmdLine());
            m_waiverMechanism->setup(commandLine.getWaiverFileName(), m_name, vendor, renderer, sessionInfo);
            m_context->getTestContext().getLog().writeSessionInfo(sessionInfo.get());
        }

        // Add main test groups
        addChild(new InfoTests(*m_context));
        addChild(new Functional::FunctionalTests(*m_context));
        addChild(new Accuracy::AccuracyTests(*m_context));
        addChild(new Performance::PerformanceTests(*m_context));
        addChild(new Stress::StressTests(*m_context));
    }
    catch (...)
    {
        delete m_context;
        m_context = nullptr;

        throw;
    }
}

void TestPackage::deinit(void)
{
    TestNode::deinit();
    delete m_context;
    m_context = nullptr;
}

tcu::TestCaseExecutor *TestPackage::createExecutor(void) const
{
    return new TestCaseWrapper(const_cast<TestPackage &>(*this), m_waiverMechanism);
}

} // namespace gles3
} // namespace deqp
