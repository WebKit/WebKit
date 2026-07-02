/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief Test batch executor.
 *//*--------------------------------------------------------------------*/

#include "xeBatchExecutor.hpp"
#include "xeTestResultParser.hpp"

#include <sstream>
#include <cstdio>

namespace xe
{

using std::string;
using std::vector;

enum
{
    TEST_LOG_TMP_BUFFER_SIZE = 1024,
    INFO_LOG_TMP_BUFFER_SIZE = 256
};

// \todo [2012-11-01 pyry] Update execute set in handler.

static inline bool isExecutedInBatch(const BatchResult *batchResult, const TestCase *testCase)
{
    std::string fullPath;
    testCase->getFullPath(fullPath);

    if (batchResult->hasTestCaseResult(fullPath.c_str()))
    {
        ConstTestCaseResultPtr data = batchResult->getTestCaseResult(fullPath.c_str());
        return data->getStatusCode() != TESTSTATUSCODE_PENDING && data->getStatusCode() != TESTSTATUSCODE_RUNNING;
    }
    else
        return false;
}

// \todo [2012-06-19 pyry] These can be optimized using TestSetIterator (once implemented)

static void computeExecuteSet(TestSet &executeSet, const TestNode *root, const TestSet &testSet,
                              const BatchResult *batchResult)
{
    ConstTestNodeIterator iter = ConstTestNodeIterator::begin(root);
    ConstTestNodeIterator end  = ConstTestNodeIterator::end(root);

    for (; iter != end; ++iter)
    {
        const TestNode *node = *iter;

        if (node->getNodeType() == TESTNODETYPE_TEST_CASE && testSet.hasNode(node))
        {
            const TestCase *testCase = static_cast<const TestCase *>(node);

            if (!isExecutedInBatch(batchResult, testCase))
                executeSet.addCase(testCase);
        }
    }
}

static void computeBatchRequest(TestSet &requestSet, const TestSet &executeSet, const TestNode *root, int maxCasesInSet)
{
    ConstTestNodeIterator iter = ConstTestNodeIterator::begin(root);
    ConstTestNodeIterator end  = ConstTestNodeIterator::end(root);
    int numCases               = 0;

    for (; (iter != end) && (numCases < maxCasesInSet); ++iter)
    {
        const TestNode *node = *iter;

        if (node->getNodeType() == TESTNODETYPE_TEST_CASE && executeSet.hasNode(node))
        {
            const TestCase *testCase = static_cast<const TestCase *>(node);
            requestSet.addCase(testCase);
            numCases += 1;
        }
    }
}

static int removeExecuted(TestSet &set, const TestNode *root, const BatchResult *batchResult)
{
    TestSet oldSet(set);
    ConstTestNodeIterator iter = ConstTestNodeIterator::begin(root);
    ConstTestNodeIterator end  = ConstTestNodeIterator::end(root);
    int numRemoved             = 0;

    for (; iter != end; ++iter)
    {
        const TestNode *node = *iter;

        if (node->getNodeType() == TESTNODETYPE_TEST_CASE && oldSet.hasNode(node))
        {
            const TestCase *testCase = static_cast<const TestCase *>(node);

            if (isExecutedInBatch(batchResult, testCase))
            {
                set.removeCase(testCase);
                numRemoved += 1;
            }
        }
    }

    return numRemoved;
}

BatchExecutorLogHandler::BatchExecutorLogHandler(BatchResult *batchResult) : m_batchResult(batchResult)
{
}

BatchExecutorLogHandler::~BatchExecutorLogHandler(void)
{
}

void BatchExecutorLogHandler::setSessionInfo(const SessionInfo &sessionInfo)
{
    m_batchResult->getSessionInfo() = sessionInfo;
}

TestCaseResultPtr BatchExecutorLogHandler::startTestCaseResult(const char *casePath)
{
    // \todo [2012-11-01 pyry] What to do with duplicate results?
    if (m_batchResult->hasTestCaseResult(casePath))
        return m_batchResult->getTestCaseResult(casePath);
    else
        return m_batchResult->createTestCaseResult(casePath);
}

void BatchExecutorLogHandler::testCaseResultUpdated(const TestCaseResultPtr &)
{
}

void BatchExecutorLogHandler::testCaseResultComplete(const TestCaseResultPtr &result)
{
    // \todo [2012-11-01 pyry] Remove from execute set here instead of updating it between sessions.
    printf("%s\n", result->getTestCasePath());
}

BatchExecutor::BatchExecutor(const TargetConfiguration &config, CommLink *commLink, const TestNode *root,
                             const TestSet &testSet, BatchResult *batchResult, InfoLog *infoLog)
    : m_config(config)
    , m_commLink(commLink)
    , m_root(root)
    , m_testSet(testSet)
    , m_logHandler(batchResult)
    , m_batchResult(batchResult)
    , m_infoLog(infoLog)
    , m_state(STATE_NOT_STARTED)
    , m_testLogParser(&m_logHandler)
{
}

BatchExecutor::~BatchExecutor(void)
{
}

void BatchExecutor::run(void)
{
    XE_CHECK(m_state == STATE_NOT_STARTED);

    // Check commlink state.
    {
        CommLinkState commState = COMMLINKSTATE_LAST;
        std::string stateStr    = "";

        commState = m_commLink->getState(stateStr);

        if (commState == COMMLINKSTATE_ERROR)
        {
            // Report error.
            XE_FAIL((string("CommLink error: '") + stateStr + "'").c_str());
        }
        else if (commState != COMMLINKSTATE_READY)
            XE_FAIL("CommLink is not ready");
    }

    // Compute initial execute set.
    computeExecuteSet(m_casesToExecute, m_root, m_testSet, m_batchResult);

    // Register callbacks.
    m_commLink->setCallbacks(enqueueStateChanged, enqueueTestLogData, enqueueInfoLogData, this);

    try
    {
        if (!m_casesToExecute.empty())
        {
            TestSet batchRequest;
            computeBatchRequest(batchRequest, m_casesToExecute, m_root, m_config.maxCasesPerSession);
            launchTestSet(batchRequest);

            m_state = STATE_STARTED;
        }
        else
            m_state = STATE_FINISHED;

        // Run handler loop until we are finished.
        while (m_state != STATE_FINISHED)
            m_dispatcher.callNext();
    }
    catch (...)
    {
        m_commLink->setCallbacks(nullptr, nullptr, nullptr, nullptr);
        throw;
    }

    // De-register callbacks.
    m_commLink->setCallbacks(nullptr, nullptr, nullptr, nullptr);
}

void BatchExecutor::cancel(void)
{
    m_state = STATE_FINISHED;
    m_dispatcher.cancel();
}

void BatchExecutor::onStateChanged(CommLinkState state, const char *message)
{
    switch (state)
    {
    case COMMLINKSTATE_READY:
    case COMMLINKSTATE_TEST_PROCESS_LAUNCHING:
    case COMMLINKSTATE_TEST_PROCESS_RUNNING:
        break; // Ignore.

    case COMMLINKSTATE_TEST_PROCESS_FINISHED:
    {
        // Feed end of string to parser. This terminates open test case if such exists.
        {
            uint8_t eos = 0;
            onTestLogData(&eos, 1);
        }

        int numExecuted = removeExecuted(m_casesToExecute, m_root, m_batchResult);

        // \note No new batch is launched if no cases were executed in last one. Otherwise excutor
        //       could end up in infinite loop.
        if (!m_casesToExecute.empty() && numExecuted > 0)
        {
            // Reset state and start batch.
            m_testLogParser.reset();

            m_commLink->reset();
            XE_CHECK(m_commLink->getState() == COMMLINKSTATE_READY);

            TestSet batchRequest;
            computeBatchRequest(batchRequest, m_casesToExecute, m_root, m_config.maxCasesPerSession);
            launchTestSet(batchRequest);
        }
        else
            m_state = STATE_FINISHED;

        break;
    }

    case COMMLINKSTATE_TEST_PROCESS_LAUNCH_FAILED:
        printf("Failed to start test process: '%s'\n", message);
        m_state = STATE_FINISHED;
        break;

    case COMMLINKSTATE_ERROR:
        printf("CommLink error: '%s'\n", message);
        m_state = STATE_FINISHED;
        break;

    default:
        XE_FAIL("Unknown state");
    }
}

void BatchExecutor::onTestLogData(const uint8_t *bytes, size_t numBytes)
{
    try
    {
        m_testLogParser.parse(bytes, numBytes);
    }
    catch (const ParseError &e)
    {
        // \todo [2012-07-06 pyry] Log error.
        DE_UNREF(e);
    }
}

void BatchExecutor::onInfoLogData(const uint8_t *bytes, size_t numBytes)
{
    if (numBytes > 0 && m_infoLog)
        m_infoLog->append(bytes, numBytes);
}

static void writeCaseListNode(std::ostream &str, const TestNode *node, const TestSet &testSet)
{
    DE_ASSERT(testSet.hasNode(node));

    TestNodeType nodeType = node->getNodeType();

    if (nodeType != TESTNODETYPE_ROOT)
        str << node->getName();

    if (nodeType == TESTNODETYPE_ROOT || nodeType == TESTNODETYPE_GROUP)
    {
        const TestGroup *group = static_cast<const TestGroup *>(node);
        bool isFirst           = true;

        str << "{";

        for (int ndx = 0; ndx < group->getNumChildren(); ndx++)
        {
            const TestNode *child = group->getChild(ndx);

            if (testSet.hasNode(child))
            {
                if (!isFirst)
                    str << ",";

                writeCaseListNode(str, child, testSet);
                isFirst = false;
            }
        }

        str << "}";
    }
}

void BatchExecutor::launchTestSet(const TestSet &testSet)
{
    std::ostringstream caseList;
    XE_CHECK(testSet.hasNode(m_root));
    XE_CHECK(m_root->getNodeType() == TESTNODETYPE_ROOT);
    writeCaseListNode(caseList, m_root, testSet);

    m_commLink->startTestProcess(m_config.binaryName.c_str(), m_config.cmdLineArgs.c_str(), m_config.workingDir.c_str(),
                                 caseList.str().c_str());
}

void BatchExecutor::enqueueStateChanged(void *userPtr, CommLinkState state, const char *message)
{
    BatchExecutor *executor = static_cast<BatchExecutor *>(userPtr);
    CallWriter writer(&executor->m_dispatcher, BatchExecutor::dispatchStateChanged);

    writer << executor << state << message;

    writer.enqueue();
}

void BatchExecutor::enqueueTestLogData(void *userPtr, const uint8_t *bytes, size_t numBytes)
{
    BatchExecutor *executor = static_cast<BatchExecutor *>(userPtr);
    CallWriter writer(&executor->m_dispatcher, BatchExecutor::dispatchTestLogData);

    writer << executor << numBytes;

    writer.write(bytes, numBytes);
    writer.enqueue();
}

void BatchExecutor::enqueueInfoLogData(void *userPtr, const uint8_t *bytes, size_t numBytes)
{
    BatchExecutor *executor = static_cast<BatchExecutor *>(userPtr);
    CallWriter writer(&executor->m_dispatcher, BatchExecutor::dispatchInfoLogData);

    writer << executor << numBytes;

    writer.write(bytes, numBytes);
    writer.enqueue();
}

void BatchExecutor::dispatchStateChanged(CallReader &data)
{
    BatchExecutor *executor = nullptr;
    CommLinkState state     = COMMLINKSTATE_LAST;
    std::string message;

    data >> executor >> state >> message;

    executor->onStateChanged(state, message.c_str());
}

void BatchExecutor::dispatchTestLogData(CallReader &data)
{
    BatchExecutor *executor = nullptr;
    size_t numBytes;

    data >> executor >> numBytes;

    executor->onTestLogData(data.getDataBlock(numBytes), numBytes);
}

void BatchExecutor::dispatchInfoLogData(CallReader &data)
{
    BatchExecutor *executor = nullptr;
    size_t numBytes;

    data >> executor >> numBytes;

    executor->onInfoLogData(data.getDataBlock(numBytes), numBytes);
}

} // namespace xe
