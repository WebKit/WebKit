#ifndef _XEXMLPARSER_HPP
#define _XEXMLPARSER_HPP
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
 *
 * \todo [2012-06-07 pyry] Not supported / handled properly:
 *  - xml namespaces (<ns:Element>)
 *  - backslash escapes in strings
 *  - &quot; -style escapes
 *  - utf-8
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "deRingBuffer.hpp"

#include <string>
#include <map>

namespace xe
{
namespace xml
{

enum Token
{
    TOKEN_INCOMPLETE = 0,               //!< Not enough data to determine token.
    TOKEN_END_OF_STRING,                //!< End of document string.
    TOKEN_DATA,                         //!< Block of data (anything outside tags).
    TOKEN_COMMENT,                      //!< <!-- comment -->
    TOKEN_IDENTIFIER,                   //!< Identifier (in tags).
    TOKEN_STRING,                       //!< String (in tags).
    TOKEN_TAG_START,                    //!< <
    TOKEN_TAG_END,                      //!< >
    TOKEN_END_TAG_START,                //!< </
    TOKEN_EMPTY_ELEMENT_END,            //!< />
    TOKEN_PROCESSING_INSTRUCTION_START, //!< <?
    TOKEN_PROCESSING_INSTRUCTION_END,   //!< ?>
    TOKEN_EQUAL,                        //!< =
    TOKEN_ENTITY,                       //!< Entity reference, such as &amp;

    TOKEN_LAST
};

enum Element
{
    ELEMENT_INCOMPLETE = 0, //!< Incomplete element.
    ELEMENT_START,          //!< Element start.
    ELEMENT_END,            //!< Element end.
    ELEMENT_DATA,           //!< Data element.
    ELEMENT_END_OF_STRING,  //!< End of document string.

    ELEMENT_LAST
};

const char *getTokenName(Token token);

// \todo [2012-10-17 pyry] Add line number etc.
class ParseError : public xe::ParseError
{
public:
    ParseError(const std::string &message) : xe::ParseError(message)
    {
    }
};

class Tokenizer
{
public:
    Tokenizer(void);
    ~Tokenizer(void);

    void clear(void); //!< Resets tokenizer to initial state.

    void feed(const uint8_t *bytes, int numBytes);
    void advance(void);

    Token getToken(void) const
    {
        return m_curToken;
    }
    int getTokenLen(void) const
    {
        return m_curTokenLen;
    }
    uint8_t getTokenByte(int offset) const
    {
        DE_ASSERT(m_curToken != TOKEN_INCOMPLETE && m_curToken != TOKEN_END_OF_STRING);
        return m_buf.peekBack(offset);
    }
    void getTokenStr(std::string &dst) const;
    void appendTokenStr(std::string &dst) const;

    void getString(std::string &dst) const;

private:
    Tokenizer(const Tokenizer &other);
    Tokenizer &operator=(const Tokenizer &other);

    int getChar(int offset) const;

    void error(const std::string &what);

    enum State
    {
        STATE_DATA = 0,
        STATE_TAG,
        STATE_IDENTIFIER,
        STATE_VALUE,
        STATE_COMMENT,
        STATE_ENTITY,

        STATE_LAST
    };

    enum
    {
        END_OF_STRING = 0,         //!< End of string (0).
        END_OF_BUFFER = 0xffffffff //!< End of current data buffer.
    };

    Token m_curToken;  //!< Current token.
    int m_curTokenLen; //!< Length of current token.

    State m_state; //!< Tokenization state.

    de::RingBuffer<uint8_t> m_buf;
};

class Parser
{
public:
    typedef std::map<std::string, std::string> AttributeMap;
    typedef AttributeMap::const_iterator AttributeIter;

    Parser(void);
    ~Parser(void);

    void clear(void); //!< Resets parser to initial state.

    void feed(const uint8_t *bytes, int numBytes);
    void advance(void);

    Element getElement(void) const
    {
        return m_element;
    }

    // For ELEMENT_START / ELEMENT_END.
    const char *getElementName(void) const
    {
        return m_elementName.c_str();
    }

    // For ELEMENT_START.
    bool hasAttribute(const char *name) const
    {
        return m_attributes.find(name) != m_attributes.end();
    }
    const char *getAttribute(const char *name) const
    {
        return m_attributes.find(name)->second.c_str();
    }
    const AttributeMap &attributes(void) const
    {
        return m_attributes;
    }

    // For ELEMENT_DATA.
    int getDataSize(void) const;
    uint8_t getDataByte(int offset) const;
    void getDataStr(std::string &dst) const;
    void appendDataStr(std::string &dst) const;

private:
    Parser(const Parser &other);
    Parser &operator=(const Parser &other);

    void parseEntityValue(void);

    void error(const std::string &what);

    enum State
    {
        STATE_DATA = 0,                  //!< Initial state - assuming data or tag open.
        STATE_ENTITY,                    //!< Parsed entity is stored - overrides data.
        STATE_IN_PROCESSING_INSTRUCTION, //!< In processing instruction.
        STATE_START_TAG_OPEN,            //!< Start tag open.
        STATE_END_TAG_OPEN,              //!< End tag open.
        STATE_EXPECTING_END_TAG_CLOSE,   //!< Expecting end tag close.
        STATE_ATTRIBUTE_LIST,            //!< Expecting attribute list.
        STATE_EXPECTING_ATTRIBUTE_EQ,    //!< Got attribute name, expecting =.
        STATE_EXPECTING_ATTRIBUTE_VALUE, //!< Expecting attribute value.
        STATE_YIELD_EMPTY_ELEMENT_END,   //!< Empty element: start has been reported but not end.

        STATE_LAST
    };

    Tokenizer m_tokenizer;

    Element m_element;
    std::string m_elementName;
    AttributeMap m_attributes;

    State m_state;
    std::string m_attribName;
    std::string m_entityValue; //!< Data override, such as entity value.
};

// Inline implementations

inline void Tokenizer::getTokenStr(std::string &dst) const
{
    DE_ASSERT(m_curToken != TOKEN_INCOMPLETE && m_curToken != TOKEN_END_OF_STRING);
    dst.resize(m_curTokenLen);
    for (int ndx = 0; ndx < m_curTokenLen; ndx++)
        dst[ndx] = m_buf.peekBack(ndx);
}

inline void Tokenizer::appendTokenStr(std::string &dst) const
{
    DE_ASSERT(m_curToken != TOKEN_INCOMPLETE && m_curToken != TOKEN_END_OF_STRING);

    size_t oldLen = dst.size();
    dst.resize(oldLen + m_curTokenLen);

    for (int ndx = 0; ndx < m_curTokenLen; ndx++)
        dst[oldLen + ndx] = m_buf.peekBack(ndx);
}

inline int Parser::getDataSize(void) const
{
    if (m_state != STATE_ENTITY)
        return m_tokenizer.getTokenLen();
    else
        return (int)m_entityValue.size();
}

inline uint8_t Parser::getDataByte(int offset) const
{
    if (m_state != STATE_ENTITY)
        return m_tokenizer.getTokenByte(offset);
    else
        return (uint8_t)m_entityValue[offset];
}

inline void Parser::getDataStr(std::string &dst) const
{
    if (m_state != STATE_ENTITY)
        return m_tokenizer.getTokenStr(dst);
    else
        dst = m_entityValue;
}

inline void Parser::appendDataStr(std::string &dst) const
{
    if (m_state != STATE_ENTITY)
        return m_tokenizer.appendTokenStr(dst);
    else
        dst += m_entityValue;
}

} // namespace xml
} // namespace xe

#endif // _XEXMLPARSER_HPP
