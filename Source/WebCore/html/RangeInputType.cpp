/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#include "RangeInputType.h"

#include "Decimal.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentInlines.h"
#include "ElementChildIterator.h"
#include "ElementRareData.h"
#include "EventNames.h"
#include "HTMLCollection.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderSlider.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "ShadowPseudoIds.h"
#include "ShadowRoot.h"
#include "SliderThumbElement.h"
#include "StepRange.h"
#include <limits>
#include <wtf/MathExtras.h>

#if ENABLE(TOUCH_EVENTS)
#include "Touch.h"
#include "TouchEvent.h"
#include "TouchList.h"
#endif

#if ENABLE(DATALIST_ELEMENT)
#include "HTMLDataListElement.h"
#include "HTMLOptionElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static const int rangeDefaultMinimum = 0;
static const int rangeDefaultMaximum = 100;
static const int rangeDefaultStep = 1;
static const int rangeDefaultStepBase = 0;
static const int rangeStepScaleFactor = 1;
static const StepRange::StepDescription rangeStepDescription { rangeDefaultStep, rangeDefaultStepBase, rangeStepScaleFactor };

static Decimal ensureMaximum(const Decimal& proposedValue, const Decimal& minimum)
{
    return proposedValue >= minimum ? proposedValue : minimum;
}

RangeInputType::RangeInputType(HTMLInputElement& element)
    : InputType(Type::Range, element)
{
    ASSERT(needsShadowSubtree());
}

const AtomString& RangeInputType::formControlType() const
{
    return InputTypeNames::range();
}

double RangeInputType::valueAsDouble() const
{
    ASSERT(element());
    return parseToDoubleForNumberType(element()->value());
}

ExceptionOr<void> RangeInputType::setValueAsDecimal(const Decimal& newValue, TextFieldEventBehavior eventBehavior) const
{
    ASSERT(element());
    element()->setValue(serialize(newValue), eventBehavior);
    return { };
}

bool RangeInputType::typeMismatchFor(const String& value) const
{
    return !value.isEmpty() && !std::isfinite(parseToDoubleForNumberType(value));
}

bool RangeInputType::supportsRequired() const
{
    return false;
}

StepRange RangeInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    ASSERT(element());
    const Decimal minimum = parseToNumber(element()->attributeWithoutSynchronization(minAttr), rangeDefaultMinimum);
    const Decimal maximum = ensureMaximum(parseToNumber(element()->attributeWithoutSynchronization(maxAttr), rangeDefaultMaximum), minimum);

    const AtomString& precisionValue = element()->attributeWithoutSynchronization(precisionAttr);
    if (!precisionValue.isNull()) {
        const Decimal step = equalLettersIgnoringASCIICase(precisionValue, "float"_s) ? Decimal::nan() : 1;
        return StepRange(minimum, RangeLimitations::Valid, minimum, maximum, step, rangeStepDescription);
    }

    const Decimal step = StepRange::parseStep(anyStepHandling, rangeStepDescription, element()->attributeWithoutSynchronization(stepAttr));
    return StepRange(minimum, RangeLimitations::Valid, minimum, maximum, step, rangeStepDescription);
}

void RangeInputType::handleMouseDownEvent(MouseEvent& event)
{
    ASSERT(element());

    if (!hasCreatedShadowSubtree())
        return;

    if (element()->isDisabledFormControl())
        return;

    if (event.button() != LeftButton || !is<Node>(event.target()))
        return;
    ASSERT(element()->shadowRoot());
    auto& targetNode = downcast<Node>(*event.target());
    if (&targetNode != element() && !targetNode.isDescendantOf(element()->userAgentShadowRoot().get()))
        return;
    auto& thumb = typedSliderThumbElement();
    if (&targetNode == &thumb)
        return;
    thumb.dragFrom(event.absoluteLocation());
}

#if ENABLE(TOUCH_EVENTS)
void RangeInputType::handleTouchEvent(TouchEvent& event)
{
    ASSERT(element());

    if (!hasCreatedShadowSubtree())
        return;

#if PLATFORM(IOS_FAMILY)
    typedSliderThumbElement().handleTouchEvent(event);
#elif ENABLE(TOUCH_SLIDER)

    if (element()->isDisabledFormControl())
        return;

    if (event.type() == eventNames().touchendEvent) {
        event.setDefaultHandled();
        return;
    }

    RefPtr<TouchList> touches = event.targetTouches();
    if (touches->length() == 1) {
        typedSliderThumbElement().setPositionFromPoint(touches->item(0)->absoluteLocation());
        event.setDefaultHandled();
    }
#else
    UNUSED_PARAM(event);
#endif
}

