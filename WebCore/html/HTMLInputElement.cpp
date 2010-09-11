/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "HTMLInputElement.h"

#include "AXObjectCache.h"
#include "Attribute.h"
#include "BeforeTextInsertedEvent.h"
#include "CSSPropertyNames.h"
#include "ChromeClient.h"
#include "DateComponents.h"
#include "Document.h"
#include "Editor.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "File.h"
#include "FileList.h"
#include "FileSystem.h"
#include "FocusController.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLDataListElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLTreeBuilder.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RegularExpression.h"
#include "RenderButton.h"
#include "RenderFileUploadControl.h"
#include "RenderImage.h"
#include "RenderSlider.h"
#include "RenderText.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScriptEventListener.h"
#include "StepRange.h"
#include "TextEvent.h"
#include "WheelEvent.h"
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHash.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int maxSavedResults = 256;

// Constant values for getAllowedValueStep().
static const double dateDefaultStep = 1.0;
static const double dateStepScaleFactor = 86400000.0;
static const double dateTimeDefaultStep = 60.0;
static const double dateTimeStepScaleFactor = 1000.0;
static const double monthDefaultStep = 1.0;
static const double monthStepScaleFactor = 1.0;
static const double numberDefaultStep = 1.0;
static const double numberStepScaleFactor = 1.0;
static const double timeDefaultStep = 60.0;
static const double timeStepScaleFactor = 1000.0;
static const double weekDefaultStep = 1.0;
static const double weekStepScaleFactor = 604800000.0;

// Constant values for minimum().
static const double numberDefaultMinimum = -DBL_MAX;
static const double rangeDefaultMinimum = 0.0;

// Constant values for maximum().
static const double numberDefaultMaximum = DBL_MAX;
static const double rangeDefaultMaximum = 100.0;

static const double defaultStepBase = 0.0;
static const double weekDefaultStepBase = -259200000.0; // The first day of 1970-W01.

static const double msecPerMinute = 60 * 1000;
static const double msecPerSecond = 1000;

static const char emailPattern[] =
    "[a-z0-9!#$%&'*+/=?^_`{|}~.-]+" // local part
    "@"
    "[a-z0-9-]+(\\.[a-z0-9-]+)+"; // domain part

static bool isNumberCharacter(UChar ch)
{
    return ch == '+' || ch == '-' || ch == '.' || ch == 'e' || ch == 'E'
        || (ch >= '0' && ch <= '9');
}

static bool isValidColorString(const String& value)
{
    if (value.isEmpty())
        return false;
    if (value[0] == '#') {
        // We don't accept #rgb and #aarrggbb formats.
        if (value.length() != 7)
            return false;
    }
    Color color(value); // This accepts named colors such as "white".
    return color.isValid() && !color.hasAlpha();
}

static bool isValidEmailAddress(const String& address)
{
    int addressLength = address.length();
    if (!addressLength)
        return false;

    DEFINE_STATIC_LOCAL(const RegularExpression, regExp, (emailPattern, TextCaseInsensitive));

    int matchLength;
    int matchOffset = regExp.match(address, 0, &matchLength);

    return !matchOffset && matchLength == addressLength;
}

HTMLInputElement::HTMLInputElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
    : HTMLTextFormControlElement(tagName, document, form)
    , m_xPos(0)
    , m_yPos(0)
    , m_maxResults(-1)
    , m_type(TEXT)
    , m_checked(false)
    , m_defaultChecked(false)
    , m_useDefaultChecked(true)
    , m_indeterminate(false)
    , m_haveType(false)
    , m_activeSubmit(false)
    , m_autocomplete(Uninitialized)
    , m_autofilled(false)
    , m_inited(false)
{
    ASSERT(hasTagName(inputTag) || hasTagName(isindexTag));
}

