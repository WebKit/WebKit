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

class TextureMapperAnimationBase {
public:
    enum class State : uint8_t { Playing, Paused, Stopped };

    struct ApplicationResult {
        std::optional<TransformationMatrix> transform;
        std::optional<double> opacity;
        std::optional<FilterOperations> filters;
        bool hasRunningAnimations { false };
    };

    enum class KeepInternalState : bool { No, Yes };

    void pause(Seconds);
    void resume();

    const String& name() const { return m_name; }
    State state() const { return m_state; }
    RefPtr<const TimingFunction> timingFunction() const { return m_timingFunction; }

protected:
    TextureMapperAnimationBase() = default;
    TextureMapperAnimationBase(const String&, const Animation&, MonotonicTime, Seconds, State);
    ~TextureMapperAnimationBase() = default;

    WEBCORE_EXPORT TextureMapperAnimationBase(const TextureMapperAnimationBase&);
    TextureMapperAnimationBase& operator=(const TextureMapperAnimationBase&);
    TextureMapperAnimationBase(TextureMapperAnimationBase&&) = default;
    TextureMapperAnimationBase& operator=(TextureMapperAnimationBase&&) = default;

    Seconds computeTotalRunningTime(MonotonicTime);

    RefPtr<const TimingFunction> timingFunctionForAnimationValue(const AnimationValueBase&);

    double applyBase(ApplicationResult&, MonotonicTime, KeepInternalState);
    void applyInternal(ApplicationResult&, const FloatAnimationValue& from, const FloatAnimationValue& to, float progress);
    void applyInternal(ApplicationResult&, const FilterAnimationValue& from, const FilterAnimationValue& to, float progress);
    void applyInternal(ApplicationResult&, const TransformAnimationValue& from, const TransformAnimationValue& to, float progress);

    static KeyframeValueList<FloatAnimationValue> createThreadsafeKeyFrames(const KeyframeValueList<FloatAnimationValue>&);
    static KeyframeValueList<FilterAnimationValue> createThreadsafeKeyFrames(const KeyframeValueList<FilterAnimationValue>&);
    static KeyframeValueList<TransformAnimationValue> createThreadsafeKeyFrames(const KeyframeValueList<TransformAnimationValue>&);

    String m_name;
    RefPtr<const TimingFunction> m_timingFunction;
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

template<typename T> class TextureMapperAnimation : public TextureMapperAnimationBase {
public:
    using Base = TextureMapperAnimationBase;

    using Base::State;
    using Base::ApplicationResult;
    using Base::KeepInternalState;

    TextureMapperAnimation() = default;
    TextureMapperAnimation(const String& name, const KeyframeValueList<T>& keyframes, const Animation& animation, MonotonicTime startTime, Seconds pauseTime, State state)
        : Base(name, animation, startTime, pauseTime, state)
        , m_keyframes(createThreadsafeKeyFrames(keyframes))
    {
    }

    ~TextureMapperAnimation() = default;

    TextureMapperAnimation(const TextureMapperAnimation&) = default;
    TextureMapperAnimation& operator=(const TextureMapperAnimation&) = default;
    TextureMapperAnimation(TextureMapperAnimation&&) = default;
    TextureMapperAnimation& operator=(TextureMapperAnimation&&) = default;

    void apply(ApplicationResult& applicationResults, MonotonicTime time, KeepInternalState keepInternalState)
    {
        auto normalizedValue = Base::applyBase(applicationResults, time, keepInternalState);
        if (!normalizedValue) {
            Base::applyInternal(applicationResults, m_keyframes[0], m_keyframes[1], 0);
            return;
        }

        if (normalizedValue == 1.0) {
            Base::applyInternal(applicationResults, m_keyframes[m_keyframes.size() - 2], m_keyframes[m_keyframes.size() - 1], 1);
            return;
        }
        if (m_keyframes.size() == 2) {
            RefPtr timingFunction = Base::timingFunctionForAnimationValue(m_keyframes[0]);
            normalizedValue = timingFunction->transformProgress(normalizedValue, m_duration);
            Base::applyInternal(applicationResults, m_keyframes[0], m_keyframes[1], normalizedValue);
            return;
        }

        for (size_t i = 0; i < m_keyframes.size() - 1; ++i) {
            const T& from = m_keyframes[i];
            const T& to = m_keyframes[i + 1];
            if (from.keyTime() > normalizedValue || to.keyTime() < normalizedValue)
                continue;

            normalizedValue = (normalizedValue - from.keyTime()) / (to.keyTime() - from.keyTime());
            RefPtr timingFunction = Base::timingFunctionForAnimationValue(from);
            normalizedValue = timingFunction->transformProgress(normalizedValue, m_duration);
            Base::applyInternal(applicationResults, m_keyframes[i], m_keyframes[i + 1], normalizedValue);
            break;
        }
    }

    using Base::pause;
    using Base::resume;
    using Base::name;
    using Base::state;
    using Base::timingFunction;

    const KeyframeValueList<T>& keyframes() const { return m_keyframes; }

private:
    KeyframeValueList<T> m_keyframes { GraphicsLayerAnimationProperty::Invalid };
};

class TextureMapperAnimations {
public:
    TextureMapperAnimations() = default;
    ~TextureMapperAnimations() = default;

