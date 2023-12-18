// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2021 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSParserToken.h"

#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include <limits.h>
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSParserToken);

template<typename CharacterType>
CSSUnitType cssPrimitiveValueUnitFromTrie(const CharacterType* data, unsigned length)
{
    ASSERT(data);
    switch (length) {
    case 1:
        switch (toASCIILower(data[0])) {
        case 'q':
            return CSSUnitType::CSS_Q;
        case 's':
            return CSSUnitType::CSS_S;
        case 'x':
            return CSSUnitType::CSS_X;
        }
        break;
    case 2:
        switch (toASCIILower(data[0])) {
        case 'c':
            switch (toASCIILower(data[1])) {
            case 'h':
                return CSSUnitType::CSS_CH;
            case 'm':
                return CSSUnitType::CSS_CM;
            }
            break;
        case 'e':
            switch (toASCIILower(data[1])) {
            case 'm':
                return CSSUnitType::CSS_EM;
            case 'x':
                return CSSUnitType::CSS_EX;
            }
            break;
        case 'f':
            if (toASCIILower(data[1]) == 'r')
                return CSSUnitType::CSS_FR;
            break;
        case 'h':
            if (toASCIILower(data[1]) == 'z')
                return CSSUnitType::CSS_HZ;
            break;
        case 'i':
            switch (toASCIILower(data[1])) {
            case 'c':
                return CSSUnitType::CSS_IC;
            case 'n':
                return CSSUnitType::CSS_IN;
            }
            break;
        case 'l':
            if (toASCIILower(data[1]) == 'h')
                return CSSUnitType::CSS_LH;
            break;
        case 'm':
            switch (toASCIILower(data[1])) {
            case 'm':
                return CSSUnitType::CSS_MM;
            case 's':
                return CSSUnitType::CSS_MS;
            }
            break;
        case 'p':
            switch (toASCIILower(data[1])) {
            case 'c':
                return CSSUnitType::CSS_PC;
            case 't':
                return CSSUnitType::CSS_PT;
            case 'x':
                return CSSUnitType::CSS_PX;
            }
            break;
        case 'v':
            switch (toASCIILower(data[1])) {
            case 'b':
                return CSSUnitType::CSS_VB;
            case 'h':
                return CSSUnitType::CSS_VH;
            case 'i':
                return CSSUnitType::CSS_VI;
            case 'w':
                return CSSUnitType::CSS_VW;
            }
            break;
        }
        break;
    case 3:
        switch (toASCIILower(data[0])) {
        case 'c':
            if (toASCIILower(data[1]) == 'a') {
                if (toASCIILower(data[2]) == 'p')
                    return CSSUnitType::CSS_CAP;
            }
            if (toASCIILower(data[1]) == 'q') {
                switch (toASCIILower(data[2])) {
                case 'b':
                    return CSSUnitType::CSS_CQB;
                case 'h':
                    return CSSUnitType::CSS_CQH;
                case 'i':
                    return CSSUnitType::CSS_CQI;
                case 'w':
                    return CSSUnitType::CSS_CQW;
                }
            }
            break;
        case 'd':
            switch (toASCIILower(data[1])) {
            case 'e':
                if (toASCIILower(data[2]) == 'g')
                    return CSSUnitType::CSS_DEG;
                break;
            case 'p':
                if (toASCIILower(data[2]) == 'i')
                    return CSSUnitType::CSS_DPI;
                break;
            case 'v':
                switch (toASCIILower(data[2])) {
                case 'b':
                    return CSSUnitType::CSS_DVB;
                case 'h':
                    return CSSUnitType::CSS_DVH;
                case 'i':
                    return CSSUnitType::CSS_DVI;
                case 'w':
                    return CSSUnitType::CSS_DVW;
                }
                break;
            }
            break;
        case 'l':
            if (toASCIILower(data[1]) == 'v') {
                switch (toASCIILower(data[2])) {
                case 'b':
                    return CSSUnitType::CSS_LVB;
                case 'h':
                    return CSSUnitType::CSS_LVH;
                case 'i':
                    return CSSUnitType::CSS_LVI;
                case 'w':
                    return CSSUnitType::CSS_LVW;
                }
            }
            break;
        case 'k':
            if (toASCIILower(data[1]) == 'h' && toASCIILower(data[2]) == 'z')
                return CSSUnitType::CSS_KHZ;
            break;
        case 'r':
            switch (toASCIILower(data[1])) {
            case 'a':
                if (toASCIILower(data[2]) == 'd')
                    return CSSUnitType::CSS_RAD;
                break;
            case 'c':
                if (toASCIILower(data[2]) == 'h')
                    return CSSUnitType::CSS_RCH;
                break;
            case 'e':
                if (toASCIILower(data[2]) == 'm')
                    return CSSUnitType::CSS_REM;
                if (toASCIILower(data[2]) == 'x')
                    return CSSUnitType::CSS_REX;
                break;
            case 'i':
                if (toASCIILower(data[2]) == 'c')
                    return CSSUnitType::CSS_RIC;
                break;
            case 'l':
                if (toASCIILower(data[2]) == 'h')
                    return CSSUnitType::CSS_RLH;
                break;
            }
            break;
        case 's':
            if (toASCIILower(data[1]) == 'v') {
                switch (toASCIILower(data[2])) {
                case 'b':
                    return CSSUnitType::CSS_SVB;
                case 'h':
                    return CSSUnitType::CSS_SVH;
                case 'i':
                    return CSSUnitType::CSS_SVI;
                case 'w':
                    return CSSUnitType::CSS_SVW;
                }
            }
            break;
        }
        break;
    case 4:
        switch (toASCIILower(data[0])) {
        case 'd':
            switch (toASCIILower(data[1])) {
            case 'p':
                switch (toASCIILower(data[2])) {
                case 'c':
                    if (toASCIILower(data[3]) == 'm')
                        return CSSUnitType::CSS_DPCM;
                    break;
                case 'p':
                    if (toASCIILower(data[3]) == 'x')
                        return CSSUnitType::CSS_DPPX;
                    break;
                }
                break;
            }
            break;
        case 'g':
            if (toASCIILower(data[1]) == 'r' && toASCIILower(data[2]) == 'a' && toASCIILower(data[3]) == 'd')
                return CSSUnitType::CSS_GRAD;
            break;
        case 'r':
            if (toASCIILower(data[1]) == 'c' && toASCIILower(data[2]) == 'a' && toASCIILower(data[3]) == 'p')
                return CSSUnitType::CSS_RCAP;
            break;
        case 't':
            if (toASCIILower(data[1]) == 'u' && toASCIILower(data[2]) == 'r' && toASCIILower(data[3]) == 'n')
                return CSSUnitType::CSS_TURN;
            break;
        case 'v':
            switch (toASCIILower(data[1])) {
            case 'm':
                switch (toASCIILower(data[2])) {
                case 'a':
                    if (toASCIILower(data[3]) == 'x')
                        return CSSUnitType::CSS_VMAX;
                    break;
                case 'i':
                    if (toASCIILower(data[3]) == 'n')
                        return CSSUnitType::CSS_VMIN;
                    break;
                }
                break;
            }
            break;
        }
        break;
    case 5:
        switch (toASCIILower(data[0])) {
        case '_':
            if (toASCIILower(data[1]) == '_' && toASCIILower(data[2]) == 'q' && toASCIILower(data[3]) == 'e' && toASCIILower(data[4]) == 'm')
                return CSSUnitType::CSS_QUIRKY_EM;
            break;
        case 'c':
            if (toASCIILower(data[1]) == 'q' && toASCIILower(data[2]) == 'm') {
                switch (toASCIILower(data[3])) {
                case 'a':
                    if (toASCIILower(data[4]) == 'x')
                        return CSSUnitType::CSS_CQMAX;
                    break;
                case 'i':
                    if (toASCIILower(data[4]) == 'n')
                        return CSSUnitType::CSS_CQMIN;
                    break;
                }
            }
            break;
        case 'd':
            if (toASCIILower(data[1]) == 'v' && toASCIILower(data[2]) == 'm') {
                switch (toASCIILower(data[3])) {
                case 'a':
                    if (toASCIILower(data[4]) == 'x')
                        return CSSUnitType::CSS_DVMAX;
                    break;
                case 'i':
                    if (toASCIILower(data[4]) == 'n')
                        return CSSUnitType::CSS_DVMIN;
                    break;
                }
            }
            break;
        case 'l':
            if (toASCIILower(data[1]) == 'v' && toASCIILower(data[2]) == 'm') {
                switch (toASCIILower(data[3])) {
                case 'a':
                    if (toASCIILower(data[4]) == 'x')
                        return CSSUnitType::CSS_LVMAX;
                    break;
                case 'i':
                    if (toASCIILower(data[4]) == 'n')
                        return CSSUnitType::CSS_LVMIN;
                    break;
                }
            }
            break;
        case 's':
            if (toASCIILower(data[1]) == 'v' && toASCIILower(data[2]) == 'm') {
                switch (toASCIILower(data[3])) {
                case 'a':
                    if (toASCIILower(data[4]) == 'x')
                        return CSSUnitType::CSS_SVMAX;
                    break;
                case 'i':
                    if (toASCIILower(data[4]) == 'n')
                        return CSSUnitType::CSS_SVMIN;
                    break;
                }
            }
            break;
        }
        break;
    }
    return CSSUnitType::CSS_UNKNOWN;
}

