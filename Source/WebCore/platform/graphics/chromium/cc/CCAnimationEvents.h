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

#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCAnimationStartedEvent;
class CCAnimationFinishedEvent;

class CCAnimationEvent {
public:
    enum Type { Started, Finished };

    virtual ~CCAnimationEvent();

    virtual Type type() const = 0;

    int layerId() const { return m_layerId; }

    const CCAnimationStartedEvent* toAnimationStartedEvent() const;
    const CCAnimationFinishedEvent* toAnimationFinishedEvent() const;

protected:
    CCAnimationEvent(int layerId);

private:
    int m_layerId;
};

// Indicates that an animation has started on a particular layer.
class CCAnimationStartedEvent : public CCAnimationEvent {
public:
    static PassOwnPtr<CCAnimationStartedEvent> create(int layerId);

    virtual ~CCAnimationStartedEvent();

    virtual Type type() const;

private:
    explicit CCAnimationStartedEvent(int layerId);
};

// Indicates that an animation has started on a particular layer.
class CCAnimationFinishedEvent : public CCAnimationEvent {
public:
    static PassOwnPtr<CCAnimationFinishedEvent> create(int layerId, int animationId);

    virtual ~CCAnimationFinishedEvent();

    virtual Type type() const;

    int animationId() const { return m_animationId; }

private:
    CCAnimationFinishedEvent(int layerId, int animationId);

    int m_animationId;
};

typedef Vector<OwnPtr<CCAnimationEvent> > CCAnimationEventsVector;

} // namespace WebCore

#endif // CCAnimationEvents_h
