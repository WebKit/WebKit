/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#if ENABLE(SVG)
#include "SMILTimeContainer.h"

#include "Document.h"
#include "SVGSMILElement.h"
#include "SystemTime.h"

using namespace std;

namespace WebCore {
    
static const double animationFrameDelay = 0.025;

SMILTimeContainer::SMILTimeContainer() 
    : m_beginTime(0)
    , m_pauseTime(0)
    , m_accumulatedPauseTime(0)
    , m_timer(this, &SMILTimeContainer::timerFired)
{
}

void SMILTimeContainer::schedule(SVGSMILElement* animation)
{
    ASSERT(animation->timeContainer() == this);
    SMILTime nextFireTime = animation->nextProgressTime();
    if (!nextFireTime.isFinite())
        return;
    m_scheduledAnimations.add(animation);
    startTimer(0);
}
    
void SMILTimeContainer::unschedule(SVGSMILElement* animation)
{
    ASSERT(animation->timeContainer() == this);
    m_scheduledAnimations.remove(animation);
}

SMILTime SMILTimeContainer::elapsed() const
{
    if (!m_beginTime)
        return 0;
    return currentTime() - m_beginTime - m_accumulatedPauseTime;
}
    
bool SMILTimeContainer::isActive() const
{
    return m_beginTime && !isPaused();
}
    
bool SMILTimeContainer::isPaused() const
{
    return m_pauseTime;
}

void SMILTimeContainer::begin()
{
    ASSERT(!m_beginTime);
    m_beginTime = currentTime();
    updateAnimations(0);
}

void SMILTimeContainer::pause()
{
    if (!m_beginTime)
        return;
    ASSERT(!isPaused());
    m_pauseTime = currentTime();
    m_timer.stop();
}

void SMILTimeContainer::resume()
{
    if (!m_beginTime)
        return;
    ASSERT(isPaused());
    m_accumulatedPauseTime += currentTime() - m_pauseTime;
    m_pauseTime = 0;
    startTimer(0);
}

void SMILTimeContainer::startTimer(SMILTime fireTime, SMILTime minimumDelay)
{
    if (!m_beginTime || isPaused())
        return;
    
    if (!fireTime.isFinite())
        return;
    
    SMILTime delay = max(fireTime - elapsed(), minimumDelay);
    m_timer.startOneShot(delay.value());
}
    
void SMILTimeContainer::timerFired(Timer<SMILTimeContainer>*)
{
    ASSERT(m_beginTime);
    ASSERT(!m_pauseTime);
    SMILTime elapsed = this->elapsed();
    updateAnimations(elapsed);
}
    
void SMILTimeContainer::updateAnimations(SMILTime elapsed)
{
    // FIXME: Sort by priority (begin time order, document order) for each target element.
    // Highest priority replace animation overrides the lower priority ones.

    SMILTime earliersFireTime = SMILTime::unresolved();
    
    Vector<SVGSMILElement*> toAnimate;
    copyToVector(m_scheduledAnimations, toAnimate);
    for (unsigned n = 0; n < toAnimate.size(); ++n) {
        // A scheduled animation may have been removed from the list by previous animation.
        if (!m_scheduledAnimations.contains(toAnimate[n]))
            continue;
        
        RefPtr<SVGSMILElement> animation = toAnimate[n];
        ASSERT(animation->timeContainer() == this);

        if (elapsed >= animation->nextProgressTime())
            animation->progress(elapsed);

        // Perhaps it decided not to animate anymore.
        if (!m_scheduledAnimations.contains(animation.get()))
            continue;
    
        SMILTime nextFireTime = animation->nextProgressTime();
        if (nextFireTime.isFinite())
            earliersFireTime = min(nextFireTime, earliersFireTime);
        else
            m_scheduledAnimations.remove(animation.get());
    }
    
    startTimer(earliersFireTime, animationFrameDelay);
    
    Document::updateDocumentsRendering();
}


}
#endif
