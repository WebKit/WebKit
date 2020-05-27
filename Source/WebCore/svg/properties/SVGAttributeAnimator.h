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

#include "CSSPropertyNames.h"
#include "QualifiedName.h"
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class SVGElement;

enum class AnimationMode : uint8_t {
    None,
    FromTo,
    FromBy,
    To,
    By,
    Values,
    Path
};

enum class CalcMode : uint8_t {
    Discrete,
    Linear,
    Paced,
    Spline
};

class SVGAttributeAnimator : public RefCounted<SVGAttributeAnimator>, public CanMakeWeakPtr<SVGAttributeAnimator> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGAttributeAnimator(const QualifiedName& attributeName)
        : m_attributeName(attributeName)
    {
    }

    virtual ~SVGAttributeAnimator() = default;

    virtual bool isDiscrete() const { return false; }

    virtual void setFromAndToValues(SVGElement*, const String&, const String&) { }
    virtual void setFromAndByValues(SVGElement*, const String&, const String&) { }
    virtual void setToAtEndOfDurationValue(const String&) { }

    virtual void start(SVGElement*) = 0;
    virtual void animate(SVGElement*, float progress, unsigned repeatCount) = 0;
    virtual void apply(SVGElement*) = 0;
    virtual void stop(SVGElement* targetElement) = 0;

    virtual Optional<float> calculateDistance(SVGElement*, const String&, const String&) const { return { }; }

protected:
    bool isAnimatedStylePropertyAniamtor(const SVGElement*) const;

    static void invalidateStyle(SVGElement*);
    static void applyAnimatedStylePropertyChange(SVGElement*, CSSPropertyID, const String& value);
    static void removeAnimatedStyleProperty(SVGElement*, CSSPropertyID);
    static void applyAnimatedPropertyChange(SVGElement*, const QualifiedName&);

    void applyAnimatedStylePropertyChange(SVGElement*, const String& value);
    void removeAnimatedStyleProperty(SVGElement*);
    void applyAnimatedPropertyChange(SVGElement*);

    const QualifiedName& m_attributeName;
};

}
