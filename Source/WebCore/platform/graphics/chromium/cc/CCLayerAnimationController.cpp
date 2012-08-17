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

#include "cc/CCActiveAnimation.h"
#include "cc/CCKeyframedAnimationCurve.h"
#include <public/WebTransformationMatrix.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>

using WebKit::WebTransformationMatrix;

namespace WebCore {

CCLayerAnimationController::CCLayerAnimationController(CCLayerAnimationControllerClient* client)
    : m_forceSync(false)
    , m_client(client)
{
}

CCLayerAnimationController::~CCLayerAnimationController()
{
}

PassOwnPtr<CCLayerAnimationController> CCLayerAnimationController::create(CCLayerAnimationControllerClient* client)
{
    return adoptPtr(new CCLayerAnimationController(client));
}

void CCLayerAnimationController::pauseAnimation(int animationId, double timeOffset)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->id() == animationId)
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Paused, timeOffset + m_activeAnimations[i]->startTime());
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

void CCLayerAnimationController::removeAnimation(int animationId, CCActiveAnimation::TargetProperty targetProperty)
{
    for (size_t i = 0; i < m_activeAnimations.size();) {
        if (m_activeAnimations[i]->id() == animationId && m_activeAnimations[i]->targetProperty() == targetProperty)
            m_activeAnimations.remove(i);
        else
            i++;
    }
}

// According to render layer backing, these are for testing only.
void CCLayerAnimationController::suspendAnimations(double monotonicTime)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (!m_activeAnimations[i]->isFinished())
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Paused, monotonicTime);
    }
}

// Looking at GraphicsLayerCA, this appears to be the analog to suspendAnimations, which is for testing.
void CCLayerAnimationController::resumeAnimations(double monotonicTime)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::Paused)
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Running, monotonicTime);
    }
}

// Ensures that the list of active animations on the main thread and the impl thread
// are kept in sync.
void CCLayerAnimationController::pushAnimationUpdatesTo(CCLayerAnimationController* controllerImpl)
{
    if (m_forceSync) {
        replaceImplThreadAnimations(controllerImpl);
        m_forceSync = false;
    } else {
        purgeAnimationsMarkedForDeletion();
        pushNewAnimationsToImplThread(controllerImpl);

        // Remove finished impl side animations only after pushing,
        // and only after the animations are deleted on the main thread
        // this insures we will never push an animation twice.
        removeAnimationsCompletedOnMainThread(controllerImpl);

        pushPropertiesToImplThread(controllerImpl);
    }
}

void CCLayerAnimationController::animate(double monotonicTime, CCAnimationEventsVector* events)
{
    startAnimationsWaitingForNextTick(monotonicTime, events);
    startAnimationsWaitingForStartTime(monotonicTime, events);
    startAnimationsWaitingForTargetAvailability(monotonicTime, events);
    resolveConflicts(monotonicTime);
    tickAnimations(monotonicTime);
    markAnimationsForDeletion(monotonicTime, events);
    startAnimationsWaitingForTargetAvailability(monotonicTime, events);
}

void CCLayerAnimationController::addAnimation(PassOwnPtr<CCActiveAnimation> animation)
{
    m_activeAnimations.append(animation);
}

CCActiveAnimation* CCLayerAnimationController::getActiveAnimation(int groupId, CCActiveAnimation::TargetProperty targetProperty) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i)
        if (m_activeAnimations[i]->group() == groupId && m_activeAnimations[i]->targetProperty() == targetProperty)
            return m_activeAnimations[i].get();
    return 0;
}

CCActiveAnimation* CCLayerAnimationController::getActiveAnimation(CCActiveAnimation::TargetProperty targetProperty) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        size_t index = m_activeAnimations.size() - i - 1;
        if (m_activeAnimations[index]->targetProperty() == targetProperty)
            return m_activeAnimations[index].get();
    }
    return 0;
}

bool CCLayerAnimationController::hasActiveAnimation() const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (!m_activeAnimations[i]->isFinished())
            return true;
    }
    return false;
}

bool CCLayerAnimationController::isAnimatingProperty(CCActiveAnimation::TargetProperty targetProperty) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() != CCActiveAnimation::Finished && m_activeAnimations[i]->runState() != CCActiveAnimation::Aborted && m_activeAnimations[i]->targetProperty() == targetProperty)
            return true;
    }
    return false;
}

void CCLayerAnimationController::notifyAnimationStarted(const CCAnimationEvent& event)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->group() == event.groupId && m_activeAnimations[i]->targetProperty() == event.targetProperty && m_activeAnimations[i]->needsSynchronizedStartTime()) {
            m_activeAnimations[i]->setNeedsSynchronizedStartTime(false);
            m_activeAnimations[i]->setStartTime(event.monotonicTime);
            return;
        }
    }
}

