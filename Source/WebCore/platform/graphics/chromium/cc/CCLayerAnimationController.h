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

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Animation;
class CCLayerAnimationControllerImpl;
class IntSize;
class KeyframeValueList;

class CCLayerAnimationController {
    WTF_MAKE_NONCOPYABLE(CCLayerAnimationController);
public:
    static PassOwnPtr<CCLayerAnimationController> create();

    virtual ~CCLayerAnimationController();

    // These are virtual for testing.
    virtual bool addAnimation(const KeyframeValueList&, const IntSize& boxSize, const Animation*, int animationId, int groupId, double timeOffset);
    virtual void pauseAnimation(int animationId, double timeOffset);
    virtual void removeAnimation(int animationId);
    virtual void suspendAnimations(double time);
    virtual void resumeAnimations();

    // Ensures that the list of active animations on the main thread and the impl thread
    // are kept in sync. This function does not take ownership of the impl thread controller.
    virtual void synchronizeAnimations(CCLayerAnimationControllerImpl*);

    bool hasActiveAnimation() const { return m_activeAnimations.size(); }
    CCActiveAnimation* getActiveAnimation(int groupId, CCActiveAnimation::TargetProperty);
    bool isAnimatingProperty(CCActiveAnimation::TargetProperty) const;

protected:
    CCLayerAnimationController();

private:
    void removeCompletedAnimations(CCLayerAnimationControllerImpl*);
    void pushNewAnimationsToImplThread(CCLayerAnimationControllerImpl*);
    void removeAnimationsCompletedOnMainThread(CCLayerAnimationControllerImpl*);
    void pushAnimationProperties(CCLayerAnimationControllerImpl*);

    void remove(int groupId, CCActiveAnimation::TargetProperty);

    Vector<OwnPtr<CCActiveAnimation> > m_activeAnimations;
};

} // namespace WebCore

#endif // CCLayerAnimationController_h
