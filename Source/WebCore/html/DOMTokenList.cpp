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

#include "ExceptionOr.h"
#include "NodeDocument.h"
#include "SpaceSplitString.h"
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DOMTokenList);

DOMTokenList::DOMTokenList(Element& element, const QualifiedName& attributeName, IsSupportedTokenFunction&& isSupportedToken)
    : m_element(element)
    , m_attributeName(attributeName)
    , m_isSupportedToken(WTF::move(isSupportedToken))
{
}

static inline bool tokenContainsHTMLSpace(StringView token)
{
    return token.find(isASCIIWhitespace<char16_t>) != notFound;
}

ExceptionOr<void> DOMTokenList::validateToken(StringView token)
{
    if (token.isEmpty())
        return Exception { ExceptionCode::SyntaxError };

    if (tokenContainsHTMLSpace(token))
        return Exception { ExceptionCode::InvalidCharacterError };

    return { };
}

ExceptionOr<void> DOMTokenList::validateTokens(std::span<const RefPtr<AtomStringImpl>> tokens)
{
    for (auto& token : tokens) {
        auto result = validateToken(token);
        if (result.hasException())
            return result;
    }
    return { };
}

bool DOMTokenList::contains(AtomStringImpl* token) const
{
    return tokens().contains(token);
}

inline ExceptionOr<void> DOMTokenList::addInternal(std::span<const RefPtr<AtomStringImpl>> newTokens)
{
    // This is usually called with a single token.
    Vector<RefPtr<AtomStringImpl>, 1> uniqueNewTokens(ReserveCapacityTag { }, newTokens.size());

    auto& tokens = this->tokens();

    for (auto& newToken : newTokens) {
        auto result = validateToken(newToken);
        if (result.hasException())
            return result;
        if (!tokens.contains(newToken) && !uniqueNewTokens.contains(newToken))
            uniqueNewTokens.append(newToken);
    }

    if (!uniqueNewTokens.isEmpty())
        tokens.appendVector(uniqueNewTokens);

    updateAssociatedAttributeFromTokens();

    return { };
}

ExceptionOr<void> DOMTokenList::add(const FixedVector<RefPtr<AtomStringImpl>>& tokens)
{
    return addInternal(tokens);
}

ExceptionOr<void> DOMTokenList::add(AtomStringImpl* token)
{
    RefPtr<AtomStringImpl> protectedToken { token };
    return addInternal(singleElementSpan(protectedToken));
}

inline ExceptionOr<void> DOMTokenList::removeInternal(std::span<const RefPtr<AtomStringImpl>> tokensToRemove)
{
    auto result = validateTokens(tokensToRemove);
    if (result.hasException())
        return result;

    auto& tokens = this->tokens();
    for (auto& tokenToRemove : tokensToRemove)
        tokens.removeFirst(tokenToRemove);

    updateAssociatedAttributeFromTokens();

    return { };
}

ExceptionOr<void> DOMTokenList::remove(const FixedVector<RefPtr<AtomStringImpl>>& tokens)
{
    return removeInternal(tokens);
}

ExceptionOr<void> DOMTokenList::remove(AtomStringImpl* token)
{
    RefPtr<AtomStringImpl> protectedToken { token };
    return removeInternal(singleElementSpan(protectedToken));
}

ExceptionOr<bool> DOMTokenList::toggle(AtomStringImpl* token, std::optional<bool> force)
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

static inline void replaceInOrderedSet(Vector<RefPtr<AtomStringImpl>, 1>& tokens, size_t tokenIndex, AtomStringImpl* newToken)
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
        tokens.removeAt(newTokenIndex);
    } else
        tokens.removeAt(tokenIndex);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-replace
ExceptionOr<bool> DOMTokenList::replace(AtomStringImpl* token, AtomStringImpl* newToken)
{
    if (isEmpty(token) || isEmpty(newToken))
        return Exception { ExceptionCode::SyntaxError };

    if (tokenContainsHTMLSpace(token) || tokenContainsHTMLSpace(newToken))
        return Exception { ExceptionCode::InvalidCharacterError };

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
        return Exception { ExceptionCode::TypeError };
    return m_isSupportedToken(protect(m_element->document()), token);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-value
const RefPtr<AtomStringImpl>& DOMTokenList::value() const
{
    return m_element->getAttribute(m_attributeName);
}

void DOMTokenList::setValue(AtomStringImpl* value)
{
    m_element->setAttribute(m_attributeName, value);
}

void DOMTokenList::updateTokensFromAttributeValue(AtomStringImpl* value)
{
    // Clear tokens but not capacity.
    m_tokens.shrink(0);

    HashSet<RefPtr<AtomStringImpl>> addedTokens;
    // https://dom.spec.whatwg.org/#ordered%20sets
    for (unsigned start = 0; ; ) {
        while (start < WTF::length(value) && isASCIIWhitespace(codeUnitAt(value, start)))
            ++start;
        if (start >= WTF::length(value))
            break;
        unsigned end = start + 1;
        while (end < WTF::length(value) && !isASCIIWhitespace(codeUnitAt(value, end)))
            ++end;
        bool wholeAttributeIsSingleToken = !start && end == WTF::length(value);
        if (wholeAttributeIsSingleToken) {
            m_tokens.append(value);
            break;
        }

        auto tokenView = StringView { value }.substring(start, end - start);
        if (!addedTokens.contains<StringViewHashTranslator>(tokenView)) {
            auto token = tokenView.toAtomString();
            m_tokens.append(token);
            addedTokens.add(WTF::move(token));
        }

        start = end + 1;
    }

    m_tokens.shrinkToFit();
    m_tokensNeedUpdating = false;
}

// https://dom.spec.whatwg.org/#concept-dtl-update
void DOMTokenList::updateAssociatedAttributeFromTokens()
{
    ASSERT(!m_tokensNeedUpdating);

    Ref element = m_element.get();
    if (m_tokens.isEmpty() && !element->hasAttribute(m_attributeName))
        return;

    if (m_tokens.isEmpty()) {
        element->setAttribute(m_attributeName, emptyAtom());
        return;
    }

    bool wholeAttributeIsSingleToken = m_tokens.size() == 1;
    if (wholeAttributeIsSingleToken) {
        SetForScope inAttributeUpdate(m_inUpdateAssociatedAttributeFromTokens, true);
        element->setAttribute(m_attributeName, m_tokens[0]);
        return;
    }

    // https://dom.spec.whatwg.org/#concept-ordered-set-serializer
    StringBuilder builder;
    for (auto& token : tokens()) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append(token);
    }
    RefPtr<AtomStringImpl> serializedValue = builder.toAtomString();

    SetForScope inAttributeUpdate(m_inUpdateAssociatedAttributeFromTokens, true);
    element->setAttribute(m_attributeName, serializedValue);
}

Vector<RefPtr<AtomStringImpl>, 1>& DOMTokenList::tokens()
{
    if (m_tokensNeedUpdating)
        updateTokensFromAttributeValue(m_element->getAttribute(m_attributeName));
    ASSERT(!m_tokensNeedUpdating);
    return m_tokens;
}

} // namespace WebCore
