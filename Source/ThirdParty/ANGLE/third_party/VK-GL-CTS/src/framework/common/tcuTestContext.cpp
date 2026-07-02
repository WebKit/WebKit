/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Context shared between test cases.
 *//*--------------------------------------------------------------------*/

#include "tcuTestContext.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

namespace tcu
{

TestContext::TestContext(Platform &platform, Archive &rootArchive, TestLog &log, const CommandLine &cmdLine,
                         qpWatchDog *watchDog)
    : m_platform(platform)
    , m_rootArchive(rootArchive)
    , m_log(log)
    , m_cmdLine(cmdLine)
    , m_watchDog(watchDog)
    , m_curArchive(nullptr)
    , m_testResult(QP_TEST_RESULT_LAST)
    , m_terminateAfter(false)
{
    setCurrentArchive(m_rootArchive);
}

void TestContext::writeSessionInfo(void)
{
    const std::string sessionInfo = "#sessionInfo commandLineParameters \"";
    m_log.writeSessionInfo(sessionInfo + m_cmdLine.getInitialCmdLine() + "\"\n");
}

void TestContext::touchWatchdog(void)
{
    if (m_watchDog)
        qpWatchDog_touch(m_watchDog);
}

void TestContext::touchWatchdogAndDisableIntervalTimeLimit(void)
{
    if (m_watchDog)
        qpWatchDog_touchAndDisableIntervalTimeLimit(m_watchDog);
}

void TestContext::touchWatchdogAndEnableIntervalTimeLimit(void)
{
    if (m_watchDog)
        qpWatchDog_touchAndEnableIntervalTimeLimit(m_watchDog);
}

void TestContext::setTestResult(qpTestResult testResult, const char *description)
{
    m_testResult     = testResult;
    m_testResultDesc = description;
}

} // namespace tcu
