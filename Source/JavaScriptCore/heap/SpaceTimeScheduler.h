/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>

namespace JSC {

class Heap;

class SpaceTimeScheduler {
public:
    class Decision {
    public:
        explicit operator bool() const { return !!m_scheduler; }
        
        double targetMutatorUtilization() const;
        double targetCollectorUtilization() const;
        Seconds elapsedInPeriod() const;
        double phase() const;
        bool shouldBeResumed() const;
        MonotonicTime timeToResume() const;
        MonotonicTime timeToStop() const;
        
    private:
        friend class SpaceTimeScheduler;

        SpaceTimeScheduler* m_scheduler { nullptr };
        double m_bytesAllocatedThisCycle { 0 };
        MonotonicTime m_now;
    };
    
    // Construct the scheduler at the start of a GC cycle.
    SpaceTimeScheduler(Heap&);
    
    // Forces the next phase to start now.
    void snapPhase();
    
    // Returns a snapshot of the current scheduling decision, which will be valid so long as
    // SpaceTimeScheduler is live and you haven't called snapPhase().
    Decision currentDecision();
    
private:
    friend class Decision;

    Heap& m_heap;
    Seconds m_period;
    double m_bytesAllocatedThisCycleAtTheBeginning;
    double m_bytesAllocatedThisCycleAtTheEnd;
    MonotonicTime m_initialTime;
};

} // namespace JSC

