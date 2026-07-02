#ifndef _RSGTOKEN_HPP
#define _RSGTOKEN_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Token class.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"

#include <vector>

namespace rsg
{

class Token
{
public:
    enum Type
    {
        IDENTIFIER,
        STRUCT,
        INVARIANT,
        PRECISION,
        VOID,
        BREAK,
        CONTINUE,
        DO,
        WHILE,
        ELSE,
        FOR,
        IF,
        DISCARD,
        RETURN,
        INC_OP,
        DEC_OP,
        LEFT_PAREN,
        RIGHT_PAREN,
        LEFT_BRACKET,  // [
        RIGHT_BRACKET, // ]
        LEFT_BRACE,    // {
        RIGHT_BRACE,   // }
        DOT,
        COMMA,
        COLON,
        SEMICOLON,
        MINUS,
        PLUS,
        MUL,
        DIV,
        MOD,
        QUESTION,
        BOOL,
        BVEC2,
        BVEC3,
        BVEC4,
        INT,
        IVEC2,
        IVEC3,
        IVEC4,
        FLOAT,
        VEC2,
        VEC3,
        VEC4,
        MAT2,
        MAT3,
        MAT4,
        SAMPLER2D,
        SAMPLERCUBE,
        FLOAT_LITERAL,
        INT_LITERAL,
        BOOL_LITERAL,
        EQUAL,
        MUL_ASSIGN,
        DIV_ASSIGN,
        ADD_ASSIGN,
        SUB_ASSIGN,
        CMP_LT,
        CMP_GT,
        CMP_LE,
        CMP_GE,
        CMP_EQ,
        CMP_NE,
        LOGICAL_AND,
        LOGICAL_OR,
        LOGICAL_NOT,
        LOGICAL_XOR,
        ATTRIBUTE,
        UNIFORM,
        VARYING,
        CONST,
        FLAT,
        HIGH_PRECISION,
        MEDIUM_PRECISION,
        LOW_PRECISION,
        IN,
        OUT,
        INOUT,
        LAYOUT,
        LOCATION,

        // Formatting only
        INDENT_INC,
        INDENT_DEC,
        NEWLINE,

        TYPE_LAST
    };

    Token(void);
    Token(Type type);
    Token(const char *identifier);
    Token(float value);
    Token(int value);
    Token(bool value);
    Token(const Token &other);

    ~Token(void);

    inline bool operator==(Type type) const
    {
        return m_type == type;
    }
    inline bool operator!=(Type type) const
    {
        return m_type != type;
    }

    bool operator==(const Token &other) const;
    bool operator!=(const Token &other) const;

    Token &operator=(const Token &other);

    inline Type getType(void) const
    {
        return m_type;
    }

    const char *getIdentifier(void) const;
    float getFloat(void) const;
    int getInt(void) const;
    bool getBool(void) const;

private:
    Type m_type;
    union
    {
        char *identifier;
        float floatValue;
        int intValue;
        bool boolValue;
    } m_arg;
};

inline Token::Token(void) : m_type(TYPE_LAST)
{
    m_arg.identifier = nullptr;
}

inline Token::Token(Type type) : m_type(type)
{
    DE_ASSERT(type != IDENTIFIER);
}

inline Token::Token(float value) : m_type(FLOAT_LITERAL)
{
    m_arg.floatValue = value;
}

inline Token::Token(int value) : m_type(INT_LITERAL)
{
    m_arg.intValue = value;
}

inline Token::Token(bool value) : m_type(BOOL_LITERAL)
{
    m_arg.boolValue = value;
}

inline bool Token::operator==(const Token &other) const
{
    return !(*this != other);
}

inline const char *Token::getIdentifier(void) const
{
    DE_ASSERT(m_type == IDENTIFIER);
    return m_arg.identifier;
}

inline float Token::getFloat(void) const
{
    DE_ASSERT(m_type == FLOAT_LITERAL);
    return m_arg.floatValue;
}

inline int Token::getInt(void) const
{
    DE_ASSERT(m_type == INT_LITERAL);
    return m_arg.intValue;
}

inline bool Token::getBool(void) const
{
    DE_ASSERT(m_type == BOOL_LITERAL);
    return m_arg.boolValue;
}

class TokenStream
{
public:
    TokenStream(void);
    ~TokenStream(void);

    int getSize(void) const
    {
        return (int)m_numTokens;
    }
    const Token &operator[](int ndx) const
    {
        return m_tokens[ndx];
    }

    TokenStream &operator<<(const Token &token);

private:
    enum
    {
        ALLOC_SIZE = 64
    };

    std::vector<Token> m_tokens;
    size_t m_numTokens;
};

inline TokenStream &TokenStream::operator<<(const Token &token)
{
    if (m_tokens.size() == m_numTokens)
        m_tokens.resize(m_numTokens + ALLOC_SIZE);

    m_tokens[m_numTokens] = token;
    m_numTokens += 1;

    return *this;
}

} // namespace rsg

#endif // _RSGTOKEN_HPP
