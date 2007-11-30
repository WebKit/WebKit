/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if ENABLE(SVG)
#include "SVGAnimateColorElement.h"

#include "Document.h"
#include "TimeScheduler.h"
#include "PlatformString.h"
#include "SVGColor.h"
#include "SVGSVGElement.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

SVGAnimateColorElement::SVGAnimateColorElement(const QualifiedName& tagName, Document* doc)
    : SVGAnimationElement(tagName, doc)
{
}

SVGAnimateColorElement::~SVGAnimateColorElement()
{
}

bool SVGAnimateColorElement::updateAnimationBaseValueFromElement()
{
    m_baseColor = SVGColor::colorFromRGBColorString(targetAttributeAnimatedValue());
    m_fromColor = Color();
    m_toColor = Color();
    return true;
}

void SVGAnimateColorElement::applyAnimatedValueToElement()
{
    if (isAdditive())
        setTargetAttributeAnimatedValue(ColorDistance::addColorsAndClamp(m_baseColor, m_animatedColor).name());
    else
        setTargetAttributeAnimatedValue(m_animatedColor.name());
}

bool SVGAnimateColorElement::updateAnimatedValue(EAnimationMode animationMode, float timePercentage, unsigned valueIndex, float percentagePast)
{
    if (animationMode == TO_ANIMATION)
        // to-animations have a special equation: value = (to - base) * (time/duration) + base
        m_animatedColor = ColorDistance(m_baseColor, m_toColor).scaledDistance(timePercentage).addToColorAndClamp(m_baseColor);
    else
        m_animatedColor = ColorDistance(m_fromColor, m_toColor).scaledDistance(percentagePast).addToColorAndClamp(m_fromColor);
    return (m_animatedColor != m_baseColor);
}

bool SVGAnimateColorElement::calculateFromAndToValues(EAnimationMode animationMode, unsigned valueIndex)
{
    switch (animationMode) {
    case FROM_TO_ANIMATION:
        m_fromColor = SVGColor::colorFromRGBColorString(m_from);
        // fall through
    case TO_ANIMATION:
        m_toColor = SVGColor::colorFromRGBColorString(m_to);
        break;
    case FROM_BY_ANIMATION:
        m_fromColor = SVGColor::colorFromRGBColorString(m_from);
        m_toColor = SVGColor::colorFromRGBColorString(m_by);
        break;
    case BY_ANIMATION:
        m_fromColor = SVGColor::colorFromRGBColorString(m_from);
        m_toColor = ColorDistance::addColorsAndClamp(m_fromColor, SVGColor::colorFromRGBColorString(m_by));
        break;
    case VALUES_ANIMATION:
        m_fromColor = SVGColor::colorFromRGBColorString(m_values[valueIndex]);
        m_toColor = ((valueIndex + 1) < m_values.size()) ? SVGColor::colorFromRGBColorString(m_values[valueIndex + 1]) : m_fromColor;
        break;
    case NO_ANIMATION:
        ASSERT_NOT_REACHED();
    }
    
    return true;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

