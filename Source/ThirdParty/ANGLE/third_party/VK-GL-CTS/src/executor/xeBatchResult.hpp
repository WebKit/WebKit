#ifndef _XEBATCHRESULT_HPP
#define _XEBATCHRESULT_HPP
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
 * \brief Test batch result.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "xeTestCase.hpp"
#include "xeTestCaseResult.hpp"
#include "deSharedPtr.hpp"

#include <string>
#include <vector>
#include <map>

namespace xe
{

class SessionInfo
{
public:
    // Produced by test binary.
    std::string releaseName;
    std::string releaseId;
    std::string targetName;
    std::string qpaCommandLineParameters;

    // Produced by Candy.
    std::string candyTargetName;
    std::string configName;
    std::string resultName;
    std::string timestamp;
};

class InfoLog
{
public:
    InfoLog(void);

    size_t getSize(void) const
    {
        return m_data.size();
    }
    const uint8_t *getBytes(void) const
    {
        return !m_data.empty() ? &m_data[0] : nullptr;
    }

    void append(const uint8_t *bytes, size_t numBytes);

private:
    InfoLog(const InfoLog &other);
    InfoLog &operator=(const InfoLog &other);

    std::vector<uint8_t> m_data;
};

class TestCaseResultData
{
public:
    TestCaseResultData(const char *casePath);
    ~TestCaseResultData(void);

    const char *getTestCasePath(void) const
    {
        return m_casePath.c_str();
    }

    void setTestResult(TestStatusCode code, const char *details);

    TestStatusCode getStatusCode(void) const
    {
        return m_statusCode;
    }
    const char *getStatusDetails(void) const
    {
        return m_statusDetails.c_str();
    }

    int getDataSize(void) const
    {
        return (int)m_data.size();
    }
    void setDataSize(int size)
    {
        m_data.resize(size);
    }

    const uint8_t *getData(void) const
    {
        return !m_data.empty() ? &m_data[0] : nullptr;
    }
    uint8_t *getData(void)
    {
        return !m_data.empty() ? &m_data[0] : nullptr;
    }

    void clear(void);

private:
    // \note statusCode and statusDetails are either set by BatchExecutor or later parsed from data.
    std::string m_casePath;
    TestStatusCode m_statusCode;
    std::string m_statusDetails;
    std::vector<uint8_t> m_data;
};

typedef de::SharedPtr<TestCaseResultData> TestCaseResultPtr;
typedef de::SharedPtr<const TestCaseResultData> ConstTestCaseResultPtr;

class BatchResult
{
public:
    BatchResult(void);
    ~BatchResult(void);

    const SessionInfo &getSessionInfo(void) const
    {
        return m_sessionInfo;
    }
    SessionInfo &getSessionInfo(void)
    {
        return m_sessionInfo;
    }

    int getNumTestCaseResults(void) const
    {
        return (int)m_testCaseResults.size();
    }
    ConstTestCaseResultPtr getTestCaseResult(int ndx) const
    {
        return ConstTestCaseResultPtr(m_testCaseResults[ndx]);
    }
    TestCaseResultPtr getTestCaseResult(int ndx)
    {
        return m_testCaseResults[ndx];
    }

    bool hasTestCaseResult(const char *casePath) const;
    ConstTestCaseResultPtr getTestCaseResult(const char *casePath) const;
    TestCaseResultPtr getTestCaseResult(const char *casePath);

    TestCaseResultPtr createTestCaseResult(const char *casePath);

private:
    BatchResult(const BatchResult &other);
    BatchResult &operator=(const BatchResult &other);

    SessionInfo m_sessionInfo;
    std::vector<TestCaseResultPtr> m_testCaseResults;
    std::map<std::string, int> m_resultMap;
};

} // namespace xe

#endif // _XEBATCHRESULT_HPP
