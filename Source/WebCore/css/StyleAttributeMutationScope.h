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

#pragma once

#include "CustomElementReactionQueue.h"
#include "Element.h"
#include "HTMLNames.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"

namespace WebCore {

class StyleAttributeMutationScope {
    WTF_MAKE_NONCOPYABLE(StyleAttributeMutationScope);
public:
    StyleAttributeMutationScope(Element* element)
        : m_element(element)
    {
        ++s_scopeCount;

        if (s_scopeCount != 1) {
            ASSERT(s_currentScope->m_element == element);
            return;
        }

        ASSERT(!s_currentScope);
        s_currentScope = this;

        if (!m_element)
            return;

        bool shouldReadOldValue = false;

        m_mutationRecipients = MutationObserverInterestGroup::createForAttributesMutation(*m_element, HTMLNames::styleAttr);
        if (m_mutationRecipients && m_mutationRecipients->isOldValueRequested())
            shouldReadOldValue = true;

        if (UNLIKELY(m_element->isDefinedCustomElement())) {
            auto* reactionQueue = m_element->reactionQueue();
            if (reactionQueue && reactionQueue->observesStyleAttribute()) {
                m_isCustomElement = true;
                shouldReadOldValue = true;
            }
        }

        if (shouldReadOldValue)
            m_oldValue = m_element->getAttribute(HTMLNames::styleAttr);
    }

    ~StyleAttributeMutationScope()
    {
        --s_scopeCount;
        if (s_scopeCount)
            return;
        ASSERT(s_currentScope == this);
        s_currentScope = nullptr;

        if (!m_shouldDeliver || !m_element)
            return;

        if (m_mutationRecipients) {
            auto mutation = MutationRecord::createAttributes(*m_element, HTMLNames::styleAttr, m_oldValue);
            m_mutationRecipients->enqueueMutationRecord(WTFMove(mutation));
        }

        if (m_isCustomElement) {
            auto& newValue = m_element->getAttribute(HTMLNames::styleAttr);
            CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(*m_element, HTMLNames::styleAttr, m_oldValue, newValue);
        }
    }

    void enqueueMutationRecord()
    {
        m_shouldDeliver = true;
    }

private:
    static unsigned s_scopeCount;
    static StyleAttributeMutationScope* s_currentScope;

    std::unique_ptr<MutationObserverInterestGroup> m_mutationRecipients;
    AtomString m_oldValue;
    RefPtr<Element> m_element;
    bool m_isCustomElement { false };
    bool m_shouldDeliver { false };
};

} // namespace WebCore
