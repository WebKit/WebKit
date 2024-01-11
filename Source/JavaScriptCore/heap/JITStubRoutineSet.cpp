/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "HeapInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(JITStubRoutineSet);

JITStubRoutineSet::JITStubRoutineSet() { }
JITStubRoutineSet::~JITStubRoutineSet()
{
    for (auto& entry : m_routines) {
        GCAwareJITStubRoutine* routine = entry.routine;
        routine->m_mayBeExecuting = false;
        
        if (!routine->m_isJettisoned) {
            // Inform the deref() routine that it should delete this stub as soon as the ref count reaches zero.
            routine->m_isJettisoned = true;
            continue;
        }
        
        routine->deleteFromGC();
    }
}

void JITStubRoutineSet::add(GCAwareJITStubRoutine* routine)
{
    RELEASE_ASSERT(!isCompilationThread());
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
                if (!routine.routine->m_isCodeImmutable)
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

void JITStubRoutineSet::deleteUnmarkedJettisonedStubRoutines(VM& vm)
{
    ASSERT(vm.heap.isInPhase(CollectorPhase::End));
    unsigned srcIndex = 0;
    unsigned dstIndex = srcIndex;
    while (srcIndex < m_routines.size()) {
        Routine routine = m_routines[srcIndex++];
        auto* stub = routine.routine;
        if (!stub->m_ownerIsDead && stub->owner())
            stub->m_ownerIsDead = !vm.heap.isMarked(stub->owner());

        // If the stub is running right now, we should keep it alive regardless of whether owner CodeBlock gets dead.
        // It is OK since we already marked all the related cells.
        if (stub->m_mayBeExecuting) {
            m_routines[dstIndex++] = routine;
            continue;
        }

        // If the stub is already jettisoned, and if it is not executed right now, then we can safely destroy this right now
        // since this is not reachable from dead CodeBlock (in CodeBlock's destructor), plus, this will not be executed later.
        if (stub->m_isJettisoned) {
            stub->deleteFromGC();
            continue;
        }

        // If the owner is already dead, then this stub will not be executed. We should remove this from this set.
        // But we should not call deleteFromGC here since unswept CodeBlock may still hold the reference to this stub.
        if (stub->m_ownerIsDead) {
            // Inform the deref() routine that it should delete this stub as soon as the ref count reaches zero.
            stub->m_isJettisoned = true;
            continue;
        }

        m_routines[dstIndex++] = routine;
        continue;
    }
    m_routines.shrinkCapacity(dstIndex);
}

template<typename Visitor>
void JITStubRoutineSet::traceMarkedStubRoutines(Visitor& visitor)
{
    for (auto& entry : m_routines) {
        GCAwareJITStubRoutine* routine = entry.routine;
        if (!routine->m_mayBeExecuting)
            continue;
        
        routine->markRequiredObjects(visitor);
    }
}

template void JITStubRoutineSet::traceMarkedStubRoutines(AbstractSlotVisitor&);
template void JITStubRoutineSet::traceMarkedStubRoutines(SlotVisitor&);

} // namespace JSC

#endif // ENABLE(JIT)

