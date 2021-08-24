/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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
#include "DOMFormData.h"
#include "DateComponents.h"
#include "DateInputType.h"
#include "DateTimeLocalInputType.h"
#include "Decimal.h"
#include "EmailInputType.h"
#include "EventNames.h"
#include "FileInputType.h"
#include "FileList.h"
#include "FormController.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HiddenInputType.h"
#include "ImageInputType.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MonthInputType.h"
#include "NodeRenderStyle.h"
#include "NumberInputType.h"
#include "Page.h"
#include "PasswordInputType.h"
#include "RadioInputType.h"
#include "RangeInputType.h"
#include "RenderElement.h"
#include "RenderTheme.h"
#include "ResetInputType.h"
#include "ScopedEventQueue.h"
#include "SearchInputType.h"
#include "SelectionRestorationMode.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StepRange.h"
#include "SubmitInputType.h"
#include "TelephoneInputType.h"
#include "TextControlInnerElements.h"
#include "TextInputType.h"
#include "TimeInputType.h"
#include "URLInputType.h"
#include "WeekInputType.h"
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

using namespace HTMLNames;

typedef bool (Settings::*InputTypeConditionalFunction)() const;
typedef const AtomString& (*InputTypeNameFunction)();
typedef Ref<InputType> (*InputTypeFactoryFunction)(HTMLInputElement&);
typedef HashMap<AtomString, std::pair<InputTypeConditionalFunction, InputTypeFactoryFunction>, ASCIICaseInsensitiveHash> InputTypeFactoryMap;

template<class T> static Ref<InputType> createInputType(HTMLInputElement& element)
{
    return adoptRef(*new T(element));
}

static InputTypeFactoryMap createInputTypeFactoryMap()
{
    static const struct InputTypes {
        InputTypeConditionalFunction conditionalFunction;
        InputTypeNameFunction nameFunction;
        InputTypeFactoryFunction factoryFunction;
    } inputTypes[] = {
        { nullptr, &InputTypeNames::button, &createInputType<ButtonInputType> },
        { nullptr, &InputTypeNames::checkbox, &createInputType<CheckboxInputType> },
#if ENABLE(INPUT_TYPE_COLOR)
        { &Settings::inputTypeColorEnabled, &InputTypeNames::color, &createInputType<ColorInputType> },
#endif
#if ENABLE(INPUT_TYPE_DATE)
        { &Settings::inputTypeDateEnabled, &InputTypeNames::date, &createInputType<DateInputType> },
#endif
#if ENABLE(INPUT_TYPE_DATETIMELOCAL)
        { &Settings::inputTypeDateTimeLocalEnabled, &InputTypeNames::datetimelocal, &createInputType<DateTimeLocalInputType> },
#endif
        { nullptr, &InputTypeNames::email, &createInputType<EmailInputType> },
        { nullptr, &InputTypeNames::file, &createInputType<FileInputType> },
        { nullptr, &InputTypeNames::hidden, &createInputType<HiddenInputType> },
        { nullptr, &InputTypeNames::image, &createInputType<ImageInputType> },
#if ENABLE(INPUT_TYPE_MONTH)
        { &Settings::inputTypeMonthEnabled, &InputTypeNames::month, &createInputType<MonthInputType> },
#endif
        { nullptr, &InputTypeNames::number, &createInputType<NumberInputType> },
        { nullptr, &InputTypeNames::password, &createInputType<PasswordInputType> },
        { nullptr, &InputTypeNames::radio, &createInputType<RadioInputType> },
        { nullptr, &InputTypeNames::range, &createInputType<RangeInputType> },
        { nullptr, &InputTypeNames::reset, &createInputType<ResetInputType> },
        { nullptr, &InputTypeNames::search, &createInputType<SearchInputType> },
        { nullptr, &InputTypeNames::submit, &createInputType<SubmitInputType> },
        { nullptr, &InputTypeNames::telephone, &createInputType<TelephoneInputType> },
#if ENABLE(INPUT_TYPE_TIME)
        { &Settings::inputTypeTimeEnabled, &InputTypeNames::time, &createInputType<TimeInputType> },
#endif
        { nullptr, &InputTypeNames::url, &createInputType<URLInputType> },
#if ENABLE(INPUT_TYPE_WEEK)
        { &Settings::inputTypeWeekEnabled, &InputTypeNames::week, &createInputType<WeekInputType> },
#endif
        // No need to register "text" because it is the default type.
    };

    InputTypeFactoryMap map;
    for (auto& inputType : inputTypes)
        map.add(inputType.nameFunction(), std::make_pair(inputType.conditionalFunction, inputType.factoryFunction));
    return map;
}

