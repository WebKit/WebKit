/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "SVGAnimateMotionElement.h"

#include "AffineTransform.h"
#include "ElementIterator.h"
#include "PathTraversalState.h"
#include "RenderSVGResource.h"
#include "SVGImageElement.h"
#include "SVGMPathElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGPathData.h"
#include "SVGPathElement.h"
#include "SVGPathUtilities.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAnimateMotionElement);
    
using namespace SVGNames;

inline SVGAnimateMotionElement::SVGAnimateMotionElement(const QualifiedName& tagName, Document& document)
    : SVGAnimationElement(tagName, document)
    , m_hasToPointAtEndOfDuration(false)
{
    setCalcMode(CalcMode::Paced);
    ASSERT(hasTagName(animateMotionTag));
}

Ref<SVGAnimateMotionElement> SVGAnimateMotionElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGAnimateMotionElement(tagName, document));
}

bool SVGAnimateMotionElement::hasValidAttributeType() const
{
    auto targetElement = makeRefPtr(this->targetElement());
    if (!targetElement)
        return false;

    // We don't have a special attribute name to verify the animation type. Check the element name instead.
    if (!targetElement->isSVGGraphicsElement())
        return false;
    // Spec: SVG 1.1 section 19.2.15
    // FIXME: svgTag is missing. Needs to be checked, if transforming <svg> could cause problems.
    if (targetElement->hasTagName(gTag)
        || targetElement->hasTagName(defsTag)
        || targetElement->hasTagName(useTag)
        || is<SVGImageElement>(*targetElement)
        || targetElement->hasTagName(switchTag)
        || targetElement->hasTagName(pathTag)
        || targetElement->hasTagName(rectTag)
        || targetElement->hasTagName(circleTag)
        || targetElement->hasTagName(ellipseTag)
        || targetElement->hasTagName(lineTag)
        || targetElement->hasTagName(polylineTag)
        || targetElement->hasTagName(polygonTag)
        || targetElement->hasTagName(textTag)
        || targetElement->hasTagName(clipPathTag)
        || targetElement->hasTagName(maskTag)
        || targetElement->hasTagName(SVGNames::aTag)
        || targetElement->hasTagName(foreignObjectTag)
        )
        return true;
    return false;
}

bool SVGAnimateMotionElement::hasValidAttributeName() const
{
    // AnimateMotion does not use attributeName so it is always valid.
    return true;
}

void SVGAnimateMotionElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::pathAttr) {
        m_path = buildPathFromString(value);
        updateAnimationPath();
        return;
    }

    SVGAnimationElement::parseAttribute(name, value);
}
    
SVGAnimateMotionElement::RotateMode SVGAnimateMotionElement::rotateMode() const
{
    static NeverDestroyed<const AtomString> autoVal("auto", AtomString::ConstructFromLiteral);
    static NeverDestroyed<const AtomString> autoReverse("auto-reverse", AtomString::ConstructFromLiteral);
    const AtomString& rotate = getAttribute(SVGNames::rotateAttr);
    if (rotate == autoVal)
        return RotateAuto;
    if (rotate == autoReverse)
        return RotateAutoReverse;
    return RotateAngle;
}

void SVGAnimateMotionElement::updateAnimationPath()
{
    m_animationPath = Path();
    bool foundMPath = false;

    for (auto& mPath : childrenOfType<SVGMPathElement>(*this)) {
        auto pathElement = mPath.pathElement();
        if (pathElement) {
            m_animationPath = pathFromGraphicsElement(pathElement.get());
            foundMPath = true;
            break;
        }
    }

    if (!foundMPath && hasAttributeWithoutSynchronization(SVGNames::pathAttr))
        m_animationPath = m_path;

    updateAnimationMode();
}

void SVGAnimateMotionElement::startAnimation()
{
    if (!hasValidAttributeType())
        return;
    auto targetElement = makeRefPtr(this->targetElement());
    if (!targetElement)
        return;
    if (AffineTransform* transform = targetElement->supplementalTransform())
        transform->makeIdentity();
}

void SVGAnimateMotionElement::stopAnimation(SVGElement* targetElement)
{
    if (!targetElement)
        return;
    if (AffineTransform* transform = targetElement->supplementalTransform())
        transform->makeIdentity();
    applyResultsToTarget();
}

