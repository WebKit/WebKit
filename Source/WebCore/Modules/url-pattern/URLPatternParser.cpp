/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "URLPatternParser.h"

#include "URLPatternTokenizer.h"
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace URLPatternUtilities {

URLPatternParser::URLPatternParser(EncodingCallbackType type, String&& segmentWildcardRegexp)
    : m_callbackType(type)
    , m_segmentWildcardRegexp(WTFMove(segmentWildcardRegexp))
{
}

ExceptionOr<void> URLPatternParser::performParse(const URLPatternStringOptions& options)
{
    ExceptionOr<void> maybeFunctionException;

    while (m_index < m_tokenList.size()) {
        auto charToken = tryToConsumeToken(TokenType::Char);
        auto nameToken = tryToConsumeToken(TokenType::Name);
        auto regexOrWildcardToken = tryToConsumeRegexOrWildcardToken(nameToken);

        if (!nameToken.isNull() || !regexOrWildcardToken.isNull()) {
            String prefix;

            if (!charToken.isNull())
                prefix = charToken.value.toString();

            if (!prefix.isEmpty() && prefix != options.prefixCodepoint)
                m_pendingFixedValue.append(std::exchange(prefix, { }));

            maybeFunctionException = maybeAddPartFromPendingFixedValue();
            if (maybeFunctionException.hasException())
                return maybeFunctionException.releaseException();

            auto modifierToken = tryToConsumeModifierToken();

            maybeFunctionException = addPart(WTFMove(prefix), nameToken, regexOrWildcardToken, { }, modifierToken);
            if (maybeFunctionException.hasException())
                return maybeFunctionException.releaseException();

            continue;
        }

        auto fixedToken = charToken;

        if (fixedToken.isNull())
            fixedToken = tryToConsumeToken(TokenType::EscapedChar);

        if (!fixedToken.isNull()) {
            m_pendingFixedValue.append(WTFMove(fixedToken.value));

            continue;
        }

        auto openToken = tryToConsumeToken(TokenType::Open);
        if (!openToken.isNull()) {
            String prefix = consumeText();
            nameToken = tryToConsumeToken(TokenType::Name);
            regexOrWildcardToken = tryToConsumeRegexOrWildcardToken(nameToken);
            String suffix = consumeText();
            consumeRequiredToken(TokenType::Close);
            auto modifierToken = tryToConsumeModifierToken();

            maybeFunctionException = addPart(WTFMove(prefix), nameToken, regexOrWildcardToken, WTFMove(suffix), modifierToken);
            if (maybeFunctionException.hasException())
                return maybeFunctionException.releaseException();

            continue;
        }

        maybeFunctionException = maybeAddPartFromPendingFixedValue();
        if (maybeFunctionException.hasException())
            return maybeFunctionException.releaseException();

        consumeRequiredToken(TokenType::End);
    }

    return { };
}

// https://urlpattern.spec.whatwg.org/#try-to-consume-a-token
Token URLPatternParser::tryToConsumeToken(TokenType type)
{
    auto& nextToken = m_tokenList[m_index];

    if (nextToken.type != type)
        return { };

    ++m_index;

    return nextToken;
}

// https://urlpattern.spec.whatwg.org/#try-to-consume-a-regexp-or-wildcard-token
Token URLPatternParser::tryToConsumeRegexOrWildcardToken(const Token& token)
{
    auto tokenResult = tryToConsumeToken(TokenType::Regexp);

    if (tokenResult.isNull() && token.isNull())
        tokenResult = tryToConsumeToken(TokenType::Asterisk);

    return tokenResult;
}

// https://urlpattern.spec.whatwg.org/#try-to-consume-a-modifier-token
Token URLPatternParser::tryToConsumeModifierToken()
{
    auto token = tryToConsumeToken(TokenType::OtherModifier);
    if (!token.isNull())
        return token;

    return tryToConsumeToken(TokenType::Asterisk);
}

// https://urlpattern.spec.whatwg.org/#consume-text
String URLPatternParser::consumeText()
{
    StringBuilder result;

    while (true) {
        auto consumedToken = tryToConsumeToken(TokenType::Char);

        if (consumedToken.isNull())
            consumedToken = tryToConsumeToken(TokenType::EscapedChar);

        if (consumedToken.isNull())
            break;

        result.append(consumedToken.value);
    }

    return result.toString();
}

// https://urlpattern.spec.whatwg.org/#consume-a-required-token
ExceptionOr<Token> URLPatternParser::consumeRequiredToken(TokenType type)
{
    auto result = tryToConsumeToken(type);

    if (result.isNull())
        return Exception { ExceptionCode::TypeError, "Null token was produced when consuming a required token."_s };

    return result;
}

// https://urlpattern.spec.whatwg.org/#maybe-add-a-part-from-the-pending-fixed-value
ExceptionOr<void> URLPatternParser::maybeAddPartFromPendingFixedValue()
{
    if (m_pendingFixedValue.isEmpty())
        return { };

    auto encodedValue = callEncodingCallback(m_callbackType, m_pendingFixedValue.toString());
    m_pendingFixedValue.clear();

    if (encodedValue.hasException())
        return encodedValue.releaseException();

    m_partList.append(Part { .type = PartType::FixedText, .value = encodedValue.releaseReturnValue(), .modifier = Modifier::None });

    return { };
}

// https://urlpattern.spec.whatwg.org/#add-a-part
ExceptionOr<void> URLPatternParser::addPart(String&& prefix, const Token& nameToken, const Token& regexpOrWildcardToken, String&& suffix, const Token& modifierToken)
{
    Modifier modifier = Modifier::None;

    if (!modifierToken.isNull()) {
        if (modifierToken.value == "?"_s)
            modifier = Modifier::Optional;
        else if (modifierToken.value == "*"_s)
            modifier = Modifier::ZeroOrMore;
        else if (modifierToken.value == "+"_s)
            modifier = Modifier::OneOrMore;
    }

    if (nameToken.isNull() && regexpOrWildcardToken.isNull() && modifier == Modifier::None) {
        m_pendingFixedValue.append(WTFMove(prefix));

        return { };
    }

    auto maybeFunctionException = maybeAddPartFromPendingFixedValue();
    if (maybeFunctionException.hasException())
        return maybeFunctionException.releaseException();

    if (nameToken.isNull() && regexpOrWildcardToken.isNull()) {
        ASSERT(suffix.isEmpty());

        if (prefix.isEmpty())
            return { };

        auto encodedValue = callEncodingCallback(m_callbackType, WTFMove(prefix));
        if (encodedValue.hasException())
            return encodedValue.releaseException();

        m_partList.append(Part { .type = PartType::FixedText, .value = encodedValue.releaseReturnValue(), .modifier = modifier });

        return { };
    }

    String regexValue;

    if (regexpOrWildcardToken.isNull())
        regexValue = m_segmentWildcardRegexp;
    else if (regexpOrWildcardToken.type == TokenType::Asterisk)
        regexValue = ".*"_s;
    else
        regexValue = regexpOrWildcardToken.value.toString();

    PartType type = PartType::Regexp;

    if (regexValue == m_segmentWildcardRegexp) {
        type = PartType::SegmentWildcard;
        regexValue = { };
    } else if (regexValue == ".*"_s) {
        type = PartType::FullWildcard;
        regexValue = { };
    }

    String name;

    if (!nameToken.isNull())
        name = nameToken.value.toString();
    else if (regexpOrWildcardToken.isNull()) {
        name = String::number(m_nextNumericName);
        ++m_nextNumericName;
    }

    if (isDuplicateName(name))
        return Exception { ExceptionCode::TypeError, "Duplicate name token produced when adding to parser part list."_s };

    auto encodedPrefix = callEncodingCallback(m_callbackType, WTFMove(prefix));
    if (encodedPrefix.hasException())
        return encodedPrefix.releaseException();

    auto encodedSuffix = callEncodingCallback(m_callbackType, WTFMove(suffix));
    if (encodedSuffix.hasException())
        return encodedSuffix.releaseException();

    m_partList.append(Part { type, WTFMove(regexValue), modifier, WTFMove(name), encodedPrefix.releaseReturnValue(), encodedSuffix.releaseReturnValue() });

    return { };
}

// https://urlpattern.spec.whatwg.org/#is-a-duplicate-name
bool URLPatternParser::isDuplicateName(StringView name) const
{
    return m_partList.containsIf([&](auto& part) {
        return part.name == name;
    });
}

// https://urlpattern.spec.whatwg.org/#parse-a-pattern-string
ExceptionOr<const Vector<Part>&> parse(StringView patternStringInput, const URLPatternStringOptions& options, EncodingCallbackType type)
{
    URLPatternParser tokenParser { type, generateSegmentWildcardRegexp(options) };

    auto maybeParserTokenList = Tokenizer(patternStringInput, TokenizePolicy::Strict).tokenize();
    if (maybeParserTokenList.hasException())
        return maybeParserTokenList.releaseException();

    tokenParser.setTokenList(maybeParserTokenList.releaseReturnValue());

    tokenParser.performParse(options);

    return tokenParser.getPartList();
}

// https://urlpattern.spec.whatwg.org/#generate-a-segment-wildcard-regexp
String generateSegmentWildcardRegexp(const URLPatternStringOptions& options)
{
    return makeString("[^"_s, escapeRegexString(options.delimiterCodepoint), "]+?"_s);
}

template<typename CharacterType>
static String escapeRegexStringForCharacters(std::span<const CharacterType> characters)
{
    static constexpr std::array regexEscapeCharacters { '.', '+', '*', '?', '^', '$', '{', '}', '(', ')', '[', ']', '|', '/', '\\' }; // NOLINT

    StringBuilder result;
    result.reserveCapacity(characters.size());

    for (auto character : characters) {
        if (std::find(regexEscapeCharacters.begin(), regexEscapeCharacters.end(), character) != regexEscapeCharacters.end())
            result.append('\\');

        result.append(character);
    }

    return result.toString();
}

// https://urlpattern.spec.whatwg.org/#escape-a-regexp-string
String escapeRegexString(StringView input)
{
    ASSERT(input.containsOnlyASCII());

    if (input.is8Bit())
        return escapeRegexStringForCharacters(input.span8());

    return escapeRegexStringForCharacters(input.span16());
}

} // namespace URLPatternUtilities
} // namespace WebCore
