// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "CSSTokenizer.h"

#include "CSSParserIdioms.h"
#include "CSSParserObserverWrapper.h"
#include "CSSParserTokenRange.h"
#include "CSSTokenizerInputStream.h"
#include "HTMLParserIdioms.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

// See: http://dev.w3.org/csswg/css-syntax/#input-preprocessing
static String preprocessString(String string)
{
    // According to the specification, we should replace '\r' and '\f' with '\n' but we do not need to
    // because our CSSTokenizer treats all of them as new lines.
    return string.replace('\0', replacementCharacter);
}

std::unique_ptr<CSSTokenizer> CSSTokenizer::tryCreate(const String& string)
{
    bool success = true;
    // We can't use makeUnique here because it does not have access to this private constructor.
    auto tokenizer = std::unique_ptr<CSSTokenizer>(new CSSTokenizer(preprocessString(string), nullptr, &success));
    if (UNLIKELY(!success))
        return nullptr;
    return tokenizer;
}

std::unique_ptr<CSSTokenizer> CSSTokenizer::tryCreate(const String& string, CSSParserObserverWrapper& wrapper)
{
    bool success = true;
    // We can't use makeUnique here because it does not have access to this private constructor.
    auto tokenizer = std::unique_ptr<CSSTokenizer>(new CSSTokenizer(preprocessString(string), &wrapper, &success));
    if (UNLIKELY(!success))
        return nullptr;
    return tokenizer;
}

CSSTokenizer::CSSTokenizer(const String& string)
    : CSSTokenizer(preprocessString(string), nullptr, nullptr)
{
}

CSSTokenizer::CSSTokenizer(const String& string, CSSParserObserverWrapper& wrapper)
    : CSSTokenizer(preprocessString(string), &wrapper, nullptr)
{
}

CSSTokenizer::CSSTokenizer(String&& string, CSSParserObserverWrapper* wrapper, bool* constructionSuccessPtr)
    : m_input(string)
{
    if (constructionSuccessPtr)
        *constructionSuccessPtr = true;

    if (string.isEmpty())
        return;

    // To avoid resizing we err on the side of reserving too much space.
    // Most strings we tokenize have about 3.5 to 5 characters per token.
    if (UNLIKELY(!m_tokens.tryReserveInitialCapacity(string.length() / 3))) {
        // When constructionSuccessPtr is null, our policy is to crash on failure.
        RELEASE_ASSERT(constructionSuccessPtr);
        *constructionSuccessPtr = false;
        return;
    }

    unsigned offset = 0;
    while (true) {
        CSSParserToken token = nextToken();
        if (token.type() == EOFToken)
            break;
        if (token.type() == CommentToken) {
            if (wrapper)
                wrapper->addComment(offset, m_input.offset(), m_tokens.size());
        } else {
            if (UNLIKELY(!m_tokens.tryAppend(token))) {
                // When constructionSuccessPtr is null, our policy is to crash on failure.
                RELEASE_ASSERT(constructionSuccessPtr);
                *constructionSuccessPtr = false;
                return;
            }
            if (wrapper)
                wrapper->addToken(offset);
        }
        offset = m_input.offset();
    }

    if (wrapper) {
        wrapper->addToken(offset);
        wrapper->finalizeConstruction(m_tokens.begin());
    }
}

CSSParserTokenRange CSSTokenizer::tokenRange() const
{
    return m_tokens;
}

unsigned CSSTokenizer::tokenCount()
{
    return m_tokens.size();
}

static bool isNewLine(UChar cc)
{
    // We check \r and \f here, since we have no preprocessing stage
    return (cc == '\r' || cc == '\n' || cc == '\f');
}

// http://dev.w3.org/csswg/css-syntax/#check-if-two-code-points-are-a-valid-escape
static bool twoCharsAreValidEscape(UChar first, UChar second)
{
    return first == '\\' && !isNewLine(second);
}

void CSSTokenizer::reconsume(UChar c)
{
    m_input.pushBack(c);
}

UChar CSSTokenizer::consume()
{
    UChar current = m_input.nextInputChar();
    m_input.advance();
    return current;
}

CSSParserToken CSSTokenizer::whiteSpace(UChar /*cc*/)
{
    m_input.advanceUntilNonWhitespace();
    return CSSParserToken(WhitespaceToken);
}

