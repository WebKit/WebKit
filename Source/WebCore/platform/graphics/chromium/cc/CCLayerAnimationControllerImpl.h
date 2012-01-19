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

#ifndef CCLayerAnimationControllerImpl_h
#define CCLayerAnimationControllerImpl_h

#include "cc/CCActiveAnimation.h"
#include "cc/CCAnimationCurve.h"

#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCLayerAnimationControllerImpl;
class IntSize;
class TransformationMatrix;
class TransformOperations;

class CCLayerAnimationControllerImplClient {
public:
    virtual ~CCLayerAnimationControllerImplClient() { }

    virtual float opacity() const = 0;
    virtual void setOpacity(float) = 0;
    virtual const TransformationMatrix& transform() const = 0;
    virtual void setTransform(const TransformationMatrix&) = 0;
    virtual void animationControllerImplDidActivate(CCLayerAnimationControllerImpl*) = 0;
};

class CCLayerAnimationControllerImpl {
public:
    static PassOwnPtr<CCLayerAnimationControllerImpl> create(CCLayerAnimationControllerImplClient*);

    void animate(double frameBeginTimeSecs);

    void add(PassOwnPtr<CCActiveAnimation>);

    // Returns the active animation in the given group, animating the given property if such an
    // animation exists.
    CCActiveAnimation* getActiveAnimation(CCActiveAnimation::GroupID, CCActiveAnimation::TargetProperty);

    // Returns true if there are any animations that are neither finished nor aborted.
    bool hasActiveAnimation() const;

private:
    // The animator is owned by the layer.
    explicit CCLayerAnimationControllerImpl(CCLayerAnimationControllerImplClient*);

    void startAnimationsWaitingForNextTick(double now);
    void startAnimationsWaitingForStartTime(double now);
    void startAnimationsWaitingForTargetAvailability(double now);
    void resolveConflicts(double now);
    void purgeFinishedAnimations();

    void tickAnimations(double now);

    CCLayerAnimationControllerImplClient* m_client;
    Vector<OwnPtr<CCActiveAnimation> > m_activeAnimations;
};

} // namespace WebCore

#endif // CCLayerAnimationControllerImpl_h

