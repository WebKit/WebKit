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

#pragma once

#include "SVGAttributeAnimationControllerBase.h"

namespace WebCore {

class SVGAnimationElement;
class SVGAttributeAnimator;
class SVGElement;
class SVGSMILElement;

class SVGAttributeAnimationController : public SVGAttributeAnimationControllerBase {
public:
    SVGAttributeAnimationController(SVGAnimationElement&, SVGElement&);
    ~SVGAttributeAnimationController();
    
private:
    SVGAttributeAnimator* animator() const;
    SVGAttributeAnimator* animatorIfExists() const { return m_animator.get(); }

    void resetAnimatedType() override;
    void clearAnimatedType(SVGElement* targetElement) override;
    
    bool calculateFromAndToValues(const String& fromString, const String& toString) override;
    bool calculateFromAndByValues(const String& fromString, const String& byString) override;
    bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) override;

    void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement) override;

    void applyResultsToTarget() override;
    float calculateDistance(const String& fromString, const String& toString) override;
    
    bool isAdditive() const override;
    bool hasValidAttributeType() const override;
    bool isDiscreteAnimator() const override;

    mutable std::unique_ptr<SVGAttributeAnimator> m_animator;
};

} // namespace WebCore
