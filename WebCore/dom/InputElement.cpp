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
#include "TextBreakIterator.h"

#if ENABLE(WML)
#include "WMLInputElement.h"
#include "WMLNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

// FIXME: According to HTML4, the length attribute's value can be arbitrarily
// large. However, due to http://bugs.webkit.org/show_bugs.cgi?id=14536 things
// get rather sluggish when a text field has a larger number of characters than
// this, even when just clicking in the text field.
const int InputElement::s_maximumLength = 524288;
const int InputElement::s_defaultSize = 20;

void InputElement::dispatchFocusEvent(InputElementData& data, Document* document)
{
    if (!data.inputElement()->isTextField())
        return;

    updatePlaceholderVisibility(data, document);

    if (data.inputElement()->isPasswordField() && document->frame())
        document->setUseSecureKeyboardEntryWhenActive(true);
}

void InputElement::dispatchBlurEvent(InputElementData& data, Document* document)
{
    if (!data.inputElement()->isTextField())
        return;

    Frame* frame = document->frame();
    if (!frame)
        return;

    updatePlaceholderVisibility(data, document);

    if (data.inputElement()->isPasswordField())
        document->setUseSecureKeyboardEntryWhenActive(false);

    frame->textFieldDidEndEditing(data.element());
}

void InputElement::updatePlaceholderVisibility(InputElementData& data, Document* document, bool placeholderValueChanged)
{
    ASSERT(data.inputElement()->isTextField());

    bool oldPlaceholderShouldBeVisible = data.placeholderShouldBeVisible();
    Element* element = data.element();

    data.setPlaceholderShouldBeVisible(data.inputElement()->value().isEmpty() 
                                       && document->focusedNode() != element
                                       && !data.inputElement()->placeholder().isEmpty());

    if ((oldPlaceholderShouldBeVisible != data.placeholderShouldBeVisible() || placeholderValueChanged) && element->renderer())
        static_cast<RenderTextControlSingleLine*>(element->renderer())->updatePlaceholderVisibility();
}

void InputElement::updateFocusAppearance(InputElementData& data, Document* document, bool restorePreviousSelection)
{
    ASSERT(data.inputElement()->isTextField());

    if (!restorePreviousSelection || data.cachedSelectionStart() == -1)
        data.inputElement()->select();
    else
        // Restore the cached selection.
        updateSelectionRange(data, data.cachedSelectionStart(), data.cachedSelectionEnd());

    if (document && document->frame())
        document->frame()->revealSelection();
}

void InputElement::updateSelectionRange(InputElementData& data, int start, int end)
{
    if (!data.inputElement()->isTextField())
        return;

    if (RenderTextControl* renderer = toRenderTextControl(data.element()->renderer()))
        renderer->setSelectionRange(start, end);
}

void InputElement::aboutToUnload(InputElementData& data, Document* document)
{
    if (!data.inputElement()->isTextField() || !data.element()->focused() || !document->frame())
        return;

    document->frame()->textFieldDidEndEditing(data.element());
}

void InputElement::setValueFromRenderer(InputElementData& data, Document* document, const String& value)
{
    // Renderer and our event handler are responsible for constraining values.
    ASSERT(value == data.inputElement()->constrainValue(value) || data.inputElement()->constrainValue(value).isEmpty());

    if (data.inputElement()->isTextField())
        updatePlaceholderVisibility(data, document);

    // Workaround for bug where trailing \n is included in the result of textContent.
    // The assert macro above may also be simplified to:  value == constrainValue(value)
    // http://bugs.webkit.org/show_bug.cgi?id=9661
    if (value == "\n")
        data.setValue("");
    else
        data.setValue(value);

    Element* element = data.element();
    element->setFormControlValueMatchesRenderer(true);

    // Fire the "input" DOM event
    element->dispatchEvent(eventNames().inputEvent, true, false);
    notifyFormStateChanged(data, document);
}

