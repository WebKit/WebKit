/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SVGLegacyAttributeAnimationController.h"

#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "QualifiedName.h"
#include "SVGAnimateElementBase.h"
#include "SVGAnimationElement.h"
#include "SVGAnimatorFactory.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "StyleProperties.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

SVGLegacyAttributeAnimationController::SVGLegacyAttributeAnimationController(SVGAnimationElement& animationElement, SVGElement& targetElement)
    : SVGAttributeAnimationControllerBase(animationElement, targetElement)
    , m_animatedPropertyType(determineAnimatedPropertyType(animationElement, targetElement, animationElement.attributeName()))
{
}

SVGAnimatedTypeAnimator& SVGLegacyAttributeAnimationController::animatedTypeAnimator()
{
    if (!m_animator)
        m_animator = SVGAnimatorFactory::create(&m_animationElement, &m_targetElement, m_animatedPropertyType);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return *m_animator;
}

bool SVGLegacyAttributeAnimationController::isAdditive() const
{
    // Spec: http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties.
    switch (m_animatedPropertyType) {
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedPreserveAspectRatio:
    case AnimatedString:
    case AnimatedUnknown:
        return false;
    case AnimatedAngle:
    case AnimatedColor:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedLength:
    case AnimatedLengthList:
    case AnimatedNumber:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPath:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedTransformList:
        return true;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return true;
    }
}

bool SVGLegacyAttributeAnimationController::hasValidAttributeType() const
{
    return m_animatedPropertyType != AnimatedUnknown;
}

bool SVGLegacyAttributeAnimationController::isDiscreteAnimator() const
{
    return m_animatedPropertyType == AnimatedBoolean
        || m_animatedPropertyType == AnimatedEnumeration
        || m_animatedPropertyType == AnimatedPreserveAspectRatio
        || m_animatedPropertyType == AnimatedString;
}

bool SVGLegacyAttributeAnimationController::calculateFromAndToValues(const String& fromString, const String& toString)
{
    m_animationElement.determinePropertyValueTypes(fromString, toString);
    animatedTypeAnimator().calculateFromAndToValues(m_fromType, m_toType, fromString, toString);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return true;
}

bool SVGLegacyAttributeAnimationController::calculateFromAndByValues(const String& fromString, const String& byString)
{
    if (m_animationElement.animationMode() == AnimationMode::By && !isAdditive())
        return false;

    // from-by animation may only be used with attributes that support addition (e.g. most numeric attributes).
    if (m_animationElement.animationMode() == AnimationMode::FromBy && !isAdditive())
        return false;

    m_animationElement.determinePropertyValueTypes(fromString, byString);
    animatedTypeAnimator().calculateFromAndByValues(m_fromType, m_toType, fromString, byString);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return true;
}

bool SVGLegacyAttributeAnimationController::calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString)
{
    if (toAtEndOfDurationString.isEmpty())
        return false;
    m_toAtEndOfDurationType = animatedTypeAnimator().constructFromString(toAtEndOfDurationString);
    return true;
}

#ifndef NDEBUG
static inline bool propertyTypesAreConsistent(AnimatedPropertyType expectedPropertyType, const SVGElementAnimatedPropertyList& animatedTypes)
{
    for (auto& type : animatedTypes) {
        for (auto& property : type.properties) {
            if (expectedPropertyType != property->animatedPropertyType()) {
                // This is the only allowed inconsistency. SVGAnimatedAngleAnimator handles both SVGAnimatedAngle & SVGAnimatedEnumeration for markers orient attribute.
                if (expectedPropertyType == AnimatedAngle && property->animatedPropertyType() == AnimatedEnumeration)
                    return true;
                return false;
            }
        }
    }

    return true;
}
#endif

void SVGLegacyAttributeAnimationController::resetAnimatedType()
{
    SVGAnimatedTypeAnimator& animator = animatedTypeAnimator();
    ASSERT(m_animatedPropertyType == animator.type());

    const QualifiedName& attributeName = m_animationElement.attributeName();
    SVGAnimationElement::ShouldApplyAnimation shouldApply = m_animationElement.shouldApplyAnimation(&m_targetElement, attributeName);

    if (shouldApply == SVGAnimationElement::DontApplyAnimation)
        return;

    if (shouldApply == SVGAnimationElement::ApplyXMLAnimation || shouldApply == SVGAnimationElement::ApplyXMLandCSSAnimation) {
        // SVG DOM animVal animation code-path.
        m_animatedProperties = animator.findAnimatedPropertiesForAttributeName(m_targetElement, attributeName);
        if (m_animatedProperties.isEmpty())
            return;

        ASSERT(propertyTypesAreConsistent(m_animatedPropertyType, m_animatedProperties));
        if (!m_animatedType)
            m_animatedType = animator.startAnimValAnimation(m_animatedProperties);
        else {
            animator.resetAnimValToBaseVal(m_animatedProperties, *m_animatedType);
            animator.animValDidChange(m_animatedProperties);
        }
        return;
    }

    // CSS properties animation code-path.
    ASSERT(m_animatedProperties.isEmpty());
    String baseValue;

    if (shouldApply == SVGAnimationElement::ApplyCSSAnimation) {
        ASSERT(SVGAnimationElement::isTargetAttributeCSSProperty(&m_targetElement, attributeName));
        m_animationElement.computeCSSPropertyValue(&m_targetElement, cssPropertyID(attributeName.localName()), baseValue);
    }

    if (!m_animatedType)
        m_animatedType = animator.constructFromString(baseValue);
    else
        m_animatedType->setValueAsString(attributeName, baseValue);
}