Ref<InputType> InputType::create(HTMLInputElement& element, const AtomString& typeName)
{
    if (!typeName.isEmpty()) {
        static const auto factoryMap = makeNeverDestroyed(createInputTypeFactoryMap());
        auto&& [conditional, factory] = factoryMap.get().get(typeName);
        if (factory && (!conditional || std::invoke(conditional, element.document().settings())))
            return factory(element);
    }
    return adoptRef(*new TextInputType(element));
}

Ref<InputType> InputType::createText(HTMLInputElement& element)
{
    return adoptRef(*new TextInputType(element));
}

InputType::~InputType() = default;

bool InputType::themeSupportsDataListUI(InputType* type)
{
    return RenderTheme::singleton().supportsDataListUI(type->formControlType());
}

bool InputType::shouldSaveAndRestoreFormControlState() const
{
    return true;
}

FormControlState InputType::saveFormControlState() const
{
    ASSERT(element());
    auto currentValue = element()->value();
    if (currentValue == element()->defaultValue())
        return { };
    return { { currentValue } };
}

void InputType::restoreFormControlState(const FormControlState& state)
{
    ASSERT(element());
    element()->setValue(state[0]);
}

bool InputType::isFormDataAppendable() const
{
    ASSERT(element());
    // There is no form data unless there's a name for non-image types.
    return !element()->name().isEmpty();
}

bool InputType::appendFormData(DOMFormData& formData, bool) const
{
    ASSERT(element());
    // Always successful.
    formData.append(element()->name(), element()->value());
    return true;
}

double InputType::valueAsDate() const
{
    return DateComponents::invalidMilliseconds();
}

ExceptionOr<void> InputType::setValueAsDate(double) const
{
    return Exception { InvalidStateError };
}

double InputType::valueAsDouble() const
{
    return std::numeric_limits<double>::quiet_NaN();
}

ExceptionOr<void> InputType::setValueAsDouble(double doubleValue, TextFieldEventBehavior eventBehavior) const
{
    return setValueAsDecimal(Decimal::fromDouble(doubleValue), eventBehavior);
}

