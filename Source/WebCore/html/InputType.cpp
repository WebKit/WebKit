/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
#include "InputType.h"

#include "AXObjectCache.h"
#include "BeforeTextInsertedEvent.h"
#include "ButtonInputType.h"
#include "CheckboxInputType.h"
#include "ColorInputType.h"
#include "DateComponents.h"
#include "DateInputType.h"
#include "DateTimeInputType.h"
#include "DateTimeLocalInputType.h"
#include "ElementShadow.h"
#include "EmailInputType.h"
#include "ExceptionCode.h"
#include "FileInputType.h"
#include "FormDataList.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLShadowElement.h"
#include "HiddenInputType.h"
#include "ImageInputType.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MonthInputType.h"
#include "NumberInputType.h"
#include "Page.h"
#include "PasswordInputType.h"
#include "RadioInputType.h"
#include "RangeInputType.h"
#include "RegularExpression.h"
#include "RenderObject.h"
#include "RenderTheme.h"
#include "ResetInputType.h"
#include "RuntimeEnabledFeatures.h"
#include "SearchInputType.h"
#include "ShadowRoot.h"
#include "SubmitInputType.h"
#include "TelephoneInputType.h"
#include "TextInputType.h"
#include "TimeInputType.h"
#include "URLInputType.h"
#include "WeekInputType.h"
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

typedef PassOwnPtr<InputType> (*InputTypeFactoryFunction)(HTMLInputElement*);
typedef HashMap<String, InputTypeFactoryFunction, CaseFoldingHash> InputTypeFactoryMap;

static PassOwnPtr<InputTypeFactoryMap> createInputTypeFactoryMap()
{
    OwnPtr<InputTypeFactoryMap> map = adoptPtr(new InputTypeFactoryMap);
    map->add(InputTypeNames::button(), ButtonInputType::create);
    map->add(InputTypeNames::checkbox(), CheckboxInputType::create);
#if ENABLE(INPUT_TYPE_COLOR)
    map->add(InputTypeNames::color(), ColorInputType::create);
#endif
#if ENABLE(INPUT_TYPE_DATE)
    if (RuntimeEnabledFeatures::inputTypeDateEnabled())
        map->add(InputTypeNames::date(), DateInputType::create);
#endif
#if ENABLE(INPUT_TYPE_DATETIME)
    map->add(InputTypeNames::datetime(), DateTimeInputType::create);
#endif
#if ENABLE(INPUT_TYPE_DATETIMELOCAL)
    map->add(InputTypeNames::datetimelocal(), DateTimeLocalInputType::create);
#endif
    map->add(InputTypeNames::email(), EmailInputType::create);
    map->add(InputTypeNames::file(), FileInputType::create);
    map->add(InputTypeNames::hidden(), HiddenInputType::create);
    map->add(InputTypeNames::image(), ImageInputType::create);
#if ENABLE(INPUT_TYPE_MONTH)
    map->add(InputTypeNames::month(), MonthInputType::create);
#endif
    map->add(InputTypeNames::number(), NumberInputType::create);
    map->add(InputTypeNames::password(), PasswordInputType::create);
    map->add(InputTypeNames::radio(), RadioInputType::create);
    map->add(InputTypeNames::range(), RangeInputType::create);
    map->add(InputTypeNames::reset(), ResetInputType::create);
    map->add(InputTypeNames::search(), SearchInputType::create);
    map->add(InputTypeNames::submit(), SubmitInputType::create);
    map->add(InputTypeNames::telephone(), TelephoneInputType::create);
#if ENABLE(INPUT_TYPE_TIME)
    map->add(InputTypeNames::time(), TimeInputType::create);
#endif
    map->add(InputTypeNames::url(), URLInputType::create);
#if ENABLE(INPUT_TYPE_WEEK)
    map->add(InputTypeNames::week(), WeekInputType::create);
#endif
    // No need to register "text" because it is the default type.
    return map.release();
}

PassOwnPtr<InputType> InputType::create(HTMLInputElement* element, const String& typeName)
{
    static const InputTypeFactoryMap* factoryMap = createInputTypeFactoryMap().leakPtr();
    PassOwnPtr<InputType> (*factory)(HTMLInputElement*) = typeName.isEmpty() ? 0 : factoryMap->get(typeName);
    if (!factory)
        factory = TextInputType::create;
    return factory(element);
}