bool SVGAnimateMotionElement::calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString)
{
    parsePoint(toAtEndOfDurationString, m_toPointAtEndOfDuration);
    m_hasToPointAtEndOfDuration = true;
    return true;
}

bool SVGAnimateMotionElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    m_hasToPointAtEndOfDuration = false;
    parsePoint(fromString, m_fromPoint);
    parsePoint(toString, m_toPoint);
    return true;
}
    
bool SVGAnimateMotionElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    m_hasToPointAtEndOfDuration = false;
    if (animationMode() == AnimationMode::By && !isAdditive())
        return false;
    parsePoint(fromString, m_fromPoint);
    FloatPoint byPoint;
    parsePoint(byString, byPoint);
    m_toPoint = FloatPoint(m_fromPoint.x() + byPoint.x(), m_fromPoint.y() + byPoint.y());
    return true;
}

void SVGAnimateMotionElement::buildTransformForProgress(AffineTransform* transform, float percentage)
{
    ASSERT(!m_animationPath.isEmpty());

    float positionOnPath = m_animationPath.length() * percentage;
    auto traversalState(m_animationPath.traversalStateAtLength(positionOnPath));
    if (!traversalState.success())
        return;

    FloatPoint position = traversalState.current();
    float angle = traversalState.normalAngle();

    transform->translate(position);
    RotateMode rotateMode = this->rotateMode();
    if (rotateMode != RotateAuto && rotateMode != RotateAutoReverse)
        return;
    if (rotateMode == RotateAutoReverse)
        angle += 180;
    transform->rotate(angle);
}

void SVGAnimateMotionElement::calculateAnimatedValue(float percentage, unsigned repeatCount)
{
    auto targetElement = makeRefPtr(this->targetElement());
    if (!targetElement)
        return;

    AffineTransform* transform = targetElement->supplementalTransform();
    if (!transform)
        return;

    if (!isAdditive())
        transform->makeIdentity();

    if (animationMode() != AnimationMode::Path) {
        FloatPoint toPointAtEndOfDuration = m_toPoint;
        if (isAccumulated() && repeatCount && m_hasToPointAtEndOfDuration)
            toPointAtEndOfDuration = m_toPointAtEndOfDuration;

        float animatedX = 0;
        animateAdditiveNumber(percentage, repeatCount, m_fromPoint.x(), m_toPoint.x(), toPointAtEndOfDuration.x(), animatedX);

        float animatedY = 0;
        animateAdditiveNumber(percentage, repeatCount, m_fromPoint.y(), m_toPoint.y(), toPointAtEndOfDuration.y(), animatedY);

        transform->translate(animatedX, animatedY);
        return;
    }

    buildTransformForProgress(transform, percentage);

    // Handle accumulate="sum".
    if (isAccumulated() && repeatCount) {
        for (unsigned i = 0; i < repeatCount; ++i)
            buildTransformForProgress(transform, 1);
    }
}

void SVGAnimateMotionElement::applyResultsToTarget()
{
    // We accumulate to the target element transform list so there is not much to do here.
    auto targetElement = makeRefPtr(this->targetElement());
    if (!targetElement)
        return;

    if (RenderElement* renderer = targetElement->renderer()) {
        renderer->setNeedsTransformUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
    }

    AffineTransform* targetSupplementalTransform = targetElement->supplementalTransform();
    if (!targetSupplementalTransform)
        return;

    // ...except in case where we have additional instances in <use> trees.
    for (auto* instance : targetElement->instances()) {
        AffineTransform* transform = instance->supplementalTransform();
        if (!transform || *transform == *targetSupplementalTransform)
            continue;
        *transform = *targetSupplementalTransform;
        if (RenderElement* renderer = instance->renderer()) {
            renderer->setNeedsTransformUpdate();
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
    }
}

Optional<float> SVGAnimateMotionElement::calculateDistance(const String& fromString, const String& toString)
{
    FloatPoint from;
    FloatPoint to;
    if (!parsePoint(fromString, from))
        return { };
    if (!parsePoint(toString, to))
        return { };
    FloatSize diff = to - from;
    return std::hypot(diff.width(), diff.height());
}

void SVGAnimateMotionElement::updateAnimationMode()
{
    if (!m_animationPath.isEmpty())
        setAnimationMode(AnimationMode::Path);
    else
        SVGAnimationElement::updateAnimationMode();
}

}
