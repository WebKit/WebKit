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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGAnimateTransformElement.h"

#include "TimeScheduler.h"
#include "SVGAngle.h"
#include "AffineTransform.h"
#include "SVGSVGElement.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
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
        const String& value = attr->value();
        if (value == "translate")
            m_type = SVGTransform::SVG_TRANSFORM_TRANSLATE;
        else if (value == "scale")
            m_type = SVGTransform::SVG_TRANSFORM_SCALE;
        else if (value == "rotate")
            m_type = SVGTransform::SVG_TRANSFORM_ROTATE;
        else if (value == "skewX")
            m_type = SVGTransform::SVG_TRANSFORM_SKEWX;
        else if (value == "skewY")
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
        m_animatedTransform = m_transformDistance.scaledDistance(percentagePast).addToSVGTransform(m_fromTransform);
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
    transform->updateLocalTransform(transformList.get());
}

bool SVGAnimateTransformElement::calculateFromAndToValues(EAnimationMode animationMode, unsigned valueIndex)
{
    switch (detectAnimationMode()) {
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
    m_transformDistance = SVGTransformDistance(m_fromTransform, m_toTransform);
    return !m_transformDistance.isZero();
}

SVGTransform SVGAnimateTransformElement::parseTransformValue(const String& data) const
{
    // FIXME: This parser should be combined with the one in SVGTransformable::parseTransformAttribute
    
    String parse = data.stripWhiteSpace();
    if (parse.isEmpty())
        return SVGTransform();
    
    int commaPos = parse.find(','); // In case two values are passed...

    SVGTransform parsedTransform;
    
    switch (m_type) {
        case SVGTransform::SVG_TRANSFORM_TRANSLATE:
        {
            double tx = 0.0, ty = 0.0;
            if (commaPos != - 1) {
                tx = parse.substring(0, commaPos).toDouble();
                ty = parse.substring(commaPos + 1).toDouble();
            } else
                tx = parse.toDouble();

            parsedTransform.setTranslate(tx, ty);
            break;
        }
        case SVGTransform::SVG_TRANSFORM_SCALE:
        {
            double sx = 1.0, sy = 1.0;
            if (commaPos != - 1) {
                sx = parse.substring(0, commaPos).toDouble();
                sy = parse.substring(commaPos + 1).toDouble();
            } else {
                sx = parse.toDouble();
                sy = sx;
            }

            parsedTransform.setScale(sx, sy);
            break;
        }
        case SVGTransform::SVG_TRANSFORM_ROTATE:
        {
            double angle = 0, cx = 0, cy = 0;
            if (commaPos != - 1) {
                angle = parse.substring(0, commaPos).toDouble(); // TODO: probably needs it's own 'angle' parser
    
                int commaPosTwo = parse.find(',', commaPos + 1); // In case three values are passed...
                if (commaPosTwo != -1) {
                    cx = parse.substring(commaPos + 1, commaPosTwo - commaPos - 1).toDouble();
                    cy = parse.substring(commaPosTwo + 1).toDouble();
                }
            }
            else 
                angle = parse.toDouble();

            parsedTransform.setRotate(angle, cx, cy);
            break;    
        }
        case SVGTransform::SVG_TRANSFORM_SKEWX:
        case SVGTransform::SVG_TRANSFORM_SKEWY:
        {
            double angle = parse.toDouble(); // TODO: probably needs it's own 'angle' parser
            
            if (m_type == SVGTransform::SVG_TRANSFORM_SKEWX)
                parsedTransform.setSkewX(angle);
            else
                parsedTransform.setSkewY(angle);

            break;
        }
        default:
            return SVGTransform();
    }
    
    return parsedTransform;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

