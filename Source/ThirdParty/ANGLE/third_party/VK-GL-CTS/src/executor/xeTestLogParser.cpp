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

#include "xeTestLogParser.hpp"
#include "deString.h"

using std::map;
using std::string;
using std::vector;

namespace xe
{

TestLogParser::TestLogParser(TestLogHandler *handler) : m_handler(handler), m_inSession(false)
{
}

TestLogParser::~TestLogParser(void)
{
}

void TestLogParser::reset(void)
{
    m_containerParser.clear();
    m_currentCaseData.clear();
    m_sessionInfo = SessionInfo();
    m_inSession   = false;
}

void TestLogParser::parse(const uint8_t *bytes, size_t numBytes)
{
    m_containerParser.feed(bytes, numBytes);

    for (;;)
    {
        ContainerElement element = m_containerParser.getElement();

        if (element == CONTAINERELEMENT_INCOMPLETE)
            break;

        switch (element)
        {
        case CONTAINERELEMENT_BEGIN_SESSION:
        {
            if (m_inSession)
                throw Error("Unexpected #beginSession");

            m_handler->setSessionInfo(m_sessionInfo);
            m_inSession = true;
            break;
        }

        case CONTAINERELEMENT_END_SESSION:
        {
            if (!m_inSession)
                throw Error("Unexpected #endSession");

            m_inSession = false;
            break;
        }

        case CONTAINERELEMENT_SESSION_INFO:
        {
            if (m_inSession)
                throw Error("Unexpected #sessionInfo");

            const char *attribute = m_containerParser.getSessionInfoAttribute();
            const char *value     = m_containerParser.getSessionInfoValue();

            if (strcmp(attribute, "releaseName") == 0)
                m_sessionInfo.releaseName = value;
            else if (strcmp(attribute, "releaseId") == 0)
                m_sessionInfo.releaseId = value;
            else if (strcmp(attribute, "targetName") == 0)
                m_sessionInfo.targetName = value;
            else if (strcmp(attribute, "candyTargetName") == 0)
                m_sessionInfo.candyTargetName = value;
            else if (strcmp(attribute, "configName") == 0)
                m_sessionInfo.configName = value;
            else if (strcmp(attribute, "resultName") == 0)
                m_sessionInfo.resultName = value;
            else if (strcmp(attribute, "timestamp") == 0)
                m_sessionInfo.timestamp = value;
            else if (strcmp(attribute, "commandLineParameters") == 0)
                m_sessionInfo.qpaCommandLineParameters = value;

            // \todo [2012-06-09 pyry] What to do with unknown/duplicate attributes? Currently just ignored.
            break;
        }

        case CONTAINERELEMENT_BEGIN_TEST_CASE_RESULT:
        {
            if (!m_inSession)
                throw Error("Unexpected #beginTestCaseResult");

            const char *casePath = m_containerParser.getTestCasePath();
            m_currentCaseData    = m_handler->startTestCaseResult(casePath);

            // Clear and set to running state.
            m_currentCaseData->setDataSize(0);
            m_currentCaseData->setTestResult(TESTSTATUSCODE_RUNNING, "Running");

            m_handler->testCaseResultUpdated(m_currentCaseData);
            break;
        }

        case CONTAINERELEMENT_END_TEST_CASE_RESULT:
            if (m_currentCaseData)
            {
                // \todo [2012-06-16 pyry] Parse status code already here?
                m_currentCaseData->setTestResult(TESTSTATUSCODE_LAST, "");
                m_handler->testCaseResultComplete(m_currentCaseData);
            }
            m_currentCaseData.clear();
            break;

        case CONTAINERELEMENT_TERMINATE_TEST_CASE_RESULT:
            if (m_currentCaseData)
            {
                TestStatusCode statusCode = TESTSTATUSCODE_CRASH;
                const char *reason        = m_containerParser.getTerminateReason();
                try
                {
                    statusCode = getTestStatusCode(reason);
                }
                catch (const xe::ParseError &)
                {
                    // Could not map status code.
                }
                m_currentCaseData->setTestResult(statusCode, reason);
                m_handler->testCaseResultComplete(m_currentCaseData);
            }
            m_currentCaseData.clear();
            break;

        case CONTAINERELEMENT_END_OF_STRING:
            if (m_currentCaseData)
            {
                // Terminate current case.
                m_currentCaseData->setTestResult(TESTSTATUSCODE_TERMINATED, "Unexpected end of string");
                m_handler->testCaseResultComplete(m_currentCaseData);
            }
            m_currentCaseData.clear();
            break;

        case CONTAINERELEMENT_TEST_LOG_DATA:
            if (m_currentCaseData)
            {
                int offset       = m_currentCaseData->getDataSize();
                int numDataBytes = m_containerParser.getDataSize();

                m_currentCaseData->setDataSize(offset + numDataBytes);
                m_containerParser.getData(m_currentCaseData->getData() + offset, numDataBytes, 0);

                m_handler->testCaseResultUpdated(m_currentCaseData);
            }
            break;

        default:
            throw ContainerParseError("Unknown container element");
        }

        m_containerParser.advance();
    }
}

} // namespace xe
