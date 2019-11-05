/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SMILTimeContainer.h"

#include "Document.h"
#include "ElementIterator.h"
#include "Page.h"
#include "SVGSMILElement.h"
#include "SVGSVGElement.h"
#include "ScopedEventQueue.h"

namespace WebCore {

static const Seconds SMILAnimationFrameDelay { 1_s / 60. };
static const Seconds SMILAnimationFrameThrottledDelay { 1_s / 30. };

SMILTimeContainer::SMILTimeContainer(SVGSVGElement& owner)
    : m_timer(*this, &SMILTimeContainer::timerFired)
    , m_ownerSVGElement(owner)
{
}

void SMILTimeContainer::schedule(SVGSMILElement* animation, SVGElement* target, const QualifiedName& attributeName)
{
    ASSERT(animation->timeContainer() == this);
    ASSERT(target);
    ASSERT(animation->hasValidAttributeName());

    ElementAttributePair key(target, attributeName);
    std::unique_ptr<AnimationsVector>& scheduled = m_scheduledAnimations.add(key, nullptr).iterator->value;
    if (!scheduled)
        scheduled = makeUnique<AnimationsVector>();
    ASSERT(!scheduled->contains(animation));
    scheduled->append(animation);

    SMILTime nextFireTime = animation->nextProgressTime();
    if (nextFireTime.isFinite())
        notifyIntervalsChanged();
}

void SMILTimeContainer::unschedule(SVGSMILElement* animation, SVGElement* target, const QualifiedName& attributeName)
{
    ASSERT(animation->timeContainer() == this);

    ElementAttributePair key(target, attributeName);
    AnimationsVector* scheduled = m_scheduledAnimations.get(key);
    ASSERT(scheduled);
    bool removed = scheduled->removeFirst(animation);
    ASSERT_UNUSED(removed, removed);
}

void SMILTimeContainer::notifyIntervalsChanged()
{
    // Schedule updateAnimations() to be called asynchronously so multiple intervals
    // can change with updateAnimations() only called once at the end.
    startTimer(elapsed(), 0);
}

Seconds SMILTimeContainer::animationFrameDelay() const
{
    auto* page = m_ownerSVGElement.document().page();
    if (!page)
        return SMILAnimationFrameDelay;
    return page->isLowPowerModeEnabled() ? SMILAnimationFrameThrottledDelay : SMILAnimationFrameDelay;
}

SMILTime SMILTimeContainer::elapsed() const
{
    if (!m_beginTime)
        return 0_s;
    if (isPaused())
        return m_accumulatedActiveTime;
    return MonotonicTime::now() + m_accumulatedActiveTime - m_resumeTime;
}

bool SMILTimeContainer::isActive() const
{
    return !!m_beginTime && !isPaused();
}

bool SMILTimeContainer::isPaused() const
{
    return !!m_pauseTime;
}

bool SMILTimeContainer::isStarted() const
{
    return !!m_beginTime;
}

void SMILTimeContainer::begin()
{
    ASSERT(!m_beginTime);
    MonotonicTime now = MonotonicTime::now();

    // If 'm_presetStartTime' is set, the timeline was modified via setElapsed() before the document began.
    // In this case pass on 'seekToTime=true' to updateAnimations().
    m_beginTime = m_resumeTime = now - m_presetStartTime;
    updateAnimations(SMILTime(m_presetStartTime), m_presetStartTime ? true : false);
    m_presetStartTime = 0_s;

    if (m_pauseTime) {
        m_pauseTime = now;
        m_timer.stop();
    }
}

void SMILTimeContainer::pause()
{
    ASSERT(!isPaused());

    m_pauseTime = MonotonicTime::now();
    if (m_beginTime) {
        m_accumulatedActiveTime += m_pauseTime - m_resumeTime;
        m_timer.stop();
    }
}

void SMILTimeContainer::resume()
{
    ASSERT(isPaused());

    m_resumeTime = MonotonicTime::now();
    m_pauseTime = MonotonicTime();
    startTimer(elapsed(), 0);
}

void SMILTimeContainer::setElapsed(SMILTime time)
{
    // If the documment didn't begin yet, record a new start time, we'll seek to once its possible.
    if (!m_beginTime) {
        m_presetStartTime = Seconds(time.value());
        return;
    }

    if (m_beginTime)
        m_timer.stop();

    MonotonicTime now = MonotonicTime::now();
    m_beginTime = now - Seconds { time.value() };

    if (m_pauseTime) {
        m_resumeTime = m_pauseTime = now;
        m_accumulatedActiveTime = Seconds(time.value());
    } else
        m_resumeTime = m_beginTime;

    processScheduledAnimations([](auto* animation) {
        animation->reset();
    });

    updateAnimations(time, true);
}

void SMILTimeContainer::startTimer(SMILTime elapsed, SMILTime fireTime, SMILTime minimumDelay)
{
    if (!m_beginTime || isPaused())
        return;

    if (!fireTime.isFinite())
        return;

    SMILTime delay = std::max(fireTime - elapsed, minimumDelay);
    m_timer.startOneShot(1_s * delay.value());
}

void SMILTimeContainer::timerFired()
{
    ASSERT(!!m_beginTime);
    ASSERT(!m_pauseTime);
    updateAnimations(elapsed());
}

void SMILTimeContainer::updateDocumentOrderIndexes()
{
    unsigned timingElementCount = 0;

    for (auto& smilElement : descendantsOfType<SVGSMILElement>(m_ownerSVGElement))
        smilElement.setDocumentOrderIndex(timingElementCount++);

    m_documentOrderIndexesDirty = false;
}

struct PriorityCompare {
    PriorityCompare(SMILTime elapsed) : m_elapsed(elapsed) {}
    bool operator()(SVGSMILElement* a, SVGSMILElement* b)
    {
        // FIXME: This should also consider possible timing relations between the elements.
        SMILTime aBegin = a->intervalBegin();
        SMILTime bBegin = b->intervalBegin();
        // Frozen elements need to be prioritized based on their previous interval.
        aBegin = a->isFrozen() && m_elapsed < aBegin ? a->previousIntervalBegin() : aBegin;
        bBegin = b->isFrozen() && m_elapsed < bBegin ? b->previousIntervalBegin() : bBegin;
        if (aBegin == bBegin)
            return a->documentOrderIndex() < b->documentOrderIndex();
        return aBegin < bBegin;
    }
    SMILTime m_elapsed;
};

void SMILTimeContainer::sortByPriority(AnimationsVector& animations, SMILTime elapsed)
{
    if (m_documentOrderIndexesDirty)
        updateDocumentOrderIndexes();
    std::sort(animations.begin(), animations.end(), PriorityCompare(elapsed));
}

void SMILTimeContainer::processAnimations(const AnimationsVector& animations, Function<void(SVGSMILElement*)>&& callback)
{
    // 'animations' may change if 'callback' causes an animation to end which will end up calling
    // unschedule(). Copy 'animations' so none of the items gets deleted out from underneath us.
    auto animationsCopy = animations;
    for (auto* animation : animations)
        callback(animation);
}

void SMILTimeContainer::processScheduledAnimations(Function<void(SVGSMILElement*)>&& callback)
{
    for (auto& it : m_scheduledAnimations)
        processAnimations(*it.value, WTFMove(callback));
}

void SMILTimeContainer::updateAnimations(SMILTime elapsed, bool seekToTime)
{
    // Don't mutate the DOM while updating the animations.
    EventQueueScope scope;

    processScheduledAnimations([](auto* animation) {
        if (!animation->hasConditionsConnected())
            animation->connectConditions();
    });

    AnimationsVector animationsToApply;
    SMILTime earliestFireTime = SMILTime::unresolved();

    for (auto& it : m_scheduledAnimations) {
        // Sort according to priority. Elements with later begin time have higher priority.
        // In case of a tie, document order decides.
        // FIXME: This should also consider timing relationships between the elements. Dependents
        // have higher priority.
        sortByPriority(*it.value, elapsed);

        RefPtr<SVGSMILElement> firstAnimation;
        processAnimations(*it.value, [&](auto* animation) {
            ASSERT(animation->timeContainer() == this);
            ASSERT(animation->targetElement());
            ASSERT(animation->hasValidAttributeName());

            // Results are accumulated to the first animation that animates and contributes to a particular element/attribute pair.
            if (!firstAnimation) {
                if (!animation->hasValidAttributeType())
                    return;
                firstAnimation = animation;
            }

            // This will calculate the contribution from the animation and add it to the resultsElement.
            if (!animation->progress(elapsed, firstAnimation.get(), seekToTime) && firstAnimation == animation)
                firstAnimation = nullptr;

            SMILTime nextFireTime = animation->nextProgressTime();
            if (nextFireTime.isFinite())
                earliestFireTime = std::min(nextFireTime, earliestFireTime);
        });

        if (firstAnimation)
            animationsToApply.append(firstAnimation.get());
    }

    // Apply results to target elements.
    for (auto& animation : animationsToApply)
        animation->applyResultsToTarget();

    startTimer(elapsed, earliestFireTime, animationFrameDelay());
}

}
