/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef PlatformCAAnimationMac_h
#define PlatformCAAnimationMac_h

#include "PlatformCAAnimation.h"

#include <wtf/RetainPtr.h>

OBJC_CLASS CAMediaTimingFunction;
OBJC_CLASS CAPropertyAnimation;
OBJC_CLASS NSString;

typedef CAPropertyAnimation* PlatformAnimationRef;

namespace WebCore {

NSString* toCAFillModeType(PlatformCAAnimation::FillModeType);
NSString* toCAValueFunctionType(PlatformCAAnimation::ValueFunctionType);
CAMediaTimingFunction* toCAMediaTimingFunction(const TimingFunction*, bool reverse);

class PlatformCAAnimationMac final : public PlatformCAAnimation {
public:
    static PassRefPtr<PlatformCAAnimation> create(AnimationType, const String& keyPath);
    static PassRefPtr<PlatformCAAnimation> create(PlatformAnimationRef);

    virtual ~PlatformCAAnimationMac();

    virtual bool isPlatformCAAnimationMac() const override { return true; }

    virtual PassRefPtr<PlatformCAAnimation> copy() const override;

    PlatformAnimationRef platformAnimation() const;
    
    virtual String keyPath() const override;
    
    virtual CFTimeInterval beginTime() const override;
    virtual void setBeginTime(CFTimeInterval) override;
    
    virtual CFTimeInterval duration() const override;
    virtual void setDuration(CFTimeInterval) override;
    
    virtual float speed() const override;
    virtual void setSpeed(float) override;

    virtual CFTimeInterval timeOffset() const override;
    virtual void setTimeOffset(CFTimeInterval) override;

    virtual float repeatCount() const override;
    virtual void setRepeatCount(float) override;

    virtual bool autoreverses() const override;
    virtual void setAutoreverses(bool) override;

    virtual FillModeType fillMode() const override;
    virtual void setFillMode(FillModeType) override;
    
    virtual void setTimingFunction(const TimingFunction*, bool reverse = false) override;
    void copyTimingFunctionFrom(const PlatformCAAnimation*) override;

    virtual bool isRemovedOnCompletion() const override;
    virtual void setRemovedOnCompletion(bool) override;

    virtual bool isAdditive() const override;
    virtual void setAdditive(bool) override;

    virtual ValueFunctionType valueFunction() const override;
    virtual void setValueFunction(ValueFunctionType) override;

    // Basic-animation properties.
    virtual void setFromValue(float) override;
    virtual void setFromValue(const WebCore::TransformationMatrix&) override;
    virtual void setFromValue(const FloatPoint3D&) override;
    virtual void setFromValue(const WebCore::Color&) override;
#if ENABLE(CSS_FILTERS)
    virtual void setFromValue(const FilterOperation*, int internalFilterPropertyIndex) override;
#endif
    virtual void copyFromValueFrom(const PlatformCAAnimation*) override;

    virtual void setToValue(float) override;
    virtual void setToValue(const WebCore::TransformationMatrix&) override;
    virtual void setToValue(const FloatPoint3D&) override;
    virtual void setToValue(const WebCore::Color&) override;
#if ENABLE(CSS_FILTERS)
    virtual void setToValue(const FilterOperation*, int internalFilterPropertyIndex) override;
#endif
    virtual void copyToValueFrom(const PlatformCAAnimation*) override;

    // Keyframe-animation properties.
    virtual void setValues(const Vector<float>&) override;
    virtual void setValues(const Vector<WebCore::TransformationMatrix>&) override;
    virtual void setValues(const Vector<FloatPoint3D>&) override;
    virtual void setValues(const Vector<WebCore::Color>&) override;
#if ENABLE(CSS_FILTERS)
    virtual void setValues(const Vector<RefPtr<FilterOperation>>&, int internalFilterPropertyIndex) override;
#endif
    virtual void copyValuesFrom(const PlatformCAAnimation*) override;

    virtual void setKeyTimes(const Vector<float>&) override;
    virtual void copyKeyTimesFrom(const PlatformCAAnimation*) override;

    virtual void setTimingFunctions(const Vector<const TimingFunction*>&, bool reverse = false) override;
    virtual void copyTimingFunctionsFrom(const PlatformCAAnimation*) override;

protected:
    PlatformCAAnimationMac(AnimationType, const String& keyPath);
    PlatformCAAnimationMac(PlatformAnimationRef);

private:
    RetainPtr<CAPropertyAnimation> m_animation;
};

PLATFORM_CAANIMATION_TYPE_CASTS(PlatformCAAnimationMac, isPlatformCAAnimationMac())

}

#endif // PlatformCAAnimationMac_h
