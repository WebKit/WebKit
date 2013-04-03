/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Collabora Ltd.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformClutterAnimation_h
#define PlatformClutterAnimation_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "FilterOperation.h"
#include "FloatPoint3D.h"
#include "TransformationMatrix.h"
#include <glib-object.h>
#include <glib.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/gobject/GRefPtr.h>

typedef struct _ClutterActor ClutterActor;
typedef struct _ClutterTimeline ClutterTimeline;
typedef struct _GraphicsLayerActor GraphicsLayerActor;

namespace WebCore {

class FloatRect;
class PlatformClutterAnimation;
class TimingFunction;

class PlatformClutterAnimation : public RefCounted<PlatformClutterAnimation> {
public:
    enum AnimationType { Basic, Keyframe };
    enum AnimatedPropertyType { NoAnimatedPropertyType, Transform, Opacity, BackgroundColor };
    enum FillModeType { NoFillMode, Forwards, Backwards, Both };
    enum ValueFunctionType { NoValueFunction, RotateX, RotateY, RotateZ, ScaleX, ScaleY, ScaleZ, Scale, TranslateX, TranslateY, TranslateZ, Translate, Matrix };

    static PassRefPtr<PlatformClutterAnimation> create(AnimationType, const String& keyPath);
    static PassRefPtr<PlatformClutterAnimation> create(PlatformClutterAnimation*);

    ~PlatformClutterAnimation();

    static bool supportsValueFunction();
    static bool supportsAdditiveValueFunction();

    AnimationType animationType() const { return m_type; }

    double beginTime() const;
    void setBeginTime(double);

    double duration() const;
    void setDuration(double);

    float speed() const;
    void setSpeed(float);

    double timeOffset() const;
    void setTimeOffset(double);

    float repeatCount() const;
    void setRepeatCount(float);

    bool autoreverses() const;
    void setAutoreverses(bool);

    FillModeType fillMode() const;
    void setFillMode(FillModeType);

    void setTimingFunction(const TimingFunction*, bool reverse = false);
    void copyTimingFunctionFrom(const PlatformClutterAnimation*);

    bool isRemovedOnCompletion() const;
    void setRemovedOnCompletion(bool);

    bool isAdditive() const;
    void setAdditive(bool);

    ValueFunctionType valueFunction() const;
    void setValueFunction(ValueFunctionType);

    // Basic-animation properties.
    void setFromValue(float);
    void setFromValue(const WebCore::TransformationMatrix&);
    void setFromValue(const FloatPoint3D&);
    void setFromValue(const WebCore::Color&);
    void copyFromValueFrom(const PlatformClutterAnimation*);

    void setToValue(float);
    void setToValue(const WebCore::TransformationMatrix&);
    void setToValue(const FloatPoint3D&);
    void setToValue(const WebCore::Color&);
    void copyToValueFrom(const PlatformClutterAnimation*);

    // Keyframe-animation properties.
    void setValues(const Vector<float>&);
    void setValues(const Vector<WebCore::TransformationMatrix>&);
    void setValues(const Vector<FloatPoint3D>&);
    void setValues(const Vector<WebCore::Color>&);
    void copyValuesFrom(const PlatformClutterAnimation*);

    void setKeyTimes(const Vector<float>&);
    void copyKeyTimesFrom(const PlatformClutterAnimation*);

    void setTimingFunctions(const Vector<const TimingFunction*>&, bool reverse = false);
    void copyTimingFunctionsFrom(const PlatformClutterAnimation*);

    void addAnimationForKey(GraphicsLayerActor*, const String&);
    void removeAnimationForKey(GraphicsLayerActor*, const String&);

    void animationDidStart();

protected:
    PlatformClutterAnimation(AnimationType, const String& keyPath);
    PlatformClutterAnimation(const PlatformClutterAnimation*);

private:
    ClutterTimeline* timeline() const;
    AnimatedPropertyType stringToAnimatedPropertyType(const String& keyPath) const;

    void addClutterTransitionForProperty(const String& property, const float fromValue, const float toValue);
    void addClutterTransitionForProperty(const String& property, const WebCore::TransformationMatrix&, const WebCore::TransformationMatrix&);
    void addClutterTransitionForProperty(const String& property, const FloatPoint3D& fromValue, const FloatPoint3D& toValue);

    void addClutterKeyframeTransitionForProperty(const String& property, const Vector<float>& values);
    void addClutterKeyframeTransitionForProperty(const String& property, const Vector<WebCore::TransformationMatrix>& values);
    void addClutterKeyframeTransitionForProperty(const String& property, const Vector<FloatPoint3D>& values);

    void addOpacityTransition();
    void addTransformTransition();

    AnimationType m_type;
    AnimatedPropertyType m_animatedPropertyType;

    GRefPtr<GObject> m_animation;
    GRefPtr<ClutterActor> m_layer;

    bool m_additive;

    float m_fromValue;
    float m_toValue;

    FloatPoint3D m_fromValue3D;
    FloatPoint3D m_toValue3D;

    WebCore::TransformationMatrix m_fromValueMatrix;
    WebCore::TransformationMatrix m_toValueMatrix;

    float m_repeatCount;

    const TimingFunction* m_timingFunction;
    ValueFunctionType m_valueFunctionType;

    Vector<float> m_keyTimes;
    Vector<const TimingFunction*> m_timingFunctions;

    Vector<float> m_values;
    Vector<FloatPoint3D> m_values3D;
    Vector<WebCore::TransformationMatrix> m_valuesMatrix;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // PlatformClutterAnimation_h
