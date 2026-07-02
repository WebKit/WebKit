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
 * \brief Test executor.
 *//*--------------------------------------------------------------------*/

#include "tcuTestSessionExecutor.hpp"
#include "qpTestLog.h"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deClock.h"

namespace tcu
{

using std::vector;

static qpTestCaseType nodeTypeToTestCaseType(TestNodeType nodeType)
{
    switch (nodeType)
    {
    case NODETYPE_SELF_VALIDATE:
        return QP_TEST_CASE_TYPE_SELF_VALIDATE;
    case NODETYPE_PERFORMANCE:
        return QP_TEST_CASE_TYPE_PERFORMANCE;
    case NODETYPE_CAPABILITY:
        return QP_TEST_CASE_TYPE_CAPABILITY;
    case NODETYPE_ACCURACY:
        return QP_TEST_CASE_TYPE_ACCURACY;
    default:
        DE_ASSERT(false);
        return QP_TEST_CASE_TYPE_LAST;
    }
}

TestSessionExecutor::TestSessionExecutor(TestPackageRoot &root, TestContext &testCtx)
    : m_testCtx(testCtx)
    , m_inflater(testCtx)
    , m_caseListFilter(testCtx.getCommandLine().createCaseListFilter(testCtx.getArchive()))
    , m_iterator(root, m_inflater, *m_caseListFilter)
    , m_state(STATE_TRAVERSE_HIERARCHY)
    , m_abortSession(false)
    , m_isInTestCase(false)
    , m_testStartTime(0)
    , m_packageStartTime(0)
{
}

TestSessionExecutor::~TestSessionExecutor(void)
{
}

bool TestSessionExecutor::iterate(void)
{
    while (!m_abortSession)
    {
        switch (m_state)
        {
        case STATE_TRAVERSE_HIERARCHY:
        {
            const TestHierarchyIterator::State hierIterState = m_iterator.getState();

            if (hierIterState == TestHierarchyIterator::STATE_ENTER_NODE ||
                hierIterState == TestHierarchyIterator::STATE_LEAVE_NODE)
            {
                TestNode *const curNode     = m_iterator.getNode();
                const TestNodeType nodeType = curNode->getNodeType();
                const bool isEnter          = hierIterState == TestHierarchyIterator::STATE_ENTER_NODE;

                switch (nodeType)
                {
                case NODETYPE_PACKAGE:
                {
                    TestPackage *const testPackage = static_cast<TestPackage *>(curNode);
                    isEnter ? enterTestPackage(testPackage) : leaveTestPackage(testPackage);
                    break;
                }

                case NODETYPE_GROUP:
                {
                    isEnter ? enterTestGroup(m_iterator.getNodePath()) : leaveTestGroup(m_iterator.getNodePath());
                    break; // nada
                }

                case NODETYPE_SELF_VALIDATE:
                case NODETYPE_PERFORMANCE:
                case NODETYPE_CAPABILITY:
                case NODETYPE_ACCURACY:
                {
                    TestCase *const testCase = static_cast<TestCase *>(curNode);

                    if (isEnter)
                    {
                        if (enterTestCase(testCase, m_iterator.getNodePath()))
                            m_state = STATE_EXECUTE_TEST_CASE;
                        // else remain in TRAVERSING_HIERARCHY => node will be exited from in the next iteration
                    }
                    else
                        leaveTestCase(testCase);

                    break;
                }

                default:
                    DE_ASSERT(false);
                    break;
                }

                m_iterator.next();
                break;
            }
            else
            {
                DE_ASSERT(hierIterState == TestHierarchyIterator::STATE_FINISHED);
                m_status.isComplete = true;
                return false;
            }
        }

        case STATE_EXECUTE_TEST_CASE:
        {
            DE_ASSERT(m_iterator.getState() == TestHierarchyIterator::STATE_LEAVE_NODE &&
                      isTestNodeTypeExecutable(m_iterator.getNode()->getNodeType()));

            TestCase *const testCase                 = static_cast<TestCase *>(m_iterator.getNode());
            const TestCase::IterateResult iterResult = iterateTestCase(testCase);

            if (iterResult == TestCase::STOP)
                m_state = STATE_TRAVERSE_HIERARCHY;

            return true;
        }

        default:
            DE_ASSERT(false);
            break;
        }
    }

    return false;
}

void TestSessionExecutor::enterTestPackage(TestPackage *testPackage)
{
    // Create test case wrapper
    DE_ASSERT(!m_caseExecutor);
    m_caseExecutor = de::MovePtr<TestCaseExecutor>(testPackage->createExecutor());
    testPackage->setCaseListFilter(m_caseListFilter.get());
    m_packageStartTime = deGetMicroseconds();
}

void TestSessionExecutor::leaveTestPackage(TestPackage *testPackage)
{
    DE_UNREF(testPackage);
    m_caseExecutor->deinitTestPackage(m_testCtx);
    // If m_caseExecutor uses local status then it may perform some tests in deinitTestPackage(). We have to update TestSessionExecutor::m_status
    if (m_caseExecutor->usesLocalStatus(m_testCtx))
        m_caseExecutor->updateGlobalStatus(m_status);

    const int64_t duration = deGetMicroseconds() - m_packageStartTime;
    m_packageStartTime     = 0;

    if (!std::string(m_testCtx.getCommandLine().getServerAddress()).empty())
        m_caseExecutor->reportDurations(m_testCtx, std::string(testPackage->getName()), duration, m_groupsDurationTime);

    m_caseExecutor.clear();

    if (!std::string(m_testCtx.getCommandLine().getServerAddress()).empty())
    {
        m_testCtx.getLog().startTestsCasesTime();

        m_testCtx.getLog() << TestLog::Integer(testPackage->getName(), "Total tests case duration in microseconds",
                                               "us", QP_KEY_TAG_TIME, duration);

        for (std::map<std::string, uint64_t>::iterator it = m_groupsDurationTime.begin();
             it != m_groupsDurationTime.end(); ++it)
            m_testCtx.getLog() << TestLog::Integer(it->first, "The test group case duration in microseconds", "us",
                                                   QP_KEY_TAG_TIME, it->second);

        m_testCtx.getLog().endTestsCasesTime();
    }
}

void TestSessionExecutor::enterTestGroup(const std::string &casePath)
{
    m_groupsDurationTime[casePath] = deGetMicroseconds();
}

void TestSessionExecutor::leaveTestGroup(const std::string &casePath)
{
    m_groupsDurationTime[casePath] = deGetMicroseconds() - m_groupsDurationTime[casePath];
}

bool TestSessionExecutor::enterTestCase(TestCase *testCase, const std::string &casePath)
{
    TestLog &log                  = m_testCtx.getLog();
    const qpTestCaseType caseType = nodeTypeToTestCaseType(testCase->getNodeType());
#if defined(DEQP_LOG_NODE_SOURCE)
    const char *caseSource = testCase->getSource();
#else
    const char *caseSource = nullptr;
#endif
    bool initOk = false;

    print("\nTest case '%s'..\n", casePath.c_str());

#if (DE_OS == DE_OS_WIN32)
    fflush(stdout);
#endif

    m_testCtx.setTestResult(QP_TEST_RESULT_LAST, "");
    m_testCtx.setTerminateAfter(false);
    log.startCase(casePath.c_str(), caseType, caseSource);

    m_isInTestCase  = true;
    m_testStartTime = deGetMicroseconds();

    try
    {
        m_caseExecutor->init(testCase, casePath);
        initOk = true;
    }
    catch (const std::bad_alloc &)
    {
        DE_ASSERT(!initOk);
        m_testCtx.setTestResult(QP_TEST_RESULT_RESOURCE_ERROR, "Failed to allocate memory in test case init");
        m_testCtx.setTerminateAfter(true);
    }
    catch (const tcu::TestException &e)
    {
        DE_ASSERT(!initOk);
        DE_ASSERT(e.getTestResult() != QP_TEST_RESULT_LAST);
        m_testCtx.setTestResult(e.getTestResult(), e.getMessage());
        m_testCtx.setTerminateAfter(e.isFatal());
        log << e;
    }
    catch (const tcu::Exception &e)
    {
        DE_ASSERT(!initOk);
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, e.getMessage());
        log << e;
    }

