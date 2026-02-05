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

#include "DocumentInlines.h"
#include "ContainerNodeInlines.h"
#include "CSSFontSelector.h"
#include "ElementInlines.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "NodeRenderStyle.h"
#include "RenderSearchField.h"
#include "RenderStyle+GettersInlines.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "StylePreferredSize.h"
#include "TextControlInnerElements.h"
#include "UserAgentParts.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SearchInputType);

using namespace HTMLNames;

SearchInputType::SearchInputType(HTMLInputElement& element)
    : BaseTextInputType(Type::Search, element)
{
    ASSERT(needsShadowSubtree());
}

// PopupMenuClient methods
void SearchInputType::valueChanged(unsigned listIndex, bool fireEvents)
{
    ASSERT(static_cast<int>(listIndex) < listSize());
    RefPtr inputElement = element();
    if (static_cast<int>(listIndex) == (listSize() - 1)) {
        if (fireEvents) {
            m_recentSearches.clear();
            const AtomString& name = inputElement->attributeWithoutSynchronization(nameAttr);
            if (!name.isEmpty()) {
                if (CheckedPtr renderer = dynamicDowncast<RenderSearchField>(inputElement->renderer()))
                    renderer->updatePopup(name, m_recentSearches);
            }
        }
    } else {
        inputElement->setValue(itemText(listIndex));
        inputElement->select();
    }
}

String SearchInputType::itemText(unsigned listIndex) const
{
#if !PLATFORM(IOS_FAMILY)
    int size = listSize();
    if (size == 1) {
        ASSERT(!listIndex);
        return searchMenuNoRecentSearchesText();
    }
    if (!listIndex)
        return searchMenuRecentSearchesText();
#endif
    if (itemIsSeparator(listIndex))
        return String();
#if !PLATFORM(IOS_FAMILY)
    if (static_cast<int>(listIndex) == (size - 1))
        return searchMenuClearRecentSearchesText();
#endif
    return m_recentSearches[listIndex - 1].string;
}

bool SearchInputType::itemIsEnabled(unsigned listIndex) const
{
    if (!listIndex || itemIsSeparator(listIndex))
        return false;
    return true;
}

PopupMenuStyle SearchInputType::itemStyle(unsigned) const
{
    return menuStyle();
}

PopupMenuStyle SearchInputType::menuStyle() const
{
    auto defaultStyle = RenderStyle::create();
    CheckedPtr renderer = dynamicDowncast<RenderSearchField>(protectedElement()->renderer());
    CheckedRef style = renderer ? renderer->style() : defaultStyle;
    return PopupMenuStyle(
        style->visitedDependentColorApplyingColorFilter(),
        style->visitedDependentBackgroundColorApplyingColorFilter(),
        style->fontCascade(),
        nullString(),
        style->usedVisibility() == Visibility::Visible,
        style->display() == DisplayType::None,
        true,
        style->writingMode().bidiDirection(),
        isOverride(style->unicodeBidi()),
        PopupMenuStyle::CustomBackgroundColor
    );
}

#if PLATFORM(WIN)
int SearchInputType::clientInsetLeft() const
{
    // Inset the menu by the radius of the cap on the left so that
    // it only runs along the straight part of the bezel.
    return height() / 2;
}

int SearchInputType::clientInsetRight() const
{
    // Inset the menu by the radius of the cap on the right so that
    // it only runs along the straight part of the bezel (unless it needs
    // to be wider).
    return height() / 2;
}

LayoutUnit SearchInputType::clientPaddingLeft() const
{
    CheckedPtr renderer = dynamicDowncast<RenderSearchField>(protectedElement()->renderer());
    return renderer ? renderer->clientPaddingLeft() : 0_lu;
}

LayoutUnit SearchInputType::clientPaddingRight() const
{
    CheckedPtr renderer = dynamicDowncast<RenderSearchField>(protectedElement()->renderer());
    return renderer ? renderer->clientPaddingRight() : 0_lu;
}

FontSelector* SearchInputType::fontSelector() const
{
    return &protect(protectedElement()->document())->fontSelector();
}

HostWindow* SearchInputType::hostWindow() const
{
    if (CheckedPtr renderer = dynamicDowncast<RenderSearchField>(protectedElement()->renderer()))
        return renderer->hostWindow();
    return nullptr;
}
#endif

int SearchInputType::listSize() const
{
    // If there are no recent searches, then our menu will have 1 "No recent searches" item.
    if (!m_recentSearches.size())
        return 1;
    // Otherwise, leave room in the menu for a header, a separator, and the "Clear recent searches" item.
    return m_recentSearches.size() + 3;
}

void SearchInputType::popupDidHide()
{
    if (CheckedPtr renderer = dynamicDowncast<RenderSearchField>(protectedElement()->renderer()))
        renderer->popupDidHide();
}

bool SearchInputType::itemIsSeparator(unsigned listIndex) const
{
    // The separator will be the second to last item in our list.
    return static_cast<int>(listIndex) == (listSize() - 2);
}

bool SearchInputType::itemIsLabel(unsigned listIndex) const
{
    return !listIndex;
}

bool SearchInputType::itemIsSelected(unsigned) const
{
    return false;
}

