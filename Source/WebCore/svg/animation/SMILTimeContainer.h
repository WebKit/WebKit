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

#pragma once

#include "QualifiedName.h"
#include "SMILTime.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    
class SVGElement;
class SVGSMILElement;
class SVGSVGElement;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SMILTimeContainer);
class SMILTimeContainer final : public RefCounted<SMILTimeContainer>  {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SMILTimeContainer);
public:
    static Ref<SMILTimeContainer> create(SVGSVGElement& owner) { return adoptRef(*new SMILTimeContainer(owner)); }

    void schedule(SVGSMILElement*, SVGElement*, const QualifiedName&);
    void unschedule(SVGSMILElement*, SVGElement*, const QualifiedName&);
    void notifyIntervalsChanged();

    WEBCORE_EXPORT Seconds animationFrameDelay() const;

    SMILTime elapsed() const;

    bool isActive() const;
    bool isPaused() const;

    void begin();
    void pause();
    void resume();
    void setElapsed(SMILTime);

    void setDocumentOrderIndexesDirty() { m_documentOrderIndexesDirty = true; }

private:
    SMILTimeContainer(SVGSVGElement& owner);

    bool isStarted() const;
    void timerFired();
    void startTimer(SMILTime elapsed, SMILTime fireTime, SMILTime minimumDelay = 0);
    void updateAnimations(SMILTime elapsed, bool seekToTime = false);

    using ElementAttributePair = std::pair<SVGElement*, QualifiedName>;
    using AnimationsVector = Vector<SVGSMILElement*>;
    using GroupedAnimationsMap = HashMap<ElementAttributePair, AnimationsVector>;

    void processScheduledAnimations(const Function<void(SVGSMILElement&)>&);
    void updateDocumentOrderIndexes();
    void sortByPriority(AnimationsVector& smilElements, SMILTime elapsed);

    MonotonicTime m_beginTime;
    MonotonicTime m_pauseTime;
    Seconds m_accumulatedActiveTime { 0_s };
    MonotonicTime m_resumeTime;
    Seconds m_presetStartTime { 0_s };

    bool m_documentOrderIndexesDirty { false };
    Timer m_timer;
    GroupedAnimationsMap m_scheduledAnimations;
    SVGSVGElement& m_ownerSVGElement;
};

} // namespace WebCore
