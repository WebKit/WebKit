/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "TextFieldInputType.h"

#include "BeforeTextInsertedEvent.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "TextControlInnerElements.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "WheelEvent.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace HTMLNames;

TextFieldInputType::TextFieldInputType(HTMLInputElement* element)
    : InputType(element)
{
}

TextFieldInputType::~TextFieldInputType()
{
}

bool TextFieldInputType::isTextField() const
{
    return true;
}

bool TextFieldInputType::valueMissing(const String& value) const
{
    return element()->required() && value.isEmpty();
}

bool TextFieldInputType::canSetSuggestedValue()
{
    return true;
}

void TextFieldInputType::setValue(const String& sanitizedValue, bool valueChanged, bool sendChangeEvent)
{
    InputType::setValue(sanitizedValue, valueChanged, sendChangeEvent);

    if (valueChanged)
        element()->updateInnerTextValue();

    unsigned max = visibleValue().length();
    if (element()->focused())
        element()->setSelectionRange(max, max);
    else
        element()->cacheSelectionInResponseToSetValue(max);
}

void TextFieldInputType::dispatchChangeEventInResponseToSetValue()
{
    // If the user is still editing this field, dispatch an input event rather than a change event.
    // The change event will be dispatched when editing finishes.
    if (element()->focused()) {
        element()->dispatchFormControlInputEvent();
        return;
    }
    InputType::dispatchChangeEventInResponseToSetValue();
}

void TextFieldInputType::handleKeydownEvent(KeyboardEvent* event)
{
    if (!element()->focused())
        return;
    Frame* frame = element()->document()->frame();
    if (!frame || !frame->editor()->doTextFieldCommandFromEvent(element(), event))
        return;
    event->setDefaultHandled();
}

void TextFieldInputType::handleKeydownEventForSpinButton(KeyboardEvent* event)
{
    if (element()->disabled() || element()->readOnly())
        return;
    const String& key = event->keyIdentifier();
    int step = 0;
    if (key == "Up")
        step = 1;
    else if (key == "Down")
        step = -1;
    else
        return;
    element()->stepUpFromRenderer(step);
    event->setDefaultHandled();
}

void TextFieldInputType::handleWheelEventForSpinButton(WheelEvent* event)
{
    if (element()->disabled() || element()->readOnly() || !element()->focused())
        return;
    int step = 0;
    if (event->wheelDeltaY() > 0)
        step = 1;
    else if (event->wheelDeltaY() < 0)
        step = -1;
    else
        return;
    element()->stepUpFromRenderer(step);
    event->setDefaultHandled();
}

void TextFieldInputType::forwardEvent(Event* event)
{
    if (element()->renderer() && (event->isMouseEvent() || event->isDragEvent() || event->hasInterface(eventNames().interfaceForWheelEvent) || event->type() == eventNames().blurEvent || event->type() == eventNames().focusEvent)) {
        RenderTextControlSingleLine* renderTextControl = toRenderTextControlSingleLine(element()->renderer());
        if (event->type() == eventNames().blurEvent) {
            if (RenderBox* innerTextRenderer = innerTextElement()->renderBox()) {
                if (RenderLayer* innerLayer = innerTextRenderer->layer())
                    innerLayer->scrollToOffset(!renderTextControl->style()->isLeftToRightDirection() ? innerLayer->scrollWidth() : 0, 0, RenderLayer::ScrollOffsetClamped);
            }

            renderTextControl->capsLockStateMayHaveChanged();
        } else if (event->type() == eventNames().focusEvent)
            renderTextControl->capsLockStateMayHaveChanged();

        element()->forwardEvent(event);
    }
}

bool TextFieldInputType::shouldSubmitImplicitly(Event* event)
{
    return (event->type() == eventNames().textInputEvent && event->hasInterface(eventNames().interfaceForTextEvent) && static_cast<TextEvent*>(event)->data() == "\n") || InputType::shouldSubmitImplicitly(event);
}

RenderObject* TextFieldInputType::createRenderer(RenderArena* arena, RenderStyle*) const
{
    return new (arena) RenderTextControlSingleLine(element());
}

bool TextFieldInputType::needsContainer() const
{
#if ENABLE(INPUT_SPEECH)
    return element()->isSpeechEnabled();
#else
    return false;
#endif
}

void TextFieldInputType::createShadowSubtree()
{
    ASSERT(!m_innerText);
    ASSERT(!m_innerBlock);
    ASSERT(!m_innerSpinButton);

    Document* document = element()->document();
    RefPtr<RenderTheme> theme = document->page() ? document->page()->theme() : RenderTheme::defaultTheme();
    bool shouldHaveSpinButton = theme->shouldHaveSpinButton(element());
    bool createsContainer = shouldHaveSpinButton || needsContainer();

    ExceptionCode ec = 0;
    m_innerText = TextControlInnerTextElement::create(document);
    if (!createsContainer) {
        element()->ensureShadowRoot()->appendChild(m_innerText, ec);
        return;
    }

    ShadowRoot* shadowRoot = element()->ensureShadowRoot();
    m_container = HTMLDivElement::create(document);
    m_container->setShadowPseudoId("-webkit-textfield-decoration-container");
    shadowRoot->appendChild(m_container, ec);

    m_innerBlock = TextControlInnerElement::create(document);
    m_innerBlock->appendChild(m_innerText, ec);
    m_container->appendChild(m_innerBlock, ec);

#if ENABLE(INPUT_SPEECH)
    ASSERT(!m_speechButton);
    if (element()->isSpeechEnabled()) {
        m_speechButton = InputFieldSpeechButtonElement::create(document);
        m_container->appendChild(m_speechButton, ec);
    }
#endif

    if (shouldHaveSpinButton) {
        m_innerSpinButton = SpinButtonElement::create(document);
        m_container->appendChild(m_innerSpinButton, ec);
    }
}

