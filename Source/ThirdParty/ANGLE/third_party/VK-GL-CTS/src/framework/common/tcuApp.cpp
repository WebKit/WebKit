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
 * \brief Render target info.
 *//*--------------------------------------------------------------------*/

#include "tcuApp.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestSessionExecutor.hpp"
#include "tcuTestHierarchyUtil.hpp"
#include "tcuMustpassGen.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "qpInfo.h"
#include "qpDebugOut.h"

#include "deMath.h"

#include <iostream>

namespace tcu
{

using std::string;

/*--------------------------------------------------------------------*//*!
 *  Writes all packages found stdout without any
 *  separations. Recommended to be used with a single package
 *  only. It's possible to use test selectors for limiting the export
 *  to one package in a multipackage binary.
 *//*--------------------------------------------------------------------*/
static void writeCaselistsToStdout(TestPackageRoot &root, TestContext &testCtx, bool printPrefix = true,
                                   bool skipGroups = false)
{
    DefaultHierarchyInflater inflater(testCtx);
    de::MovePtr<const CaseListFilter> caseListFilter(
        testCtx.getCommandLine().createCaseListFilter(testCtx.getArchive()));
    TestHierarchyIterator iter(root, inflater, *caseListFilter);

    while (iter.getState() != TestHierarchyIterator::STATE_FINISHED)
    {
        iter.next();

        while (iter.getNode()->getNodeType() != NODETYPE_PACKAGE)
        {
            if (iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE)
            {
                const bool isTest = isTestNodeTypeExecutable(iter.getNode()->getNodeType());
                if (isTest || !skipGroups)
                {
                    const char *prefix = (printPrefix ? (isTest ? "TEST: " : "GROUP: ") : "");
                    std::cout << prefix << iter.getNodePath() << "\n";
                }
            }
            iter.next();
        }

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE &&
                  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);
        iter.next();
    }
}

/*--------------------------------------------------------------------*//*!
 * Verifies that amber capability requirements in the .amber files
 * match with capabilities defined on the CTS C code.
 *//*--------------------------------------------------------------------*/
static void verifyAmberCapabilityCoherency(TestPackageRoot &root, TestContext &testCtx)
{
    DefaultHierarchyInflater inflater(testCtx);
    de::MovePtr<const CaseListFilter> caseListFilter(
        testCtx.getCommandLine().createCaseListFilter(testCtx.getArchive()));
    TestHierarchyIterator iter(root, inflater, *caseListFilter);
    int count      = 0;
    int errorCount = 0;

    bool ok = true;

    while (iter.getState() != TestHierarchyIterator::STATE_FINISHED)
    {
        iter.next();

        while (iter.getNode()->getNodeType() != NODETYPE_PACKAGE)
        {
            if (iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE &&
                isTestNodeTypeExecutable(iter.getNode()->getNodeType()))
            {
                std::cout << iter.getNodePath() << "\n";
                testCtx.getLog() << tcu::TestLog::Message << iter.getNodePath() << tcu::TestLog::EndMessage;
                if (!iter.getNode()->validateRequirements())
                {
                    ok = false;
                    errorCount++;
                }
                count++;
            }
            iter.next();
        }

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE &&
                  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);
        iter.next();
    }
    std::cout << count << " amber tests, " << errorCount << " errors.\n";
    if (!ok)
        TCU_THROW(InternalError, "One or more CTS and Amber test requirements do not match; check log for details");
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct test application
 *
 * If a fatal error occurs during initialization constructor will call
 * die() with debug information.
 *
 * \param platform Reference to platform implementation.
 *//*--------------------------------------------------------------------*/
