/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>

    This file is part of the WebKit project

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
#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)
#include "SVGAnimateTransformElement.h"

#include "AffineTransform.h"
#include "RenderObject.h"
#include "SVGAngle.h"
#include "SVGParserUtilities.h"
#include "SVGSVGElement.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "TimeScheduler.h"

#include <math.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

SVGAnimateTransformElement::SVGAnimateTransformElement(const QualifiedName& tagName, Document* doc)
    : SVGAnimationElement(tagName, doc)
    , m_type(SVGTransform::SVG_TRANSFORM_UNKNOWN)
{
}

SVGAnimateTransformElement::~SVGAnimateTransformElement()
{
}

bool SVGAnimateTransformElement::hasValidTarget() const
{
    return (SVGAnimationElement::hasValidTarget() && targetElement()->isStyledTransformable());
}

void SVGAnimateTransformElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::typeAttr) {
        if (attr->value() == "translate")
            m_type = SVGTransform::SVG_TRANSFORM_TRANSLATE;
        else if (attr->value() == "scale")
            m_type = SVGTransform::SVG_TRANSFORM_SCALE;
        else if (attr->value() == "rotate")
            m_type = SVGTransform::SVG_TRANSFORM_ROTATE;
        else if (attr->value() == "skewX")
            m_type = SVGTransform::SVG_TRANSFORM_SKEWX;
        else if (attr->value() == "skewY")
            m_type = SVGTransform::SVG_TRANSFORM_SKEWY;
    } else
        SVGAnimationElement::parseMappedAttribute(attr);
}

bool SVGAnimateTransformElement::updateAnimatedValue(EAnimationMode animationMode, float timePercentage, unsigned valueIndex, float percentagePast)
{
    if (animationMode == TO_ANIMATION)
        // to-animations have a special equation: value = (to - base) * (time/duration) + base
        m_animatedTransform = SVGTransformDistance(m_baseTransform, m_toTransform).scaledDistance(timePercentage).addToSVGTransform(m_baseTransform);
    else
        m_animatedTransform = SVGTransformDistance(m_fromTransform, m_toTransform).scaledDistance(percentagePast).addToSVGTransform(m_fromTransform);
    return (m_animatedTransform != m_baseTransform);
}

bool SVGAnimateTransformElement::updateAnimationBaseValueFromElement()
{
    m_baseTransform = SVGTransform();
    m_toTransform = SVGTransform();
    m_fromTransform = SVGTransform();
    m_animatedTransform = SVGTransform();
    
    if (!targetElement()->isStyledTransformable())
        return false;
    
    SVGStyledTransformableElement* transform = static_cast<SVGStyledTransformableElement*>(targetElement());
    RefPtr<SVGTransformList> transformList = transform->transform();
    if (!transformList)
        return false;
    
    m_baseTransform = transformList->concatenateForType(m_type);
    
    // If a base value is empty, its type should match m_type instead of being unknown.
    // It's not certain whether this should be part of SVGTransformList or not -- cying
    if (m_baseTransform.type() == SVGTransform::SVG_TRANSFORM_UNKNOWN)
        m_baseTransform = SVGTransform(m_type);
        
    return true;
}

void SVGAnimateTransformElement::applyAnimatedValueToElement()
{
    if (!targetElement()->isStyledTransformable())
        return;
    
    SVGStyledTransformableElement* transform = static_cast<SVGStyledTransformableElement*>(targetElement());
    RefPtr<SVGTransformList> transformList = transform->transform();
    if (!transformList)
        return;
    
    ExceptionCode ec;
    if (!isAdditive())
        transformList->clear(ec);
    
    transformList->appendItem(m_animatedTransform, ec);
    transform->setTransform(transformList.get());
    if (transform->renderer())
        transform->renderer()->setNeedsLayout(true); // should really be in setTransform
}

bool SVGAnimateTransformElement::calculateFromAndToValues(EAnimationMode animationMode, unsigned valueIndex)
{
    switch (animationMode) {
    case FROM_TO_ANIMATION:
        m_fromTransform = parseTransformValue(m_from);
        // fall through
    case TO_ANIMATION:
        m_toTransform = parseTransformValue(m_to);
        break;
    case FROM_BY_ANIMATION:
        m_fromTransform = parseTransformValue(m_from);
        m_toTransform = parseTransformValue(m_by);
        break;
    case BY_ANIMATION:
        m_fromTransform = parseTransformValue(m_from);
        m_toTransform = SVGTransformDistance::addSVGTransforms(m_fromTransform, parseTransformValue(m_by));
        break;
    case VALUES_ANIMATION:
        m_fromTransform = parseTransformValue(m_values[valueIndex]);
        m_toTransform = ((valueIndex + 1) < m_values.size()) ? parseTransformValue(m_values[valueIndex + 1]) : m_fromTransform;
        break;
    case NO_ANIMATION:
        ASSERT_NOT_REACHED();
    }
    
    return true;
}

SVGTransform SVGAnimateTransformElement::parseTransformValue(const String& value) const
{
    SVGTransform result;
    const UChar* ptr = value.characters();
    SVGTransformable::parseTransformValue(m_type, ptr, ptr + value.length(), result); // ignoring return value
    return result;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

