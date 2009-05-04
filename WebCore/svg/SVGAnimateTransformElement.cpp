/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2008 Apple Inc. All Rights Reserved.

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

#include "MappedAttribute.h"
#include "RenderObject.h"
#include "SVGAngle.h"
#include "SVGElementInstance.h"
#include "SVGParserUtilities.h"
#include "SVGSVGElement.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTextElement.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUseElement.h"
#include "TransformationMatrix.h"
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

    
static PassRefPtr<SVGTransformList> transformListFor(SVGElement* element)
{
    ASSERT(element);
    if (element->isStyledTransformable())
        return static_cast<SVGStyledTransformableElement*>(element)->transform();
    if (element->hasTagName(SVGNames::textTag))
        return static_cast<SVGTextElement*>(element)->transform();
    return 0;
}
    
void SVGAnimateTransformElement::resetToBaseValue(const String& baseValue)
{
    if (!hasValidTarget())
        return;
    if (baseValue.isEmpty()) {
        ExceptionCode ec;
        RefPtr<SVGTransformList> list = transformListFor(targetElement());
        list->clear(ec);
    } else
        targetElement()->setAttribute(SVGNames::transformAttr, baseValue);
}

void SVGAnimateTransformElement::calculateAnimatedValue(float percentage, unsigned repeat, SVGSMILElement* resultElement)
{
    if (!hasValidTarget())
        return;
    SVGElement* targetElement = resultElement->targetElement();
    RefPtr<SVGTransformList> transformList = transformListFor(targetElement);
    ASSERT(transformList);

    ExceptionCode ec;
    if (!isAdditive())
        transformList->clear(ec);
    if (isAccumulated() && repeat) {
        SVGTransform accumulatedTransform = SVGTransformDistance(m_fromTransform, m_toTransform).scaledDistance(repeat).addToSVGTransform(SVGTransform());
        transformList->appendItem(accumulatedTransform, ec);
    }
    SVGTransform transform = SVGTransformDistance(m_fromTransform, m_toTransform).scaledDistance(percentage).addToSVGTransform(m_fromTransform);
    transformList->appendItem(transform, ec);
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
    
void SVGAnimateTransformElement::applyResultsToTarget()
{
    if (!hasValidTarget())
        return;
    // We accumulate to the target element transform list so there is not much to do here.
    SVGElement* targetElement = this->targetElement();
    if (targetElement->renderer())
        targetElement->renderer()->setNeedsLayout(true);
    
    // ...except in case where we have additional instances in <use> trees.
    HashSet<SVGElementInstance*> instances = targetElement->instancesForElement();
    RefPtr<SVGTransformList> transformList = transformListFor(targetElement);
    HashSet<SVGElementInstance*>::iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::iterator it = instances.begin(); it != end; ++it) {
        SVGElement* shadowTreeElement = (*it)->shadowTreeElement();
        ASSERT(shadowTreeElement);
        if (shadowTreeElement->isStyledTransformable())
            static_cast<SVGStyledTransformableElement*>(shadowTreeElement)->setTransform(transformList.get());
        else if (shadowTreeElement->hasTagName(SVGNames::textTag))
            static_cast<SVGTextElement*>(shadowTreeElement)->setTransform(transformList.get());
        if (shadowTreeElement->renderer())
            shadowTreeElement->renderer()->setNeedsLayout(true);
    }
}
    
float SVGAnimateTransformElement::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: This is not correct in all cases. The spec demands that each component (translate x and y for example) 
    // is paced separately. To implement this we need to treat each component as individual animation everywhere.
    SVGTransform from = parseTransformValue(fromString);
    if (!from.isValid())
        return -1.f;
    SVGTransform to = parseTransformValue(toString);
    if (!to.isValid() || from.type() != to.type())
        return -1.f;
    if (to.type() == SVGTransform::SVG_TRANSFORM_TRANSLATE) {
        FloatSize diff = to.translate() - from.translate();
        return sqrtf(diff.width() * diff.width() + diff.height() * diff.height());
    }
    if (to.type() == SVGTransform::SVG_TRANSFORM_ROTATE)
        return fabsf(to.angle() - from.angle());
    if (to.type() == SVGTransform::SVG_TRANSFORM_SCALE) {
        FloatSize diff = to.scale() - from.scale();
        return sqrtf(diff.width() * diff.width() + diff.height() * diff.height());
    }
    return -1.f;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

