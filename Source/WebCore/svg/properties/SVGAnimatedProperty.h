/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "SVGAnimatedPropertyBase.h"
#include "SVGAttributeAnimator.h"

namespace WebCore {

class SVGElement;

// CRTP (Curiously Recurring Template Pattern) template layer providing type-safe instance animation methods.
// The ConcreteAnimatedPropertyType parameter should be the concrete animated property type (e.g., SVGAnimatedValueProperty<PropertyType>).
// This eliminates the need for unsafe static_cast operations in instance animation methods.
template<typename ConcreteAnimatedPropertyType>
class SVGAnimatedProperty : public SVGAnimatedPropertyBase {
public:
    // Type-safe instance animation methods that dispatch to derived class implementations.
    // These methods take ConcreteAnimatedPropertyType& instead of SVGAnimatedPropertyBase& to ensure type safety.
    void instanceStartAnimation(SVGAttributeAnimator& animator, ConcreteAnimatedPropertyType& animated)
    {
        // If this is hot on some benchmarks, we could easily devirtualize by casting
        // `this` to `ConcreteAnimatedPropertyType` and calling `instanceStartAnimationImpl`
        // directly on the subclass, in which case `instanceStartAnimationImpl` would not need
        // to be virtual anymore.
        instanceStartAnimationImpl(animator, animated);
    }

    void instanceStopAnimation(SVGAttributeAnimator& animator)
    {
        // If this is hot on some benchmarks, we could easily devirtualize by casting
        // `this` to `ConcreteAnimatedPropertyType` and calling `instanceStopAnimationImpl`
        // directly on the subclass, in which case `instanceStopAnimationImpl` would not need
        // to be virtual anymore.
        instanceStopAnimationImpl(animator);
    }

protected:
    explicit SVGAnimatedProperty(SVGElement* contextElement)
        : SVGAnimatedPropertyBase(contextElement)
    {
    }

    virtual void instanceStartAnimationImpl(SVGAttributeAnimator&, ConcreteAnimatedPropertyType&) = 0;
    virtual void instanceStopAnimationImpl(SVGAttributeAnimator&) = 0;
};

} // namespace WebCore

