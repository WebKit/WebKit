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
    detachShadows();
}

PassRefPtr<ShadowBlockElement> RenderMeter::createPart(PseudoId pseudoId)
{
    RefPtr<ShadowBlockElement> element = ShadowBlockElement::createForPart(toHTMLElement(node()), pseudoId);
    if (element->renderer())
        addChild(element->renderer());
    return element;
}

void RenderMeter::updateFromElement()
{
    updateShadows();
    RenderIndicator::updateFromElement();
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
    if (shadowAttached()) {
        m_barPart->layoutAsPart(barPartRect());
        m_valuePart->layoutAsPart(valuePartRect());
    }
}

void RenderMeter::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (!oldStyle)
        return;

    if (oldStyle->appearance() != style()->appearance()) {
        detachShadows();
        updateShadows();
    }
}

bool RenderMeter::shouldHaveParts() const
{
    return !theme()->supportsMeter(style()->appearance());
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
    int width = static_cast<int>(rect.width()*valueRatio());
    if (!style()->isLeftToRightDirection()) {
        rect.setX(rect.x() + (rect.width() - width));
        rect.setWidth(width);
    } else
        rect.setWidth(width);

    return rect;
}

PseudoId RenderMeter::valuePseudoId() const
{
    HTMLMeterElement* element = static_cast<HTMLMeterElement*>(node());

    switch (element->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        return METER_OPTIMUM;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        return METER_SUBOPTIMAL;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        return METER_EVEN_LESS_GOOD;
    }

    ASSERT_NOT_REACHED();
    return NOPSEUDO;
}

PseudoId RenderMeter::barPseudoId() const
{
    return METER_BAR;
}

void RenderMeter::detachShadows()
{
    if (shadowAttached()) {
        m_valuePart->detach();
        m_valuePart = 0;
        m_barPart->detach();
        m_barPart = 0;
    }
}

void RenderMeter::updateShadows()
{
    if (!shadowAttached() && shouldHaveParts()) {
        m_barPart = createPart(barPseudoId());
        m_valuePart = createPart(valuePseudoId());
    }

    if (shadowAttached()) {
        m_barPart->updateStyleForPart(barPseudoId());
        m_valuePart->updateStyleForPart(valuePseudoId());
    }
}

} // namespace WebCore

#endif