PassOwnPtr<InputType> InputType::createText(HTMLInputElement* element)
{
    return TextInputType::create(element);
}

InputType::~InputType()
{
}

bool InputType::themeSupportsDataListUI(InputType* type)
{
    Document* document = type->element()->document();
    RefPtr<RenderTheme> theme = document->page() ? document->page()->theme() : RenderTheme::defaultTheme();
    return theme->supportsDataListUI(type->formControlType());
}

bool InputType::isTextField() const
{
    return false;
}

bool InputType::isTextType() const
{
    return false;
}

bool InputType::isRangeControl() const
{
    return false;
}

bool InputType::saveFormControlState(String& result) const
{
    String currentValue = element()->value();
    if (currentValue == element()->defaultValue())
        return false;
    result = currentValue;
    return true;
}

void InputType::restoreFormControlState(const String& state)
{
    element()->setValue(state);
}

bool InputType::isFormDataAppendable() const
{
    // There is no form data unless there's a name for non-image types.
    return !element()->name().isEmpty();
}

bool InputType::appendFormData(FormDataList& encoding, bool) const
{
    // Always successful.
    encoding.appendData(element()->name(), element()->value());
    return true;
}

double InputType::valueAsDate() const
{
    return DateComponents::invalidMilliseconds();
}

void InputType::setValueAsDate(double, ExceptionCode& ec) const
{
    ec = INVALID_STATE_ERR;
}

double InputType::valueAsNumber() const
{
    return numeric_limits<double>::quiet_NaN();
}

void InputType::setValueAsNumber(double, TextFieldEventBehavior, ExceptionCode& ec) const
{
    ec = INVALID_STATE_ERR;
}

bool InputType::supportsValidation() const
{
    return true;
}

bool InputType::typeMismatchFor(const String&) const
{
    return false;
}

bool InputType::typeMismatch() const
{
    return false;
}

bool InputType::supportsRequired() const
{
    // Almost all validatable types support @required.
    return supportsValidation();
}

bool InputType::valueMissing(const String&) const
{
    return false;
}

bool InputType::patternMismatch(const String&) const
{
    return false;
}

bool InputType::rangeUnderflow(const String&) const
{
    return false;
}

bool InputType::rangeOverflow(const String&) const
{
    return false;
}

bool InputType::supportsRangeLimitation() const
{
    return false;
}

double InputType::defaultValueForStepUp() const
{
    return 0;
}

double InputType::minimum() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

double InputType::maximum() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool InputType::sizeShouldIncludeDecoration(int, int& preferredSize) const
{
    preferredSize = element()->size();
    return false;
}

bool InputType::stepMismatch(const String&, double) const
{
    // Non-supported types should be rejected by HTMLInputElement::getAllowedValueStep().
    ASSERT_NOT_REACHED();
    return false;
}

double InputType::stepBase() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

double InputType::stepBaseWithDecimalPlaces(unsigned* decimalPlaces) const
{
    if (decimalPlaces)
        *decimalPlaces = 0;
    return stepBase();
}

double InputType::defaultStep() const
{
    return numeric_limits<double>::quiet_NaN();
}

double InputType::stepScaleFactor() const
{
    return numeric_limits<double>::quiet_NaN();
}

bool InputType::parsedStepValueShouldBeInteger() const
{
    return false;
}

bool InputType::scaledStepValueShouldBeInteger() const
{
    return false;
}

double InputType::acceptableError(double) const
{
    return 0;
}

String InputType::typeMismatchText() const
{
    return validationMessageTypeMismatchText();
}

String InputType::valueMissingText() const
{
    return validationMessageValueMissingText();
}

void InputType::handleClickEvent(MouseEvent*)
{
}

void InputType::handleMouseDownEvent(MouseEvent*)
{
}

void InputType::handleDOMActivateEvent(Event*)
{
}

void InputType::handleKeydownEvent(KeyboardEvent*)
{
}

void InputType::handleKeypressEvent(KeyboardEvent*)
{
}

void InputType::handleKeyupEvent(KeyboardEvent*)
{
}

void InputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*)
{
}

void InputType::handleWheelEvent(WheelEvent*)
{
}

void InputType::forwardEvent(Event*)
{
}

bool InputType::shouldSubmitImplicitly(Event* event)
{
    return event->isKeyboardEvent() && event->type() == eventNames().keypressEvent && static_cast<KeyboardEvent*>(event)->charCode() == '\r';
}