void SVGLegacyAttributeAnimationController::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement)
{
    ASSERT(resultElement);

    const QualifiedName& attributeName = m_animationElement.attributeName();
    SVGAnimationElement::ShouldApplyAnimation shouldApply = m_animationElement.shouldApplyAnimation(&m_targetElement, attributeName);
    
    if (shouldApply == SVGAnimationElement::DontApplyAnimation)
        return;

    ASSERT(m_animatedPropertyType == determineAnimatedPropertyType(m_animationElement, m_targetElement, attributeName));

    ASSERT(percentage >= 0 && percentage <= 1);
    ASSERT(m_animatedPropertyType != AnimatedTransformList || m_animationElement.hasTagName(SVGNames::animateTransformTag));
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animator);
    ASSERT(m_animator->type() == m_animatedPropertyType);
    ASSERT(m_fromType);
    ASSERT(m_fromType->type() == m_animatedPropertyType);
    ASSERT(m_toType);

    if (shouldApply == SVGAnimationElement::ApplyXMLAnimation || shouldApply == SVGAnimationElement::ApplyXMLandCSSAnimation) {
        // SVG DOM animVal animation code-path.
        if (m_animator->findAnimatedPropertiesForAttributeName(m_targetElement, attributeName).isEmpty())
            return;
    }

    SVGAnimateElementBase& resultAnimationElement = downcast<SVGAnimateElementBase>(*resultElement);
    SVGLegacyAttributeAnimationController& resultAnimationController = static_cast<SVGLegacyAttributeAnimationController&>(resultAnimationElement.attributeAnimationController());
    
    ASSERT(resultAnimationController.m_animatedType);
    ASSERT(resultAnimationController.m_animatedPropertyType == m_animatedPropertyType);

    if (m_animationElement.hasTagName(SVGNames::setTag))
        percentage = 1;

    if (m_animationElement.calcMode() == CalcMode::Discrete)
        percentage = percentage < 0.5 ? 0 : 1;

    // Target element might have changed.
    m_animator->setContextElement(&m_targetElement);

    // Be sure to detach list wrappers before we modfiy their underlying value. If we'd do
    // if after calculateAnimatedValue() ran the cached pointers in the list propery tear
    // offs would point nowhere, and we couldn't create copies of those values anymore,
    // while detaching. This is covered by assertions, moving this down would fire them.
    if (!m_animatedProperties.isEmpty())
        m_animator->animValWillChange(m_animatedProperties);

    // Values-animation accumulates using the last values entry corresponding to the end of duration time.
    SVGAnimatedType* toAtEndOfDurationType = m_toAtEndOfDurationType ? m_toAtEndOfDurationType.get() : m_toType.get();
    m_animator->calculateAnimatedValue(percentage, repeatCount, m_fromType.get(), m_toType.get(), toAtEndOfDurationType, resultAnimationController.m_animatedType.get());
}

static inline void applyCSSPropertyToTarget(SVGElement& targetElement, CSSPropertyID id, const String& value)
{
    ASSERT(!targetElement.m_deletionHasBegun);

    if (!targetElement.ensureAnimatedSMILStyleProperties().setProperty(id, value, false))
        return;

    targetElement.invalidateStyle();
}

static inline void removeCSSPropertyFromTarget(SVGElement& targetElement, CSSPropertyID id)
{
    ASSERT(!targetElement.m_deletionHasBegun);
    targetElement.ensureAnimatedSMILStyleProperties().removeProperty(id);
    targetElement.invalidateStyle();
}

static inline void applyCSSPropertyToTargetAndInstances(SVGElement& targetElement, const QualifiedName& attributeName, const String& valueAsString)
{
    // FIXME: Do we really need to check both isConnected and !parentNode?
    if (attributeName == anyQName() || !targetElement.isConnected() || !targetElement.parentNode())
        return;

    CSSPropertyID id = cssPropertyID(attributeName.localName());

    SVGElement::InstanceUpdateBlocker blocker(targetElement);
    applyCSSPropertyToTarget(targetElement, id, valueAsString);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    for (auto* instance : targetElement.instances())
        applyCSSPropertyToTarget(*instance, id, valueAsString);
}

