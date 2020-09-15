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
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "TextEvent.h"
#include "TextEventInputType.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>

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

Optional<Style::ElementStyle> TextControlInnerContainer::resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle*)
{
    auto elementStyle = resolveStyle(&parentStyle);
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

Optional<Style::ElementStyle> TextControlInnerElement::resolveCustomStyle(const RenderStyle&, const RenderStyle* shadowHostStyle)
{
    auto newStyle = RenderStyle::createPtr();
    newStyle->inheritFrom(*shadowHostStyle);
    newStyle->setFlexGrow(1);
    newStyle->setMinWidth(Length { 0, Fixed }); // Needed for correct shrinking.
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
        int pixels = emSize->computeLength<int>(CSSToLengthConversionData { newStyle.get(), nullptr, nullptr, nullptr, 1.0, WTF::nullopt });
        newStyle->setFlexBasis(Length { pixels, Fixed });
    }

    return { WTFMove(newStyle) };
}

// MARK: TextControlInnerTextElement

inline TextControlInnerTextElement::TextControlInnerTextElement(Document& document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<TextControlInnerTextElement> TextControlInnerTextElement::create(Document& document)
{
    return adoptRef(*new TextControlInnerTextElement(document));
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
        if (auto host = makeRefPtr(shadowHost()))
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

Optional<Style::ElementStyle> TextControlInnerTextElement::resolveCustomStyle(const RenderStyle&, const RenderStyle* shadowHostStyle)
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
    static MainThreadNeverDestroyed<const AtomString> placeholderName("placeholder", AtomString::ConstructFromLiteral);
    element->setPseudo(placeholderName);
    return element;
}

Optional<Style::ElementStyle> TextControlPlaceholderElement::resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle* shadowHostStyle)
{
    auto style = resolveStyle(&parentStyle);

    auto& controlElement = downcast<HTMLTextFormControlElement>(*containingShadowRoot()->host());
    style.renderStyle->setDisplay(controlElement.isPlaceholderVisible() ? DisplayType::Block : DisplayType::None);

    if (is<HTMLInputElement>(controlElement)) {
        auto& inputElement = downcast<HTMLInputElement>(controlElement);
        style.renderStyle->setTextOverflow(inputElement.shouldTruncateText(*shadowHostStyle) ? TextOverflow::Ellipsis : TextOverflow::Clip);
    }
    return style;
}

// MARK: SearchFieldResultsButtonElement

inline SearchFieldResultsButtonElement::SearchFieldResultsButtonElement(Document& document)
    : HTMLDivElement(divTag, document)
{
}

Ref<SearchFieldResultsButtonElement> SearchFieldResultsButtonElement::create(Document& document)
{
    return adoptRef(*new SearchFieldResultsButtonElement(document));
}

void SearchFieldResultsButtonElement::defaultEventHandler(Event& event)
{
    // On mousedown, bring up a menu, if needed
    auto input = makeRefPtr(downcast<HTMLInputElement>(shadowHost()));
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
bool SearchFieldResultsButtonElement::willRespondToMouseClickEvents()
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
    static MainThreadNeverDestroyed<const AtomString> webkitSearchCancelButtonName("-webkit-search-cancel-button", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> buttonName("button", AtomString::ConstructFromLiteral);
    element->setPseudo(webkitSearchCancelButtonName);
#if !PLATFORM(IOS_FAMILY)
    element->setAttributeWithoutSynchronization(aria_labelAttr, AXSearchFieldCancelButtonText());
#endif
    element->setAttributeWithoutSynchronization(roleAttr, buttonName);
    return element;
}

Optional<Style::ElementStyle> SearchFieldCancelButtonElement::resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle*)
{
    auto elementStyle = resolveStyle(&parentStyle);
    auto& inputElement = downcast<HTMLInputElement>(*shadowHost());
    elementStyle.renderStyle->setVisibility(elementStyle.renderStyle->visibility() == Visibility::Hidden || inputElement.value().isEmpty() ? Visibility::Hidden : Visibility::Visible);
    return elementStyle;
}

void SearchFieldCancelButtonElement::defaultEventHandler(Event& event)
{
    RefPtr<HTMLInputElement> input(downcast<HTMLInputElement>(shadowHost()));
    if (!input || input->isDisabledOrReadOnly()) {
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
bool SearchFieldCancelButtonElement::willRespondToMouseClickEvents()
{
    const RefPtr<HTMLInputElement> input = downcast<HTMLInputElement>(shadowHost());
    if (input && !input->isDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}
#endif

}
