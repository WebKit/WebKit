/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCAnimationEvents_h
#define CCAnimationEvents_h

#include "cc/CCActiveAnimation.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCAnimationStartedEvent;
class CCFloatAnimationFinishedEvent;
class CCTransformAnimationFinishedEvent;

class CCAnimationEvent {
public:
    enum Type { Started, FinishedFloatAnimation, FinishedTransformAnimation };

    virtual ~CCAnimationEvent();

    virtual Type type() const = 0;

    int layerId() const { return m_layerId; }

    CCActiveAnimation::TargetProperty targetProperty() const { return m_targetProperty; }

    const CCAnimationStartedEvent* toAnimationStartedEvent() const;
    const CCFloatAnimationFinishedEvent* toFloatAnimationFinishedEvent() const;
    const CCTransformAnimationFinishedEvent* toTransformAnimationFinishedEvent() const;

protected:
    CCAnimationEvent(int layerId, CCActiveAnimation::TargetProperty);

private:
    int m_layerId;
    CCActiveAnimation::TargetProperty m_targetProperty;
};

// Indicates that an animation has started on a particular layer.
class CCAnimationStartedEvent : public CCAnimationEvent {
public:
    static PassOwnPtr<CCAnimationStartedEvent> create(int layerId, CCActiveAnimation::TargetProperty);

    virtual ~CCAnimationStartedEvent();

    virtual Type type() const;

private:
    CCAnimationStartedEvent(int layerId, CCActiveAnimation::TargetProperty);
};

// Indicates that a float animation has completed.
class CCFloatAnimationFinishedEvent : public CCAnimationEvent {
public:
    static PassOwnPtr<CCFloatAnimationFinishedEvent> create(int layerId, CCActiveAnimation::TargetProperty, float finalValue);

    virtual ~CCFloatAnimationFinishedEvent();

    virtual Type type() const;

    float finalValue() const { return m_finalValue; }

private:
    CCFloatAnimationFinishedEvent(int layerId, CCActiveAnimation::TargetProperty, float finalValue);

    float m_finalValue;
};

// Indicates that a transform animation has completed.
class CCTransformAnimationFinishedEvent : public CCAnimationEvent {
public:
    static PassOwnPtr<CCTransformAnimationFinishedEvent> create(int layerId, CCActiveAnimation::TargetProperty, const TransformationMatrix& finalValue);

    virtual ~CCTransformAnimationFinishedEvent();

    virtual Type type() const;

    const TransformationMatrix& finalValue() const { return m_finalValue; }

private:
    CCTransformAnimationFinishedEvent(int layerId, CCActiveAnimation::TargetProperty, const TransformationMatrix& finalValue);

    TransformationMatrix m_finalValue;
};

typedef Vector<OwnPtr<CCAnimationEvent> > CCAnimationEventsVector;

} // namespace WebCore

#endif // CCAnimationEvents_h