    void setTranslate(TransformationMatrix transform) { m_translate = transform; }
    void setRotate(TransformationMatrix transform) { m_rotate = transform; }
    void setScale(TransformationMatrix transform) { m_scale = transform; }
    void setTransform(TransformationMatrix transform) { m_transform = transform; }

    void add(const TextureMapperAnimation<FloatAnimationValue>&);
    void add(const TextureMapperAnimation<FilterAnimationValue>&);
    void add(const TextureMapperAnimation<TransformAnimationValue>&);
    void remove(const String&);
    void remove(const String&, GraphicsLayerAnimationProperty);
    void pause(const String&, Seconds);
    void suspend(MonotonicTime);
    void resume();

    void apply(TextureMapperAnimationBase::ApplicationResult&, MonotonicTime, TextureMapperAnimationBase::KeepInternalState = TextureMapperAnimationBase::KeepInternalState::No);

    bool isEmpty() const { return m_floatAnimations.isEmpty() && m_filterAnimations.isEmpty() && m_transformAnimations.isEmpty(); }
    size_t size() const { return m_floatAnimations.size() + m_filterAnimations.size() + m_transformAnimations.size(); }

    const Vector<TextureMapperAnimation<FloatAnimationValue>>& floatAnimations() const { return m_floatAnimations; }
    Vector<TextureMapperAnimation<FloatAnimationValue>>& floatAnimations() { return m_floatAnimations; }
    const Vector<TextureMapperAnimation<FilterAnimationValue>>& filterAnimations() const { return m_filterAnimations; }
    Vector<TextureMapperAnimation<FilterAnimationValue>>& filterAnimations() { return m_filterAnimations; }
    const Vector<TextureMapperAnimation<TransformAnimationValue>>& transformAnimations() const { return m_transformAnimations; }
    Vector<TextureMapperAnimation<TransformAnimationValue>>& transformAnimations() { return m_transformAnimations; }

    bool hasActiveAnimationsOfType(GraphicsLayerAnimationProperty) const;

    bool hasRunningAnimations() const;
    bool hasRunningTransformAnimations() const;

private:
    TransformationMatrix m_translate;
    TransformationMatrix m_rotate;
    TransformationMatrix m_scale;
    TransformationMatrix m_transform;

    Vector<TextureMapperAnimation<FloatAnimationValue>> m_floatAnimations;
    Vector<TextureMapperAnimation<FilterAnimationValue>> m_filterAnimations;
    Vector<TextureMapperAnimation<TransformAnimationValue>> m_transformAnimations;
};

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
