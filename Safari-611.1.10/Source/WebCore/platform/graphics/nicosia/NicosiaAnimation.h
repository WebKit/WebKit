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
        Optional<WebCore::TransformationMatrix> transform;
        Optional<double> opacity;
        Optional<WebCore::FilterOperations> filters;
        bool hasRunningAnimations { false };
    };

    Animation()
        : m_keyframes(WebCore::AnimatedPropertyInvalid)
    { }
    Animation(const String&, const WebCore::KeyframeValueList&, const WebCore::FloatSize&, const WebCore::Animation&, bool, MonotonicTime, Seconds, AnimationState);

    WEBCORE_EXPORT Animation(const Animation&);
    Animation& operator=(const Animation&);
    Animation(Animation&&) = default;
    Animation& operator=(Animation&&) = default;

    void apply(ApplicationResult&, MonotonicTime);
    void applyKeepingInternalState(ApplicationResult&, MonotonicTime);
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
    WebCore::Animation::AnimationDirection m_direction;
    bool m_fillsForwards;
    bool m_listsMatch;
    MonotonicTime m_startTime;
    Seconds m_pauseTime;
    Seconds m_totalRunningTime;
    MonotonicTime m_lastRefreshedTime;
    AnimationState m_state;
};

class Animations {
public:
    Animations() = default;

    void add(const Animation&);
    void remove(const String& name);
    void remove(const String& name, WebCore::AnimatedPropertyID);
    void pause(const String&, Seconds);
    void suspend(MonotonicTime);
    void resume();

    void apply(Animation::ApplicationResult&, MonotonicTime);
    void applyKeepingInternalState(Animation::ApplicationResult&, MonotonicTime);

    bool isEmpty() const { return m_animations.isEmpty(); }
    size_t size() const { return m_animations.size(); }
    const Vector<Animation>& animations() const { return m_animations; }
    Vector<Animation>& animations() { return m_animations; }

    bool hasRunningAnimations() const;
    bool hasActiveAnimationsOfType(WebCore::AnimatedPropertyID type) const;

private:
    Vector<Animation> m_animations;
};

}
