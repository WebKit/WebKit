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

    AnimationType animationType() const { return m_properties.animationType; }
    void setHasExplicitBeginTime(bool hasExplicitBeginTime) { m_properties.hasExplicitBeginTime = hasExplicitBeginTime; }
    bool hasExplicitBeginTime() const { return m_properties.hasExplicitBeginTime; }

    void didStart(CFTimeInterval beginTime) { m_properties.beginTime = beginTime; }

    class KeyframeValue {
    public:
        enum KeyframeType {
            NumberKeyType,
            ColorKeyType,
            PointKeyType,
            TransformKeyType,
            FilterKeyType,
        };

        ~KeyframeValue()
        {
        }

        KeyframeValue(float value = 0)
            : keyType(NumberKeyType)
            , number(value)
        {
        }

        KeyframeValue(WebCore::Color value)
            : keyType(ColorKeyType)
            , color(value)
        {
        }

        KeyframeValue(const WebCore::FloatPoint3D& value)
            : keyType(PointKeyType)
            , point(value)
        {
        }

        KeyframeValue(const WebCore::TransformationMatrix& value)
            : keyType(TransformKeyType)
            , transform(value)
        {
        }

        KeyframeValue(RefPtr<WebCore::FilterOperation>&& value)
            : keyType(FilterKeyType)
            , filter(WTFMove(value))
        {
        }

        KeyframeValue(const KeyframeValue& other)
        {
            *this = other;
        }

        KeyframeValue& operator=(const KeyframeValue& other)
        {
            keyType = other.keyType;
            switch (keyType) {
            case NumberKeyType:
                number = other.number;
                break;
            case ColorKeyType:
                color = other.color;
                break;
            case PointKeyType:
                point = other.point;
                break;
            case TransformKeyType:
                transform = other.transform;
                break;
            case FilterKeyType:
                filter = other.filter;
                break;
            }

            return *this;
        }

        KeyframeType keyframeType() const { return keyType; }

        float numberValue() const
        {
            ASSERT(keyType == NumberKeyType);
            return number;
        }

        WebCore::Color colorValue() const
        {
            ASSERT(keyType == ColorKeyType);
            return color;
        }

        const WebCore::FloatPoint3D& pointValue() const
        {
            ASSERT(keyType == PointKeyType);
            return point;
        }

        const WebCore::TransformationMatrix& transformValue() const
        {
            ASSERT(keyType == TransformKeyType);
            return transform;
        }

        const WebCore::FilterOperation* filterValue() const
        {
            ASSERT(keyType == FilterKeyType);
            return filter.get();
        }

        void encode(IPC::Encoder&) const;
        static Optional<KeyframeValue> decode(IPC::Decoder&);

    private:
        KeyframeType keyType;
        union {
            float number;
            WebCore::Color color;
            WebCore::FloatPoint3D point;
            WebCore::TransformationMatrix transform;
        };
        RefPtr<WebCore::FilterOperation> filter;
    };

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
    };

    const Properties& properties() const { return m_properties; }

    typedef Vector<std::pair<String, Properties>> AnimationsList;
    static void updateLayerAnimations(CALayer *, RemoteLayerTreeHost*, const AnimationsList& animationsToAdd, const HashSet<String>& animationsToRemove);

private:
    PlatformCAAnimationRemote(AnimationType, const String& keyPath);

    Properties m_properties;
};

WTF::TextStream& operator<<(WTF::TextStream&, const PlatformCAAnimationRemote::KeyframeValue&);
WTF::TextStream& operator<<(WTF::TextStream&, const PlatformCAAnimationRemote::Properties&);

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_CAANIMATION(WebKit::PlatformCAAnimationRemote, isPlatformCAAnimationRemote())

namespace WTF {

template<> struct EnumTraits<WebKit::PlatformCAAnimationRemote::KeyframeValue::KeyframeType> {
    using values = EnumValues<
        WebKit::PlatformCAAnimationRemote::KeyframeValue::KeyframeType,
        WebKit::PlatformCAAnimationRemote::KeyframeValue::KeyframeType::NumberKeyType,
        WebKit::PlatformCAAnimationRemote::KeyframeValue::KeyframeType::ColorKeyType,
        WebKit::PlatformCAAnimationRemote::KeyframeValue::KeyframeType::PointKeyType,
        WebKit::PlatformCAAnimationRemote::KeyframeValue::KeyframeType::TransformKeyType,
        WebKit::PlatformCAAnimationRemote::KeyframeValue::KeyframeType::FilterKeyType
    >;
};

} // namespace WTF
