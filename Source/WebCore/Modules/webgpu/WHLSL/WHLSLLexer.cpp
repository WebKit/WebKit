/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WHLSLLexer.h"

#if ENABLE(WEBGPU)

namespace WebCore {

namespace WHLSL {

const char* Token::typeName(Type type)
{
    switch (type) {
    case Type::IntLiteral:
        return "int literal";
    case Type::UintLiteral:
        return "uint literal";
    case Type::FloatLiteral:
        return "float literal";
    case Type::Struct:
        return "struct";
    case Type::Typedef:
        return "typedef";
    case Type::Enum:
        return "enum";
    case Type::Operator:
        return "operator";
    case Type::If:
        return "if";
    case Type::Else:
        return "else";
    case Type::Continue:
        return "continue";
    case Type::Break:
        return "break";
    case Type::Switch:
        return "switch";
    case Type::Case:
        return "case";
    case Type::Default:
        return "default";
    case Type::Fallthrough:
        return "fallthrough";
    case Type::For:
        return "for";
    case Type::While:
        return "while";
    case Type::Do:
        return "do";
    case Type::Return:
        return "return";
    case Type::Trap:
        return "trap";
    case Type::Null:
        return "null";
    case Type::True:
        return "true";
    case Type::False:
        return "false";
    case Type::Constant:
        return "constant";
    case Type::Device:
        return "device";
    case Type::Threadgroup:
        return "threadgroup";
    case Type::Thread:
        return "thread";
    case Type::Space:
        return "space";
    case Type::Vertex:
        return "vertex";
    case Type::Fragment:
        return "fragment";
    case Type::Compute:
        return "compute";
    case Type::NumThreads:
        return "numthreads";
    case Type::SVInstanceID:
        return "SV_InstanceID";
    case Type::SVVertexID:
        return "SV_VertexID";
    case Type::PSize:
        return "PSIZE";
    case Type::SVPosition:
        return "SV_Position";
    case Type::SVIsFrontFace:
        return "SV_IsFrontFace";
    case Type::SVSampleIndex:
        return "SV_SampleIndex";
    case Type::SVInnerCoverage:
        return "SV_InnerCoverage";
    case Type::SVTarget:
        return "SV_Target";
    case Type::SVDepth:
        return "SV_Depth";
    case Type::SVCoverage:
        return "SV_Coverage";
    case Type::SVDispatchThreadID:
        return "SV_DispatchThreadID";
    case Type::SVGroupID:
        return "SV_GroupID";
    case Type::SVGroupIndex:
        return "SV_GroupIndex";
    case Type::SVGroupThreadID:
        return "SV_GroupThreadID";
    case Type::Attribute:
        return "SV_Attribute";
    case Type::Register:
        return "register";
    case Type::Specialized:
        return "specialized";
    case Type::Native:
        return "native";
    case Type::Restricted:
        return "restricted";
    case Type::Underscore:
        return "_";
    case Type::Auto:
        return "auto";
    case Type::Protocol:
        return "protocol";
    case Type::Const:
        return "const";
    case Type::Static:
        return "static";
    case Type::Qualifier:
        return "qualifier";
    case Type::Identifier:
        return "identifier";
    case Type::OperatorName:
        return "operator name";
    case Type::EqualsSign:
        return "=";
    case Type::Semicolon:
        return ";";
    case Type::LeftCurlyBracket:
        return "{";
    case Type::RightCurlyBracket:
        return "}";
    case Type::Colon:
        return ":";
    case Type::Comma:
        return ",";
    case Type::LeftParenthesis:
        return "(";
    case Type::RightParenthesis:
        return ")";
    case Type::SquareBracketPair:
        return "[]";
    case Type::LeftSquareBracket:
        return "[";
    case Type::RightSquareBracket:
        return "]";
    case Type::Star:
        return "*";
    case Type::LessThanSign:
        return "<";
    case Type::GreaterThanSign:
        return ">";
    case Type::FullStop:
        return ".";
    case Type::PlusEquals:
        return "+=";
    case Type::MinusEquals:
        return "-=";
    case Type::TimesEquals:
        return "*=";
    case Type::DivideEquals:
        return "/=";
    case Type::ModEquals:
        return "%=";
    case Type::XorEquals:
        return "^=";
    case Type::AndEquals:
        return "&=";
    case Type::OrEquals:
        return "|=";
    case Type::RightShiftEquals:
        return ">>=";
    case Type::LeftShiftEquals:
        return "<<=";
    case Type::PlusPlus:
        return "++";
    case Type::MinusMinus:
        return "--";
    case Type::Arrow:
        return "->";
    case Type::QuestionMark:
        return "?";
    case Type::OrOr:
        return "||";
    case Type::AndAnd:
        return "&&";
    case Type::Or:
        return "|";
    case Type::Xor:
        return "^";
    case Type::And:
        return "&";
    case Type::LessThanOrEqualTo:
        return "<=";
    case Type::GreaterThanOrEqualTo:
        return ">=";
    case Type::EqualComparison:
        return "==";
    case Type::NotEqual:
        return "!=";
    case Type::RightShift:
        return ">>";
    case Type::LeftShift:
        return "<<";
    case Type::Plus:
        return "+";
    case Type::Minus:
        return "-";
    case Type::Divide:
        return "/";
    case Type::Mod:
        return "%";
    case Type::Tilde:
        return "~";
    case Type::ExclamationPoint:
        return "!";
    case Type::At:
        return "@";
    case Type::EndOfFile:
        return "EOF";
    case Type::Invalid:
        return "LEXING_ERROR";
    }
}

auto Lexer::recognizeKeyword(unsigned end) -> Optional<Token::Type>
{
    auto substring = m_stringView.substring(m_offset, end - m_offset);
    if (substring == "struct")
        return Token::Type::Struct;
    if (substring == "typedef")
        return Token::Type::Typedef;
    if (substring == "enum")
        return Token::Type::Enum;
    if (substring == "operator")
        return Token::Type::Operator;
    if (substring == "if")
        return Token::Type::If;
    if (substring == "else")
        return Token::Type::Else;
    if (substring == "continue")
        return Token::Type::Continue;
    if (substring == "break")
        return Token::Type::Break;
    if (substring == "switch")
        return Token::Type::Switch;
    if (substring == "case")
        return Token::Type::Case;
    if (substring == "default")
        return Token::Type::Default;
    if (substring == "fallthrough")
        return Token::Type::Fallthrough;
    if (substring == "for")
        return Token::Type::For;
    if (substring == "while")
        return Token::Type::While;
    if (substring == "do")
        return Token::Type::Do;
    if (substring == "return")
        return Token::Type::Return;
    if (substring == "trap")
        return Token::Type::Trap;
    if (substring == "null")
        return Token::Type::Null;
    if (substring == "true")
        return Token::Type::True;
    if (substring == "false")
        return Token::Type::False;
    if (substring == "constant")
        return Token::Type::Constant;
    if (substring == "device")
        return Token::Type::Device;
    if (substring == "threadgroup")
        return Token::Type::Threadgroup;
    if (substring == "thread")
        return Token::Type::Thread;
    if (substring == "space")
        return Token::Type::Space;
    if (substring == "vertex")
        return Token::Type::Vertex;
    if (substring == "fragment")
        return Token::Type::Fragment;
    if (substring == "compute")
        return Token::Type::Compute;
    if (substring == "numthreads")
        return Token::Type::NumThreads;
    if (substring == "SV_InstanceID")
        return Token::Type::SVInstanceID;
    if (substring == "SV_VertexID")
        return Token::Type::SVVertexID;
    if (substring == "PSIZE")
        return Token::Type::PSize;
    if (substring == "SV_Position")
        return Token::Type::SVPosition;
    if (substring == "SV_IsFrontFace")
        return Token::Type::SVIsFrontFace;
    if (substring == "SV_SampleIndex")
        return Token::Type::SVSampleIndex;
    if (substring == "SV_InnerCoverage")
        return Token::Type::SVInnerCoverage;
    if (substring == "SV_Target") // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195807 Make this work with strings like "SV_Target0".
        return Token::Type::SVTarget;
    if (substring == "SV_Depth")
        return Token::Type::SVDepth;
    if (substring == "SV_Coverage")
        return Token::Type::SVCoverage;
    if (substring == "SV_DispatchThreadID")
        return Token::Type::SVDispatchThreadID;
    if (substring == "SV_GroupID")
        return Token::Type::SVGroupID;
    if (substring == "SV_GroupIndex")
        return Token::Type::SVGroupIndex;
    if (substring == "SV_GroupThreadID")
        return Token::Type::SVGroupThreadID;
    if (substring == "attribute")
        return Token::Type::Attribute;
    if (substring == "register")
        return Token::Type::Register;
    if (substring == "specialized")
        return Token::Type::Specialized;
    if (substring == "native")
        return Token::Type::Native;
    if (substring == "restricted")
        return Token::Type::Restricted;
    if (substring == "_")
        return Token::Type::Underscore;
    if (substring == "auto")
        return Token::Type::Auto;
    if (substring == "protocol")
        return Token::Type::Protocol;
    if (substring == "const")
        return Token::Type::Const;
    if (substring == "static")
        return Token::Type::Static;
    if (substring == "nointerpolation")
        return Token::Type::Qualifier;
    if (substring == "noperspective")
        return Token::Type::Qualifier;
    if (substring == "uniform")
        return Token::Type::Qualifier;
    if (substring == "centroid")
        return Token::Type::Qualifier;
    if (substring == "sample")
        return Token::Type::Qualifier;
    return WTF::nullopt;
}

auto Lexer::consumeTokenFromStream() -> Token
{
    auto prepare = [&](unsigned newOffset, Token::Type type) -> Token {
        auto oldOffset = m_offset;
        m_offset = newOffset;
        skipWhitespaceAndComments();
        return { { oldOffset, newOffset }, type };
    };

    if (auto newOffset = identifier(m_offset)) {
        if (auto result = recognizeKeyword(*newOffset)) {
            if (*result == Token::Type::Operator) {
                if (auto newerOffset = completeOperatorName(*newOffset))
                    return prepare(*newerOffset, Token::Type::OperatorName);
            }
            return prepare(*newOffset, *result);
        }
        return prepare(*newOffset, Token::Type::Identifier);
    }
    if (auto newOffset = floatLiteral(m_offset))
        return prepare(*newOffset, Token::Type::FloatLiteral);
    if (auto newOffset = uintLiteral(m_offset))
        return prepare(*newOffset, Token::Type::UintLiteral);
    if (auto newOffset = intLiteral(m_offset))
        return prepare(*newOffset, Token::Type::IntLiteral);
    // Sorted by length, so longer matches are preferable to shorter matches.
    if (auto newOffset = string(">>=", m_offset))
        return prepare(*newOffset, Token::Type::RightShiftEquals);
    if (auto newOffset = string("<<=", m_offset))
        return prepare(*newOffset, Token::Type::LeftShiftEquals);
    if (auto newOffset = string("+=", m_offset))
        return prepare(*newOffset, Token::Type::PlusEquals);
    if (auto newOffset = string("-=", m_offset))
        return prepare(*newOffset, Token::Type::MinusEquals);
    if (auto newOffset = string("*=", m_offset))
        return prepare(*newOffset, Token::Type::TimesEquals);
    if (auto newOffset = string("/=", m_offset))
        return prepare(*newOffset, Token::Type::DivideEquals);
    if (auto newOffset = string("%=", m_offset))
        return prepare(*newOffset, Token::Type::ModEquals);
    if (auto newOffset = string("^=", m_offset))
        return prepare(*newOffset, Token::Type::XorEquals);
    if (auto newOffset = string("&=", m_offset))
        return prepare(*newOffset, Token::Type::AndEquals);
    if (auto newOffset = string("|=", m_offset))
        return prepare(*newOffset, Token::Type::OrEquals);
    if (auto newOffset = string("++", m_offset))
        return prepare(*newOffset, Token::Type::PlusPlus);
    if (auto newOffset = string("--", m_offset))
        return prepare(*newOffset, Token::Type::MinusMinus);
    if (auto newOffset = string("->", m_offset))
        return prepare(*newOffset, Token::Type::Arrow);
    if (auto newOffset = string("[]", m_offset))
        return prepare(*newOffset, Token::Type::SquareBracketPair);
    if (auto newOffset = string("||", m_offset))
        return prepare(*newOffset, Token::Type::OrOr);
    if (auto newOffset = string("&&", m_offset))
        return prepare(*newOffset, Token::Type::AndAnd);
    if (auto newOffset = string("<=", m_offset))
        return prepare(*newOffset, Token::Type::LessThanOrEqualTo);
    if (auto newOffset = string(">=", m_offset))
        return prepare(*newOffset, Token::Type::GreaterThanOrEqualTo);
    if (auto newOffset = string("==", m_offset))
        return prepare(*newOffset, Token::Type::EqualComparison);
    if (auto newOffset = string("!=", m_offset))
        return prepare(*newOffset, Token::Type::NotEqual);
    if (auto newOffset = string(">>", m_offset))
        return prepare(*newOffset, Token::Type::RightShift);
    if (auto newOffset = string("<<", m_offset))
        return prepare(*newOffset, Token::Type::LeftShift);
    if (auto newOffset = character('=', m_offset))
        return prepare(*newOffset, Token::Type::EqualsSign);
    if (auto newOffset = character(';', m_offset))
        return prepare(*newOffset, Token::Type::Semicolon);
    if (auto newOffset = character('{', m_offset))
        return prepare(*newOffset, Token::Type::LeftCurlyBracket);
    if (auto newOffset = character('}', m_offset))
        return prepare(*newOffset, Token::Type::RightCurlyBracket);
    if (auto newOffset = character(':', m_offset))
        return prepare(*newOffset, Token::Type::Colon);
    if (auto newOffset = character(',', m_offset))
        return prepare(*newOffset, Token::Type::Comma);
    if (auto newOffset = character('(', m_offset))
        return prepare(*newOffset, Token::Type::LeftParenthesis);
    if (auto newOffset = character(')', m_offset))
        return prepare(*newOffset, Token::Type::RightParenthesis);
    if (auto newOffset = character('[', m_offset))
        return prepare(*newOffset, Token::Type::LeftSquareBracket);
    if (auto newOffset = character(']', m_offset))
        return prepare(*newOffset, Token::Type::RightSquareBracket);
    if (auto newOffset = character('*', m_offset))
        return prepare(*newOffset, Token::Type::Star);
    if (auto newOffset = character('<', m_offset))
        return prepare(*newOffset, Token::Type::LessThanSign);
    if (auto newOffset = character('>', m_offset))
        return prepare(*newOffset, Token::Type::GreaterThanSign);
    if (auto newOffset = character('.', m_offset))
        return prepare(*newOffset, Token::Type::FullStop);
    if (auto newOffset = character('?', m_offset))
        return prepare(*newOffset, Token::Type::QuestionMark);
    if (auto newOffset = character('|', m_offset))
        return prepare(*newOffset, Token::Type::Or);
    if (auto newOffset = character('^', m_offset))
        return prepare(*newOffset, Token::Type::Xor);
    if (auto newOffset = character('&', m_offset))
        return prepare(*newOffset, Token::Type::And);
    if (auto newOffset = character('+', m_offset))
        return prepare(*newOffset, Token::Type::Plus);
    if (auto newOffset = character('-', m_offset))
        return prepare(*newOffset, Token::Type::Minus);
    if (auto newOffset = character('/', m_offset))
        return prepare(*newOffset, Token::Type::Divide);
    if (auto newOffset = character('%', m_offset))
        return prepare(*newOffset, Token::Type::Mod);
    if (auto newOffset = character('~', m_offset))
        return prepare(*newOffset, Token::Type::Tilde);
    if (auto newOffset = character('!', m_offset))
        return prepare(*newOffset, Token::Type::ExclamationPoint);
    if (auto newOffset = character('@', m_offset))
        return prepare(*newOffset, Token::Type::At);

    if (m_offset == m_stringView.length())
        return prepare(m_offset, Token::Type::EndOfFile);
    return prepare(m_offset, Token::Type::Invalid);
}

unsigned Lexer::lineNumberFromOffset(unsigned targetOffset)
{
    // Counting from 1 to match most text editors.
    unsigned lineNumber = 1;
    for (unsigned offset = 0; offset < targetOffset; ++offset) {
        if (m_stringView[offset] == '\n')
            ++lineNumber;
    }
    return lineNumber;
}

// We can take advantage of two properties of Unicode:
// 1. The consitutent UTF-16 code units for all non-BMP code points are surrogates,
// which means we'll never see a false match. If we see a BMP code unit, we
// really have a BMP code point.
// 2. Everything we're looking for is in BMP

static inline bool isWhitespace(UChar codeUnit)
{
    switch (codeUnit) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        return true;
    default:
        return false;
    }
}

static inline bool isNewline(UChar codeUnit)
{
    switch (codeUnit) {
    case '\r':
    case '\n':
        return true;
    default:
        return false;
    }
}

void Lexer::skipWhitespaceAndComments()
{
    while (m_offset < m_stringView.length()) {
        if (isWhitespace(m_stringView[m_offset]))
            ++m_offset;
        else if (m_stringView[m_offset] == '/' && m_offset + 1 < m_stringView.length()) {
            if (m_stringView[m_offset + 1] == '/') {
                // Line comment
                m_offset += 2;
                // Note that in the case of \r\n this makes the comment end on the \r. It is fine, as the \n after that is simple whitespace.
                for ( ; m_offset < m_stringView.length() && !isNewline(m_stringView[m_offset]); ++m_offset) { }
            } else if (m_stringView[m_offset + 1] == '*') {
                // Long comment
                for ( ; m_offset < m_stringView.length() ; ++m_offset) {
                    if (m_stringView[m_offset] == '*' && m_offset + 1 < m_stringView.length() && m_stringView[m_offset + 1] == '/') {
                        m_offset += 2;
                        break;
                    }
                }
            }
        } else
            break;
    }
}

// Regular expression are unnecessary; we shouldn't need to compile them.

Optional<unsigned> Lexer::coreDecimalIntLiteral(unsigned offset) const
{
    if (offset >= m_stringView.length())
        return WTF::nullopt;
    if (m_stringView[offset] == '0')
        return offset + 1;
    if (m_stringView[offset] >= '1' && m_stringView[offset] <= '9') {
        ++offset;
        for ( ; offset < m_stringView.length() && m_stringView[offset] >= '0' && m_stringView[offset] <= '9'; ++offset) {
        }
        return offset;
    }
    return WTF::nullopt;
}

Optional<unsigned> Lexer::decimalIntLiteral(unsigned offset) const
{
    if (offset < m_stringView.length() && m_stringView[offset] == '-')
        ++offset;
    return coreDecimalIntLiteral(offset);
}

Optional<unsigned> Lexer::decimalUintLiteral(unsigned offset) const
{
    auto result = coreDecimalIntLiteral(offset);
    if (!result)
        return WTF::nullopt;
    if (*result < m_stringView.length() && m_stringView[*result] == 'u')
        return *result + 1;
    return WTF::nullopt;
}

static inline bool isHexadecimalCharacter(UChar character)
{
    return (character >= '0' && character <= '9')
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F');
}

Optional<unsigned> Lexer::coreHexadecimalIntLiteral(unsigned offset) const
{
    if (offset + 1 >= m_stringView.length() || m_stringView[offset] != '0' || m_stringView[offset + 1] != 'x')
        return WTF::nullopt;

    offset += 2;
    if (offset >= m_stringView.length() || !isHexadecimalCharacter(m_stringView[offset]))
        return WTF::nullopt;
    ++offset;
    for ( ; offset < m_stringView.length() && isHexadecimalCharacter(m_stringView[offset]); ++offset) {
    }
    return offset;
}

Optional<unsigned> Lexer::hexadecimalIntLiteral(unsigned offset) const
{
    if (offset < m_stringView.length() && m_stringView[offset] == '-')
        ++offset;
    return coreHexadecimalIntLiteral(offset);
}

Optional<unsigned> Lexer::hexadecimalUintLiteral(unsigned offset) const
{
    auto result = coreHexadecimalIntLiteral(offset);
    if (!result)
        return WTF::nullopt;
    if (*result < m_stringView.length() && m_stringView[*result] == 'u')
        return *result + 1;
    return WTF::nullopt;
}

Optional<unsigned> Lexer::intLiteral(unsigned offset) const
{
    if (auto result = decimalIntLiteral(offset))
        return result;
    if (auto result = hexadecimalIntLiteral(offset))
        return result;
    return WTF::nullopt;
}

Optional<unsigned> Lexer::uintLiteral(unsigned offset) const
{
    if (auto result = decimalUintLiteral(offset))
        return result;
    if (auto result = hexadecimalUintLiteral(offset))
        return result;
    return WTF::nullopt;
}

Optional<unsigned> Lexer::digit(unsigned offset) const
{
    if (offset < m_stringView.length() && m_stringView[offset] >= '0' && m_stringView[offset] <= '9')
        return offset + 1;
    return WTF::nullopt;
}

unsigned Lexer::digitStar(unsigned offset) const
{
    while (auto result = digit(offset))
        offset = *result;
    return offset;
}

Optional<unsigned> Lexer::character(char character, unsigned offset) const
{
    if (offset < m_stringView.length() && m_stringView[offset] == character)
        return offset + 1;
    return WTF::nullopt;
}

Optional<unsigned> Lexer::coreFloatLiteralType1(unsigned offset) const
{
    auto result = digit(offset);
    if (!result)
        return WTF::nullopt;
    auto result2 = digitStar(*result);
    auto result3 = character('.', result2);
    if (!result3)
        return WTF::nullopt;
    return digitStar(*result3);
}

Optional<unsigned> Lexer::coreFloatLiteral(unsigned offset) const
{
    if (auto type1 = coreFloatLiteralType1(offset))
        return type1;
    auto result = digitStar(offset);
    auto result2 = character('.', result);
    if (!result2)
        return WTF::nullopt;
    auto result3 = digit(*result2);
    if (!result3)
        return WTF::nullopt;
    return digitStar(*result3);
}

Optional<unsigned> Lexer::floatLiteral(unsigned offset) const
{
    if (offset < m_stringView.length() && m_stringView[offset] == '-')
        ++offset;
    auto result = coreFloatLiteral(offset);
    if (!result)
        return WTF::nullopt;
    offset = *result;
    if (offset < m_stringView.length() && m_stringView[offset] == 'f')
        ++offset;
    return offset;
}

Optional<unsigned> Lexer::validIdentifier(unsigned offset) const
{
    if (offset >= m_stringView.length()
        || !((m_stringView[offset] >= 'a' && m_stringView[offset] <= 'z')
            || (m_stringView[offset] >= 'A' && m_stringView[offset] <= 'Z')
            || (m_stringView[offset] == '_')))
        return WTF::nullopt;
    ++offset;
    while (true) {
        if (offset >= m_stringView.length()
            || !((m_stringView[offset] >= 'a' && m_stringView[offset] <= 'z')
                || (m_stringView[offset] >= 'A' && m_stringView[offset] <= 'Z')
                || (m_stringView[offset] >= '0' && m_stringView[offset] <= '9')
                || (m_stringView[offset] == '_')))
            return offset;
        ++offset;
    }
}

Optional<unsigned> Lexer::identifier(unsigned offset) const
{
    return validIdentifier(offset);
}

Optional<unsigned> Lexer::completeOperatorName(unsigned offset) const
{
    // Sorted by length, so longer matches are preferable to shorter matches.
    if (auto result = string("&[]", offset))
        return result;
    if (auto result = string("[]=", offset))
        return result;
    if (auto result = string(">>", offset))
        return result;
    if (auto result = string("<<", offset))
        return result;
    if (auto result = string("++", offset))
        return result;
    if (auto result = string("--", offset))
        return result;
    if (auto result = string("&&", offset))
        return result;
    if (auto result = string("||", offset))
        return result;
    if (auto result = string(">=", offset))
        return result;
    if (auto result = string("<=", offset))
        return result;
    if (auto result = string("==", offset))
        return result;
    if (auto result = string("[]", offset))
        return result;
    if (auto result = string("&.", offset))
        return validIdentifier(*result);
    if (auto result = character('+', offset))
        return result;
    if (auto result = character('-', offset))
        return result;
    if (auto result = character('*', offset))
        return result;
    if (auto result = character('/', offset))
        return result;
    if (auto result = character('%', offset))
        return result;
    if (auto result = character('<', offset))
        return result;
    if (auto result = character('>', offset))
        return result;
    if (auto result = character('!', offset))
        return result;
    if (auto result = character('~', offset))
        return result;
    if (auto result = character('&', offset))
        return result;
    if (auto result = character('^', offset))
        return result;
    if (auto result = character('|', offset))
        return result;
    if (auto result = character('.', offset)) {
        if (auto result2 = validIdentifier(*result)) {
            if (auto result3 = character('=', *result2))
                return result3;
            return *result2;
        }
    }
    return WTF::nullopt;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