#if ENABLE(TOUCH_SLIDER)
bool RangeInputType::hasTouchEventHandler() const
{
    return true;
}
#endif
#endif // ENABLE(TOUCH_EVENTS)

void RangeInputType::disabledStateChanged()
{
    if (!hasCreatedShadowSubtree())
        return;
    typedSliderThumbElement().hostDisabledStateChanged();
}

auto RangeInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    ASSERT(element());

    if (element()->isDisabledFormControl())
        return ShouldCallBaseEventHandler::Yes;

    const String& key = event.keyIdentifier();

    const Decimal current = parseToNumberOrNaN(element()->value());
    ASSERT(current.isFinite());

    auto stepRange = createStepRange(AnyStepHandling::Reject);

    // FIXME: We can't use stepUp() for the step value "any". So, we increase
    // or decrease the value by 1/100 of the value range. Is it reasonable?
    const Decimal step = equalLettersIgnoringASCIICase(element()->attributeWithoutSynchronization(stepAttr), "any"_s) ? (stepRange.maximum() - stepRange.minimum()) / 100 : stepRange.step();
    const Decimal bigStep = std::max((stepRange.maximum() - stepRange.minimum()) / 10, step);

    bool isVertical = false;
    if (auto* renderer = element()->renderer())
        isVertical = renderer->style().effectiveAppearance() == ControlPartType::SliderVertical;

    Decimal newValue;
    if (key == "Up"_s)
        newValue = current + step;
    else if (key == "Down"_s)
        newValue = current - step;
    else if (key == "Left"_s)
        newValue = isVertical ? current + step : current - step;
    else if (key == "Right"_s)
        newValue = isVertical ? current - step : current + step;
    else if (key == "PageUp"_s)
        newValue = current + bigStep;
    else if (key == "PageDown"_s)
        newValue = current - bigStep;
    else if (key == "Home"_s)
        newValue = isVertical ? stepRange.maximum() : stepRange.minimum();
    else if (key == "End"_s)
        newValue = isVertical ? stepRange.minimum() : stepRange.maximum();
    else
        return ShouldCallBaseEventHandler::Yes; // Did not match any key binding.

    newValue = stepRange.clampValue(newValue);

    if (newValue != current) {
        EventQueueScope scope;
        setValueAsDecimal(newValue, DispatchInputAndChangeEvent);
    }

    event.setDefaultHandled();
    return ShouldCallBaseEventHandler::Yes;
}

void RangeInputType::createShadowSubtree()
{
    ASSERT(needsShadowSubtree());
    ASSERT(element());
    ASSERT(element()->userAgentShadowRoot());

    Document& document = element()->document();

    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { *element()->userAgentShadowRoot() };

    auto track = HTMLDivElement::create(document);
    auto container = SliderContainerElement::create(document);
    element()->userAgentShadowRoot()->appendChild(ContainerNode::ChildChange::Source::Parser, container);
    container->appendChild(ContainerNode::ChildChange::Source::Parser, track);

    track->setPseudo(ShadowPseudoIds::webkitSliderRunnableTrack());
    track->appendChild(ContainerNode::ChildChange::Source::Parser, SliderThumbElement::create(document));
}

HTMLElement* RangeInputType::sliderTrackElement() const
{
    ASSERT(element());

    if (!hasCreatedShadowSubtree())
        return nullptr;

    ASSERT(element()->userAgentShadowRoot());
    ASSERT(element()->userAgentShadowRoot()->firstChild()); // container
    ASSERT(element()->userAgentShadowRoot()->firstChild()->isHTMLElement());
    ASSERT(element()->userAgentShadowRoot()->firstChild()->firstChild()); // track

    RefPtr<ShadowRoot> root = element()->userAgentShadowRoot();
    if (!root)
        return nullptr;
    
    auto* container = childrenOfType<SliderContainerElement>(*root).first();
    if (!container)
        return nullptr;

    return childrenOfType<HTMLElement>(*container).first();
}

SliderThumbElement& RangeInputType::typedSliderThumbElement() const
{
    ASSERT(hasCreatedShadowSubtree());
    ASSERT(sliderTrackElement()->firstChild()); // thumb
    ASSERT(sliderTrackElement()->firstChild()->isHTMLElement());

    return static_cast<SliderThumbElement&>(*sliderTrackElement()->firstChild());
}

HTMLElement* RangeInputType::sliderThumbElement() const
{
    return &typedSliderThumbElement();
}

RenderPtr<RenderElement> RangeInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    return createRenderer<RenderSlider>(*element(), WTFMove(style));
}

Decimal RangeInputType::parseToNumber(const String& src, const Decimal& defaultValue) const
{
    return parseToDecimalForNumberType(src, defaultValue);
}

