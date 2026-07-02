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
 * \brief XML Parser.
 *//*--------------------------------------------------------------------*/

#include "xeXMLParser.hpp"
#include "deInt32.h"

namespace xe
{
namespace xml
{

enum
{
    TOKENIZER_INITIAL_BUFFER_SIZE = 1024
};

static inline bool isIdentifierStartChar(int ch)
{
    return de::inRange<int>(ch, 'a', 'z') || de::inRange<int>(ch, 'A', 'Z');
}

static inline bool isIdentifierChar(int ch)
{
    return isIdentifierStartChar(ch) || de::inRange<int>(ch, '0', '9') || (ch == '-') || (ch == '_');
}

static inline bool isWhitespaceChar(int ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static int getNextBufferSize(int curSize, int minNewSize)
{
    return de::max(curSize * 2, 1 << deLog2Ceil32(minNewSize));
}

Tokenizer::Tokenizer(void)
    : m_curToken(TOKEN_INCOMPLETE)
    , m_curTokenLen(0)
    , m_state(STATE_DATA)
    , m_buf(TOKENIZER_INITIAL_BUFFER_SIZE)
{
}

Tokenizer::~Tokenizer(void)
{
}

void Tokenizer::clear(void)
{
    m_curToken    = TOKEN_INCOMPLETE;
    m_curTokenLen = 0;
    m_state       = STATE_DATA;
    m_buf.clear();
}

void Tokenizer::error(const std::string &what)
{
    throw ParseError(what);
}

void Tokenizer::feed(const uint8_t *bytes, int numBytes)
{
    // Grow buffer if necessary.
    if (m_buf.getNumFree() < numBytes)
    {
        m_buf.resize(getNextBufferSize(m_buf.getSize(), m_buf.getNumElements() + numBytes));
    }

    // Append to front.
    m_buf.pushFront(bytes, numBytes);

    // If we haven't parsed complete token, re-try after data feed.
    if (m_curToken == TOKEN_INCOMPLETE)
        advance();
}

int Tokenizer::getChar(int offset) const
{
    DE_ASSERT(de::inRange(offset, 0, m_buf.getNumElements()));

    if (offset < m_buf.getNumElements())
        return m_buf.peekBack(offset);
    else
        return END_OF_BUFFER;
}

void Tokenizer::advance(void)
{
    if (m_curToken != TOKEN_INCOMPLETE)
    {
        // Parser should not try to advance beyond end of string.
        DE_ASSERT(m_curToken != TOKEN_END_OF_STRING);

        // If current token is tag end, change state to data.
        if (m_curToken == TOKEN_TAG_END || m_curToken == TOKEN_EMPTY_ELEMENT_END ||
            m_curToken == TOKEN_PROCESSING_INSTRUCTION_END || m_curToken == TOKEN_COMMENT || m_curToken == TOKEN_ENTITY)
            m_state = STATE_DATA;

        // Advance buffer by length of last token.
        m_buf.popBack(m_curTokenLen);

        // Reset state.
        m_curToken    = TOKEN_INCOMPLETE;
        m_curTokenLen = 0;

        // If we hit end of string here, report it as end of string.
        if (getChar(0) == END_OF_STRING)
        {
            m_curToken    = TOKEN_END_OF_STRING;
            m_curTokenLen = 1;
            return;
        }
    }

    int curChar = getChar(m_curTokenLen);

    for (;;)
    {
        if (m_state == STATE_DATA)
        {
            // Advance until we hit end of buffer or tag start and treat that as data token.
            if (curChar == END_OF_STRING || curChar == (int)END_OF_BUFFER || curChar == '<' || curChar == '&')
            {
                if (curChar == '<')
                    m_state = STATE_TAG;
                else if (curChar == '&')
                    m_state = STATE_ENTITY;

                if (m_curTokenLen > 0)
                {
                    // Report data token.
                    m_curToken = TOKEN_DATA;
                    return;
                }
                else if (curChar == END_OF_STRING || curChar == (int)END_OF_BUFFER)
                {
                    // Just return incomplete token, no data parsed.
                    return;
                }
                else
                {
                    DE_ASSERT(m_state == STATE_TAG || m_state == STATE_ENTITY);
                    continue;
                }
            }
        }
        else
        {
            // Eat all whitespace if present.
            if (m_curTokenLen == 0)
            {
                while (isWhitespaceChar(curChar))
                {
                    m_buf.popBack();
                    curChar = getChar(0);
                }
            }

            // Handle end of string / buffer.
            if (curChar == END_OF_STRING)
                error("Unexpected end of string");
            else if (curChar == (int)END_OF_BUFFER)
            {
                DE_ASSERT(m_curToken == TOKEN_INCOMPLETE);
                return;
            }

            if (m_curTokenLen == 0)
            {
                // Expect start of identifier, value or special tag token.
                if (curChar == '\'' || curChar == '"')
                    m_state = STATE_VALUE;
                else if (isIdentifierStartChar(curChar))
                    m_state = STATE_IDENTIFIER;
                else if (curChar == '<' || curChar == '?' || curChar == '/')
                    m_state = STATE_TAG;
                else if (curChar == '&')
                    DE_ASSERT(m_state == STATE_ENTITY);
                else if (curChar == '=')
                {
                    m_curToken    = TOKEN_EQUAL;
                    m_curTokenLen = 1;
                    return;
                }
                else if (curChar == '>')
                {
                    m_curToken    = TOKEN_TAG_END;
                    m_curTokenLen = 1;
                    return;
                }
                else
                    error("Unexpected character");
            }
            else if (m_state == STATE_IDENTIFIER)
            {
                if (!isIdentifierChar(curChar))
                {
                    m_curToken = TOKEN_IDENTIFIER;
                    return;
                }
            }
            else if (m_state == STATE_VALUE)
            {
                // \todo [2012-06-07 pyry] Escapes.
                if (curChar == '\'' || curChar == '"')
                {
                    // \todo [2012-10-17 pyry] Should we actually do the check against getChar(0)?
                    if (curChar != getChar(0))
                        error("Mismatched quote");
                    m_curToken = TOKEN_STRING;
                    m_curTokenLen += 1;
                    return;
                }
            }
            else if (m_state == STATE_COMMENT)
            {
                DE_ASSERT(m_curTokenLen >= 2); // 2 characters have been parsed if we are in comment state.

                if (m_curTokenLen <= 3)
                {
                    if (curChar != '-')
                        error("Invalid comment start");
                }
                else
                {
                    int prev2 = m_curTokenLen > 5 ? getChar(m_curTokenLen - 2) : 0;
                    int prev1 = m_curTokenLen > 4 ? getChar(m_curTokenLen - 1) : 0;

                    if (prev2 == '-' && prev1 == '-')
                    {
                        if (curChar != '>')
                            error("Invalid comment end");
                        m_curToken = TOKEN_COMMENT;
                        m_curTokenLen += 1;
                        return;
                    }
                }
            }
            else if (m_state == STATE_ENTITY)
            {
                if (m_curTokenLen >= 1)
                {
                    if (curChar == ';')
                    {
                        m_curToken = TOKEN_ENTITY;
                        m_curTokenLen += 1;
                        return;
                    }
                    else if (!de::inRange<int>(curChar, '0', '9') && !de::inRange<int>(curChar, 'a', 'z') &&
                             !de::inRange<int>(curChar, 'A', 'Z'))
                        error("Invalid entity");
                }
            }
            else
            {
                // Special tokens are at most 2 characters.
                DE_ASSERT(m_state == STATE_TAG && m_curTokenLen == 1);

                int prevChar = getChar(m_curTokenLen - 1);

                if (prevChar == '<')
                {
                    // Tag start.
                    if (curChar == '/')
                    {
                        m_curToken    = TOKEN_END_TAG_START;
                        m_curTokenLen = 2;
                        return;
                    }
                    else if (curChar == '?')
                    {
                        m_curToken    = TOKEN_PROCESSING_INSTRUCTION_START;
                        m_curTokenLen = 2;
                        return;
                    }
                    else if (curChar == '!')
                    {
                        m_state = STATE_COMMENT;
                    }
                    else
                    {
                        m_curToken    = TOKEN_TAG_START;
                        m_curTokenLen = 1;
                        return;
                    }
                }
                else if (prevChar == '?')
                {
                    if (curChar != '>')
                        error("Invalid processing instruction end");
                    m_curToken    = TOKEN_PROCESSING_INSTRUCTION_END;
                    m_curTokenLen = 2;
                    return;
                }
                else if (prevChar == '/')
                {
                    if (curChar != '>')
                        error("Invalid empty element end");
                    m_curToken    = TOKEN_EMPTY_ELEMENT_END;
                    m_curTokenLen = 2;
                    return;
                }
                else
                    error("Could not parse special token");
            }
        }

        m_curTokenLen += 1;
        curChar = getChar(m_curTokenLen);
    }
}

void Tokenizer::getString(std::string &dst) const
{
    DE_ASSERT(m_curToken == TOKEN_STRING);
    dst.resize(m_curTokenLen - 2);
    for (int ndx = 0; ndx < m_curTokenLen - 2; ndx++)
        dst[ndx] = m_buf.peekBack(ndx + 1);
}

Parser::Parser(void) : m_element(ELEMENT_INCOMPLETE), m_state(STATE_DATA)
{
}

Parser::~Parser(void)
{
}

void Parser::clear(void)
{
    m_tokenizer.clear();
    m_elementName.clear();
    m_attributes.clear();
    m_attribName.clear();
    m_entityValue.clear();

    m_element = ELEMENT_INCOMPLETE;
    m_state   = STATE_DATA;
}

void Parser::error(const std::string &what)
{
    throw ParseError(what);
}

void Parser::feed(const uint8_t *bytes, int numBytes)
{
    m_tokenizer.feed(bytes, numBytes);

    if (m_element == ELEMENT_INCOMPLETE)
        advance();
}

void Parser::advance(void)
{
    if (m_element == ELEMENT_START)
        m_attributes.clear();

    // \note No token is advanced when element end is reported.
    if (m_state == STATE_YIELD_EMPTY_ELEMENT_END)
    {
        DE_ASSERT(m_element == ELEMENT_START);
        m_element = ELEMENT_END;
        m_state   = STATE_DATA;
        return;
    }

    if (m_element != ELEMENT_INCOMPLETE)
    {
        m_tokenizer.advance();
        m_element = ELEMENT_INCOMPLETE;
    }

    for (;;)
    {
        Token curToken = m_tokenizer.getToken();

        // Skip comments.
        while (curToken == TOKEN_COMMENT)
        {
            m_tokenizer.advance();
            curToken = m_tokenizer.getToken();
        }

        if (curToken == TOKEN_INCOMPLETE)
        {
            DE_ASSERT(m_element == ELEMENT_INCOMPLETE);
            return;
        }

        switch (m_state)
        {
        case STATE_ENTITY:
            m_state = STATE_DATA;
            // Fall-through

        case STATE_DATA:
            switch (curToken)
            {
            case TOKEN_DATA:
                m_element = ELEMENT_DATA;
                return;

            case TOKEN_END_OF_STRING:
                m_element = ELEMENT_END_OF_STRING;
                return;

            case TOKEN_TAG_START:
                m_state = STATE_START_TAG_OPEN;
                break;

            case TOKEN_END_TAG_START:
                m_state = STATE_END_TAG_OPEN;
                break;

            case TOKEN_PROCESSING_INSTRUCTION_START:
                m_state = STATE_IN_PROCESSING_INSTRUCTION;
                break;

            case TOKEN_ENTITY:
                m_state   = STATE_ENTITY;
                m_element = ELEMENT_DATA;
                parseEntityValue();
                return;

            default:
                error("Unexpected token");
            }
            break;

        case STATE_IN_PROCESSING_INSTRUCTION:
            if (curToken == TOKEN_PROCESSING_INSTRUCTION_END)
                m_state = STATE_DATA;
            else if (curToken != TOKEN_IDENTIFIER && curToken != TOKEN_EQUAL && curToken != TOKEN_STRING)
                error("Unexpected token in processing instruction");
            break;

        case STATE_START_TAG_OPEN:
            if (curToken != TOKEN_IDENTIFIER)
                error("Expected identifier");
            m_tokenizer.getTokenStr(m_elementName);
            m_state = STATE_ATTRIBUTE_LIST;
            break;

        case STATE_END_TAG_OPEN:
            if (curToken != TOKEN_IDENTIFIER)
                error("Expected identifier");
            m_tokenizer.getTokenStr(m_elementName);
            m_state = STATE_EXPECTING_END_TAG_CLOSE;
            break;

        case STATE_EXPECTING_END_TAG_CLOSE:
            if (curToken != TOKEN_TAG_END)
                error("Expected tag end");
            m_state   = STATE_DATA;
            m_element = ELEMENT_END;
            return;

        case STATE_ATTRIBUTE_LIST:
            if (curToken == TOKEN_IDENTIFIER)
            {
                m_tokenizer.getTokenStr(m_attribName);
                m_state = STATE_EXPECTING_ATTRIBUTE_EQ;
            }
            else if (curToken == TOKEN_EMPTY_ELEMENT_END)
            {
                m_state   = STATE_YIELD_EMPTY_ELEMENT_END;
                m_element = ELEMENT_START;
                return;
            }
            else if (curToken == TOKEN_TAG_END)
            {
                m_state   = STATE_DATA;
                m_element = ELEMENT_START;
                return;
            }
            else
                error("Unexpected token");
            break;

        case STATE_EXPECTING_ATTRIBUTE_EQ:
            if (curToken != TOKEN_EQUAL)
                error("Expected '='");
            m_state = STATE_EXPECTING_ATTRIBUTE_VALUE;
            break;

        case STATE_EXPECTING_ATTRIBUTE_VALUE:
            if (curToken != TOKEN_STRING)
                error("Expected value");
            if (hasAttribute(m_attribName.c_str()))
                error("Duplicate attribute");

            m_tokenizer.getString(m_attributes[m_attribName]);
            m_state = STATE_ATTRIBUTE_LIST;
            break;

        default:
            DE_ASSERT(false);
        }

        m_tokenizer.advance();
    }
}

static char getEntityValue(const std::string &entity)
{
    static const struct
    {
        const char *name;
        char value;
    } s_entities[] = {
        {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'}, {"&apos;", '\''}, {"&quot;", '"'},
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_entities); ndx++)
    {
        if (entity == s_entities[ndx].name)
            return s_entities[ndx].value;
    }

    return 0;
}

void Parser::parseEntityValue(void)
{
    DE_ASSERT(m_state == STATE_ENTITY && m_tokenizer.getToken() == TOKEN_ENTITY);

    std::string entity;
    m_tokenizer.getTokenStr(entity);

    const char value = getEntityValue(entity);
    if (value == 0)
        error("Invalid entity '" + entity + "'");

    m_entityValue.resize(1);
    m_entityValue[0] = value;
}

} // namespace xml
} // namespace xe
