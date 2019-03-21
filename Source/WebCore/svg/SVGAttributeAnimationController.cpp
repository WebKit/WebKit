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
#include "SVGAttributeAnimationController.h"

#include "QualifiedName.h"
#include "SVGAnimationElement.h"
#include "SVGAttributeAnimator.h"
#include "SVGElement.h"

namespace WebCore {

SVGAttributeAnimationController::SVGAttributeAnimationController(SVGAnimationElement& animationElement, SVGElement& targetElement)
    : SVGAttributeAnimationControllerBase(animationElement, targetElement)
{
}
    
SVGAttributeAnimationController::~SVGAttributeAnimationController()
{
    if (m_animator)
        m_targetElement.animatorWillBeDeleted(m_animationElement.attributeName());
}

SVGAttributeAnimator* SVGAttributeAnimationController::animator() const
{
    if (!m_animator)
        m_animator = m_targetElement.createAnimator(m_animationElement.attributeName(), m_animationElement.animationMode(), m_animationElement.calcMode(), m_animationElement.isAccumulated(), m_animationElement.isAdditive());

    return m_animator.get();
}

bool SVGAttributeAnimationController::isDiscreteAnimator() const
{
    return hasValidAttributeType() && animatorIfExists() && animatorIfExists()->isDiscrete();
}

bool SVGAttributeAnimationController::isAdditive() const
{
    return !isDiscreteAnimator();
}
    
bool SVGAttributeAnimationController::hasValidAttributeType() const
{
    return m_targetElement.isAnimatedAttribute(m_animationElement.attributeName());
}

bool SVGAttributeAnimationController::calculateFromAndToValues(const String& from, const String& to)
{
    if (auto* animator = this->animator()) {
        animator->setFromAndToValues(&m_targetElement, from, to);
        return true;
    }
    return false;
}

bool SVGAttributeAnimationController::calculateFromAndByValues(const String& from, const String& by)
{
    if (m_animationElement.animationMode() == AnimationMode::By && (!isAdditive() || isDiscreteAnimator()))
        return false;
    
    if (m_animationElement.animationMode() == AnimationMode::FromBy && isDiscreteAnimator())
        return false;

    if (auto* animator = this->animator()) {
        animator->setFromAndByValues(&m_targetElement, from, by);
        return true;
    }
    return false;
}

bool SVGAttributeAnimationController::calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString)
{
    if (toAtEndOfDurationString.isEmpty())
        return false;

    if (isDiscreteAnimator())
        return true;

    if (auto* animator = this->animator()) {
        animator->setToAtEndOfDurationValue(toAtEndOfDurationString);
        return true;
    }
    return false;
}

void SVGAttributeAnimationController::resetAnimatedType()
{
    if (auto* animator = this->animator())
        animator->start(&m_targetElement);
}

void SVGAttributeAnimationController::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement*)
{
    ASSERT(percentage >= 0 && percentage <= 1);
    if (m_animationElement.hasTagName(SVGNames::setTag))
        percentage = 1;

    if (m_animationElement.calcMode() == CalcMode::Discrete)
        percentage = percentage < 0.5 ? 0 : 1;

    if (auto* animator = this->animator())
        animator->progress(&m_targetElement, percentage, repeatCount);
}

void SVGAttributeAnimationController::applyResultsToTarget()
{
    if (auto* animator = this->animator())
        animator->apply(&m_targetElement);
}

void SVGAttributeAnimationController::clearAnimatedType(SVGElement* targetElement)
{
    if (auto* animator = this->animatorIfExists())
        animator->stop(targetElement);
}

float SVGAttributeAnimationController::calculateDistance(const String& from, const String& to)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    if (auto* animator = this->animator())
        return animator->calculateDistance(&m_targetElement, from, to);
    return -1;
}

}