CSSUnitType CSSParserToken::stringToUnitType(StringView stringView)
{
    if (stringView.is8Bit())
        return cssPrimitiveValueUnitFromTrie(stringView.characters8(), stringView.length());
    return cssPrimitiveValueUnitFromTrie(stringView.characters16(), stringView.length());
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, BlockType blockType)
    : m_type(type)
    , m_blockType(blockType)
{
}

// Just a helper used for Delimiter tokens.
CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar c)
    : m_type(type)
    , m_blockType(NotBlock)
    , m_delimiter(c)
{
    ASSERT(m_type == DelimiterToken);
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, StringView value, BlockType blockType)
    : m_type(type)
    , m_blockType(blockType)
{
    initValueFromStringView(value);
    m_id = -1;
}

CSSParserToken::CSSParserToken(double numericValue, NumericValueType numericValueType, NumericSign sign, StringView originalText)
    : m_type(NumberToken)
    , m_blockType(NotBlock)
    , m_numericValueType(numericValueType)
    , m_numericSign(sign)
    , m_unit(static_cast<unsigned>(CSSUnitType::CSS_NUMBER))
    , m_numericValue(numericValue)
{
    initValueFromStringView(originalText);
}

CSSParserToken::CSSParserToken(HashTokenType type, StringView value)
    : m_type(HashToken)
    , m_blockType(NotBlock)
    , m_hashTokenType(type)
{
    initValueFromStringView(value);
}

