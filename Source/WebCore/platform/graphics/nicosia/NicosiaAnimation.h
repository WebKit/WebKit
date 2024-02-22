/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#pragma once

#include "Animation.h"
#include "GraphicsLayer.h"

namespace WebCore {
class TransformationMatrix;
}

namespace Nicosia {

class Animation {
public:
    enum class AnimationState { Playing, Paused, Stopped };

    struct ApplicationResult {
        std::optional<WebCore::TransformationMatrix> transform;
        std::optional<double> opacity;
        std::optional<WebCore::FilterOperations> filters;
        bool hasRunningAnimations { false };
    };

    Animation()
        : m_keyframes(WebCore::AnimatedProperty::Invalid)
    { }
    Animation(const String&, const WebCore::KeyframeValueList&, const WebCore::FloatSize&, const WebCore::Animation&, MonotonicTime, Seconds, AnimationState);

    WEBCORE_EXPORT Animation(const Animation&);
    Animation& operator=(const Animation&);
    Animation(Animation&&) = default;
    Animation& operator=(Animation&&) = default;

    enum class KeepInternalState { Yes, No };
    void apply(ApplicationResult&, MonotonicTime, KeepInternalState);

    void pause(Seconds);
    void resume();

    const String& name() const { return m_name; }
    const WebCore::KeyframeValueList& keyframes() const { return m_keyframes; }
    AnimationState state() const { return m_state; }
    WebCore::TimingFunction* timingFunction() const { return m_timingFunction.get(); }

private:
    void applyInternal(ApplicationResult&, const WebCore::AnimationValue& from, const WebCore::AnimationValue& to, float progress);
    Seconds computeTotalRunningTime(MonotonicTime);

    String m_name;
    WebCore::KeyframeValueList m_keyframes;
    WebCore::FloatSize m_boxSize;
    RefPtr<WebCore::TimingFunction> m_timingFunction;
    double m_iterationCount;
    double m_duration;
    WebCore::Animation::Direction m_direction;
    bool m_fillsForwards;
    MonotonicTime m_startTime;
    Seconds m_pauseTime;
    Seconds m_totalRunningTime;
    MonotonicTime m_lastRefreshedTime;
    AnimationState m_state;
};

class Animations {
public:
    Animations() = default;

    void setTranslate(WebCore::TransformationMatrix transform) { m_translate = transform; }
    void setRotate(WebCore::TransformationMatrix transform) { m_rotate = transform; }
    void setScale(WebCore::TransformationMatrix transform) { m_scale = transform; }
    void setTransform(WebCore::TransformationMatrix transform) { m_transform = transform; }

    void add(const Animation&);
    void remove(const String& name);
    void remove(const String& name, WebCore::AnimatedProperty);
    void pause(const String&, Seconds);
    void suspend(MonotonicTime);
    void resume();

    void apply(Animation::ApplicationResult&, MonotonicTime, Animation::KeepInternalState = Animation::KeepInternalState::No);

    bool isEmpty() const { return m_animations.isEmpty(); }
    size_t size() const { return m_animations.size(); }
    const Vector<Animation>& animations() const { return m_animations; }
    Vector<Animation>& animations() { return m_animations; }

    bool hasActiveAnimationsOfType(WebCore::AnimatedProperty type) const;

    bool hasRunningAnimations() const;
    bool hasRunningTransformAnimations() const;

private:
    WebCore::TransformationMatrix m_translate;
    WebCore::TransformationMatrix m_rotate;
    WebCore::TransformationMatrix m_scale;
    WebCore::TransformationMatrix m_transform;

    Vector<Animation> m_animations;
};

}
