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

#include "AXObjectCache.h"
#include "FloatConversion.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLProgressElement.h"
#include "LocalizedStrings.h"
#include "RenderMeter.h"
#include "RenderObject.h"
#include "RenderProgress.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityProgressIndicator::AccessibilityProgressIndicator(RenderProgress* renderer)
    : AccessibilityRenderObject(renderer)
{
}

Ref<AccessibilityProgressIndicator> AccessibilityProgressIndicator::create(RenderProgress* renderer)
{
    return adoptRef(*new AccessibilityProgressIndicator(renderer));
}
    
#if ENABLE(METER_ELEMENT)
AccessibilityProgressIndicator::AccessibilityProgressIndicator(RenderMeter* renderer)
    : AccessibilityRenderObject(renderer)
{
}

Ref<AccessibilityProgressIndicator> AccessibilityProgressIndicator::create(RenderMeter* renderer)
{
    return adoptRef(*new AccessibilityProgressIndicator(renderer));
}
#endif

bool AccessibilityProgressIndicator::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}
    
String AccessibilityProgressIndicator::valueDescription() const
{
    // If the author has explicitly provided a value through aria-valuetext, use it.
    String description = AccessibilityRenderObject::valueDescription();
    if (!description.isEmpty())
        return description;

#if ENABLE(METER_ELEMENT)
    if (!m_renderer->isMeter())
        return String();

    HTMLMeterElement* meter = meterElement();
    if (!meter)
        return String();

    // The HTML spec encourages authors to include a textual representation of the meter's state in
    // the element's contents. We'll fall back on that if there is not a more accessible alternative.
    AccessibilityObject* axMeter = axObjectCache()->getOrCreate(meter);
    if (is<AccessibilityNodeObject>(axMeter)) {
        description = downcast<AccessibilityNodeObject>(axMeter)->accessibilityDescriptionForChildren();
        if (!description.isEmpty())
            return description;
    }

    return meter->textContent();
#endif

    return String();
}

float AccessibilityProgressIndicator::valueForRange() const
{
    if (!m_renderer)
        return 0.0;
    
    if (m_renderer->isProgress()) {
        HTMLProgressElement* progress = progressElement();
        if (progress && progress->position() >= 0)
            return narrowPrecisionToFloat(progress->value());
    }

#if ENABLE(METER_ELEMENT)
    if (m_renderer->isMeter()) {
        if (HTMLMeterElement* meter = meterElement())
            return narrowPrecisionToFloat(meter->value());
    }
#endif

    // Indeterminate progress bar should return 0.
    return 0.0;
}

float AccessibilityProgressIndicator::maxValueForRange() const
{
    if (!m_renderer)
        return 0.0;

    if (m_renderer->isProgress()) {
        if (HTMLProgressElement* progress = progressElement())
            return narrowPrecisionToFloat(progress->max());
    }
    
#if ENABLE(METER_ELEMENT)
    if (m_renderer->isMeter()) {
        if (HTMLMeterElement* meter = meterElement())
            return narrowPrecisionToFloat(meter->max());
    }
#endif

    return 0.0;
}

float AccessibilityProgressIndicator::minValueForRange() const
{
    if (!m_renderer)
        return 0.0;
    
    if (m_renderer->isProgress())
        return 0.0;
    
#if ENABLE(METER_ELEMENT)
    if (m_renderer->isMeter()) {
        if (HTMLMeterElement* meter = meterElement())
            return narrowPrecisionToFloat(meter->min());
    }
#endif
    
    return 0.0;
}
    
AccessibilityRole AccessibilityProgressIndicator::roleValue() const
{
#if ENABLE(METER_ELEMENT)
    if (meterElement())
        return AccessibilityRole::Meter;
#endif
    return AccessibilityRole::ProgressIndicator;
}

HTMLProgressElement* AccessibilityProgressIndicator::progressElement() const
{
    if (!is<RenderProgress>(*m_renderer))
        return nullptr;
    
    return downcast<RenderProgress>(*m_renderer).progressElement();
}

#if ENABLE(METER_ELEMENT)
HTMLMeterElement* AccessibilityProgressIndicator::meterElement() const
{
    if (!is<RenderMeter>(*m_renderer))
        return nullptr;

    return downcast<RenderMeter>(*m_renderer).meterElement();
}

String AccessibilityProgressIndicator::gaugeRegionValueDescription() const
{
#if PLATFORM(COCOA)
    if (!m_renderer || !m_renderer->isMeter())
        return String();
    
    // Only expose this when the author has explicitly specified the following attributes.
    if (!hasAttribute(lowAttr) && !hasAttribute(highAttr) && !hasAttribute(optimumAttr))
        return String();
    
    if (HTMLMeterElement* element = meterElement()) {
        switch (element->gaugeRegion()) {
        case HTMLMeterElement::GaugeRegionOptimum:
            return AXMeterGaugeRegionOptimumText();
        case HTMLMeterElement::GaugeRegionSuboptimal:
            return AXMeterGaugeRegionSuboptimalText();
        case HTMLMeterElement::GaugeRegionEvenLessGood:
            return AXMeterGaugeRegionLessGoodText();
        default:
            break;
        }
    }
#endif
    return String();
}
#endif

Element* AccessibilityProgressIndicator::element() const
{
    if (m_renderer->isProgress())
        return progressElement();

#if ENABLE(METER_ELEMENT)
    if (m_renderer->isMeter())
        return meterElement();
#endif

    return AccessibilityObject::element();
}

} // namespace WebCore

