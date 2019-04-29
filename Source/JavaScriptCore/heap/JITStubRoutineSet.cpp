/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "JITStubRoutineSet.h"

#if ENABLE(JIT)

#include "GCAwareJITStubRoutine.h"
#include "JSCInlines.h"
#include "SlotVisitor.h"

namespace JSC {

JITStubRoutineSet::JITStubRoutineSet() { }
JITStubRoutineSet::~JITStubRoutineSet()
{
    for (auto& entry : m_routines) {
        GCAwareJITStubRoutine* routine = entry.routine;
        routine->m_mayBeExecuting = false;
        
        if (!routine->m_isJettisoned) {
            // Inform the deref() routine that it should delete this guy as soon
            // as the ref count reaches zero.
            routine->m_isJettisoned = true;
            continue;
        }
        
        routine->deleteFromGC();
    }
}

void JITStubRoutineSet::add(GCAwareJITStubRoutine* routine)
{
    ASSERT(!routine->m_isJettisoned);
    
    m_routines.append(Routine {
        routine->startAddress(),
        routine
    });
}

void JITStubRoutineSet::prepareForConservativeScan()
{
    if (m_routines.isEmpty()) {
        m_range = Range<uintptr_t> { 0, 0 };
        return;
    }
    std::sort(
        m_routines.begin(), m_routines.end(),
        [&] (const Routine& a, const Routine& b) {
            return a.startAddress < b.startAddress;
        });
    m_range = Range<uintptr_t> {
        m_routines.first().startAddress,
        m_routines.last().routine->endAddress()
    };
}

void JITStubRoutineSet::clearMarks()
{
    for (auto& entry : m_routines)
        entry.routine->m_mayBeExecuting = false;
}

void JITStubRoutineSet::markSlow(uintptr_t address)
{
    ASSERT(isJITPC(bitwise_cast<void*>(address)));
    ASSERT(!m_routines.isEmpty());

    Routine* result = approximateBinarySearch<Routine>(
        m_routines.begin(), m_routines.size(), address,
        [] (const Routine* routine) -> uintptr_t { return routine->startAddress; });
    if (result) {
        auto markIfContained = [&] (const Routine& routine, uintptr_t address) {
            if (routine.startAddress <= address && address < routine.routine->endAddress()) {
                routine.routine->m_mayBeExecuting = true;
                return true;
            }
            return false;
        };

        if (result > m_routines.begin()) {
            if (markIfContained(result[-1], address))
                return;
        }
        if (markIfContained(result[0], address))
            return;
        if (result + 1 < m_routines.end()) {
            if (markIfContained(result[1], address))
                return;
        }
    }
}

void JITStubRoutineSet::deleteUnmarkedJettisonedStubRoutines()
{
    unsigned srcIndex = 0;
    unsigned dstIndex = srcIndex;
    while (srcIndex < m_routines.size()) {
        Routine routine = m_routines[srcIndex++];
        if (!routine.routine->m_isJettisoned || routine.routine->m_mayBeExecuting) {
            m_routines[dstIndex++] = routine;
            continue;
        }
        routine.routine->deleteFromGC();
    }
    m_routines.shrink(dstIndex);
}

void JITStubRoutineSet::traceMarkedStubRoutines(SlotVisitor& visitor)
{
    for (auto& entry : m_routines) {
        GCAwareJITStubRoutine* routine = entry.routine;
        if (!routine->m_mayBeExecuting)
            continue;
        
        routine->markRequiredObjects(visitor);
    }
}

} // namespace JSC

#endif // ENABLE(JIT)

