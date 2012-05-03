/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#if ENABLE(SVG)
#include "Document.h"
#include "SVGNames.h"
#include "SVGSMILElement.h"
#include "SVGSVGElement.h"
#include <wtf/CurrentTime.h>

using namespace std;

namespace WebCore {
    
static const double animationFrameDelay = 0.025;

SMILTimeContainer::SMILTimeContainer(SVGSVGElement* owner) 
    : m_beginTime(0)
    , m_pauseTime(0)
    , m_accumulatedPauseTime(0)
    , m_presetStartTime(0)
    , m_documentOrderIndexesDirty(false)
    , m_timer(this, &SMILTimeContainer::timerFired)
    , m_ownerSVGElement(owner)
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
    double now = currentTime();

    // If 'm_presetStartTime' is set, the timeline was modified via setElapsed() before the document began.
    // In this case pass on 'seekToTime=true' to updateAnimations().
    m_beginTime = now - m_presetStartTime;
    updateAnimations(SMILTime(m_presetStartTime), m_presetStartTime ? true : false);
    m_presetStartTime = 0;

    if (m_pauseTime) {
        m_pauseTime = now;
        m_timer.stop();
    }
}

void SMILTimeContainer::pause()
{
    ASSERT(!isPaused());
    m_pauseTime = currentTime();

    if (m_beginTime)
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

void SMILTimeContainer::setElapsed(SMILTime time)
{
    // If the documment didn't begin yet, record a new start time, we'll seek to once its possible.
    if (!m_beginTime) {
        m_presetStartTime = time.value();
        return;
    }

    if (m_beginTime)
        m_timer.stop();

    m_beginTime = currentTime() - time.value();
    m_accumulatedPauseTime = 0;

    Vector<SVGSMILElement*> toReset;
    copyToVector(m_scheduledAnimations, toReset);
    for (unsigned n = 0; n < toReset.size(); ++n)
        toReset[n]->reset();

    updateAnimations(time, true);
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
    updateAnimations(elapsed());
}

void SMILTimeContainer::updateDocumentOrderIndexes()
{
    unsigned timingElementCount = 0;
    for (Node* node = m_ownerSVGElement; node; node = node->traverseNextNode(m_ownerSVGElement)) {
        if (SVGSMILElement::isSMILElement(node))
            static_cast<SVGSMILElement*>(node)->setDocumentOrderIndex(timingElementCount++);
    }
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

void SMILTimeContainer::sortByPriority(Vector<SVGSMILElement*>& smilElements, SMILTime elapsed)
{
    if (m_documentOrderIndexesDirty)
        updateDocumentOrderIndexes();
    std::sort(smilElements.begin(), smilElements.end(), PriorityCompare(elapsed));
}
    
static bool applyOrderSortFunction(SVGSMILElement* a, SVGSMILElement* b)
{
    if (!a->hasTagName(SVGNames::animateTransformTag) && b->hasTagName(SVGNames::animateTransformTag))
        return true;
    return false;
}
    
static void sortByApplyOrder(Vector<SVGSMILElement*>& smilElements)
{
    std::sort(smilElements.begin(), smilElements.end(), applyOrderSortFunction);
}

void SMILTimeContainer::updateAnimations(SMILTime elapsed, bool seekToTime)
{
    SMILTime earliersFireTime = SMILTime::unresolved();

    Vector<SVGSMILElement*> toAnimate;
    copyToVector(m_scheduledAnimations, toAnimate);

    // Sort according to priority. Elements with later begin time have higher priority.
    // In case of a tie, document order decides. 
    // FIXME: This should also consider timing relationships between the elements. Dependents
    // have higher priority.
    sortByPriority(toAnimate, elapsed);
    
    // Calculate animation contributions.
    typedef pair<SVGElement*, QualifiedName> ElementAttributePair;
    typedef HashMap<ElementAttributePair, RefPtr<SVGSMILElement> > ResultElementMap;
    ResultElementMap resultsElements;
    for (unsigned n = 0; n < toAnimate.size(); ++n) {
        SVGSMILElement* animation = toAnimate[n];
        ASSERT(animation->timeContainer() == this);

        SVGElement* targetElement = animation->targetElement();
        if (!targetElement)
            continue;
        
        QualifiedName attributeName = animation->attributeName();
        if (attributeName == anyQName()) {
            if (animation->hasTagName(SVGNames::animateMotionTag))
                attributeName = SVGNames::animateMotionTag;
            else
                continue;
        }
        
        // Results are accumulated to the first animation that animates a particular element/attribute pair.
        ElementAttributePair key(targetElement, attributeName); 
        SVGSMILElement* resultElement = resultsElements.get(key).get();
        if (!resultElement) {
            if (!animation->hasValidAttributeType())
                continue;
            resultElement = animation;
            resultElement->resetToBaseValue();
            resultsElements.add(key, resultElement);
        }

        // This will calculate the contribution from the animation and add it to the resultsElement.
        animation->progress(elapsed, resultElement, seekToTime);

        SMILTime nextFireTime = animation->nextProgressTime();
        if (nextFireTime.isFinite())
            earliersFireTime = min(nextFireTime, earliersFireTime);
    }
    
    Vector<SVGSMILElement*> animationsToApply;
    ResultElementMap::iterator end = resultsElements.end();
    for (ResultElementMap::iterator it = resultsElements.begin(); it != end; ++it)
        animationsToApply.append(it->second.get());

    // Sort <animateTranform> to be the last one to be applied. <animate> may change transform attribute as
    // well (directly or indirectly by modifying <use> x/y) and this way transforms combine properly.
    sortByApplyOrder(animationsToApply);
    
    // Apply results to target elements.
    for (unsigned n = 0; n < animationsToApply.size(); ++n)
        animationsToApply[n]->applyResultsToTarget();

    startTimer(earliersFireTime, animationFrameDelay);
    
    Document::updateStyleForAllDocuments();
}

}

#endif // ENABLE(SVG)