ExceptionOr<void> InputType::setValueAsDecimal(const Decimal&, TextFieldEventBehavior) const
{
    return Exception { InvalidStateError };
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

bool InputType::hasBadInput() const
{
    return false;
}

bool InputType::patternMismatch(const String&) const
{
    return false;
}

bool InputType::rangeUnderflow(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return numericValue < createStepRange(AnyStepHandling::Reject).minimum();
}

bool InputType::rangeOverflow(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return numericValue > createStepRange(AnyStepHandling::Reject).maximum();
}

bool InputType::isInvalid(const String& value) const
{
    switch (m_type) {
    case Type::Button:
        return isInvalidInputType<ButtonInputType>(*this, value);
    case Type::Checkbox:
        return isInvalidInputType<CheckboxInputType>(*this, value);
    case Type::Color:
#if ENABLE(INPUT_TYPE_COLOR)
        return isInvalidInputType<ColorInputType>(*this, value);
#else
        return false;
#endif
    case Type::Date:
#if ENABLE(INPUT_TYPE_DATE)
        return isInvalidInputType<DateInputType>(*this, value);
#else
        return false;
#endif
    case Type::DateTimeLocal:
#if ENABLE(INPUT_TYPE_DATETIMELOCAL)
        return isInvalidInputType<DateTimeLocalInputType>(*this, value);
#else
        return false;
#endif
    case Type::Email:
        return isInvalidInputType<EmailInputType>(*this, value);
    case Type::File:
        return isInvalidInputType<FileInputType>(*this, value);
    case Type::Hidden:
        return isInvalidInputType<HiddenInputType>(*this, value);
    case Type::Image:
        return isInvalidInputType<ImageInputType>(*this, value);
    case Type::Month:
#if ENABLE(INPUT_TYPE_MONTH)
        return isInvalidInputType<MonthInputType>(*this, value);
#else
        return false;
#endif
    case Type::Number:
        return isInvalidInputType<NumberInputType>(*this, value);
    case Type::Password:
        return isInvalidInputType<PasswordInputType>(*this, value);
    case Type::Radio:
        return isInvalidInputType<RadioInputType>(*this, value);
    case Type::Range:
        return isInvalidInputType<RangeInputType>(*this, value);
    case Type::Reset:
        return isInvalidInputType<ResetInputType>(*this, value);
    case Type::Search:
        return isInvalidInputType<SearchInputType>(*this, value);
    case Type::Submit:
        return isInvalidInputType<SubmitInputType>(*this, value);
    case Type::Telephone:
        return isInvalidInputType<TelephoneInputType>(*this, value);
    case Type::Time:
#if ENABLE(INPUT_TYPE_TIME)
        return isInvalidInputType<TimeInputType>(*this, value);
#else
        return false;
#endif
    case Type::URL:
        return isInvalidInputType<URLInputType>(*this, value);
    case Type::Week:
#if ENABLE(INPUT_TYPE_WEEK)
        return isInvalidInputType<WeekInputType>(*this, value);
#else
        return false;
#endif
    case Type::Text:
        return isInvalidInputType<TextInputType>(*this, value);
    }
    ASSERT_NOT_REACHED();
    return false;
}

Decimal InputType::defaultValueForStepUp() const
{
    return 0;
}

double InputType::minimum() const
{
    return createStepRange(AnyStepHandling::Reject).minimum().toDouble();
}

double InputType::maximum() const
{
    return createStepRange(AnyStepHandling::Reject).maximum().toDouble();
}

bool InputType::sizeShouldIncludeDecoration(int, int& preferredSize) const
{
    ASSERT(element());
    preferredSize = element()->size();
    return false;
}

float InputType::decorationWidth() const
{
    return 0;
}

bool InputType::isInRange(const String& value) const
{
    if (!isSteppable())
        return false;

    StepRange stepRange(createStepRange(AnyStepHandling::Reject));
    if (!stepRange.hasRangeLimitations())
        return false;
    
    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return true;

    return numericValue >= stepRange.minimum() && numericValue <= stepRange.maximum();
}

bool InputType::isOutOfRange(const String& value) const
{
    if (!isSteppable() || value.isEmpty())
        return false;

    StepRange stepRange(createStepRange(AnyStepHandling::Reject));
    if (!stepRange.hasRangeLimitations())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return true;

    return numericValue < stepRange.minimum() || numericValue > stepRange.maximum();
}

bool InputType::stepMismatch(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return createStepRange(AnyStepHandling::Reject).stepMismatch(numericValue);
}

String InputType::badInputText() const
{
    ASSERT_NOT_REACHED();
    return validationMessageTypeMismatchText();
}

String InputType::typeMismatchText() const
{
    return validationMessageTypeMismatchText();
}

String InputType::valueMissingText() const
{
    return validationMessageValueMissingText();
}

String InputType::validationMessage() const
{
    ASSERT(element());
    String value = element()->value();

    // The order of the following checks is meaningful. e.g. We'd like to show the
    // badInput message even if the control has other validation errors.
    if (hasBadInput())
        return badInputText();

    if (valueMissing(value))
        return valueMissingText();

    if (typeMismatch())
        return typeMismatchText();

    if (patternMismatch(value))
        return validationMessagePatternMismatchText();

    if (element()->tooShort())
        return validationMessageTooShortText(numGraphemeClusters(value), element()->minLength());

    if (element()->tooLong())
        return validationMessageTooLongText(numGraphemeClusters(value), element()->effectiveMaxLength());

    if (!isSteppable())
        return emptyString();

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return emptyString();

    StepRange stepRange(createStepRange(AnyStepHandling::Reject));

    if (numericValue < stepRange.minimum())
        return validationMessageRangeUnderflowText(serialize(stepRange.minimum()));

    if (numericValue > stepRange.maximum())
        return validationMessageRangeOverflowText(serialize(stepRange.maximum()));

    if (stepRange.stepMismatch(numericValue)) {
        const String stepString = stepRange.hasStep() ? serializeForNumberType(stepRange.step() / stepRange.stepScaleFactor()) : emptyString();
        return validationMessageStepMismatchText(serialize(stepRange.stepBase()), stepString);
    }

    return emptyString();
}

void InputType::handleClickEvent(MouseEvent&)
{
}

void InputType::handleMouseDownEvent(MouseEvent&)
{
}

void InputType::handleDOMActivateEvent(Event&)
{
}

auto InputType::handleKeydownEvent(KeyboardEvent&) -> ShouldCallBaseEventHandler
{
    return ShouldCallBaseEventHandler::Yes;
}

void InputType::handleKeypressEvent(KeyboardEvent&)
{
}

void InputType::handleKeyupEvent(KeyboardEvent&)
{
}

void InputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent&)
{
}

