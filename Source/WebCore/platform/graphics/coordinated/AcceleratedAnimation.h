/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
#include "GraphicsLayer.h"
#include "GraphicsLayerAnimation.h"
#include "GraphicsLayerKeyframeValueList.h"

namespace WebCore {

class GraphicsLayerAnimationValue;

class AcceleratedAnimation : public ThreadSafeRefCounted<AcceleratedAnimation> {
public:
    static Ref<AcceleratedAnimation> create(const String& name, const GraphicsLayerKeyframeValueList& keyframes, const GraphicsLayerAnimation& animation)
    {
        return adoptRef(*new AcceleratedAnimation(name, keyframes, animation));
    }

    enum class State : uint8_t { Playing, Paused, Stopped };
    struct Playback {
        void start(MonotonicTime time)
        {
            startTime = time;
            pauseTime = { };
            state = State::Playing;
        }

        void pause(Seconds offset)
        {
            pauseTime = offset;
            state = State::Paused;
        }

        MonotonicTime startTime;
        Seconds pauseTime;
        State state { State::Stopped };
    };

    struct ApplyResult {
        std::optional<TransformationMatrix> transform;
        std::optional<double> opacity;
        std::optional<FilterOperations> filters;
        bool hasRunningAnimations { false };
    };

    void apply(ApplyResult&, const Playback&, MonotonicTime) const;

    const String& name() const LIFETIME_BOUND { return m_name; }
    const GraphicsLayerKeyframeValueList& keyframes() const LIFETIME_BOUND { return m_keyframes; }
    bool isTransformAnimation() const;

private:
    AcceleratedAnimation(const String&, const GraphicsLayerKeyframeValueList&, const GraphicsLayerAnimation&);

    void applyInternal(ApplyResult&, const GraphicsLayerAnimationValue& from, const GraphicsLayerAnimationValue& to, float progress) const;
    const TimingFunction& timingFunctionForKeyframe(const GraphicsLayerAnimationValue&) const;

    String m_name;
    GraphicsLayerKeyframeValueList m_keyframes;
    Ref<TimingFunction> m_timingFunction;
    RefPtr<TimingFunction> m_defaultTimingFunctionForKeyframes;
    double m_iterationCount { 0 };
    double m_duration { 0 };
    GraphicsLayerAnimation::Direction m_direction { GraphicsLayerAnimation::Direction::Normal };
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
