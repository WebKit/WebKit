/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)
#include "IDBKeyPath.h"

#include <wtf/ASCIICType.h>
#include <wtf/dtoa.h>
#include <wtf/text/StringBuilder.h>

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
    return error == IDBKeyPathParseError::None;
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
        error = IDBKeyPathParseError::Start;
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
                error = IDBKeyPathParseError::Identifier;
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
                error = IDBKeyPathParseError::Dot;
                return;
            }
            break;
        }
        case End: {
            error = IDBKeyPathParseError::None;
            return;
        }
        }
    }
}

bool isIDBKeyPathValid(const IDBKeyPath& keyPath)
{
    auto visitor = WTF::makeVisitor([](const String& string) {
        return IDBIsValidKeyPath(string);
    }, [](const Vector<String>& vector) {
        if (vector.isEmpty())
            return false;
        for (auto& key : vector) {
            if (!IDBIsValidKeyPath(key))
                return false;
        }
        return true;
    });
    return WTF::visit(visitor, keyPath);
}

IDBKeyPath isolatedCopy(const IDBKeyPath& keyPath)
{
    auto visitor = WTF::makeVisitor([](const String& string) -> IDBKeyPath {
        return string.isolatedCopy();
    }, [](const Vector<String>& vector) -> IDBKeyPath {
        Vector<String> vectorCopy;
        vectorCopy.reserveInitialCapacity(vector.size());
        for (auto& string : vector)
            vectorCopy.uncheckedAppend(string.isolatedCopy());
        return vectorCopy;
    });

    return WTF::visit(visitor, keyPath);
}

#if !LOG_DISABLED
String loggingString(const IDBKeyPath& path)
{
    auto visitor = WTF::makeVisitor([](const String& string) {
        return makeString("< ", string, " >");
    }, [](const Vector<String>& strings) {
        if (strings.isEmpty())
            return "< >"_str;

        StringBuilder builder;
        builder.append("< ");
        for (size_t i = 0; i < strings.size() - 1; ++i) {
            builder.append(strings[i]);
            builder.append(", ");
        }
        builder.append(strings.last());
        builder.append(" >");

        return builder.toString();
    });

    return WTF::visit(visitor, path);
}
#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