App::App(Platform &platform, Archive &archive, TestLog &log, const CommandLine &cmdLine)
    : m_platform(platform)
    , m_watchDog(nullptr)
    , m_crashHandler(nullptr)
    , m_crashed(false)
    , m_testCtx(nullptr)
    , m_testRoot(nullptr)
    , m_testExecutor(nullptr)
{
    if (!cmdLine.isSubProcess())
    {
        print("dEQP Core %s (0x%08x) starting..\n", qpGetReleaseName(), qpGetReleaseId());
        print("  target implementation = '%s'\n", qpGetTargetName());
    }

    if (!deSetRoundingMode(DE_ROUNDINGMODE_TO_NEAREST_EVEN))
        qpPrintf("WARNING: Failed to set floating-point rounding mode!\n");

    try
    {
        const RunMode runMode = cmdLine.getRunMode();

        // Initialize watchdog
        if (cmdLine.isWatchDogEnabled())
            TCU_CHECK_INTERNAL(m_watchDog = qpWatchDog_create(onWatchdogTimeout, this, cmdLine.getWatchDogTotalTime(),
                                                              cmdLine.getWatchDogIntervalTime()));

        // Initialize crash handler.
        if (cmdLine.isCrashHandlingEnabled())
            TCU_CHECK_INTERNAL(m_crashHandler = qpCrashHandler_create(onCrash, this));

        // Create test context
        m_testCtx = new TestContext(m_platform, archive, log, cmdLine, m_watchDog);

        // Create root from registry
        m_testRoot = new TestPackageRoot(*m_testCtx, TestPackageRegistry::getSingleton());

        // \note No executor is created if runmode is not EXECUTE
        if (runMode == RUNMODE_EXECUTE)
            m_testExecutor = new TestSessionExecutor(*m_testRoot, *m_testCtx);
        else if (runMode == RUNMODE_DUMP_STDOUT_CASELIST)
            writeCaselistsToStdout(*m_testRoot, *m_testCtx);
        else if (runMode == RUNMODE_DUMP_STDOUT_TRIE)
            writeCaselistsToStdout(*m_testRoot, *m_testCtx, false, true);
        else if (runMode == RUNMODE_DUMP_XML_CASELIST)
            writeXmlCaselistsToFiles(*m_testRoot, *m_testCtx, cmdLine);
        else if (runMode == RUNMODE_DUMP_TEXT_CASELIST)
            writeTxtCaselistsToFiles(*m_testRoot, *m_testCtx, cmdLine);
        else if (runMode == RUNMODE_DUMP_TEXT_TRIE)
            writeTxtCaselistsToFiles(*m_testRoot, *m_testCtx, cmdLine, false, true);
        else if (runMode == RUNMODE_VERIFY_AMBER_COHERENCY)
            verifyAmberCapabilityCoherency(*m_testRoot, *m_testCtx);
        else if (runMode == RUNMODE_GEN_MUSTPASS)
            genMustpassFromSpec(*m_testRoot, *m_testCtx, cmdLine);
        else
            DE_ASSERT(false);
    }
    catch (const std::exception &e)
    {
        cleanup();
        die("Failed to initialize dEQP: %s", e.what());
    }
}

App::~App(void)
{
    cleanup();
}

void App::cleanup(void)
{
    delete m_testExecutor;
    delete m_testRoot;
    delete m_testCtx;

    if (m_crashHandler)
        qpCrashHandler_destroy(m_crashHandler);

    if (m_watchDog)
        qpWatchDog_destroy(m_watchDog);
}

/*--------------------------------------------------------------------*//*!
 * \brief Step forward test execution
 * \return true if application should call iterate() again and false
 *         if test execution session is complete.
 *//*--------------------------------------------------------------------*/