#if ENABLE(TOUCH_EVENTS)
void InputType::handleTouchEvent(TouchEvent&)
{
}
#endif

void InputType::forwardEvent(Event&)
{
}

bool InputType::shouldSubmitImplicitly(Event& event)
{
    return is<KeyboardEvent>(event) && event.type() == eventNames().keypressEvent && downcast<KeyboardEvent>(event).charCode() == '\r';
}

RenderPtr<RenderElement> InputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    return RenderPtr<RenderElement>(RenderElement::createFor(*element(), WTFMove(style)));
}

void InputType::blur()
{
    ASSERT(element());
    element()->defaultBlur();
}

void InputType::createShadowSubtreeAndUpdateInnerTextElementEditability(ContainerNode::ChildChange::Source, bool)
{
}

void InputType::destroyShadowSubtree()
{
    ASSERT(element());
    RefPtr<ShadowRoot> root = element()->userAgentShadowRoot();
    if (!root)
        return;

    root->removeChildren();
}

Decimal InputType::parseToNumber(const String&, const Decimal& defaultValue) const
{
    ASSERT_NOT_REACHED();
    return defaultValue;
}

Decimal InputType::parseToNumberOrNaN(const String& string) const
{
    return parseToNumber(string, Decimal::nan());
}

String InputType::serialize(const Decimal&) const
{
    ASSERT_NOT_REACHED();
    return String();
}

DateComponentsType InputType::dateType() const
{
    return DateComponentsType::Invalid;
}

void InputType::dispatchSimulatedClickIfActive(KeyboardEvent& event) const
{
    ASSERT(element());
    if (element()->active())
        element()->dispatchSimulatedClick(&event);
    event.setDefaultHandled();
}

Chrome* InputType::chrome() const
{
    ASSERT(element());
    if (Page* page = element()->document().page())
        return &page->chrome();
    return nullptr;
}

bool InputType::canSetStringValue() const
{
    return true;
}

bool InputType::hasCustomFocusLogic() const
{
    return true;
}

bool InputType::isKeyboardFocusable(KeyboardEvent* event) const
{
    ASSERT(element());
    return !element()->isReadOnly() && element()->isTextFormControlKeyboardFocusable(event);
}

bool InputType::isMouseFocusable() const
{
    ASSERT(element());
    return element()->isTextFormControlMouseFocusable();
}

bool InputType::shouldUseInputMethod() const
{
    return false;
}

void InputType::handleFocusEvent(Node*, FocusDirection)
{
}

void InputType::handleBlurEvent()
{
}

bool InputType::accessKeyAction(bool)
{
    ASSERT(element());
    element()->focus({ SelectionRestorationMode::SelectAll });
    return false;
}

void InputType::addSearchResult()
{
}

void InputType::attach()
{
}

void InputType::detach()
{
}

bool InputType::shouldRespectAlignAttribute()
{
    return false;
}

bool InputType::canBeSuccessfulSubmitButton()
{
    return false;
}

HTMLElement* InputType::placeholderElement() const
{
    return nullptr;
}

bool InputType::rendererIsNeeded()
{
    return true;
}

FileList* InputType::files()
{
    return nullptr;
}

