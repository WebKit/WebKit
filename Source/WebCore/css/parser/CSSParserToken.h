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

#pragma once

#include "CSSPrimitiveValue.h"
#include <wtf/text/StringView.h>

namespace WebCore {

enum CSSParserTokenType {
    IdentToken = 0,
    FunctionToken,
    AtKeywordToken,
    HashToken,
    UrlToken,
    BadUrlToken,
    DelimiterToken,
    NumberToken,
    PercentageToken,
    DimensionToken,
    IncludeMatchToken,
    DashMatchToken,
    PrefixMatchToken,
    SuffixMatchToken,
    SubstringMatchToken,
    ColumnToken,
    UnicodeRangeToken,
    WhitespaceToken,
    CDOToken,
    CDCToken,
    ColonToken,
    SemicolonToken,
    CommaToken,
    LeftParenthesisToken,
    RightParenthesisToken,
    LeftBracketToken,
    RightBracketToken,
    LeftBraceToken,
    RightBraceToken,
    StringToken,
    BadStringToken,
    EOFToken,
    CommentToken,
};

enum NumericSign {
    NoSign,
    PlusSign,
    MinusSign,
};

enum NumericValueType {
    IntegerValueType,
    NumberValueType,
};

enum HashTokenType {
    HashTokenId,
    HashTokenUnrestricted,
};

class CSSParserToken {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum BlockType {
        NotBlock,
        BlockStart,
        BlockEnd,
    };

    CSSParserToken(CSSParserTokenType, BlockType = NotBlock);
    CSSParserToken(CSSParserTokenType, StringView, BlockType = NotBlock);

    CSSParserToken(CSSParserTokenType, UChar); // for DelimiterToken
    CSSParserToken(CSSParserTokenType, double, NumericValueType, NumericSign); // for NumberToken
    CSSParserToken(CSSParserTokenType, UChar32, UChar32); // for UnicodeRangeToken

    CSSParserToken(HashTokenType, StringView);

    bool operator==(const CSSParserToken& other) const;
    bool operator!=(const CSSParserToken& other) const { return !(*this == other); }

    // Converts NumberToken to DimensionToken.
    void convertToDimensionWithUnit(StringView);

    // Converts NumberToken to PercentageToken.
    void convertToPercentage();

    CSSParserTokenType type() const { return static_cast<CSSParserTokenType>(m_type); }
    StringView value() const
    {
        if (m_valueIs8Bit)
            return StringView(static_cast<const LChar*>(m_valueDataCharRaw), m_valueLength);
        return StringView(static_cast<const UChar*>(m_valueDataCharRaw), m_valueLength);
    }

    UChar delimiter() const;
    NumericSign numericSign() const;
    NumericValueType numericValueType() const;
    double numericValue() const;
    HashTokenType getHashTokenType() const { ASSERT(m_type == HashToken); return m_hashTokenType; }
    BlockType getBlockType() const { return static_cast<BlockType>(m_blockType); }
    CSSUnitType unitType() const { return static_cast<CSSUnitType>(m_unit); }
    UChar32 unicodeRangeStart() const { ASSERT(m_type == UnicodeRangeToken); return m_unicodeRange.start; }
    UChar32 unicodeRangeEnd() const { ASSERT(m_type == UnicodeRangeToken); return m_unicodeRange.end; }
    CSSValueID id() const;
    CSSValueID functionId() const;

    bool hasStringBacking() const;

    CSSPropertyID parseAsCSSPropertyID() const;

    void serialize(StringBuilder&) const;

    CSSParserToken copyWithUpdatedString(const StringView&) const;

private:
    void initValueFromStringView(StringView string)
    {
        m_valueLength = string.length();
        m_valueIs8Bit = string.is8Bit();
        m_valueDataCharRaw = m_valueIs8Bit ? const_cast<void*>(static_cast<const void*>(string.characters8())) : const_cast<void*>(static_cast<const void*>(string.characters16()));
    }
    unsigned m_type : 6; // CSSParserTokenType
    unsigned m_blockType : 2; // BlockType
    unsigned m_numericValueType : 1; // NumericValueType
    unsigned m_numericSign : 2; // NumericSign
    unsigned m_unit : 7; // CSSUnitType

    bool valueDataCharRawEqual(const CSSParserToken& other) const;

    // m_value... is an unpacked StringView so that we can pack it
    // tightly with the rest of this object for a smaller object size.
    bool m_valueIs8Bit : 1;
    unsigned m_valueLength;
    void* m_valueDataCharRaw; // Either LChar* or UChar*.

    union {
        UChar m_delimiter;
        HashTokenType m_hashTokenType;
        double m_numericValue;
        mutable int m_id;

        struct {
            UChar32 start;
            UChar32 end;
        } m_unicodeRange;
    };
};

} // namespace WebCore
