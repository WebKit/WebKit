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

#include "xeBatchResult.hpp"
#include "deMemory.h"

using std::map;
using std::string;
using std::vector;

namespace xe
{

// InfoLog

InfoLog::InfoLog(void)
{
}

void InfoLog::append(const uint8_t *bytes, size_t numBytes)
{
    DE_ASSERT(numBytes > 0);
    const size_t oldSize = m_data.size();
    m_data.resize(oldSize + numBytes);
    deMemcpy(&m_data[oldSize], bytes, numBytes);
}

// TestCaseResultData

TestCaseResultData::TestCaseResultData(const char *casePath) : m_casePath(casePath), m_statusCode(TESTSTATUSCODE_LAST)
{
}

TestCaseResultData::~TestCaseResultData(void)
{
}

void TestCaseResultData::setTestResult(TestStatusCode statusCode, const char *statusDetails)
{
    m_statusCode    = statusCode;
    m_statusDetails = statusDetails;
}

void TestCaseResultData::clear(void)
{
    m_statusCode = TESTSTATUSCODE_LAST;
    m_statusDetails.clear();
    m_casePath.clear();
    m_data.clear();
}

// BatchResult

BatchResult::BatchResult(void)
{
}

BatchResult::~BatchResult(void)
{
}

bool BatchResult::hasTestCaseResult(const char *casePath) const
{
    return m_resultMap.find(casePath) != m_resultMap.end();
}

ConstTestCaseResultPtr BatchResult::getTestCaseResult(const char *casePath) const
{
    map<string, int>::const_iterator pos = m_resultMap.find(casePath);
    DE_ASSERT(pos != m_resultMap.end());
    return getTestCaseResult(pos->second);
}

TestCaseResultPtr BatchResult::getTestCaseResult(const char *casePath)
{
    map<string, int>::const_iterator pos = m_resultMap.find(casePath);
    DE_ASSERT(pos != m_resultMap.end());
    return getTestCaseResult(pos->second);
}

TestCaseResultPtr BatchResult::createTestCaseResult(const char *casePath)
{
    DE_ASSERT(!hasTestCaseResult(casePath));

    m_testCaseResults.reserve(m_testCaseResults.size() + 1);
    m_resultMap[casePath] = (int)m_testCaseResults.size();

    TestCaseResultPtr caseResult(new TestCaseResultData(casePath));
    m_testCaseResults.push_back(caseResult);

    return caseResult;
}

} // namespace xe
