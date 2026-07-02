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
 * \brief Test result collector
 *//*--------------------------------------------------------------------*/

#include "tcuResultCollector.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestLog.hpp"

namespace tcu
{

static int testResultSeverity(qpTestResult testResult)
{
    switch (testResult)
    {
    case QP_TEST_RESULT_LAST:
        return -1;
    case QP_TEST_RESULT_PASS:
        return 0;
    case QP_TEST_RESULT_PENDING:
        return 10;
    case QP_TEST_RESULT_NOT_SUPPORTED:
        return 20;
    case QP_TEST_RESULT_QUALITY_WARNING:
        return 30;
    case QP_TEST_RESULT_COMPATIBILITY_WARNING:
    case QP_TEST_RESULT_CAPABILITY_WARNING:
        return 40;
    case QP_TEST_RESULT_TIMEOUT:
        return 50;
    case QP_TEST_RESULT_WAIVER:
        return 60;
    case QP_TEST_RESULT_FAIL:
        return 100;
    case QP_TEST_RESULT_RESOURCE_ERROR:
        return 110;
    case QP_TEST_RESULT_INTERNAL_ERROR:
        return 120;
    case QP_TEST_RESULT_CRASH:
        return 150;
    default:
        DE_FATAL("Impossible case");
    }
    return 0;
}

ResultCollector::ResultCollector(void) : m_log(nullptr), m_prefix(""), m_result(QP_TEST_RESULT_LAST), m_message("Pass")
{
}

ResultCollector::ResultCollector(TestLog &log, const std::string &prefix)
    : m_log(&log)
    , m_prefix(prefix)
    , m_result(QP_TEST_RESULT_LAST)
    , m_message("Pass")
{
}

qpTestResult ResultCollector::getResult(void) const
{
    if (m_result == QP_TEST_RESULT_LAST)
        return QP_TEST_RESULT_PASS;
    else
        return m_result;
}

void ResultCollector::addResult(qpTestResult result, const std::string &msg)
{
    if (m_log != nullptr)
        (*m_log) << TestLog::Message << m_prefix << msg << TestLog::EndMessage;

    if (testResultSeverity(result) > testResultSeverity(m_result))
    {
        m_result  = result;
        m_message = msg;
    }
}

bool ResultCollector::checkResult(bool condition, qpTestResult result, const std::string &msg)
{
    if (!condition)
        addResult(result, msg);
    return condition;
}

void ResultCollector::fail(const std::string &msg)
{
    addResult(QP_TEST_RESULT_FAIL, msg);
}

bool ResultCollector::check(bool condition, const std::string &msg)
{
    return checkResult(condition, QP_TEST_RESULT_FAIL, msg);
}

void ResultCollector::setTestContextResult(TestContext &testCtx)
{
    testCtx.setTestResult(getResult(), getMessage().c_str());
}

} // namespace tcu
