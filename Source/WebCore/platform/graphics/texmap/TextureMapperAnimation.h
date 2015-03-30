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

#ifndef TextureMapperAnimation_h
#define TextureMapperAnimation_h

#include "GraphicsLayer.h"
#include "TransformationMatrix.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class TextureMapperAnimation {
public:
    enum AnimationState { PlayingState, PausedState, StoppedState };
    class Client {
    public:
        virtual void setAnimatedTransform(const TransformationMatrix&) = 0;
        virtual void setAnimatedOpacity(float) = 0;
        virtual void setAnimatedFilters(const FilterOperations&) = 0;
    };

    TextureMapperAnimation()
        : m_keyframes(AnimatedPropertyInvalid)
    { }
    TextureMapperAnimation(const String&, const KeyframeValueList&, const FloatSize&, const Animation*, double, bool);
    TextureMapperAnimation(const TextureMapperAnimation&);
    void apply(Client*);
    void pause(double);
    void resume();
    double computeTotalRunningTime();
    AnimationState state() const { return m_state; }
    void setState(AnimationState s, double pauseTime = 0)
    {
        m_state = s;
        m_pauseTime = pauseTime;
    }
    AnimatedPropertyID property() const { return m_keyframes.property(); }
    bool isActive() const;
    String name() const { return m_name; }
    FloatSize boxSize() const { return m_boxSize; }
    double startTime() const { return m_startTime; }
    double pauseTime() const { return m_pauseTime; }
    PassRefPtr<Animation> animation() const { return m_animation.get(); }
    const KeyframeValueList& keyframes() const { return m_keyframes; }
    bool listsMatch() const { return m_listsMatch; }

private:
    void applyInternal(Client*, const AnimationValue& from, const AnimationValue& to, float progress);
    KeyframeValueList m_keyframes;
    FloatSize m_boxSize;
    RefPtr<Animation> m_animation;
    String m_name;
    bool m_listsMatch;
    double m_startTime;
    double m_pauseTime;
    double m_totalRunningTime;
    double m_lastRefreshedTime;
    AnimationState m_state;
};

class TextureMapperAnimations {
public:
    TextureMapperAnimations() { }

    void add(const TextureMapperAnimation&);
    void remove(const String& name);
    void remove(const String& name, AnimatedPropertyID);
    void pause(const String&, double);
    void suspend(double);
    void resume();
    void apply(TextureMapperAnimation::Client*);
    bool isEmpty() const { return m_animations.isEmpty(); }
    size_t size() const { return m_animations.size(); }
    const Vector<TextureMapperAnimation>& animations() const { return m_animations; }
    Vector<TextureMapperAnimation>& animations() { return m_animations; }

    bool hasRunningAnimations() const;
    bool hasActiveAnimationsOfType(AnimatedPropertyID type) const;

    TextureMapperAnimations getActiveAnimations() const;

private:
    Vector<TextureMapperAnimation> m_animations;
};

}

#endif // TextureMapperAnimation_h
