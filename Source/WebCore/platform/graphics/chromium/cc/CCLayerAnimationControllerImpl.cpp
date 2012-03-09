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

#include "cc/CCLayerAnimationControllerImpl.h"

#include "TransformationMatrix.h"
#include "TransformOperations.h"

namespace WebCore {

// A collection of properties. Used when deterimining if animations waiting for target
// availibility are able to run.
typedef HashSet<int> TargetProperties;

PassOwnPtr<CCLayerAnimationControllerImpl> CCLayerAnimationControllerImpl::create(CCLayerAnimationControllerImplClient* client)
{
    return adoptPtr(new CCLayerAnimationControllerImpl(client));
}

CCLayerAnimationControllerImpl::CCLayerAnimationControllerImpl(CCLayerAnimationControllerImplClient* client)
    : m_client(client)
{
}

CCLayerAnimationControllerImpl::~CCLayerAnimationControllerImpl()
{
}

void CCLayerAnimationControllerImpl::animate(double monotonicTime, CCAnimationEventsVector& events)
{
    startAnimationsWaitingForNextTick(monotonicTime, events);
    startAnimationsWaitingForStartTime(monotonicTime, events);
    startAnimationsWaitingForTargetAvailability(monotonicTime, events);
    resolveConflicts(monotonicTime);
    tickAnimations(monotonicTime);
    purgeFinishedAnimations(events);
    startAnimationsWaitingForTargetAvailability(monotonicTime, events);
}

void CCLayerAnimationControllerImpl::add(PassOwnPtr<CCActiveAnimation> anim)
{
    m_activeAnimations.append(anim);
}

CCActiveAnimation* CCLayerAnimationControllerImpl::getActiveAnimation(int groupId, CCActiveAnimation::TargetProperty property)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->group() == groupId && m_activeAnimations[i]->targetProperty() == property)
            return m_activeAnimations[i].get();
    }
    return 0;
}

bool CCLayerAnimationControllerImpl::hasActiveAnimation() const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() != CCActiveAnimation::Finished && m_activeAnimations[i]->runState() != CCActiveAnimation::Aborted)
            return true;
    }
    return false;
}

void CCLayerAnimationControllerImpl::startAnimationsWaitingForNextTick(double monotonicTime, CCAnimationEventsVector& events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::WaitingForNextTick) {
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Running, monotonicTime);
            m_activeAnimations[i]->setStartTime(monotonicTime);
            events.append(CCAnimationStartedEvent::create(m_client->id()));
        }
    }
}

void CCLayerAnimationControllerImpl::startAnimationsWaitingForStartTime(double monotonicTime, CCAnimationEventsVector& events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::WaitingForStartTime && m_activeAnimations[i]->startTime() <= monotonicTime) {
            m_activeAnimations[i]->setRunState(CCActiveAnimation::Running, monotonicTime);
            events.append(CCAnimationStartedEvent::create(m_client->id()));
        }
    }
}

void CCLayerAnimationControllerImpl::startAnimationsWaitingForTargetAvailability(double monotonicTime, CCAnimationEventsVector& events)
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
                if (!blockedProperties.add(*pIter).second)
                    nullIntersection = false;
            }

            // If the intersection is null, then we are free to start the animations in the group.
            if (nullIntersection) {
                m_activeAnimations[i]->setRunState(CCActiveAnimation::Running, monotonicTime);
                m_activeAnimations[i]->setStartTime(monotonicTime);
                events.append(CCAnimationStartedEvent::create(m_client->id()));
                for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                    if (m_activeAnimations[i]->group() == m_activeAnimations[j]->group()) {
                        m_activeAnimations[j]->setRunState(CCActiveAnimation::Running, monotonicTime);
                        m_activeAnimations[j]->setStartTime(monotonicTime);
                    }
                }
            }
        }
    }
}

void CCLayerAnimationControllerImpl::resolveConflicts(double monotonicTime)
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

void CCLayerAnimationControllerImpl::purgeFinishedAnimations(CCAnimationEventsVector& events)
{
    // Each iteration, m_activeAnimations.size() decreases or i increments,
    // guaranteeing progress towards loop termination.
    size_t i = 0;
    while (i < m_activeAnimations.size()) {
        bool allAnimsWithSameIdAreFinished = false;
        if (m_activeAnimations[i]->isFinished()) {
            allAnimsWithSameIdAreFinished = true;
            for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                if (m_activeAnimations[i]->group() == m_activeAnimations[j]->group() && !m_activeAnimations[j]->isFinished()) {
                    allAnimsWithSameIdAreFinished = false;
                    break;
                }
            }
        }
        if (allAnimsWithSameIdAreFinished) {
            events.append(CCAnimationFinishedEvent::create(m_client->id(), m_activeAnimations[i]->id()));
            m_finishedAnimations.append(m_activeAnimations[i]->signature());
            m_activeAnimations.remove(i);
        } else
            i++;
    }
}

void CCLayerAnimationControllerImpl::tickAnimations(double monotonicTime)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == CCActiveAnimation::Running) {
            double trimmed = m_activeAnimations[i]->trimTimeToCurrentIteration(monotonicTime);

            switch (m_activeAnimations[i]->targetProperty()) {

            case CCActiveAnimation::Transform: {
                const CCTransformAnimationCurve* transformAnimationCurve = m_activeAnimations[i]->curve()->toTransformAnimationCurve();
                const TransformationMatrix matrix = transformAnimationCurve->getValue(trimmed, m_client->bounds());
                if (m_activeAnimations[i]->isFinishedAt(monotonicTime))
                    m_activeAnimations[i]->setRunState(CCActiveAnimation::Finished, monotonicTime);

                m_client->setTransform(matrix);
                break;
            }

            case CCActiveAnimation::Opacity: {
                const CCFloatAnimationCurve* floatAnimationCurve = m_activeAnimations[i]->curve()->toFloatAnimationCurve();
                const float opacity = floatAnimationCurve->getValue(trimmed);
                if (m_activeAnimations[i]->isFinishedAt(monotonicTime))
                    m_activeAnimations[i]->setRunState(CCActiveAnimation::Finished, monotonicTime);

                m_client->setOpacity(opacity);
                break;
            }

            } // switch
        } // if running
    } // for each animation
}

} // namespace WebCore
