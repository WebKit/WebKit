/*
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
              (C) 2007 Rob Buis <buis@kde.org>

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
#include "SVGAnimateMotionElement.h"

#include "RenderObject.h"
#include "SVGMPathElement.h"
#include "SVGParserUtilities.h"
#include "SVGPathElement.h"
#include "SVGTransformList.h"

namespace WebCore {
    
using namespace SVGNames;

SVGAnimateMotionElement::SVGAnimateMotionElement(const QualifiedName& tagName, Document* doc)
    : SVGAnimationElement(tagName, doc)
    , m_rotateMode(AngleMode)
    , m_angle(0)
{
    m_calcMode = CALCMODE_PACED;
}

SVGAnimateMotionElement::~SVGAnimateMotionElement()
{
}

bool SVGAnimateMotionElement::hasValidTarget() const
{
    if (!SVGAnimationElement::hasValidTarget())
        return false;
    if (!targetElement()->isStyledTransformable())
        return false;
    // Spec: SVG 1.1 section 19.2.15
    if (targetElement()->hasTagName(gTag)
        || targetElement()->hasTagName(defsTag)
        || targetElement()->hasTagName(useTag)
        || targetElement()->hasTagName(imageTag)
        || targetElement()->hasTagName(switchTag)
        || targetElement()->hasTagName(pathTag)
        || targetElement()->hasTagName(rectTag)
        || targetElement()->hasTagName(circleTag)
        || targetElement()->hasTagName(ellipseTag)
        || targetElement()->hasTagName(lineTag)
        || targetElement()->hasTagName(polylineTag)
        || targetElement()->hasTagName(polygonTag)
        || targetElement()->hasTagName(textTag)
        || targetElement()->hasTagName(clipPathTag)
        || targetElement()->hasTagName(maskTag)
        || targetElement()->hasTagName(aTag)
        || targetElement()->hasTagName(foreignObjectTag))
        return true;
    return false;
}

void SVGAnimateMotionElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::rotateAttr) {
        if (attr->value() == "auto")
            m_rotateMode = AutoMode;
        else if (attr->value() == "auto-reverse")
            m_rotateMode = AutoReverseMode;
        else {
            m_rotateMode = AngleMode;
            m_angle = attr->value().toFloat();
        }
    } else if (attr->name() == SVGNames::keyPointsAttr) {
        m_keyPoints.clear();
        parseKeyNumbers(m_keyPoints, attr->value());
    } else if (attr->name() == SVGNames::dAttr) {
        m_path = Path();
        pathFromSVGData(m_path, attr->value());
    } else
        SVGAnimationElement::parseMappedAttribute(attr);
}

Path SVGAnimateMotionElement::animationPath()
{
    for (Node* child = firstChild(); child; child->nextSibling()) {
        if (child->hasTagName(SVGNames::mpathTag)) {
            SVGMPathElement* mPath = static_cast<SVGMPathElement*>(child);
            SVGPathElement* pathElement = mPath->pathElement();
            if (pathElement)
                return pathElement->toPathData();
            // The spec would probably have us throw up an error here, but instead we try to fall back to the d value
        }
    }
    if (hasAttribute(SVGNames::dAttr))
        return m_path;
    return Path();
}

bool SVGAnimateMotionElement::updateAnimatedValue(EAnimationMode animationMode, float timePercentage, unsigned valueIndex, float percentagePast)
{
    if (animationMode == TO_ANIMATION) {
        // to-animations have a special equation: value = (to - base) * (time/duration) + base
        m_animatedTranslation.setWidth((m_toPoint.x() - m_basePoint.x()) * timePercentage + m_basePoint.x());
        m_animatedTranslation.setHeight((m_toPoint.y() - m_basePoint.y()) * timePercentage + m_basePoint.y());
        m_animatedAngle = 0.0f;
    } else {
        m_animatedTranslation.setWidth(m_pointDiff.width() * percentagePast + m_fromPoint.x());
        m_animatedTranslation.setHeight(m_pointDiff.height() * percentagePast + m_fromPoint.y());
        m_animatedAngle = m_angleDiff * percentagePast + m_fromAngle;
    }
    return true;
}

static bool parsePoint(const String& s, FloatPoint& point)
{
    if (s.isEmpty())
        return false;
    const UChar* cur = s.characters();
    const UChar* end = cur + s.length();
    
    if (!skipOptionalSpaces(cur, end))
        return false;
    
    float x = 0.0f;
    if (!parseNumber(cur, end, x))
        return false;
    
    float y = 0.0f;
    if (!parseNumber(cur, end, y))
        return false;
    
    point = FloatPoint(x, y);
    
    // disallow anying except spaces at the end
    return !skipOptionalSpaces(cur, end);
}

bool SVGAnimateMotionElement::calculateFromAndToValues(EAnimationMode animationMode, unsigned valueIndex)
{
    m_fromAngle = 0.0f;
    m_toAngle = 0.0f;
    switch (animationMode) {
    case FROM_TO_ANIMATION:
        parsePoint(m_from, m_fromPoint);
        // fall through
    case TO_ANIMATION:
        parsePoint(m_to, m_toPoint);
        break;
    case FROM_BY_ANIMATION:
        parsePoint(m_from, m_fromPoint);
        parsePoint(m_to, m_toPoint);
        break;
    case BY_ANIMATION:
    {
        parsePoint(m_from, m_fromPoint);
        FloatPoint byPoint;
        parsePoint(m_by, byPoint);
        m_toPoint = FloatPoint(m_fromPoint.x() + byPoint.x(), m_fromPoint.y() + byPoint.y());
        break;
    }
    case VALUES_ANIMATION:
        parsePoint(m_values[valueIndex], m_fromPoint);
        if ((valueIndex + 1) < m_values.size())
            parsePoint(m_values[valueIndex + 1], m_toPoint);
        else
            m_toPoint = m_fromPoint;
        break;
    case NO_ANIMATION:
        ASSERT_NOT_REACHED();
    }
    
    m_pointDiff = m_toPoint - m_fromPoint;
    m_angleDiff = 0.0f;
    return (m_pointDiff.width() != 0 || m_pointDiff.height() != 0);
}

bool SVGAnimateMotionElement::updateAnimationBaseValueFromElement()
{
    if (!targetElement()->isStyledTransformable())
        return false;
    
    m_basePoint = static_cast<SVGStyledTransformableElement*>(targetElement())->getBBox().location();
    return true;
}

void SVGAnimateMotionElement::applyAnimatedValueToElement()
{
    if (!targetElement()->isStyledTransformable())
        return;
    
    SVGStyledTransformableElement* transformableElement = static_cast<SVGStyledTransformableElement*>(targetElement());
    RefPtr<SVGTransformList> transformList = transformableElement->transform();
    if (!transformList)
        return;
    
    ExceptionCode ec;
    if (!isAdditive())
        transformList->clear(ec);
    
    AffineTransform transform;
    transform.rotate(m_animatedAngle);
    transform.translate(m_animatedTranslation.width(), m_animatedTranslation.height());
    if (!transform.isIdentity()) {
        transformList->appendItem(SVGTransform(transform), ec);
        transformableElement->setTransform(transformList.get());
        if (transformableElement->renderer())
            transformableElement->renderer()->setNeedsLayout(true); // should be part of setTransform
    }
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
