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

#include "xeContainerFormatParser.hpp"
#include "deInt32.h"

namespace xe
{

enum
{
    CONTAINERFORMATPARSER_INITIAL_BUFFER_SIZE = 1024
};

static int getNextBufferSize(int curSize, int minNewSize)
{
    return de::max(curSize * 2, 1 << deLog2Ceil32(minNewSize));
}

ContainerFormatParser::ContainerFormatParser(void)
    : m_element(CONTAINERELEMENT_INCOMPLETE)
    , m_elementLen(0)
    , m_state(STATE_AT_LINE_START)
    , m_buf(CONTAINERFORMATPARSER_INITIAL_BUFFER_SIZE)
{
}

ContainerFormatParser::~ContainerFormatParser(void)
{
}

void ContainerFormatParser::clear(void)
{
    m_element    = CONTAINERELEMENT_INCOMPLETE;
    m_elementLen = 0;
    m_state      = STATE_AT_LINE_START;
    m_buf.clear();
}

void ContainerFormatParser::error(const std::string &what)
{
    throw ContainerParseError(what);
}

void ContainerFormatParser::feed(const uint8_t *bytes, size_t numBytes)
{
    // Grow buffer if necessary.
    if (m_buf.getNumFree() < (int)numBytes)
        m_buf.resize(getNextBufferSize(m_buf.getSize(), m_buf.getNumElements() + (int)numBytes));

    // Append to front.
    m_buf.pushFront(bytes, (int)numBytes);

    // If we haven't parsed complete element, re-try after data feed.
    if (m_element == CONTAINERELEMENT_INCOMPLETE)
        advance();
}

const char *ContainerFormatParser::getSessionInfoAttribute(void) const
{
    DE_ASSERT(m_element == CONTAINERELEMENT_SESSION_INFO);
    return m_attribute.c_str();
}

const char *ContainerFormatParser::getSessionInfoValue(void) const
{
    DE_ASSERT(m_element == CONTAINERELEMENT_SESSION_INFO);
    return m_value.c_str();
}

const char *ContainerFormatParser::getTestCasePath(void) const
{
    DE_ASSERT(m_element == CONTAINERELEMENT_BEGIN_TEST_CASE_RESULT);
    return m_value.c_str();
}

const char *ContainerFormatParser::getTerminateReason(void) const
{
    DE_ASSERT(m_element == CONTAINERELEMENT_TERMINATE_TEST_CASE_RESULT);
    return m_value.c_str();
}

int ContainerFormatParser::getDataSize(void) const
{
    DE_ASSERT(m_element == CONTAINERELEMENT_TEST_LOG_DATA);
    return m_elementLen;
}

void ContainerFormatParser::getData(uint8_t *dst, int numBytes, int offset)
{
    DE_ASSERT(de::inBounds(offset, 0, m_elementLen) && numBytes > 0 && de::inRange(numBytes + offset, 0, m_elementLen));

    for (int ndx = 0; ndx < numBytes; ndx++)
        dst[ndx] = m_buf.peekBack(offset + ndx);
}

int ContainerFormatParser::getChar(int offset) const
{
    DE_ASSERT(de::inRange(offset, 0, m_buf.getNumElements()));

    if (offset < m_buf.getNumElements())
        return m_buf.peekBack(offset);
    else
        return END_OF_BUFFER;
}

const char *ContainerFormatParser::getTestRunsParams(void) const
{
    DE_ASSERT(m_element == CONTAINERELEMENT_TEST_RUN_PARAM_BEGIN);
    return m_value.c_str();
}

void ContainerFormatParser::advance(void)
{
    if (m_element != CONTAINERELEMENT_INCOMPLETE)
    {
        m_buf.popBack(m_elementLen);

        m_element    = CONTAINERELEMENT_INCOMPLETE;
        m_elementLen = 0;
        m_attribute.clear();
        m_value.clear();
    }

    for (;;)
    {
        int curChar = getChar(m_elementLen);

        if (curChar != (int)END_OF_BUFFER)
            m_elementLen += 1;

        if (curChar == END_OF_STRING)
        {
            if (m_elementLen == 1)
                m_element = CONTAINERELEMENT_END_OF_STRING;
            else if (m_state == STATE_CONTAINER_LINE)
                parseContainerLine();
            else
                m_element = CONTAINERELEMENT_TEST_LOG_DATA;

            break;
        }
        else if (curChar == (int)END_OF_BUFFER)
        {
            if (m_elementLen > 0 && m_state == STATE_DATA)
                m_element = CONTAINERELEMENT_TEST_LOG_DATA;

            break;
        }
        else if (curChar == '\r' || curChar == '\n')
        {
            // Check for \r\n
            int nextChar = getChar(m_elementLen);
            if (curChar == '\n' || (nextChar != (int)END_OF_BUFFER && nextChar != '\n'))
            {
                if (m_state == STATE_CONTAINER_LINE)
                    parseContainerLine();
                else
                    m_element = CONTAINERELEMENT_TEST_LOG_DATA;

                m_state = STATE_AT_LINE_START;
                break;
            }
            // else handle following end or \n in next iteration.
        }
        else if (m_state == STATE_AT_LINE_START)
        {
            DE_ASSERT(m_elementLen == 1);
            m_state = (curChar == '#') ? STATE_CONTAINER_LINE : STATE_DATA;
        }
    }
}

void ContainerFormatParser::parseContainerLine(void)
{
    static const struct
    {
        const char *name;
        ContainerElement element;
    } s_elements[] = {
        {"beginTestCaseResult", CONTAINERELEMENT_BEGIN_TEST_CASE_RESULT},
        {"endTestCaseResult", CONTAINERELEMENT_END_TEST_CASE_RESULT},
        {"terminateTestCaseResult", CONTAINERELEMENT_TERMINATE_TEST_CASE_RESULT},
        {"sessionInfo", CONTAINERELEMENT_SESSION_INFO},
        {"beginSession", CONTAINERELEMENT_BEGIN_SESSION},
        {"endSession", CONTAINERELEMENT_END_SESSION},
        {"beginTestRunParamsCollection", CONTAINERELEMENT_TEST_RUN_PARAM_SESSION_BEGIN},
        {"endTestRunParamsCollection", CONTAINERELEMENT_TEST_RUN_PARAM_SESSION_END},
        {"beginTestRunParams", CONTAINERELEMENT_TEST_RUN_PARAM_BEGIN},
        {"endTestRunParams", CONTAINERELEMENT_TEST_RUN_PARAM_END},
    };

    DE_ASSERT(m_elementLen >= 1);
    DE_ASSERT(getChar(0) == '#');

    int offset = 1;

    for (int elemNdx = 0; elemNdx < DE_LENGTH_OF_ARRAY(s_elements); elemNdx++)
    {
        bool isMatch = false;
        int ndx      = 0;

        for (;;)
        {
            int bufChar = (offset + ndx < m_elementLen) ? getChar(offset + ndx) : 0;
            bool bufEnd =
                bufChar == 0 || bufChar == ' ' || bufChar == '\r' || bufChar == '\n' || bufChar == (int)END_OF_BUFFER;
            int elemChar = s_elements[elemNdx].name[ndx];
            bool elemEnd = elemChar == 0;

            if (bufEnd || elemEnd)
            {
                isMatch = bufEnd == elemEnd;
                break;
            }
            else if (bufChar != elemChar)
                break;

            ndx += 1;
        }

        if (isMatch)
        {
            m_element = s_elements[elemNdx].element;
            offset += ndx;
            break;
        }
    }

    switch (m_element)
    {
    case CONTAINERELEMENT_BEGIN_SESSION:
    case CONTAINERELEMENT_END_SESSION:
    case CONTAINERELEMENT_END_TEST_CASE_RESULT:
    case CONTAINERELEMENT_TEST_RUN_PARAM_SESSION_BEGIN:
    case CONTAINERELEMENT_TEST_RUN_PARAM_SESSION_END:
    case CONTAINERELEMENT_TEST_RUN_PARAM_END:
        break; // No attribute or value.

    case CONTAINERELEMENT_BEGIN_TEST_CASE_RESULT:
    case CONTAINERELEMENT_TERMINATE_TEST_CASE_RESULT:
    case CONTAINERELEMENT_TEST_RUN_PARAM_BEGIN:
        if (getChar(offset) != ' ')
            error("Expected value after instruction");
        offset += 1;
        parseContainerValue(m_value, offset);
        break;

    case CONTAINERELEMENT_SESSION_INFO:
        if (getChar(offset) != ' ')
            error("Expected attribute name after #sessionInfo");
        offset += 1;
        parseContainerValue(m_attribute, offset);
        if (getChar(offset) != ' ')
            error("No value for #sessionInfo attribute");
        offset += 1;

        if (m_attribute == "timestamp")
        {
            m_value.clear();

            // \note Candy produces unescaped timestamps.
            for (;;)
            {
                const int curChar = offset < m_elementLen ? getChar(offset) : 0;
                const bool isEnd  = curChar == 0 || curChar == (int)END_OF_BUFFER || curChar == '\n' || curChar == '\t';

                if (isEnd)
                    break;
                else
                    m_value.push_back((char)curChar);

                offset += 1;
            }
        }
        else
            parseContainerValue(m_value, offset);
        break;

    default:
        // \todo [2012-06-09 pyry] Implement better way to handle # at the beginning of log lines.
        m_element = CONTAINERELEMENT_TEST_LOG_DATA;
        break;
    }
}

void ContainerFormatParser::parseContainerValue(std::string &dst, int &offset) const
{
    DE_ASSERT(offset < m_elementLen);

    bool isString = getChar(offset) == '"' || getChar(offset) == '\'';
    int quotChar  = isString ? getChar(offset) : 0;

    if (isString)
        offset += 1;

    dst.clear();

    for (;;)
    {
        int curChar = offset < m_elementLen ? getChar(offset) : 0;
        bool isEnd  = curChar == 0 || curChar == (int)END_OF_BUFFER ||
                     (isString ? (curChar == quotChar) : (curChar == ' ' || curChar == '\n' || curChar == '\r'));

        if (isEnd)
            break;
        else
        {
            // \todo [2012-06-09 pyry] Escapes.
            dst.push_back((char)curChar);
        }

        offset += 1;
    }

    if (isString && getChar(offset) == quotChar)
        offset += 1;
}

} // namespace xe
