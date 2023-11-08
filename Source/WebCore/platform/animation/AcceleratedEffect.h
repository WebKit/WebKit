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

#pragma once

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "AcceleratedEffectValues.h"
#include "AnimationEffectTiming.h"
#include "CompositeOperation.h"
#include "TimingFunction.h"
#include "WebAnimationTypes.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>

namespace WebCore {

class IntRect;
class KeyframeEffect;

enum class AcceleratedEffectProperty : uint16_t {
    Invalid = 1 << 0,
    Opacity = 1 << 1,
    Transform = 1 << 2,
    Translate = 1 << 3,
    Rotate = 1 << 4,
    Scale = 1 << 5,
    OffsetPath = 1 << 6,
    OffsetDistance = 1 << 7,
    OffsetPosition = 1 << 8,
    OffsetAnchor = 1 << 9,
    OffsetRotate = 1 << 10,
    Filter = 1 << 11,
#if ENABLE(FILTERS_LEVEL_2)
    BackdropFilter = 1 << 12
#endif
};

struct AcceleratedEffectKeyframe {
    double offset;
    AcceleratedEffectValues values;
    RefPtr<TimingFunction> timingFunction;
    std::optional<CompositeOperation> compositeOperation;
    OptionSet<AcceleratedEffectProperty> animatedProperties;

    AcceleratedEffectKeyframe clone() const;
};

class AcceleratedEffect : public RefCounted<AcceleratedEffect> {
    WTF_MAKE_ISO_ALLOCATED(AcceleratedEffect);
public:
    static Ref<AcceleratedEffect> create(const KeyframeEffect&, const IntRect&);
    WEBCORE_EXPORT static Ref<AcceleratedEffect> create(AnimationEffectTiming, Vector<AcceleratedEffectKeyframe>&&, WebAnimationType, CompositeOperation, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<AcceleratedEffectProperty>&&, bool paused, double playbackRate, std::optional<Seconds> startTime, std::optional<Seconds> holdTime);

    virtual ~AcceleratedEffect() = default;

    WEBCORE_EXPORT Ref<AcceleratedEffect> clone() const;
    WEBCORE_EXPORT Ref<AcceleratedEffect> copyWithProperties(OptionSet<AcceleratedEffectProperty>&) const;

    // Encoding and decoding support
    AnimationEffectTiming timing() const { return m_timing; }
    const Vector<AcceleratedEffectKeyframe>& keyframes() const { return m_keyframes; }
    WebAnimationType animationType() const { return m_animationType; }
    CompositeOperation compositeOperation() const { return m_compositeOperation; }
    const RefPtr<TimingFunction>& defaultKeyframeTimingFunction() const { return m_defaultKeyframeTimingFunction; }
    const OptionSet<AcceleratedEffectProperty>& animatedProperties() const { return m_animatedProperties; }
    bool paused() const { return m_paused; }
    double playbackRate() const { return m_playbackRate; }
    std::optional<Seconds> startTime() const { return m_startTime; }
    std::optional<Seconds> holdTime() const { return m_holdTime; }

    bool animatesTransformRelatedProperty() const;

private:
    AcceleratedEffect(const KeyframeEffect&, const IntRect&);
    explicit AcceleratedEffect(AnimationEffectTiming, Vector<AcceleratedEffectKeyframe>&&, WebAnimationType, CompositeOperation, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<AcceleratedEffectProperty>&&, bool paused, double playbackRate, std::optional<WTF::Seconds> startTime, std::optional<WTF::Seconds> holdTime);
    explicit AcceleratedEffect(const AcceleratedEffect&, OptionSet<AcceleratedEffectProperty>&);

    AnimationEffectTiming m_timing;
    Vector<AcceleratedEffectKeyframe> m_keyframes;
    WebAnimationType m_animationType { WebAnimationType::WebAnimation };
    CompositeOperation m_compositeOperation { CompositeOperation::Replace };
    RefPtr<TimingFunction> m_defaultKeyframeTimingFunction;
    OptionSet<AcceleratedEffectProperty> m_animatedProperties;
    bool m_paused { false };
    double m_playbackRate { 1 };
    std::optional<Seconds> m_startTime;
    std::optional<Seconds> m_holdTime;
};

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
