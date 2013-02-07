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
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

static void timelineStartedCallback(ClutterTimeline*, PlatformClutterAnimation* animation)
{
    animation->animationDidStart();
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
    // FIXME: We support only Basic type now.
    ASSERT(type == Basic);
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
    ASSERT(m_type == Basic);

    double duration = clutter_timeline_get_duration(CLUTTER_TIMELINE(m_animation.get()));
    return duration / 1000;
}

void PlatformClutterAnimation::setDuration(double value)
{
    ASSERT(m_type == Basic);

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
    notImplemented();
    return NoValueFunction;
}

void PlatformClutterAnimation::setValueFunction(ValueFunctionType value)
{
    notImplemented();
}

void PlatformClutterAnimation::setFromValue(float value)
{
    if (m_fromValue == value)
        return;
    m_fromValue = value;
}

void PlatformClutterAnimation::setFromValue(const WebCore::TransformationMatrix& value)
{
    notImplemented();
}

void PlatformClutterAnimation::setFromValue(const FloatPoint3D& value)
{
    notImplemented();
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
    if (m_toValue == value)
        return;
    m_toValue = value;
}

void PlatformClutterAnimation::setToValue(const WebCore::TransformationMatrix& value)
{
    notImplemented();
}

void PlatformClutterAnimation::setToValue(const FloatPoint3D& value)
{
    notImplemented();
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
    notImplemented();
}

void PlatformClutterAnimation::setValues(const Vector<WebCore::TransformationMatrix>& value)
{
    notImplemented();
}

void PlatformClutterAnimation::setValues(const Vector<FloatPoint3D>& value)
{
    notImplemented();
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
    notImplemented();
}

void PlatformClutterAnimation::copyKeyTimesFrom(const PlatformClutterAnimation* value)
{
    notImplemented();
}

void PlatformClutterAnimation::setTimingFunctions(const Vector<const TimingFunction*>& value, bool reverse)
{
    notImplemented();
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
    ASSERT(m_type == Basic);
    return CLUTTER_TIMELINE(m_animation.get());
}

void PlatformClutterAnimation::addOpacityTransition()
{
    GRefPtr<ClutterTransition> transition = adoptGRef(clutter_property_transition_new("opacity"));
    clutter_transition_set_from(transition.get(), G_TYPE_UINT, static_cast<guint8>(255 * m_fromValue));
    clutter_transition_set_to(transition.get(), G_TYPE_UINT, static_cast<guint8>(255 * m_toValue));

    clutter_transition_group_add_transition(CLUTTER_TRANSITION_GROUP(m_animation.get()), transition.get());
}

void PlatformClutterAnimation::addAnimationForKey(GraphicsLayerActor* platformLayer, const String& key)
{
    ASSERT(animationType() == Basic);
    ASSERT(!g_object_get_data(G_OBJECT(platformLayer), key.utf8().data()));

    if (m_animatedPropertyType == Opacity)
        addOpacityTransition();
    else if (m_animatedPropertyType == Transform)
        ASSERT_NOT_REACHED();
    else if (m_animatedPropertyType == BackgroundColor)
        ASSERT_NOT_REACHED();
    else
        ASSERT_NOT_REACHED();

    ClutterAnimationMode animationMode = toClutterAnimationMode(m_timingFunction);
    ClutterTimeline* clutterTimeline = timeline();
    clutter_timeline_set_progress_mode(clutterTimeline, animationMode);

    g_signal_connect(clutterTimeline, "started", G_CALLBACK(timelineStartedCallback), this);
    g_object_set_data(G_OBJECT(platformLayer), key.utf8().data(), this);

    m_layer = CLUTTER_ACTOR(platformLayer);

    clutter_actor_add_transition(m_layer.get(), key.utf8().data(), CLUTTER_TRANSITION(m_animation.get()));
}

void PlatformClutterAnimation::removeAnimationForKey(GraphicsLayerActor* layer, const String& key)
{
    ASSERT(animationType() == Basic);

    clutter_actor_remove_transition(CLUTTER_ACTOR(layer), key.utf8().data());
    g_object_set_data(G_OBJECT(layer), key.utf8().data(), 0);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