static int numCharactersInGraphemeClusters(StringImpl* s, int numGraphemeClusters)
{
    if (!s)
        return 0;

    TextBreakIterator* it = characterBreakIterator(s->characters(), s->length());
    if (!it)
        return 0;

    for (int i = 0; i < numGraphemeClusters; ++i) {
        if (textBreakNext(it) == TextBreakDone)
            return s->length();
    }

    return textBreakCurrent(it);
}

String InputElement::constrainValue(const InputElementData& data, const String& proposedValue, int maxLength)
{
    String string = proposedValue;
    if (!data.inputElement()->isTextField())
        return string;
        
    string.replace("\r\n", " ");
    string.replace('\r', ' ');
    string.replace('\n', ' ');
    
    StringImpl* s = string.impl();
    int newLength = numCharactersInGraphemeClusters(s, maxLength);
    for (int i = 0; i < newLength; ++i) {
        const UChar& current = (*s)[i];
        if (current < ' ' && current != '\t') {
            newLength = i;
            break;
        }
    }

    if (newLength < static_cast<int>(string.length()))
        return string.left(newLength);

    return string;
}

static int numGraphemeClusters(StringImpl* s)
{
    if (!s)
        return 0;

    TextBreakIterator* it = characterBreakIterator(s->characters(), s->length());
    if (!it)
        return 0;

    int num = 0;
    while (textBreakNext(it) != TextBreakDone)
        ++num;

    return num;
}

void InputElement::handleBeforeTextInsertedEvent(InputElementData& data, Document* document, Event* event)
{
    ASSERT(event->isBeforeTextInsertedEvent());

    // Make sure that the text to be inserted will not violate the maxLength.
    int oldLength = numGraphemeClusters(data.inputElement()->value().impl());
    ASSERT(oldLength <= data.maxLength());
    int selectionLength = numGraphemeClusters(plainText(document->frame()->selection()->selection().toNormalizedRange().get()).impl());
    ASSERT(oldLength >= selectionLength);
    int maxNewLength = data.maxLength() - (oldLength - selectionLength);

    // Truncate the inserted text to avoid violating the maxLength and other constraints.
    BeforeTextInsertedEvent* textEvent = static_cast<BeforeTextInsertedEvent*>(event);
    textEvent->setText(constrainValue(data, textEvent->text(), maxNewLength));
}

void InputElement::parseSizeAttribute(InputElementData& data, MappedAttribute* attribute)
{
    data.setSize(attribute->isNull() ? InputElement::s_defaultSize : attribute->value().toInt());

    if (RenderObject* renderer = data.element()->renderer())
        renderer->setNeedsLayoutAndPrefWidthsRecalc();
}

void InputElement::parseMaxLengthAttribute(InputElementData& data, MappedAttribute* attribute)
{
    int maxLength = attribute->isNull() ? InputElement::s_maximumLength : attribute->value().toInt();
    if (maxLength <= 0 || maxLength > InputElement::s_maximumLength)
        maxLength = InputElement::s_maximumLength;

    int oldMaxLength = data.maxLength();
    data.setMaxLength(maxLength);

    if (oldMaxLength != maxLength)
        updateValueIfNeeded(data);

    data.element()->setNeedsStyleRecalc();
}

void InputElement::updateValueIfNeeded(InputElementData& data)
{
    String oldValue = data.value();
    String newValue = data.inputElement()->constrainValue(oldValue);
    if (newValue != oldValue)
        data.inputElement()->setValue(newValue);
}

void InputElement::notifyFormStateChanged(InputElementData& data, Document* document)
{
    Frame* frame = document->frame();
    if (!frame)
        return;

    if (Page* page = frame->page())
        page->chrome()->client()->formStateDidChange(data.element());
}

// InputElementData
InputElementData::InputElementData(InputElement* inputElement, Element* element)
    : m_inputElement(inputElement)
    , m_element(element)
    , m_placeholderShouldBeVisible(false)
    , m_size(InputElement::s_defaultSize)
    , m_maxLength(InputElement::s_maximumLength)
    , m_cachedSelectionStart(-1)
    , m_cachedSelectionEnd(-1)
{
    ASSERT(m_inputElement);
    ASSERT(m_element);
}

InputElementData::~InputElementData()
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
