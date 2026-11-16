#ifndef _TCURESULTCOLLECTOR_HPP
#define _TCURESULTCOLLECTOR_HPP
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

#include "tcuDefs.hpp"
#include "qpTestLog.h"

#include <string>

namespace tcu
{

class TestLog;
class TestContext;

/*--------------------------------------------------------------------*//*!
 * This utility class collects test results with associated messages,
 * optionally logs them, and finally sets the test result of a TestContext to
 * the most severe collected result. This allows multiple problems to be
 * easily reported from a single test run.
 *//*--------------------------------------------------------------------*/
class ResultCollector
{
public:
    ResultCollector(void);
    ResultCollector(TestLog &log, const std::string &prefix = "");

    qpTestResult getResult(void) const;
    const std::string getMessage(void) const
    {
        return m_message;
    }

    void fail(const std::string &msg);
    bool check(bool condition, const std::string &msg);

    void addResult(qpTestResult result, const std::string &msg);
    bool checkResult(bool condition, qpTestResult result, const std::string &msg);

    void setTestContextResult(TestContext &testCtx);

private:
    TestLog *const m_log;
    const std::string m_prefix;
    qpTestResult m_result;
    std::string m_message;
} DE_WARN_UNUSED_TYPE;

} // namespace tcu

#endif // _TCURESULTCOLLECTOR_HPP