void InputType::setFiles(RefPtr<FileList>&&, WasSetByJavaScript)
{
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
    ASSERT(element());
    element()->setValueInternal(sanitizedValue, eventBehavior);
    if (!valueChanged)
        return;
    element()->invalidateStyleForSubtree();

    switch (eventBehavior) {
    case DispatchChangeEvent:
        element()->dispatchFormControlChangeEvent();
        break;
    case DispatchInputAndChangeEvent:
        element()->dispatchFormControlInputEvent();
        if (auto element = this->element())
            element->dispatchFormControlChangeEvent();
        break;
    case DispatchNoEvent:
        break;
    }
}

bool InputType::canSetValue(const String&)
{
    return true;
}

void InputType::willDispatchClick(InputElementClickState&)
{
}

void InputType::didDispatchClick(Event&, const InputElementClickState&)
{
}

String InputType::localizeValue(const String& proposedValue) const
{
    return proposedValue;
}

String InputType::visibleValue() const
{
    ASSERT(element());
    return element()->value();
}

bool InputType::isEmptyValue() const
{
    return true;
}

String InputType::sanitizeValue(const String& proposedValue) const
{
    return proposedValue;
}

#if ENABLE(DRAG_SUPPORT)

bool InputType::receiveDroppedFiles(const DragData&)
{
    ASSERT_NOT_REACHED();
    return false;
}

#endif

Icon* InputType::icon() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

String InputType::displayString() const
{
    ASSERT_NOT_REACHED();
    return String();
}

bool InputType::shouldResetOnDocumentActivation()
{
    return false;
}

bool InputType::shouldRespectListAttribute()
{
    return false;
}

bool InputType::isInteractiveContent() const
{
    return m_type != Type::Hidden;
}

bool InputType::supportLabels() const
{
    return m_type != Type::Hidden;
}

bool InputType::isEnumeratable() const
{
    return m_type != Type::Image;
}

bool InputType::shouldRespectHeightAndWidthAttributes()
{
    return false;
}

bool InputType::supportsPlaceholder() const
{
    return false;
}

bool InputType::supportsReadOnly() const
{
    return false;
}

void InputType::updateInnerTextValue()
{
}

void InputType::updatePlaceholderText()
{
}

void InputType::capsLockStateMayHaveChanged()
{
}

void InputType::updateAutoFillButton()
{
}

void InputType::subtreeHasChanged()
{
    ASSERT_NOT_REACHED();
}

#if ENABLE(TOUCH_EVENTS)
bool InputType::hasTouchEventHandler() const
{
    return false;
}
#endif

String InputType::defaultToolTip() const
{
    return String();
}

#if ENABLE(DATALIST_ELEMENT)
void InputType::dataListMayHaveChanged()
{
}

std::optional<Decimal> InputType::findClosestTickMarkValue(const Decimal&)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}
#endif

bool InputType::matchesIndeterminatePseudoClass() const
{
    return false;
}

bool InputType::shouldAppearIndeterminate() const
{
    return false;
}

bool InputType::isPresentingAttachedView() const
{
    return false;
}

bool InputType::supportsSelectionAPI() const
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

ExceptionOr<void> InputType::applyStep(int count, AnyStepHandling anyStepHandling, TextFieldEventBehavior eventBehavior)
{
    StepRange stepRange(createStepRange(anyStepHandling));
    if (!stepRange.hasStep())
        return Exception { InvalidStateError };

    ASSERT(element());
    const Decimal current = parseToNumberOrNaN(element()->value());
    if (!current.isFinite())
        return Exception { InvalidStateError };
    Decimal newValue = current + stepRange.step() * count;
    if (!newValue.isFinite())
        return Exception { InvalidStateError };

    const Decimal acceptableErrorValue = stepRange.acceptableError();
    if (newValue - stepRange.minimum() < -acceptableErrorValue)
        return Exception { InvalidStateError };
    if (newValue < stepRange.minimum())
        newValue = stepRange.minimum();

    if (!equalLettersIgnoringASCIICase(element()->attributeWithoutSynchronization(stepAttr), "any"))
        newValue = stepRange.alignValueForStep(current, newValue);

    if (newValue - stepRange.maximum() > acceptableErrorValue)
        return Exception { InvalidStateError };
    if (newValue > stepRange.maximum())
        newValue = stepRange.maximum();

    auto protectedThis = makeRef(*this);
    auto result = setValueAsDecimal(newValue, eventBehavior);
    if (result.hasException() || !element())
        return result;

    if (AXObjectCache* cache = element()->document().existingAXObjectCache())
        cache->postNotification(element(), AXObjectCache::AXValueChanged);

    return result;
}

