/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2019 The Android Open Source Project
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
 * \brief OpenGL ES 3 Test Package that runs on GL4.5 context
 *//*--------------------------------------------------------------------*/

#include "tgl45es3TestPackage.hpp"
#include "tes3TestCaseWrapper.hpp"
#include "tes3InfoTests.hpp"
#include "es3fFunctionalTests.hpp"
#include "es3sStressTests.hpp"
#include "gluStateReset.hpp"
#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuWaiverUtil.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{

TestPackageGL45ES3::TestPackageGL45ES3(tcu::TestContext &testCtx)
    : tcu::TestPackage(testCtx, "dEQP-GL45-ES3", "dEQP OpenGL ES 3 Tests On GL4.5 Context")
    , m_archive(testCtx.getRootArchive(), "gles3/")
    , m_context(nullptr)
    , m_waiverMechanism(new tcu::WaiverUtil)
{
}

TestPackageGL45ES3::~TestPackageGL45ES3(void)
{
    // Destroy children first since destructors may access context.
    TestNode::deinit();
    delete m_context;
}

void TestPackageGL45ES3::init(void)
{
    try
    {
        // Create context
        m_context = new Context(m_testCtx, glu::ApiType::core(4, 5));

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
        addChild(new Functional::GL45ES3FunctionalTests(*m_context));
    }
    catch (...)
    {
        delete m_context;
        m_context = nullptr;

        throw;
    }
}

void TestPackageGL45ES3::deinit(void)
{
    TestNode::deinit();
    delete m_context;
    m_context = nullptr;
}

tcu::TestCaseExecutor *TestPackageGL45ES3::createExecutor(void) const
{
    return new TestCaseWrapper<TestPackageGL45ES3>(const_cast<TestPackageGL45ES3 &>(*this), m_waiverMechanism);
}

} // namespace gles3
} // namespace deqp