#if !PLATFORM(COCOA)
void SearchInputType::setTextFromItem(unsigned listIndex)
{
    protectedElement()->setValue(itemText(listIndex));
}
#endif

void SearchInputType::addSearchResult()
{
#if !PLATFORM(IOS_FAMILY)
    RefPtr inputElement = element();
    if (inputElement->maxResults() <= 0)
        return;

    String value = inputElement->value();
    if (value.isEmpty())
        return;

    CheckedPtr renderer = dynamicDowncast<RenderSearchField>(inputElement->renderer());
    if (renderer && renderer->page().usesEphemeralSession())
        return;

    m_recentSearches.removeAllMatching([value] (const RecentSearch& recentSearch) {
        return recentSearch.string == value;
    });

    RecentSearch recentSearch = { value, WallTime::now() };
    m_recentSearches.insert(0, recentSearch);
    while (static_cast<int>(m_recentSearches.size()) > inputElement->maxResults())
        m_recentSearches.removeLast();

    const AtomString& name = inputElement->attributeWithoutSynchronization(nameAttr);
    if (CheckedPtr renderer = dynamicDowncast<RenderSearchField>(inputElement->renderer()))
        renderer->updatePopup(name, m_recentSearches);
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
        if (RefPtr resultsButton = m_resultsButton) {
            if (RefPtr input = element())
                updateResultButtonPseudoType(*resultsButton, input->maxResults());
        }
    }
    BaseTextInputType::attributeChanged(name);
}

RenderPtr<RenderElement> SearchInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    // FIXME: https://github.com/llvm/llvm-project/pull/142471 Moving style is not unsafe.
    SUPPRESS_UNCOUNTED_ARG return createRenderer<RenderSearchField>(*protectedElement(), WTF::move(style));
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
    ASSERT(element());

    TextFieldInputType::createShadowSubtree();
    Ref document = element()->document();
    RefPtr container = containerElement();
    RefPtr textWrapper = innerBlockElement();
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { *container };
    ASSERT(container);
    ASSERT(textWrapper);

    Ref resultsButton = SearchFieldResultsButtonElement::create(document);
    container->insertBefore(resultsButton, textWrapper.copyRef());
    updateResultButtonPseudoType(resultsButton, element()->maxResults());
    m_resultsButton = WTF::move(resultsButton);

    Ref cancelButton = SearchFieldCancelButtonElement::create(document);
    container->insertBefore(cancelButton, protect(textWrapper->nextSibling()));
    m_cancelButton = WTF::move(cancelButton);
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
    Ref element = *this->element();
    if (!element->isMutable())
        return TextFieldInputType::handleKeydownEvent(event);

    const String& key = event.keyIdentifier();
    if (key == "U+001B"_s) {
        element->setValue(emptyString(), DispatchChangeEvent);
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

void SearchInputType::didSetValueByUserEdit()
{
    ASSERT(element());
    if (m_cancelButton) {
        if (CheckedPtr renderer = dynamicDowncast<RenderSearchField>(element()->renderer()))
            renderer->updateCancelButtonVisibility();
    }

    TextFieldInputType::didSetValueByUserEdit();
}

bool SearchInputType::sizeShouldIncludeDecoration(int, int& preferredSize) const
{
    ASSERT(element());
    Ref element = *this->element();
    preferredSize = element->size();
    // https://html.spec.whatwg.org/multipage/input.html#the-size-attribute
    // If the attribute is present, then its value must be parsed using the rules for parsing non-negative integers, and if the
    // result is a number greater than zero, then the user agent should ensure that at least that many characters are visible.
    if (!element->hasAttributeWithoutSynchronization(sizeAttr))
        return false;
    if (auto parsedSize = parseHTMLNonNegativeInteger(element->attributeWithoutSynchronization(sizeAttr)))
        return static_cast<int>(parsedSize.value()) == preferredSize;
    return false;
}

float SearchInputType::decorationWidth(float) const
{
    float width = 0;
    if (RefPtr resultsButton = m_resultsButton; resultsButton && resultsButton->renderStyle()) {
        // FIXME: Document what invariant holds to allow only using fixed logical widths?
        CheckedPtr renderStyle = resultsButton->renderStyle();
        if (auto fixedLogicalWidth = renderStyle->logicalWidth().tryFixed())
            width += fixedLogicalWidth->resolveZoom(renderStyle->usedZoomForLength());
    }
    if (RefPtr cancelButton = m_cancelButton; cancelButton && cancelButton->renderStyle()) {
        // FIXME: Document what invariant holds to allow only using fixed logical widths?
        CheckedPtr renderStyle = cancelButton->renderStyle();
        if (auto fixedLogicalWidth = renderStyle->logicalWidth().tryFixed())
            width += fixedLogicalWidth->resolveZoom(renderStyle->usedZoomForLength());
    }
    return width;
}

void SearchInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    bool emptinessChanged = valueChanged && sanitizedValue.isEmpty() != protectedElement()->value()->isEmpty();

    BaseTextInputType::setValue(sanitizedValue, valueChanged, eventBehavior, selection);

    if (!emptinessChanged)
        return;

    if (RefPtr cancelButton = m_cancelButton)
        cancelButton->invalidateStyleInternal();
}

} // namespace WebCore