CSSParserToken CSSTokenizer::blockStart(CSSParserTokenType type)
{
    m_blockStack.append(type);
    return CSSParserToken(type, CSSParserToken::BlockStart);
}

CSSParserToken CSSTokenizer::blockStart(CSSParserTokenType blockType, CSSParserTokenType type, StringView name)
{
    m_blockStack.append(blockType);
    return CSSParserToken(type, name, CSSParserToken::BlockStart);
}

CSSParserToken CSSTokenizer::blockEnd(CSSParserTokenType type, CSSParserTokenType startType)
{
    if (!m_blockStack.isEmpty() && m_blockStack.last() == startType) {
        m_blockStack.removeLast();
        return CSSParserToken(type, CSSParserToken::BlockEnd);
    }
    return CSSParserToken(type);
}

CSSParserToken CSSTokenizer::leftParenthesis(UChar /*cc*/)
{
    return blockStart(LeftParenthesisToken);
}

CSSParserToken CSSTokenizer::rightParenthesis(UChar /*cc*/)
{
    return blockEnd(RightParenthesisToken, LeftParenthesisToken);
}

CSSParserToken CSSTokenizer::leftBracket(UChar /*cc*/)
{
    return blockStart(LeftBracketToken);
}

CSSParserToken CSSTokenizer::rightBracket(UChar /*cc*/)
{
    return blockEnd(RightBracketToken, LeftBracketToken);
}

CSSParserToken CSSTokenizer::leftBrace(UChar /*cc*/)
{
    return blockStart(LeftBraceToken);
}

CSSParserToken CSSTokenizer::rightBrace(UChar /*cc*/)
{
    return blockEnd(RightBraceToken, LeftBraceToken);
}

