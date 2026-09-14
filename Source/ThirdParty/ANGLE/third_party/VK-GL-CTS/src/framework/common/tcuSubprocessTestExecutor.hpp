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
    SubprocessTestExecutor(TestContext &testCtx);
    ~SubprocessTestExecutor() = default;
    bool isSubprocessCase(const std::string &casePath, bool checkList = false, bool groupPath = false) const;
    bool updateSubprocessCase(const std::string &casePath, qpTestResult caseResult, const std::string &caseDesc,
                              int exitCode);
    int addSubprocessCase(const std::string &casePath);
    uint32_t getSubprocessCaseCount();
    uint32_t inSubprocessCaseCount();
    void spawnSubprocessCases();
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
        char casePath[256];
        char caseDesc[256];
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
};

} // namespace tcu

#endif // _TCUSUBPROCESSTESTEXECUTOR_HPP
