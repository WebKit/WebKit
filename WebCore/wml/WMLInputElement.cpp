/**
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#if ENABLE(WML)
#include "WMLInputElement.h"

#include "Attribute.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "RenderTextControlSingleLine.h"
#include "TextEvent.h"
#include "WMLDocument.h"
#include "WMLNames.h"
#include "WMLPageState.h"

namespace WebCore {

WMLInputElement::WMLInputElement(const QualifiedName& tagName, Document* doc)
    : WMLFormControlElement(tagName, doc)
    , m_isPasswordField(false)
    , m_isEmptyOk(false)
    , m_numOfCharsAllowedByMask(0)
{
}

PassRefPtr<WMLInputElement> WMLInputElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new WMLInputElement(tagName, document));
}

WMLInputElement::~WMLInputElement()
{
    if (m_isPasswordField)
        document()->unregisterForDocumentActivationCallbacks(this);
}

static const AtomicString& formatCodes()
{
    DEFINE_STATIC_LOCAL(AtomicString, codes, ("AaNnXxMm"));
    return codes;
}

bool WMLInputElement::isKeyboardFocusable(KeyboardEvent*) const
{
    return WMLFormControlElement::isFocusable();
}

bool WMLInputElement::isMouseFocusable() const
{
    return WMLFormControlElement::isFocusable();
}

void WMLInputElement::dispatchFocusEvent()
{
    InputElement::dispatchFocusEvent(this, this);
    WMLElement::dispatchFocusEvent();
}

void WMLInputElement::dispatchBlurEvent()
{
    // Firstly check if it is allowed to leave this input field
    String val = value();
    if ((!m_isEmptyOk && val.isEmpty()) || !isConformedToInputMask(val)) {
        updateFocusAppearance(true);
        return;
    }

    // update the name variable of WML input elmenet
    String nameVariable = formControlName();
    if (!nameVariable.isEmpty())
        wmlPageStateForDocument(document())->storeVariable(nameVariable, val); 

    InputElement::dispatchBlurEvent(this, this);
    WMLElement::dispatchBlurEvent();
}

void WMLInputElement::updateFocusAppearance(bool restorePreviousSelection)
{
    InputElement::updateFocusAppearance(m_data, this, this, restorePreviousSelection);
}

void WMLInputElement::aboutToUnload()
{
    InputElement::aboutToUnload(this, this);
}

int WMLInputElement::size() const
{
    return m_data.size();
}

const AtomicString& WMLInputElement::formControlType() const
{
    // needs to be lowercase according to DOM spec
    if (m_isPasswordField) {
        DEFINE_STATIC_LOCAL(const AtomicString, password, ("password"));
        return password;
    }

    DEFINE_STATIC_LOCAL(const AtomicString, text, ("text"));
    return text;
}

const AtomicString& WMLInputElement::formControlName() const
{
    return m_data.name();
}

const String& WMLInputElement::suggestedValue() const
{
    return m_data.suggestedValue();
}

String WMLInputElement::value() const
{
    String value = m_data.value();
    if (value.isNull())
        value = constrainValue(getAttribute(HTMLNames::valueAttr));

    return value;
}

void WMLInputElement::setValue(const String& value, bool sendChangeEvent)
{
    setFormControlValueMatchesRenderer(false);
    m_data.setValue(constrainValue(value));
    if (inDocument())
        document()->updateStyleIfNeeded();
    if (renderer())
        renderer()->updateFromElement();
    setNeedsStyleRecalc();

    unsigned max = m_data.value().length();
    if (document()->focusedNode() == this)
        InputElement::updateSelectionRange(this, this, max, max);
    else
        cacheSelection(max, max);

    InputElement::notifyFormStateChanged(this);
}

void WMLInputElement::setValueForUser(const String& value)
{
    /* InputElement class defines pure virtual function 'setValueForUser', which 
       will be useful only in HTMLInputElement. Do nothing in 'WMLInputElement'.
     */
}

void WMLInputElement::setValueFromRenderer(const String& value)
{
    InputElement::setValueFromRenderer(m_data, this, this, value);
}

bool WMLInputElement::saveFormControlState(String& result) const
{
    if (m_isPasswordField)
        return false;

    result = value();
    return true;
}

void WMLInputElement::restoreFormControlState(const String& state)
{
    ASSERT(!m_isPasswordField); // should never save/restore password fields
    setValue(state);
}

void WMLInputElement::select()
{
    if (RenderTextControl* r = toRenderTextControl(renderer()))
        r->select();
}

void WMLInputElement::accessKeyAction(bool)
{
    // should never restore previous selection here
    focus(false);
}

void WMLInputElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == HTMLNames::nameAttr)
        m_data.setName(parseValueForbiddingVariableReferences(attr->value()));
    else if (attr->name() == HTMLNames::typeAttr) {
        String type = parseValueForbiddingVariableReferences(attr->value());
        m_isPasswordField = (type == "password");
    } else if (attr->name() == HTMLNames::valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (m_data.value().isNull())
            setNeedsStyleRecalc();
        setFormControlValueMatchesRenderer(false);
    } else if (attr->name() == HTMLNames::maxlengthAttr)
        InputElement::parseMaxLengthAttribute(m_data, this, this, attr);
    else if (attr->name() == HTMLNames::sizeAttr)
        InputElement::parseSizeAttribute(m_data, this, attr);
    else if (attr->name() == WMLNames::formatAttr)
        m_formatMask = validateInputMask(parseValueForbiddingVariableReferences(attr->value()));
    else if (attr->name() == WMLNames::emptyokAttr)
        m_isEmptyOk = (attr->value() == "true");
    else
        WMLElement::parseMappedAttribute(attr);

    // FIXME: Handle 'accesskey' attribute
    // FIXME: Handle 'tabindex' attribute
    // FIXME: Handle 'title' attribute
}

void WMLInputElement::copyNonAttributeProperties(const Element* source)
{
    const WMLInputElement* sourceElement = static_cast<const WMLInputElement*>(source);
    m_data.setValue(sourceElement->m_data.value());
    WMLElement::copyNonAttributeProperties(source);
}

RenderObject* WMLInputElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderTextControlSingleLine(this, false);
}

void WMLInputElement::detach()
{
    WMLElement::detach();
    setFormControlValueMatchesRenderer(false);
}
    
bool WMLInputElement::appendFormData(FormDataList& encoding, bool)
{
    if (formControlName().isEmpty())
        return false;

    encoding.appendData(formControlName(), value());
    return true;
}

void WMLInputElement::reset()
{
    setValue(String());
}

void WMLInputElement::defaultEventHandler(Event* evt)
{
    bool clickDefaultFormButton = false;

    if (evt->type() == eventNames().textInputEvent && evt->isTextEvent()) {
        TextEvent* textEvent = static_cast<TextEvent*>(evt);
        if (textEvent->data() == "\n")
            clickDefaultFormButton = true;
        else if (renderer() && !isConformedToInputMask(textEvent->data()[0], toRenderTextControl(renderer())->text().length() + 1))
            // If the inputed char doesn't conform to the input mask, stop handling 
            return;
    }

    if (evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent() && focused() && document()->frame()
        && document()->frame()->editor()->doTextFieldCommandFromEvent(this, static_cast<KeyboardEvent*>(evt))) {
        evt->setDefaultHandled();
        return;
    }
    
    // Let the key handling done in EventTargetNode take precedence over the event handling here for editable text fields
    if (!clickDefaultFormButton) {
        WMLElement::defaultEventHandler(evt);
        if (evt->defaultHandled())
            return;
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == eventNames().keypressEvent && evt->isKeyboardEvent()) {
        // Simulate mouse click on the default form button for enter for these types of elements.
        if (static_cast<KeyboardEvent*>(evt)->charCode() == '\r')
            clickDefaultFormButton = true;
    }

    if (clickDefaultFormButton) {
        // Fire onChange for text fields.
        RenderObject* r = renderer();
        if (r && toRenderTextControl(r)->wasChangedSinceLastChangeEvent()) {
            dispatchEvent(Event::create(eventNames().changeEvent, true, false));
            
            // Refetch the renderer since arbitrary JS code run during onchange can do anything, including destroying it.
            r = renderer();
            if (r)
                toRenderTextControl(r)->setChangedSinceLastChangeEvent(false);
        }

        evt->setDefaultHandled();
        return;
    }

    if (evt->isBeforeTextInsertedEvent())
        InputElement::handleBeforeTextInsertedEvent(m_data, this, this, evt);

    if (renderer() && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent() || evt->type() == eventNames().blurEvent || evt->type() == eventNames().focusEvent))
        toRenderTextControlSingleLine(renderer())->forwardEvent(evt);
}

void WMLInputElement::cacheSelection(int start, int end)
{
    m_data.setCachedSelectionStart(start);
    m_data.setCachedSelectionEnd(end);
}

String WMLInputElement::constrainValue(const String& proposedValue) const
{
    return InputElement::sanitizeUserInputValue(this, proposedValue, m_data.maxLength());
}

void WMLInputElement::documentDidBecomeActive()
{
    ASSERT(m_isPasswordField);
    reset();
}

void WMLInputElement::willMoveToNewOwnerDocument()
{
    // Always unregister for cache callbacks when leaving a document, even if we would otherwise like to be registered
    if (m_isPasswordField)
        document()->unregisterForDocumentActivationCallbacks(this);

    WMLElement::willMoveToNewOwnerDocument();
}

void WMLInputElement::didMoveToNewOwnerDocument()
{
    if (m_isPasswordField)
        document()->registerForDocumentActivationCallbacks(this);

    WMLElement::didMoveToNewOwnerDocument();
}

