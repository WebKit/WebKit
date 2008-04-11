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
#include "SVGTextElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"

#include <math.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

SVGAnimateTransformElement::SVGAnimateTransformElement(const QualifiedName& tagName, Document* doc)
    : SVGAnimationElement(tagName, doc)
    , m_type(SVGTransform::SVG_TRANSFORM_UNKNOWN)
    , m_baseIndexInTransformList(0)
{
}

SVGAnimateTransformElement::~SVGAnimateTransformElement()
{
}

bool SVGAnimateTransformElement::hasValidTarget() const
{
    SVGElement* targetElement = this->targetElement();
    return SVGAnimationElement::hasValidTarget() && (targetElement->isStyledTransformable() || targetElement->hasTagName(SVGNames::textTag));
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

bool SVGAnimateTransformElement::updateAnimatedValue(float percentage)
{
    m_animatedTransform = SVGTransformDistance(m_fromTransform, m_toTransform).scaledDistance(percentage).addToSVGTransform(m_fromTransform);
    return true;
}
    
static PassRefPtr<SVGTransformList> transformListFor(SVGElement* element)
{
    ASSERT(element);
    if (element->isStyledTransformable())
        return static_cast<SVGStyledTransformableElement*>(element)->transform();
    if (element->hasTagName(SVGNames::textTag))
        return static_cast<SVGTextElement*>(element)->transform();
    return 0;
}

void SVGAnimateTransformElement::applyAnimatedValueToElement(unsigned repeat)
{
    SVGElement* targetElement = this->targetElement();
    RefPtr<SVGTransformList> transformList = transformListFor(targetElement);
    ASSERT(transformList);

    // FIXME: Handle multiple additive tranforms.
    // FIXME: Handle accumulate.
    ExceptionCode ec;
    if (isAdditive()) {
        while (transformList->numberOfItems() > m_baseIndexInTransformList)
            transformList->removeItem(transformList->numberOfItems() - 1, ec);
    } else
        transformList->clear(ec);
    transformList->appendItem(m_animatedTransform, ec);
    
    if (targetElement->renderer())
        targetElement->renderer()->setNeedsLayout(true); // should really be in setTransform
}
    
bool SVGAnimateTransformElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    m_fromTransform = parseTransformValue(fromString);
    if (!m_fromTransform.isValid())
        return false;
    m_toTransform = parseTransformValue(toString);
    return m_toTransform.isValid();
}

bool SVGAnimateTransformElement::calculateFromAndByValues(const String& fromString, const String& byString)
{

    m_fromTransform = parseTransformValue(fromString);
    if (!m_fromTransform.isValid())
        return false;
    m_toTransform = SVGTransformDistance::addSVGTransforms(m_fromTransform, parseTransformValue(byString));
    return m_toTransform.isValid();
}
    
void SVGAnimateTransformElement::startedActiveInterval()
{
    // FIXME: Make multiple additive animations work.
    SVGAnimationElement::startedActiveInterval();
    if (!m_animationValid)
        return;
    
    RefPtr<SVGTransformList> transformList = transformListFor(targetElement());
    ASSERT(transformList);
    m_baseIndexInTransformList = transformList->numberOfItems();
}

SVGTransform SVGAnimateTransformElement::parseTransformValue(const String& value) const
{
    if (value.isEmpty())
        return SVGTransform(m_type);
    SVGTransform result;
    // FIXME: This is pretty dumb but parseTransformValue() wants those parenthesis.
    String parseString("(" + value + ")");
    const UChar* ptr = parseString.characters();
    SVGTransformable::parseTransformValue(m_type, ptr, ptr + parseString.length(), result); // ignoring return value
    return result;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