static inline void removeCSSPropertyFromTargetAndInstances(SVGElement& targetElement, const QualifiedName& attributeName)
{
    // FIXME: Do we really need to check both isConnected and !parentNode?
    if (attributeName == anyQName() || !targetElement.isConnected() || !targetElement.parentNode())
        return;

    CSSPropertyID id = cssPropertyID(attributeName.localName());

    SVGElement::InstanceUpdateBlocker blocker(targetElement);
    removeCSSPropertyFromTarget(targetElement, id);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    for (auto* instance : targetElement.instances())
        removeCSSPropertyFromTarget(*instance, id);
}

static inline void notifyTargetAboutAnimValChange(SVGElement& targetElement, const QualifiedName& attributeName)
{
    ASSERT(!targetElement.m_deletionHasBegun);
    targetElement.svgAttributeChanged(attributeName);
}

static inline void notifyTargetAndInstancesAboutAnimValChange(SVGElement& targetElement, const QualifiedName& attributeName)
{
    if (attributeName == anyQName() || !targetElement.isConnected() || !targetElement.parentNode())
        return;

    SVGElement::InstanceUpdateBlocker blocker(targetElement);
    notifyTargetAboutAnimValChange(targetElement, attributeName);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    for (auto* instance : targetElement.instances())
        notifyTargetAboutAnimValChange(*instance, attributeName);
}

void SVGLegacyAttributeAnimationController::applyResultsToTarget()
{
    ASSERT(m_animatedPropertyType != AnimatedTransformList || m_animationElement.hasTagName(SVGNames::animateTransformTag));
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animator);

    // Early exit if our animated type got destroyed by a previous endedActiveInterval().
    if (!m_animatedType)
        return;

    const QualifiedName& attributeName = m_animationElement.attributeName();

    if (m_animatedProperties.isEmpty()) {
        // CSS properties animation code-path.
        // Convert the result of the animation to a String and apply it as CSS property on the target & all instances.
        applyCSSPropertyToTargetAndInstances(m_targetElement, attributeName, m_animatedType->valueAsString());
        return;
    }

    // We do update the style and the animation property independent of each other.
    SVGAnimationElement::ShouldApplyAnimation shouldApply = m_animationElement.shouldApplyAnimation(&m_targetElement, attributeName);
    if (shouldApply == SVGAnimationElement::ApplyXMLandCSSAnimation)
        applyCSSPropertyToTargetAndInstances(m_targetElement, attributeName, m_animatedType->valueAsString());

    // SVG DOM animVal animation code-path.
    // At this point the SVG DOM values are already changed, unlike for CSS.
    // We only have to trigger update notifications here.
    m_animator->animValDidChange(m_animatedProperties);
    notifyTargetAndInstancesAboutAnimValChange(m_targetElement, attributeName);
}

void SVGLegacyAttributeAnimationController::clearAnimatedType(SVGElement* targetElement)
{
    if (!m_animatedType)
        return;

    // If the SVGAnimatedType is a list type, e.g. SVGLengthListValues, the wrappers of the
    // animated properties have to be detached from the items in the list before it's deleted.
    if (!m_animatedProperties.isEmpty())
        m_animator->animValWillChange(m_animatedProperties);

    if (!targetElement) {
        m_animatedType = nullptr;
        return;
    }

    const QualifiedName& attributeName = m_animationElement.attributeName();

    if (m_animatedProperties.isEmpty()) {
        // CSS properties animation code-path.
        removeCSSPropertyFromTargetAndInstances(*targetElement, m_animationElement.attributeName());
        m_animatedType = nullptr;
        return;
    }

    SVGAnimationElement::ShouldApplyAnimation shouldApply = m_animationElement.shouldApplyAnimation(targetElement, attributeName);
    if (shouldApply == SVGAnimationElement::ApplyXMLandCSSAnimation)
        removeCSSPropertyFromTargetAndInstances(*targetElement, attributeName);

    // SVG DOM animVal animation code-path.
    if (m_animator) {
        m_animator->stopAnimValAnimation(m_animatedProperties);
        notifyTargetAndInstancesAboutAnimValChange(*targetElement, attributeName);
    }

    m_animatedProperties.clear();
    m_animatedType = nullptr;
}

float SVGLegacyAttributeAnimationController::calculateDistance(const String& fromString, const String& toString)
{
    return animatedTypeAnimator().calculateDistance(fromString, toString);
}

}