PassRefPtr<HTMLFormElement> InputType::formForSubmission() const
{
    return element()->form();
}

RenderObject* InputType::createRenderer(RenderArena*, RenderStyle* style) const
{
    return RenderObject::createObject(element(), style);
}

void InputType::createShadowSubtree()
{
}

void InputType::destroyShadowSubtree()
{
    ElementShadow* shadow = element()->shadow();
    if (!shadow)
        return;

    ShadowRoot* root = shadow->oldestShadowRoot();
    root->removeAllChildren();

    // It's ok to clear contents of all other ShadowRoots because they must have
    // been created by TextFieldDecorationElement, and we don't allow adding
    // AuthorShadowRoot to HTMLInputElement.
    while ((root = root->youngerShadowRoot())) {
        root->removeAllChildren();
        root->appendChild(HTMLShadowElement::create(shadowTag, element()->document()));
    }
}

double InputType::parseToDouble(const String&, double defaultValue) const
{
    return defaultValue;
}

double InputType::parseToDoubleWithDecimalPlaces(const String& src, double defaultValue, unsigned *decimalPlaces) const
{
    if (decimalPlaces)
        *decimalPlaces = 0;
    return parseToDouble(src, defaultValue);
}

bool InputType::parseToDateComponents(const String&, DateComponents*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

String InputType::serialize(double) const
{
    ASSERT_NOT_REACHED();
    return String();
}

void InputType::dispatchSimulatedClickIfActive(KeyboardEvent* event) const
{
    if (element()->active())
        element()->dispatchSimulatedClick(event);
    event->setDefaultHandled();
}

Chrome* InputType::chrome() const
{
    if (Page* page = element()->document()->page())
        return page->chrome();
    return 0;
}

bool InputType::canSetStringValue() const
{
    return true;
}

bool InputType::isKeyboardFocusable() const
{
    return true;
}

bool InputType::shouldUseInputMethod() const
{
    return false;
}

void InputType::handleFocusEvent()
{
}

void InputType::handleBlurEvent()
{
}

void InputType::accessKeyAction(bool)
{
    element()->focus(false);
}

void InputType::attach()
{
}

void InputType::detach()
{
}

void InputType::altAttributeChanged()
{
}

void InputType::srcAttributeChanged()
{
}

void InputType::willMoveToNewOwnerDocument()
{
}

bool InputType::shouldRespectAlignAttribute()
{
    return false;
}

bool InputType::canChangeFromAnotherType() const
{
    return true;
}

void InputType::minOrMaxAttributeChanged()
{
}

void InputType::stepAttributeChanged()
{
}

bool InputType::canBeSuccessfulSubmitButton()
{
    return false;
}

HTMLElement* InputType::placeholderElement() const
{
    return 0;
}

bool InputType::rendererIsNeeded()
{
    return true;
}

FileList* InputType::files()
{
    return 0;
}

bool InputType::getTypeSpecificValue(String&)
{
    return false;
}

String InputType::fallbackValue() const
{
    return String();
}

String InputType::defaultValue() const
{
    return String();
}

bool InputType::canSetSuggestedValue()
{
    return false;
}

bool InputType::shouldSendChangeEventAfterCheckedChanged()
{
    return true;
}

bool InputType::storesValueSeparateFromAttribute()
{
    return true;
}

void InputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    element()->setValueInternal(sanitizedValue, eventBehavior);
    element()->setNeedsStyleRecalc();
    if (valueChanged && eventBehavior != DispatchNoEvent)
        element()->dispatchFormControlChangeEvent();
}

bool InputType::canSetValue(const String&)
{
    return true;
}

PassOwnPtr<ClickHandlingState> InputType::willDispatchClick()
{
    return nullptr;
}

void InputType::didDispatchClick(Event*, const ClickHandlingState&)
{
}

String InputType::visibleValue() const
{
    return element()->value();
}

String InputType::convertFromVisibleValue(const String& visibleValue) const
{
    return visibleValue;
}

bool InputType::isAcceptableValue(const String&)
{
    return true;
}

String InputType::sanitizeValue(const String& proposedValue) const
{
    return proposedValue;
}

bool InputType::hasUnacceptableValue()
{
    return false;
}

void InputType::receiveDroppedFiles(const Vector<String>&)
{
    ASSERT_NOT_REACHED();
}

