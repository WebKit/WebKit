/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AnimationBase.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class CompositeAnimation;
class Document;
class Frame;

enum SetChanged { DoNotCallSetChanged, CallSetChanged };

class CSSAnimationControllerPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CSSAnimationControllerPrivate(Frame&);
    ~CSSAnimationControllerPrivate();

    // Returns the time until the next animation needs to be serviced, or -1 if there are none.
    Optional<Seconds> updateAnimations(SetChanged callSetChanged = DoNotCallSetChanged);
    void updateAnimationTimer(SetChanged callSetChanged = DoNotCallSetChanged);

    CompositeAnimation& ensureCompositeAnimation(Element&);
    bool clear(Element&);

    void updateStyleIfNeededDispatcherFired();
    void startUpdateStyleIfNeededDispatcher();
    void addEventToDispatch(Element&, const AtomString& eventType, const String& name, double elapsedTime);
    void addElementChangeToDispatch(Element&);

    bool hasAnimations() const { return !m_compositeAnimations.isEmpty(); }

    bool isSuspended() const { return m_isSuspended; }
    void suspendAnimations();
    void resumeAnimations();
    void animationFrameCallbackFired();

    void updateThrottlingState();
    Seconds animationInterval() const;

    void suspendAnimationsForDocument(Document*);
    void resumeAnimationsForDocument(Document*);
    bool animationsAreSuspendedForDocument(Document*);
    void startAnimationsIfNotSuspended(Document*);
    void detachFromDocument(Document*);

    bool isRunningAnimationOnRenderer(RenderElement&, CSSPropertyID) const;
    bool isRunningAcceleratedAnimationOnRenderer(RenderElement&, CSSPropertyID) const;

    bool pauseAnimationAtTime(Element&, const AtomString& name, double t);
    bool pauseTransitionAtTime(Element&, const String& property, double t);
    unsigned numberOfActiveAnimations(Document*) const;

    std::unique_ptr<RenderStyle> animatedStyleForElement(Element&);

    bool computeExtentOfAnimation(Element&, LayoutRect&) const;

    MonotonicTime beginAnimationUpdateTime();
    
    void beginAnimationUpdate();
    void endAnimationUpdate();
    void receivedStartTimeResponse(MonotonicTime);
    
    void addToAnimationsWaitingForStyle(AnimationBase&);
    void removeFromAnimationsWaitingForStyle(AnimationBase&);

    void addToAnimationsWaitingForStartTimeResponse(AnimationBase&, bool willGetResponse);
    void removeFromAnimationsWaitingForStartTimeResponse(AnimationBase&);

    void animationWillBeRemoved(AnimationBase&);

    void updateAnimationTimerForElement(Element&);

    bool allowsNewAnimationsWhileSuspended() const { return m_allowsNewAnimationsWhileSuspended; }
    void setAllowsNewAnimationsWhileSuspended(bool);

    void setRequiresLayout() { m_requiresLayout = true; }

private:
    void animationTimerFired();

    void styleAvailable();
    void fireEventsAndUpdateStyle();
    void startTimeResponse(MonotonicTime);

    HashMap<RefPtr<Element>, RefPtr<CompositeAnimation>> m_compositeAnimations;
    Timer m_animationTimer;
    Timer m_updateStyleIfNeededDispatcher;
    Frame& m_frame;

    struct EventToDispatch {
        Ref<Element> element;
        AtomString eventType;
        String name;
        double elapsedTime;
    };
    Vector<EventToDispatch> m_eventsToDispatch;
    Vector<Ref<Element>> m_elementChangesToDispatch;
    HashSet<Document*> m_suspendedDocuments;

    Optional<MonotonicTime> m_beginAnimationUpdateTime;

    using AnimationsSet = HashSet<RefPtr<AnimationBase>>;
    AnimationsSet m_animationsWaitingForStyle;
    AnimationsSet m_animationsWaitingForStartTimeResponse;

    int m_beginAnimationUpdateCount;

    bool m_waitingForAsyncStartNotification;
    bool m_isSuspended { false };
    bool m_requiresLayout { false };

    // Used to flag whether we should revert to previous buggy
    // behavior of allowing new transitions and animations to
    // run even when this object is suspended.
    bool m_allowsNewAnimationsWhileSuspended;
};

} // namespace WebCore
