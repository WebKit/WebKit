/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextControlInnerElements.h"

#include "CSSPrimitiveValue.h"
#include "CSSToLengthConversionData.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderSearchField.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ShadowPseudoIds.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "TextEvent.h"
#include "TextEventInputType.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextControlInnerContainer);
WTF_MAKE_ISO_ALLOCATED_IMPL(TextControlInnerElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(TextControlInnerTextElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(TextControlPlaceholderElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(SearchFieldResultsButtonElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(SearchFieldCancelButtonElement);

using namespace HTMLNames;

TextControlInnerContainer::TextControlInnerContainer(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<TextControlInnerContainer> TextControlInnerContainer::create(Document& document)
{
    return adoptRef(*new TextControlInnerContainer(document));
}
    
RenderPtr<RenderElement> TextControlInnerContainer::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderTextControlInnerContainer>(*this, WTFMove(style));
}

static inline bool isStrongPasswordTextField(const Element* element)
{
    return is<HTMLInputElement>(element) && downcast<HTMLInputElement>(element)->hasAutoFillStrongPasswordButton();
}

std::optional<Style::ElementStyle> TextControlInnerContainer::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle*)
{
    auto elementStyle = resolveStyle(resolutionContext);
    if (isStrongPasswordTextField(shadowHost())) {
        elementStyle.renderStyle->setFlexWrap(FlexWrap::Wrap);
        elementStyle.renderStyle->setOverflowX(Overflow::Hidden);
        elementStyle.renderStyle->setOverflowY(Overflow::Hidden);
    }
    return elementStyle;
}

TextControlInnerElement::TextControlInnerElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<TextControlInnerElement> TextControlInnerElement::create(Document& document)
{
    return adoptRef(*new TextControlInnerElement(document));
}

std::optional<Style::ElementStyle> TextControlInnerElement::resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle* shadowHostStyle)
{
    auto newStyle = RenderStyle::createPtr();
    newStyle->inheritFrom(*shadowHostStyle);
    newStyle->setFlexGrow(1);

    // Needed for correct shrinking.
    if (newStyle->isHorizontalWritingMode())
        newStyle->setMinWidth(Length { 0, LengthType::Fixed });
    else
        newStyle->setMinHeight(Length { 0, LengthType::Fixed });

    newStyle->setDisplay(DisplayType::Block);
    newStyle->setDirection(TextDirection::LTR);
    // We don't want the shadow DOM to be editable, so we set this block to read-only in case the input itself is editable.
    newStyle->setUserModify(UserModify::ReadOnly);

    if (isStrongPasswordTextField(shadowHost())) {
        newStyle->setFlexShrink(0);
        newStyle->setTextOverflow(TextOverflow::Clip);
        newStyle->setOverflowX(Overflow::Hidden);
        newStyle->setOverflowY(Overflow::Hidden);

        // Set "flex-basis: 1em". Note that CSSPrimitiveValue::computeLengthInt() only needs the element's
        // style to calculate em lengths. Since the element might not be in a document, just pass nullptr
        // for the root element style, the parent element style, and the render view.
        auto emSize = CSSPrimitiveValue::create(1, CSSUnitType::CSS_EMS);
        int pixels = emSize->computeLength<int>(CSSToLengthConversionData { *newStyle, nullptr, nullptr, nullptr });
        newStyle->setFlexBasis(Length { pixels, LengthType::Fixed });
    }

    return { WTFMove(newStyle) };
}

// MARK: TextControlInnerTextElement

inline TextControlInnerTextElement::TextControlInnerTextElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<TextControlInnerTextElement> TextControlInnerTextElement::create(Document& document, bool isEditable)
{
    auto result = adoptRef(*new TextControlInnerTextElement(document));
    constexpr bool initialization = true;
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { result };
    result->updateInnerTextElementEditabilityImpl(isEditable, initialization);
    return result;
}

void TextControlInnerTextElement::updateInnerTextElementEditabilityImpl(bool isEditable, bool initialization)
{
    const auto& value = isEditable ? plaintextOnlyAtom() : falseAtom();
    if (initialization) {
        Vector<Attribute> attributes { Attribute(contenteditableAttr, value) };
        parserSetAttributes(attributes);
    } else
        setAttributeWithoutSynchronization(contenteditableAttr, value);
}

void TextControlInnerTextElement::defaultEventHandler(Event& event)
{
    // FIXME: In the future, we should add a way to have default event listeners.
    // Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    // Or possibly we could just use a normal event listener.
    if (event.isBeforeTextInsertedEvent()) {
        // A TextControlInnerTextElement can have no host if its been detached,
        // but kept alive by an EditCommand. In this case, an undo/redo can
        // cause events to be sent to the TextControlInnerTextElement. To
        // prevent an infinite loop, we must check for this case before sending
        // the event up the chain.
        if (RefPtr host = shadowHost())
            host->defaultEventHandler(event);
    }
    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

RenderPtr<RenderElement> TextControlInnerTextElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderTextControlInnerBlock>(*this, WTFMove(style));
}

RenderTextControlInnerBlock* TextControlInnerTextElement::renderer() const
{
    return downcast<RenderTextControlInnerBlock>(HTMLDivElement::renderer());
}

std::optional<Style::ElementStyle> TextControlInnerTextElement::resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle* shadowHostStyle)
{
    auto style = downcast<HTMLTextFormControlElement>(*shadowHost()).createInnerTextStyle(*shadowHostStyle);
    return { makeUnique<RenderStyle>(WTFMove(style)) };
}

// MARK: TextControlPlaceholderElement

inline TextControlPlaceholderElement::TextControlPlaceholderElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<TextControlPlaceholderElement> TextControlPlaceholderElement::create(Document& document)
{
    auto element = adoptRef(*new TextControlPlaceholderElement(document));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setPseudo(ShadowPseudoIds::placeholder());
    return element;
}

std::optional<Style::ElementStyle> TextControlPlaceholderElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    auto style = resolveStyle(resolutionContext);

    auto& controlElement = downcast<HTMLTextFormControlElement>(*containingShadowRoot()->host());
    style.renderStyle->setDisplay(controlElement.isPlaceholderVisible() ? DisplayType::Block : DisplayType::None);

    if (is<HTMLInputElement>(controlElement)) {
        auto& inputElement = downcast<HTMLInputElement>(controlElement);
        style.renderStyle->setTextOverflow(inputElement.shouldTruncateText(*shadowHostStyle) ? TextOverflow::Ellipsis : TextOverflow::Clip);
        style.renderStyle->setPaddingTop(Length { 0, LengthType::Fixed });
        style.renderStyle->setPaddingBottom(Length { 0, LengthType::Fixed });
    }
    return style;
}

