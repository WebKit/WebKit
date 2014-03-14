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
#include "RangeInputType.h"

#include "AXObjectCache.h"
#include "ExceptionCodePlaceholder.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderSlider.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "SliderThumbElement.h"
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

static Decimal ensureMaximum(const Decimal& proposedValue, const Decimal& minimum, const Decimal& fallbackValue)
{
    return proposedValue >= minimum ? proposedValue : std::max(minimum, fallbackValue);
}

RangeInputType::RangeInputType(HTMLInputElement& element)
    : InputType(element)
#if ENABLE(DATALIST_ELEMENT)
    , m_tickMarkValuesDirty(true)
#endif
{
}

bool RangeInputType::isRangeControl() const
{
    return true;
}

const AtomicString& RangeInputType::formControlType() const
{
    return InputTypeNames::range();
}

double RangeInputType::valueAsDouble() const
{
    return parseToDoubleForNumberType(element().value());
}

void RangeInputType::setValueAsDecimal(const Decimal& newValue, TextFieldEventBehavior eventBehavior, ExceptionCode&) const
{
    element().setValue(serialize(newValue), eventBehavior);
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
    DEPRECATED_DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (rangeDefaultStep, rangeDefaultStepBase, rangeStepScaleFactor));

    const Decimal minimum = parseToNumber(element().fastGetAttribute(minAttr), rangeDefaultMinimum);
    const Decimal maximum = ensureMaximum(parseToNumber(element().fastGetAttribute(maxAttr), rangeDefaultMaximum), minimum, rangeDefaultMaximum);

    const AtomicString& precisionValue = element().fastGetAttribute(precisionAttr);
    if (!precisionValue.isNull()) {
        const Decimal step = equalIgnoringCase(precisionValue, "float") ? Decimal::nan() : 1;
        return StepRange(minimum, minimum, maximum, step, stepDescription);
    }

    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element().fastGetAttribute(stepAttr));
    return StepRange(minimum, minimum, maximum, step, stepDescription);
}

bool RangeInputType::isSteppable() const
{
    return true;
}

#if !PLATFORM(IOS)
void RangeInputType::handleMouseDownEvent(MouseEvent* event)
{
    if (element().isDisabledOrReadOnly())
        return;

    Node* targetNode = event->target()->toNode();
    if (event->button() != LeftButton || !targetNode)
        return;
    ASSERT(element().shadowRoot());
    if (targetNode != &element() && !targetNode->isDescendantOf(element().userAgentShadowRoot()))
        return;
    SliderThumbElement& thumb = typedSliderThumbElement();
    if (targetNode == &thumb)
        return;
    thumb.dragFrom(event->absoluteLocation());
}
#endif

#if ENABLE(TOUCH_EVENTS)
void RangeInputType::handleTouchEvent(TouchEvent* event)
{
#if PLATFORM(IOS)
    typedSliderThumbElement().handleTouchEvent(event);
#elif ENABLE(TOUCH_SLIDER)
    if (element().isDisabledOrReadOnly())
        return;

    if (event->type() == eventNames().touchendEvent) {
        event->setDefaultHandled();
        return;
    }

    TouchList* touches = event->targetTouches();
    if (touches->length() == 1) {
        typedSliderThumbElement().setPositionFromPoint(touches->item(0)->absoluteLocation());
        event->setDefaultHandled();
    }
#endif
}

#if ENABLE(TOUCH_SLIDER)
bool RangeInputType::hasTouchEventHandler() const
{
    return true;
}
#endif

#if PLATFORM(IOS)
void RangeInputType::disabledAttributeChanged()
{
    typedSliderThumbElement().disabledAttributeChanged();
}
#endif
#endif // ENABLE(TOUCH_EVENTS)

void RangeInputType::handleKeydownEvent(KeyboardEvent* event)
{
    if (element().isDisabledOrReadOnly())
        return;

    const String& key = event->keyIdentifier();

    const Decimal current = parseToNumberOrNaN(element().value());
    ASSERT(current.isFinite());

    StepRange stepRange(createStepRange(RejectAny));


    // FIXME: We can't use stepUp() for the step value "any". So, we increase
    // or decrease the value by 1/100 of the value range. Is it reasonable?
    const Decimal step = equalIgnoringCase(element().fastGetAttribute(stepAttr), "any") ? (stepRange.maximum() - stepRange.minimum()) / 100 : stepRange.step();
    const Decimal bigStep = std::max((stepRange.maximum() - stepRange.minimum()) / 10, step);

    bool isVertical = false;
    if (element().renderer()) {
        ControlPart part = element().renderer()->style().appearance();
        isVertical = part == SliderVerticalPart || part == MediaVolumeSliderPart;
    }

    Decimal newValue;
    if (key == "Up")
        newValue = current + step;
    else if (key == "Down")
        newValue = current - step;
    else if (key == "Left")
        newValue = isVertical ? current + step : current - step;
    else if (key == "Right")
        newValue = isVertical ? current - step : current + step;
    else if (key == "PageUp")
        newValue = current + bigStep;
    else if (key == "PageDown")
        newValue = current - bigStep;
    else if (key == "Home")
        newValue = isVertical ? stepRange.maximum() : stepRange.minimum();
    else if (key == "End")
        newValue = isVertical ? stepRange.minimum() : stepRange.maximum();
    else
        return; // Did not match any key binding.

    newValue = stepRange.clampValue(newValue);

    if (newValue != current) {
        EventQueueScope scope;
        TextFieldEventBehavior eventBehavior = DispatchChangeEvent;
        setValueAsDecimal(newValue, eventBehavior, IGNORE_EXCEPTION);

        if (AXObjectCache* cache = element().document().existingAXObjectCache())
            cache->postNotification(&element(), AXObjectCache::AXValueChanged);
        element().dispatchFormControlChangeEvent();
    }

    event->setDefaultHandled();
}

