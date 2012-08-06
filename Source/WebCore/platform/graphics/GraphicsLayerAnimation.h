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

#ifndef GraphicsLayerAnimation_h
#define GraphicsLayerAnimation_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayer.h"
#include "TransformationMatrix.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class GraphicsLayerAnimation {
public:
    enum AnimationState { PlayingState, PausedState, StoppedState };
    class Client {
    public:
        virtual void setAnimatedTransform(const TransformationMatrix&) = 0;
        virtual void setAnimatedOpacity(float) = 0;
    };

    GraphicsLayerAnimation()
        : m_keyframes(AnimatedPropertyInvalid)
    { }
    GraphicsLayerAnimation(const String&, const KeyframeValueList&, const IntSize&, const Animation*, double, bool);
    void apply(Client*);
    void pause(double);
    AnimationState state() const { return m_state; }
    void setState(AnimationState s) { m_state = s; }
    AnimatedPropertyID property() const { return m_keyframes.property(); }
    bool isActive() const;
    String name() const { return m_name; }

private:
    void applyInternal(Client*, const AnimationValue* from, const AnimationValue* to, float progress);
    KeyframeValueList m_keyframes;
    IntSize m_boxSize;
    RefPtr<Animation> m_animation;
    String m_name;
    bool m_listsMatch;
    bool m_hasBigRotation;
    double m_startTime;
    double m_pauseTime;
    AnimationState m_state;
};

class GraphicsLayerAnimations {
public:
    GraphicsLayerAnimations() { }

    void add(const GraphicsLayerAnimation&);
    void remove(const String& name);
    void pause(const String&, double);
    void apply(GraphicsLayerAnimation::Client*);
    bool isEmpty() const { return m_animations.isEmpty(); }

    bool hasRunningAnimations() const;
    bool hasActiveAnimationsOfType(AnimatedPropertyID type) const;

private:
    Vector<GraphicsLayerAnimation> m_animations;
};

}
#endif

#endif // GraphicsLayerAnimation_h
