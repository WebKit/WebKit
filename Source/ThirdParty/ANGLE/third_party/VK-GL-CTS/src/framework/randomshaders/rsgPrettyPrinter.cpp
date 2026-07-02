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
 * \brief Shader Source Formatter.
 *//*--------------------------------------------------------------------*/

#include "rsgPrettyPrinter.hpp"
#include "deStringUtil.hpp"

namespace rsg
{

static const char *s_tokenStr[] = {
    nullptr,       // IDENTIFIER,
    "struct",      // STRUCT,
    "invariant",   // INVARIANT,
    "precision",   // PRECISION,
    "void",        // VOID,
    "break",       // BREAK,
    "continue",    // CONTINUE,
    "do ",         // DO,
    "while ",      // WHILE,
    "else ",       // ELSE,
    "for ",        // FOR,
    "if ",         // IF,
    "discard",     // DISCARD,
    "return ",     // RETURN,
    "++",          // INC_OP,
    "--",          // DEC_OP,
    "(",           // LEFT_PAREN,
    ")",           // RIGHT_PAREN,
    "[",           // LEFT_BRACKET,
    "]",           // RIGHT_BRACKET,
    "{",           // LEFT_BRACE,
    "}",           // RIGHT_BRACE,
    ".",           // DOT,
    ", ",          // COMMA,
    " : ",         // COLON,
    ";",           // SEMICOLON,
    " - ",         // MINUS,
    " + ",         // PLUS,
    " * ",         // MUL,
    " / ",         // DIV,
    " % ",         // MOD,
    " ? ",         // QUESTION,
    "bool",        // BOOL,
    "bvec2",       // BVEC2,
    "bvec3",       // BVEC3,
    "bvec4",       // BVEC4,
    "int",         // INT,
    "ivec2",       // IVEC2,
    "ivec3",       // IVEC3,
    "ivec4",       // IVEC4,
    "float",       // FLOAT,
    "vec2",        // VEC2,
    "vec3",        // VEC3,
    "vec4",        // VEC4,
    "mat2",        // MAT2,
    "mat3",        // MAT3,
    "mat4",        // MAT4,
    "sampler2D",   // SAMPLER2D,
    "samplerCube", // SAMPLERCUBE,
    nullptr,       // FLOAT_LITERAL,
    nullptr,       // INT_LITERAL,
    nullptr,       // BOOL_LITERAL,
    " = ",         // EQUAL,
    " *= ",        // MUL_ASSIGN,
    " /= ",        // DIV_ASSIGN,
    " += ",        // ADD_ASSIGN,
    " -= ",        // SUB_ASSIGN,
    " < ",         // CMP_LT,
    " > ",         // CMP_GT,
    " <= ",        // CMP_LE,
    " >= ",        // CMP_GE,
    " == ",        // CMP_EQ,
    " != ",        // CMP_NE,
    " && ",        // LOGICAL_AND,
    " || ",        // LOGICAL_OR,
    "!",           // LOGICAL_NOT,
    " ^^ ",        // LOGICAL_XOR,
    "attribute",   // ATTRIBUTE,
    "uniform",     // UNIFORM,
    "varying",     // VARYING,
    "const",       // CONST,
    "flat",        // FLAT,
    "highp",       // HIGH_PRECISION,
    "mediump",     // MEDIUM_PRECISION,
    "lowp",        // LOW_PRECISION,
    "in",          // IN,
    "out",         // OUT,
    "inout",       // INOUT,
    "layout",      // LAYOUT,
    "location",    // LOCATION,
    nullptr,       // INDENT_INC,
    nullptr,       // INDENT_DEC,
    "\n"           // NEWLINE,
};

PrettyPrinter::PrettyPrinter(std::ostringstream &str) : m_str(str), m_indentDepth(0)
{
}

inline const char *PrettyPrinter::getSimpleTokenStr(Token::Type token)
{
    DE_ASSERT(de::inBounds<int>(token, 0, (int)DE_LENGTH_OF_ARRAY(s_tokenStr)));
    return s_tokenStr[token];
}

void PrettyPrinter::append(const TokenStream &tokens)
{
    for (int ndx = 0; ndx < tokens.getSize(); ndx++)
        processToken(tokens[ndx]);
}

inline bool isIdentifierChar(char c)
{
    return de::inRange(c, 'a', 'z') || de::inRange(c, 'A', 'Z') || de::inRange(c, '0', '9') || c == '_';
}

void PrettyPrinter::processToken(const Token &token)
{
    bool prevIsIdentifierChar = m_line.length() > 0 && isIdentifierChar(m_line[m_line.length() - 1]);

    switch (token.getType())
    {
    case Token::IDENTIFIER:
        if (prevIsIdentifierChar)
            m_line += " ";
        m_line += token.getIdentifier();
        break;

    case Token::FLOAT_LITERAL:
    {
        std::string f = de::toString(token.getFloat());
        if (f.find('.') == std::string::npos)
            f += ".0"; // Make sure value parses as float
        m_line += f;
        break;
    }

    case Token::INT_LITERAL:
        m_line += de::toString(token.getInt());
        break;

    case Token::BOOL_LITERAL:
        m_line += (token.getBool() ? "true" : "false");
        break;

    case Token::INDENT_INC:
        m_indentDepth += 1;
        break;

    case Token::INDENT_DEC:
        m_indentDepth -= 1;
        break;

    case Token::NEWLINE:
        // Indent
        for (int i = 0; i < m_indentDepth; i++)
            m_str << "\t";

        // Flush line to source
        m_str << m_line + "\n";
        m_line = "";
        break;

    default:
    {
        const char *tokenStr = getSimpleTokenStr(token.getType());
        if (prevIsIdentifierChar && isIdentifierChar(tokenStr[0]))
            m_line += " ";
        m_line += tokenStr;
        break;
    }
    }
}

} // namespace rsg