void CCLayerAnimationController::setClient(CCLayerAnimationControllerClient* client)
{
    m_client = client;
}

void CCLayerAnimationController::pushNewAnimationsToImplThread(CCLayerAnimationController* controllerImpl) const
{
    // Any new animations owned by the main thread's controller are cloned and adde to the impl thread's controller.
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        // If the animation is already running on the impl thread, there is no need to copy it over.
        if (controllerImpl->getActiveAnimation(m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty()))
            continue;

        // If the animation is not running on the impl thread, it does not necessarily mean that it needs
        // to be copied over and started; it may have already finished. In this case, the impl thread animation
        // will have already notified that it has started and the main thread animation will no longer need
        // a synchronized start time.
        if (!m_activeAnimations[i]->needsSynchronizedStartTime())
            continue;

        OwnPtr<CCActiveAnimation> toAdd(m_activeAnimations[i]->cloneForImplThread());
        ASSERT(!toAdd->needsSynchronizedStartTime());
        // The new animation should be set to run as soon as possible.
        toAdd->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 0);
        toAdd->setStartTime(0);
        controllerImpl->addAnimation(toAdd.release());
    }
}

void CCLayerAnimationController::removeAnimationsCompletedOnMainThread(CCLayerAnimationController* controllerImpl) const
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

void CCLayerAnimationController::pushPropertiesToImplThread(CCLayerAnimationController* controllerImpl) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        CCActiveAnimation* currentImpl = controllerImpl->getActiveAnimation(m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty());
        if (currentImpl)
            m_activeAnimations[i]->pushPropertiesTo(currentImpl);
    }
}

void CCLayerAnimationController::startAnimationsWaitingForNextTick(double monotonicTime, CCAnimationEventsVector* events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::WaitingForNextTick) {
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Running, monotonicTime);
            if (!m_activeAnimations[i]->hasSetStartTime())
                m_activeAnimations[i]->setStartTime(monotonicTime);
            if (events)
                events->append(CCAnimationEvent(CCAnimationEvent::Started, m_client->id(), m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty(), monotonicTime));
        }
    }
}

void CCLayerAnimationController::startAnimationsWaitingForStartTime(double monotonicTime, CCAnimationEventsVector* events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::WaitingForStartTime && m_activeAnimations[i]->startTime() <= monotonicTime) {
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Running, monotonicTime);
            if (events)
                events->append(CCAnimationEvent(CCAnimationEvent::Started, m_client->id(), m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty(), monotonicTime));
        }
    }
}

void CCLayerAnimationController::startAnimationsWaitingForTargetAvailability(double monotonicTime, CCAnimationEventsVector* events)
{
    // First collect running properties.
    TargetProperties blockedProperties;
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::Running || m_activeAnimations[i]->runState() == CCActiveAnimation::Finished)
            blockedProperties.add(m_activeAnimations[i]->targetProperty());
    }

    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::WaitingForTargetAvailability) {
            // Collect all properties for animations with the same group id (they should all also be in the list of animations).
            TargetProperties enqueuedProperties;
            enqueuedProperties.add(m_activeAnimations[i]->targetProperty());
            for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                if (m_activeAnimations[i]->group() == m_activeAnimations[j]->group())
                    enqueuedProperties.add(m_activeAnimations[j]->targetProperty());
            }

            // Check to see if intersection of the list of properties affected by the group and the list of currently
            // blocked properties is null. In any case, the group's target properties need to be added to the list
            // of blocked properties.
            bool nullIntersection = true;
            for (TargetProperties::iterator pIter = enqueuedProperties.begin(); pIter != enqueuedProperties.end(); ++pIter) {
                if (!blockedProperties.add(*pIter).isNewEntry)
                    nullIntersection = false;
            }

            // If the intersection is null, then we are free to start the animations in the group.
            if (nullIntersection) {
                m_activeAnimations[i]->setRunState(CCActiveAnimation::Running, monotonicTime);
                if (!m_activeAnimations[i]->hasSetStartTime())
                    m_activeAnimations[i]->setStartTime(monotonicTime);
                if (events)
                    events->append(CCAnimationEvent(CCAnimationEvent::Started, m_client->id(), m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty(), monotonicTime));
                for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                    if (m_activeAnimations[i]->group() == m_activeAnimations[j]->group()) {
                        m_activeAnimations[j]->setRunState(CCActiveAnimation::Running, monotonicTime);
                        if (!m_activeAnimations[j]->hasSetStartTime())
                            m_activeAnimations[j]->setStartTime(monotonicTime);
                    }
                }
            }
        }
    }
}