void WMLInputElement::initialize()
{
    String nameVariable = formControlName();
    String variableValue;
    WMLPageState* pageSate = wmlPageStateForDocument(document()); 
    ASSERT(pageSate);
    if (!nameVariable.isEmpty())
        variableValue = pageSate->getVariable(nameVariable);

    if (variableValue.isEmpty() || !isConformedToInputMask(variableValue)) {
        String val = value();
        if (isConformedToInputMask(val))
            variableValue = val;
        else
            variableValue = "";
 
        pageSate->storeVariable(nameVariable, variableValue);
    }
    setValue(variableValue);
 
    if (!hasAttribute(WMLNames::emptyokAttr)) {
        if (m_formatMask.isEmpty() || 
            // check if the format codes is just "*f"
           (m_formatMask.length() == 2 && m_formatMask[0] == '*' && formatCodes().find(m_formatMask[1]) != -1))
            m_isEmptyOk = true;
    }
}

String WMLInputElement::validateInputMask(const String& inputMask)
{
    bool isValid = true;
    bool hasWildcard = false;
    unsigned escapeCharCount = 0;
    unsigned maskLength = inputMask.length();
    UChar formatCode;
 
    for (unsigned i = 0; i < maskLength; ++i) {
        formatCode = inputMask[i];
        if (formatCodes().find(formatCode) == -1) {
            if (formatCode == '*' || (WTF::isASCIIDigit(formatCode) && formatCode != '0')) {
                // validate codes which ends with '*f' or 'nf'
                formatCode = inputMask[++i];
                if ((i + 1 != maskLength) || formatCodes().find(formatCode) == -1) {
                    isValid = false;
                    break;
                }
                hasWildcard = true;
            } else if (formatCode == '\\') {
                //skip over the next mask character
                ++i;
                ++escapeCharCount;
            } else {
                isValid = false;
                break;
            }
        }
    }

    if (!isValid)
        return String();
 
    // calculate the number of characters allowed to be entered by input mask
    m_numOfCharsAllowedByMask = maskLength;

    if (escapeCharCount)
        m_numOfCharsAllowedByMask -= escapeCharCount;

    if (hasWildcard) {
        formatCode = inputMask[maskLength - 2];
        if (formatCode == '*')
            m_numOfCharsAllowedByMask = m_data.maxLength();
        else {
            unsigned leftLen = String(&formatCode).toInt();
            m_numOfCharsAllowedByMask = leftLen + m_numOfCharsAllowedByMask - 2;
        }
    }

    return inputMask;
}

bool WMLInputElement::isConformedToInputMask(const String& inputChars)
{
    for (unsigned i = 0; i < inputChars.length(); ++i)
        if (!isConformedToInputMask(inputChars[i], i + 1, false))
            return false;

    return true;
}
 
bool WMLInputElement::isConformedToInputMask(UChar inChar, unsigned inputCharCount, bool isUserInput)
{
    if (m_formatMask.isEmpty())
        return true;
 
    if (inputCharCount > m_numOfCharsAllowedByMask)
        return false;

    unsigned maskIndex = 0;
    if (isUserInput) {
        unsigned cursorPosition = 0;
        if (renderer())
            cursorPosition = toRenderTextControl(renderer())->selectionStart(); 
        else
            cursorPosition = m_data.cachedSelectionStart();

        maskIndex = cursorPositionToMaskIndex(cursorPosition);
    } else
        maskIndex = cursorPositionToMaskIndex(inputCharCount - 1);

    bool ok = true;
    UChar mask = m_formatMask[maskIndex];
    // match the inputed character with input mask
    switch (mask) {
    case 'A':
        ok = !WTF::isASCIIDigit(inChar) && !WTF::isASCIILower(inChar) && WTF::isASCIIPrintable(inChar);
        break;
    case 'a':
        ok = !WTF::isASCIIDigit(inChar) && !WTF::isASCIIUpper(inChar) && WTF::isASCIIPrintable(inChar);
        break;
    case 'N':
        ok = WTF::isASCIIDigit(inChar);
        break;
    case 'n':
        ok = !WTF::isASCIIAlpha(inChar) && WTF::isASCIIPrintable(inChar);
        break;
    case 'X':
        ok = !WTF::isASCIILower(inChar) && WTF::isASCIIPrintable(inChar);
        break;
    case 'x':
        ok = !WTF::isASCIIUpper(inChar) && WTF::isASCIIPrintable(inChar);
        break;
    case 'M':
        ok = WTF::isASCIIPrintable(inChar);
        break;
    case 'm':
        ok = WTF::isASCIIPrintable(inChar);
        break;
    default:
        ok = (mask == inChar);
        break;
    }

    return ok;
}

unsigned WMLInputElement::cursorPositionToMaskIndex(unsigned cursorPosition)
{
    UChar mask;
    int index = -1;
    do {
        mask = m_formatMask[++index];
        if (mask == '\\')
            ++index;
        else if (mask == '*' || (WTF::isASCIIDigit(mask) && mask != '0')) {
            index = m_formatMask.length() - 1;
            break;
        }
    } while (cursorPosition--);
 
    return index;
}

}

#endif