bool App::iterate(void)
{
    if (!m_testExecutor)
    {
        DE_ASSERT(m_testCtx->getCommandLine().getRunMode() != RUNMODE_EXECUTE);
        return false;
    }

    // Poll platform events
    const bool platformOk = m_platform.processEvents();

    // Iterate a step.
    bool testExecOk = false;
    if (platformOk)
    {
        try
        {
            testExecOk = m_testExecutor->iterate();
        }
        catch (const std::exception &e)
        {
            die("%s", e.what());
        }
    }

    if ((!platformOk || !testExecOk))
    {
        if (!m_testCtx->getCommandLine().isSubProcess())
        {
            if (!platformOk)
                print("\nABORTED!\n");
            else
                print("\nDONE!\n");
        }

        const RunMode runMode = m_testCtx->getCommandLine().getRunMode();
        if (runMode == RUNMODE_EXECUTE)
        {
            const TestRunStatus &result = m_testExecutor->getStatus();
            if (!m_testCtx->getCommandLine().isSubProcess())
            {
                // Report statistics.
                print("\nTest run totals:\n");
                print("  Passed:        %d/%d (%.1f%%)\n", result.numPassed, result.numExecuted,
                      (result.numExecuted > 0 ? (100.0f * (float)result.numPassed / (float)result.numExecuted) : 0.0f));
                print("  Failed:        %d/%d (%.1f%%)\n", result.numFailed, result.numExecuted,
                      (result.numExecuted > 0 ? (100.0f * (float)result.numFailed / (float)result.numExecuted) : 0.0f));
                print("  Not supported: %d/%d (%.1f%%)\n", result.numNotSupported, result.numExecuted,
                      (result.numExecuted > 0 ? (100.0f * (float)result.numNotSupported / (float)result.numExecuted) :
                                                0.0f));
                print(
                    "  Warnings:      %d/%d (%.1f%%)\n", result.numWarnings, result.numExecuted,
                    (result.numExecuted > 0 ? (100.0f * (float)result.numWarnings / (float)result.numExecuted) : 0.0f));
                print("  Waived:        %d/%d (%.1f%%)\n", result.numWaived, result.numExecuted,
                      (result.numExecuted > 0 ? (100.0f * (float)result.numWaived / (float)result.numExecuted) : 0.0f));
                if (!result.isComplete)
                    print("Test run was ABORTED!\n");
            }
            else
            {
                // subprocess sends test statisticts through qpa file, so that main process may read it
                // and add to global statistics ( search for #SubProcessStatus to see how it's done )
                std::ostringstream str;
                str << "\n#SubProcessStatus " << result.numExecuted << " " << result.numPassed << " "
                    << result.numFailed << " " << result.numNotSupported << " " << result.numWarnings << " "
                    << result.numWaived << "\n";
                m_testCtx->getLog().writeRaw(str.str().c_str());
            }
        }
    }

    return platformOk && testExecOk;
}

const TestRunStatus &App::getResult(void) const
{
    return m_testExecutor->getStatus();
}

void App::onWatchdogTimeout(qpWatchDog *watchDog, void *userPtr, qpTimeoutReason reason)
{
    DE_UNREF(watchDog);
    static_cast<App *>(userPtr)->onWatchdogTimeout(reason);
}

void App::onCrash(qpCrashHandler *crashHandler, void *userPtr)
{
    DE_UNREF(crashHandler);
    static_cast<App *>(userPtr)->onCrash();
}

void App::onWatchdogTimeout(qpTimeoutReason reason)
{
    if (!m_crashLock.tryLock() || m_crashed)
        return; // In crash handler already.

    m_crashed = true;

    m_testCtx->getLog().terminateCase(QP_TEST_RESULT_TIMEOUT);
    die("Watchdog timer timeout for %s",
        (reason == QP_TIMEOUT_REASON_INTERVAL_LIMIT ? "touch interval" : "total time"));
}

static void writeCrashToLog(void *userPtr, const char *infoString)
{
    // \note THIS IS CALLED BY SIGNAL HANDLER! CALLING MALLOC/FREE IS NOT ALLOWED!
    TestLog *log = static_cast<TestLog *>(userPtr);
    log->writeMessage(infoString);
}

static void writeCrashToConsole(void *userPtr, const char *infoString)
{
    // \note THIS IS CALLED BY SIGNAL HANDLER! CALLING MALLOC/FREE IS NOT ALLOWED!
    DE_UNREF(userPtr);
    qpPrint(infoString);
}

void App::onCrash(void)
{
    // \note THIS IS CALLED BY SIGNAL HANDLER! CALLING MALLOC/FREE IS NOT ALLOWED!

    if (!m_crashLock.tryLock() || m_crashed)
        return; // In crash handler already.

    m_crashed = true;

    bool isInCase = m_testExecutor ? m_testExecutor->isInTestCase() : false;

    if (isInCase)
    {
        qpCrashHandler_writeCrashInfo(m_crashHandler, writeCrashToLog, &m_testCtx->getLog());
        m_testCtx->getLog().terminateCase(QP_TEST_RESULT_CRASH);
    }
    else
        qpCrashHandler_writeCrashInfo(m_crashHandler, writeCrashToConsole, nullptr);

    die("Test program crashed");
}

} // namespace tcu
