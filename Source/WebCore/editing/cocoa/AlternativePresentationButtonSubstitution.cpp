/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "AlternativePresentationButtonSubstitution.h"

#if ENABLE(ALTERNATIVE_PRESENTATION_BUTTON_ELEMENT)

#include "AlternativePresentationButtonElement.h"
#include "HTMLInputElement.h"
#include "InputTypeNames.h"
#include "ShadowRoot.h"
#include "StyleProperties.h"

namespace WebCore {

AlternativePresentationButtonSubstitution::AlternativePresentationButtonSubstitution(HTMLInputElement& element, Vector<Ref<Element>>&& elementsToHide)
    : m_shadowHost { makeRef(element) }
{
    initializeSavedDisplayStyles(WTFMove(elementsToHide));
}

AlternativePresentationButtonSubstitution::AlternativePresentationButtonSubstitution(Element& element, Vector<Ref<Element>>&& elementsToHide)
    : m_shadowHost { makeRef(element) }
    , m_alternativePresentationButtonElement { AlternativePresentationButtonElement::create(element.document()) }
{
    initializeSavedDisplayStyles(WTFMove(elementsToHide));
}

void AlternativePresentationButtonSubstitution::initializeSavedDisplayStyles(Vector<Ref<Element>>&& elements)
{
    m_savedDisplayStyles.reserveInitialCapacity(elements.size());
    for (auto& element : elements) {
        if (is<StyledElement>(element))
            m_savedDisplayStyles.uncheckedAppend({ adoptRef(downcast<StyledElement>(element.leakRef())) });
    }
}

void AlternativePresentationButtonSubstitution::apply()
{
    auto saveStyles = [&] {
        for (auto& savedDisplayStyle : m_savedDisplayStyles) {
            auto* styleProperties = savedDisplayStyle.element->inlineStyle();
            if (!styleProperties)
                continue;
            savedDisplayStyle.value = styleProperties->getPropertyValue(CSSPropertyDisplay);
            savedDisplayStyle.important = styleProperties->propertyIsImportant(CSSPropertyDisplay);
            savedDisplayStyle.wasSpecified = true;
        }
    };
    auto attachShadowRoot = [&] {
        if (is<HTMLInputElement>(m_shadowHost)) {
            m_savedShadowHostInputType = m_shadowHost->attributeWithoutSynchronization(HTMLNames::typeAttr);
            auto& inputElement = downcast<HTMLInputElement>(m_shadowHost.get());
            ASSERT(inputElement.type() != InputTypeNames::alternativePresentationButton());
            inputElement.setTypeWithoutUpdatingAttribute(InputTypeNames::alternativePresentationButton());
            return;
        }
        ASSERT(m_alternativePresentationButtonElement);
        ASSERT(!m_shadowHost->shadowRoot());
        ASSERT(!m_shadowHost->userAgentShadowRoot());
        m_shadowHost->ensureUserAgentShadowRoot().appendChild(*m_alternativePresentationButtonElement);
    };
    auto hideElements = [&] {
        for (auto& savedDisplayStyle : m_savedDisplayStyles)
            savedDisplayStyle.element->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true); // important
    };

    ASSERT(!m_isApplied);
    m_isApplied = true;
    saveStyles();
    attachShadowRoot();
    hideElements();
}

void AlternativePresentationButtonSubstitution::unapply()
{
    auto detachShadowRoot = [&] {
        if (is<HTMLInputElement>(m_shadowHost)) {
            auto& inputElement = downcast<HTMLInputElement>(m_shadowHost.get());
            if (inputElement.type() == InputTypeNames::alternativePresentationButton())
                inputElement.setTypeWithoutUpdatingAttribute(m_savedShadowHostInputType);
            return;
        }
        ASSERT(m_alternativePresentationButtonElement);
        ASSERT(m_shadowHost->userAgentShadowRoot());
        ASSERT(m_shadowHost->userAgentShadowRoot()->countChildNodes() == 1);
        m_shadowHost->userAgentShadowRoot()->removeChild(*m_alternativePresentationButtonElement);
        ASSERT(!m_shadowHost->userAgentShadowRoot()->countChildNodes());
        m_shadowHost->removeShadowRoot();
    };
    auto restoreStyles = [&] {
        for (auto& savedDisplayStyle : m_savedDisplayStyles) {
            if (savedDisplayStyle.wasSpecified)
                savedDisplayStyle.element->setInlineStyleProperty(CSSPropertyDisplay, savedDisplayStyle.value, savedDisplayStyle.important);
            else
                savedDisplayStyle.element->removeInlineStyleProperty(CSSPropertyDisplay);
        }
    };

    ASSERT(m_isApplied);
    m_isApplied = false;
    restoreStyles();
    detachShadowRoot();
}

Vector<Ref<Element>> AlternativePresentationButtonSubstitution::replacedElements()
{
    Vector<Ref<Element>> result;
    result.reserveInitialCapacity(m_savedDisplayStyles.size() + 1);
    result.uncheckedAppend(m_shadowHost.copyRef());
    for (auto& savedDisplayStyle : m_savedDisplayStyles)
        result.uncheckedAppend(savedDisplayStyle.element.copyRef());
    return result;
}

} // namespace WebCore

#endif // ENABLE(ALTERNATIVE_PRESENTATION_BUTTON_ELEMENT)