Icon* InputType::icon() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool InputType::shouldResetOnDocumentActivation()
{
    return false;
}

bool InputType::shouldRespectListAttribute()
{
    return false;
}

bool InputType::shouldRespectSpeechAttribute()
{
    return false;
}

bool InputType::isTextButton() const
{
    return false;
}

bool InputType::isRadioButton() const
{
    return false;
}

bool InputType::isSearchField() const
{
    return false;
}

bool InputType::isHiddenType() const
{
    return false;
}

bool InputType::isPasswordField() const
{
    return false;
}

bool InputType::isCheckbox() const
{
    return false;
}

bool InputType::isEmailField() const
{
    return false;
}

bool InputType::isFileUpload() const
{
    return false;
}

bool InputType::isImageButton() const
{
    return false;
}

bool InputType::supportLabels() const
{
    return true;
}

bool InputType::isNumberField() const
{
    return false;
}

bool InputType::isSubmitButton() const
{
    return false;
}

bool InputType::isTelephoneField() const
{
    return false;
}

bool InputType::isURLField() const
{
    return false;
}

bool InputType::isDateField() const
{
    return false;
}

bool InputType::isDateTimeField() const
{
    return false;
}

bool InputType::isDateTimeLocalField() const
{
    return false;
}

bool InputType::isMonthField() const
{
    return false;
}

bool InputType::isTimeField() const
{
    return false;
}

bool InputType::isWeekField() const
{
    return false;
}

bool InputType::isEnumeratable()
{
    return true;
}

bool InputType::isCheckable()
{
    return false;
}

bool InputType::isSteppable() const
{
    return false;
}

#if ENABLE(INPUT_TYPE_COLOR)
bool InputType::isColorControl() const
{
    return false;
}
#endif

bool InputType::shouldRespectHeightAndWidthAttributes()
{
    return false;
}

bool InputType::supportsPlaceholder() const
{
    return false;
}

bool InputType::usesFixedPlaceholder() const
{
    return false;
}

String InputType::fixedPlaceholder()
{
    return String();
}

void InputType::updatePlaceholderText()
{
}

void InputType::multipleAttributeChanged()
{
}

void InputType::disabledAttributeChanged()
{
}

void InputType::readonlyAttributeChanged()
{
}

String InputType::defaultToolTip() const
{
    return String();
}

bool InputType::supportsIndeterminateAppearance() const
{
    return false;
}

unsigned InputType::height() const
{
    return 0;
}

unsigned InputType::width() const
{
    return 0;
}