void RangeInputType::createShadowSubtree()
{
    ASSERT(element().userAgentShadowRoot());

    Document& document = element().document();
    RefPtr<HTMLDivElement> track = HTMLDivElement::create(document);
    track->setPseudo(AtomicString("-webkit-slider-runnable-track", AtomicString::ConstructFromLiteral));
    track->appendChild(SliderThumbElement::create(document), IGNORE_EXCEPTION);
    RefPtr<HTMLElement> container = SliderContainerElement::create(document);
    container->appendChild(track.release(), IGNORE_EXCEPTION);
    element().userAgentShadowRoot()->appendChild(container.release(), IGNORE_EXCEPTION);
}

HTMLElement* RangeInputType::sliderTrackElement() const
{
    ASSERT(element().userAgentShadowRoot());
    ASSERT(element().userAgentShadowRoot()->firstChild()); // container
    ASSERT(element().userAgentShadowRoot()->firstChild()->isHTMLElement());
    ASSERT(element().userAgentShadowRoot()->firstChild()->firstChild()); // track

    return &toHTMLElement(*element().userAgentShadowRoot()->firstChild()->firstChild());
}

SliderThumbElement& RangeInputType::typedSliderThumbElement() const
{
    ASSERT(sliderTrackElement()->firstChild()); // thumb
    ASSERT(sliderTrackElement()->firstChild()->isHTMLElement());

    return static_cast<SliderThumbElement&>(*sliderTrackElement()->firstChild());
}

HTMLElement* RangeInputType::sliderThumbElement() const
{
    return &typedSliderThumbElement();
}

RenderPtr<RenderElement> RangeInputType::createInputRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSlider>(element(), std::move(style));
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
void RangeInputType::accessKeyAction(bool sendMouseEvents)
{
    InputType::accessKeyAction(sendMouseEvents);

    element().dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

void RangeInputType::minOrMaxAttributeChanged()
{
    InputType::minOrMaxAttributeChanged();

    // Sanitize the value.
    if (element().hasDirtyValue())
        element().setValue(element().value());

    typedSliderThumbElement().setPositionFromValue();
}

void RangeInputType::setValue(const String& value, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    InputType::setValue(value, valueChanged, eventBehavior);

    if (!valueChanged)
        return;

    typedSliderThumbElement().setPositionFromValue();
}

String RangeInputType::fallbackValue() const
{
    return serializeForNumberType(createStepRange(RejectAny).defaultValue());
}

String RangeInputType::sanitizeValue(const String& proposedValue) const
{
    StepRange stepRange(createStepRange(RejectAny));
    const Decimal proposedNumericValue = parseToNumber(proposedValue, stepRange.defaultValue());
    return serializeForNumberType(stepRange.clampValue(proposedNumericValue));
}

bool RangeInputType::shouldRespectListAttribute()
{
    return InputType::themeSupportsDataListUI(this);
}

#if ENABLE(DATALIST_ELEMENT)
void RangeInputType::listAttributeTargetChanged()
{
    m_tickMarkValuesDirty = true;
    HTMLElement* sliderTrackElement = this->sliderTrackElement();
    if (sliderTrackElement->renderer())
        sliderTrackElement->renderer()->setNeedsLayout();
}

void RangeInputType::updateTickMarkValues()
{
    if (!m_tickMarkValuesDirty)
        return;
    m_tickMarkValues.clear();
    m_tickMarkValuesDirty = false;
    HTMLDataListElement* dataList = element().dataList();
    if (!dataList)
        return;
    RefPtr<HTMLCollection> options = dataList->options();
    m_tickMarkValues.reserveCapacity(options->length());
    for (unsigned i = 0; i < options->length(); ++i) {
        Node* node = options->item(i);
        HTMLOptionElement* optionElement = toHTMLOptionElement(node);
        String optionValue = optionElement->value();
        if (!element().isValidValue(optionValue))
            continue;
        m_tickMarkValues.append(parseToNumber(optionValue, Decimal::nan()));
    }
    m_tickMarkValues.shrinkToFit();
    std::sort(m_tickMarkValues.begin(), m_tickMarkValues.end());
}

Decimal RangeInputType::findClosestTickMarkValue(const Decimal& value)
{
    updateTickMarkValues();
    if (!m_tickMarkValues.size())
        return Decimal::nan();

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
    const Decimal closestLeft = middle ? m_tickMarkValues[middle - 1] : Decimal::infinity(Decimal::Negative);
    const Decimal closestRight = middle != m_tickMarkValues.size() ? m_tickMarkValues[middle] : Decimal::infinity(Decimal::Positive);
    if (closestRight - value < value - closestLeft)
        return closestRight;
    return closestLeft;
}
#endif

} // namespace WebCore
