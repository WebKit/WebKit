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
#if ENABLE(SVG_ANIMATION)
#include "SVGAnimateColorElement.h"

#include "Document.h"
#include "PlatformString.h"
#include "SVGColor.h"
#include "SVGSVGElement.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {
    
// FIXME: This class needs to die. SVGAnimateElement (which has superset of the functionality) should be instantiated instead.

SVGAnimateColorElement::SVGAnimateColorElement(const QualifiedName& tagName, Document* doc)
    : SVGAnimationElement(tagName, doc)
{
}

SVGAnimateColorElement::~SVGAnimateColorElement()
{
}

void SVGAnimateColorElement::applyAnimatedValueToElement(unsigned repeat)
{
    if (isAdditive()) {
        Color baseColor = SVGColor::colorFromRGBColorString(m_savedBaseValue);
        setTargetAttributeAnimatedValue(ColorDistance::addColorsAndClamp(baseColor, m_animatedColor).name());
    } else
        setTargetAttributeAnimatedValue(m_animatedColor.name());
}

bool SVGAnimateColorElement::updateAnimatedValue(float percentage)
{
    Color oldColor = m_animatedColor;
    m_animatedColor = ColorDistance(m_fromColor, m_toColor).scaledDistance(percentage).addToColorAndClamp(m_fromColor);
    return m_animatedColor != oldColor;
}
    
bool SVGAnimateColorElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    m_fromColor = SVGColor::colorFromRGBColorString(fromString);
    m_toColor = SVGColor::colorFromRGBColorString(toString);
    return true;
}

bool SVGAnimateColorElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    m_fromColor = SVGColor::colorFromRGBColorString(fromString);
    m_toColor = ColorDistance::addColorsAndClamp(m_fromColor, SVGColor::colorFromRGBColorString(byString));
    return true;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