void InputType::applyStep(double count, AnyStepHandling anyStepHandling, TextFieldEventBehavior eventBehavior, ExceptionCode& ec)
{
    double step;
    unsigned stepDecimalPlaces, currentDecimalPlaces;
    if (!getAllowedValueStepWithDecimalPlaces(anyStepHandling, &step, &stepDecimalPlaces)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    const double nan = numeric_limits<double>::quiet_NaN();
    double current = parseToDoubleWithDecimalPlaces(element()->value(), nan, &currentDecimalPlaces);
    if (!isfinite(current)) {
        ec = INVALID_STATE_ERR;
        return;
    }
    double newValue = current + step * count;
    if (isinf(newValue)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    double acceptableErrorValue = acceptableError(step);
    if (newValue - minimum() < -acceptableErrorValue) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (newValue < minimum())
        newValue = minimum();

    const AtomicString& stepString = element()->fastGetAttribute(stepAttr);
    if (!equalIgnoringCase(stepString, "any"))
        newValue = alignValueForStep(newValue, step, currentDecimalPlaces, stepDecimalPlaces);

    if (newValue - maximum() > acceptableErrorValue) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (newValue > maximum())
        newValue = maximum();

    element()->setValueAsNumber(newValue, ec, eventBehavior);

    if (AXObjectCache::accessibilityEnabled())
         element()->document()->axObjectCache()->postNotification(element()->renderer(), AXObjectCache::AXValueChanged, true);
}

double InputType::alignValueForStep(double newValue, double step, unsigned currentDecimalPlaces, unsigned stepDecimalPlaces)
{
    if (newValue >= pow(10.0, 21.0))
        return newValue;

    unsigned baseDecimalPlaces;
    double base = stepBaseWithDecimalPlaces(&baseDecimalPlaces);
    baseDecimalPlaces = min(baseDecimalPlaces, 16u);
    if (element()->stepMismatch(element()->value())) {
        double scale = pow(10.0, static_cast<double>(max(stepDecimalPlaces, currentDecimalPlaces)));
        newValue = round(newValue * scale) / scale;
    } else {
        double scale = pow(10.0, static_cast<double>(max(stepDecimalPlaces, baseDecimalPlaces)));
        newValue = round((base + round((newValue - base) / step) * step) * scale) / scale;
    }

    return newValue;
}

bool InputType::getAllowedValueStep(double* step) const
{
    return getAllowedValueStepWithDecimalPlaces(RejectAny, step, 0);
}

bool InputType::getAllowedValueStepWithDecimalPlaces(AnyStepHandling anyStepHandling, double* step, unsigned* decimalPlaces) const
{
    ASSERT(step);
    double defaultStepValue = defaultStep();
    double stepScaleFactorValue = stepScaleFactor();
    if (!isfinite(defaultStepValue) || !isfinite(stepScaleFactorValue))
        return false;
    const AtomicString& stepString = element()->fastGetAttribute(stepAttr);
    if (stepString.isEmpty()) {
        *step = defaultStepValue * stepScaleFactorValue;
        if (decimalPlaces)
            *decimalPlaces = 0;
        return true;
    }

    if (equalIgnoringCase(stepString, "any")) {
        switch (anyStepHandling) {
        case RejectAny:
            return false;
        case AnyIsDefaultStep:
            *step = defaultStepValue * stepScaleFactorValue;
            if (decimalPlaces)
                *decimalPlaces = 0;
            return true;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    double parsed;
    if (!decimalPlaces) {
        if (!parseToDoubleForNumberType(stepString, &parsed) || parsed <= 0.0) {
            *step = defaultStepValue * stepScaleFactorValue;
            return true;
        }
    } else {
        if (!parseToDoubleForNumberTypeWithDecimalPlaces(stepString, &parsed, decimalPlaces) || parsed <= 0.0) {
            *step = defaultStepValue * stepScaleFactorValue;
            *decimalPlaces = 0;
            return true;
        }
    }
    // For date, month, week, the parsed value should be an integer for some types.
    if (parsedStepValueShouldBeInteger())
        parsed = max(round(parsed), 1.0);
    double result = parsed * stepScaleFactorValue;
    // For datetime, datetime-local, time, the result should be an integer.
    if (scaledStepValueShouldBeInteger())
        result = max(round(result), 1.0);
    ASSERT(result > 0);
    *step = result;
    return true;
}

void InputType::stepUp(int n, ExceptionCode& ec)
{
    applyStep(n, RejectAny, DispatchNoEvent, ec);
}

void InputType::stepUpFromRenderer(int n)
{
    // The differences from stepUp()/stepDown():
    //
    // Difference 1: the current value
    // If the current value is not a number, including empty, the current value is assumed as 0.
    //   * If 0 is in-range, and matches to step value
    //     - The value should be the +step if n > 0
    //     - The value should be the -step if n < 0
    //     If -step or +step is out of range, new value should be 0.
    //   * If 0 is smaller than the minimum value
    //     - The value should be the minimum value for any n
    //   * If 0 is larger than the maximum value
    //     - The value should be the maximum value for any n
    //   * If 0 is in-range, but not matched to step value
    //     - The value should be the larger matched value nearest to 0 if n > 0
    //       e.g. <input type=number min=-100 step=3> -> 2
    //     - The value should be the smaler matched value nearest to 0 if n < 0
    //       e.g. <input type=number min=-100 step=3> -> -1
    //   As for date/datetime-local/month/time/week types, the current value is assumed as "the current local date/time".
    //   As for datetime type, the current value is assumed as "the current date/time in UTC".
    // If the current value is smaller than the minimum value:
    //  - The value should be the minimum value if n > 0
    //  - Nothing should happen if n < 0
    // If the current value is larger than the maximum value:
    //  - The value should be the maximum value if n < 0
    //  - Nothing should happen if n > 0
    //
    // Difference 2: clamping steps
    // If the current value is not matched to step value:
    // - The value should be the larger matched value nearest to 0 if n > 0
    //   e.g. <input type=number value=3 min=-100 step=3> -> 5
    // - The value should be the smaler matched value nearest to 0 if n < 0
    //   e.g. <input type=number value=3 min=-100 step=3> -> 2
    //
    // n is assumed as -n if step < 0.

    ASSERT(isSteppable());
    if (!isSteppable())
        return;
    ASSERT(n);
    if (!n)
        return;

    unsigned stepDecimalPlaces, baseDecimalPlaces;
    double step, base;
    // FIXME: Not any changes after stepping, even if it is an invalid value, may be better.
    // (e.g. Stepping-up for <input type="number" value="foo" step="any" /> => "foo")
    if (!getAllowedValueStepWithDecimalPlaces(AnyIsDefaultStep, &step, &stepDecimalPlaces))
      return;
    base = stepBaseWithDecimalPlaces(&baseDecimalPlaces);
    baseDecimalPlaces = min(baseDecimalPlaces, 16u);

    int sign;
    if (step > 0)
        sign = n;
    else if (step < 0)
        sign = -n;
    else
        sign = 0;

    const double nan = numeric_limits<double>::quiet_NaN();
    String currentStringValue = element()->value();
    double current = parseToDouble(currentStringValue, nan);
    if (!isfinite(current)) {
        ExceptionCode ec;
        current = defaultValueForStepUp();
        double nextDiff = step * n;
        if (current < minimum() - nextDiff)
            current = minimum() - nextDiff;
        if (current > maximum() - nextDiff)
            current = maximum() - nextDiff;
        element()->setValueAsNumber(current, ec, DispatchInputAndChangeEvent);
    }
    if ((sign > 0 && current < minimum()) || (sign < 0 && current > maximum()))
        element()->setValue(serialize(sign > 0 ? minimum() : maximum()), DispatchInputAndChangeEvent);
    else {
        ExceptionCode ec;
        if (element()->stepMismatch(element()->value())) {
            ASSERT(step);
            double newValue;
            double scale = pow(10.0, static_cast<double>(max(stepDecimalPlaces, baseDecimalPlaces)));

            if (sign < 0)
                newValue = round((base + floor((current - base) / step) * step) * scale) / scale;
            else if (sign > 0)
                newValue = round((base + ceil((current - base) / step) * step) * scale) / scale;
            else
                newValue = current;

            if (newValue < minimum())
                newValue = minimum();
            if (newValue > maximum())
                newValue = maximum();

            element()->setValueAsNumber(newValue, ec, n == 1 || n == -1 ? DispatchInputAndChangeEvent : DispatchNoEvent);
            current = newValue;
            if (n > 1)
                applyStep(n - 1, AnyIsDefaultStep, DispatchInputAndChangeEvent, ec);
            else if (n < -1)
                applyStep(n + 1, AnyIsDefaultStep, DispatchInputAndChangeEvent, ec);
        } else
            applyStep(n, AnyIsDefaultStep, DispatchInputAndChangeEvent, ec);
    }
}

namespace InputTypeNames {

// The type names must be lowercased because they will be the return values of
// input.type and input.type must be lowercase according to DOM Level 2.

const AtomicString& button()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("button"));
    return name;
}

const AtomicString& checkbox()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("checkbox"));
    return name;
}

#if ENABLE(INPUT_TYPE_COLOR)
const AtomicString& color()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("color"));
    return name;
}
#endif

const AtomicString& date()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("date"));
    return name;
}

const AtomicString& datetime()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("datetime"));
    return name;
}

const AtomicString& datetimelocal()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("datetime-local"));
    return name;
}

const AtomicString& email()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("email"));
    return name;
}

const AtomicString& file()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("file"));
    return name;
}

const AtomicString& hidden()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("hidden"));
    return name;
}

const AtomicString& image()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("image"));
    return name;
}

const AtomicString& month()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("month"));
    return name;
}

const AtomicString& number()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("number"));
    return name;
}

const AtomicString& password()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("password"));
    return name;
}

const AtomicString& radio()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("radio"));
    return name;
}

const AtomicString& range()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("range"));
    return name;
}

const AtomicString& reset()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("reset"));
    return name;
}

const AtomicString& search()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("search"));
    return name;
}

const AtomicString& submit()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("submit"));
    return name;
}

const AtomicString& telephone()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("tel"));
    return name;
}

const AtomicString& text()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("text"));
    return name;
}

const AtomicString& time()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("time"));
    return name;
}

const AtomicString& url()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("url"));
    return name;
}

const AtomicString& week()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("week"));
    return name;
}

} // namespace WebCore::InputTypeNames
} // namespace WebCore
