#ifndef _XETESTLOGPARSER_HPP
#define _XETESTLOGPARSER_HPP
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
 * \brief Test log parser.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "xeTestCaseResult.hpp"
#include "xeContainerFormatParser.hpp"
#include "xeTestResultParser.hpp"
#include "xeBatchResult.hpp"

#include <string>
#include <vector>
#include <map>

namespace xe
{

class TestLogHandler
{
public:
    virtual ~TestLogHandler(void)
    {
    }

    virtual void setSessionInfo(const SessionInfo &sessionInfo) = 0;

    virtual TestCaseResultPtr startTestCaseResult(const char *casePath)      = 0;
    virtual void testCaseResultUpdated(const TestCaseResultPtr &resultData)  = 0;
    virtual void testCaseResultComplete(const TestCaseResultPtr &resultData) = 0;
};

class TestLogParser
{
public:
    TestLogParser(TestLogHandler *handler);
    ~TestLogParser(void);

    void reset(void);

    void parse(const uint8_t *bytes, size_t numBytes);

private:
    TestLogParser(const TestLogParser &other);
    TestLogParser &operator=(const TestLogParser &other);

    ContainerFormatParser m_containerParser;
    TestLogHandler *m_handler;

    SessionInfo m_sessionInfo;
    TestCaseResultPtr m_currentCaseData;
    bool m_inSession;
};

} // namespace xe

#endif // _XETESTLOGPARSER_HPP
