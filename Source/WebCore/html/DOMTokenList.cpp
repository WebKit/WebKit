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

#include "ExceptionCode.h"
#include "HTMLParserIdioms.h"
#include "SpaceSplitString.h"
#include <wtf/HashSet.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

DOMTokenList::DOMTokenList(Element& element, const QualifiedName& attributeName)
    : m_element(element)
    , m_attributeName(attributeName)
{
}

bool DOMTokenList::validateToken(const String& token, ExceptionCode& ec)
{
    if (token.isEmpty()) {
        ec = SYNTAX_ERR;
        return false;
    }

    unsigned length = token.length();
    for (unsigned i = 0; i < length; ++i) {
        if (isHTMLSpace(token[i])) {
            ec = INVALID_CHARACTER_ERR;
            return false;
        }
    }

    return true;
}

bool DOMTokenList::validateTokens(const String* tokens, size_t length, ExceptionCode& ec)
{
    for (size_t i = 0; i < length; ++i) {
        if (!validateToken(tokens[i], ec))
            return false;
    }
    return true;
}

bool DOMTokenList::contains(const AtomicString& token) const
{
    return tokens().contains(token);
}

inline void DOMTokenList::addInternal(const String* newTokens, size_t length, ExceptionCode& ec)
{
    // This is usually called with a single token.
    Vector<AtomicString, 1> uniqueNewTokens;
    uniqueNewTokens.reserveInitialCapacity(length);

    auto& tokens = this->tokens();
    for (size_t i = 0; i < length; ++i) {
        if (!validateToken(newTokens[i], ec))
            return;
        if (!tokens.contains(newTokens[i]) && !uniqueNewTokens.contains(newTokens[i]))
            uniqueNewTokens.uncheckedAppend(newTokens[i]);
    }

    if (!uniqueNewTokens.isEmpty())
        tokens.appendVector(uniqueNewTokens);

    updateAssociatedAttributeFromTokens();
}

void DOMTokenList::add(const Vector<String>& tokens, ExceptionCode& ec)
{
    addInternal(tokens.data(), tokens.size(), ec);
}

void DOMTokenList::add(const WTF::AtomicString& token, ExceptionCode& ec)
{
    addInternal(&token.string(), 1, ec);
}

inline void DOMTokenList::removeInternal(const String* tokensToRemove, size_t length, ExceptionCode& ec)
{
    if (!validateTokens(tokensToRemove, length, ec))
        return;

    auto& tokens = this->tokens();
    for (size_t i = 0; i < length; ++i)
        tokens.removeFirst(tokensToRemove[i]);

    updateAssociatedAttributeFromTokens();
}

void DOMTokenList::remove(const Vector<String>& tokens, ExceptionCode& ec)
{
    removeInternal(tokens.data(), tokens.size(), ec);
}

void DOMTokenList::remove(const WTF::AtomicString& token, ExceptionCode& ec)
{
    removeInternal(&token.string(), 1, ec);
}

bool DOMTokenList::toggle(const AtomicString& token, Optional<bool> force, ExceptionCode& ec)
{
    if (!validateToken(token, ec))
        return false;

    auto& tokens = this->tokens();

    if (tokens.contains(token)) {
        if (!force.valueOr(false)) {
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

const AtomicString& DOMTokenList::value() const
{
    if (m_cachedValue.isNull()) {
        // https://dom.spec.whatwg.org/#concept-ordered-set-serializer
        StringBuilder builder;
        for (auto& token : tokens()) {
            if (!builder.isEmpty())
                builder.append(' ');
            builder.append(token);
        }
        m_cachedValue = builder.toAtomicString();
        ASSERT(!m_cachedValue.isNull());
    }
    ASSERT(!m_tokensNeedUpdating);
    return m_cachedValue;
}

void DOMTokenList::setValue(const String& value)
{
    updateTokensFromAttributeValue(value);
    updateAssociatedAttributeFromTokens();
}

void DOMTokenList::updateTokensFromAttributeValue(const String& value)
{
    // Clear tokens but not capacity.
    m_tokens.shrink(0);

    HashSet<AtomicString> addedTokens;
    // https://dom.spec.whatwg.org/#ordered%20sets
    for (unsigned start = 0; ; ) {
        while (start < value.length() && isHTMLSpace(value[start]))
            ++start;
        if (start >= value.length())
            break;
        unsigned end = start + 1;
        while (end < value.length() && !isHTMLSpace(value[end]))
            ++end;

        AtomicString token = value.substring(start, end - start);
        if (!addedTokens.contains(token)) {
            m_tokens.append(token);
            addedTokens.add(token);
        }

        start = end + 1;
    }

    m_tokens.shrinkToFit();
    m_tokensNeedUpdating = false;
    m_cachedValue = nullAtom;
}

void DOMTokenList::associatedAttributeValueChanged(const AtomicString&)
{
    // Do not reset the DOMTokenList value if the attribute value was changed by us.
    if (m_inUpdateAssociatedAttributeFromTokens)
        return;

    m_tokensNeedUpdating = true;
    m_cachedValue = nullAtom;
}

// https://dom.spec.whatwg.org/#concept-dtl-update
void DOMTokenList::updateAssociatedAttributeFromTokens()
{
    ASSERT(!m_tokensNeedUpdating);

    m_cachedValue = nullAtom;

    TemporaryChange<bool> inAttributeUpdate(m_inUpdateAssociatedAttributeFromTokens, true);
    m_element.setAttributeWithoutSynchronization(m_attributeName, value());
    ASSERT_WITH_MESSAGE(m_cachedValue, "Calling value() should have cached its results");
}

Vector<AtomicString>& DOMTokenList::tokens()
{
    if (m_tokensNeedUpdating)
        updateTokensFromAttributeValue(m_element.fastGetAttribute(m_attributeName));
    ASSERT(!m_tokensNeedUpdating);
    return m_tokens;
}

} // namespace WebCore
