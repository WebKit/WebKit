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

#ifndef PlatformCAAnimationRemote_h
#define PlatformCAAnimationRemote_h

#include <WebCore/PlatformCAAnimation.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace IPC {
class ArgumentEncoder;
class ArgumentDecoder;
};

OBJC_CLASS CALayer;

namespace WebKit {

class RemoteLayerTreeHost;

class PlatformCAAnimationRemote final : public WebCore::PlatformCAAnimation {
public:
    static PassRefPtr<PlatformCAAnimation> create(AnimationType, const String& keyPath);

    virtual ~PlatformCAAnimationRemote() { }

    virtual bool isPlatformCAAnimationRemote() const override { return true; }

    virtual PassRefPtr<PlatformCAAnimation> copy() const override;

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

    virtual void setTimingFunction(const WebCore::TimingFunction*, bool reverse = false) override;
    void copyTimingFunctionFrom(const WebCore::PlatformCAAnimation*) override;

    virtual bool isRemovedOnCompletion() const override;
    virtual void setRemovedOnCompletion(bool) override;

    virtual bool isAdditive() const override;
    virtual void setAdditive(bool) override;

    virtual ValueFunctionType valueFunction() const override;
    virtual void setValueFunction(ValueFunctionType) override;

    // Basic-animation properties.
    virtual void setFromValue(float) override;
    virtual void setFromValue(const WebCore::TransformationMatrix&) override;
    virtual void setFromValue(const WebCore::FloatPoint3D&) override;
    virtual void setFromValue(const WebCore::Color&) override;
#if ENABLE(CSS_FILTERS)
    virtual void setFromValue(const WebCore::FilterOperation*, int internalFilterPropertyIndex) override;
#endif
    virtual void copyFromValueFrom(const WebCore::PlatformCAAnimation*) override;

    virtual void setToValue(float) override;
    virtual void setToValue(const WebCore::TransformationMatrix&) override;
    virtual void setToValue(const WebCore::FloatPoint3D&) override;
    virtual void setToValue(const WebCore::Color&) override;
#if ENABLE(CSS_FILTERS)
    virtual void setToValue(const WebCore::FilterOperation*, int internalFilterPropertyIndex) override;
#endif
    virtual void copyToValueFrom(const WebCore::PlatformCAAnimation*) override;

    // Keyframe-animation properties.
    virtual void setValues(const Vector<float>&) override;
    virtual void setValues(const Vector<WebCore::TransformationMatrix>&) override;
    virtual void setValues(const Vector<WebCore::FloatPoint3D>&) override;
    virtual void setValues(const Vector<WebCore::Color>&) override;
#if ENABLE(CSS_FILTERS)
    virtual void setValues(const Vector<RefPtr<WebCore::FilterOperation>>&, int internalFilterPropertyIndex) override;
#endif
    virtual void copyValuesFrom(const WebCore::PlatformCAAnimation*) override;

    virtual void setKeyTimes(const Vector<float>&) override;
    virtual void copyKeyTimesFrom(const WebCore::PlatformCAAnimation*) override;

    virtual void setTimingFunctions(const Vector<const WebCore::TimingFunction*>&, bool reverse = false) override;
    virtual void copyTimingFunctionsFrom(const WebCore::PlatformCAAnimation*) override;

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

        KeyframeValue(PassRefPtr<WebCore::FilterOperation> value)
            : keyType(FilterKeyType)
            , filter(value)
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

        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, KeyframeValue&);

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
            , hasNonZeroBeginTime(false)
        {
        }

        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, Properties&);

        String keyPath;
        PlatformCAAnimation::AnimationType animationType;

        double beginTime;
        double duration;
        double timeOffset;
        float repeatCount;
        float speed;

        PlatformCAAnimation::FillModeType fillMode;
        PlatformCAAnimation::ValueFunctionType valueFunction;

        bool autoReverses;
        bool removedOnCompletion;
        bool additive;
        bool reverseTimingFunctions;
        bool hasNonZeroBeginTime;

        // For basic animations, these vectors have two entries. For keyframe animations, two or more.
        // timingFunctions has n-1 entries.
        Vector<KeyframeValue> keyValues;
        Vector<float> keyTimes;
        Vector<RefPtr<WebCore::TimingFunction>> timingFunctions;
    };

    const Properties& properties() const { return m_properties; }

    typedef HashMap<String, Properties> AnimationsMap;
    static void updateLayerAnimations(CALayer *, RemoteLayerTreeHost*, const AnimationsMap& animationsToAdd, const HashSet<String>& animationsToRemove);

private:
    PlatformCAAnimationRemote(AnimationType, const String& keyPath);

    Properties m_properties;
};

PLATFORM_CAANIMATION_TYPE_CASTS(PlatformCAAnimationRemote, isPlatformCAAnimationRemote())

} // namespace WebKit

#endif // PlatformCAAnimationRemote_h
