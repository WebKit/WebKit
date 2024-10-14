/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#if USE(TEXTURE_MAPPER)
#include "Animation.h"
#include "GraphicsLayer.h"

namespace WebCore {

class TextureMapperAnimation {
public:
    enum class State : uint8_t { Playing, Paused, Stopped };

    struct ApplicationResult {
        std::optional<TransformationMatrix> transform;
        std::optional<double> opacity;
        std::optional<FilterOperations> filters;
        bool hasRunningAnimations { false };
    };

    TextureMapperAnimation() = default;
    TextureMapperAnimation(const String&, const KeyframeValueList&, const FloatSize&, const Animation&, MonotonicTime, Seconds, State);
    ~TextureMapperAnimation() = default;

    WEBCORE_EXPORT TextureMapperAnimation(const TextureMapperAnimation&);
    TextureMapperAnimation& operator=(const TextureMapperAnimation&);
    TextureMapperAnimation(TextureMapperAnimation&&) = default;
    TextureMapperAnimation& operator=(TextureMapperAnimation&&) = default;

    enum class KeepInternalState : bool { Yes, No };
    void apply(ApplicationResult&, MonotonicTime, KeepInternalState);

    void pause(Seconds);
    void resume();

    const String& name() const { return m_name; }
    const KeyframeValueList& keyframes() const { return m_keyframes; }
    State state() const { return m_state; }
    TimingFunction* timingFunction() const { return m_timingFunction.get(); }

private:
    void applyInternal(ApplicationResult&, const AnimationValue& from, const AnimationValue& to, float progress);
    Seconds computeTotalRunningTime(MonotonicTime);

    String m_name;
    KeyframeValueList m_keyframes { AnimatedProperty::Invalid };
    FloatSize m_boxSize;
    RefPtr<TimingFunction> m_timingFunction;
    double m_iterationCount { 0 };
    double m_duration { 0 };
    Animation::Direction m_direction { Animation::Direction::Normal };
    bool m_fillsForwards { false };
    MonotonicTime m_startTime;
    Seconds m_pauseTime;
    Seconds m_totalRunningTime;
    MonotonicTime m_lastRefreshedTime;
    State m_state { State::Stopped };
};

class TextureMapperAnimations {
public:
    TextureMapperAnimations() = default;
    ~TextureMapperAnimations() = default;

    void setTranslate(TransformationMatrix transform) { m_translate = transform; }
    void setRotate(TransformationMatrix transform) { m_rotate = transform; }
    void setScale(TransformationMatrix transform) { m_scale = transform; }
    void setTransform(TransformationMatrix transform) { m_transform = transform; }

    void add(const TextureMapperAnimation&);
    void remove(const String&);
    void remove(const String&, AnimatedProperty);
    void pause(const String&, Seconds);
    void suspend(MonotonicTime);
    void resume();

    void apply(TextureMapperAnimation::ApplicationResult&, MonotonicTime, TextureMapperAnimation::KeepInternalState = TextureMapperAnimation::KeepInternalState::No);

    bool isEmpty() const { return m_animations.isEmpty(); }
    size_t size() const { return m_animations.size(); }
    const Vector<TextureMapperAnimation>& animations() const { return m_animations; }
    Vector<TextureMapperAnimation>& animations() { return m_animations; }

    bool hasActiveAnimationsOfType(AnimatedProperty) const;

    bool hasRunningAnimations() const;
    bool hasRunningTransformAnimations() const;

private:
    TransformationMatrix m_translate;
    TransformationMatrix m_rotate;
    TransformationMatrix m_scale;
    TransformationMatrix m_transform;

    Vector<TextureMapperAnimation> m_animations;
};

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