HTMLElement* TextFieldInputType::containerElement() const
{
    return m_container.get();
}

HTMLElement* TextFieldInputType::innerBlockElement() const
{
    return m_innerBlock.get();
}

HTMLElement* TextFieldInputType::innerTextElement() const
{
    ASSERT(m_innerText);
    return m_innerText.get();
}

HTMLElement* TextFieldInputType::innerSpinButtonElement() const
{
    return m_innerSpinButton.get();
}

#if ENABLE(INPUT_SPEECH)
HTMLElement* TextFieldInputType::speechButtonElement() const
{
    return m_speechButton.get();
}
#endif

HTMLElement* TextFieldInputType::placeholderElement() const
{
    return m_placeholder.get();
}

void TextFieldInputType::destroyShadowSubtree()
{
    InputType::destroyShadowSubtree();
    m_innerText.clear();
    m_placeholder.clear();
    m_innerBlock.clear();
#if ENABLE(INPUT_SPEECH)
    m_speechButton.clear();
#endif
    m_innerSpinButton.clear();
    m_container.clear();
}

void TextFieldInputType::disabledAttributeChanged()
{
    if (m_innerSpinButton)
        m_innerSpinButton->releaseCapture();
}

void TextFieldInputType::readonlyAttributeChanged()
{
    if (m_innerSpinButton)
        m_innerSpinButton->releaseCapture();
}

bool TextFieldInputType::shouldUseInputMethod() const
{
    return true;
}

static bool isASCIILineBreak(UChar c)
{
    return c == '\r' || c == '\n';
}

static String limitLength(const String& string, int maxLength)
{
    unsigned newLength = numCharactersInGraphemeClusters(string, maxLength);
    for (unsigned i = 0; i < newLength; ++i) {
        const UChar current = string[i];
        if (current < ' ' && current != '\t') {
            newLength = i;
            break;
        }
    }
    return string.left(newLength);
}

String TextFieldInputType::sanitizeValue(const String& proposedValue) const
{
    return limitLength(proposedValue.removeCharacters(isASCIILineBreak), HTMLInputElement::maximumLength);
}

void TextFieldInputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent* event)
{
    // Make sure that the text to be inserted will not violate the maxLength.

    // We use RenderTextControlSingleLine::text() instead of InputElement::value()
    // because they can be mismatched by sanitizeValue() in
    // HTMLInputElement::subtreeHasChanged() in some cases.
    unsigned oldLength = numGraphemeClusters(element()->innerTextValue());

    // selectionLength represents the selection length of this text field to be
    // removed by this insertion.
    // If the text field has no focus, we don't need to take account of the
    // selection length. The selection is the source of text drag-and-drop in
    // that case, and nothing in the text field will be removed.
    unsigned selectionLength = element()->focused() ? numGraphemeClusters(plainText(element()->document()->frame()->selection()->selection().toNormalizedRange().get())) : 0;
    ASSERT(oldLength >= selectionLength);

    // Selected characters will be removed by the next text event.
    unsigned baseLength = oldLength - selectionLength;
    unsigned maxLength = static_cast<unsigned>(isTextType() ? element()->maxLength() : HTMLInputElement::maximumLength); // maxLength can never be negative.
    unsigned appendableLength = maxLength > baseLength ? maxLength - baseLength : 0;

    // Truncate the inserted text to avoid violating the maxLength and other constraints.
    String eventText = event->text();
    eventText.replace("\r\n", " ");
    eventText.replace('\r', ' ');
    eventText.replace('\n', ' ');

    event->setText(limitLength(eventText, appendableLength));
}

bool TextFieldInputType::shouldRespectListAttribute()
{
    return true;
}

void TextFieldInputType::updatePlaceholderText()
{
    if (!supportsPlaceholder())
        return;
    ExceptionCode ec = 0;
    String placeholderText = element()->strippedPlaceholder();
    if (placeholderText.isEmpty()) {
        if (m_placeholder) {
            m_placeholder->parentNode()->removeChild(m_placeholder.get(), ec);
            ASSERT(!ec);
            m_placeholder.clear();
        }
        return;
    }
    if (!m_placeholder) {
        m_placeholder = HTMLDivElement::create(element()->document());
        m_placeholder->setShadowPseudoId("-webkit-input-placeholder");
        element()->shadowRoot()->insertBefore(m_placeholder, m_container ? m_container->nextSibling() : innerTextElement()->nextSibling(), ec);
        ASSERT(!ec);
    }
    m_placeholder->setInnerText(placeholderText, ec);
    ASSERT(!ec);
}

bool TextFieldInputType::appendFormData(FormDataList& list, bool multipart) const
{
    InputType::appendFormData(list, multipart);
    const AtomicString& dirnameAttrValue = element()->fastGetAttribute(dirnameAttr);
    if (!dirnameAttrValue.isNull())
        list.appendData(dirnameAttrValue, element()->directionForFormData());
    return true;
}

} // namespace WebCore
