#ifndef _XEBATCHEXECUTOR_HPP
#define _XEBATCHEXECUTOR_HPP
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

#include "xeDefs.hpp"
#include "xeBatchResult.hpp"
#include "xeCommLink.hpp"
#include "xeTestLogParser.hpp"
#include "xeCallQueue.hpp"

#include <string>
#include <vector>

namespace xe
{

struct TargetConfiguration
{
    TargetConfiguration(void) : maxCasesPerSession(1000)
    {
    }

    std::string binaryName;
    std::string workingDir;
    std::string cmdLineArgs;
    int maxCasesPerSession;
};

class BatchExecutorLogHandler : public TestLogHandler
{
public:
    BatchExecutorLogHandler(BatchResult *batchResult);
    ~BatchExecutorLogHandler(void);

    void setSessionInfo(const SessionInfo &sessionInfo);

    TestCaseResultPtr startTestCaseResult(const char *casePath);
    void testCaseResultUpdated(const TestCaseResultPtr &resultData);
    void testCaseResultComplete(const TestCaseResultPtr &resultData);

private:
    BatchResult *m_batchResult;
};

class BatchExecutor
{
public:
    BatchExecutor(const TargetConfiguration &config, CommLink *commLink, const TestNode *root, const TestSet &testSet,
                  BatchResult *batchResult, InfoLog *infoLog);
    ~BatchExecutor(void);

    void run(void);
    void cancel(void); //!< Cancel current run(), can be called from any thread.

private:
    BatchExecutor(const BatchExecutor &other);
    BatchExecutor &operator=(const BatchExecutor &other);

    bool iterate(void);

    void onStateChanged(CommLinkState state, const char *message);
    void onTestLogData(const uint8_t *bytes, size_t numBytes);
    void onInfoLogData(const uint8_t *bytes, size_t numBytes);

    void launchTestSet(const TestSet &testSet);

    // Callbacks for CommLink.
    static void enqueueStateChanged(void *userPtr, CommLinkState state, const char *message);
    static void enqueueTestLogData(void *userPtr, const uint8_t *bytes, size_t numBytes);
    static void enqueueInfoLogData(void *userPtr, const uint8_t *bytes, size_t numBytes);

    // Called in CallQueue dispatch.
    static void dispatchStateChanged(CallReader &data);
    static void dispatchTestLogData(CallReader &data);
    static void dispatchInfoLogData(CallReader &data);

    enum State
    {
        STATE_NOT_STARTED,
        STATE_STARTED,
        STATE_FINISHED,

        STATE_LAST
    };

    TargetConfiguration m_config;
    CommLink *m_commLink;

    const TestNode *m_root;
    const TestSet &m_testSet;

    BatchExecutorLogHandler m_logHandler;
    BatchResult *m_batchResult;
    InfoLog *m_infoLog;

    State m_state;
    TestSet m_casesToExecute;

    TestLogParser m_testLogParser;

    CallQueue m_dispatcher;
};

} // namespace xe

#endif // _XEBATCHEXECUTOR_HPP