String RangeInputType::serialize(const Decimal& value) const
{
    if (!value.isFinite())
        return String();
    return serializeForNumberType(value);
}

// FIXME: Could share this with BaseClickableWithKeyInputType and BaseCheckableInputType if we had a common base class.
bool RangeInputType::accessKeyAction(bool sendMouseEvents)
{
    auto* element = this->element();
    return InputType::accessKeyAction(sendMouseEvents) || (element && element->dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents));
}

void RangeInputType::attributeChanged(const QualifiedName& name)
{
    // FIXME: Don't we need to do this work for precisionAttr too?
    if (name == maxAttr || name == minAttr || name == valueAttr) {
        // Sanitize the value.
        if (auto* element = this->element()) {
            if (element->hasDirtyValue())
                element->setValue(element->value());
        }
        if (hasCreatedShadowSubtree())
            typedSliderThumbElement().setPositionFromValue();
    }
    InputType::attributeChanged(name);
}

void RangeInputType::setValue(const String& value, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    InputType::setValue(value, valueChanged, eventBehavior, selection);

    if (!valueChanged)
        return;

    if (eventBehavior == DispatchNoEvent) {
        ASSERT(element());
        element()->setTextAsOfLastFormControlChangeEvent(value);
    }

    if (hasCreatedShadowSubtree())
        typedSliderThumbElement().setPositionFromValue();
}

String RangeInputType::fallbackValue() const
{
    return serializeForNumberType(createStepRange(AnyStepHandling::Reject).defaultValue());
}

String RangeInputType::sanitizeValue(const String& proposedValue) const
{
    StepRange stepRange(createStepRange(AnyStepHandling::Reject));
    const Decimal proposedNumericValue = parseToNumber(proposedValue, stepRange.defaultValue());
    return serializeForNumberType(stepRange.clampValue(proposedNumericValue));
}

bool RangeInputType::shouldRespectListAttribute()
{
#if ENABLE(DATALIST_ELEMENT)
    return DeprecatedGlobalSettings::dataListElementEnabled();
#else
    return InputType::themeSupportsDataListUI(this);
#endif
}

#if ENABLE(DATALIST_ELEMENT)
void RangeInputType::dataListMayHaveChanged()
{
    m_tickMarkValuesDirty = true;
    RefPtr<HTMLElement> sliderTrackElement = this->sliderTrackElement();
    if (sliderTrackElement && sliderTrackElement->renderer())
        sliderTrackElement->renderer()->setNeedsLayout();
}

void RangeInputType::updateTickMarkValues()
{
    if (!m_tickMarkValuesDirty)
        return;
    m_tickMarkValues.clear();
    m_tickMarkValuesDirty = false;
    ASSERT(element());
    auto dataList = element()->dataList();
    if (!dataList)
        return;
    Ref<HTMLCollection> options = dataList->options();
    m_tickMarkValues.reserveCapacity(options->length());
    for (unsigned i = 0; i < options->length(); ++i) {
        RefPtr<Node> node = options->item(i);
        HTMLOptionElement& optionElement = downcast<HTMLOptionElement>(*node);
        String optionValue = optionElement.value();
        if (!element()->isValidValue(optionValue))
            continue;
        m_tickMarkValues.append(parseToNumber(optionValue, Decimal::nan()));
    }
    m_tickMarkValues.shrinkToFit();
    std::sort(m_tickMarkValues.begin(), m_tickMarkValues.end());
}

std::optional<Decimal> RangeInputType::findClosestTickMarkValue(const Decimal& value)
{
    updateTickMarkValues();
    if (!m_tickMarkValues.size())
        return std::nullopt;

    size_t left = 0;
    size_t right = m_tickMarkValues.size();
    size_t middle;
    while (true) {
        ASSERT(left <= right);
        middle = left + (right - left) / 2;
        if (!middle)
            break;
        if (middle == m_tickMarkValues.size() - 1 && m_tickMarkValues[middle] < value) {
            middle++;
            break;
        }
        if (m_tickMarkValues[middle - 1] <= value && m_tickMarkValues[middle] >= value)
            break;

        if (m_tickMarkValues[middle] < value)
            left = middle;
        else
            right = middle;
    }

    std::optional<Decimal> closestLeft = middle ? std::make_optional(m_tickMarkValues[middle - 1]) : std::nullopt;
    std::optional<Decimal> closestRight = middle != m_tickMarkValues.size() ? std::make_optional(m_tickMarkValues[middle]) : std::nullopt;

    if (!closestLeft)
        return closestRight;
    if (!closestRight)
        return closestLeft;

    if (*closestRight - value < value - *closestLeft)
        return closestRight;

    return closestLeft;
}
#endif

} // namespace WebCore
