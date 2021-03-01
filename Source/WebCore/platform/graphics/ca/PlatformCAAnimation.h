/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "FilterOperation.h"
#include "FloatPoint3D.h"
#include "TransformationMatrix.h"
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class FloatRect;
class TimingFunction;

class PlatformCAAnimation : public RefCounted<PlatformCAAnimation> {
public:
    enum AnimationType { Basic, Group, Keyframe, Spring };
    enum FillModeType { NoFillMode, Forwards, Backwards, Both };
    enum ValueFunctionType { NoValueFunction, RotateX, RotateY, RotateZ, ScaleX, ScaleY, ScaleZ, Scale, TranslateX, TranslateY, TranslateZ, Translate };

    virtual ~PlatformCAAnimation() = default;

    virtual bool isPlatformCAAnimationCocoa() const { return false; }
    virtual bool isPlatformCAAnimationWin() const { return false; }
    virtual bool isPlatformCAAnimationRemote() const { return false; }
    
    virtual Ref<PlatformCAAnimation> copy() const = 0;
    
    AnimationType animationType() const { return m_type; }
    virtual String keyPath() const = 0;
    
    virtual CFTimeInterval beginTime() const = 0;
    virtual void setBeginTime(CFTimeInterval) = 0;
    
    virtual CFTimeInterval duration() const = 0;
    virtual void setDuration(CFTimeInterval) = 0;
    
    virtual float speed() const = 0;
    virtual void setSpeed(float) = 0;

    virtual CFTimeInterval timeOffset() const = 0;
    virtual void setTimeOffset(CFTimeInterval) = 0;

    virtual float repeatCount() const = 0;
    virtual void setRepeatCount(float) = 0;

    virtual bool autoreverses() const = 0;
    virtual void setAutoreverses(bool) = 0;

    virtual FillModeType fillMode() const = 0;
    virtual void setFillMode(FillModeType) = 0;
    
    virtual void setTimingFunction(const TimingFunction*, bool reverse = false) = 0;
    virtual void copyTimingFunctionFrom(const PlatformCAAnimation&) = 0;

    virtual bool isRemovedOnCompletion() const = 0;
    virtual void setRemovedOnCompletion(bool) = 0;

    virtual bool isAdditive() const = 0;
    virtual void setAdditive(bool) = 0;

    virtual ValueFunctionType valueFunction() const = 0;
    virtual void setValueFunction(ValueFunctionType) = 0;

    // Basic-animation properties.
    virtual void setFromValue(float) = 0;
    virtual void setFromValue(const WebCore::TransformationMatrix&) = 0;
    virtual void setFromValue(const FloatPoint3D&) = 0;
    virtual void setFromValue(const WebCore::Color&) = 0;
    virtual void setFromValue(const FilterOperation*, int internalFilterPropertyIndex) = 0;
    virtual void copyFromValueFrom(const PlatformCAAnimation&) = 0;

    virtual void setToValue(float) = 0;
    virtual void setToValue(const WebCore::TransformationMatrix&) = 0;
    virtual void setToValue(const FloatPoint3D&) = 0;
    virtual void setToValue(const WebCore::Color&) = 0;
    virtual void setToValue(const FilterOperation*, int internalFilterPropertyIndex) = 0;
    virtual void copyToValueFrom(const PlatformCAAnimation&) = 0;

    // Keyframe-animation properties.
    virtual void setValues(const Vector<float>&) = 0;
    virtual void setValues(const Vector<WebCore::TransformationMatrix>&) = 0;
    virtual void setValues(const Vector<FloatPoint3D>&) = 0;
    virtual void setValues(const Vector<WebCore::Color>&) = 0;
    virtual void setValues(const Vector<RefPtr<FilterOperation>>&, int internalFilterPropertyIndex) = 0;
    virtual void copyValuesFrom(const PlatformCAAnimation&) = 0;

    virtual void setKeyTimes(const Vector<float>&) = 0;
    virtual void copyKeyTimesFrom(const PlatformCAAnimation&) = 0;

    virtual void setTimingFunctions(const Vector<const TimingFunction*>&, bool reverse = false) = 0;
    virtual void copyTimingFunctionsFrom(const PlatformCAAnimation&) = 0;

    // Animation group properties.
    virtual void setAnimations(const Vector<RefPtr<PlatformCAAnimation>>&) = 0;
    virtual void copyAnimationsFrom(const PlatformCAAnimation&) = 0;

    void setActualStartTimeIfNeeded(CFTimeInterval t)
    {
        if (beginTime() <= 0)
            setBeginTime(t);
    }

    bool isBasicAnimation() const;

protected:
    PlatformCAAnimation(AnimationType type = Basic)
        : m_type(type)
    {
    }

    void setType(AnimationType type) { m_type = type; }

private:
    AnimationType m_type;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, PlatformCAAnimation::AnimationType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, PlatformCAAnimation::FillModeType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, PlatformCAAnimation::ValueFunctionType);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CAANIMATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::PlatformCAAnimation& animation) { return animation.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

namespace WTF {

template<> struct EnumTraits<WebCore::PlatformCAAnimation::AnimationType> {
    using values = EnumValues<
        WebCore::PlatformCAAnimation::AnimationType,
        WebCore::PlatformCAAnimation::AnimationType::Basic,
        WebCore::PlatformCAAnimation::AnimationType::Group,
        WebCore::PlatformCAAnimation::AnimationType::Keyframe,
        WebCore::PlatformCAAnimation::AnimationType::Spring
    >;
};

template<> struct EnumTraits<WebCore::PlatformCAAnimation::FillModeType> {
    using values = EnumValues<
        WebCore::PlatformCAAnimation::FillModeType,
        WebCore::PlatformCAAnimation::FillModeType::NoFillMode,
        WebCore::PlatformCAAnimation::FillModeType::Forwards,
        WebCore::PlatformCAAnimation::FillModeType::Backwards,
        WebCore::PlatformCAAnimation::FillModeType::Both
    >;
};

template<> struct EnumTraits<WebCore::PlatformCAAnimation::ValueFunctionType> {
    using values = EnumValues<
        WebCore::PlatformCAAnimation::ValueFunctionType,
        WebCore::PlatformCAAnimation::ValueFunctionType::NoValueFunction,
        WebCore::PlatformCAAnimation::ValueFunctionType::RotateX,
        WebCore::PlatformCAAnimation::ValueFunctionType::RotateY,
        WebCore::PlatformCAAnimation::ValueFunctionType::RotateZ,
        WebCore::PlatformCAAnimation::ValueFunctionType::ScaleX,
        WebCore::PlatformCAAnimation::ValueFunctionType::ScaleY,
        WebCore::PlatformCAAnimation::ValueFunctionType::ScaleZ,
        WebCore::PlatformCAAnimation::ValueFunctionType::Scale,
        WebCore::PlatformCAAnimation::ValueFunctionType::TranslateX,
        WebCore::PlatformCAAnimation::ValueFunctionType::TranslateY,
        WebCore::PlatformCAAnimation::ValueFunctionType::TranslateZ,
        WebCore::PlatformCAAnimation::ValueFunctionType::Translate
    >;
};

} // namespace WTF
