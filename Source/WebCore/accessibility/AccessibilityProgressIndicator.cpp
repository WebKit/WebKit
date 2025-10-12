/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "AccessibilityProgressIndicator.h"

#include "AXObjectCacheInlines.h"
#include "AccessibilityObjectInlines.h"
#include "FloatConversion.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLProgressElement.h"
#include "LocalizedStrings.h"
#include "RenderMeter.h"
#include "RenderObject.h"
#include "RenderProgress.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityProgressIndicator::AccessibilityProgressIndicator(AXID axID, RenderObject& renderer, AXObjectCache& cache)
    : AccessibilityRenderObject(axID, renderer, cache)
{
    ASSERT(is<RenderProgress>(renderer) || is<RenderMeter>(renderer) || is<HTMLProgressElement>(renderer.node()) || is<HTMLMeterElement>(renderer.node()));
}

AccessibilityProgressIndicator::AccessibilityProgressIndicator(AXID axID, Element& element, AXObjectCache& cache)
    : AccessibilityRenderObject(axID, element, cache)
{
    ASSERT(is<HTMLProgressElement>(element) || is<HTMLMeterElement>(element));
}

Ref<AccessibilityProgressIndicator> AccessibilityProgressIndicator::create(AXID axID, RenderObject& renderer, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityProgressIndicator(axID, renderer, cache));
}

Ref<AccessibilityProgressIndicator> AccessibilityProgressIndicator::create(AXID axID, Element& element, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityProgressIndicator(axID, element, cache));
}

bool AccessibilityProgressIndicator::computeIsIgnored() const
{
    return isIgnoredByDefault();
}

String AccessibilityProgressIndicator::valueDescription() const
{
    // If the author has explicitly provided a value through aria-valuetext, use it.
    String description = AccessibilityRenderObject::valueDescription();
    if (!description.isEmpty())
        return description;

    RefPtr meter = meterElement();
    if (!meter)
        return description;

    // The HTML spec encourages authors to include a textual representation of the meter's state in
    // the element's contents. We'll fall back on that if there is not a more accessible alternative.
    if (RefPtr nodeObject = dynamicDowncast<AccessibilityNodeObject>(axObjectCache()->getOrCreate(*meter)))
        description = nodeObject->accessibilityDescriptionForChildren();

    if (description.isEmpty())
        description = meter->textContent();

    String gaugeRegionValue = gaugeRegionValueDescription();
    if (!gaugeRegionValue.isEmpty())
        description = description.isEmpty() ? gaugeRegionValue : makeString(description, ", "_s,  gaugeRegionValue);

    return description;
}

bool AccessibilityProgressIndicator::isIndeterminate() const
{
    if (RefPtr progress = progressElement())
        return !progress->hasAttribute(valueAttr);
    return false;
}

float AccessibilityProgressIndicator::valueForRange() const
{
    if (RefPtr progress = progressElement(); progress && progress->position() >= 0)
        return narrowPrecisionToFloat(progress->value());

    if (RefPtr meter = meterElement())
        return narrowPrecisionToFloat(meter->value());

    // Indeterminate progress bar should return 0.
    return 0.0;
}

float AccessibilityProgressIndicator::maxValueForRange() const
{
    if (RefPtr progress = progressElement())
        return narrowPrecisionToFloat(progress->max());

    if (RefPtr meter = meterElement())
        return narrowPrecisionToFloat(meter->max());

    return 0.0;
}

float AccessibilityProgressIndicator::minValueForRange() const
{
    if (progressElement())
        return 0.0;

    if (RefPtr meter = meterElement())
        return narrowPrecisionToFloat(meter->min());

    return 0.0;
}

AccessibilityRole AccessibilityProgressIndicator::determineAccessibilityRole()
{
    if (meterElement())
        return AccessibilityRole::Meter;
    return AccessibilityRole::ProgressIndicator;
}

HTMLProgressElement* AccessibilityProgressIndicator::progressElement() const
{
    return dynamicDowncast<HTMLProgressElement>(node());
}

HTMLMeterElement* AccessibilityProgressIndicator::meterElement() const
{
    return dynamicDowncast<HTMLMeterElement>(node());
}

String AccessibilityProgressIndicator::gaugeRegionValueDescription() const
{
#if PLATFORM(COCOA)
    RefPtr meterElement = this->meterElement();
    if (!meterElement)
        return String();

    // Only expose this when the author has explicitly specified the following attributes.
    if (!hasAttribute(lowAttr) && !hasAttribute(highAttr) && !hasAttribute(optimumAttr))
        return String();

    switch (meterElement->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        return AXMeterGaugeRegionOptimumText();
    case HTMLMeterElement::GaugeRegionSuboptimal:
        return AXMeterGaugeRegionSuboptimalText();
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        return AXMeterGaugeRegionLessGoodText();
    }
#endif
    return String();
}

} // namespace WebCore
