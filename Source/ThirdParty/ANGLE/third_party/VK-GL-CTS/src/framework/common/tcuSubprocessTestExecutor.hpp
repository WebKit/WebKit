#ifndef _TCUSUBPROCESSTESTEXECUTOR_HPP
#define _TCUSUBPROCESSTESTEXECUTOR_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
* \brief Subprocess test executor header file.
*//*--------------------------------------------------------------------*/

#include "tcuTestContext.hpp"
#include "tcuTestPackage.hpp"
#include "tcuTestCase.hpp"
#include "tcuMaybe.hpp"
#include "deProcess.hpp"

#include <vector>

namespace tcu
{

struct SubprocessTestExecutor
{
    /*
    1. The group path MUST NOT END WITH A DOT!
    ------------------------------------------
        When CTS terminates individual tests in a given group, it repeatedly
        calls the TestSessionExecutor::leaveTestGroup(casePath) method, removing
        the last part of the test name following the dot in each call.
        If the path of the test being run matches the group path name, it is treated
        as a candidate for execution in the subprocess. Only when the name matches
        the group path name and ends without any characters after it, the list
        of collected candidates is passed on to be executed in the subprocesses.
    2. The group path MUST BE UNIQUE!
    ---------------------------------
        The group path must be unique among all the test paths run in subprocesses.
        The program matches names by looking for the name from the beginning, so if
        the group name is part of another test name being run, it will be misinterpreted
        as the name of a group of tests to be run in subprocesses.
    */
    SubprocessTestExecutor(TestContext &testCtx, const std::vector<std::string> &groupPaths);
    ~SubprocessTestExecutor() = default;
    bool isSubprocessCase(const std::string &casePath, bool checkList = false, bool groupPath = false) const;
    bool updateSubprocessCase(const std::string &casePath, qpTestResult caseResult, const std::string &caseDesc,
                              int exitCode);
    int addSubprocessCase(const std::string &casePath);
    uint32_t getSubprocessCaseCount();
    uint32_t inSubprocessCaseCount();
    void spawnSubprocessCases(const std::string &groupPath);
    uint32_t updateRunStatus(TestRunStatus &runStatus);

    struct Case
    {
        Case(const std::string &casePath_) : casePath(casePath_)
        {
        }
        const std::string casePath;
    };

    using Item = std::pair<std::string, qpTestResult>;

    struct Subprocess
    {
        Subprocess(uint32_t firstCase, uint32_t caseCount, deProcess *process);
        ~Subprocess();

        inline uint32_t getFirstCase() const;
        inline uint32_t getCaseCount() const;
        inline int getExitCode() const;
        inline bool hasProcess() const;
        bool isRunning(bool freeIfNotRunning);

    private:
        const uint32_t m_firstCase;
        const uint32_t m_caseCount;
        deProcess *m_process;
        int m_exitCode;
    };

    struct SharedCase
    {
        struct Helper
        {
            const int exitCode;
            const qpTestResult caseResult;
            const std::string casePath;
            const std::string caseDesc;
            Helper(const std::string &casePath_, int exitCode_ = 1, qpTestResult caseResult_ = QP_TEST_RESULT_LAST,
                   const std::string &caseDesc_ = std::string());
        };

        SharedCase();
        SharedCase(const Helper &helper);
        Helper operator()() const;

        int exitCode;
        qpTestResult caseResult;
        // Some vk-gl-cts case names exceed 256 characters; keep enough headroom that truncation stays
        // a theoretical concern (checked with DE_ASSERT in the constructor) rather than an expected one.
        char casePath[512];
        char caseDesc[512];
    };

    struct SharedMemory
    {
        friend struct SubprocessTestExecutor;
        const std::string name;
        ~SharedMemory();
        SharedMemory(const std::string &name_);
        bool allocate(size_t size, unsigned long &error, bool raise = false);
        bool open(size_t size, unsigned long &error, bool raise = false);
        bool write(const SharedCase::Helper &aCase, uint32_t atIndex);
        auto read(uint32_t atIndex) -> SharedCase::Helper;
        int find(const std::string &casePath) const;
        size_t getSize() const;
        void close();

    private:
        tcu::Maybe<bool> m_state;
        size_t m_size;
        void *m_handle;
        SharedCase *m_data;
    };

private:
    uint32_t getSubprocessCaseCount(TestContext &testCtx);
    void waitForSubprocesses(const uint32_t start, const uint32_t count);

    TestContext &m_testCtx;
    uint32_t m_subprocessCasesMax;
    uint32_t m_subprocessCaseCount;
    uint32_t m_waitForSubprocesses;
    uint32_t m_prettyPrinting;
    const uint64_t m_sessionStartTime;
    std::vector<Item> m_subprocessCases;
    std::vector<Subprocess> m_subprocesses;
    std::vector<std::string> m_subprocessFiles;
    SharedMemory m_sharedMemory;
    const std::vector<std::string> m_groupPaths;
};

} // namespace tcu

#endif // _TCUSUBPROCESSTESTEXECUTOR_HPP
