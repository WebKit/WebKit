#ifndef _XECONTAINERFORMATPARSER_HPP
#define _XECONTAINERFORMATPARSER_HPP
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
 * \brief Test log container format parser.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "deRingBuffer.hpp"

namespace xe
{

enum ContainerElement
{
    CONTAINERELEMENT_INCOMPLETE = 0,
    CONTAINERELEMENT_END_OF_STRING,
    CONTAINERELEMENT_BEGIN_SESSION,
    CONTAINERELEMENT_END_SESSION,
    CONTAINERELEMENT_SESSION_INFO,
    CONTAINERELEMENT_BEGIN_TEST_CASE_RESULT,
    CONTAINERELEMENT_END_TEST_CASE_RESULT,
    CONTAINERELEMENT_TERMINATE_TEST_CASE_RESULT,
    CONTAINERELEMENT_TEST_LOG_DATA,
    CONTAINERELEMENT_TEST_RUN_PARAM_SESSION_BEGIN,
    CONTAINERELEMENT_TEST_RUN_PARAM_SESSION_END,
    CONTAINERELEMENT_TEST_RUN_PARAM_BEGIN,
    CONTAINERELEMENT_TEST_RUN_PARAM_END,

    CONTAINERELEMENT_LAST
};

class ContainerParseError : public ParseError
{
public:
    ContainerParseError(const std::string &message) : ParseError(message)
    {
    }
};

class ContainerFormatParser
{
public:
    ContainerFormatParser(void);
    ~ContainerFormatParser(void);

    void clear(void);

    void feed(const uint8_t *bytes, size_t numBytes);
    void advance(void);

    ContainerElement getElement(void) const
    {
        return m_element;
    }

    // SESSION_INFO
    const char *getSessionInfoAttribute(void) const;
    const char *getSessionInfoValue(void) const;

    // BEGIN_TEST_CASE
    const char *getTestCasePath(void) const;

    // TERMINATE_TEST_CASE
    const char *getTerminateReason(void) const;

    // TEST_LOG_DATA
    int getDataSize(void) const;
    void getData(uint8_t *dst, int numBytes, int offset);

    // TEST_RUN_PARAM
    const char *getTestRunsParams(void) const;

private:
    ContainerFormatParser(const ContainerFormatParser &other);
    ContainerFormatParser &operator=(const ContainerFormatParser &other);

    void error(const std::string &what);

    enum State
    {
        STATE_AT_LINE_START,
        STATE_CONTAINER_LINE,
        STATE_DATA,

        STATE_LAST
    };

    enum
    {
        END_OF_STRING = 0,         //!< End of string (0).
        END_OF_BUFFER = 0xffffffff //!< End of current data buffer.
    };

    int getChar(int offset) const;
    void parseContainerLine(void);
    void parseContainerValue(std::string &dst, int &offset) const;

    ContainerElement m_element;
    int m_elementLen;
    State m_state;
    std::string m_attribute;
    std::string m_value;

    de::RingBuffer<uint8_t> m_buf;
};

} // namespace xe

#endif // _XECONTAINERFORMATPARSER_HPP
