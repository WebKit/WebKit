/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AcceleratedEffect.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "AnimationEffect.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "DeclarativeAnimation.h"
#include "Document.h"
#include "KeyframeEffect.h"
#include "KeyframeList.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AcceleratedEffect);

static AcceleratedEffectProperty acceleratedPropertyFromCSSProperty(AnimatableProperty property, const Settings& settings)
{
#if !defined(NDEBUG)
    ASSERT(CSSPropertyAnimation::animationOfPropertyIsAccelerated(property, settings));
#else
    UNUSED_PARAM(settings);
#endif
    ASSERT(std::holds_alternative<CSSPropertyID>(property));

    switch (std::get<CSSPropertyID>(property)) {
    case CSSPropertyOpacity:
        return AcceleratedEffectProperty::Opacity;
    case CSSPropertyTransform:
        return AcceleratedEffectProperty::Transform;
    case CSSPropertyTranslate:
        return AcceleratedEffectProperty::Translate;
    case CSSPropertyRotate:
        return AcceleratedEffectProperty::Rotate;
    case CSSPropertyScale:
        return AcceleratedEffectProperty::Scale;
    case CSSPropertyOffsetPath:
        return AcceleratedEffectProperty::OffsetPath;
    case CSSPropertyOffsetDistance:
        return AcceleratedEffectProperty::OffsetDistance;
    case CSSPropertyOffsetPosition:
        return AcceleratedEffectProperty::OffsetPosition;
    case CSSPropertyOffsetAnchor:
        return AcceleratedEffectProperty::OffsetAnchor;
    case CSSPropertyOffsetRotate:
        return AcceleratedEffectProperty::OffsetRotate;
    case CSSPropertyFilter:
        return AcceleratedEffectProperty::Filter;
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
        return AcceleratedEffectProperty::BackdropFilter;
#endif
    default:
        ASSERT_NOT_REACHED();
        return AcceleratedEffectProperty::Invalid;
    }
}

Ref<AcceleratedEffect> AcceleratedEffect::create(const KeyframeEffect& effect)
{
    return adoptRef(*new AcceleratedEffect(effect));
}

AcceleratedEffect::AcceleratedEffect(const KeyframeEffect& effect)
{
    m_fill = effect.fill();
    m_direction = effect.direction();
    m_compositeOperation = effect.composite();
    m_iterationStart = effect.iterationStart();
    m_iterations = effect.iterations();
    m_delay = effect.delay();
    m_endDelay = effect.endDelay();
    m_iterationDuration = effect.iterationDuration();
    m_animationType = effect.animationType();

    ASSERT(effect.animation());
    if (auto* animation = effect.animation()) {
        m_paused = animation->playState() == WebAnimation::PlayState::Paused;
        m_playbackRate = animation->playbackRate();
        ASSERT(animation->holdTime() || animation->startTime());
        m_holdTime = animation->holdTime();
        m_startTime = animation->startTime();
        if (is<DeclarativeAnimation>(animation)) {
            if (auto* defaultKeyframeTimingFunction = downcast<DeclarativeAnimation>(*animation).backingAnimation().timingFunction())
                m_defaultKeyframeTimingFunction = defaultKeyframeTimingFunction;
        }
    }

    m_timingFunction = effect.timingFunction();

    if (m_iterationDuration && m_iterations)
        m_activeDuration = m_iterationDuration * m_iterations;

    m_endTime = std::max(0_s, m_delay + m_activeDuration + m_endDelay);

    ASSERT(effect.document());
    auto& settings = effect.document()->settings();

    for (auto& srcKeyframe : effect.blendingKeyframes()) {
        OptionSet<AcceleratedEffectProperty> animatedProperties;
        for (auto animatedCSSProperty : srcKeyframe.properties()) {
            if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(animatedCSSProperty, settings)) {
                auto acceleratedProperty = acceleratedPropertyFromCSSProperty(animatedCSSProperty, settings);
                animatedProperties.add(acceleratedProperty);
                m_animatedProperties.add(acceleratedProperty);
            }
        }

        if (animatedProperties.isEmpty())
            continue;

        AcceleratedEffectKeyframe acceleratedKeyframe;
        acceleratedKeyframe.offset = srcKeyframe.key();
        acceleratedKeyframe.compositeOperation = srcKeyframe.compositeOperation();
        acceleratedKeyframe.animatedProperties = WTFMove(animatedProperties);

        acceleratedKeyframe.values = [&]() -> AcceleratedEffectValues {
            if (auto* style = srcKeyframe.style())
                return { *style };
            return { };
        }();

        acceleratedKeyframe.timingFunction = srcKeyframe.timingFunction();

        m_keyframes.append(WTFMove(acceleratedKeyframe));
    }
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