CSSParserToken CSSTokenizer::plusOrFullStop(UChar cc)
{
    if (nextCharsAreNumber(cc)) {
        reconsume(cc);
        return consumeNumericToken();
    }
    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::asterisk(UChar cc)
{
    ASSERT_UNUSED(cc, cc == '*');
    if (consumeIfNext('='))
        return CSSParserToken(SubstringMatchToken);
    return CSSParserToken(DelimiterToken, '*');
}

CSSParserToken CSSTokenizer::lessThan(UChar cc)
{
    ASSERT_UNUSED(cc, cc == '<');
    if (m_input.peek(0) == '!' && m_input.peek(1) == '-' && m_input.peek(2) == '-') {
        m_input.advance(3);
        return CSSParserToken(CDOToken);
    }
    return CSSParserToken(DelimiterToken, '<');
}

CSSParserToken CSSTokenizer::comma(UChar /*cc*/)
{
    return CSSParserToken(CommaToken);
}

CSSParserToken CSSTokenizer::hyphenMinus(UChar cc)
{
    if (nextCharsAreNumber(cc)) {
        reconsume(cc);
        return consumeNumericToken();
    }
    if (m_input.peek(0) == '-' && m_input.peek(1) == '>') {
        m_input.advance(2);
        return CSSParserToken(CDCToken);
    }
    if (nextCharsAreIdentifier(cc)) {
        reconsume(cc);
        return consumeIdentLikeToken();
    }
    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::solidus(UChar cc)
{
    if (consumeIfNext('*')) {
        // These get ignored, but we need a value to return.
        consumeUntilCommentEndFound();
        return CSSParserToken(CommentToken);
    }

    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::colon(UChar /*cc*/)
{
    return CSSParserToken(ColonToken);
}

CSSParserToken CSSTokenizer::semiColon(UChar /*cc*/)
{
    return CSSParserToken(SemicolonToken);
}

CSSParserToken CSSTokenizer::hash(UChar cc)
{
    UChar nextChar = m_input.peek(0);
    if (isNameCodePoint(nextChar) || twoCharsAreValidEscape(nextChar, m_input.peek(1))) {
        HashTokenType type = nextCharsAreIdentifier() ? HashTokenId : HashTokenUnrestricted;
        return CSSParserToken(type, consumeName());
    }

    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::circumflexAccent(UChar cc)
{
    ASSERT_UNUSED(cc, cc == '^');
    if (consumeIfNext('='))
        return CSSParserToken(PrefixMatchToken);
    return CSSParserToken(DelimiterToken, '^');
}

CSSParserToken CSSTokenizer::dollarSign(UChar cc)
{
    ASSERT_UNUSED(cc, cc == '$');
    if (consumeIfNext('='))
        return CSSParserToken(SuffixMatchToken);
    return CSSParserToken(DelimiterToken, '$');
}

CSSParserToken CSSTokenizer::verticalLine(UChar cc)
{
    ASSERT_UNUSED(cc, cc == '|');
    if (consumeIfNext('='))
        return CSSParserToken(DashMatchToken);
    if (consumeIfNext('|'))
        return CSSParserToken(ColumnToken);
    return CSSParserToken(DelimiterToken, '|');
}

CSSParserToken CSSTokenizer::tilde(UChar cc)
{
    ASSERT_UNUSED(cc, cc == '~');
    if (consumeIfNext('='))
        return CSSParserToken(IncludeMatchToken);
    return CSSParserToken(DelimiterToken, '~');
}

CSSParserToken CSSTokenizer::commercialAt(UChar cc)
{
    ASSERT_UNUSED(cc, cc == '@');
    if (nextCharsAreIdentifier())
        return CSSParserToken(AtKeywordToken, consumeName());
    return CSSParserToken(DelimiterToken, '@');
}

CSSParserToken CSSTokenizer::reverseSolidus(UChar cc)
{
    if (twoCharsAreValidEscape(cc, m_input.peek(0))) {
        reconsume(cc);
        return consumeIdentLikeToken();
    }
    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::asciiDigit(UChar cc)
{
    reconsume(cc);
    return consumeNumericToken();
}

CSSParserToken CSSTokenizer::letterU(UChar cc)
{
    if (m_input.peek(0) == '+' && (isASCIIHexDigit(m_input.peek(1)) || m_input.peek(1) == '?')) {
        m_input.advance();
        return consumeUnicodeRange();
    }
    reconsume(cc);
    return consumeIdentLikeToken();
}

CSSParserToken CSSTokenizer::nameStart(UChar cc)
{
    reconsume(cc);
    return consumeIdentLikeToken();
}

CSSParserToken CSSTokenizer::stringStart(UChar cc)
{
    return consumeStringTokenUntil(cc);
}

CSSParserToken CSSTokenizer::endOfFile(UChar /*cc*/)
{
    return CSSParserToken(EOFToken);
}

const CSSTokenizer::CodePoint CSSTokenizer::codePoints[128] = {
    &CSSTokenizer::endOfFile,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    &CSSTokenizer::whiteSpace,
    &CSSTokenizer::whiteSpace,
    0,
    &CSSTokenizer::whiteSpace,
    &CSSTokenizer::whiteSpace,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    &CSSTokenizer::whiteSpace,
    0,
    &CSSTokenizer::stringStart,
    &CSSTokenizer::hash,
    &CSSTokenizer::dollarSign,
    0,
    0,
    &CSSTokenizer::stringStart,
    &CSSTokenizer::leftParenthesis,
    &CSSTokenizer::rightParenthesis,
    &CSSTokenizer::asterisk,
    &CSSTokenizer::plusOrFullStop,
    &CSSTokenizer::comma,
    &CSSTokenizer::hyphenMinus,
    &CSSTokenizer::plusOrFullStop,
    &CSSTokenizer::solidus,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::asciiDigit,
    &CSSTokenizer::colon,
    &CSSTokenizer::semiColon,
    &CSSTokenizer::lessThan,
    0,
    0,
    0,
    &CSSTokenizer::commercialAt,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::letterU,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::leftBracket,
    &CSSTokenizer::reverseSolidus,
    &CSSTokenizer::rightBracket,
    &CSSTokenizer::circumflexAccent,
    &CSSTokenizer::nameStart,
    0,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::letterU,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::nameStart,
    &CSSTokenizer::leftBrace,
    &CSSTokenizer::verticalLine,
    &CSSTokenizer::rightBrace,
    &CSSTokenizer::tilde,
    0,
};
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
const unsigned codePointsNumber = 128;
#endif

CSSParserToken CSSTokenizer::nextToken()
{
    // Unlike the HTMLTokenizer, the CSS Syntax spec is written
    // as a stateless, (fixed-size) look-ahead tokenizer.
    // We could move to the stateful model and instead create
    // states for all the "next 3 codepoints are X" cases.
    // State-machine tokenizers are easier to write to handle
    // incremental tokenization of partial sources.
    // However, for now we follow the spec exactly.
    UChar cc = consume();
    CodePoint codePointFunc = 0;

    if (isASCII(cc)) {
        ASSERT_WITH_SECURITY_IMPLICATION(cc < codePointsNumber);
        codePointFunc = codePoints[cc];
    } else
        codePointFunc = &CSSTokenizer::nameStart;

    if (codePointFunc)
        return ((this)->*(codePointFunc))(cc);
    return CSSParserToken(DelimiterToken, cc);
}

// This method merges the following spec sections for efficiency
// http://www.w3.org/TR/css3-syntax/#consume-a-number
// http://www.w3.org/TR/css3-syntax/#convert-a-string-to-a-number
CSSParserToken CSSTokenizer::consumeNumber()
{
    ASSERT(nextCharsAreNumber());

    NumericValueType type = IntegerValueType;
    NumericSign sign = NoSign;
    unsigned numberLength = 0;

    UChar next = m_input.peek(0);
    if (next == '+') {
        ++numberLength;
        sign = PlusSign;
    } else if (next == '-') {
        ++numberLength;
        sign = MinusSign;
    }

    numberLength = m_input.skipWhilePredicate<isASCIIDigit>(numberLength);
    next = m_input.peek(numberLength);
    if (next == '.' && isASCIIDigit(m_input.peek(numberLength + 1))) {
        type = NumberValueType;
        numberLength = m_input.skipWhilePredicate<isASCIIDigit>(numberLength + 2);
        next = m_input.peek(numberLength);
    }

    if (next == 'E' || next == 'e') {
        next = m_input.peek(numberLength + 1);
        if (isASCIIDigit(next)) {
            type = NumberValueType;
            numberLength = m_input.skipWhilePredicate<isASCIIDigit>(numberLength + 1);
        } else if ((next == '+' || next == '-') && isASCIIDigit(m_input.peek(numberLength + 2))) {
            type = NumberValueType;
            numberLength = m_input.skipWhilePredicate<isASCIIDigit>(numberLength + 3);
        }
    }

    double value = m_input.getDouble(0, numberLength);
    m_input.advance(numberLength);

    return CSSParserToken(NumberToken, value, type, sign);
}

// http://www.w3.org/TR/css3-syntax/#consume-a-numeric-token
CSSParserToken CSSTokenizer::consumeNumericToken()
{
    CSSParserToken token = consumeNumber();
    if (nextCharsAreIdentifier())
        token.convertToDimensionWithUnit(consumeName());
    else if (consumeIfNext('%'))
        token.convertToPercentage();
    return token;
}

// http://dev.w3.org/csswg/css-syntax/#consume-ident-like-token
CSSParserToken CSSTokenizer::consumeIdentLikeToken()
{
    StringView name = consumeName();
    if (consumeIfNext('(')) {
        if (equalIgnoringASCIICase(name, "url")) {
            // The spec is slightly different so as to avoid dropping whitespace
            // tokens, but they wouldn't be used and this is easier.
            m_input.advanceUntilNonWhitespace();
            UChar next = m_input.peek(0);
            if (next != '"' && next != '\'')
                return consumeUrlToken();
        }
        return blockStart(LeftParenthesisToken, FunctionToken, name);
    }
    return CSSParserToken(IdentToken, name);
}

// http://dev.w3.org/csswg/css-syntax/#consume-a-string-token
CSSParserToken CSSTokenizer::consumeStringTokenUntil(UChar endingCodePoint)
{
    // Strings without escapes get handled without allocations
    for (unsigned size = 0; ; size++) {
        UChar cc = m_input.peek(size);
        if (cc == endingCodePoint) {
            unsigned startOffset = m_input.offset();
            m_input.advance(size + 1);
            return CSSParserToken(StringToken, m_input.rangeAt(startOffset, size));
        }
        if (isNewLine(cc)) {
            m_input.advance(size);
            return CSSParserToken(BadStringToken);
        }
        if (cc == kEndOfFileMarker || cc == '\\')
            break;
    }

    StringBuilder output;
    while (true) {
        UChar cc = consume();
        if (cc == endingCodePoint || cc == kEndOfFileMarker)
            return CSSParserToken(StringToken, registerString(output.toString()));
        if (isNewLine(cc)) {
            reconsume(cc);
            return CSSParserToken(BadStringToken);
        }
        if (cc == '\\') {
            if (m_input.nextInputChar() == kEndOfFileMarker)
                continue;
            if (isNewLine(m_input.peek(0)))
                consumeSingleWhitespaceIfNext(); // This handles \r\n for us
            else
                output.appendCharacter(consumeEscape());
        } else
            output.append(cc);
    }
}

CSSParserToken CSSTokenizer::consumeUnicodeRange()
{
    ASSERT(isASCIIHexDigit(m_input.peek(0)) || m_input.peek(0) == '?');
    int lengthRemaining = 6;
    UChar32 start = 0;

    while (lengthRemaining && isASCIIHexDigit(m_input.peek(0))) {
        start = start * 16 + toASCIIHexValue(consume());
        --lengthRemaining;
    }

    UChar32 end = start;
    if (lengthRemaining && consumeIfNext('?')) {
        do {
            start *= 16;
            end = end * 16 + 0xF;
            --lengthRemaining;
        } while (lengthRemaining && consumeIfNext('?'));
    } else if (m_input.peek(0) == '-' && isASCIIHexDigit(m_input.peek(1))) {
        m_input.advance();
        lengthRemaining = 6;
        end = 0;
        do {
            end = end * 16 + toASCIIHexValue(consume());
            --lengthRemaining;
        } while (lengthRemaining && isASCIIHexDigit(m_input.peek(0)));
    }

    return CSSParserToken(UnicodeRangeToken, start, end);
}

// http://dev.w3.org/csswg/css-syntax/#non-printable-code-point
static bool isNonPrintableCodePoint(UChar cc)
{
    return cc <= '\x8' || cc == '\xb' || (cc >= '\xe' && cc <= '\x1f') || cc == '\x7f';
}

// http://dev.w3.org/csswg/css-syntax/#consume-url-token
CSSParserToken CSSTokenizer::consumeUrlToken()
{
    m_input.advanceUntilNonWhitespace();

    // URL tokens without escapes get handled without allocations
    for (unsigned size = 0; ; size++) {
        UChar cc = m_input.peek(size);
        if (cc == ')') {
            unsigned startOffset = m_input.offset();
            m_input.advance(size + 1);
            return CSSParserToken(UrlToken, m_input.rangeAt(startOffset, size));
        }
        if (cc <= ' ' || cc == '\\' || cc == '"' || cc == '\'' || cc == '(' || cc == '\x7f')
            break;
    }

    StringBuilder result;
    while (true) {
        UChar cc = consume();
        if (cc == ')' || cc == kEndOfFileMarker)
            return CSSParserToken(UrlToken, registerString(result.toString()));

        if (isHTMLSpace(cc)) {
            m_input.advanceUntilNonWhitespace();
            if (consumeIfNext(')') || m_input.nextInputChar() == kEndOfFileMarker)
                return CSSParserToken(UrlToken, registerString(result.toString()));
            break;
        }

        if (cc == '"' || cc == '\'' || cc == '(' || isNonPrintableCodePoint(cc))
            break;

        if (cc == '\\') {
            if (twoCharsAreValidEscape(cc, m_input.peek(0))) {
                result.appendCharacter(consumeEscape());
                continue;
            }
            break;
        }

        result.append(cc);
    }

    consumeBadUrlRemnants();
    return CSSParserToken(BadUrlToken);
}

// http://dev.w3.org/csswg/css-syntax/#consume-the-remnants-of-a-bad-url
void CSSTokenizer::consumeBadUrlRemnants()
{
    while (true) {
        UChar cc = consume();
        if (cc == ')' || cc == kEndOfFileMarker)
            return;
        if (twoCharsAreValidEscape(cc, m_input.peek(0)))
            consumeEscape();
    }
}

void CSSTokenizer::consumeSingleWhitespaceIfNext()
{
    // We check for \r\n and HTML spaces since we don't do preprocessing
    UChar next = m_input.peek(0);
    if (next == '\r' && m_input.peek(1) == '\n')
        m_input.advance(2);
    else if (isHTMLSpace(next))
        m_input.advance();
}

void CSSTokenizer::consumeUntilCommentEndFound()
{
    UChar c = consume();
    while (true) {
        if (c == kEndOfFileMarker)
            return;
        if (c != '*') {
            c = consume();
            continue;
        }
        c = consume();
        if (c == '/')
            return;
    }
}

bool CSSTokenizer::consumeIfNext(UChar character)
{
    // Since we're not doing replacement we can't tell the difference from
    // a NUL in the middle and the kEndOfFileMarker, so character must not be
    // NUL.
    ASSERT(character);
    if (m_input.peek(0) == character) {
        m_input.advance();
        return true;
    }
    return false;
}

// http://www.w3.org/TR/css3-syntax/#consume-a-name
StringView CSSTokenizer::consumeName()
{
    // Names without escapes get handled without allocations
    for (unsigned size = 0; ; ++size) {
        UChar cc = m_input.peek(size);
        if (isNameCodePoint(cc))
            continue;
        // peek will return NUL when we hit the end of the
        // input. In that case we want to still use the rangeAt() fast path
        // below.
        if (cc == kEndOfFileMarker && m_input.offset() + size < m_input.length())
            break;
        if (cc == '\\')
            break;
        unsigned startOffset = m_input.offset();
        m_input.advance(size);
        return m_input.rangeAt(startOffset, size);
    }

    StringBuilder result;
    while (true) {
        UChar cc = consume();
        if (isNameCodePoint(cc)) {
            result.append(cc);
            continue;
        }
        if (twoCharsAreValidEscape(cc, m_input.peek(0))) {
            result.appendCharacter(consumeEscape());
            continue;
        }
        reconsume(cc);
        return registerString(result.toString());
    }
}

// http://dev.w3.org/csswg/css-syntax/#consume-an-escaped-code-point
UChar32 CSSTokenizer::consumeEscape()
{
    UChar cc = consume();
    ASSERT(!isNewLine(cc));
    if (isASCIIHexDigit(cc)) {
        unsigned consumedHexDigits = 1;
        StringBuilder hexChars;
        hexChars.append(cc);
        while (consumedHexDigits < 6 && isASCIIHexDigit(m_input.peek(0))) {
            cc = consume();
            hexChars.append(cc);
            consumedHexDigits++;
        };
        consumeSingleWhitespaceIfNext();
        bool ok = false;
        UChar32 codePoint = hexChars.toString().toUIntStrict(&ok, 16);
        ASSERT(ok);
        if (!codePoint || (0xD800 <= codePoint && codePoint <= 0xDFFF) || codePoint > 0x10FFFF)
            return replacementCharacter;
        return codePoint;
    }

    if (cc == kEndOfFileMarker)
        return replacementCharacter;
    return cc;
}

bool CSSTokenizer::nextTwoCharsAreValidEscape()
{
    return twoCharsAreValidEscape(m_input.peek(0), m_input.peek(1));
}

// http://www.w3.org/TR/css3-syntax/#starts-with-a-number
bool CSSTokenizer::nextCharsAreNumber(UChar first)
{
    UChar second = m_input.peek(0);
    if (isASCIIDigit(first))
        return true;
    if (first == '+' || first == '-')
        return ((isASCIIDigit(second)) || (second == '.' && isASCIIDigit(m_input.peek(1))));
    if (first =='.')
        return (isASCIIDigit(second));
    return false;
}

bool CSSTokenizer::nextCharsAreNumber()
{
    UChar first = consume();
    bool areNumber = nextCharsAreNumber(first);
    reconsume(first);
    return areNumber;
}

// http://dev.w3.org/csswg/css-syntax/#would-start-an-identifier
bool CSSTokenizer::nextCharsAreIdentifier(UChar first)
{
    UChar second = m_input.peek(0);
    if (isNameStartCodePoint(first) || twoCharsAreValidEscape(first, second))
        return true;

    if (first == '-')
        return isNameStartCodePoint(second) || second == '-' || nextTwoCharsAreValidEscape();

    return false;
}

bool CSSTokenizer::nextCharsAreIdentifier()
{
    UChar first = consume();
    bool areIdentifier = nextCharsAreIdentifier(first);
    reconsume(first);
    return areIdentifier;
}

StringView CSSTokenizer::registerString(const String& string)
{
    m_stringPool.append(string);
    return string;
}

} // namespace WebCore
