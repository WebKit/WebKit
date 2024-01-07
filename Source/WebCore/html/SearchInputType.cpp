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

#include "ElementInlines.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "NodeRenderStyle.h"
#include "RenderSearchField.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "TextControlInnerElements.h"
#include "UserAgentParts.h"

namespace WebCore {

using namespace HTMLNames;

SearchInputType::SearchInputType(HTMLInputElement& element)
    : BaseTextInputType(Type::Search, element)
    , m_searchEventTimer(*this, &SearchInputType::searchEventTimerFired)
{
    ASSERT(needsShadowSubtree());
}

void SearchInputType::addSearchResult()
{
#if !PLATFORM(IOS_FAMILY)
    // Normally we've got the correct renderer by the time we get here. However when the input type changes
    // we don't update the associated renderers until after the next tree update, so we could actually end up here
    // with a mismatched renderer (e.g. through form submission).
    ASSERT(element());
    if (CheckedPtr renderer = dynamicDowncast<RenderSearchField>(element()->renderer()))
        renderer->addSearchResult();
#endif
}

static void updateResultButtonPseudoType(SearchFieldResultsButtonElement& resultButton, int maxResults)
{
    if (!maxResults)
        resultButton.setUserAgentPart(UserAgentParts::webkitSearchResultsDecoration());
    else if (maxResults < 0)
        resultButton.setUserAgentPart(UserAgentParts::webkitSearchDecoration());
    else
        resultButton.setUserAgentPart(UserAgentParts::webkitSearchResultsButton());
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

const AtomString& SearchInputType::formControlType() const
{
    return InputTypeNames::search();
}

bool SearchInputType::needsContainer() const
{
    return true;
}

void SearchInputType::createShadowSubtree()
{
    ASSERT(needsShadowSubtree());
    ASSERT(!m_resultsButton);
    ASSERT(!m_cancelButton);

    TextFieldInputType::createShadowSubtree();
    RefPtr<HTMLElement> container = containerElement();
    RefPtr<HTMLElement> textWrapper = innerBlockElement();
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { *container };
    ASSERT(container);
    ASSERT(textWrapper);

    ASSERT(element());
    m_resultsButton = SearchFieldResultsButtonElement::create(element()->document());
    container->insertBefore(*m_resultsButton, textWrapper.copyRef());
    updateResultButtonPseudoType(*m_resultsButton, element()->maxResults());

    m_cancelButton = SearchFieldCancelButtonElement::create(element()->document());
    container->insertBefore(*m_cancelButton, textWrapper->protectedNextSibling());
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
    if (!element()->isMutable())
        return TextFieldInputType::handleKeydownEvent(event);

    const String& key = event.keyIdentifier();
    if (key == "U+001B"_s) {
        Ref<HTMLInputElement> protectedInputElement(*element());
        protectedInputElement->setValue(emptyString(), DispatchChangeEvent);
        if (protectedInputElement->document().settings().searchInputIncrementalAttributeAndSearchEventEnabled())
            protectedInputElement->onSearch();
        event.setDefaultHandled();
        return ShouldCallBaseEventHandler::Yes;
    }
    return TextFieldInputType::handleKeydownEvent(event);
}

void SearchInputType::removeShadowSubtree()
{
    TextFieldInputType::removeShadowSubtree();
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
    return element()->document().settings().searchInputIncrementalAttributeAndSearchEventEnabled()
        && element()->hasAttributeWithoutSynchronization(incrementalAttr);
}

void SearchInputType::didSetValueByUserEdit()
{
    ASSERT(element());
    if (m_cancelButton) {
        if (CheckedPtr renderer = dynamicDowncast<RenderSearchField>(element()->renderer()))
            renderer->updateCancelButtonVisibility();
    }
    // If the incremental attribute is set, then dispatch the search event
    if (searchEventsShouldBeDispatched())
        startSearchEventTimer();

    TextFieldInputType::didSetValueByUserEdit();
}

bool SearchInputType::sizeShouldIncludeDecoration(int, int& preferredSize) const
{
    ASSERT(element());
    preferredSize = element()->size();
    // https://html.spec.whatwg.org/multipage/input.html#the-size-attribute
    // If the attribute is present, then its value must be parsed using the rules for parsing non-negative integers, and if the
    // result is a number greater than zero, then the user agent should ensure that at least that many characters are visible.
    if (!element()->hasAttributeWithoutSynchronization(sizeAttr))
        return false;
    if (auto parsedSize = parseHTMLNonNegativeInteger(element()->attributeWithoutSynchronization(sizeAttr)))
        return static_cast<int>(parsedSize.value()) == preferredSize;
    return false;
}

float SearchInputType::decorationWidth() const
{
    float width = 0;
    if (m_resultsButton && m_resultsButton->renderStyle())
        width += m_resultsButton->renderStyle()->logicalWidth().value();
    if (m_cancelButton && m_cancelButton->renderStyle())
        width += m_cancelButton->renderStyle()->logicalWidth().value();
    return width;
}

void SearchInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    bool emptinessChanged = valueChanged && sanitizedValue.isEmpty() != element()->value().isEmpty();

    BaseTextInputType::setValue(sanitizedValue, valueChanged, eventBehavior, selection);

    if (m_cancelButton && emptinessChanged)
        m_cancelButton->invalidateStyleInternal();
}

} // namespace WebCore
