// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
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

template<typename CharacterType>
CSSPrimitiveValue::UnitType cssPrimitiveValueUnitFromTrie(const CharacterType* data, unsigned length)
{
    ASSERT(data);
    ASSERT(length);
    switch (length) {
    case 1:
        switch (toASCIILower(data[0])) {
        case 's':
            return CSSPrimitiveValue::UnitType::CSS_S;
        }
        break;
    case 2:
        switch (toASCIILower(data[0])) {
        case 'c':
            switch (toASCIILower(data[1])) {
            case 'h':
                return CSSPrimitiveValue::UnitType::CSS_CHS;
            case 'm':
                return CSSPrimitiveValue::UnitType::CSS_CM;
            }
            break;
        case 'e':
            switch (toASCIILower(data[1])) {
            case 'm':
                return CSSPrimitiveValue::UnitType::CSS_EMS;
            case 'x':
                return CSSPrimitiveValue::UnitType::CSS_EXS;
            }
            break;
        case 'f':
            if (toASCIILower(data[1]) == 'r')
                return CSSPrimitiveValue::UnitType::CSS_FR;
            break;
        case 'h':
            if (toASCIILower(data[1]) == 'z')
                return CSSPrimitiveValue::UnitType::CSS_HZ;
            break;
        case 'i':
            if (toASCIILower(data[1]) == 'n')
                return CSSPrimitiveValue::UnitType::CSS_IN;
            break;
        case 'm':
            switch (toASCIILower(data[1])) {
            case 'm':
                return CSSPrimitiveValue::UnitType::CSS_MM;
            case 's':
                return CSSPrimitiveValue::UnitType::CSS_MS;
            }
            break;
        case 'p':
            switch (toASCIILower(data[1])) {
            case 'c':
                return CSSPrimitiveValue::UnitType::CSS_PC;
            case 't':
                return CSSPrimitiveValue::UnitType::CSS_PT;
            case 'x':
                return CSSPrimitiveValue::UnitType::CSS_PX;
            }
            break;
        case 'v':
            switch (toASCIILower(data[1])) {
            case 'h':
                return CSSPrimitiveValue::UnitType::CSS_VH;
            case 'w':
                return CSSPrimitiveValue::UnitType::CSS_VW;
            }
            break;
        }
        break;
    case 3:
        switch (toASCIILower(data[0])) {
        case 'd':
            switch (toASCIILower(data[1])) {
            case 'e':
                if (toASCIILower(data[2]) == 'g')
                    return CSSPrimitiveValue::UnitType::CSS_DEG;
                break;
            case 'p':
                if (toASCIILower(data[2]) == 'i')
                    return CSSPrimitiveValue::UnitType::CSS_DPI;
                break;
            }
        break;
        case 'k':
            if (toASCIILower(data[1]) == 'h' && toASCIILower(data[2]) == 'z')
                return CSSPrimitiveValue::UnitType::CSS_KHZ;
            break;
        case 'r':
            switch (toASCIILower(data[1])) {
            case 'a':
                if (toASCIILower(data[2]) == 'd')
                    return CSSPrimitiveValue::UnitType::CSS_RAD;
                break;
            case 'e':
                if (toASCIILower(data[2]) == 'm')
                    return CSSPrimitiveValue::UnitType::CSS_REMS;
                break;
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
                        return CSSPrimitiveValue::UnitType::CSS_DPCM;
                    break;
                case 'p':
                    if (toASCIILower(data[3]) == 'x')
                        return CSSPrimitiveValue::UnitType::CSS_DPPX;
                    break;
                }
            break;
        }
        break;
        case 'g':
            if (toASCIILower(data[1]) == 'r' && toASCIILower(data[2]) == 'a' && toASCIILower(data[3]) == 'd')
                return CSSPrimitiveValue::UnitType::CSS_GRAD;
            break;
        case 't':
            if (toASCIILower(data[1]) == 'u' && toASCIILower(data[2]) == 'r' && toASCIILower(data[3]) == 'n')
                return CSSPrimitiveValue::UnitType::CSS_TURN;
            break;
        case 'v':
            switch (toASCIILower(data[1])) {
            case 'm':
                switch (toASCIILower(data[2])) {
                case 'a':
                    if (toASCIILower(data[3]) == 'x')
                        return CSSPrimitiveValue::UnitType::CSS_VMAX;
                    break;
                case 'i':
                    if (toASCIILower(data[3]) == 'n')
                        return CSSPrimitiveValue::UnitType::CSS_VMIN;
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
                return CSSPrimitiveValue::UnitType::CSS_QUIRKY_EMS;
            break;
        }
        break;
    }
    return CSSPrimitiveValue::UnitType::CSS_UNKNOWN;
}

static CSSPrimitiveValue::UnitType stringToUnitType(StringView stringView)
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

CSSParserToken::CSSParserToken(CSSParserTokenType type, double numericValue, NumericValueType numericValueType, NumericSign sign)
    : m_type(type)
    , m_blockType(NotBlock)
    , m_numericValueType(numericValueType)
    , m_numericSign(sign)
    , m_unit(static_cast<unsigned>(CSSPrimitiveValue::UnitType::CSS_NUMBER))
{
    ASSERT(type == NumberToken);
    m_numericValue = numericValue;
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar32 start, UChar32 end)
    : m_type(UnicodeRangeToken)
    , m_blockType(NotBlock)
{
    ASSERT_UNUSED(type, type == UnicodeRangeToken);
    m_unicodeRange.start = start;
    m_unicodeRange.end = end;
}

CSSParserToken::CSSParserToken(HashTokenType type, StringView value)
    : m_type(HashToken)
    , m_blockType(NotBlock)
    , m_hashTokenType(type)
{
    initValueFromStringView(value);
}

void CSSParserToken::convertToDimensionWithUnit(StringView unit)
{
    ASSERT(m_type == NumberToken);
    m_type = DimensionToken;
    initValueFromStringView(unit);
    m_unit = static_cast<unsigned>(stringToUnitType(unit));
}

void CSSParserToken::convertToPercentage()
{
    ASSERT(m_type == NumberToken);
    m_type = PercentageToken;
    m_unit = static_cast<unsigned>(CSSPrimitiveValue::UnitType::CSS_PERCENTAGE);
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
    CSSParserTokenType tokenType = type();
    return tokenType == IdentToken
        || tokenType == FunctionToken
        || tokenType == AtKeywordToken
        || tokenType == HashToken
        || tokenType == UrlToken
        || tokenType == DimensionToken
        || tokenType == StringToken;
}

CSSParserToken CSSParserToken::copyWithUpdatedString(const StringView& string) const
{
    CSSParserToken copy(*this);
    copy.initValueFromStringView(string);
    return copy;
}

bool CSSParserToken::valueDataCharRawEqual(const CSSParserToken& other) const
{
    if (m_valueLength != other.m_valueLength)
        return false;

    if (m_valueDataCharRaw == other.m_valueDataCharRaw && m_valueIs8Bit == other.m_valueIs8Bit)
        return true;

    if (m_valueIs8Bit)
        return other.m_valueIs8Bit ? equal(static_cast<const LChar*>(m_valueDataCharRaw), static_cast<const LChar*>(other.m_valueDataCharRaw), m_valueLength) : equal(static_cast<const LChar*>(m_valueDataCharRaw), static_cast<const UChar*>(other.m_valueDataCharRaw), m_valueLength);
    
    return other.m_valueIs8Bit ? equal(static_cast<const UChar*>(m_valueDataCharRaw), static_cast<const LChar*>(other.m_valueDataCharRaw), m_valueLength) : equal(static_cast<const UChar*>(m_valueDataCharRaw), static_cast<const UChar*>(other.m_valueDataCharRaw), m_valueLength);
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
        return valueDataCharRawEqual(other);
    case DimensionToken:
        if (!valueDataCharRawEqual(other))
            return false;
        FALLTHROUGH;
    case NumberToken:
    case PercentageToken:
        return m_numericSign == other.m_numericSign && m_numericValue == other.m_numericValue && m_numericValueType == other.m_numericValueType;
    case UnicodeRangeToken:
        return m_unicodeRange.start == other.m_unicodeRange.start && m_unicodeRange.end == other.m_unicodeRange.end;
    default:
        return true;
    }
}

