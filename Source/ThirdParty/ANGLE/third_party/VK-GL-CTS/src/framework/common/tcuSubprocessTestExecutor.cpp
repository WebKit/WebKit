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
* \brief Subprocess test executor implementation file.
*//*--------------------------------------------------------------------*/

#include "tcuSubprocessTestExecutor.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "deClock.h"
#include "deStringUtil.hpp"

#include <algorithm>
#include <bitset>
#include <cctype>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string_view>
#include <thread>
#include <regex>
#include <tuple>

namespace fs = std::filesystem;
#define DEBUG_SUBPROCESS 0

namespace tcu
{

SubprocessTestExecutor::SubprocessTestExecutor(TestContext &testCtx)
    : m_testCtx(testCtx)
    , m_subprocessCasesMax(0u)
    , m_subprocessCaseCount(0u)
    , m_waitForSubprocesses(0u)
    , m_prettyPrinting(0u)
    , m_sessionStartTime(deGetMicroseconds())
    , m_subprocessCases()
    , m_subprocesses()
    , m_subprocessFiles()
    , m_sharedMemory("SharedMemory")
{
    if (inSubprocessCaseCount())
    {
#if DEBUG_SUBPROCESS
        std::this_thread::sleep_for(std::chrono::seconds(10));
#endif
        unsigned long error = 0;
        m_sharedMemory.open((m_subprocessCasesMax * sizeof(SharedCase)), error, true);
    }
}

std::pair<std::string, std::string> resolvePath(const char *path)
{
    const auto full = [&]()
    {
        fs::path abs = fs::absolute(path);

        std::error_code ec;
        fs::path canon = fs::weakly_canonical(abs, ec);

        if (!ec)
            return canon;

        return abs;
    }();
    return {full.parent_path().string(), full.filename().string()};
}

uint32_t SubprocessTestExecutor::getSubprocessCaseCount(TestContext &testCtx)
{
    if (m_subprocessCaseCount)
        return m_subprocessCaseCount;

    auto throwException = [&](const char *what)
    {
        std::ostringstream os;
        os << "Unknown \"" << what << "\", please read help about --deqp-device-fault-subprocess-count.";
        TCU_THROW(InternalError, os.str());
    };
    const tcu::CommandLine &cmdLine = testCtx.getCommandLine();
    if (const char *var = cmdLine.getDeviceFaultSubprocessCount(); var[0])
    {
        if (std::isdigit(var[0]))
        {
            const auto vars = de::splitString(var, ',');

            const std::string_view var1(vars[0]);
            auto begin       = var1.data();
            auto end         = begin + var1.length();
            auto [ptr1, ec1] = std::from_chars(begin, end, m_subprocessCaseCount);
            if (!(ec1 == std::errc{} && ptr1 == end))
            {
                m_subprocessCasesMax  = 0u;
                m_subprocessCaseCount = 0u;
                throwException(var);
            }
            if (std::numeric_limits<uint16_t>::max() < m_subprocessCaseCount)
            {
                m_subprocessCasesMax  = 0u;
                m_subprocessCaseCount = 0u;
                throwException(var);
            }

            if (vars.size() > 1u)
            {
                const std::string_view var2(vars[1]);
                begin            = var2.data();
                end              = begin + var2.length();
                auto [ptr2, ec2] = std::from_chars(begin, end, m_waitForSubprocesses);
                if (!(ec2 == std::errc{} && ptr2 == end))
                {
                    m_subprocessCasesMax  = 0u;
                    m_subprocessCaseCount = 0u;
                    m_waitForSubprocesses = 0u;
                    throwException(var);
                }
            }

            if (vars.size() > 2u)
            {
                const std::string_view var3(vars[2]);
                begin            = var3.data();
                end              = begin + var3.length();
                auto [ptr3, ec3] = std::from_chars(begin, end, m_prettyPrinting);
                if (!(ec3 == std::errc{} && ptr3 == end))
                {
                    m_subprocessCasesMax  = 0u;
                    m_subprocessCaseCount = 0u;
                    m_waitForSubprocesses = 0u;
                    m_prettyPrinting      = 0u;
                    throwException(var);
                }
            }
        }
        else
        {
            char c1        = '.';
            char c2        = '.';
            char c3        = '.';
            char c4        = '.';
            uint64_t stamp = 0u;
            std::istringstream str(var);
            if (false == ((str >> std::noskipws >> c1 >> stamp >> c2 >> m_subprocessCasesMax >> c3 >>
                           m_subprocessCaseCount >> c4) ||
                          '@' == c1 || '@' == c2 || '@' == c3 || '@' == c4 || m_sessionStartTime == stamp))
            {
                m_subprocessCaseCount = 0u;
                throwException(var);
            }
        }
    }

    return m_subprocessCaseCount;
}

bool SubprocessTestExecutor::isSubprocessCase(const std::string &casePath, bool checkList, bool groupPath) const
{
    DE_ASSERT(false == groupPath || false == checkList);
    if (checkList)
    {
        return m_subprocessCases.end() != std::find_if(m_subprocessCases.begin(), m_subprocessCases.end(),
                                                       [&](const Item &c) { return casePath == c.first; });
    }
    const std::string_view lookupPath(".postmortem.device_fault");
    const auto pos = casePath.find(lookupPath);
    const bool subprocessCase =
        pos != std::string::npos && (groupPath ? casePath.length() == (pos + lookupPath.size()) : true);
    return subprocessCase;
}

uint32_t SubprocessTestExecutor::inSubprocessCaseCount()
{
    constexpr uint32_t spMax = std::numeric_limits<uint16_t>::max();
    const uint32_t spCount   = getSubprocessCaseCount(m_testCtx);
    return (spCount > spMax) ? (spCount - spMax) : 0u;
}

uint32_t SubprocessTestExecutor::getSubprocessCaseCount()
{
    return getSubprocessCaseCount(m_testCtx);
}

int SubprocessTestExecutor::addSubprocessCase(const std::string &casePath)
{
    if (m_subprocessCaseCount && isSubprocessCase(casePath) && isSubprocessCase(casePath, true) == false)
    {
        m_subprocessCases.emplace_back(casePath, QP_TEST_RESULT_LAST);
        return int(m_subprocessCases.size());
    }
    return (-1);
}

bool SubprocessTestExecutor::updateSubprocessCase(const std::string &casePath, qpTestResult caseResult,
                                                  const std::string &caseDesc, int exitCode)

{
    if (auto pCase = std::find_if(m_subprocessCases.begin(), m_subprocessCases.end(),
                                  [&](const Item &c) { return casePath == c.first; });
        m_subprocessCases.end() != pCase)
    {
        pCase->second = caseResult;
    }

    const int index = m_sharedMemory.find(casePath);
    if (index >= 0)
    {
        const SharedCase::Helper newCase(casePath, exitCode, caseResult, caseDesc);
        return m_sharedMemory.write(newCase, uint32_t(index));
    }
    return false;
}

std::string makeCommandLine(const tcu::CommandLine &srcLine, uint64_t stamp,
                            const std::vector<SubprocessTestExecutor::Item> &casePaths, std::string &processFile,
                            uint32_t first, uint32_t count, uint32_t rem, uint32_t casesMax)
{
    const auto [logDir, _] = resolvePath(srcLine.getLogFileName());

    std::ostringstream logFileName;
    logFileName << "device_fault";
    logFileName << '_' << (first / count) << '_' << count;
    logFileName << '_' << (rem ? rem : count) << ".qpa";
    processFile = logFileName.str();
    const fs::path logFilePath(fs::path(logDir) / processFile);

    std::ostringstream marker;
    marker << '@' << stamp << '@' << casesMax << '@' << (std::numeric_limits<uint16_t>::max() + (rem ? rem : count))
           << '@';

    std::string cmdLine = srcLine.getInitialCmdLine();
    const std::regex re_n(R"(-n\s+(\S+))");
    const std::regex re_case(R"(--deqp-case=([^\s]+))");
    const std::regex re_caseList(R"(--deqp-caselist=([^\s]+))");
    const std::regex re_caseListFile(R"(--deqp-caselist-file=([^\s]+))");
    const std::regex re_caseListResource(R"(--deqp-caselist-resource=([^\s]+))");
    const std::regex re_caseListStdIn(R"(--deqp-stdin-caselist=([^\s]+))");
    const std::regex re_logFileName(R"(--deqp-log-filename=([^\s]+))");
    const std::regex re_subprocessCount(R"(--deqp-device-fault-subprocess-count=([^\s]+))");
    const std::regex *res[]{
        &re_n,           &re_case,           &re_caseList, &re_caseListFile, &re_caseListResource, &re_caseListStdIn,
        &re_logFileName, &re_subprocessCount};
    for (const std::regex *re : res)
    {
        cmdLine = std::regex_replace(cmdLine, *re, "");
    }
    const std::string appName = srcLine.getApplicationName() + ' ';
    cmdLine.insert(0, appName);
    cmdLine += (" --deqp-case=");
    for (uint32_t k = 0u; k < count; ++k)
    {
        if (k)
            cmdLine += ',';
        cmdLine += casePaths[first + k].first;
    }
    cmdLine += (" --deqp-log-filename=\"" + logFilePath.string() + '\"');
    cmdLine += (" --deqp-device-fault-subprocess-count=" + marker.str());

    return cmdLine;
}

void SubprocessTestExecutor::spawnSubprocessCases()
{
    if (0u == m_subprocessCaseCount || std::numeric_limits<uint16_t>::max() < m_subprocessCaseCount)
    {
        return;
    }

    const uint32_t casesMax = uint32_t(m_subprocessCases.size());
    const uint32_t intCount = uint32_t(casesMax / m_subprocessCaseCount);
    const uint32_t rem      = uint32_t(casesMax % m_subprocessCaseCount);

    std::string processFile;
    const uint32_t subprocessCount = (casesMax + m_subprocessCaseCount - 1u) / m_subprocessCaseCount;
    m_subprocessFiles.reserve(subprocessCount);
    m_subprocesses.reserve(subprocessCount);

    unsigned long error = 0;
    m_sharedMemory.allocate((casesMax * sizeof(SharedCase)), error, true);
    for (uint32_t i = 0u; i < casesMax; ++i)
    {
        m_sharedMemory.write(
            SharedCase::Helper(m_subprocessCases[i].first, int(i + 1), QP_TEST_RESULT_LAST, m_sharedMemory.name), i);
#if DEBUG_SUBPROCESS
        error = (unsigned long)m_sharedMemory.find(m_subprocessCases[i].first);
        DE_ASSERT(error == i);
#endif
    }

    for (uint32_t i = 0u; i < intCount; ++i)
    {
        const std::string cmdLine =
            makeCommandLine(m_testCtx.getCommandLine(), m_sessionStartTime, m_subprocessCases, processFile,
                            (i * m_subprocessCaseCount), m_subprocessCaseCount, 0u, casesMax);
        deProcess *p = deProcess_create();
        if (deProcess_start(p, cmdLine.c_str(), "."))
        {
            m_subprocesses.emplace_back((i * m_subprocessCaseCount), m_subprocessCaseCount, p);
            m_subprocessFiles.emplace_back(processFile);

            if (m_waitForSubprocesses)
            {
                waitForSubprocesses(i, 1u);
            }
        }
    }
    if (rem)
    {
        const std::string cmdLine =
            makeCommandLine(m_testCtx.getCommandLine(), m_sessionStartTime, m_subprocessCases, processFile,
                            (intCount * m_subprocessCaseCount), rem, m_subprocessCaseCount, casesMax);
        deProcess *p = deProcess_create();
        if (deProcess_start(p, cmdLine.c_str(), "."))
        {
            m_subprocesses.emplace_back((intCount * m_subprocessCaseCount), rem, p);
            m_subprocessFiles.emplace_back(processFile);

            if (m_waitForSubprocesses)
            {
                waitForSubprocesses(intCount, 1u);
            }
        }
    }

    if (0u == m_waitForSubprocesses)
    {
        waitForSubprocesses(0u, uint32_t(m_subprocesses.size()));
    }
}

uint32_t SubprocessTestExecutor::updateRunStatus(TestRunStatus &runStatus)
{
    for (const Item &i : m_subprocessCases)
    {
        switch (i.second)
        {
        case QP_TEST_RESULT_PASS:
            runStatus.numPassed += 1;
            runStatus.numExecuted += 1;
            break;
        case QP_TEST_RESULT_FAIL:
            runStatus.numFailed += 1;
            runStatus.numExecuted += 1;
            break;
        case QP_TEST_RESULT_DEVICE_LOST:
            runStatus.numDeviceLost += 1;
            runStatus.numExecuted += 1;
            break;
        case QP_TEST_RESULT_NOT_SUPPORTED:
            runStatus.numNotSupported += 1;
            runStatus.numExecuted += 1;
            break;
        case QP_TEST_RESULT_QUALITY_WARNING:
            runStatus.numWarnings += 1;
            runStatus.numExecuted += 1;
            break;
        case QP_TEST_RESULT_WAIVER:
            runStatus.numWaived += 1;
            runStatus.numExecuted += 1;
            break;
        default:
            break;
        }
    }
    return uint32_t(m_subprocessCases.size());
}

SubprocessTestExecutor::Subprocess::Subprocess(uint32_t firstCase, uint32_t caseCount, deProcess *process)
    : m_firstCase(firstCase)
    , m_caseCount(caseCount)
    , m_process(process)
    , m_exitCode(999)
{
}

SubprocessTestExecutor::Subprocess::~Subprocess()
{
    if (m_process)
    {
        deProcess_destroy(m_process);
        m_process = nullptr;
    }
}

bool SubprocessTestExecutor::Subprocess::isRunning(bool freeIfNotRunning)
{
    bool is = false;
    if (nullptr != m_process)
    {
        is         = deProcess_isRunning(m_process);
        m_exitCode = deProcess_getExitCode(m_process);
        if (false == is && freeIfNotRunning)
        {
            deProcess_destroy(m_process);
            m_process = nullptr;
        }
    }
    return is;
}

int SubprocessTestExecutor::Subprocess::getExitCode() const
{
    return m_exitCode;
}
bool SubprocessTestExecutor::Subprocess::hasProcess() const
{
    return m_process != nullptr;
}

uint32_t SubprocessTestExecutor::Subprocess::getFirstCase() const
{
    return m_firstCase;
}

uint32_t SubprocessTestExecutor::Subprocess::getCaseCount() const
{
    return m_caseCount;
}

void SubprocessTestExecutor::waitForSubprocesses(const uint32_t start, const uint32_t count)
{
#if DEBUG_SUBPROCESS
    std::this_thread::sleep_for(std::chrono::minutes(3));
#endif

    auto isRunning = [](const int status) { return status != 0; };

    std::vector<int> statuses(count);
    std::iota(statuses.begin(), statuses.end(), start);
    std::transform(statuses.begin(), statuses.end(), statuses.begin(),
                   [this](const int index) { return m_subprocesses[uint32_t(index)].hasProcess() ? 1 : 0; });
    do
    {
        for (uint32_t i = 0u; i < count; ++i)
        {
            const uint32_t subprocessIndex = i + start;
            const int status               = m_subprocesses[subprocessIndex].isRunning(true) ? 1 : 0;
            if (status == 0 && statuses[i])
            {
                for (uint32_t caseNum = 0u; caseNum < m_subprocesses[subprocessIndex].getCaseCount(); ++caseNum)
                {
                    const uint32_t caseIndex = m_subprocesses[subprocessIndex].getFirstCase() + caseNum;
                    m_sharedMemory.m_data[caseIndex].exitCode = m_subprocesses[subprocessIndex].getExitCode();
                    const SharedCase::Helper c                = m_sharedMemory.m_data[caseIndex]();
                    m_subprocessCases[caseIndex].second       = c.caseResult;

                    if (m_prettyPrinting > 1u)
                    {
                        print("  %s (%s, \"%s\", exitCode=0x%x, file=%s)\n", qpGetTestResultName(c.caseResult),
                              c.casePath.c_str(), c.caseDesc.c_str(), c.exitCode,
                              m_subprocessFiles[subprocessIndex].c_str());
                    }
                    else if (m_prettyPrinting > 0u)
                    {
                        print("  %s (%s, \"%s\", exitCode=0x%x)\n", qpGetTestResultName(c.caseResult),
                              c.casePath.c_str(), c.caseDesc.c_str(), c.exitCode);
                    }
                    else
                    {
                        print("  %s (%s, \"%s\")\n", qpGetTestResultName(c.caseResult), c.casePath.c_str(),
                              c.caseDesc.c_str());
                    }
#if (DE_OS == DE_OS_WIN32)
                    fflush(stdout);
#endif
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            statuses[i] = status;
        }
    } while (std::any_of(statuses.begin(), statuses.end(), isRunning));
}

SubprocessTestExecutor::SharedMemory::~SharedMemory()
{
    close();
}

SubprocessTestExecutor::SharedMemory::SharedMemory(const std::string &name_)
    : name(name_)
    , m_state()
    , m_size(0)
    , m_handle(nullptr)
    , m_data(nullptr)
{
}

size_t SubprocessTestExecutor::SharedMemory::getSize() const
{
    return m_size;
}

int SubprocessTestExecutor::SharedMemory::find(const std::string &casePath) const
{
    DE_ASSERT(m_state);
    const int count = int(m_size / sizeof(SharedCase));
    for (int i = 0; i < count; ++i)
    {
        std::string vCasePath(sizeof(SharedCase::casePath), ' ');
        const auto wr = std::snprintf(vCasePath.data(), sizeof(SharedCase::casePath), "%s", m_data[i].casePath);
        vCasePath.resize(wr);
        if (vCasePath == casePath)
            return i;
    }
    return (-1);
}

SubprocessTestExecutor::SharedCase::Helper::Helper(const std::string &casePath_, int exitCode_,
                                                   qpTestResult caseResult_, const std::string &caseDesc_)
    : exitCode(exitCode_)
    , caseResult(caseResult_)
    , casePath(casePath_)
    , caseDesc(caseDesc_)
{
}

SubprocessTestExecutor::SharedCase::SharedCase()
{
    exitCode   = 1;
    caseResult = QP_TEST_RESULT_LAST;
    std::fill(std::begin(casePath), std::end(casePath), '\0');
    std::fill(std::begin(caseDesc), std::end(caseDesc), '\0');
}

SubprocessTestExecutor::SharedCase::SharedCase(const Helper &helper) : SubprocessTestExecutor::SharedCase::SharedCase()
{
    exitCode   = helper.exitCode;
    caseResult = helper.caseResult;
    std::snprintf(casePath, sizeof(casePath), "%s", helper.casePath.c_str());
    std::snprintf(caseDesc, sizeof(caseDesc), "%s", helper.caseDesc.c_str());
}

SubprocessTestExecutor::SharedCase::Helper SubprocessTestExecutor::SharedCase::operator()() const
{
    std::string vCasePath(sizeof(casePath), ' ');
    std::string vCaseDesc(sizeof(caseDesc), ' ');
    const auto pathLen = std::snprintf(&vCasePath[0], sizeof(casePath), "%s", casePath);
    const auto descLen = std::snprintf(&vCaseDesc[0], sizeof(caseDesc), "%s", caseDesc);
    vCasePath.resize(pathLen);
    vCaseDesc.resize(descLen);
    return Helper(vCasePath, exitCode, caseResult, vCaseDesc);
}

bool SubprocessTestExecutor::SharedMemory::write(const SharedCase::Helper &aCase, uint32_t atIndex)
{
    DE_ASSERT(m_state);
    DE_ASSERT(atIndex < (m_size / sizeof(SharedCase)));
    const SharedCase::Helper oldCase = read(atIndex);
    if (oldCase.casePath.empty() || oldCase.casePath == aCase.casePath)
    {
        m_data[atIndex] = SharedCase(aCase);
        return true;
    }
    return false;
}

SubprocessTestExecutor::SharedCase::Helper SubprocessTestExecutor::SharedMemory::read(uint32_t atIndex)
{
    DE_ASSERT(m_state);
    DE_ASSERT(atIndex < (m_size / sizeof(SharedCase)));
    return m_data[atIndex]();
}

} // namespace tcu
