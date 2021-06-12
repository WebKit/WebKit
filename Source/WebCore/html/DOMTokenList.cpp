/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMTokenList.h"

#include "HTMLParserIdioms.h"
#include "SpaceSplitString.h"
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

DOMTokenList::DOMTokenList(Element& element, const QualifiedName& attributeName, IsSupportedTokenFunction&& isSupportedToken)
    : m_element(element)
    , m_attributeName(attributeName)
    , m_isSupportedToken(WTFMove(isSupportedToken))
{
}

static inline bool tokenContainsHTMLSpace(const String& token)
{
    return token.find(isHTMLSpace) != notFound;
}

ExceptionOr<void> DOMTokenList::validateToken(const String& token)
{
    if (token.isEmpty())
        return Exception { SyntaxError };

    if (tokenContainsHTMLSpace(token))
        return Exception { InvalidCharacterError };

    return { };
}

ExceptionOr<void> DOMTokenList::validateTokens(const String* tokens, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        auto result = validateToken(tokens[i]);
        if (result.hasException())
            return result;
    }
    return { };
}

bool DOMTokenList::contains(const AtomString& token) const
{
    return tokens().contains(token);
}

inline ExceptionOr<void> DOMTokenList::addInternal(const String* newTokens, size_t length)
{
    // This is usually called with a single token.
    Vector<AtomString, 1> uniqueNewTokens;
    uniqueNewTokens.reserveInitialCapacity(length);

    auto& tokens = this->tokens();

    for (size_t i = 0; i < length; ++i) {
        auto result = validateToken(newTokens[i]);
        if (result.hasException())
            return result;
        if (!tokens.contains(newTokens[i]) && !uniqueNewTokens.contains(newTokens[i]))
            uniqueNewTokens.uncheckedAppend(newTokens[i]);
    }

    if (!uniqueNewTokens.isEmpty())
        tokens.appendVector(uniqueNewTokens);

    updateAssociatedAttributeFromTokens();

    return { };
}

ExceptionOr<void> DOMTokenList::add(const Vector<String>& tokens)
{
    return addInternal(tokens.data(), tokens.size());
}

ExceptionOr<void> DOMTokenList::add(const AtomString& token)
{
    return addInternal(&token.string(), 1);
}

inline ExceptionOr<void> DOMTokenList::removeInternal(const String* tokensToRemove, size_t length)
{
    auto result = validateTokens(tokensToRemove, length);
    if (result.hasException())
        return result;

    auto& tokens = this->tokens();
    for (size_t i = 0; i < length; ++i)
        tokens.removeFirst(tokensToRemove[i]);

    updateAssociatedAttributeFromTokens();

    return { };
}

ExceptionOr<void> DOMTokenList::remove(const Vector<String>& tokens)
{
    return removeInternal(tokens.data(), tokens.size());
}

ExceptionOr<void> DOMTokenList::remove(const AtomString& token)
{
    return removeInternal(&token.string(), 1);
}

ExceptionOr<bool> DOMTokenList::toggle(const AtomString& token, std::optional<bool> force)
{
    auto result = validateToken(token);
    if (result.hasException())
        return result.releaseException();

    auto& tokens = this->tokens();

    if (tokens.contains(token)) {
        if (!force.value_or(false)) {
            tokens.removeFirst(token);
            updateAssociatedAttributeFromTokens();
            return false;
        }
        return true;
    }

    if (force && !force.value())
        return false;

    tokens.append(token);
    updateAssociatedAttributeFromTokens();
    return true;
}

static inline void replaceInOrderedSet(Vector<AtomString>& tokens, size_t tokenIndex, const AtomString& newToken)
{
    ASSERT(tokenIndex != notFound);
    ASSERT(tokenIndex < tokens.size());

    auto newTokenIndex = tokens.find(newToken);
    if (newTokenIndex == notFound) {
        tokens[tokenIndex] = newToken;
        return;
    }

    if (newTokenIndex == tokenIndex)
        return;

    if (newTokenIndex > tokenIndex) {
        tokens[tokenIndex] = newToken;
        tokens.remove(newTokenIndex);
    } else
        tokens.remove(tokenIndex);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-replace
ExceptionOr<bool> DOMTokenList::replace(const AtomString& token, const AtomString& newToken)
{
    if (token.isEmpty() || newToken.isEmpty())
        return Exception { SyntaxError };

    if (tokenContainsHTMLSpace(token) || tokenContainsHTMLSpace(newToken))
        return Exception { InvalidCharacterError };

    auto& tokens = this->tokens();

    auto tokenIndex = tokens.find(token);
    if (tokenIndex == notFound)
        return false;

    replaceInOrderedSet(tokens, tokenIndex, newToken);
    ASSERT(token == newToken || tokens.find(token) == notFound);

    updateAssociatedAttributeFromTokens();

    return true;
}

// https://dom.spec.whatwg.org/#concept-domtokenlist-validation
ExceptionOr<bool> DOMTokenList::supports(StringView token)
{
    if (!m_isSupportedToken)
        return Exception { TypeError };
    return m_isSupportedToken(m_element.document(), token);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-value
const AtomString& DOMTokenList::value() const
{
    return m_element.getAttribute(m_attributeName);
}

void DOMTokenList::setValue(const String& value)
{
    m_element.setAttribute(m_attributeName, value);
}

void DOMTokenList::updateTokensFromAttributeValue(const String& value)
{
    // Clear tokens but not capacity.
    m_tokens.shrink(0);

    HashSet<AtomString> addedTokens;
    // https://dom.spec.whatwg.org/#ordered%20sets
    for (unsigned start = 0; ; ) {
        while (start < value.length() && isHTMLSpace(value[start]))
            ++start;
        if (start >= value.length())
            break;
        unsigned end = start + 1;
        while (end < value.length() && !isHTMLSpace(value[end]))
            ++end;

        AtomString token = value.substring(start, end - start);
        if (!addedTokens.contains(token)) {
            m_tokens.append(token);
            addedTokens.add(token);
        }

        start = end + 1;
    }

    m_tokens.shrinkToFit();
    m_tokensNeedUpdating = false;
}

void DOMTokenList::associatedAttributeValueChanged(const AtomString&)
{
    // Do not reset the DOMTokenList value if the attribute value was changed by us.
    if (m_inUpdateAssociatedAttributeFromTokens)
        return;

    m_tokensNeedUpdating = true;
}

// https://dom.spec.whatwg.org/#concept-dtl-update
void DOMTokenList::updateAssociatedAttributeFromTokens()
{
    ASSERT(!m_tokensNeedUpdating);

    if (m_tokens.isEmpty() && !m_element.hasAttribute(m_attributeName))
        return;

    // https://dom.spec.whatwg.org/#concept-ordered-set-serializer
    StringBuilder builder;
    for (auto& token : tokens()) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append(token);
    }
    AtomString serializedValue = builder.toAtomString();

    SetForScope<bool> inAttributeUpdate(m_inUpdateAssociatedAttributeFromTokens, true);
    m_element.setAttribute(m_attributeName, serializedValue);
}

Vector<AtomString>& DOMTokenList::tokens()
{
    if (m_tokensNeedUpdating)
        updateTokensFromAttributeValue(m_element.getAttribute(m_attributeName));
    ASSERT(!m_tokensNeedUpdating);
    return m_tokens;
}

} // namespace WebCore
