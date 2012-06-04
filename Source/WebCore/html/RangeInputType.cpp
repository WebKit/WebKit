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
#include "ElementShadow.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderSlider.h"
#include "ShadowRoot.h"
#include "SliderThumbElement.h"
#include "StepRange.h"
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

static const int rangeDefaultMinimum = 0;
static const int rangeDefaultMaximum = 100;
static const int rangeDefaultStep = 1;
static const int rangeDefaultStepBase = 0;
static const int rangeStepScaleFactor = 1;

static double ensureMaximum(double proposedValue, double minimum, double fallbackValue)
{
    return proposedValue >= minimum ? proposedValue : std::max(minimum, fallbackValue);
}

PassOwnPtr<InputType> RangeInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new RangeInputType(element));
}

bool RangeInputType::isRangeControl() const
{
    return true;
}

const AtomicString& RangeInputType::formControlType() const
{
    return InputTypeNames::range();
}

double RangeInputType::valueAsNumber() const
{
    return parseToNumber(element()->value(), numeric_limits<double>::quiet_NaN());
}

void RangeInputType::setValueAsNumber(double newValue, TextFieldEventBehavior eventBehavior, ExceptionCode&) const
{
    element()->setValue(serialize(newValue), eventBehavior);
}

bool RangeInputType::supportsRequired() const
{
    return false;
}

StepRange RangeInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (rangeDefaultStep, rangeDefaultStepBase, rangeStepScaleFactor));

    double minimum = parseToNumber(element()->fastGetAttribute(minAttr), rangeDefaultMinimum);
    double maximum = ensureMaximum(parseToNumber(element()->fastGetAttribute(maxAttr), rangeDefaultMaximum), minimum, rangeDefaultMaximum);

    const AtomicString& precisionValue = element()->fastGetAttribute(precisionAttr);
    if (!precisionValue.isNull()) {
        StepRange::NumberWithDecimalPlacesOrMissing step(1, !equalIgnoringCase(precisionValue, "float"));
        return StepRange(minimum, minimum, maximum, step, stepDescription);
    }

    StepRange::NumberWithDecimalPlacesOrMissing step = StepRange::parseStep(anyStepHandling, stepDescription, element()->fastGetAttribute(stepAttr));
    return StepRange(minimum, minimum, maximum, step, stepDescription);
}

bool RangeInputType::isSteppable() const
{
    return true;
}

void RangeInputType::handleMouseDownEvent(MouseEvent* event)
{
    if (element()->disabled() || element()->readOnly())
        return;

    Node* targetNode = event->target()->toNode();
    if (event->button() != LeftButton || !targetNode)
        return;
    ASSERT(element()->shadow());
    if (targetNode != element() && !targetNode->isDescendantOf(element()->shadow()->oldestShadowRoot()))
        return;
    SliderThumbElement* thumb = sliderThumbElementOf(element());
    if (targetNode == thumb)
        return;
    thumb->dragFrom(event->absoluteLocation());
}

void RangeInputType::handleKeydownEvent(KeyboardEvent* event)
{
    if (element()->disabled() || element()->readOnly())
        return;

    const String& key = event->keyIdentifier();

    double current = parseToNumber(element()->value(), numeric_limits<double>::quiet_NaN());
    ASSERT(isfinite(current));

    StepRange stepRange(createStepRange(RejectAny));

    double step, bigStep;
    if (equalIgnoringCase(element()->fastGetAttribute(stepAttr), "any")) {
        // FIXME: We can't use stepUp() for the step value "any". So, we increase
        // or decrease the value by 1/100 of the value range. Is it reasonable?
        step = (stepRange.maximum() - stepRange.minimum()) / 100;
        bigStep = step * 10;
    } else {
        if (!element()->getAllowedValueStep(&step))
            ASSERT_NOT_REACHED();

        bigStep = (stepRange.maximum() - stepRange.minimum()) / 10;
        if (bigStep < step)
            bigStep = step;
    }

    bool isVertical = false;
    if (element()->renderer()) {
        ControlPart part = element()->renderer()->style()->appearance();
        isVertical = part == SliderVerticalPart || part == MediaVolumeSliderPart;
    }

    double newValue;
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
        ExceptionCode ec;
        TextFieldEventBehavior eventBehavior = DispatchChangeEvent;
        setValueAsNumber(newValue, eventBehavior, ec);

        if (AXObjectCache::accessibilityEnabled())
            element()->document()->axObjectCache()->postNotification(element()->renderer(), AXObjectCache::AXValueChanged, true);
        element()->dispatchFormControlChangeEvent();
    }

    event->setDefaultHandled();
}

void RangeInputType::createShadowSubtree()
{
    ASSERT(element()->shadow());

    Document* document = element()->document();
    RefPtr<HTMLDivElement> track = HTMLDivElement::create(document);
    track->setShadowPseudoId("-webkit-slider-runnable-track");
    ExceptionCode ec = 0;
    track->appendChild(SliderThumbElement::create(document), ec);
    RefPtr<HTMLElement> container = SliderContainerElement::create(document);
    container->appendChild(track.release(), ec);
    container->appendChild(TrackLimiterElement::create(document), ec);
    element()->shadow()->oldestShadowRoot()->appendChild(container.release(), ec);
}

RenderObject* RangeInputType::createRenderer(RenderArena* arena, RenderStyle*) const
{
    return new (arena) RenderSlider(element());
}

double RangeInputType::parseToNumber(const String& src, double defaultValue) const
{
    return parseToDoubleForNumberType(src, defaultValue);
}

String RangeInputType::serialize(double value) const
{
    if (!isfinite(value))
        return String();
    return serializeForNumberType(value);
}

// FIXME: Could share this with BaseClickableWithKeyInputType and BaseCheckableInputType if we had a common base class.
void RangeInputType::accessKeyAction(bool sendMouseEvents)
{
    InputType::accessKeyAction(sendMouseEvents);

    // Send mouse button events if the caller specified sendMouseEvents.
    // FIXME: The comment above is no good. It says what we do, but not why.
    element()->dispatchSimulatedClick(0, sendMouseEvents);
}

void RangeInputType::minOrMaxAttributeChanged()
{
    InputType::minOrMaxAttributeChanged();

    // Sanitize the value.
    if (element()->hasDirtyValue())
        element()->setValue(element()->value());
    element()->setNeedsStyleRecalc();
}

void RangeInputType::setValue(const String& value, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    InputType::setValue(value, valueChanged, eventBehavior);

    if (!valueChanged)
        return;

    sliderThumbElementOf(element())->setPositionFromValue();
}

String RangeInputType::fallbackValue() const
{
    return serializeForNumberType(createStepRange(RejectAny).defaultValue());
}

String RangeInputType::sanitizeValue(const String& proposedValue) const
{
    StepRange stepRange(createStepRange(RejectAny));
    double proposedDoubleValue = parseToNumber(proposedValue, stepRange.defaultValue());
    return serializeForNumberType(stepRange.clampValue(proposedDoubleValue));
}

bool RangeInputType::shouldRespectListAttribute()
{
    return InputType::themeSupportsDataListUI(this);
}

} // namespace WebCore
