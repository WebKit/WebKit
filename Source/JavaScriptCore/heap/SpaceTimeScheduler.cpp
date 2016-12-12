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

#include "config.h"
#include "SpaceTimeScheduler.h"

#include "JSCInlines.h"

namespace JSC {

double SpaceTimeScheduler::Decision::targetMutatorUtilization() const
{
    double bytesSinceBeginningOfCycle =
        m_bytesAllocatedThisCycle - m_scheduler->m_bytesAllocatedThisCycleAtTheBeginning;
    
    double maxHeadroom =
        m_scheduler->m_bytesAllocatedThisCycleAtTheEnd - m_scheduler->m_bytesAllocatedThisCycleAtTheBeginning;
    
    double headroomFullness =
        bytesSinceBeginningOfCycle / maxHeadroom;
    
    // headroomFullness can be NaN and other interesting things if
    // bytesAllocatedThisCycleAtTheBeginning is zero. We see that in debug tests. This code
    // defends against all floating point dragons.
    
    if (!(headroomFullness >= 0))
        headroomFullness = 0;
    if (!(headroomFullness <= 1))
        headroomFullness = 1;
    
    double mutatorUtilization = 1 - headroomFullness;
    
    // Scale the mutator utilization into the permitted window.
    mutatorUtilization =
        Options::minimumMutatorUtilization() +
        mutatorUtilization * (
            Options::maximumMutatorUtilization() -
            Options::minimumMutatorUtilization());
    
    return mutatorUtilization;
}

double SpaceTimeScheduler::Decision::targetCollectorUtilization() const
{
    return 1 - targetMutatorUtilization();
}

Seconds SpaceTimeScheduler::Decision::elapsedInPeriod() const
{
    return (m_now - m_scheduler->m_initialTime) % m_scheduler->m_period;
}

double SpaceTimeScheduler::Decision::phase() const
{
    return elapsedInPeriod() / m_scheduler->m_period;
}

bool SpaceTimeScheduler::Decision::shouldBeResumed() const
{
    if (Options::collectorShouldResumeFirst())
        return phase() <= targetMutatorUtilization();
    return phase() > targetCollectorUtilization();
}

MonotonicTime SpaceTimeScheduler::Decision::timeToResume() const
{
    ASSERT(!shouldBeResumed());
    if (Options::collectorShouldResumeFirst())
        return m_now - elapsedInPeriod() + m_scheduler->m_period;
    return m_now - elapsedInPeriod() + m_scheduler->m_period * targetCollectorUtilization();
}

MonotonicTime SpaceTimeScheduler::Decision::timeToStop() const
{
    ASSERT(shouldBeResumed());
    if (Options::collectorShouldResumeFirst())
        return m_now - elapsedInPeriod() + m_scheduler->m_period * targetMutatorUtilization();
    return m_now - elapsedInPeriod() + m_scheduler->m_period;
}

SpaceTimeScheduler::SpaceTimeScheduler(Heap& heap)
    : m_heap(heap)
    , m_period(Seconds::fromMilliseconds(Options::concurrentGCPeriodMS()))
    , m_bytesAllocatedThisCycleAtTheBeginning(heap.m_bytesAllocatedThisCycle)
    , m_bytesAllocatedThisCycleAtTheEnd(
        Options::concurrentGCMaxHeadroom() *
        std::max<double>(m_bytesAllocatedThisCycleAtTheBeginning, heap.m_maxEdenSize))
{
    snapPhase();
}

void SpaceTimeScheduler::snapPhase()
{
    m_initialTime = MonotonicTime::now();
}

SpaceTimeScheduler::Decision SpaceTimeScheduler::currentDecision()
{
    Decision result;
    result.m_scheduler = this;
    result.m_bytesAllocatedThisCycle = m_heap.m_bytesAllocatedThisCycle;
    result.m_now = MonotonicTime::now();
    return result;
}

} // namespace JSC