static StringView mergeIfAdjacent(StringView a, StringView b)
{
    if (a.is8Bit() && b.is8Bit()) {
        auto characters = a.characters8();
        if (characters + a.length() == b.characters8())
            return { characters, a.length() + b.length() };
    } else if (!a.is8Bit() && !b.is8Bit()) {
        auto characters = a.characters16();
        if (characters + a.length() == b.characters16())
            return { characters, a.length() + b.length() };
    }
    return { };
}

void CSSParserToken::convertToDimensionWithUnit(StringView unit)
{
    ASSERT(m_type == NumberToken);
    auto originalNumberText = originalText();
    auto originalNumberTextLength = originalNumberText.length();
    auto string = unit;
    if (originalNumberTextLength && originalNumberTextLength < 16) {
        if (auto merged = mergeIfAdjacent(originalNumberText, unit))
            string = merged;
    }
    m_type = DimensionToken;
    m_unit = static_cast<unsigned>(stringToUnitType(unit));
    m_nonUnitPrefixLength = string == unit ? 0 : originalNumberTextLength;
    initValueFromStringView(string);
}

void CSSParserToken::convertToPercentage()
{
    ASSERT(m_type == NumberToken);
    m_type = PercentageToken;
    m_unit = static_cast<unsigned>(CSSUnitType::CSS_PERCENTAGE);
}

StringView CSSParserToken::originalText() const
{
    ASSERT(m_type == NumberToken || m_type == DimensionToken);
    return value();
}

StringView CSSParserToken::unitString() const
{
    ASSERT(m_type == DimensionToken);
    return value().substring(m_nonUnitPrefixLength);
}

UChar CSSParserToken::delimiter() const
{
    ASSERT(m_type == DelimiterToken);
    return m_delimiter;
}

NumericSign CSSParserToken::numericSign() const
{
    // This is valid for DimensionToken and PercentageToken, but only used
    // in <an+b> parsing on NumberTokens.
    ASSERT(m_type == NumberToken);
    return static_cast<NumericSign>(m_numericSign);
}