void CCLayerAnimationController::resolveConflicts(double monotonicTime)
{
    // Find any animations that are animating the same property and resolve the
    // confict. We could eventually blend, but for now we'll just abort the
    // previous animation (where 'previous' means: (1) has a prior start time or
    // (2) has an equal start time, but was added to the queue earlier, i.e.,
    // has a lower index in m_activeAnimations).
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::Running) {
            for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                if (m_activeAnimations[j]->runState() == CCActiveAnimation::Running && m_activeAnimations[i]->targetProperty() == m_activeAnimations[j]->targetProperty()) {
                    if (m_activeAnimations[i]->startTime() > m_activeAnimations[j]->startTime())
                        m_activeAnimations[j]->setRunState(CCActiveAnimation::Aborted, monotonicTime);
                    else
                        m_activeAnimations[i]->setRunState(CCActiveAnimation::Aborted, monotonicTime);
                }
            }
        }
    }
}


void CCLayerAnimationController::markAnimationsForDeletion(double monotonicTime, CCAnimationEventsVector* events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); i++) {
        int groupId = m_activeAnimations[i]->group();
        bool allAnimsWithSameIdAreFinished = false;
        // If an animation is finished, and not already marked for deletion,
        // Find out if all other animations in the same group are also finished.
        if (m_activeAnimations[i]->isFinished()) {
            allAnimsWithSameIdAreFinished = true;
            for (size_t j = 0; j < m_activeAnimations.size(); ++j) {
                if (groupId == m_activeAnimations[j]->group() && !m_activeAnimations[j]->isFinished()) {
                    allAnimsWithSameIdAreFinished = false;
                    break;
                }
            }
        }
        if (allAnimsWithSameIdAreFinished) {
            // We now need to remove all animations with the same group id as groupId
            // (and send along animation finished notifications, if necessary).
            for (size_t j = i; j < m_activeAnimations.size(); j++) {
                if (groupId == m_activeAnimations[j]->group()) {
                    if (events)
                        events->append(CCAnimationEvent(CCAnimationEvent::Finished, m_client->id(), m_activeAnimations[j]->group(), m_activeAnimations[j]->targetProperty(), monotonicTime));
                    m_activeAnimations[j]->setRunState(CCActiveAnimation::WaitingForDeletion, monotonicTime);
                }
            }
        }
    }
}

void CCLayerAnimationController::purgeAnimationsMarkedForDeletion()
{
    for (size_t i = 0; i < m_activeAnimations.size();) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::WaitingForDeletion)
            m_activeAnimations.remove(i);
        else
            i++;
    }
}

void CCLayerAnimationController::replaceImplThreadAnimations(CCLayerAnimationController* controllerImpl) const
{
    controllerImpl->m_activeAnimations.clear();
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        OwnPtr<CCActiveAnimation> toAdd(m_activeAnimations[i]->cloneForImplThread());
        if (m_activeAnimations[i]->needsSynchronizedStartTime()) {
            // We haven't received an animation started notification yet, so it
            // is important that we add it in a 'waiting' and not 'running' state.
            toAdd->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 0);
            toAdd->setStartTime(0);
        }
        controllerImpl->addAnimation(toAdd.release());
    }
}

void CCLayerAnimationController::tickAnimations(double monotonicTime)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::Running || m_activeAnimations[i]->runState() == CCActiveAnimation::Paused) {
            double trimmed = m_activeAnimations[i]->trimTimeToCurrentIteration(monotonicTime);

            // Animation assumes its initial value until it gets the synchronized start time
            // from the impl thread and can start ticking.
            if (m_activeAnimations[i]->needsSynchronizedStartTime())
                trimmed = 0;

            switch (m_activeAnimations[i]->targetProperty()) {

            case CCActiveAnimation::Transform: {
                const CCTransformAnimationCurve* transformAnimationCurve = m_activeAnimations[i]->curve()->toTransformAnimationCurve();
                const WebTransformationMatrix matrix = transformAnimationCurve->getValue(trimmed);
                if (m_activeAnimations[i]->isFinishedAt(monotonicTime))
                    m_activeAnimations[i]->setRunState(CCActiveAnimation::Finished, monotonicTime);

                m_client->setTransformFromAnimation(matrix);
                break;
            }

            case CCActiveAnimation::Opacity: {
                const CCFloatAnimationCurve* floatAnimationCurve = m_activeAnimations[i]->curve()->toFloatAnimationCurve();
                const float opacity = floatAnimationCurve->getValue(trimmed);
                if (m_activeAnimations[i]->isFinishedAt(monotonicTime))
                    m_activeAnimations[i]->setRunState(CCActiveAnimation::Finished, monotonicTime);

                m_client->setOpacityFromAnimation(opacity);
                break;
            }

            }
        }
    }
}

} // namespace WebCore