bool InputType::getAllowedValueStep(Decimal* step) const
{
    StepRange stepRange(createStepRange(AnyStepHandling::Reject));
    *step = stepRange.step();
    return stepRange.hasStep();
}

StepRange InputType::createStepRange(AnyStepHandling) const
{
    ASSERT_NOT_REACHED();
    return StepRange();
}

ExceptionOr<void> InputType::stepUp(int n)
{
    if (!isSteppable())
        return Exception { InvalidStateError };
    return applyStep(n, AnyStepHandling::Reject, DispatchNoEvent);
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
    //     - The value should be the smaller matched value nearest to 0 if n < 0
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
    // - The value should be the smaller matched value nearest to 0 if n < 0
    //   e.g. <input type=number value=3 min=-100 step=3> -> 2
    //
    // n is assumed as -n if step < 0.

    ASSERT(isSteppable());
    if (!isSteppable())
        return;
    ASSERT(n);
    if (!n)
        return;

    StepRange stepRange(createStepRange(AnyStepHandling::Default));

    // FIXME: Not any changes after stepping, even if it is an invalid value, may be better.
    // (e.g. Stepping-up for <input type="number" value="foo" step="any" /> => "foo")
    if (!stepRange.hasStep())
      return;

    EventQueueScope scope;
    const Decimal step = stepRange.step();

    int sign;
    if (step > 0)
        sign = n;
    else if (step < 0)
        sign = -n;
    else
        sign = 0;

    ASSERT(element());
    String currentStringValue = element()->value();
    Decimal current = parseToNumberOrNaN(currentStringValue);
    if (!current.isFinite()) {
        current = defaultValueForStepUp();
        const Decimal nextDiff = step * n;
        if (current < stepRange.minimum() - nextDiff)
            current = stepRange.minimum() - nextDiff;
        if (current > stepRange.maximum() - nextDiff)
            current = stepRange.maximum() - nextDiff;
        setValueAsDecimal(current, DispatchNoEvent);
    }
    if ((sign > 0 && current < stepRange.minimum()) || (sign < 0 && current > stepRange.maximum()))
        setValueAsDecimal(sign > 0 ? stepRange.minimum() : stepRange.maximum(), DispatchInputAndChangeEvent);
    else {
        if (stepMismatch(element()->value())) {
            ASSERT(!step.isZero());
            const Decimal base = stepRange.stepBase();
            Decimal newValue;
            if (sign < 0)
                newValue = base + ((current - base) / step).floor() * step;
            else if (sign > 0)
                newValue = base + ((current - base) / step).ceiling() * step;
            else
                newValue = current;

            if (newValue < stepRange.minimum())
                newValue = stepRange.minimum();
            if (newValue > stepRange.maximum())
                newValue = stepRange.maximum();

            setValueAsDecimal(newValue, n == 1 || n == -1 ? DispatchInputAndChangeEvent : DispatchNoEvent);
            if (n > 1)
                applyStep(n - 1, AnyStepHandling::Default, DispatchInputAndChangeEvent);
            else if (n < -1)
                applyStep(n + 1, AnyStepHandling::Default, DispatchInputAndChangeEvent);
        } else
            applyStep(n, AnyStepHandling::Default, DispatchInputAndChangeEvent);
    }
}

Color InputType::valueAsColor() const
{
    return Color::transparentBlack;
}

void InputType::selectColor(StringView)
{
}

Vector<Color> InputType::suggestedColors() const
{
    return { };
}

RefPtr<TextControlInnerTextElement> InputType::innerTextElement() const
{
    return nullptr;
}

String InputType::resultForDialogSubmit() const
{
    ASSERT(element());
    return element()->value();
}

} // namespace WebCore
