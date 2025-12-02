/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteAnimation.h"

#if ENABLE(THREADED_ANIMATIONS)

#import "RemoteAnimationUtilities.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAnimation);

Ref<RemoteAnimation> RemoteAnimation::create(const WebCore::AcceleratedEffect& effect, const RemoteAnimationTimeline& timeline)
{
    return adoptRef(*new RemoteAnimation(effect, timeline));
}

RemoteAnimation::RemoteAnimation(const WebCore::AcceleratedEffect& effect, const RemoteAnimationTimeline& timeline)
    : m_effect(effect)
    , m_timeline(timeline)
{
}

void RemoteAnimation::apply(WebCore::AcceleratedEffectValues& values)
{
    m_effect->apply(values, m_timeline->currentTime(), m_timeline->duration());
}

Ref<JSON::Object> RemoteAnimation::toJSONForTesting() const
{
    auto convertProperties = [](const OptionSet<WebCore::AcceleratedEffectProperty>& properties) {
        Ref convertedProperties = JSON::Array::create();
        for (auto property : properties)
            convertedProperties->pushString(toStringForTesting(property));
        return convertedProperties;
    };

    auto convertKeyframes = [](const Vector<WebCore::AcceleratedEffect::Keyframe>& keyframes) {
        Ref convertedKeyframes = JSON::Array::create();
        for (auto& keyframe : keyframes) {
            Ref convertedKeyframe = WebKit::toJSONForTesting(keyframe.values(), keyframe.animatedProperties());
            convertedKeyframe->setDouble("offset"_s, keyframe.offset());
            convertedKeyframe->setValue("composite"_s, WebKit::toJSONForTesting(keyframe.compositeOperation()));
            convertedKeyframe->setValue("easing"_s, WebKit::toJSONForTesting(keyframe.timingFunction()));
            convertedKeyframes->pushObject(WTFMove(convertedKeyframe));
        }
        return convertedKeyframes;
    };

    auto resolvedTiming = m_effect->resolvedTimingForTesting(m_timeline->currentTime(), m_timeline->duration());

    Ref object = JSON::Object::create();
    object->setValue("composite"_s, WebKit::toJSONForTesting(m_effect->compositeOperation()));
    object->setBoolean("paused"_s, m_effect->paused());
    object->setDouble("playbackRate"_s, m_effect->playbackRate());
    object->setValue("startTime"_s, WebKit::toJSONForTesting(m_effect->startTime()));
    object->setValue("holdTime"_s, WebKit::toJSONForTesting(m_effect->holdTime()));
    object->setArray("properties"_s, convertProperties(animatedProperties()));
    object->setArray("keyframes"_s, convertKeyframes(keyframes()));
    object->setObject("timing"_s, WebKit::toJSONForTesting(m_effect->timing()));
    object->setObject("timeline"_s, m_timeline->toJSONForTesting());
    if (auto progress = resolvedTiming.transformedProgress)
        object->setDouble("progress"_s, *progress);
    return object;
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