    DE_ASSERT(initOk || m_testCtx.getTestResult() != QP_TEST_RESULT_LAST);

    return initOk;
}

void TestSessionExecutor::leaveTestCase(TestCase *testCase)
{
    TestLog &log = m_testCtx.getLog();

    // De-init case.
    try
    {
        m_caseExecutor->deinit(testCase);
    }
    catch (const tcu::Exception &e)
    {
        const bool suppressLogging = m_testCtx.getLog().isSupressLogging();

        if (suppressLogging)
            m_testCtx.getLog().supressLogging(false);

        log << e << TestLog::Message << "Error in test case deinit, test program will terminate."
            << TestLog::EndMessage;
        m_testCtx.setTerminateAfter(true);

        m_testCtx.getLog().supressLogging(suppressLogging);
    }

    {
        const int64_t duration = deGetMicroseconds() - m_testStartTime;
        m_testStartTime        = 0;
        m_testCtx.getLog() << TestLog::Integer("TestDuration", "Test case duration in microseconds", "us",
                                               QP_KEY_TAG_TIME, duration);
    }

    {
        const qpTestResult testResult    = m_testCtx.getTestResult();
        const char *const testResultDesc = m_testCtx.getTestResultDesc();
        const bool terminateAfter        = m_testCtx.getTerminateAfter();
        DE_ASSERT(testResult != QP_TEST_RESULT_LAST);

        m_isInTestCase = false;
        m_testCtx.getLog().endCase(testResult, testResultDesc);

        // Update statistics.
        print("  %s (%s)\n", qpGetTestResultName(testResult), testResultDesc);

#if (DE_OS == DE_OS_WIN32)
        fflush(stdout);
#endif
        if (!m_caseExecutor->usesLocalStatus(m_testCtx))
        {
            m_status.numExecuted += 1;
            switch (testResult)
            {
            case QP_TEST_RESULT_PASS:
                m_status.numPassed += 1;
                break;
            case QP_TEST_RESULT_NOT_SUPPORTED:
                m_status.numNotSupported += 1;
                break;
            case QP_TEST_RESULT_QUALITY_WARNING:
                m_status.numWarnings += 1;
                break;
            case QP_TEST_RESULT_COMPATIBILITY_WARNING:
                m_status.numWarnings += 1;
                break;
            case QP_TEST_RESULT_CAPABILITY_WARNING:
                m_status.numWarnings += 1;
                break;
            case QP_TEST_RESULT_WAIVER:
                m_status.numWaived += 1;
                break;
            case QP_TEST_RESULT_DEVICE_LOST:
                m_status.numDeviceLost += 1;
                m_status.numFailed += 1;
                break;
            default:
                m_status.numFailed += 1;
                break;
            }
        }
        else
        {
            m_caseExecutor->updateGlobalStatus(m_status);
        }

        // terminateAfter, Resource error or any error in deinit means that execution should end
        if (terminateAfter || testResult == QP_TEST_RESULT_RESOURCE_ERROR ||
            (m_status.numFailed > 0 && m_testCtx.getCommandLine().isTerminateOnFailEnabled()) ||
            (m_status.numDeviceLost > 0 && m_testCtx.getCommandLine().isTerminateOnDeviceLostEnabled()))

            m_abortSession = true;
    }

    if (m_testCtx.getWatchDog())
        qpWatchDog_reset(m_testCtx.getWatchDog());
}

TestCase::IterateResult TestSessionExecutor::iterateTestCase(TestCase *testCase)
{
    TestLog &log                          = m_testCtx.getLog();
    TestCase::IterateResult iterateResult = TestCase::STOP;

    m_testCtx.touchWatchdog();

    try
    {
        iterateResult = m_caseExecutor->iterate(testCase);
    }
    catch (const std::bad_alloc &)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_RESOURCE_ERROR, "Failed to allocate memory during test execution");
        m_testCtx.setTerminateAfter(true);
    }
    catch (const tcu::TestException &e)
    {
        log << e;
        m_testCtx.setTestResult(e.getTestResult(), e.getMessage());
        m_testCtx.setTerminateAfter(e.isFatal());
    }
    catch (const tcu::Exception &e)
    {
        log << e;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, e.getMessage());
    }

    return iterateResult;
}

} // namespace tcu
