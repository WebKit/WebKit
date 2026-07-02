#ifndef _XETESTRESULTPARSER_HPP
#define _XETESTRESULTPARSER_HPP
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
 * \brief Test case result parser.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "xeXMLParser.hpp"
#include "xeTestCaseResult.hpp"

#include <vector>

namespace xe
{

enum TestLogVersion
{
    TESTLOGVERSION_0_2_0 = 0,
    TESTLOGVERSION_0_3_0,
    TESTLOGVERSION_0_3_1,
    TESTLOGVERSION_0_3_2,
    TESTLOGVERSION_0_3_3,
    TESTLOGVERSION_0_3_4,

    TESTLOGVERSION_LAST
};

class TestResultParseError : public ParseError
{
public:
    TestResultParseError(const std::string &message) : ParseError(message)
    {
    }
};

class TestResultParser
{
public:
    enum ParseResult
    {
        PARSERESULT_NOT_CHANGED,
        PARSERESULT_CHANGED,
        PARSERESULT_COMPLETE,
        PARSERESULT_ERROR,

        PARSERESULT_LAST
    };

    TestResultParser(void);
    ~TestResultParser(void);

    void init(TestCaseResult *dstResult);
    ParseResult parse(const uint8_t *bytes, int numBytes);

private:
    TestResultParser(const TestResultParser &other);
    TestResultParser &operator=(const TestResultParser &other);

    void clear(void);

    void handleElementStart(void);
    void handleElementEnd(void);
    void handleData(void);

    const char *getAttribute(const char *name);

    ri::Item *getCurrentItem(void);
    ri::List *getCurrentItemList(void);
    void pushItem(ri::Item *item);
    void popItem(void);
    void updateCurrentItemList(void);

    enum State
    {
        STATE_NOT_INITIALIZED = 0,
        STATE_INITIALIZED,
        STATE_IN_TEST_CASE_RESULT,
        STATE_TEST_CASE_RESULT_ENDED,

        STATE_LAST
    };

    xml::Parser m_xmlParser;
    TestCaseResult *m_result;

    State m_state;
    TestLogVersion m_logVersion; //!< Only valid in STATE_IN_TEST_CASE_RESULT.

    std::vector<ri::Item *> m_itemStack;
    ri::List *m_curItemList;

    int m_base64DecodeOffset;

    std::string m_curNumValue;
};

// Helpers exposed to other parsers.
TestStatusCode getTestStatusCode(const char *statusCode);

// Parsing helpers.

class TestCaseResultData;

void parseTestCaseResultFromData(TestResultParser *parser, TestCaseResult *result, const TestCaseResultData &data);

} // namespace xe

#endif // _XETESTRESULTPARSER_HPP
