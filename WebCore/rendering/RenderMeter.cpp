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

#if ENABLE(METER_TAG)

#include "RenderMeter.h"

#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "RenderTheme.h"
#include "ShadowElement.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderMeter::RenderMeter(HTMLMeterElement* element)
    : RenderIndicator(element)
{
}

RenderMeter::~RenderMeter()
{
    if (m_valuePart)
        m_valuePart->detach();
    if (m_barPart)
        m_barPart->detach();
}

void RenderMeter::computeLogicalWidth()
{
    RenderBox::computeLogicalWidth();
    setWidth(theme()->meterSizeForBounds(this, frameRect()).width());
}

void RenderMeter::computeLogicalHeight()
{
    RenderBox::computeLogicalHeight();
    setHeight(theme()->meterSizeForBounds(this, frameRect()).height());
}

void RenderMeter::layoutParts()
{
    // We refresh shadow node here because the state can depend
    // on the frame size of this render object.
    updatePartsState();
    if (m_valuePart)
        m_valuePart->layoutAsPart(valuePartRect());
    if (m_barPart)
        m_barPart->layoutAsPart(barPartRect());
}

bool RenderMeter::shouldHaveParts() const
{
    bool hasTheme = theme()->supportsMeter(style()->appearance(), isHorizontal());
    if (!hasTheme)
        return true;
    bool shadowsHaveStyle = ShadowBlockElement::partShouldHaveStyle(this, barPseudoId()) || ShadowBlockElement::partShouldHaveStyle(this, valuePseudoId());
    if (shadowsHaveStyle)
        return true;
    return false;
}

double RenderMeter::valueRatio() const
{
    HTMLMeterElement* element = static_cast<HTMLMeterElement*>(node());
    double min = element->min();
    double max = element->max();
    double value = element->value();

    if (max <= min)
        return 0;
    return (value - min) / (max - min);
}

IntRect RenderMeter::barPartRect() const
{
    return IntRect(borderLeft() + paddingLeft(), borderTop() + paddingTop(), lround(width() - borderLeft() - paddingLeft() - borderRight() - paddingRight()), height()  - borderTop() - paddingTop() - borderBottom() - paddingBottom());
}

IntRect RenderMeter::valuePartRect() const
{
    IntRect rect = barPartRect();
    
    if (rect.height() <= rect.width()) {
        int width = static_cast<int>(rect.width()*valueRatio());
        if (style()->direction() == RTL) {
            rect.setX(rect.x() + (rect.width() - width));
            rect.setWidth(width);
        } else
            rect.setWidth(width);
    } else {
        int height = static_cast<int>(rect.height()*valueRatio());
        rect.setY(rect.y() + (rect.height() - height));
        rect.setHeight(height);
    }

    return rect;
}

bool RenderMeter::isHorizontal() const
{
    IntRect rect = barPartRect();
    return rect.height() <= rect.width();
}

PseudoId RenderMeter::valuePseudoId() const
{
    HTMLMeterElement* element = static_cast<HTMLMeterElement*>(node());

    if (isHorizontal()) {
        switch (element->gaugeRegion()) {
        case HTMLMeterElement::GaugeRegionOptimum:
            return METER_HORIZONTAL_OPTIMUM;
        case HTMLMeterElement::GaugeRegionSuboptimal:
            return METER_HORIZONTAL_SUBOPTIMAL;
        case HTMLMeterElement::GaugeRegionEvenLessGood:
            return METER_HORIZONTAL_EVEN_LESS_GOOD;
        }
    } else {
        switch (element->gaugeRegion()) {
        case HTMLMeterElement::GaugeRegionOptimum:
            return METER_VERTICAL_OPTIMUM;
        case HTMLMeterElement::GaugeRegionSuboptimal:
            return METER_VERTICAL_SUBOPTIMAL;
        case HTMLMeterElement::GaugeRegionEvenLessGood:
            return METER_VERTICAL_EVEN_LESS_GOOD;
        }
    }

    ASSERT_NOT_REACHED();
    return NOPSEUDO;
}

PseudoId RenderMeter::barPseudoId() const
{
    return isHorizontal() ? METER_HORIZONTAL_BAR : METER_VERTICAL_BAR;
}

void RenderMeter::updatePartsState()
{
    if (shouldHaveParts() && !m_barPart) {
        ASSERT(!m_valuePart);
        m_barPart = ShadowBlockElement::createForPart(static_cast<HTMLElement*>(node()), barPseudoId());
        addChild(m_barPart->renderer());
        m_valuePart = ShadowBlockElement::createForPart(static_cast<HTMLElement*>(node()), valuePseudoId());
        addChild(m_valuePart->renderer());
    } else if (!shouldHaveParts() && m_barPart) {
        ASSERT(m_valuePart);
        m_barPart->detach();
        m_barPart = 0;
        m_valuePart->detach();
        m_valuePart = 0;
    }

    if (m_barPart) {
        ASSERT(m_valuePart);
        m_barPart->updateStyleForPart(barPseudoId());
        m_valuePart->updateStyleForPart(valuePseudoId());
    }
}

} // namespace WebCore

#endif
