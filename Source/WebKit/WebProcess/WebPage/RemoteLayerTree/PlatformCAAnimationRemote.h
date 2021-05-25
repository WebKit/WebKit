/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include <WebCore/PlatformCAAnimation.h>
#include <wtf/EnumTraits.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace IPC {
class Encoder;
class Decoder;
};

namespace WTF {
class TextStream;
};

OBJC_CLASS CALayer;

namespace WebKit {

class RemoteLayerTreeHost;

class PlatformCAAnimationRemote final : public WebCore::PlatformCAAnimation {
public:
    static Ref<PlatformCAAnimation> create(AnimationType, const String& keyPath);

    virtual ~PlatformCAAnimationRemote() { }

    bool isPlatformCAAnimationRemote() const override { return true; }

    Ref<PlatformCAAnimation> copy() const override;

    String keyPath() const override;

    CFTimeInterval beginTime() const override;
    void setBeginTime(CFTimeInterval) override;

    CFTimeInterval duration() const override;
    void setDuration(CFTimeInterval) override;

    float speed() const override;
    void setSpeed(float) override;

    CFTimeInterval timeOffset() const override;
    void setTimeOffset(CFTimeInterval) override;

    float repeatCount() const override;
    void setRepeatCount(float) override;

    bool autoreverses() const override;
    void setAutoreverses(bool) override;

    FillModeType fillMode() const override;
    void setFillMode(FillModeType) override;

    void setTimingFunction(const WebCore::TimingFunction*, bool reverse = false) override;
    void copyTimingFunctionFrom(const WebCore::PlatformCAAnimation&) override;

    bool isRemovedOnCompletion() const override;
    void setRemovedOnCompletion(bool) override;

    bool isAdditive() const override;
    void setAdditive(bool) override;

    ValueFunctionType valueFunction() const override;
    void setValueFunction(ValueFunctionType) override;

    // Basic-animation properties.
    void setFromValue(float) override;
    void setFromValue(const WebCore::TransformationMatrix&) override;
    void setFromValue(const WebCore::FloatPoint3D&) override;
    void setFromValue(const WebCore::Color&) override;
    void setFromValue(const WebCore::FilterOperation*, int internalFilterPropertyIndex) override;
    void copyFromValueFrom(const WebCore::PlatformCAAnimation&) override;

    void setToValue(float) override;
    void setToValue(const WebCore::TransformationMatrix&) override;
    void setToValue(const WebCore::FloatPoint3D&) override;
    void setToValue(const WebCore::Color&) override;
    void setToValue(const WebCore::FilterOperation*, int internalFilterPropertyIndex) override;
    void copyToValueFrom(const WebCore::PlatformCAAnimation&) override;

    // Keyframe-animation properties.
    void setValues(const Vector<float>&) override;
    void setValues(const Vector<WebCore::TransformationMatrix>&) override;
    void setValues(const Vector<WebCore::FloatPoint3D>&) override;
    void setValues(const Vector<WebCore::Color>&) override;
    void setValues(const Vector<RefPtr<WebCore::FilterOperation>>&, int internalFilterPropertyIndex) override;
    void copyValuesFrom(const WebCore::PlatformCAAnimation&) override;

    void setKeyTimes(const Vector<float>&) override;
    void copyKeyTimesFrom(const WebCore::PlatformCAAnimation&) override;

    void setTimingFunctions(const Vector<const WebCore::TimingFunction*>&, bool reverse = false) override;
    void copyTimingFunctionsFrom(const WebCore::PlatformCAAnimation&) override;

    // Animation group properties.
    void setAnimations(const Vector<RefPtr<PlatformCAAnimation>>&) final;
    void copyAnimationsFrom(const PlatformCAAnimation&) final;

    AnimationType animationType() const { return m_properties.animationType; }
    void setHasExplicitBeginTime(bool hasExplicitBeginTime) { m_properties.hasExplicitBeginTime = hasExplicitBeginTime; }
    bool hasExplicitBeginTime() const { return m_properties.hasExplicitBeginTime; }

    void didStart(CFTimeInterval beginTime) { m_properties.beginTime = beginTime; }


    using KeyframeValue = Variant<float, WebCore::Color, WebCore::FloatPoint3D, WebCore::TransformationMatrix, RefPtr<WebCore::FilterOperation>>;

    struct Properties {
        Properties()
            : animationType(Basic)
            , beginTime(0)
            , duration(0)
            , timeOffset(0)
            , repeatCount(1)
            , speed(1)
            , fillMode(NoFillMode)
            , valueFunction(NoValueFunction)
            , autoReverses(false)
            , removedOnCompletion(true)
            , additive(false)
            , reverseTimingFunctions(false)
            , hasExplicitBeginTime(false)
        {
        }

        void encode(IPC::Encoder&) const;
        static Optional<Properties> decode(IPC::Decoder&);

        String keyPath;
        PlatformCAAnimation::AnimationType animationType;

        CFTimeInterval beginTime;
        double duration;
        double timeOffset;
        float repeatCount;
        float speed;

        PlatformCAAnimation::FillModeType fillMode;
        PlatformCAAnimation::ValueFunctionType valueFunction;
        RefPtr<WebCore::TimingFunction> timingFunction;

        bool autoReverses;
        bool removedOnCompletion;
        bool additive;
        bool reverseTimingFunctions;
        bool hasExplicitBeginTime;

        // For basic animations, these vectors have two entries. For keyframe animations, two or more.
        // timingFunctions has n-1 entries.
        Vector<KeyframeValue> keyValues;
        Vector<float> keyTimes;
        Vector<RefPtr<WebCore::TimingFunction>> timingFunctions;

        Vector<Properties> animations;
    };

    const Properties& properties() const { return m_properties; }

    typedef Vector<std::pair<String, Properties>> AnimationsList;
    static void updateLayerAnimations(CALayer *, RemoteLayerTreeHost*, const AnimationsList& animationsToAdd, const HashSet<String>& animationsToRemove);

private:
    PlatformCAAnimationRemote(AnimationType, const String& keyPath);

    Properties m_properties;
};

WTF::TextStream& operator<<(WTF::TextStream&, const PlatformCAAnimationRemote::Properties&);

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_CAANIMATION(WebKit::PlatformCAAnimationRemote, isPlatformCAAnimationRemote())