PassRefPtr<HTMLInputElement> HTMLInputElement::create(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
{
    return adoptRef(new HTMLInputElement(tagName, document, form));
}

HTMLInputElement::~HTMLInputElement()
{
    if (needsActivationCallback())
        document()->unregisterForDocumentActivationCallbacks(this);

    document()->checkedRadioButtons().removeButton(this);

    // Need to remove this from the form while it is still an HTMLInputElement,
    // so can't wait for the base class's destructor to do it.
    removeFromForm();
}

const AtomicString& HTMLInputElement::formControlName() const
{
    return m_data.name();
}

bool HTMLInputElement::autoComplete() const
{
    if (m_autocomplete != Uninitialized)
        return m_autocomplete == On;
    return HTMLTextFormControlElement::autoComplete();
}

static inline CheckedRadioButtons& checkedRadioButtons(const HTMLInputElement* element)
{
    if (HTMLFormElement* form = element->form())
        return form->checkedRadioButtons();
    return element->document()->checkedRadioButtons();
}

void HTMLInputElement::updateCheckedRadioButtons()
{
    if (attached() && checked())
        checkedRadioButtons(this).addButton(this);

    if (form()) {
        const Vector<HTMLFormControlElement*>& controls = form()->associatedElements();
        for (unsigned i = 0; i < controls.size(); ++i) {
            HTMLFormControlElement* control = controls[i];
            if (control->name() != name())
                continue;
            if (control->type() != type())
                continue;
            control->setNeedsValidityCheck();
        }
    } else {
        // FIXME: Traversing the document is inefficient.
        for (Node* node = document()->body(); node; node = node->traverseNextNode()) {
            if (!node->isElementNode())
                continue;
            Element* element = static_cast<Element*>(node);
            if (element->formControlName() != name())
                continue;
            if (element->formControlType() != type())
                continue;
            HTMLFormControlElement* control = static_cast<HTMLFormControlElement*>(element);
            if (control->form())
                continue;
            control->setNeedsValidityCheck();
        }
    }
   
    if (renderer() && renderer()->style()->hasAppearance())
        renderer()->theme()->stateChanged(renderer(), CheckedState);
}

bool HTMLInputElement::isValidValue(const String& value) const
{
    // Should not call isValidValue() for the following types because
    // we can't set string values for these types.
    if (inputType() == CHECKBOX || inputType() == FILE || inputType() == RADIO) {
        ASSERT_NOT_REACHED();
        return false;
    }
    return !typeMismatch(value)
        && !stepMismatch(value)
        && !rangeUnderflow(value)
        && !rangeOverflow(value)
        && !tooLong(value, IgnoreDirtyFlag)
        && !patternMismatch(value)
        && !valueMissing(value);
}

bool HTMLInputElement::typeMismatch(const String& value) const
{
    switch (inputType()) {
    case COLOR:
        return !isValidColorString(value);
    case NUMBER:
        ASSERT(parseToDoubleForNumberType(value, 0));
        return false;
    case URL:
        return !KURL(KURL(), value).isValid();
    case EMAIL: {
        if (!multiple())
            return !isValidEmailAddress(value);
        Vector<String> addresses;
        value.split(',', addresses);
        for (unsigned i = 0; i < addresses.size(); ++i) {
            if (!isValidEmailAddress(addresses[i]))
                return true;
        }
        return false;
    }
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case TIME:
    case WEEK:
        return !parseToDateComponents(inputType(), value, 0);
    case BUTTON:
    case CHECKBOX:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RANGE:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool HTMLInputElement::valueMissing(const String& value) const
{
    if (!isRequiredFormControl() || readOnly() || disabled())
        return false;

    switch (inputType()) {
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case FILE:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        return value.isEmpty();
    case CHECKBOX:
        return !checked();
    case RADIO:
        return !checkedRadioButtons(this).checkedButtonForGroup(name());
    case COLOR:
        return false;
    case BUTTON:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case RANGE:
    case RESET:
    case SUBMIT:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool HTMLInputElement::patternMismatch(const String& value) const
{
    if (!isTextType())
        return false;
    const AtomicString& pattern = getAttribute(patternAttr);
    // Empty values can't be mismatched
    if (pattern.isEmpty() || value.isEmpty())
        return false;
    RegularExpression patternRegExp(pattern, TextCaseSensitive);
    int matchLength = 0;
    int valueLength = value.length();
    int matchOffset = patternRegExp.match(value, 0, &matchLength);
    return matchOffset || matchLength != valueLength;
}

bool HTMLInputElement::tooLong(const String& value, NeedsToCheckDirtyFlag check) const
{
    // We use isTextType() instead of supportsMaxLength() because of the
    // 'virtual' overhead.
    if (!isTextType())
        return false;
    int max = maxLength();
    if (max < 0)
        return false;
    if (check == CheckDirtyFlag) {
        // Return false for the default value even if it is longer than maxLength.
        bool userEdited = !m_data.value().isNull();
        if (!userEdited)
            return false;
    }
    return numGraphemeClusters(value) > static_cast<unsigned>(max);
}

bool HTMLInputElement::rangeUnderflow(const String& value) const
{
    const double nan = numeric_limits<double>::quiet_NaN();
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case NUMBER:
    case TIME:
    case WEEK: {
        double doubleValue = parseToDouble(value, nan);
        return isfinite(doubleValue) && doubleValue < minimum();
    }
    case RANGE: // Guaranteed by sanitization.
        ASSERT(parseToDouble(value, nan) >= minimum());
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        break;
    }
    return false;
}

bool HTMLInputElement::rangeOverflow(const String& value) const
{
    const double nan = numeric_limits<double>::quiet_NaN();
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case NUMBER:
    case TIME:
    case WEEK: {
        double doubleValue = parseToDouble(value, nan);
        return isfinite(doubleValue) && doubleValue > maximum();
    }
    case RANGE: // Guaranteed by sanitization.
        ASSERT(parseToDouble(value, nan) <= maximum());
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        break;
    }
    return false;
}

double HTMLInputElement::minimum() const
{
    switch (inputType()) {
    case DATE:
        return parseToDouble(getAttribute(minAttr), DateComponents::minimumDate());
    case DATETIME:
    case DATETIMELOCAL:
        return parseToDouble(getAttribute(minAttr), DateComponents::minimumDateTime());
    case MONTH:
        return parseToDouble(getAttribute(minAttr), DateComponents::minimumMonth());
    case NUMBER:
        return parseToDouble(getAttribute(minAttr), numberDefaultMinimum);
    case RANGE:
        return parseToDouble(getAttribute(minAttr), rangeDefaultMinimum);
    case TIME:
        return parseToDouble(getAttribute(minAttr), DateComponents::minimumTime());
    case WEEK:
        return parseToDouble(getAttribute(minAttr), DateComponents::minimumWeek());
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

double HTMLInputElement::maximum() const
{
    switch (inputType()) {
    case DATE:
        return parseToDouble(getAttribute(maxAttr), DateComponents::maximumDate());
    case DATETIME:
    case DATETIMELOCAL:
        return parseToDouble(getAttribute(maxAttr), DateComponents::maximumDateTime());
    case MONTH:
        return parseToDouble(getAttribute(maxAttr), DateComponents::maximumMonth());
    case NUMBER:
        return parseToDouble(getAttribute(maxAttr), numberDefaultMaximum);
    case RANGE: {
        double max = parseToDouble(getAttribute(maxAttr), rangeDefaultMaximum);
        // A remedy for the inconsistent min/max values for RANGE.
        // Sets the maximum to the default or the minimum value.
        double min = minimum();
        if (max < min)
            max = std::max(min, rangeDefaultMaximum);
        return max;
    }
    case TIME:
        return parseToDouble(getAttribute(maxAttr), DateComponents::maximumTime());
    case WEEK:
        return parseToDouble(getAttribute(maxAttr), DateComponents::maximumWeek());
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

double HTMLInputElement::stepBase() const
{
    switch (inputType()) {
    case RANGE:
        return minimum();
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case NUMBER:
    case TIME:
        return parseToDouble(getAttribute(minAttr), defaultStepBase);
    case WEEK:
        return parseToDouble(getAttribute(minAttr), weekDefaultStepBase);
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0.0;
}

bool HTMLInputElement::stepMismatch(const String& value) const
{
    double step;
    if (!getAllowedValueStep(&step))
        return false;
    switch (inputType()) {
    case RANGE:
        // stepMismatch doesn't occur for RANGE. RenderSlider guarantees the
        // value matches to step on user input, and sanitation takes care
        // of the general case.
        return false;
    case NUMBER: {
        double doubleValue;
        if (!parseToDoubleForNumberType(value, &doubleValue))
            return false;
        doubleValue = fabs(doubleValue - stepBase());
        if (isinf(doubleValue))
            return false;
        // double's fractional part size is DBL_MAN_DIG-bit.  If the current
        // value is greater than step*2^DBL_MANT_DIG, the following fmod() makes
        // no sense.
        if (doubleValue / pow(2.0, DBL_MANT_DIG) > step)
            return false;
        double remainder = fmod(doubleValue, step);
        // Accepts errors in lower 7-bit.
        double acceptableError = step / pow(2.0, DBL_MANT_DIG - 7);
        return acceptableError < remainder && remainder < (step - acceptableError);
    }
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case TIME:
    case WEEK: {
        const double nan = numeric_limits<double>::quiet_NaN();
        double doubleValue = parseToDouble(value, nan);
        doubleValue = fabs(doubleValue - stepBase());
        if (!isfinite(doubleValue))
            return false;
        ASSERT(round(doubleValue) == doubleValue);
        ASSERT(round(step) == step);
        return fmod(doubleValue, step);
    }
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        break;
    }
    // Non-supported types should be rejected by getAllowedValueStep().
    ASSERT_NOT_REACHED();
    return false;
}

bool HTMLInputElement::getStepParameters(double* defaultStep, double* stepScaleFactor) const
{
    ASSERT(defaultStep);
    ASSERT(stepScaleFactor);
    switch (inputType()) {
    case NUMBER:
    case RANGE:
        *defaultStep = numberDefaultStep;
        *stepScaleFactor = numberStepScaleFactor;
        return true;
    case DATE:
        *defaultStep = dateDefaultStep;
        *stepScaleFactor = dateStepScaleFactor;
        return true;
    case DATETIME:
    case DATETIMELOCAL:
        *defaultStep = dateTimeDefaultStep;
        *stepScaleFactor = dateTimeStepScaleFactor;
        return true;
    case MONTH:
        *defaultStep = monthDefaultStep;
        *stepScaleFactor = monthStepScaleFactor;
        return true;
    case TIME:
        *defaultStep = timeDefaultStep;
        *stepScaleFactor = timeStepScaleFactor;
        return true;
    case WEEK:
        *defaultStep = weekDefaultStep;
        *stepScaleFactor = weekStepScaleFactor;
        return true;
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool HTMLInputElement::getAllowedValueStep(double* step) const
{
    ASSERT(step);
    double defaultStep;
    double stepScaleFactor;
    if (!getStepParameters(&defaultStep, &stepScaleFactor))
        return false;
    const AtomicString& stepString = getAttribute(stepAttr);
    if (stepString.isEmpty()) {
        *step = defaultStep * stepScaleFactor;
        return true;
    }
    if (equalIgnoringCase(stepString, "any"))
        return false;
    double parsed;
    if (!parseToDoubleForNumberType(stepString, &parsed) || parsed <= 0.0) {
        *step = defaultStep * stepScaleFactor;
        return true;
    }
    // For DATE, MONTH, WEEK, the parsed value should be an integer.
    if (inputType() == DATE || inputType() == MONTH || inputType() == WEEK)
        parsed = max(round(parsed), 1.0);
    double result = parsed * stepScaleFactor;
    // For DATETIME, DATETIMELOCAL, TIME, the result should be an integer.
    if (inputType() == DATETIME || inputType() == DATETIMELOCAL || inputType() == TIME)
        result = max(round(result), 1.0);
    ASSERT(result > 0);
    *step = result;
    return true;
}

void HTMLInputElement::applyStep(double count, ExceptionCode& ec)
{
    double step;
    if (!getAllowedValueStep(&step)) {
        ec = INVALID_STATE_ERR;
        return;
    }
    const double nan = numeric_limits<double>::quiet_NaN();
    double current = parseToDouble(value(), nan);
    if (!isfinite(current)) {
        ec = INVALID_STATE_ERR;
        return;
    }
    double newValue = current + step * count;
    if (isinf(newValue)) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (newValue < minimum()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    double base = stepBase();
    newValue = base + round((newValue - base) / step) * step;
    if (newValue > maximum()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    setValueAsNumber(newValue, ec);
}

void HTMLInputElement::stepUp(int n, ExceptionCode& ec)
{
    applyStep(n, ec);
}

void HTMLInputElement::stepDown(int n, ExceptionCode& ec)
{
    applyStep(-n, ec);
}

bool HTMLInputElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    // If text fields can be focused, then they should always be keyboard focusable
    if (isTextField())
        return HTMLFormControlElementWithState::isFocusable();
        
    // If the base class says we can't be focused, then we can stop now.
    if (!HTMLFormControlElementWithState::isKeyboardFocusable(event))
        return false;

    if (inputType() == RADIO) {

        // Never allow keyboard tabbing to leave you in the same radio group.  Always
        // skip any other elements in the group.
        Node* currentFocusedNode = document()->focusedNode();
        if (currentFocusedNode && currentFocusedNode->hasTagName(inputTag)) {
            HTMLInputElement* focusedInput = static_cast<HTMLInputElement*>(currentFocusedNode);
            if (focusedInput->inputType() == RADIO && focusedInput->form() == form() && focusedInput->name() == name())
                return false;
        }
        
        // Allow keyboard focus if we're checked or if nothing in the group is checked.
        return checked() || !checkedRadioButtons(this).checkedButtonForGroup(name());
    }
    
    return true;
}

bool HTMLInputElement::isMouseFocusable() const
{
    if (isTextField())
        return HTMLFormControlElementWithState::isFocusable();
    return HTMLFormControlElementWithState::isMouseFocusable();
}

void HTMLInputElement::updateFocusAppearance(bool restorePreviousSelection)
{        
    if (isTextField())
        InputElement::updateFocusAppearance(m_data, this, this, restorePreviousSelection);
    else
        HTMLFormControlElementWithState::updateFocusAppearance(restorePreviousSelection);
}

void HTMLInputElement::aboutToUnload()
{
    InputElement::aboutToUnload(this, this);
}

bool HTMLInputElement::shouldUseInputMethod() const
{
    // The reason IME's are disabled for the password field is because IMEs 
    // can access the underlying password and display it in clear text --
    // e.g. you can use it to access the stored password for any site 
    // with only trivial effort.
    return isTextField() && inputType() != PASSWORD;
}

void HTMLInputElement::handleFocusEvent()
{
    InputElement::dispatchFocusEvent(this, this);
}

void HTMLInputElement::handleBlurEvent()
{
    if (inputType() == NUMBER) {
        // Reset the renderer value, which might be unmatched with the element value.
        setFormControlValueMatchesRenderer(false);
        // We need to reset the renderer value explicitly because an unacceptable
        // renderer value should be purged before style calculation.
        if (renderer())
            renderer()->updateFromElement();
    }
    InputElement::dispatchBlurEvent(this, this);
}

void HTMLInputElement::setType(const String& t)
{
    if (t.isEmpty()) {
        int exccode;
        removeAttribute(typeAttr, exccode);
    } else
        setAttribute(typeAttr, t);
}

typedef HashMap<String, HTMLInputElement::InputType, CaseFoldingHash> InputTypeMap;
static PassOwnPtr<InputTypeMap> createTypeMap()
{
    OwnPtr<InputTypeMap> map = adoptPtr(new InputTypeMap);
    map->add("button", HTMLInputElement::BUTTON);
    map->add("checkbox", HTMLInputElement::CHECKBOX);
    map->add("color", HTMLInputElement::COLOR);
    map->add("date", HTMLInputElement::DATE);
    map->add("datetime", HTMLInputElement::DATETIME);
    map->add("datetime-local", HTMLInputElement::DATETIMELOCAL);
    map->add("email", HTMLInputElement::EMAIL);
    map->add("file", HTMLInputElement::FILE);
    map->add("hidden", HTMLInputElement::HIDDEN);
    map->add("image", HTMLInputElement::IMAGE);
    map->add("khtml_isindex", HTMLInputElement::ISINDEX);
    map->add("month", HTMLInputElement::MONTH);
    map->add("number", HTMLInputElement::NUMBER);
    map->add("password", HTMLInputElement::PASSWORD);
    map->add("radio", HTMLInputElement::RADIO);
    map->add("range", HTMLInputElement::RANGE);
    map->add("reset", HTMLInputElement::RESET);
    map->add("search", HTMLInputElement::SEARCH);
    map->add("submit", HTMLInputElement::SUBMIT);
    map->add("tel", HTMLInputElement::TELEPHONE);
    map->add("time", HTMLInputElement::TIME);
    map->add("url", HTMLInputElement::URL);
    map->add("week", HTMLInputElement::WEEK);
    // No need to register "text" because it is the default type.
    return map.release();
}

void HTMLInputElement::setInputType(const String& t)
{
    static const InputTypeMap* typeMap = createTypeMap().leakPtr();
    InputType newType = t.isNull() ? TEXT : typeMap->get(t);

    // IMPORTANT: Don't allow the type to be changed to FILE after the first
    // type change, otherwise a JavaScript programmer would be able to set a text
    // field's value to something like /etc/passwd and then change it to a file field.
    if (inputType() != newType) {
        if (newType == FILE && m_haveType)
            // Set the attribute back to the old value.
            // Useful in case we were called from inside parseMappedAttribute.
            setAttribute(typeAttr, type());
        else {
            checkedRadioButtons(this).removeButton(this);

            if (newType == FILE && !m_fileList)
                m_fileList = FileList::create();

            bool wasAttached = attached();
            if (wasAttached)
                detach();

            bool didStoreValue = storesValueSeparateFromAttribute();
            bool wasPasswordField = inputType() == PASSWORD;
            bool didRespectHeightAndWidth = respectHeightAndWidthAttrs();
            m_type = newType;
            setNeedsWillValidateCheck();
            bool willStoreValue = storesValueSeparateFromAttribute();
            bool isPasswordField = inputType() == PASSWORD;
            bool willRespectHeightAndWidth = respectHeightAndWidthAttrs();

            if (didStoreValue && !willStoreValue && !m_data.value().isNull()) {
                setAttribute(valueAttr, m_data.value());
                m_data.setValue(String());
            }
            if (!didStoreValue && willStoreValue)
                m_data.setValue(sanitizeValue(getAttribute(valueAttr)));
            else
                InputElement::updateValueIfNeeded(m_data, this);

            if (wasPasswordField && !isPasswordField)
                unregisterForActivationCallbackIfNeeded();
            else if (!wasPasswordField && isPasswordField)
                registerForActivationCallbackIfNeeded();

            if (didRespectHeightAndWidth != willRespectHeightAndWidth) {
                NamedNodeMap* map = attributeMap();
                ASSERT(map);
                if (Attribute* height = map->getAttributeItem(heightAttr))
                    attributeChanged(height, false);
                if (Attribute* width = map->getAttributeItem(widthAttr))
                    attributeChanged(width, false);
                if (Attribute* align = map->getAttributeItem(alignAttr))
                    attributeChanged(align, false);
            }

            if (wasAttached) {
                attach();
                if (document()->focusedNode() == this)
                    updateFocusAppearance(true);
            }

            checkedRadioButtons(this).addButton(this);
        }

        setNeedsValidityCheck();
        InputElement::notifyFormStateChanged(this);
    }
    m_haveType = true;

    if (inputType() != IMAGE && m_imageLoader)
        m_imageLoader.clear();
}

static const AtomicString* createFormControlTypes()
{
    AtomicString* types = new AtomicString[HTMLInputElement::numberOfTypes];
    // The values must be lowercased because they will be the return values of
    //  input.type and it must be lowercase according to DOM Level 2.
    types[HTMLInputElement::BUTTON] = "button";
    types[HTMLInputElement::CHECKBOX] = "checkbox";
    types[HTMLInputElement::COLOR] = "color";
    types[HTMLInputElement::DATE] = "date";
    types[HTMLInputElement::DATETIME] = "datetime";
    types[HTMLInputElement::DATETIMELOCAL] = "datetime-local";
    types[HTMLInputElement::EMAIL] = "email";
    types[HTMLInputElement::FILE] = "file";
    types[HTMLInputElement::HIDDEN] = "hidden";
    types[HTMLInputElement::IMAGE] = "image";
    types[HTMLInputElement::ISINDEX] = emptyAtom;
    types[HTMLInputElement::MONTH] = "month";
    types[HTMLInputElement::NUMBER] = "number";
    types[HTMLInputElement::PASSWORD] = "password";
    types[HTMLInputElement::RADIO] = "radio";
    types[HTMLInputElement::RANGE] = "range";
    types[HTMLInputElement::RESET] = "reset";
    types[HTMLInputElement::SEARCH] = "search";
    types[HTMLInputElement::SUBMIT] = "submit";
    types[HTMLInputElement::TELEPHONE] = "tel";
    types[HTMLInputElement::TEXT] = "text";
    types[HTMLInputElement::TIME] = "time";
    types[HTMLInputElement::URL] = "url";
    types[HTMLInputElement::WEEK] = "week";
    return types;
}

const AtomicString& HTMLInputElement::formControlType() const
{
    static const AtomicString* formControlTypes = createFormControlTypes();
    return formControlTypes[inputType()];
}

bool HTMLInputElement::saveFormControlState(String& result) const
{
    switch (inputType()) {
    case BUTTON:
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case RANGE:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK: {
        String currentValue = value();
        if (currentValue == defaultValue())
            return false;
        result = currentValue;
        return true;
    }
    case CHECKBOX:
    case RADIO:
        result = checked() ? "on" : "off";
        return true;
    case PASSWORD:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void HTMLInputElement::restoreFormControlState(const String& state)
{
    ASSERT(inputType() != PASSWORD); // should never save/restore password fields
    switch (inputType()) {
    case BUTTON:
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case RANGE:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        setValue(state);
        break;
    case CHECKBOX:
    case RADIO:
        setChecked(state == "on");
        break;
    case PASSWORD:
        break;
    }
}

bool HTMLInputElement::canStartSelection() const
{
    if (!isTextField())
        return false;
    return HTMLFormControlElementWithState::canStartSelection();
}

bool HTMLInputElement::canHaveSelection() const
{
    return isTextField();
}

void HTMLInputElement::accessKeyAction(bool sendToAnyElement)
{
    switch (inputType()) {
    case BUTTON:
    case CHECKBOX:
    case FILE:
    case IMAGE:
    case RADIO:
    case RANGE:
    case RESET:
    case SUBMIT:
        focus(false);
        // send the mouse button events iff the caller specified sendToAnyElement
        dispatchSimulatedClick(0, sendToAnyElement);
        break;
    case HIDDEN:
        // a no-op for this type
        break;
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        // should never restore previous selection here
        focus(false);
         break;
    }
}

bool HTMLInputElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (((attrName == heightAttr || attrName == widthAttr) && respectHeightAndWidthAttrs())
        || attrName == vspaceAttr 
        || attrName == hspaceAttr) {
        result = eUniversal;
        return false;
    } 

    if (attrName == alignAttr) {
        if (inputType() == IMAGE) {
            // Share with <img> since the alignment behavior is the same.
            result = eReplaced;
            return false;
        }
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLInputElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == nameAttr) {
        checkedRadioButtons(this).removeButton(this);
        m_data.setName(attr->value());
        checkedRadioButtons(this).addButton(this);
        HTMLFormControlElementWithState::parseMappedAttribute(attr);
    } else if (attr->name() == autocompleteAttr) {
        if (equalIgnoringCase(attr->value(), "off")) {
            m_autocomplete = Off;
            registerForActivationCallbackIfNeeded();
        } else {
            bool needsToUnregister = m_autocomplete == Off;

            if (attr->isEmpty())
                m_autocomplete = Uninitialized;
            else
                m_autocomplete = On;

            if (needsToUnregister)
                unregisterForActivationCallbackIfNeeded();
        }
    } else if (attr->name() == typeAttr) {
        setInputType(attr->value());
    } else if (attr->name() == valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (m_data.value().isNull())
            setNeedsStyleRecalc();
        setFormControlValueMatchesRenderer(false);
        setNeedsValidityCheck();
    } else if (attr->name() == checkedAttr) {
        m_defaultChecked = !attr->isNull();
        if (m_useDefaultChecked) {
            setChecked(m_defaultChecked);
            m_useDefaultChecked = true;
        }
        setNeedsValidityCheck();
    } else if (attr->name() == maxlengthAttr) {
        InputElement::parseMaxLengthAttribute(m_data, this, this, attr);
        setNeedsValidityCheck();
    } else if (attr->name() == sizeAttr)
        InputElement::parseSizeAttribute(m_data, this, attr);
    else if (attr->name() == altAttr) {
        if (renderer() && inputType() == IMAGE)
            toRenderImage(renderer())->updateAltText();
    } else if (attr->name() == srcAttr) {
        if (renderer() && inputType() == IMAGE) {
            if (!m_imageLoader)
                m_imageLoader = adoptPtr(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
        }
    } else if (attr->name() == usemapAttr || attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSSPropertyMarginTop, attr->value());
        addCSSLength(attr, CSSPropertyMarginBottom, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSSPropertyMarginLeft, attr->value());
        addCSSLength(attr, CSSPropertyMarginRight, attr->value());
    } else if (attr->name() == alignAttr) {
        if (inputType() == IMAGE)
            addHTMLAlignment(attr);
    } else if (attr->name() == widthAttr) {
        if (respectHeightAndWidthAttrs())
            addCSSLength(attr, CSSPropertyWidth, attr->value());
    } else if (attr->name() == heightAttr) {
        if (respectHeightAndWidthAttrs())
            addCSSLength(attr, CSSPropertyHeight, attr->value());
    } else if (attr->name() == onsearchAttr) {
        // Search field and slider attributes all just cause updateFromElement to be called through style recalcing.
        setAttributeEventListener(eventNames().searchEvent, createAttributeEventListener(this, attr));
    } else if (attr->name() == resultsAttr) {
        int oldResults = m_maxResults;
        m_maxResults = !attr->isNull() ? std::min(attr->value().toInt(), maxSavedResults) : -1;
        // FIXME: Detaching just for maxResults change is not ideal.  We should figure out the right
        // time to relayout for this change.
        if (m_maxResults != oldResults && (m_maxResults <= 0 || oldResults <= 0) && attached()) {
            detach();
            attach();
        }
        setNeedsStyleRecalc();
    } else if (attr->name() == autosaveAttr
               || attr->name() == incrementalAttr)
        setNeedsStyleRecalc();
    else if (attr->name() == minAttr
             || attr->name() == maxAttr) {
        if (inputType() == RANGE) {
            // Sanitize the value.
            setValue(value());
            setNeedsStyleRecalc();
        }
        setNeedsValidityCheck();
    } else if (attr->name() == multipleAttr
             || attr->name() == patternAttr
             || attr->name() == precisionAttr
             || attr->name() == stepAttr)
        setNeedsValidityCheck();
#if ENABLE(DATALIST)
    else if (attr->name() == listAttr)
        m_hasNonEmptyList = !attr->isEmpty();
        // FIXME: we need to tell this change to a renderer if the attribute affects the appearance.
#endif
#if ENABLE(INPUT_SPEECH)
    else if (attr->name() == speechAttr) {
      if (renderer())
          renderer()->updateFromElement();
      setNeedsStyleRecalc();
    }
#endif
    else
        HTMLTextFormControlElement::parseMappedAttribute(attr);
}

bool HTMLInputElement::rendererIsNeeded(RenderStyle *style)
{
    if (inputType() == HIDDEN)
        return false;
    return HTMLFormControlElementWithState::rendererIsNeeded(style);
}

RenderObject* HTMLInputElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    switch (inputType()) {
    case BUTTON:
    case RESET:
    case SUBMIT:
        return new (arena) RenderButton(this);
    case CHECKBOX:
    case RADIO:
        return RenderObject::createObject(this, style);
    case FILE:
        return new (arena) RenderFileUploadControl(this);
    case HIDDEN:
        break;
    case IMAGE: {
        RenderImage* image = new (arena) RenderImage(this);
        image->setImageResource(RenderImageResource::create());
        return image;
    }
    case RANGE:
        return new (arena) RenderSlider(this);
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        return new (arena) RenderTextControlSingleLine(this, placeholderShouldBeVisible());
    }
    ASSERT(false);
    return 0;
}

void HTMLInputElement::attach()
{
    if (!m_inited) {
        if (!m_haveType)
            setInputType(getAttribute(typeAttr));
        m_inited = true;
    }

    HTMLFormControlElementWithState::attach();

    if (inputType() == IMAGE) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer() && m_imageLoader->haveFiredBeforeLoadEvent()) {
            RenderImage* renderImage = toRenderImage(renderer());
            RenderImageResource* renderImageResource = renderImage->imageResource();
            renderImageResource->setCachedImage(m_imageLoader->image()); 

            // If we have no image at all because we have no src attribute, set
            // image height and width for the alt text instead.
            if (!m_imageLoader->image() && !renderImageResource->cachedImage())
                renderImage->setImageSizeForAltText();
        }
    }

    if (inputType() == RADIO)
        updateCheckedRadioButtons();

    if (document()->focusedNode() == this)
        document()->updateFocusAppearanceSoon(true /* restore selection */);
}

void HTMLInputElement::detach()
{
    HTMLFormControlElementWithState::detach();
    setFormControlValueMatchesRenderer(false);
}

String HTMLInputElement::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElement::altText()
    String alt = getAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    if (alt.isNull())
        alt = getAttribute(valueAttr);
    if (alt.isEmpty())
        alt = inputElementAltText();
    return alt;
}

bool HTMLInputElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint. So we do likewise.
    return !disabled() && (inputType() == IMAGE || inputType() == SUBMIT);
}

bool HTMLInputElement::isActivatedSubmit() const
{
    return m_activeSubmit;
}

void HTMLInputElement::setActivatedSubmit(bool flag)
{
    m_activeSubmit = flag;
}

bool HTMLInputElement::appendFormData(FormDataList& encoding, bool multipart)
{
    // image generates its own names, but for other types there is no form data unless there's a name
    if (name().isEmpty() && inputType() != IMAGE)
        return false;

    switch (inputType()) {
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case HIDDEN:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case RANGE:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        // always successful
        encoding.appendData(name(), value());
        return true;

    case CHECKBOX:
    case RADIO:
        if (checked()) {
            encoding.appendData(name(), value());
            return true;
        }
        break;

    case BUTTON:
    case RESET:
        // these types of buttons are never successful
        return false;

    case IMAGE:
        if (m_activeSubmit) {
            encoding.appendData(name().isEmpty() ? "x" : (name() + ".x"), m_xPos);
            encoding.appendData(name().isEmpty() ? "y" : (name() + ".y"), m_yPos);
            if (!name().isEmpty() && !value().isEmpty())
                encoding.appendData(name(), value());
            return true;
        }
        break;

    case SUBMIT:
        if (m_activeSubmit) {
            String encstr = valueWithDefault();
            encoding.appendData(name(), encstr);
            return true;
        }
        break;

    case FILE: {
        unsigned numFiles = m_fileList->length();
        if (!multipart) {
            // Send only the basenames.
            // 4.10.16.4 and 4.10.16.6 sections in HTML5.
    
            // Unlike the multipart case, we have no special
            // handling for the empty fileList because Netscape
            // doesn't support for non-multipart submission of
            // file inputs, and Firefox doesn't add "name=" query
            // parameter.

            for (unsigned i = 0; i < numFiles; ++i) 
                encoding.appendData(name(), m_fileList->item(i)->fileName());
            return true;
        }

        // If no filename at all is entered, return successful but empty.
        // Null would be more logical, but Netscape posts an empty file. Argh.
        if (!numFiles) {
            encoding.appendBlob(name(), File::create(""));
            return true;
        }

        for (unsigned i = 0; i < numFiles; ++i)
            encoding.appendBlob(name(), m_fileList->item(i));
        return true;
        }
    }
    return false;
}

void HTMLInputElement::reset()
{
    if (storesValueSeparateFromAttribute())
        setValue(String());

    setChecked(m_defaultChecked);
    m_useDefaultChecked = true;
}

bool HTMLInputElement::isTextField() const
{
    switch (inputType()) {
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        return true;
    case BUTTON:
    case CHECKBOX:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case RADIO:
    case RANGE:
    case RESET:
    case SUBMIT:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool HTMLInputElement::isTextType() const
{
    switch (inputType()) {
    case EMAIL:
    case PASSWORD:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case URL:
        return true;
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case RADIO:
    case RANGE:
    case RESET:
    case SUBMIT:
    case TIME:
    case WEEK:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void HTMLInputElement::setChecked(bool nowChecked, bool sendChangeEvent)
{
    if (checked() == nowChecked)
        return;

    checkedRadioButtons(this).removeButton(this);

    m_useDefaultChecked = false;
    m_checked = nowChecked;
    setNeedsStyleRecalc();

    updateCheckedRadioButtons();

    // Ideally we'd do this from the render tree (matching
    // RenderTextView), but it's not possible to do it at the moment
    // because of the way the code is structured.
    if (renderer() && AXObjectCache::accessibilityEnabled())
        renderer()->document()->axObjectCache()->postNotification(renderer(), AXObjectCache::AXCheckedStateChanged, true);

    // Only send a change event for items in the document (avoid firing during
    // parsing) and don't send a change event for a radio button that's getting
    // unchecked to match other browsers. DOM is not a useful standard for this
    // because it says only to fire change events at "lose focus" time, which is
    // definitely wrong in practice for these types of elements.
    if (sendChangeEvent && inDocument() && (inputType() != RADIO || nowChecked))
        dispatchFormControlChangeEvent();
}

void HTMLInputElement::setIndeterminate(bool newValue)
{
    // Only checkboxes and radio buttons honor indeterminate.
    if (!allowsIndeterminate() || indeterminate() == newValue)
        return;

    m_indeterminate = newValue;

    setNeedsStyleRecalc();

    if (renderer() && renderer()->style()->hasAppearance())
        renderer()->theme()->stateChanged(renderer(), CheckedState);
}

int HTMLInputElement::size() const
{
    return m_data.size();
}

void HTMLInputElement::copyNonAttributeProperties(const Element* source)
{
    const HTMLInputElement* sourceElement = static_cast<const HTMLInputElement*>(source);

    m_data.setValue(sourceElement->m_data.value());
    setChecked(sourceElement->m_checked);
    m_defaultChecked = sourceElement->m_defaultChecked;
    m_useDefaultChecked = sourceElement->m_useDefaultChecked;
    m_indeterminate = sourceElement->m_indeterminate;

    HTMLFormControlElementWithState::copyNonAttributeProperties(source);
}

String HTMLInputElement::value() const
{
    if (inputType() == FILE) {
        if (!m_fileList->isEmpty()) {
            // HTML5 tells us that we're supposed to use this goofy value for
            // file input controls.  Historically, browsers reveals the real
            // file path, but that's a privacy problem.  Code on the web
            // decided to try to parse the value by looking for backslashes
            // (because that's what Windows file paths use).  To be compatible
            // with that code, we make up a fake path for the file.
            return "C:\\fakepath\\" + m_fileList->item(0)->fileName();
        }
        return String();
    }

    String value = m_data.value();
    if (value.isNull()) {
        value = sanitizeValue(fastGetAttribute(valueAttr));
        
        // If no attribute exists, extra handling may be necessary.
        // For Checkbox Types just use "on" or "" based off the checked() state of the control.
        // For a Range Input use the calculated default value.
        if (value.isNull()) {
            if (inputType() == CHECKBOX || inputType() == RADIO)
                return checked() ? "on" : "";
            if (inputType() == RANGE)
                return serializeForNumberType(StepRange(this).defaultValue());
        }
    }

    return value;
}

String HTMLInputElement::valueWithDefault() const
{
    String v = value();
    if (v.isNull()) {
        switch (inputType()) {
        case BUTTON:
        case CHECKBOX:
        case COLOR:
        case DATE:
        case DATETIME:
        case DATETIMELOCAL:
        case EMAIL:
        case FILE:
        case HIDDEN:
        case IMAGE:
        case ISINDEX:
        case MONTH:
        case NUMBER:
        case PASSWORD:
        case RADIO:
        case RANGE:
        case SEARCH:
        case TELEPHONE:
        case TEXT:
        case TIME:
        case URL:
        case WEEK:
            break;
        case RESET:
            v = resetButtonDefaultLabel();
            break;
        case SUBMIT:
            v = submitButtonDefaultLabel();
            break;
        }
    }
    return v;
}

void HTMLInputElement::setValueForUser(const String& value)
{
    // Call setValue and make it send a change event.
    setValue(value, true);
}

const String& HTMLInputElement::suggestedValue() const
{
    return m_data.suggestedValue();
}

void HTMLInputElement::setSuggestedValue(const String& value)
{
    if (inputType() != TEXT)
        return;
    setFormControlValueMatchesRenderer(false);
    m_data.setSuggestedValue(sanitizeValue(value));
    updatePlaceholderVisibility(false);
    if (renderer())
        renderer()->updateFromElement();
    setNeedsStyleRecalc();
}

void HTMLInputElement::setValue(const String& value, bool sendChangeEvent)
{
    // For security reasons, we don't allow setting the filename, but we do allow clearing it.
    // The HTML5 spec (as of the 10/24/08 working draft) says that the value attribute isn't applicable to the file upload control
    // but we don't want to break existing websites, who may be relying on this method to clear things.
    if (inputType() == FILE && !value.isEmpty())
        return;

    setFormControlValueMatchesRenderer(false);
    if (storesValueSeparateFromAttribute()) {
        if (inputType() == FILE)
            m_fileList->clear();
        else {
            m_data.setValue(sanitizeValue(value));
            if (isTextField())
                updatePlaceholderVisibility(false);
        }
        setNeedsStyleRecalc();
    } else
        setAttribute(valueAttr, sanitizeValue(value));

    setNeedsValidityCheck();

    if (isTextField()) {
        unsigned max = m_data.value().length();
        if (document()->focusedNode() == this)
            InputElement::updateSelectionRange(this, this, max, max);
        else
            cacheSelection(max, max);
        m_data.setSuggestedValue(String());
    }

    // Don't dispatch the change event when focused, it will be dispatched
    // when the control loses focus.
    if (sendChangeEvent && document()->focusedNode() != this)
        dispatchFormControlChangeEvent();

    InputElement::notifyFormStateChanged(this);
}

double HTMLInputElement::parseToDouble(const String& src, double defaultValue) const
{
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case TIME:
    case WEEK: {
        DateComponents date;
        if (!parseToDateComponents(inputType(), src, &date))
            return defaultValue;
        double msec = date.millisecondsSinceEpoch();
        ASSERT(isfinite(msec));
        return msec;
    }
    case MONTH: {
        DateComponents date;
        if (!parseToDateComponents(inputType(), src, &date))
            return defaultValue;
        double months = date.monthsSinceEpoch();
        ASSERT(isfinite(months));
        return months;
    }
    case NUMBER:
    case RANGE: {
        double numberValue;
        if (!parseToDoubleForNumberType(src, &numberValue))
            return defaultValue;
        ASSERT(isfinite(numberValue));
        return numberValue;
    }

    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        return defaultValue;
    }
    ASSERT_NOT_REACHED();
    return defaultValue;
}

double HTMLInputElement::valueAsDate() const
{
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case TIME:
    case WEEK:
        return parseToDouble(value(), DateComponents::invalidMilliseconds());
    case MONTH: {
        DateComponents date;
        if (!parseToDateComponents(inputType(), value(), &date))
            return DateComponents::invalidMilliseconds();
        double msec = date.millisecondsSinceEpoch();
        ASSERT(isfinite(msec));
        return msec;
    }

    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case DATETIMELOCAL: // valueAsDate doesn't work for the DATETIMELOCAL type according to the standard.
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case NUMBER:
    case PASSWORD:
    case RADIO:
    case RANGE:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        return DateComponents::invalidMilliseconds();
    }
    ASSERT_NOT_REACHED();
    return DateComponents::invalidMilliseconds();
}

void HTMLInputElement::setValueAsDate(double value, ExceptionCode& ec)
{
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case TIME:
    case WEEK:
        setValue(serializeForDateTimeTypes(value));
        return;
    case MONTH: {
        DateComponents date;
        if (!date.setMillisecondsSinceEpochForMonth(value)) {
            setValue(String());
            return;
        }
        setValue(date.toString());
        return;
    }
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case DATETIMELOCAL: // valueAsDate doesn't work for the DATETIMELOCAL type according to the standard.
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case NUMBER:
    case PASSWORD:
    case RADIO:
    case RANGE:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        ec = INVALID_STATE_ERR;
        return;
    }
    ASSERT_NOT_REACHED();
}

double HTMLInputElement::valueAsNumber() const
{
    const double nan = numeric_limits<double>::quiet_NaN();
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case NUMBER:
    case RANGE:
    case TIME:
    case WEEK:
        return parseToDouble(value(), nan);

    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        return nan;
    }
    ASSERT_NOT_REACHED();
    return nan;
}

void HTMLInputElement::setValueAsNumber(double newValue, ExceptionCode& ec)
{
    if (!isfinite(newValue)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case NUMBER:
    case RANGE:
    case TIME:
    case WEEK:
        setValue(serialize(newValue));
        return;

    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        ec = INVALID_STATE_ERR;
        return;
    }
    ASSERT_NOT_REACHED();
}

String HTMLInputElement::serializeForDateTimeTypes(double value) const
{
    bool success = false;
    DateComponents date;
    switch (inputType()) {
    case DATE:
        success = date.setMillisecondsSinceEpochForDate(value);
        break;
    case DATETIME:
        success = date.setMillisecondsSinceEpochForDateTime(value);
        break;
    case DATETIMELOCAL:
        success = date.setMillisecondsSinceEpochForDateTimeLocal(value);
        break;
    case MONTH:
        success = date.setMonthsSinceEpoch(value);
        break;
    case TIME:
        success = date.setMillisecondsSinceMidnight(value);
        break;
    case WEEK:
        success = date.setMillisecondsSinceEpochForWeek(value);
        break;
    case NUMBER:
    case RANGE:
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        ASSERT_NOT_REACHED();
        return String();
    }
    if (!success)
        return String();

    double step;
    if (!getAllowedValueStep(&step))
        return date.toString();
    if (!fmod(step, msecPerMinute))
        return date.toString(DateComponents::None);
    if (!fmod(step, msecPerSecond))
        return date.toString(DateComponents::Second);
    return date.toString(DateComponents::Millisecond);
}

String HTMLInputElement::serialize(double value) const
{
    if (!isfinite(value))
        return String();
    switch (inputType()) {
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case MONTH:
    case TIME:
    case WEEK:
        return serializeForDateTimeTypes(value);
    case NUMBER:
    case RANGE:
        return serializeForNumberType(value);

    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SEARCH:
    case SUBMIT:
    case TELEPHONE:
    case TEXT:
    case URL:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String HTMLInputElement::placeholder() const
{
    return getAttribute(placeholderAttr).string();
}

void HTMLInputElement::setPlaceholder(const String& value)
{
    setAttribute(placeholderAttr, value);
}

bool HTMLInputElement::searchEventsShouldBeDispatched() const
{
    return hasAttribute(incrementalAttr);
}

void HTMLInputElement::setValueFromRenderer(const String& value)
{
    // File upload controls will always use setFileListFromRenderer.
    ASSERT(inputType() != FILE);
    m_data.setSuggestedValue(String());
    updatePlaceholderVisibility(false);
    InputElement::setValueFromRenderer(m_data, this, this, value);
    setNeedsValidityCheck();

    // Clear autofill flag (and yellow background) on user edit.
    setAutofilled(false);
}

void HTMLInputElement::setFileListFromRenderer(const Vector<String>& paths)
{
    m_fileList->clear();
    int size = paths.size();

#if ENABLE(DIRECTORY_UPLOAD)
    // If a directory is being selected, the UI allows a directory to be chosen
    // and the paths provided here share a root directory somewhere up the tree;
    // we want to store only the relative paths from that point.
    if (webkitdirectory() && size > 0) {
        String rootPath = directoryName(paths[0]);
        // Find the common root path.
        for (int i = 1; i < size; i++) {
            while (!paths[i].startsWith(rootPath))
                rootPath = directoryName(rootPath);
        }
        rootPath = directoryName(rootPath);
        ASSERT(rootPath.length());
        for (int i = 0; i < size; i++) {
            // Normalize backslashes to slashes before exposing the relative path to script.
            String relativePath = paths[i].substring(1 + rootPath.length()).replace('\\','/');
            m_fileList->append(File::create(relativePath, paths[i]));
        }
    } else {
        for (int i = 0; i < size; i++)
            m_fileList->append(File::create(paths[i]));
    }
#else
    for (int i = 0; i < size; i++)
        m_fileList->append(File::create(paths[i]));
#endif

    setFormControlValueMatchesRenderer(true);
    InputElement::notifyFormStateChanged(this);
    setNeedsValidityCheck();
}

bool HTMLInputElement::storesValueSeparateFromAttribute() const
{
    switch (inputType()) {
    case BUTTON:
    case CHECKBOX:
    case HIDDEN:
    case IMAGE:
    case RADIO:
    case RESET:
    case SUBMIT:
        return false;
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case FILE:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case RANGE:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        return true;
    }
    return false;
}

struct EventHandlingState : FastAllocBase {
    RefPtr<HTMLInputElement> m_currRadio;
    bool m_indeterminate;
    bool m_checked;
    
    EventHandlingState(bool indeterminate, bool checked)
        : m_indeterminate(indeterminate)
        , m_checked(checked) { }
};

void* HTMLInputElement::preDispatchEventHandler(Event* evt)
{
    // preventDefault or "return false" are used to reverse the automatic checking/selection we do here.
    // This result gives us enough info to perform the "undo" in postDispatch of the action we take here.
    void* result = 0; 
    if ((inputType() == CHECKBOX || inputType() == RADIO) && evt->type() == eventNames().clickEvent) {
        OwnPtr<EventHandlingState> state = adoptPtr(new EventHandlingState(indeterminate(), checked()));
        if (inputType() == CHECKBOX) {
            if (indeterminate())
                setIndeterminate(false);
            else
                setChecked(!checked(), true);
        } else {
            // For radio buttons, store the current selected radio object.
            // We really want radio groups to end up in sane states, i.e., to have something checked.
            // Therefore if nothing is currently selected, we won't allow this action to be "undone", since
            // we want some object in the radio group to actually get selected.
            HTMLInputElement* currRadio = checkedRadioButtons(this).checkedButtonForGroup(name());
            if (currRadio) {
                // We have a radio button selected that is not us.  Cache it in our result field and ref it so
                // that it can't be destroyed.
                state->m_currRadio = currRadio;
            }
            if (indeterminate())
                setIndeterminate(false);
            setChecked(true, true);
        }
        result = state.leakPtr(); // FIXME: Check whether this actually ends up leaking.
    }
    return result;
}

void HTMLInputElement::postDispatchEventHandler(Event *evt, void* data)
{
    if ((inputType() == CHECKBOX || inputType() == RADIO) && evt->type() == eventNames().clickEvent) {
        
        if (EventHandlingState* state = reinterpret_cast<EventHandlingState*>(data)) {
            if (inputType() == CHECKBOX) {
                // Reverse the checking we did in preDispatch.
                if (evt->defaultPrevented() || evt->defaultHandled()) {
                    setIndeterminate(state->m_indeterminate);
                    setChecked(state->m_checked);
                }
            } else {
                HTMLInputElement* input = state->m_currRadio.get();
                if (evt->defaultPrevented() || evt->defaultHandled()) {
                    // Restore the original selected radio button if possible.
                    // Make sure it is still a radio button and only do the restoration if it still
                    // belongs to our group.

                    if (input && input->form() == form() && input->inputType() == RADIO && input->name() == name()) {
                        // Ok, the old radio button is still in our form and in our group and is still a 
                        // radio button, so it's safe to restore selection to it.
                        input->setChecked(true);
                    }
                    setIndeterminate(state->m_indeterminate);
                }
            }
            delete state;
        }

        // Left clicks on radio buttons and check boxes already performed default actions in preDispatchEventHandler(). 
        evt->setDefaultHandled();
    }
}

void HTMLInputElement::defaultEventHandler(Event* evt)
{
    // FIXME: It would be better to refactor this for the different types of input element.
    // Having them all in one giant function makes this hard to read, and almost all the handling is type-specific.

    bool implicitSubmission = false;

    if (isTextField() && evt->type() == eventNames().textInputEvent && evt->isTextEvent() && static_cast<TextEvent*>(evt)->data() == "\n")
        implicitSubmission = true;

    if (inputType() == IMAGE && evt->isMouseEvent() && evt->type() == eventNames().clickEvent) {
        // record the mouse position for when we get the DOMActivate event
        MouseEvent* me = static_cast<MouseEvent*>(evt);
        // FIXME: We could just call offsetX() and offsetY() on the event,
        // but that's currently broken, so for now do the computation here.
        if (me->isSimulated() || !renderer()) {
            m_xPos = 0;
            m_yPos = 0;
        } else {
            // FIXME: This doesn't work correctly with transforms.
            // FIXME: pageX/pageY need adjusting for pageZoomFactor(). Use actualPageLocation()?
            IntPoint absOffset = roundedIntPoint(renderer()->localToAbsolute());
            m_xPos = me->pageX() - absOffset.x();
            m_yPos = me->pageY() - absOffset.y();
        }
    }

    if (hasSpinButton() && evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent()) {
        String key = static_cast<KeyboardEvent*>(evt)->keyIdentifier();
        int step = 0;
        if (key == "Up")
            step = 1;
        else if (key == "Down")
            step = -1;
        if (step) {
            stepUpFromRenderer(step);
            evt->setDefaultHandled();
            return;
        }
    }
 
   if (isTextField()
            && evt->type() == eventNames().keydownEvent
            && evt->isKeyboardEvent()
            && focused()
            && document()->frame()
            && document()->frame()->editor()->doTextFieldCommandFromEvent(this, static_cast<KeyboardEvent*>(evt))) {
        evt->setDefaultHandled();
        return;
    }

    if (inputType() == RADIO && evt->type() == eventNames().clickEvent) {
        evt->setDefaultHandled();
        return;
    }
    
    // Call the base event handler before any of our own event handling for almost all events in text fields.
    // Makes editing keyboard handling take precedence over the keydown and keypress handling in this function.
    bool callBaseClassEarly = isTextField() && !implicitSubmission
        && (evt->type() == eventNames().keydownEvent || evt->type() == eventNames().keypressEvent);
    if (callBaseClassEarly) {
        HTMLFormControlElementWithState::defaultEventHandler(evt);
        if (evt->defaultHandled())
            return;
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. JavaScript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if (evt->type() == eventNames().DOMActivateEvent && !disabled()) {
        if (inputType() == IMAGE || inputType() == SUBMIT || inputType() == RESET) {
            if (!form())
                return;
            if (inputType() == RESET)
                form()->reset();
            else {
                m_activeSubmit = true;
                // FIXME: Would be cleaner to get m_xPos and m_yPos out of the underlying mouse
                // event (if any) here instead of relying on the variables set above when
                // processing the click event. Even better, appendFormData could pass the
                // event in, and then we could get rid of m_xPos and m_yPos altogether!
                if (!form()->prepareSubmit(evt)) {
                    m_xPos = 0;
                    m_yPos = 0;
                }
                m_activeSubmit = false;
            }
        } else if (inputType() == FILE && renderer())
            toRenderFileUploadControl(renderer())->click();
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->type() == eventNames().keypressEvent && evt->isKeyboardEvent()) {
        bool clickElement = false;

        int charCode = static_cast<KeyboardEvent*>(evt)->charCode();

        if (charCode == '\r') {
            switch (inputType()) {
            case CHECKBOX:
            case COLOR:
            case DATE:
            case DATETIME:
            case DATETIMELOCAL:
            case EMAIL:
            case HIDDEN:
            case ISINDEX:
            case MONTH:
            case NUMBER:
            case PASSWORD:
            case RADIO:
            case RANGE:
            case SEARCH:
            case TELEPHONE:
            case TEXT:
            case TIME:
            case URL:
            case WEEK:
                // Simulate mouse click on the default form button for enter for these types of elements.
                implicitSubmission = true;
                break;
            case BUTTON:
            case FILE:
            case IMAGE:
            case RESET:
            case SUBMIT:
                // Simulate mouse click for enter for these types of elements.
                clickElement = true;
                break;
            }
        } else if (charCode == ' ') {
            switch (inputType()) {
            case BUTTON:
            case CHECKBOX:
            case FILE:
            case IMAGE:
            case RESET:
            case SUBMIT:
            case RADIO:
                // Prevent scrolling down the page.
                evt->setDefaultHandled();
                return;
            default:
                break;
            }
        }

        if (clickElement) {
            dispatchSimulatedClick(evt);
            evt->setDefaultHandled();
            return;
        }
    }

    if (evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent()) {
        String key = static_cast<KeyboardEvent*>(evt)->keyIdentifier();

        if (key == "U+0020") {
            switch (inputType()) {
            case BUTTON:
            case CHECKBOX:
            case FILE:
            case IMAGE:
            case RESET:
            case SUBMIT:
            case RADIO:
                setActive(true, true);
                // No setDefaultHandled(), because IE dispatches a keypress in this case
                // and the caller will only dispatch a keypress if we don't call setDefaultHandled.
                return;
            default:
                break;
            }
        }

        if (inputType() == RADIO && (key == "Up" || key == "Down" || key == "Left" || key == "Right")) {
            // Left and up mean "previous radio button".
            // Right and down mean "next radio button".
            // Tested in WinIE, and even for RTL, left still means previous radio button (and so moves
            // to the right).  Seems strange, but we'll match it.
            bool forward = (key == "Down" || key == "Right");
            
            // We can only stay within the form's children if the form hasn't been demoted to a leaf because
            // of malformed HTML.
            Node* n = this;
            while ((n = (forward ? n->traverseNextNode() : n->traversePreviousNode()))) {
                // Once we encounter a form element, we know we're through.
                if (n->hasTagName(formTag))
                    break;
                    
                // Look for more radio buttons.
                if (n->hasTagName(inputTag)) {
                    HTMLInputElement* elt = static_cast<HTMLInputElement*>(n);
                    if (elt->form() != form())
                        break;
                    if (n->hasTagName(inputTag)) {
                        HTMLInputElement* inputElt = static_cast<HTMLInputElement*>(n);
                        if (inputElt->inputType() == RADIO && inputElt->name() == name() && inputElt->isFocusable()) {
                            inputElt->setChecked(true);
                            document()->setFocusedNode(inputElt);
                            inputElt->dispatchSimulatedClick(evt, false, false);
                            evt->setDefaultHandled();
                            break;
                        }
                    }
                }
            }
        }
    }

    if (evt->type() == eventNames().keyupEvent && evt->isKeyboardEvent()) {
        bool clickElement = false;

        String key = static_cast<KeyboardEvent*>(evt)->keyIdentifier();

        if (key == "U+0020") {
            switch (inputType()) {
            case BUTTON:
            case CHECKBOX:
            case FILE:
            case IMAGE:
            case RESET:
            case SUBMIT:
                // Simulate mouse click for spacebar for these types of elements.
                // The AppKit already does this for some, but not all, of them.
                clickElement = true;
                break;
            case RADIO:
                // If an unselected radio is tabbed into (because the entire group has nothing
                // checked, or because of some explicit .focus() call), then allow space to check it.
                if (!checked())
                    clickElement = true;
                break;
            case COLOR:
            case DATE:
            case DATETIME:
            case DATETIMELOCAL:
            case EMAIL:
            case HIDDEN:
            case ISINDEX:
            case MONTH:
            case NUMBER:
            case PASSWORD:
            case RANGE:
            case SEARCH:
            case TELEPHONE:
            case TEXT:
            case TIME:
            case URL:
            case WEEK:
                break;
            }
        }

        if (clickElement) {
            if (active())
                dispatchSimulatedClick(evt);
            evt->setDefaultHandled();
            return;
        }        
    }

    if (implicitSubmission) {
        if (isSearchField()) {
            addSearchResult();
            onSearch();
        }
        // Fire onChange for text fields.
        RenderObject* r = renderer();
        if (r && r->isTextField() && toRenderTextControl(r)->wasChangedSinceLastChangeEvent()) {
            dispatchFormControlChangeEvent();
            // Refetch the renderer since arbitrary JS code run during onchange can do anything, including destroying it.
            r = renderer();
            if (r && r->isTextField())
                toRenderTextControl(r)->setChangedSinceLastChangeEvent(false);
        }

        RefPtr<HTMLFormElement> formForSubmission = form();
        // If there is no form and the element is an <isindex>, then create a temporary form just to be used for submission.
        if (!formForSubmission && inputType() == ISINDEX)
            formForSubmission = createTemporaryFormForIsIndex();

        // Form may never have been present, or may have been destroyed by code responding to the change event.
        if (formForSubmission)
            formForSubmission->submitImplicitly(evt, canTriggerImplicitSubmission());

        evt->setDefaultHandled();
        return;
    }

    if (evt->isBeforeTextInsertedEvent())
        handleBeforeTextInsertedEvent(evt);

    if (hasSpinButton() && evt->isWheelEvent()) {
        WheelEvent* wheel = static_cast<WheelEvent*>(evt);
        int step = 0;
        if (wheel->wheelDeltaY() > 0) {
            step = 1;
        } else if (wheel->wheelDeltaY() < 0) {
            step = -1;
        }
        if (step) {
            stepUpFromRenderer(step);
            evt->setDefaultHandled();
            return;
        }
    }
    if (isTextField() && renderer() && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent() || evt->type() == eventNames().blurEvent || evt->type() == eventNames().focusEvent))
        toRenderTextControlSingleLine(renderer())->forwardEvent(evt);

    if (inputType() == RANGE && renderer() && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent()))
        toRenderSlider(renderer())->forwardEvent(evt);

    if (!callBaseClassEarly && !evt->defaultHandled())
        HTMLFormControlElementWithState::defaultEventHandler(evt);
}

void HTMLInputElement::handleBeforeTextInsertedEvent(Event* event)
{
    if (inputType() == NUMBER) {
        BeforeTextInsertedEvent* textEvent = static_cast<BeforeTextInsertedEvent*>(event);
        unsigned length = textEvent->text().length();
        bool hasInvalidChar = false;
        for (unsigned i = 0; i < length; ++i) {
            if (!isNumberCharacter(textEvent->text()[i])) {
                hasInvalidChar = true;
                break;
            }
        }
        if (hasInvalidChar) {
            Vector<UChar> stripped;
            stripped.reserveCapacity(length);
            for (unsigned i = 0; i < length; ++i) {
                UChar ch = textEvent->text()[i];
                if (!isNumberCharacter(ch))
                    continue;
                stripped.append(ch);
            }
            textEvent->setText(String::adopt(stripped));
        }
    }
    InputElement::handleBeforeTextInsertedEvent(m_data, this, this, event);
}

PassRefPtr<HTMLFormElement> HTMLInputElement::createTemporaryFormForIsIndex()
{
    RefPtr<HTMLFormElement> form = HTMLFormElement::create(document());
    form->registerFormElement(this);
    form->setMethod("GET");
    if (!document()->baseURL().isEmpty()) {
        // We treat the href property of the <base> element as the form action, as per section 7.5 
        // "Queries and Indexes" of the HTML 2.0 spec. <http://www.w3.org/MarkUp/html-spec/html-spec_7.html#SEC7.5>.
        form->setAction(document()->baseURL().string());
    }
    return form.release();
}

bool HTMLInputElement::isURLAttribute(Attribute *attr) const
{
    return (attr->name() == srcAttr);
}

String HTMLInputElement::defaultValue() const
{
    return getAttribute(valueAttr);
}

void HTMLInputElement::setDefaultValue(const String &value)
{
    setAttribute(valueAttr, value);
}

bool HTMLInputElement::defaultChecked() const
{
    return !getAttribute(checkedAttr).isNull();
}

void HTMLInputElement::setDefaultName(const AtomicString& name)
{
    m_data.setName(name);
}

String HTMLInputElement::accept() const
{
    return getAttribute(acceptAttr);
}

String HTMLInputElement::alt() const
{
    return getAttribute(altAttr);
}

int HTMLInputElement::maxLength() const
{
    return m_data.maxLength();
}

void HTMLInputElement::setMaxLength(int maxLength, ExceptionCode& ec)
{
    if (maxLength < 0)
        ec = INDEX_SIZE_ERR;
    else
        setAttribute(maxlengthAttr, String::number(maxLength));
}

bool HTMLInputElement::multiple() const
{
    return !getAttribute(multipleAttr).isNull();
}

#if ENABLE(DIRECTORY_UPLOAD)
bool HTMLInputElement::webkitdirectory() const
{
    return !getAttribute(webkitdirectoryAttr).isNull();
}
#endif

void HTMLInputElement::setSize(unsigned size)
{
    setAttribute(sizeAttr, String::number(size));
}

KURL HTMLInputElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLInputElement::setAutofilled(bool b)
{
    if (b == m_autofilled)
        return;
        
    m_autofilled = b;
    setNeedsStyleRecalc();
}

FileList* HTMLInputElement::files()
{
    if (inputType() != FILE)
        return 0;
    return m_fileList.get();
}

bool HTMLInputElement::isAcceptableValue(const String& proposedValue) const
{
    if (inputType() != NUMBER)
        return true;
    return proposedValue.isEmpty() || parseToDoubleForNumberType(proposedValue, 0);
}

String HTMLInputElement::sanitizeValue(const String& proposedValue) const
{
    if (inputType() == NUMBER)
        return parseToDoubleForNumberType(proposedValue, 0) ? proposedValue : String();

    if (isTextField())
        return InputElement::sanitizeValueForTextField(this, proposedValue);

    // If the proposedValue is null than this is a reset scenario and we
    // want the range input's value attribute to take priority over the
    // calculated default (middle) value.
    if (inputType() == RANGE && !proposedValue.isNull())
        return serializeForNumberType(StepRange(this).clampValue(proposedValue));

    return proposedValue;
}

bool HTMLInputElement::hasUnacceptableValue() const
{
    return inputType() == NUMBER && renderer() && !isAcceptableValue(toRenderTextControl(renderer())->text());
}

bool HTMLInputElement::needsActivationCallback()
{
    return inputType() == PASSWORD || m_autocomplete == Off;
}

void HTMLInputElement::registerForActivationCallbackIfNeeded()
{
    if (needsActivationCallback())
        document()->registerForDocumentActivationCallbacks(this);
}

void HTMLInputElement::unregisterForActivationCallbackIfNeeded()
{
    if (!needsActivationCallback())
        document()->unregisterForDocumentActivationCallbacks(this);
}

bool HTMLInputElement::isRequiredFormControl() const
{
    if (!required())
        return false;

    switch (inputType()) {
    case CHECKBOX:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case FILE:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case RADIO:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        return true;
    case BUTTON:
    case COLOR:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case RANGE:
    case RESET:
    case SUBMIT:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void HTMLInputElement::cacheSelection(int start, int end)
{
    m_data.setCachedSelectionStart(start);
    m_data.setCachedSelectionEnd(end);
}

void HTMLInputElement::addSearchResult()
{
    ASSERT(isSearchField());
    if (renderer())
        toRenderTextControlSingleLine(renderer())->addSearchResult();
}

void HTMLInputElement::onSearch()
{
    ASSERT(isSearchField());
    if (renderer())
        toRenderTextControlSingleLine(renderer())->stopSearchEventTimer();
    dispatchEvent(Event::create(eventNames().searchEvent, true, false));
}

void HTMLInputElement::documentDidBecomeActive()
{
    ASSERT(needsActivationCallback());
    reset();
}

void HTMLInputElement::willMoveToNewOwnerDocument()
{
    if (m_imageLoader)
        m_imageLoader->elementWillMoveToNewOwnerDocument();

    // Always unregister for cache callbacks when leaving a document, even if we would otherwise like to be registered
    if (needsActivationCallback())
        document()->unregisterForDocumentActivationCallbacks(this);
        
    document()->checkedRadioButtons().removeButton(this);
    
    HTMLFormControlElementWithState::willMoveToNewOwnerDocument();
}

void HTMLInputElement::didMoveToNewOwnerDocument()
{
    registerForActivationCallbackIfNeeded();
        
    HTMLFormControlElementWithState::didMoveToNewOwnerDocument();
}
    
void HTMLInputElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLFormControlElementWithState::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, src());
}

bool HTMLInputElement::recalcWillValidate() const
{
    switch (inputType()) {
    case CHECKBOX:
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case FILE:
    case ISINDEX:
    case MONTH:
    case NUMBER:
    case PASSWORD:
    case RADIO:
    case RANGE:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK:
        return HTMLFormControlElementWithState::recalcWillValidate();
    case BUTTON:
    case HIDDEN:
    case IMAGE:
    case RESET:
    case SUBMIT:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool HTMLInputElement::parseToDateComponents(InputType type, const String& formString, DateComponents* out)
{
    if (formString.isEmpty())
        return false;
    DateComponents ignoredResult;
    if (!out)
        out = &ignoredResult;
    const UChar* characters = formString.characters();
    unsigned length = formString.length();
    unsigned end;

    switch (type) {
    case DATE:
        return out->parseDate(characters, length, 0, end) && end == length;
    case DATETIME:
        return out->parseDateTime(characters, length, 0, end) && end == length;
    case DATETIMELOCAL:
        return out->parseDateTimeLocal(characters, length, 0, end) && end == length;
    case MONTH:
        return out->parseMonth(characters, length, 0, end) && end == length;
    case WEEK:
        return out->parseWeek(characters, length, 0, end) && end == length;
    case TIME:
        return out->parseTime(characters, length, 0, end) && end == length;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

#if ENABLE(DATALIST)

HTMLElement* HTMLInputElement::list() const
{
    return dataList();
}

HTMLDataListElement* HTMLInputElement::dataList() const
{
    if (!m_hasNonEmptyList)
        return 0;

    switch (inputType()) {
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case MONTH:
    case NUMBER:
    case RANGE:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
    case TIME:
    case URL:
    case WEEK: {
        Element* element = document()->getElementById(getAttribute(listAttr));
        if (element && element->hasTagName(datalistTag))
            return static_cast<HTMLDataListElement*>(element);
        break;
    }
    case BUTTON:
    case CHECKBOX:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case PASSWORD:
    case RADIO:
    case RESET:
    case SUBMIT:
        break;
    }
    return 0;
}

HTMLOptionElement* HTMLInputElement::selectedOption() const
{
    String currentValue = value();
    // The empty value never matches to a datalist option because it
    // doesn't represent a suggestion according to the standard.
    if (currentValue.isEmpty())
        return 0;

    HTMLDataListElement* sourceElement = dataList();
    if (!sourceElement)
        return 0;
    RefPtr<HTMLCollection> options = sourceElement->options();
    for (unsigned i = 0; options && i < options->length(); ++i) {
        HTMLOptionElement* option = static_cast<HTMLOptionElement*>(options->item(i));
        if (!option->disabled() && currentValue == option->value())
            return option;
    }
    return 0;
}

#endif // ENABLE(DATALIST)

void HTMLInputElement::stepUpFromRenderer(int n)
{
    // The differences from stepUp()/stepDown():
    // If the current value is not a number, the value will be
    //  - The value should be the minimum value if n > 0
    //  - The value should be the maximum value if n < 0
    // If the current value is smaller than the minimum value:
    //  - The value should be the minimum value if n > 0
    //  - Nothing should happen if n < 0
    // If the current value is larger than the maximum value:
    //  - The value should be the maximum value if n < 0
    //  - Nothing should happen if n > 0

    ASSERT(hasSpinButton());
    if (!hasSpinButton())
        return;
    ASSERT(n);
    if (!n)
        return;

    const double nan = numeric_limits<double>::quiet_NaN();
    String currentStringValue = value();
    double current = parseToDouble(currentStringValue, nan);
    if (!isfinite(current) || (n > 0 && current < minimum()) || (n < 0 && current > maximum()))
        setValue(serialize(n > 0 ? minimum() : maximum()));
    else {
        ExceptionCode ec;
        stepUp(n, ec);
    }

    if (currentStringValue != value()) {
        if (renderer() && renderer()->isTextField())
            toRenderTextControl(renderer())->setChangedSinceLastChangeEvent(true);
        dispatchEvent(Event::create(eventNames().inputEvent, true, false));
    }
}

#if ENABLE(WCSS)
void HTMLInputElement::setWapInputFormat(String& mask)
{
    String validateMask = validateInputMask(m_data, mask);
    if (!validateMask.isEmpty())
        m_data.setInputFormatMask(validateMask);
}
#endif

#if ENABLE(INPUT_SPEECH)
bool HTMLInputElement::isSpeechEnabled() const
{
    switch (inputType()) {
    // FIXME: Add support for RANGE, EMAIL, URL, COLOR and DATE/TIME input types.
    case NUMBER:
    case PASSWORD:
    case SEARCH:
    case TELEPHONE:
    case TEXT:
        return hasAttribute(speechAttr);
    case BUTTON:
    case CHECKBOX:
    case COLOR:
    case DATE:
    case DATETIME:
    case DATETIMELOCAL:
    case EMAIL:
    case FILE:
    case HIDDEN:
    case IMAGE:
    case ISINDEX:
    case MONTH:
    case RADIO:
    case RANGE:
    case RESET:
    case SUBMIT:
    case TIME:
    case URL:
    case WEEK:
        return false;
    }
    return false;
}
#endif

} // namespace