void CSSParserToken::serialize(StringBuilder& builder) const
{
    // This is currently only used for @supports CSSOM. To keep our implementation
    // simple we handle some of the edge cases incorrectly (see comments below).
    switch (type()) {
    case IdentToken:
        serializeIdentifier(value().toString(), builder);
        break;
    case FunctionToken:
        serializeIdentifier(value().toString(), builder);
        builder.append('(');
        break;
    case AtKeywordToken:
        builder.append('@');
        serializeIdentifier(value().toString(), builder);
        break;
    case HashToken:
        builder.append('#');
        serializeIdentifier(value().toString(), builder, (getHashTokenType() == HashTokenUnrestricted));
        break;
    case UrlToken:
        builder.appendLiteral("url(");
        serializeIdentifier(value().toString(), builder);
        builder.append(')');
        break;
    case DelimiterToken:
        if (delimiter() == '\\') {
            builder.appendLiteral("\\\n");
            break;
        }
        builder.append(delimiter());
        break;
    case NumberToken:
        // These won't properly preserve the NumericValueType flag
        if (m_numericSign == PlusSign)
            builder.append('+');
        builder.appendNumber(numericValue());
        break;
    case PercentageToken:
        builder.appendNumber(numericValue());
        builder.append('%');
        break;
    case DimensionToken:
        // This will incorrectly serialize e.g. 4e3e2 as 4000e2
        builder.appendNumber(numericValue());
        serializeIdentifier(value().toString(), builder);
        break;
    case UnicodeRangeToken:
        builder.appendLiteral("U+");
        appendUnsignedAsHex(unicodeRangeStart(), builder);
        builder.append('-');
        appendUnsignedAsHex(unicodeRangeEnd(), builder);
        break;
    case StringToken:
        serializeString(value().toString(), builder);
        break;

    case IncludeMatchToken:
        builder.appendLiteral("~=");
        break;
    case DashMatchToken:
        builder.appendLiteral("|=");
        break;
    case PrefixMatchToken:
        builder.appendLiteral("^=");
        break;
    case SuffixMatchToken:
        builder.appendLiteral("$=");
        break;
    case SubstringMatchToken:
        builder.appendLiteral("*=");
        break;
    case ColumnToken:
        builder.appendLiteral("||");
        break;
    case CDOToken:
        builder.appendLiteral("<!--");
        break;
    case CDCToken:
        builder.appendLiteral("-->");
        break;
    case BadStringToken:
        builder.appendLiteral("'\n");
        break;
    case BadUrlToken:
        builder.appendLiteral("url(()");
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
