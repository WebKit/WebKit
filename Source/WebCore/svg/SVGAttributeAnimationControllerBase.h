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

#include "QualifiedName.h"
#include "SVGAnimatedPropertyType.h"

namespace WebCore {

class SVGAnimationElement;
class SVGElement;
class SVGSMILElement;

class SVGAttributeAnimationControllerBase {
public:
    SVGAttributeAnimationControllerBase(SVGAnimationElement&, SVGElement&);
    virtual ~SVGAttributeAnimationControllerBase() = default;

    static AnimatedPropertyType determineAnimatedPropertyType(const SVGAnimationElement&, SVGElement&, const QualifiedName&);

    virtual bool isAdditive() const = 0;
    virtual bool hasValidAttributeType() const = 0;
    
    virtual void resetAnimatedType() = 0;
    virtual void clearAnimatedType(SVGElement* targetElement) = 0;
    
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) = 0;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) = 0;
    virtual bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) = 0;

    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement) = 0;
    virtual void applyResultsToTarget() = 0;
    
    virtual float calculateDistance(const String& fromString, const String& toString) = 0;
    
protected:
    SVGAnimationElement& m_animationElement;
    SVGElement& m_targetElement;
};

} // namespace WebCore

