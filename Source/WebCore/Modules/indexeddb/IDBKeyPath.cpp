/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBKeyPath.h"

#if ENABLE(INDEXED_DATABASE)

#include "KeyedCoding.h"
#include <wtf/ASCIICType.h>
#include <wtf/dtoa.h>

namespace WebCore {

class IDBKeyPathLexer {
public:
    enum TokenType {
        TokenIdentifier,
        TokenDot,
        TokenEnd,
        TokenError
    };

    explicit IDBKeyPathLexer(const String& s)
        : m_string(s)
        , m_remainingText(s)
        , m_currentTokenType(TokenError)
    {
    }

    TokenType currentTokenType() const { return m_currentTokenType; }

    TokenType nextTokenType()
    {
        m_currentTokenType = lex(m_currentElement);
        return m_currentTokenType;
    }

    const String& currentElement() { return m_currentElement; }

private:
    TokenType lex(String&);
    TokenType lexIdentifier(String&);

    String m_currentElement;
    const String m_string;
    StringView m_remainingText;
    TokenType m_currentTokenType;
};

IDBKeyPathLexer::TokenType IDBKeyPathLexer::lex(String& element)
{
    if (m_remainingText.isEmpty())
        return TokenEnd;

    if (m_remainingText[0] == '.') {
        m_remainingText = m_remainingText.substring(1);
        return TokenDot;
    }

    return lexIdentifier(element);
}

namespace {

// The following correspond to grammar in ECMA-262.
const uint32_t unicodeLetter = U_GC_L_MASK | U_GC_NL_MASK;
const uint32_t unicodeCombiningMark = U_GC_MN_MASK | U_GC_MC_MASK;
const uint32_t unicodeDigit = U_GC_ND_MASK;
const uint32_t unicodeConnectorPunctuation = U_GC_PC_MASK;
const UChar ZWNJ = 0x200C;
const UChar ZWJ = 0x200D;

static inline bool isIdentifierStartCharacter(UChar c)
{
    return (U_GET_GC_MASK(c) & unicodeLetter) || (c == '$') || (c == '_');
}

static inline bool isIdentifierCharacter(UChar c)
{
    return (U_GET_GC_MASK(c) & (unicodeLetter | unicodeCombiningMark | unicodeDigit | unicodeConnectorPunctuation)) || (c == '$') || (c == '_') || (c == ZWNJ) || (c == ZWJ);
}

} // namespace

IDBKeyPathLexer::TokenType IDBKeyPathLexer::lexIdentifier(String& element)
{
    StringView start = m_remainingText;
    if (!m_remainingText.isEmpty() && isIdentifierStartCharacter(m_remainingText[0]))
        m_remainingText = m_remainingText.substring(1);
    else
        return TokenError;

    while (!m_remainingText.isEmpty() && isIdentifierCharacter(m_remainingText[0]))
        m_remainingText = m_remainingText.substring(1);

    element = start.substring(0, start.length() - m_remainingText.length()).toString();
    return TokenIdentifier;
}

static bool IDBIsValidKeyPath(const String& keyPath)
{
    IDBKeyPathParseError error;
    Vector<String> keyPathElements;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    return error == IDBKeyPathParseErrorNone;
}

void IDBParseKeyPath(const String& keyPath, Vector<String>& elements, IDBKeyPathParseError& error)
{
    // IDBKeyPath ::= EMPTY_STRING | identifier ('.' identifier)*
    // The basic state machine is:
    //   Start => {Identifier, End}
    //   Identifier => {Dot, End}
    //   Dot => {Identifier}
    // It bails out as soon as it finds an error, but doesn't discard the bits it managed to parse.
    enum ParserState { Identifier, Dot, End };

    IDBKeyPathLexer lexer(keyPath);
    IDBKeyPathLexer::TokenType tokenType = lexer.nextTokenType();
    ParserState state;
    if (tokenType == IDBKeyPathLexer::TokenIdentifier)
        state = Identifier;
    else if (tokenType == IDBKeyPathLexer::TokenEnd)
        state = End;
    else {
        error = IDBKeyPathParseErrorStart;
        return;
    }

    while (1) {
        switch (state) {
        case Identifier : {
            IDBKeyPathLexer::TokenType tokenType = lexer.currentTokenType();
            ASSERT(tokenType == IDBKeyPathLexer::TokenIdentifier);

            String element = lexer.currentElement();
            elements.append(element);

            tokenType = lexer.nextTokenType();
            if (tokenType == IDBKeyPathLexer::TokenDot)
                state = Dot;
            else if (tokenType == IDBKeyPathLexer::TokenEnd)
                state = End;
            else {
                error = IDBKeyPathParseErrorIdentifier;
                return;
            }
            break;
        }
        case Dot: {
            IDBKeyPathLexer::TokenType tokenType = lexer.currentTokenType();
            ASSERT(tokenType == IDBKeyPathLexer::TokenDot);

            tokenType = lexer.nextTokenType();
            if (tokenType == IDBKeyPathLexer::TokenIdentifier)
                state = Identifier;
            else {
                error = IDBKeyPathParseErrorDot;
                return;
            }
            break;
        }
        case End: {
            error = IDBKeyPathParseErrorNone;
            return;
        }
        }
    }
}

IDBKeyPath::IDBKeyPath(const String& string)
    : m_type(StringType)
    , m_string(string)
{
    ASSERT(!m_string.isNull());
}

IDBKeyPath::IDBKeyPath(const Vector<String>& array)
    : m_type(ArrayType)
    , m_array(array)
{
#ifndef NDEBUG
    for (size_t i = 0; i < m_array.size(); ++i)
        ASSERT(!m_array[i].isNull());
#endif
}

bool IDBKeyPath::isValid() const
{
    switch (m_type) {
    case NullType:
        return false;

    case StringType:
        return IDBIsValidKeyPath(m_string);

    case ArrayType:
        if (m_array.isEmpty())
            return false;
        for (size_t i = 0; i < m_array.size(); ++i) {
            if (!IDBIsValidKeyPath(m_array[i]))
                return false;
        }
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool IDBKeyPath::operator==(const IDBKeyPath& other) const
{
    if (m_type != other.m_type)
        return false;

    switch (m_type) {
    case NullType:
        return true;
    case StringType:
        return m_string == other.m_string;
    case ArrayType:
        return m_array == other.m_array;
    }
    ASSERT_NOT_REACHED();
    return false;
}

IDBKeyPath IDBKeyPath::isolatedCopy() const
{
    IDBKeyPath result;
    result.m_type = m_type;
    result.m_string = m_string.isolatedCopy();

    result.m_array.reserveInitialCapacity(m_array.size());
    for (size_t i = 0; i < m_array.size(); ++i)
        result.m_array.uncheckedAppend(m_array[i].isolatedCopy());

    return result;
}

void IDBKeyPath::encode(KeyedEncoder& encoder) const
{
    encoder.encodeEnum("type", m_type);
    switch (m_type) {
    case IDBKeyPath::NullType:
        break;
    case IDBKeyPath::StringType:
        encoder.encodeString("string", m_string);
        break;
    case IDBKeyPath::ArrayType:
        encoder.encodeObjects("array", m_array.begin(), m_array.end(), [](WebCore::KeyedEncoder& encoder, const String& string) {
            encoder.encodeString("string", string);
        });
        break;
    default:
        ASSERT_NOT_REACHED();
    };
}

bool IDBKeyPath::decode(KeyedDecoder& decoder, IDBKeyPath& result)
{
    auto enumFunction = [](int64_t value) {
        return value == NullType || value == StringType || value == ArrayType;
    };

    if (!decoder.decodeVerifiedEnum("type", result.m_type, enumFunction))
        return false;

    if (result.m_type == NullType)
        return true;

    if (result.m_type == StringType)
        return decoder.decodeString("string", result.m_string);

    ASSERT(result.m_type == ArrayType);

    auto arrayFunction = [](KeyedDecoder& decoder, String& result) {
        return decoder.decodeString("string", result);
    };

    return decoder.decodeObjects("array", result.m_array, arrayFunction);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
