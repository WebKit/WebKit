/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SearchInputType.h"

#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "RenderSearchField.h"
#include "ShadowRoot.h"
#include "TextControlInnerElements.h"

namespace WebCore {

using namespace HTMLNames;

SearchInputType::SearchInputType(HTMLInputElement& element)
    : BaseTextInputType(element)
    , m_searchEventTimer(*this, &SearchInputType::searchEventTimerFired)
{
}

void SearchInputType::addSearchResult()
{
#if !PLATFORM(IOS_FAMILY)
    // Normally we've got the correct renderer by the time we get here. However when the input type changes
    // we don't update the associated renderers until after the next tree update, so we could actually end up here
    // with a mismatched renderer (e.g. through form submission).
    ASSERT(element());
    if (is<RenderSearchField>(element()->renderer()))
        downcast<RenderSearchField>(*element()->renderer()).addSearchResult();
#endif
}

static void updateResultButtonPseudoType(SearchFieldResultsButtonElement& resultButton, int maxResults)
{
    if (!maxResults)
        resultButton.setPseudo(AtomicString("-webkit-search-results-decoration", AtomicString::ConstructFromLiteral));
    else if (maxResults < 0)
        resultButton.setPseudo(AtomicString("-webkit-search-decoration", AtomicString::ConstructFromLiteral));
    else
        resultButton.setPseudo(AtomicString("-webkit-search-results-button", AtomicString::ConstructFromLiteral));
}

void SearchInputType::attributeChanged(const QualifiedName& name)
{
    if (name == resultsAttr) {
        if (m_resultsButton) {
            if (auto* element = this->element())
                updateResultButtonPseudoType(*m_resultsButton, element->maxResults());
        }
    }
    BaseTextInputType::attributeChanged(name);
}

RenderPtr<RenderElement> SearchInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    return createRenderer<RenderSearchField>(*element(), WTFMove(style));
}

const AtomicString& SearchInputType::formControlType() const
{
    return InputTypeNames::search();
}

bool SearchInputType::isSearchField() const
{
    return true;
}

bool SearchInputType::needsContainer() const
{
    return true;
}

void SearchInputType::createShadowSubtree()
{
    ASSERT(!m_resultsButton);
    ASSERT(!m_cancelButton);

    TextFieldInputType::createShadowSubtree();
    RefPtr<HTMLElement> container = containerElement();
    RefPtr<HTMLElement> textWrapper = innerBlockElement();
    ASSERT(container);
    ASSERT(textWrapper);

    ASSERT(element());
    m_resultsButton = SearchFieldResultsButtonElement::create(element()->document());
    updateResultButtonPseudoType(*m_resultsButton, element()->maxResults());
    container->insertBefore(*m_resultsButton, textWrapper.get());

    m_cancelButton = SearchFieldCancelButtonElement::create(element()->document());
    container->insertBefore(*m_cancelButton, textWrapper->nextSibling());
}

HTMLElement* SearchInputType::resultsButtonElement() const
{
    return m_resultsButton.get();
}

HTMLElement* SearchInputType::cancelButtonElement() const
{
    return m_cancelButton.get();
}

auto SearchInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    ASSERT(element());
    if (element()->isDisabledOrReadOnly())
        return TextFieldInputType::handleKeydownEvent(event);

    const String& key = event.keyIdentifier();
    if (key == "U+001B") {
        Ref<HTMLInputElement> protectedInputElement(*element());
        protectedInputElement->setValueForUser(emptyString());
        protectedInputElement->onSearch();
        event.setDefaultHandled();
        return ShouldCallBaseEventHandler::Yes;
    }
    return TextFieldInputType::handleKeydownEvent(event);
}

void SearchInputType::destroyShadowSubtree()
{
    TextFieldInputType::destroyShadowSubtree();
    m_resultsButton = nullptr;
    m_cancelButton = nullptr;
}

void SearchInputType::startSearchEventTimer()
{
    ASSERT(element());
    ASSERT(element()->renderer());
    unsigned length = element()->innerTextValue().length();

    if (!length) {
        m_searchEventTimer.startOneShot(0_ms);
        return;
    }

    // After typing the first key, we wait 0.5 seconds.
    // After the second key, 0.4 seconds, then 0.3, then 0.2 from then on.
    m_searchEventTimer.startOneShot(std::max(200_ms, 600_ms - 100_ms * length));
}

void SearchInputType::stopSearchEventTimer()
{
    m_searchEventTimer.stop();
}

void SearchInputType::searchEventTimerFired()
{
    ASSERT(element());
    element()->onSearch();
}

bool SearchInputType::searchEventsShouldBeDispatched() const
{
    ASSERT(element());
    return element()->hasAttributeWithoutSynchronization(incrementalAttr);
}

void SearchInputType::didSetValueByUserEdit()
{
    ASSERT(element());
    if (m_cancelButton && is<RenderSearchField>(element()->renderer()))
        downcast<RenderSearchField>(*element()->renderer()).updateCancelButtonVisibility();
    // If the incremental attribute is set, then dispatch the search event
    if (searchEventsShouldBeDispatched())
        startSearchEventTimer();

    TextFieldInputType::didSetValueByUserEdit();
}

bool SearchInputType::sizeShouldIncludeDecoration(int, int& preferredSize) const
{
    ASSERT(element());
    preferredSize = element()->size();
    return true;
}

float SearchInputType::decorationWidth() const
{
    float width = 0;
    if (m_resultsButton)
        width += m_resultsButton->computedStyle()->logicalWidth().value();
    if (m_cancelButton)
        width += m_cancelButton->computedStyle()->logicalWidth().value();
    return width;
}

} // namespace WebCore
