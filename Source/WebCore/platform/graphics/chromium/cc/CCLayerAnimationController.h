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

#ifndef CCLayerAnimationController_h
#define CCLayerAnimationController_h

#include "cc/CCActiveAnimation.h"
#include "cc/CCAnimationCurve.h"
#include "cc/CCAnimationEvents.h"

#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
namespace WebCore {

class Animation;
class IntSize;
class KeyframeValueList;
class TransformationMatrix;

class CCLayerAnimationControllerClient {
public:
    virtual ~CCLayerAnimationControllerClient() { }

    virtual int id() const = 0;
    virtual void setOpacityFromAnimation(float) = 0;
    virtual float opacity() const = 0;
    virtual void setTransformFromAnimation(const TransformationMatrix&) = 0;
    virtual const TransformationMatrix& transform() const = 0;
    virtual const IntSize& bounds() const = 0;
};

class CCLayerAnimationController {
    WTF_MAKE_NONCOPYABLE(CCLayerAnimationController);
public:
    static PassOwnPtr<CCLayerAnimationController> create(CCLayerAnimationControllerClient*);

    virtual ~CCLayerAnimationController();

    // These methods are virtual for testing.
    virtual bool addAnimation(const KeyframeValueList&, const IntSize& boxSize, const Animation*, int animationId, int groupId, double timeOffset);
    virtual void pauseAnimation(int animationId, double timeOffset);
    virtual void removeAnimation(int animationId);
    virtual void suspendAnimations(double monotonicTime);
    virtual void resumeAnimations(double monotonicTime);

    // Ensures that the list of active animations on the main thread and the impl thread
    // are kept in sync. This function does not take ownership of the impl thread controller.
    virtual void pushAnimationUpdatesTo(CCLayerAnimationController*);

    void animate(double monotonicTime, CCAnimationEventsVector*);

    void add(PassOwnPtr<CCActiveAnimation>);

    // Returns the active animation in the given group, animating the given property if such an
    // animation exists.
    CCActiveAnimation* getActiveAnimation(int groupId, CCActiveAnimation::TargetProperty) const;

    // Returns true if there are any animations that have neither finished nor aborted.
    bool hasActiveAnimation() const;

    // Returns true if there is an animation currently animating the given property, or
    // if there is an animation scheduled to animate this property in the future.
    bool isAnimatingProperty(CCActiveAnimation::TargetProperty) const;

    // This is called in response to an animation being started on the impl thread. This
    // function updates the corresponding main thread animation's start time.
    void notifyAnimationStarted(const CCAnimationEvent&);

    // If a sync is forced, then the next time animation updates are pushed to the impl
    // thread, all animations will be transferred.
    void setForceSync() { m_forceSync = true; }

    void setClient(CCLayerAnimationControllerClient*);

protected:
    explicit CCLayerAnimationController(CCLayerAnimationControllerClient*);

private:
    typedef HashSet<int> TargetProperties;

    void pushNewAnimationsToImplThread(CCLayerAnimationController*) const;
    void removeAnimationsCompletedOnMainThread(CCLayerAnimationController*) const;
    void pushPropertiesToImplThread(CCLayerAnimationController*) const;
    void replaceImplThreadAnimations(CCLayerAnimationController*) const;

    void startAnimationsWaitingForNextTick(double monotonicTime, CCAnimationEventsVector*);
    void startAnimationsWaitingForStartTime(double monotonicTime, CCAnimationEventsVector*);
    void startAnimationsWaitingForTargetAvailability(double monotonicTime, CCAnimationEventsVector*);
    void resolveConflicts(double monotonicTime);
    void purgeFinishedAnimations(double monotonicTime, CCAnimationEventsVector*);

    void tickAnimations(double monotonicTime);

    // If this is true, we force a sync to the impl thread.
    bool m_forceSync;

    CCLayerAnimationControllerClient* m_client;
    Vector<OwnPtr<CCActiveAnimation> > m_activeAnimations;
};

} // namespace WebCore

#endif // CCLayerAnimationController_h
