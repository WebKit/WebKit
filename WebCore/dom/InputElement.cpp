/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "InputElement.h"

#include "BeforeTextInsertedEvent.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "Page.h"
#include "RenderTextControlSingleLine.h"
#include "SelectionController.h"
#include "TextIterator.h"

#if ENABLE(WML)
#include "WMLInputElement.h"
#include "WMLNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

// FIXME: According to HTML4, the length attribute's value can be arbitrarily
// large. However, due to https://bugs.webkit.org/show_bug.cgi?id=14536 things
// get rather sluggish when a text field has a larger number of characters than
// this, even when just clicking in the text field.
const int InputElement::s_maximumLength = 524288;
const int InputElement::s_defaultSize = 20;

void InputElement::dispatchFocusEvent(InputElement* inputElement, Element* element)
{
    if (!inputElement->isTextField())
        return;

    updatePlaceholderVisibility(inputElement, element);

    Document* document = element->document();
    if (inputElement->isPasswordField() && document->frame())
        document->setUseSecureKeyboardEntryWhenActive(true);
}

void InputElement::dispatchBlurEvent(InputElement* inputElement, Element* element)
{
    if (!inputElement->isTextField())
        return;

    Document* document = element->document();
    Frame* frame = document->frame();
    if (!frame)
        return;

    updatePlaceholderVisibility(inputElement, element);

    if (inputElement->isPasswordField())
        document->setUseSecureKeyboardEntryWhenActive(false);

    frame->textFieldDidEndEditing(element);
}

bool InputElement::placeholderShouldBeVisible(const InputElement* inputElement, const Element* element)
{
    return inputElement->value().isEmpty()
        && element->document()->focusedNode() != element
        && !inputElement->placeholder().isEmpty();
}

void InputElement::updatePlaceholderVisibility(InputElement* inputElement, Element* element, bool placeholderValueChanged)
{
    ASSERT(inputElement->isTextField());
    bool placeholderVisible = inputElement->placeholderShouldBeVisible();
    if (element->renderer())
        toRenderTextControlSingleLine(element->renderer())->updatePlaceholderVisibility(placeholderVisible, placeholderValueChanged);
}

void InputElement::updateFocusAppearance(InputElementData& data, InputElement* inputElement, Element* element, bool restorePreviousSelection)
{
    ASSERT(inputElement->isTextField());

    if (!restorePreviousSelection || data.cachedSelectionStart() == -1)
        inputElement->select();
    else
        // Restore the cached selection.
        updateSelectionRange(inputElement, element, data.cachedSelectionStart(), data.cachedSelectionEnd());

    Document* document = element->document();
    if (document && document->frame())
        document->frame()->revealSelection();
}

void InputElement::updateSelectionRange(InputElement* inputElement, Element* element, int start, int end)
{
    if (!inputElement->isTextField())
        return;

    element->document()->updateLayoutIgnorePendingStylesheets();

    if (RenderTextControl* renderer = toRenderTextControl(element->renderer()))
        renderer->setSelectionRange(start, end);
}

void InputElement::aboutToUnload(InputElement* inputElement, Element* element)
{
    if (!inputElement->isTextField() || !element->focused())
        return;

    Document* document = element->document();
    Frame* frame = document->frame();
    if (!frame)
        return;

    frame->textFieldDidEndEditing(element);
}

void InputElement::setValueFromRenderer(InputElementData& data, InputElement* inputElement, Element* element, const String& value)
{
    // Renderer and our event handler are responsible for sanitizing values.
    ASSERT(value == inputElement->sanitizeValue(value) || inputElement->sanitizeValue(value).isEmpty());

    if (inputElement->isTextField())
        updatePlaceholderVisibility(inputElement, element);

    // Workaround for bug where trailing \n is included in the result of textContent.
    // The assert macro above may also be simplified to:  value == constrainValue(value)
    // http://bugs.webkit.org/show_bug.cgi?id=9661
    if (value == "\n")
        data.setValue("");
    else
        data.setValue(value);

    element->setFormControlValueMatchesRenderer(true);

    element->dispatchEvent(Event::create(eventNames().inputEvent, true, false));
    notifyFormStateChanged(element);
}

String InputElement::sanitizeValue(const InputElement* inputElement, const String& proposedValue)
{
    return InputElement::sanitizeUserInputValue(inputElement, proposedValue, s_maximumLength);
}