NumericValueType CSSParserToken::numericValueType() const
{
    ASSERT(m_type == NumberToken || m_type == PercentageToken || m_type == DimensionToken);
    return static_cast<NumericValueType>(m_numericValueType);
}

double CSSParserToken::numericValue() const
{
    ASSERT(m_type == NumberToken || m_type == PercentageToken || m_type == DimensionToken);
    return m_numericValue;
}

CSSPropertyID CSSParserToken::parseAsCSSPropertyID() const
{
    ASSERT(m_type == IdentToken);
    return cssPropertyID(value());
}

CSSValueID CSSParserToken::id() const
{
    if (m_type != IdentToken)
        return CSSValueInvalid;
    if (m_id < 0)
        m_id = cssValueKeywordID(value());
    return static_cast<CSSValueID>(m_id);
}

CSSValueID CSSParserToken::functionId() const
{
    if (m_type != FunctionToken)
        return CSSValueInvalid;
    if (m_id < 0)
        m_id = cssValueKeywordID(value());
    return static_cast<CSSValueID>(m_id);
}

bool CSSParserToken::hasStringBacking() const
{
    switch (type()) {
    case AtKeywordToken:
    case DimensionToken:
    case FunctionToken:
    case HashToken:
    case IdentToken:
    case NumberToken:
    case PercentageToken:
    case StringToken:
    case UrlToken:
        return true;
    case BadStringToken:
    case BadUrlToken:
    case CDCToken:
    case CDOToken:
    case ColonToken:
    case ColumnToken:
    case CommaToken:
    case CommentToken:
    case DashMatchToken:
    case DelimiterToken:
    case EOFToken:
    case IncludeMatchToken:
    case LeftBraceToken:
    case LeftBracketToken:
    case LeftParenthesisToken:
    case PrefixMatchToken:
    case RightBraceToken:
    case RightBracketToken:
    case RightParenthesisToken:
    case SemicolonToken:
    case SubstringMatchToken:
    case SuffixMatchToken:
    case WhitespaceToken:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

CSSParserToken CSSParserToken::copyWithUpdatedString(StringView string) const
{
    ASSERT(value() == string);
    CSSParserToken copy(*this);
    copy.initValueFromStringView(string);
    return copy;
}

bool CSSParserToken::operator==(const CSSParserToken& other) const
{
    if (m_type != other.m_type)
        return false;
    switch (m_type) {
    case DelimiterToken:
        return delimiter() == other.delimiter();
    case HashToken:
        if (m_hashTokenType != other.m_hashTokenType)
            return false;
        FALLTHROUGH;
    case IdentToken:
    case FunctionToken:
    case StringToken:
    case UrlToken:
        return value() == other.value();
    case DimensionToken:
        if (unitString() != other.unitString())
            return false;
        FALLTHROUGH;
    case NumberToken:
    case PercentageToken:
        return m_numericSign == other.m_numericSign && m_numericValue == other.m_numericValue && m_numericValueType == other.m_numericValueType;
    default:
        return true;
    }
}

struct NextTokenNeedsCommentBuilder {
    constexpr NextTokenNeedsCommentBuilder(std::initializer_list<CSSParserTokenType> tokens)
    {
        for (auto token : tokens)
            buffer[token] = true;
    }

    std::array<bool, numberOfCSSParserTokenTypes> buffer { false };
};

void CSSParserToken::serialize(StringBuilder& builder, const CSSParserToken* nextToken) const
{
    // This is currently only used for @supports CSSOM. To keep our implementation
    // simple we handle some of the edge cases incorrectly (see comments below).
    auto appendCommentIfNeeded = [&] (const NextTokenNeedsCommentBuilder& tokensNeedingComment, auto... delimitersNeedingComment) {
        if (!nextToken)
            return;

        CSSParserTokenType nextType = nextToken->type();
        if (tokensNeedingComment.buffer[nextType]) {
            builder.append("/**/");
            return;
        }

        if (nextType == DelimiterToken && ((delimitersNeedingComment == nextToken->delimiter()) || ... || false)) {
            builder.append("/**/");
            return;
        }
    };

    switch (type()) {
    case IdentToken:
        serializeIdentifier(value().toString(), builder);
        appendCommentIfNeeded({ IdentToken, FunctionToken, UrlToken, BadUrlToken, NumberToken, PercentageToken, DimensionToken, CDCToken, LeftParenthesisToken }, '-');
        break;
    case FunctionToken:
        serializeIdentifier(value().toString(), builder);
        builder.append('(');
        break;
    case AtKeywordToken:
        builder.append('@');
        serializeIdentifier(value().toString(), builder);
        appendCommentIfNeeded({ IdentToken, FunctionToken, UrlToken, BadUrlToken, NumberToken, PercentageToken, DimensionToken, CDCToken }, '-');
        break;
    case HashToken:
        builder.append('#');
        serializeIdentifier(value().toString(), builder, (getHashTokenType() == HashTokenUnrestricted));
        appendCommentIfNeeded({ IdentToken, FunctionToken, UrlToken, BadUrlToken, NumberToken, PercentageToken, DimensionToken, CDCToken }, '-');
        break;
    case UrlToken:
        builder.append("url(");
        serializeIdentifier(value().toString(), builder);
        builder.append(')');
        break;
    case DelimiterToken:
        switch (delimiter()) {
        case '\\':
            builder.append("\\\n");
            break;

        case '#':
        case '-':
            builder.append(delimiter());
            appendCommentIfNeeded({ IdentToken, FunctionToken, UrlToken, BadUrlToken, NumberToken, PercentageToken, DimensionToken }, '-');
            break;

        case '@':
            builder.append('@');
            appendCommentIfNeeded({ IdentToken, FunctionToken, UrlToken, BadUrlToken }, '-');
            break;

        case '.':
        case '+':
            builder.append(delimiter());
            appendCommentIfNeeded({ NumberToken, PercentageToken, DimensionToken });
            break;

        case '/':
            builder.append('/');
            // Weirdly Clang errors if you try to use the fold expression in buildNextTokenNeedsCommentTable() because the true value is unused.
            // So we just build the table by hand here instead. See: rdar://69710661
            appendCommentIfNeeded({ }, '*');
            break;

        default:
            builder.append(delimiter());
            break;
        }
        break;
    case NumberToken:
        // These won't properly preserve the NumericValueType flag.
        if (m_numericSign == PlusSign)
            builder.append('+');
        builder.append(numericValue());
        appendCommentIfNeeded({ IdentToken, FunctionToken, UrlToken, BadUrlToken, NumberToken, PercentageToken, DimensionToken }, '%');
        break;
    case PercentageToken:
        builder.append(numericValue(), '%');
        break;
    case DimensionToken:
        // This will incorrectly serialize e.g. 4e3e2 as 4000e2.
        builder.append(numericValue());
        serializeIdentifier(unitString().toString(), builder);
        appendCommentIfNeeded({ IdentToken, FunctionToken, UrlToken, BadUrlToken, NumberToken, PercentageToken, DimensionToken, CDCToken }, '-');
        break;
    case StringToken:
        serializeString(value().toString(), builder);
        break;

    case IncludeMatchToken:
        builder.append("~=");
        break;
    case DashMatchToken:
        builder.append("|=");
        break;
    case PrefixMatchToken:
        builder.append("^=");
        break;
    case SuffixMatchToken:
        builder.append("$=");
        break;
    case SubstringMatchToken:
        builder.append("*=");
        break;
    case ColumnToken:
        builder.append("||");
        break;
    case CDOToken:
        builder.append("<!--");
        break;
    case CDCToken:
        builder.append("-->");
        break;
    case BadStringToken:
        builder.append("'\n");
        break;
    case BadUrlToken:
        builder.append("url(()");
        break;
    case WhitespaceToken:
        builder.append(' ');
        break;
    case ColonToken:
        builder.append(':');
        break;
    case SemicolonToken:
        builder.append(';');
        break;
    case CommaToken:
        builder.append(',');
        break;
    case LeftParenthesisToken:
        builder.append('(');
        break;
    case RightParenthesisToken:
        builder.append(')');
        break;
    case LeftBracketToken:
        builder.append('[');
        break;
    case RightBracketToken:
        builder.append(']');
        break;
    case LeftBraceToken:
        builder.append('{');
        break;
    case RightBraceToken:
        builder.append('}');
        break;

    case EOFToken:
    case CommentToken:
        ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace WebCore
