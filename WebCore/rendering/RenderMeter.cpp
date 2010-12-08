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

class MeterPartElement : public ShadowBlockElement {
public:
    static PassRefPtr<MeterPartElement> createForPart(HTMLElement*, PseudoId);

    void hide();
    void restoreVisibility();

    virtual void updateStyleForPart(PseudoId);

private:
    MeterPartElement(HTMLElement*);
    void saveVisibility();

    EVisibility m_originalVisibility;
};

MeterPartElement::MeterPartElement(HTMLElement* shadowParent)
    : ShadowBlockElement(shadowParent)
{
}

PassRefPtr<MeterPartElement> MeterPartElement::createForPart(HTMLElement* shadowParent, PseudoId pseudoId)
{
    RefPtr<MeterPartElement> ret = adoptRef(new MeterPartElement(shadowParent));
    ret->initAsPart(pseudoId);
    ret->saveVisibility();
    return ret;
}

void MeterPartElement::hide()
{
    if (renderer())
        renderer()->style()->setVisibility(HIDDEN);
}

void MeterPartElement::restoreVisibility()
{
    if (renderer())
        renderer()->style()->setVisibility(m_originalVisibility);
}

void MeterPartElement::updateStyleForPart(PseudoId pseudoId)
{
    if (renderer()->style()->styleType() == pseudoId)
        return;

    ShadowBlockElement::updateStyleForPart(pseudoId);
    saveVisibility();
}

void MeterPartElement::saveVisibility()
{
    m_originalVisibility = renderer()->style()->visibility();
}

RenderMeter::RenderMeter(HTMLMeterElement* element)
    : RenderIndicator(element)
{
}

RenderMeter::~RenderMeter()
{
    if (shadowAttached()) {
        m_verticalValuePart->detach();
        m_verticalBarPart->detach();
        m_horizontalValuePart->detach();
        m_horizontalBarPart->detach();
    }
}

PassRefPtr<MeterPartElement> RenderMeter::createPart(PseudoId pseudoId)
{
    RefPtr<MeterPartElement> element = MeterPartElement::createForPart(static_cast<HTMLElement*>(node()), pseudoId);
    if (element->renderer())
        addChild(element->renderer());
    return element;
}

void RenderMeter::updateFromElement()
{
    if (!shadowAttached()) {
        m_horizontalBarPart = createPart(barPseudoId(HORIZONTAL));
        m_horizontalValuePart = createPart(valuePseudoId(HORIZONTAL));
        m_verticalBarPart = createPart(barPseudoId(VERTICAL));
        m_verticalValuePart = createPart(valuePseudoId(VERTICAL));
    }

    m_horizontalBarPart->updateStyleForPart(barPseudoId(HORIZONTAL));
    m_horizontalValuePart->updateStyleForPart(valuePseudoId(HORIZONTAL));
    m_verticalBarPart->updateStyleForPart(barPseudoId(VERTICAL));
    m_verticalValuePart->updateStyleForPart(valuePseudoId(VERTICAL));
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
    m_horizontalBarPart->layoutAsPart(barPartRect());
    m_horizontalValuePart->layoutAsPart(valuePartRect(HORIZONTAL));
    m_verticalBarPart->layoutAsPart(barPartRect());
    m_verticalValuePart->layoutAsPart(valuePartRect(VERTICAL));

    if (shouldHaveParts()) {
        if (HORIZONTAL == orientation()) {
            m_verticalBarPart->hide();
            m_verticalValuePart->hide();
            m_horizontalBarPart->restoreVisibility();
            m_horizontalValuePart->restoreVisibility();
        } else {
            m_verticalBarPart->restoreVisibility();
            m_verticalValuePart->restoreVisibility();
            m_horizontalBarPart->hide();
            m_horizontalValuePart->hide();
        }
    } else {
        m_verticalBarPart->hide();
        m_verticalValuePart->hide();
        m_horizontalBarPart->hide();
        m_horizontalValuePart->hide();
    }
}

bool RenderMeter::shouldHaveParts() const
{
    EBoxOrient currentOrientation = orientation();
    bool hasTheme = theme()->supportsMeter(style()->appearance(), HORIZONTAL == currentOrientation);
    if (!hasTheme)
        return true;
    bool shadowsHaveStyle = ShadowBlockElement::partShouldHaveStyle(this, barPseudoId(currentOrientation)) || ShadowBlockElement::partShouldHaveStyle(this, valuePseudoId(currentOrientation));
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

IntRect RenderMeter::valuePartRect(EBoxOrient asOrientation) const
{
    IntRect rect = barPartRect();
    
    if (HORIZONTAL == asOrientation) {
        int width = static_cast<int>(rect.width()*valueRatio());
        if (!style()->isLeftToRightDirection()) {
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

EBoxOrient RenderMeter::orientation() const
{
    IntRect rect = barPartRect();
    return rect.height() <= rect.width() ? HORIZONTAL : VERTICAL;
}

PseudoId RenderMeter::valuePseudoId(EBoxOrient asOrientation) const
{
    HTMLMeterElement* element = static_cast<HTMLMeterElement*>(node());

    if (HORIZONTAL == asOrientation) {
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

PseudoId RenderMeter::barPseudoId(EBoxOrient asOrientation) const
{
    return HORIZONTAL == asOrientation ? METER_HORIZONTAL_BAR : METER_VERTICAL_BAR;
}

} // namespace WebCore

#endif
