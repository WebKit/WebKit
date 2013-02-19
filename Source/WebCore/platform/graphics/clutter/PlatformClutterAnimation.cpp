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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "PlatformClutterAnimation.h"

#include "FloatConversion.h"
#include "GraphicsLayerActor.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "TimingFunction.h"
#include "UnitBezier.h"
#include <limits.h>
#include <wtf/CurrentTime.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/UnusedParam.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

static void timelineStartedCallback(ClutterTimeline*, PlatformClutterAnimation* animation)
{
    animation->animationDidStart();
}

static String toClutterActorPropertyString(const PlatformClutterAnimation::ValueFunctionType valueFunctionType)
{
    // ClutterActor doesn't have 'scale' and 'translate' properties. So we should support
    // 'scale' and 'translate' ValueFunctionType by combination of existing property animations. 
    const char* clutterActorProperty[] = { "NoProperty", "rotation-angle-x", "rotation-angle-y", "rotation-angle-z", "scale-x", "scale-y", "scale-z", "scale", "translation-x", "translation-y", "translation-z", "translate", "transform" }; 
    return clutterActorProperty[valueFunctionType];
}

static ClutterAnimationMode toClutterAnimationMode(const TimingFunction* timingFunction)
{
    ASSERT(timingFunction);

    if (timingFunction->isLinearTimingFunction())
        return CLUTTER_LINEAR;
    if (timingFunction->isCubicBezierTimingFunction()) {
        CubicBezierTimingFunction::TimingFunctionPreset timingFunctionPreset = static_cast<const CubicBezierTimingFunction*>(timingFunction)->timingFunctionPreset(); 
        switch (timingFunctionPreset) {
        case CubicBezierTimingFunction::Ease:
            return CLUTTER_EASE;
        case CubicBezierTimingFunction::EaseIn:
            return CLUTTER_EASE_IN;
        case CubicBezierTimingFunction::EaseOut:
            return CLUTTER_EASE_OUT;
        case CubicBezierTimingFunction::EaseInOut:
            return CLUTTER_EASE_IN_OUT;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return CLUTTER_EASE;
}

static gboolean clutterMatrixProgress(const GValue* fromValue, const GValue* toValue, gdouble progress, GValue* returnValue)
{
    const CoglMatrix* fromCoglMatrix = static_cast<CoglMatrix*>(g_value_get_boxed(fromValue));
    const CoglMatrix* toCoglMatrix = static_cast<CoglMatrix*>(g_value_get_boxed(toValue));

    ASSERT(fromCoglMatrix && toCoglMatrix);

    TransformationMatrix fromMatrix(fromCoglMatrix);
    TransformationMatrix toMatrix(toCoglMatrix);
    toMatrix.blend(fromMatrix, progress);

    CoglMatrix resultCoglMatrix = toMatrix;
    g_value_set_boxed(returnValue, &resultCoglMatrix);

    return true;
}

PlatformClutterAnimation::AnimatedPropertyType PlatformClutterAnimation::stringToAnimatedPropertyType(const String& keyPath) const
{
    if (keyPath == "transform")
        return Transform;
    if (keyPath == "opacity")
        return Opacity;
    if (keyPath == "backgroundColor")
        return BackgroundColor;
    return NoAnimatedPropertyType;
}

PassRefPtr<PlatformClutterAnimation> PlatformClutterAnimation::create(AnimationType type, const String& keyPath)
{
    return adoptRef(new PlatformClutterAnimation(type, keyPath));
}

PassRefPtr<PlatformClutterAnimation> PlatformClutterAnimation::create(PlatformClutterAnimation* animation)
{
    return adoptRef(new PlatformClutterAnimation(animation));
}

PlatformClutterAnimation::PlatformClutterAnimation(AnimationType type, const String& keyPath)
    : m_type(type)
    , m_animatedPropertyType(stringToAnimatedPropertyType(keyPath))
    , m_additive(false)
    , m_fromValue(0)
    , m_toValue(0)
    , m_repeatCount(0)
    , m_timingFunction(0)
    , m_valueFunctionType(NoValueFunction)
{
    m_animation = adoptGRef(G_OBJECT(clutter_transition_group_new()));
}

PlatformClutterAnimation::PlatformClutterAnimation(const PlatformClutterAnimation* animation)
{
    notImplemented();
}

PlatformClutterAnimation::~PlatformClutterAnimation()
{
    m_animation.clear();
    m_layer.clear();
}

bool PlatformClutterAnimation::supportsValueFunction()
{
    return true;
}

bool PlatformClutterAnimation::supportsAdditiveValueFunction()
{
    // FIXME: Clutter 1.12 doesn't support additive valueFunction type animations.
    // So, we use matrix animation instead until clutter supports it.
    return false;
}

double PlatformClutterAnimation::beginTime() const
{
    notImplemented();
    return 0;
}

void PlatformClutterAnimation::setBeginTime(double value)
{
    notImplemented();
}

double PlatformClutterAnimation::duration() const
{
    double duration = clutter_timeline_get_duration(CLUTTER_TIMELINE(m_animation.get()));
    return duration / 1000;
}

void PlatformClutterAnimation::setDuration(double value)
{
    // Clutter Animation sets the duration time in milliseconds.
    gint duration = value * 1000;
    clutter_timeline_set_duration(CLUTTER_TIMELINE(m_animation.get()), duration);
}

float PlatformClutterAnimation::speed() const
{
    notImplemented();
    return 0;
}

void PlatformClutterAnimation::setSpeed(float value)
{
    notImplemented();
}

double PlatformClutterAnimation::timeOffset() const
{
    notImplemented();
    return 0;
}

void PlatformClutterAnimation::setTimeOffset(double value)
{
    notImplemented();
}

float PlatformClutterAnimation::repeatCount() const
{
    return m_repeatCount;
}

void PlatformClutterAnimation::setRepeatCount(float value)
{
    if (m_repeatCount == value)
        return;

    m_repeatCount = value;
    clutter_timeline_set_repeat_count(timeline(), static_cast<gint>(value == numeric_limits<float>::max() ? -1 : value)); 
}

bool PlatformClutterAnimation::autoreverses() const
{
    notImplemented();
    return false;
}

void PlatformClutterAnimation::setAutoreverses(bool value)
{
    notImplemented();
}

PlatformClutterAnimation::FillModeType PlatformClutterAnimation::fillMode() const
{
    notImplemented();
    return PlatformClutterAnimation::NoFillMode;
}

void PlatformClutterAnimation::setFillMode(FillModeType value)
{
    notImplemented();
}

void PlatformClutterAnimation::setTimingFunction(const TimingFunction* timingFunction, bool reverse)
{
    if (!timingFunction)
        return;

    m_timingFunction = timingFunction;
}

void PlatformClutterAnimation::copyTimingFunctionFrom(const PlatformClutterAnimation* value)
{
    notImplemented();
}

bool PlatformClutterAnimation::isRemovedOnCompletion() const
{
    notImplemented();
    return false;
}

void PlatformClutterAnimation::setRemovedOnCompletion(bool value)
{
    notImplemented();
}

bool PlatformClutterAnimation::isAdditive() const
{
    return m_additive;
}

void PlatformClutterAnimation::setAdditive(bool value)
{
    if (m_additive == value)
        return;

    m_additive = value;
}

PlatformClutterAnimation::ValueFunctionType PlatformClutterAnimation::valueFunction() const
{
    return m_valueFunctionType;
}

void PlatformClutterAnimation::setValueFunction(ValueFunctionType value)
{
    if (m_valueFunctionType == value)
        return;

    m_valueFunctionType = value;
}

void PlatformClutterAnimation::setFromValue(float value)
{
    if (animationType() != Basic || m_fromValue == value)
        return;

    m_fromValue = value;
}

void PlatformClutterAnimation::setFromValue(const WebCore::TransformationMatrix& value)
{
    if (animationType() != Basic || m_fromValueMatrix == value)
        return;

    m_fromValueMatrix = value;
}

void PlatformClutterAnimation::setFromValue(const FloatPoint3D& value)
{
    if (animationType() != Basic || m_fromValue3D == value)
        return;

    m_fromValue3D = value;
}

void PlatformClutterAnimation::setFromValue(const WebCore::Color& value)
{
    notImplemented();
}

void PlatformClutterAnimation::copyFromValueFrom(const PlatformClutterAnimation* value)
{
    notImplemented();
}

void PlatformClutterAnimation::setToValue(float value)
{
    if (animationType() != Basic || m_toValue == value)
        return;

    m_toValue = value;
}

void PlatformClutterAnimation::setToValue(const WebCore::TransformationMatrix& value)
{
    if (animationType() != Basic || m_toValueMatrix == value)
        return;

    m_toValueMatrix = value;
}

void PlatformClutterAnimation::setToValue(const FloatPoint3D& value)
{
    if (animationType() != Basic || m_toValue3D == value)
        return;

    m_toValue3D = value;
}

void PlatformClutterAnimation::setToValue(const WebCore::Color& value)
{
    notImplemented();
}

void PlatformClutterAnimation::copyToValueFrom(const PlatformClutterAnimation* value)
{
    notImplemented();
}

void PlatformClutterAnimation::setValues(const Vector<float>& value)
{
    ASSERT(animationType() == Keyframe);

    m_values = value;
}

void PlatformClutterAnimation::setValues(const Vector<WebCore::TransformationMatrix>& value)
{
    notImplemented();
}

void PlatformClutterAnimation::setValues(const Vector<FloatPoint3D>& value)
{
    ASSERT(animationType() == Keyframe);

    m_values3D = value;
}

void PlatformClutterAnimation::setValues(const Vector<WebCore::Color>& value)
{
    notImplemented();
}

void PlatformClutterAnimation::copyValuesFrom(const PlatformClutterAnimation* value)
{
    notImplemented();
}

void PlatformClutterAnimation::setKeyTimes(const Vector<float>& value)
{
    ASSERT(animationType() == Keyframe);

    m_keyTimes = value;
}

void PlatformClutterAnimation::copyKeyTimesFrom(const PlatformClutterAnimation* value)
{
    notImplemented();
}

void PlatformClutterAnimation::setTimingFunctions(const Vector<const TimingFunction*>& value, bool reverse)
{
    ASSERT(animationType() == Keyframe);

    m_timingFunctions = value;
}

void PlatformClutterAnimation::copyTimingFunctionsFrom(const PlatformClutterAnimation* value)
{
    notImplemented();
}

void PlatformClutterAnimation::animationDidStart()
{
    ASSERT(CLUTTER_IS_ACTOR(m_layer.get()));

    PlatformClutterLayerClient* client = graphicsLayerActorGetClient(GRAPHICS_LAYER_ACTOR(m_layer.get()));
    if (!client)
        return;

    client->platformClutterLayerAnimationStarted(WTF::currentTime());
}

ClutterTimeline* PlatformClutterAnimation::timeline() const
{
    ASSERT(m_animation);
    return CLUTTER_TIMELINE(m_animation.get());
}

void PlatformClutterAnimation::addClutterTransitionForProperty(const String& property, const float fromValue, const float toValue)
{
    ASSERT(property != "NoProperty");

    GType gType = (property == "opacity" ? G_TYPE_UINT : G_TYPE_FLOAT);

    GRefPtr<ClutterTransition> transition = adoptGRef(clutter_property_transition_new(property.utf8().data()));
    clutter_transition_set_from(transition.get(), gType, (gType == G_TYPE_UINT ? static_cast<unsigned>(fromValue) : fromValue));
    clutter_transition_set_to(transition.get(), gType, (gType == G_TYPE_UINT ? static_cast<unsigned>(toValue) : toValue));

    clutter_timeline_set_progress_mode(timeline(), toClutterAnimationMode(m_timingFunction));

    clutter_transition_group_add_transition(CLUTTER_TRANSITION_GROUP(m_animation.get()), transition.get());
}

void PlatformClutterAnimation::addClutterTransitionForProperty(const String& property, const WebCore::TransformationMatrix& fromValue, const WebCore::TransformationMatrix& toValue)
{
    ASSERT(property != "NoProperty");

    const CoglMatrix fromCoglMatrix = fromValue;
    const CoglMatrix toCoglMatrix = toValue;

    GRefPtr<ClutterTransition> transition = adoptGRef(clutter_property_transition_new(property.utf8().data()));
    clutter_transition_set_from(transition.get(), CLUTTER_TYPE_MATRIX, &fromCoglMatrix);
    clutter_transition_set_to(transition.get(), CLUTTER_TYPE_MATRIX, &toCoglMatrix);

    clutter_timeline_set_progress_mode(timeline(), toClutterAnimationMode(m_timingFunction));

    clutter_transition_group_add_transition(CLUTTER_TRANSITION_GROUP(m_animation.get()), transition.get());

    // FIXME: The matrix interpolation api, clutter_matrix_progress of Clutter 1.12 works unexpectedly.
    // So we overwrite it and handle the interpolation of two matrices with TransformationMatrix.
    // See https://bugzilla.gnome.org/show_bug.cgi?id=694197
    clutter_interval_register_progress_func(CLUTTER_TYPE_MATRIX, clutterMatrixProgress);
}

void PlatformClutterAnimation::addClutterTransitionForProperty(const String& property, const FloatPoint3D& fromValue, const FloatPoint3D& toValue)
{
    ASSERT(property != "NoProperty");

    if (property == "scale") {
        addClutterTransitionForProperty(String("scale-x"), fromValue.x(), toValue.x());
        addClutterTransitionForProperty(String("scale-y"), fromValue.y(), toValue.y());
        return;
    }
    if (property == "translate") {
        addClutterTransitionForProperty(String("translation-x"), fromValue.x(), toValue.x());
        addClutterTransitionForProperty(String("translation-y"), fromValue.x(), toValue.y());
        return;
    }

    ASSERT_NOT_REACHED();
}

void PlatformClutterAnimation::addClutterKeyframeTransitionForProperty(const String& property, const Vector<float>& values)
{
    ASSERT(property != "NoProperty");

    GType gType = (property == "opacity" ? G_TYPE_UINT : G_TYPE_FLOAT);

    GRefPtr<ClutterTransition> transition = adoptGRef(clutter_keyframe_transition_new(property.utf8().data()));
    clutter_transition_set_from(transition.get(), gType, values.first());
    clutter_transition_set_to(transition.get(), gType, values.last());

    // Ignore the first keyframe, since it's a '0' frame, meaningless.
    const unsigned nKeyframes = values.size() - 1;
    OwnArrayPtr<ClutterAnimationMode> animationModes = adoptArrayPtr(new ClutterAnimationMode[nKeyframes]);
    OwnArrayPtr<double> keyTimes = adoptArrayPtr(new double[nKeyframes]);
    GOwnPtr<GValue> keyValues(g_new0(GValue, nKeyframes));

    for (unsigned i = 0; i < nKeyframes; ++i) {
        keyTimes[i] = static_cast<double>(m_keyTimes[i + 1]);
        animationModes[i] = toClutterAnimationMode(m_timingFunctions[i]);
        g_value_init(&keyValues.get()[i], gType);
        if (gType == G_TYPE_UINT)
            g_value_set_uint(&keyValues.get()[i], static_cast<unsigned>(values[i + 1]));
        else
            g_value_set_float(&keyValues.get()[i], values[i + 1]);
    }

    clutter_keyframe_transition_set_key_frames(CLUTTER_KEYFRAME_TRANSITION(transition.get()), nKeyframes, keyTimes.get());
    clutter_keyframe_transition_set_values(CLUTTER_KEYFRAME_TRANSITION(transition.get()), nKeyframes, keyValues.get());
    clutter_keyframe_transition_set_modes(CLUTTER_KEYFRAME_TRANSITION(transition.get()), nKeyframes, animationModes.get());

    clutter_transition_group_add_transition(CLUTTER_TRANSITION_GROUP(m_animation.get()), transition.get());

    for (unsigned i = 0; i < nKeyframes; ++i)
        g_value_unset(&keyValues.get()[i]);
}

void PlatformClutterAnimation::addClutterKeyframeTransitionForProperty(const String& property, const Vector<FloatPoint3D>& values)
{
    ASSERT(property != "NoProperty");

    Vector<float> valuesX, valuesY;
    for (unsigned i = 0; i < values.size(); ++i) {
        valuesX.append(values[i].x());
        valuesY.append(values[i].y());
    }

    if (property == "scale") {
        addClutterKeyframeTransitionForProperty(String("scale-x"), valuesX);
        addClutterKeyframeTransitionForProperty(String("scale-y"), valuesY);
        return;
    }
    if (property == "translate") {
        addClutterKeyframeTransitionForProperty(String("translation-x"), valuesX);
        addClutterKeyframeTransitionForProperty(String("translation-y"), valuesY);
        return;
    }

    ASSERT_NOT_REACHED();
}

void PlatformClutterAnimation::addOpacityTransition()
{
    if (animationType() == Keyframe) {
        for (unsigned i = 0; i < m_values.size(); ++i)
            m_values[i] *= 255;

        addClutterKeyframeTransitionForProperty(String("opacity"), m_values);
    } else {
        m_fromValue *= 255;
        m_toValue *= 255;

        addClutterTransitionForProperty(String("opacity"), m_fromValue, m_toValue);
    }
}

void PlatformClutterAnimation::addTransformTransition()
{
    const bool isKeyframe = (animationType() == Keyframe);

    switch (m_valueFunctionType) {
    case RotateX:
    case RotateY:
    case RotateZ:
        if (isKeyframe) {
            for (unsigned i = 0; i < m_values.size(); ++i)
                m_values[i] = rad2deg(m_values[i]);

            addClutterKeyframeTransitionForProperty(toClutterActorPropertyString(m_valueFunctionType), m_values);
        } else {
            m_fromValue = rad2deg(m_fromValue);
            m_toValue = rad2deg(m_toValue);

            addClutterTransitionForProperty(toClutterActorPropertyString(m_valueFunctionType), m_fromValue, m_toValue);
        }
        break;
    case ScaleX:
    case ScaleY:
    case ScaleZ:
    case TranslateX:
    case TranslateY:
    case TranslateZ:
        if (isKeyframe)
            addClutterKeyframeTransitionForProperty(toClutterActorPropertyString(m_valueFunctionType), m_values);
        else
            addClutterTransitionForProperty(toClutterActorPropertyString(m_valueFunctionType), m_fromValue, m_toValue);
        break;
    case Scale:
    case Translate:
        if (isKeyframe)
            addClutterKeyframeTransitionForProperty(toClutterActorPropertyString(m_valueFunctionType), m_values3D); 
        else
            addClutterTransitionForProperty(toClutterActorPropertyString(m_valueFunctionType), m_fromValue3D, m_toValue3D);
        break;
    case Matrix:
        addClutterTransitionForProperty(toClutterActorPropertyString(m_valueFunctionType), m_fromValueMatrix, m_toValueMatrix);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // If clutter covers valueFunction type animations, we should release keeping transform matrix.
    // Otherwise, the transformation is applied twice unexpectedly. See graphicsLayerActorApplyTransform.
    graphicsLayerActorSetTransform(GRAPHICS_LAYER_ACTOR(m_layer.get()), 0);
}

void PlatformClutterAnimation::addAnimationForKey(GraphicsLayerActor* platformLayer, const String& key)
{
    ASSERT(!g_object_get_data(G_OBJECT(platformLayer), key.utf8().data()));

    m_layer = CLUTTER_ACTOR(platformLayer);

    if (m_animatedPropertyType == Opacity)
        addOpacityTransition();
    else if (m_animatedPropertyType == Transform)
        addTransformTransition();
    else if (m_animatedPropertyType == BackgroundColor)
        ASSERT_NOT_REACHED();
    else
        ASSERT_NOT_REACHED();

    g_signal_connect(timeline(), "started", G_CALLBACK(timelineStartedCallback), this);
    g_object_set_data(G_OBJECT(platformLayer), key.utf8().data(), this);

    clutter_actor_add_transition(m_layer.get(), key.utf8().data(), CLUTTER_TRANSITION(m_animation.get()));
}

void PlatformClutterAnimation::removeAnimationForKey(GraphicsLayerActor* layer, const String& key)
{
    clutter_actor_remove_transition(CLUTTER_ACTOR(layer), key.utf8().data());
    g_object_set_data(G_OBJECT(layer), key.utf8().data(), 0);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