String InputElement::sanitizeUserInputValue(const InputElement* inputElement, const String& proposedValue, int maxLength)
{
    if (!inputElement->isTextField())
        return proposedValue;

    String string = proposedValue;
    string.replace("\r\n", " ");
    string.replace('\r', ' ');
    string.replace('\n', ' ');

    unsigned newLength = string.numCharactersInGraphemeClusters(maxLength);
    for (unsigned i = 0; i < newLength; ++i) {
        const UChar current = string[i];
        if (current < ' ' && current != '\t') {
            newLength = i;
            break;
        }
    }
    return string.left(newLength);
}

void InputElement::handleBeforeTextInsertedEvent(InputElementData& data, InputElement* inputElement, Element* element, Event* event)
{
    ASSERT(event->isBeforeTextInsertedEvent());
    // Make sure that the text to be inserted will not violate the maxLength.

    // We use RenderTextControlSingleLine::text() instead of InputElement::value()
    // because they can be mismatched by sanitizeValue() in
    // RenderTextControlSingleLine::subtreeHasChanged() in some cases.
    unsigned oldLength = toRenderTextControlSingleLine(element->renderer())->text().numGraphemeClusters();

    // selection() may be a pre-edit text.
    unsigned selectionLength = plainText(element->document()->frame()->selection()->selection().toNormalizedRange().get()).numGraphemeClusters();
    ASSERT(oldLength >= selectionLength);

    // Selected characters will be removed by the next text event.
    unsigned baseLength = oldLength - selectionLength;
    unsigned maxLength = static_cast<unsigned>(data.maxLength()); // maxLength() can never be negative.
    unsigned appendableLength = maxLength > baseLength ? maxLength - baseLength : 0;

    // Truncate the inserted text to avoid violating the maxLength and other constraints.
    BeforeTextInsertedEvent* textEvent = static_cast<BeforeTextInsertedEvent*>(event);
    textEvent->setText(sanitizeUserInputValue(inputElement, textEvent->text(), appendableLength));
}

void InputElement::parseSizeAttribute(InputElementData& data, Element* element, MappedAttribute* attribute)
{
    data.setSize(attribute->isNull() ? InputElement::s_defaultSize : attribute->value().toInt());

    if (RenderObject* renderer = element->renderer())
        renderer->setNeedsLayoutAndPrefWidthsRecalc();
}

void InputElement::parseMaxLengthAttribute(InputElementData& data, InputElement* inputElement, Element* element, MappedAttribute* attribute)
{
    int maxLength = attribute->isNull() ? InputElement::s_maximumLength : attribute->value().toInt();
    if (maxLength <= 0 || maxLength > InputElement::s_maximumLength)
        maxLength = InputElement::s_maximumLength;

    int oldMaxLength = data.maxLength();
    data.setMaxLength(maxLength);

    if (oldMaxLength != maxLength)
        updateValueIfNeeded(data, inputElement);

    element->setNeedsStyleRecalc();
}

void InputElement::updateValueIfNeeded(InputElementData& data, InputElement* inputElement)
{
    String oldValue = data.value();
    String newValue = sanitizeValue(inputElement, oldValue);
    if (newValue != oldValue)
        inputElement->setValue(newValue);
}

void InputElement::notifyFormStateChanged(Element* element)
{
    Document* document = element->document();
    Frame* frame = document->frame();
    if (!frame)
        return;

    if (Page* page = frame->page())
        page->chrome()->client()->formStateDidChange(element);
}

// InputElementData
InputElementData::InputElementData()
    : m_size(InputElement::s_defaultSize)
    , m_maxLength(InputElement::s_maximumLength)
    , m_cachedSelectionStart(-1)
    , m_cachedSelectionEnd(-1)
{
}

const AtomicString& InputElementData::name() const
{
    return m_name.isNull() ? emptyAtom : m_name;
}

InputElement* toInputElement(Element* element)
{
    if (element->isHTMLElement() && (element->hasTagName(inputTag) || element->hasTagName(isindexTag)))
        return static_cast<HTMLInputElement*>(element);

#if ENABLE(WML)
    if (element->isWMLElement() && element->hasTagName(WMLNames::inputTag))
        return static_cast<WMLInputElement*>(element);
#endif

    return 0;
}

}
