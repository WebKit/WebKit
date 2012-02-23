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

#include "config.h"

#include "cc/CCLayerAnimationController.h"

#include "GraphicsLayer.h" // for KeyframeValueList
#include "cc/CCActiveAnimation.h"
#include "cc/CCLayerAnimationControllerImpl.h"
#include <wtf/HashMap.h>

namespace WebCore {

CCLayerAnimationController::CCLayerAnimationController()
{
}

CCLayerAnimationController::~CCLayerAnimationController()
{
}

PassOwnPtr<CCLayerAnimationController> CCLayerAnimationController::create()
{
    return adoptPtr(new CCLayerAnimationController);
}

bool CCLayerAnimationController::addAnimation(const KeyframeValueList& valueList,
                                              const IntSize&,
                                              const Animation* animation,
                                              int animationId,
                                              int groupId,
                                              double timeOffset)
{
    return false;
}

void CCLayerAnimationController::pauseAnimation(int animationId, double timeOffset)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->id() == animationId)
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Paused, timeOffset);
    }
}

void CCLayerAnimationController::removeAnimation(int animationId)
{
    for (size_t i = 0; i < m_activeAnimations.size();) {
        if (m_activeAnimations[i]->id() == animationId)
            m_activeAnimations.remove(i);
        else
            i++;
    }
}

// According to render layer backing, these are for testing only.
void CCLayerAnimationController::suspendAnimations(double time)
{
}

// Looking at GraphicsLayerCA, this appears to be the analog to suspendAnimations, which is for testing.
void CCLayerAnimationController::resumeAnimations()
{
}

// Ensures that the list of active animations on the main thread and the impl thread
// are kept in sync.
void CCLayerAnimationController::synchronizeAnimations(CCLayerAnimationControllerImpl* controllerImpl)
{
    removeCompletedAnimations(controllerImpl);
    pushNewAnimationsToImplThread(controllerImpl);
    removeAnimationsCompletedOnMainThread(controllerImpl);
    pushAnimationProperties(controllerImpl);
}

void CCLayerAnimationController::removeCompletedAnimations(CCLayerAnimationControllerImpl* controllerImpl)
{
    // Any animations finished on the impl thread are removed from the main thread's collection.
    for (size_t i = 0; i < controllerImpl->m_finishedAnimations.size(); ++i)
        remove(controllerImpl->m_finishedAnimations[i].groupId, controllerImpl->m_finishedAnimations[i].targetProperty);
    controllerImpl->m_finishedAnimations.clear();
}

void CCLayerAnimationController::pushNewAnimationsToImplThread(CCLayerAnimationControllerImpl* controllerImpl)
{
    // Any new animations owned by the main thread's controller are cloned and adde to the impl thread's controller.
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (!controllerImpl->getActiveAnimation(m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty()))
            controllerImpl->add(m_activeAnimations[i]->cloneForImplThread());
    }
}

void CCLayerAnimationController::removeAnimationsCompletedOnMainThread(CCLayerAnimationControllerImpl* controllerImpl)
{
    // Delete all impl thread animations for which there is no corresponding main thread animation.
    // Each iteration, controller->m_activeAnimations.size() is decremented or i is incremented
    // guaranteeing progress towards loop termination.
    for (size_t i = 0; i < controllerImpl->m_activeAnimations.size();) {
        CCActiveAnimation* current = getActiveAnimation(controllerImpl->m_activeAnimations[i]->group(), controllerImpl->m_activeAnimations[i]->targetProperty());
        if (!current)
            controllerImpl->m_activeAnimations.remove(i);
        else
            i++;
    }
}

void CCLayerAnimationController::pushAnimationProperties(CCLayerAnimationControllerImpl* controllerImpl)
{
    // Delete all impl thread animations for which there is no corresponding main thread animation.
    // Each iteration, controller->m_activeAnimations.size() is decremented or i is incremented
    // guaranteeing progress towards loop termination.
    for (size_t i = 0; i < controllerImpl->m_activeAnimations.size(); ++i) {
        CCActiveAnimation* currentImpl = controllerImpl->m_activeAnimations[i].get();
        CCActiveAnimation* current = getActiveAnimation(currentImpl->group(), currentImpl->targetProperty());
        ASSERT(current);
        if (current)
            current->synchronizeProperties(currentImpl);
    }
}

CCActiveAnimation* CCLayerAnimationController::getActiveAnimation(int groupId, CCActiveAnimation::TargetProperty targetProperty)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i)
        if (m_activeAnimations[i]->group() == groupId && m_activeAnimations[i]->targetProperty() == targetProperty)
            return m_activeAnimations[i].get();
    return 0;
}

void CCLayerAnimationController::remove(int groupId, CCActiveAnimation::TargetProperty targetProperty)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->group() == groupId && m_activeAnimations[i]->targetProperty() == targetProperty) {
            m_activeAnimations.remove(i);
            return;
        }
    }
}

} // namespace WebCore
