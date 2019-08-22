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

static ALWAYS_INLINE bool isValidIdentifierStart(UChar theChar)
{
    return (theChar >= 'a' && theChar <= 'z')
        || (theChar >= 'A' && theChar <= 'Z')
        || (theChar == '_');
}

static ALWAYS_INLINE bool isValidNonStartingIdentifierChar(UChar theChar)
{
    return (theChar >= 'a' && theChar <= 'z')
        || (theChar >= 'A' && theChar <= 'Z')
        || (theChar >= '0' && theChar <= '9')
        || (theChar == '_');
}

static ALWAYS_INLINE bool isHexadecimalCharacter(UChar character)
{
    return (character >= '0' && character <= '9')
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F');
}

static ALWAYS_INLINE bool isDigit(UChar theChar)
{
    return theChar >= '0' && theChar <= '9';
}

auto Lexer::consumeTokenFromStream() -> Token
{
    UChar current = 0;
    unsigned offset = m_offset;

    auto peek = [&] () -> UChar {
        if (offset < m_stringView.length())
            return m_stringView[offset];
        return 0;
    };

    auto shift = [&] {
        if (offset < m_stringView.length()) {
            current = m_stringView[offset];
            ++offset;
            return current;
        }
        current = 0;
        return current;
    };

    auto consume = [&] (UChar theChar) {
        if (peek() == theChar) {
            shift();
            return true;
        }
        return false;
    };

    auto nextIsIdentifier = [&] {
        return isValidNonStartingIdentifierChar(peek());
    };

    auto token = [&] (Token::Type type) -> Token {
        auto oldOffset = m_offset;
        m_offset = offset;
        skipWhitespaceAndComments();
        return { { oldOffset, offset, m_nameSpace }, type };
    };

    switch (shift()) {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'Q':
    case 'R':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case 'g':
    case 'h':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'q':
    case 'x':
    case 'y':
    case 'z':
parseIdentifier:
        while (isValidNonStartingIdentifierChar(peek()))
            shift();
        return token(Token::Type::Identifier);

    case 's':
        switch (peek()) {
        case 'a':
            shift();
            if (!consume('m'))
                goto parseIdentifier;
            if (!consume('p'))
                goto parseIdentifier;
            if (!consume('l'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Qualifier);

        case 'w':
            shift();
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('c'))
                goto parseIdentifier;
            if (!consume('h'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Switch);

        case 't':
            shift();
            switch (peek()) {
            case 'r':
                shift();
                if (!consume('u'))
                    goto parseIdentifier;
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Struct);
            case 'a':
                shift();
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('i'))
                    goto parseIdentifier;
                if (!consume('c'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Static);
            default:
                goto parseIdentifier;
            }
            break;

        case 'p':
            shift();
            switch (peek()) {
            case 'a':
                shift();
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Space);
            case 'e':
                shift();
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('i'))
                    goto parseIdentifier;
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('l'))
                    goto parseIdentifier;
                if (!consume('i'))
                    goto parseIdentifier;
                if (!consume('z'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (!consume('d'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Specialized);
            default:
                goto parseIdentifier;
            }
        default:
            goto parseIdentifier;
        }

    case 'S':
        if (!consume('V'))
            goto parseIdentifier;
        if (!consume('_'))
            goto parseIdentifier;

        switch (peek()) {
        case 'G':
            shift();
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('o'))
                goto parseIdentifier;
            if (!consume('u'))
                goto parseIdentifier;
            if (!consume('p'))
                goto parseIdentifier;

            switch (peek()) {
            case 'I':
                shift();
                switch (peek()) {
                case 'D':
                    shift();
                    if (nextIsIdentifier())
                        goto parseIdentifier;
                    return token(Token::Type::SVGroupID);
                case 'n':
                    shift();
                    if (!consume('d'))
                        goto parseIdentifier;
                    if (!consume('e'))
                        goto parseIdentifier;
                    if (!consume('x'))
                        goto parseIdentifier;
                    if (nextIsIdentifier())
                        goto parseIdentifier;
                    return token(Token::Type::SVGroupIndex);

                default:
                    goto parseIdentifier;
                }

            case 'T':
                shift();
                if (!consume('h'))
                    goto parseIdentifier;
                if (!consume('r'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('d'))
                    goto parseIdentifier;
                if (!consume('I'))
                    goto parseIdentifier;
                if (!consume('D'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::SVGroupThreadID);

            default:
                goto parseIdentifier;
            }
        case 'D':
            shift();
            switch (peek()) {
            case 'e':
                shift();
                if (!consume('p'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('h'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::SVDepth);
            case 'i':
                shift();
                if (!consume('s'))
                    goto parseIdentifier;
                if (!consume('p'))
                    goto parseIdentifier;
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('h'))
                    goto parseIdentifier;
                if (!consume('T'))
                    goto parseIdentifier;
                if (!consume('h'))
                    goto parseIdentifier;
                if (!consume('r'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('d'))
                    goto parseIdentifier;
                if (!consume('I'))
                    goto parseIdentifier;
                if (!consume('D'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::SVDispatchThreadID);
            default:
                goto parseIdentifier;
            }
        case 'C':
            shift();
            if (!consume('o'))
                goto parseIdentifier;
            if (!consume('v'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('a'))
                goto parseIdentifier;
            if (!consume('g'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::SVCoverage);
        case 'T':
            shift();
            if (!consume('a'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('g'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::SVTarget);
        case 'S':
            shift();
            if (!consume('a'))
                goto parseIdentifier;
            if (!consume('m'))
                goto parseIdentifier;
            if (!consume('p'))
                goto parseIdentifier;
            if (!consume('l'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('I'))
                goto parseIdentifier;
            if (!consume('n'))
                goto parseIdentifier;
            if (!consume('d'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('x'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::SVSampleIndex);
        case 'P':
            shift();
            if (!consume('o'))
                goto parseIdentifier;
            if (!consume('s'))
                goto parseIdentifier;
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('o'))
                goto parseIdentifier;
            if (!consume('n'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::SVPosition);
        case 'V':
            shift();
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('x'))
                goto parseIdentifier;
            if (!consume('I'))
                goto parseIdentifier;
            if (!consume('D'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::SVVertexID);
        case 'I':
            shift();
            switch (peek()) {
            case 's':
                shift();
                if (!consume('F'))
                    goto parseIdentifier;
                if (!consume('r'))
                    goto parseIdentifier;
                if (!consume('o'))
                    goto parseIdentifier;
                if (!consume('n'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('F'))
                    goto parseIdentifier;
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::SVIsFrontFace);
            case 'n':
                shift();
                switch (peek()) {
                case 'n':
                    shift();
                    if (!consume('e'))
                        goto parseIdentifier;
                    if (!consume('r'))
                        goto parseIdentifier;
                    if (!consume('C'))
                        goto parseIdentifier;
                    if (!consume('o'))
                        goto parseIdentifier;
                    if (!consume('v'))
                        goto parseIdentifier;
                    if (!consume('e'))
                        goto parseIdentifier;
                    if (!consume('r'))
                        goto parseIdentifier;
                    if (!consume('a'))
                        goto parseIdentifier;
                    if (!consume('g'))
                        goto parseIdentifier;
                    if (!consume('e'))
                        goto parseIdentifier;
                    if (nextIsIdentifier())
                        goto parseIdentifier;
                    return token(Token::Type::SVInnerCoverage);
                case 's':
                    shift();
                    if (!consume('t'))
                        goto parseIdentifier;
                    if (!consume('a'))
                        goto parseIdentifier;
                    if (!consume('n'))
                        goto parseIdentifier;
                    if (!consume('c'))
                        goto parseIdentifier;
                    if (!consume('e'))
                        goto parseIdentifier;
                    if (!consume('I'))
                        goto parseIdentifier;
                    if (!consume('D'))
                        goto parseIdentifier;
                    if (nextIsIdentifier())
                        goto parseIdentifier;
                    return token(Token::Type::SVInstanceID);
                default:
                    goto parseIdentifier;
                }

            default:
                goto parseIdentifier;
            }
        default:
            goto parseIdentifier;
        }

    case 't':
        switch (peek()) {
        case 'r':
            shift();
            if (!consume('u'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::True);
        case 'y':
            shift();
            if (!consume('p'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('d'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('f'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Typedef);
        case 'h':
            shift();
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('a'))
                goto parseIdentifier;
            if (!consume('d'))
                goto parseIdentifier;

            if (!nextIsIdentifier())
                return token(Token::Type::Thread);

            if (peek() != 'g')
                goto parseIdentifier;

            shift();
            RELEASE_ASSERT(current == 'g');
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('o'))
                goto parseIdentifier;
            if (!consume('u'))
                goto parseIdentifier;
            if (!consume('p'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Threadgroup);

        default:
            goto parseIdentifier;
        }

    case 'e':
        switch (peek()) {
        case 'n':
            shift();
            if (!consume('u'))
                goto parseIdentifier;
            if (!consume('m'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Enum);
        case 'l':
            shift();
            if (!consume('s'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Else);
        default:
            goto parseIdentifier;
        }

    case 'o':
        if (!consume('p'))
            goto parseIdentifier;
        if (!consume('e'))
            goto parseIdentifier;
        if (!consume('r'))
            goto parseIdentifier;
        if (!consume('a'))
            goto parseIdentifier;
        if (!consume('t'))
            goto parseIdentifier;
        if (!consume('o'))
            goto parseIdentifier;
        if (!consume('r'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;

        switch (peek()) {
        case '&':
            shift();
            switch (peek()) {
            case '[':
                shift();
                if (consume(']'))
                    return token(Token::Type::OperatorName);
                return token(Token::Type::Invalid);
            case '.':
                shift();
                if (!isValidIdentifierStart(peek()))
                    return token(Token::Type::Invalid);
                shift();
                while (isValidNonStartingIdentifierChar(peek()))
                    shift();
                return token(Token::Type::OperatorName);
            default:
                return token(Token::Type::OperatorName);
            }

        case '[':
            shift();
            if (!consume(']'))
                return token(Token::Type::Invalid);
            consume('=');
            return token(Token::Type::OperatorName);

        case '>':
            shift();
            if (consume('>'))
                return token(Token::Type::OperatorName);
            if (consume('='))
                return token(Token::Type::OperatorName);
            return token(Token::Type::OperatorName);

        case '<':
            shift();
            if (consume('<'))
                return token(Token::Type::OperatorName);
            if (consume('='))
                return token(Token::Type::OperatorName);
            return token(Token::Type::OperatorName);

        case '+':
            shift();
            consume('+');
            return token(Token::Type::OperatorName);

        case '-':
            shift();
            consume('-');
            return token(Token::Type::OperatorName);

        case '|':
            shift();
            return token(Token::Type::OperatorName);

        case '=':
            shift();
            if (!consume('='))
                return token(Token::Type::Invalid);
            return token(Token::Type::OperatorName);

        case '*':
            shift();
            return token(Token::Type::OperatorName);

        case '/':
            shift();
            return token(Token::Type::OperatorName);

        case '%':
            shift();
            return token(Token::Type::OperatorName);

        case '!':
            shift();
            return token(Token::Type::OperatorName);

        case '~':
            shift();
            return token(Token::Type::OperatorName);

        case '^':
            shift();
            return token(Token::Type::OperatorName);

        case '.':
            shift();
            if (!isValidIdentifierStart(peek()))
                return token(Token::Type::Invalid);
            shift();
            while (isValidNonStartingIdentifierChar(peek()))
                shift();
            consume('=');
            return token(Token::Type::OperatorName);
        default:
            break;
        }

        return token(Token::Type::Operator);

    case 'i':
        if (!consume('f'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::If);

    case 'c':
        switch (peek()) {
        case 'a':
            shift();
            if (!consume('s'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Case);

        case 'e':
            shift();
            if (!consume('n'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('o'))
                goto parseIdentifier;
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('d'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Qualifier);
        case 'o':
            shift();
            switch (peek()) {
            case 'm':
                shift();
                if (!consume('p'))
                    goto parseIdentifier;
                if (!consume('u'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Compute);
            case 'n':
                shift();
                switch (peek()) {
                case 't':
                    shift();
                    if (!consume('i'))
                        goto parseIdentifier;
                    if (!consume('n'))
                        goto parseIdentifier;
                    if (!consume('u'))
                        goto parseIdentifier;
                    if (!consume('e'))
                        goto parseIdentifier;
                    if (nextIsIdentifier())
                        goto parseIdentifier;
                    return token(Token::Type::Continue);
                case 's':
                    shift();
                    if (!consume('t'))
                        goto parseIdentifier;

                    if (!nextIsIdentifier())
                        return token(Token::Type::Const);

                    if (!consume('a'))
                        goto parseIdentifier;
                    if (!consume('n'))
                        goto parseIdentifier;
                    if (!consume('t'))
                        goto parseIdentifier;
                    if (nextIsIdentifier())
                        goto parseIdentifier;
                    return token(Token::Type::Constant);

                default:
                    goto parseIdentifier;
                }

            default:
                goto parseIdentifier;
            }

        default:
            goto parseIdentifier;
        }

    case 'b':
        if (!consume('r'))
            goto parseIdentifier;
        if (!consume('e'))
            goto parseIdentifier;
        if (!consume('a'))
            goto parseIdentifier;
        if (!consume('k'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::Break);

    case 'd':
        switch (peek()) {
        case 'o':
            shift();
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Do);
        case 'e':
            shift();
            switch (peek()) {
            case 'f':
                shift();
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('u'))
                    goto parseIdentifier;
                if (!consume('l'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Default);
            case 'v':
                shift();
                if (!consume('i'))
                    goto parseIdentifier;
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Device);
            default:
                goto parseIdentifier;
            }

        default:
            goto parseIdentifier;
        }


    case 'f':
        switch (peek()) {
        case 'o':
            shift();
            if (!consume('r'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::For);
        case 'r':
            shift();
            if (!consume('a'))
                goto parseIdentifier;
            if (!consume('g'))
                goto parseIdentifier;
            if (!consume('m'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('n'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Fragment);
        case 'a':
            shift();
            if (!consume('l'))
                goto parseIdentifier;

            switch (peek()) {
            case 'l':
                shift();
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('h'))
                    goto parseIdentifier;
                if (!consume('r'))
                    goto parseIdentifier;
                if (!consume('o'))
                    goto parseIdentifier;
                if (!consume('u'))
                    goto parseIdentifier;
                if (!consume('g'))
                    goto parseIdentifier;
                if (!consume('h'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Fallthrough);

            case 's':
                shift();
                if (!consume('e'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::False);
            default:
                goto parseIdentifier;
            }
        case 'e':
            shift();
            switch (peek()) {
            case 'f':
                shift();
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('u'))
                    goto parseIdentifier;
                if (!consume('l'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Default);
            case 'v':
                shift();
                if (!consume('i'))
                    goto parseIdentifier;
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Device);
            default:
                goto parseIdentifier;
            }

        default:
            goto parseIdentifier;
        }

    case 'w':
        if (!consume('h'))
            goto parseIdentifier;
        if (!consume('i'))
            goto parseIdentifier;
        if (!consume('l'))
            goto parseIdentifier;
        if (!consume('e'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::While);

    case 'r':
        if (!consume('e'))
            goto parseIdentifier;
        switch (peek()) {
        case 't':
            shift();
            if (!consume('u'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('n'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Return);
        case 'g':
            shift();
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('s'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Register);
        case 's':
            shift();
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('c'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (!consume('d'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Restricted);
        default:
            goto parseIdentifier;
        }

    case 'n':
        switch (peek()) {
        case 'a':
            shift();
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('v'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Native);
        case 'u':
            shift();
            switch (peek()) {
            case 'l':
                shift();
                if (!consume('l'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Null);
            case 'm':
                shift();
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('h'))
                    goto parseIdentifier;
                if (!consume('r'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('d'))
                    goto parseIdentifier;
                if (!consume('s'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::NumThreads);
            default:
                goto parseIdentifier;
            }

        case 'o':
            shift();
            switch (peek()) {
            case 'i':
                shift();
                if (!consume('n'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (!consume('r'))
                    goto parseIdentifier;
                if (!consume('p'))
                    goto parseIdentifier;
                if (!consume('o'))
                    goto parseIdentifier;
                if (!consume('l'))
                    goto parseIdentifier;
                if (!consume('a'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('i'))
                    goto parseIdentifier;
                if (!consume('o'))
                    goto parseIdentifier;
                if (!consume('n'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Qualifier);
            case 'p':
                shift();
                if (!consume('e'))
                    goto parseIdentifier;
                if (!consume('r'))
                    goto parseIdentifier;
                if (!consume('s'))
                    goto parseIdentifier;
                if (!consume('p'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (!consume('c'))
                    goto parseIdentifier;
                if (!consume('t'))
                    goto parseIdentifier;
                if (!consume('i'))
                    goto parseIdentifier;
                if (!consume('v'))
                    goto parseIdentifier;
                if (!consume('e'))
                    goto parseIdentifier;
                if (nextIsIdentifier())
                    goto parseIdentifier;
                return token(Token::Type::Qualifier);
            default:
                goto parseIdentifier;
            }

        default:
            goto parseIdentifier;
        }

    case 'v':
        if (!consume('e'))
            goto parseIdentifier;
        if (!consume('r'))
            goto parseIdentifier;
        if (!consume('t'))
            goto parseIdentifier;
        if (!consume('e'))
            goto parseIdentifier;
        if (!consume('x'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::Vertex);

    case 'P':
        if (!consume('S'))
            goto parseIdentifier;
        if (!consume('I'))
            goto parseIdentifier;
        if (!consume('Z'))
            goto parseIdentifier;
        if (!consume('E'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::PSize);

    case 'u':
        if (!consume('n'))
            goto parseIdentifier;
        if (!consume('i'))
            goto parseIdentifier;
        if (!consume('f'))
            goto parseIdentifier;
        if (!consume('o'))
            goto parseIdentifier;
        if (!consume('r'))
            goto parseIdentifier;
        if (!consume('m'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::Qualifier);

    case 'p':
        if (!consume('r'))
            goto parseIdentifier;
        if (!consume('o'))
            goto parseIdentifier;
        if (!consume('t'))
            goto parseIdentifier;
        if (!consume('o'))
            goto parseIdentifier;
        if (!consume('c'))
            goto parseIdentifier;
        if (!consume('o'))
            goto parseIdentifier;
        if (!consume('l'))
            goto parseIdentifier;
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::Protocol);

    case '_':
        if (nextIsIdentifier())
            goto parseIdentifier;
        return token(Token::Type::Underscore);

    case 'a':
        switch (peek()) {
        case 't':
            shift();
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('r'))
                goto parseIdentifier;
            if (!consume('i'))
                goto parseIdentifier;
            if (!consume('b'))
                goto parseIdentifier;
            if (!consume('u'))
                goto parseIdentifier;
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('e'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Attribute);
        case 'u':
            shift();
            if (!consume('t'))
                goto parseIdentifier;
            if (!consume('o'))
                goto parseIdentifier;
            if (nextIsIdentifier())
                goto parseIdentifier;
            return token(Token::Type::Auto);
        default:
            goto parseIdentifier;
        }

    case '>':
        if (consume('>')) {
            if (consume('='))
                return token(Token::Type::RightShiftEquals);
            return token(Token::Type::RightShift);
        }
        if (consume('='))
            return token(Token::Type::GreaterThanOrEqualTo);
        return token(Token::Type::GreaterThanSign);
    case '<':
        if (consume('<')) {
            if (consume('='))
                return token(Token::Type::LeftShiftEquals);
            return token(Token::Type::LeftShift);
        }
        if (consume('='))
            return token(Token::Type::LessThanOrEqualTo);
        return token(Token::Type::LessThanSign);

    case '+':
        if (consume('='))
            return token(Token::Type::PlusEquals);
        if (consume('+'))
            return token(Token::Type::PlusPlus);
        return token(Token::Type::Plus);

    case '-':
        if (consume('='))
            return token(Token::Type::MinusEquals);
        if (consume('-'))
            return token(Token::Type::MinusMinus);
        if (consume('>'))
            return token(Token::Type::Arrow);
        if (isDigit(peek())) {
            shift();
            goto parseNumber;
        }
        if (consume('.'))
            goto parseFloatAfterDot;
        return token(Token::Type::Minus);

    case '*':
        if (consume('='))
            return token(Token::Type::TimesEquals);
        return token(Token::Type::Star);

    case '/':
        if (consume('='))
            return token(Token::Type::DivideEquals);
        return token(Token::Type::Divide);

    case '%':
        if (consume('='))
            return token(Token::Type::ModEquals);
        return token(Token::Type::Mod);

    case '^':
        if (consume('='))
            return token(Token::Type::XorEquals);
        return token(Token::Type::Xor);

    case '&':
        if (consume('='))
            return token(Token::Type::AndEquals);
        if (consume('&'))
            return token(Token::Type::AndAnd);
        return token(Token::Type::And);

    case '|':
        if (consume('='))
            return token(Token::Type::OrEquals);
        if (consume('|'))
            return token(Token::Type::OrOr);
        return token(Token::Type::Or);

    case '[':
        if (consume(']'))
            return token(Token::Type::SquareBracketPair);
        return token(Token::Type::LeftSquareBracket);

    case '=':
        if (consume('='))
            return token(Token::Type::EqualComparison);
        return token(Token::Type::EqualsSign);

    case '!':
        if (consume('='))
            return token(Token::Type::NotEqual);
        return token(Token::Type::ExclamationPoint);

    case ';':
        return token(Token::Type::Semicolon);

    case '{':
        return token(Token::Type::LeftCurlyBracket);

    case '}':
        return token(Token::Type::RightCurlyBracket);

    case ':':
        return token(Token::Type::Colon);

    case ',':
        return token(Token::Type::Comma);

    case '(':
        return token(Token::Type::LeftParenthesis);

    case ')':
        return token(Token::Type::RightParenthesis);

    case ']':
        return token(Token::Type::RightSquareBracket);

    case '.':
        if (isDigit(peek()))
            goto parseFloatAfterDot;
        return token(Token::Type::FullStop);

    case '?':
        return token(Token::Type::QuestionMark);

    case '~':
        return token(Token::Type::Tilde);

    case '@':
        return token(Token::Type::At);

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
parseNumber:
        if (current == '0' && consume('x')) {
            while (isHexadecimalCharacter(peek()))
                shift();
            if (consume('u'))
                return token(Token::Type::UintLiteral);
            return token(Token::Type::IntLiteral);
        }

        while (isDigit(peek()))
            shift();

        if (consume('.')) {
parseFloatAfterDot:
            while (isDigit(peek()))
                shift();
            consume('f');
            return token(Token::Type::FloatLiteral);
        }

        if (consume('f'))
            return token(Token::Type::FloatLiteral);

        if (consume('u'))
            return token(Token::Type::UintLiteral);

        return token(Token::Type::IntLiteral);
    }

    default: 
        break;
    }

    if (m_offset == m_stringView.length())
        return token(Token::Type::EndOfFile);
    return token(Token::Type::Invalid);
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

auto Lexer::lineAndColumnNumberFromOffset(const StringView& stringView, unsigned targetOffset) -> LineAndColumn
{
    // Counting from 1 to match most text editors.
    unsigned lineNumber = 1;
    unsigned columnNumber = 1;
    for (unsigned offset = 0; offset < std::min(stringView.length(), targetOffset); ++offset) {
        ++columnNumber;
        if (isNewline(stringView[offset])) {
            ++lineNumber;
            columnNumber = 1;
        }
    }
    return { lineNumber, columnNumber };
}

static Optional<StringView> sourceFromNameSpace(AST::NameSpace nameSpace, const String& source1, const String* source2)
{
    switch (nameSpace) {
    case AST::NameSpace::StandardLibrary:
        return WTF::nullopt;
    case AST::NameSpace::NameSpace1:
        return StringView(source1);
    case AST::NameSpace::NameSpace2:
        ASSERT(source2);
        return StringView(*source2);
    }
}

String Lexer::errorString(Error error, const String& source1, const String* source2)
{
    if (auto codeLocation = error.codeLocation()) {
        if (auto source = sourceFromNameSpace(codeLocation.nameSpace(), source1, source2)) {
            auto lineAndColumn = lineAndColumnNumberFromOffset(*source, error.codeLocation().startOffset());
            return makeString(lineAndColumn.line, ':', lineAndColumn.column, ": ", error.message());
        }
    }
    return error.message();
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
            } else
                break;
        } else
            break;
    }
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