// MARK: SearchFieldResultsButtonElement

inline SearchFieldResultsButtonElement::SearchFieldResultsButtonElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    if (document.quirks().shouldHideSearchFieldResultsButton())
        setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

    setHasCustomStyleResolveCallbacks();
}

Ref<SearchFieldResultsButtonElement> SearchFieldResultsButtonElement::create(Document& document)
{
    return adoptRef(*new SearchFieldResultsButtonElement(document));
}

std::optional<Style::ElementStyle> SearchFieldResultsButtonElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    RefPtr input = downcast<HTMLInputElement>(shadowHost());
    if (input && input->maxResults() >= 0)
        return std::nullopt;

    if (!shadowHostStyle)
        return std::nullopt;

    auto appearance = shadowHostStyle->effectiveAppearance();
    if (appearance == ControlPartType::TextField) {
        auto elementStyle = resolveStyle(resolutionContext);
        elementStyle.renderStyle->setDisplay(DisplayType::None);
        return elementStyle;
    }
    if (appearance != ControlPartType::SearchField) {
        SetForScope canAdjustStyleForAppearance(m_canAdjustStyleForAppearance, false);
        return resolveStyle(resolutionContext);
    }

    return std::nullopt;
}

void SearchFieldResultsButtonElement::defaultEventHandler(Event& event)
{
    // On mousedown, bring up a menu, if needed
    RefPtr input = downcast<HTMLInputElement>(shadowHost());
    if (input && event.type() == eventNames().mousedownEvent && is<MouseEvent>(event) && downcast<MouseEvent>(event).button() == LeftButton) {
        input->focus();
        input->select();
#if !PLATFORM(IOS_FAMILY)
        document().updateStyleIfNeeded();

        if (auto* renderer = input->renderer()) {
            auto& searchFieldRenderer = downcast<RenderSearchField>(*renderer);
            if (searchFieldRenderer.popupIsVisible())
                searchFieldRenderer.hidePopup();
            else if (input->maxResults() > 0)
                searchFieldRenderer.showPopup();
        }
#endif
        event.setDefaultHandled();
    }

    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

#if !PLATFORM(IOS_FAMILY)
bool SearchFieldResultsButtonElement::willRespondToMouseClickEventsWithEditability(Editability) const
{
    return true;
}
#endif

// MARK: SearchFieldCancelButtonElement

inline SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<SearchFieldCancelButtonElement> SearchFieldCancelButtonElement::create(Document& document)
{
    auto element = adoptRef(*new SearchFieldCancelButtonElement(document));

    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setPseudo(ShadowPseudoIds::webkitSearchCancelButton());
#if !PLATFORM(IOS_FAMILY)
    element->setAttributeWithoutSynchronization(aria_labelAttr, AtomString { AXSearchFieldCancelButtonText() });
#endif
    element->setAttributeWithoutSynchronization(roleAttr, HTMLNames::buttonTag->localName());
    return element;
}

std::optional<Style::ElementStyle> SearchFieldCancelButtonElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    auto elementStyle = resolveStyle(resolutionContext);
    auto& inputElement = downcast<HTMLInputElement>(*shadowHost());
    elementStyle.renderStyle->setVisibility(elementStyle.renderStyle->visibility() == Visibility::Hidden || inputElement.value().isEmpty() ? Visibility::Hidden : Visibility::Visible);

    if (shadowHostStyle && shadowHostStyle->effectiveAppearance() == ControlPartType::TextField)
        elementStyle.renderStyle->setDisplay(DisplayType::None);

    return elementStyle;
}

void SearchFieldCancelButtonElement::defaultEventHandler(Event& event)
{
    RefPtr<HTMLInputElement> input(downcast<HTMLInputElement>(shadowHost()));
    if (!input || !input->isMutable()) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    if (event.type() == eventNames().mousedownEvent && is<MouseEvent>(event) && downcast<MouseEvent>(event).button() == LeftButton) {
        input->focus();
        input->select();
        event.setDefaultHandled();
    }

    if (event.type() == eventNames().clickEvent) {
        input->setValueForUser(emptyString());
        input->onSearch();
        event.setDefaultHandled();
    }

    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

#if !PLATFORM(IOS_FAMILY)
bool SearchFieldCancelButtonElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    const RefPtr<HTMLInputElement> input = downcast<HTMLInputElement>(shadowHost());
    if (input && input->isMutable())
        return true;

    return HTMLDivElement::willRespondToMouseClickEventsWithEditability(editability);
}
#endif

}
