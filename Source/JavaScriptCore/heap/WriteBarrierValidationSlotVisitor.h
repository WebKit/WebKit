/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ASSERT_ENABLED

#include <JavaScriptCore/AbstractSlotVisitor.h>
#include <wtf/ConcurrentPtrHashSet.h>

namespace JSC {

class Heap;
class JSCell;

// A minimal AbstractSlotVisitor subclass that checks if a specific target cell
// is visited. Used for validating that writeBarrier(from, to) calls actually
// result in 'from' visiting 'to' during GC traversal.
// Validation occurs in the destructor after visitChildren has been called.
class WriteBarrierValidationSlotVisitor final : public AbstractSlotVisitor {
    WTF_MAKE_NONCOPYABLE(WriteBarrierValidationSlotVisitor);
    using Base = AbstractSlotVisitor;

public:
    WriteBarrierValidationSlotVisitor(Heap&, const JSCell* from, JSCell* to);
    ~WriteBarrierValidationSlotVisitor();

    // AbstractSlotVisitor overrides
    void append(const ConservativeRoots&) final { }
    void appendUnbarriered(JSCell*) final;
    void appendHiddenUnbarriered(JSCell*) final;

    bool isFirstVisit() const final { return true; }
    bool isMarked(const void*) const final { return false; }
    bool isMarked(MarkedBlock&, HeapCell*) const final { return false; }
    bool isMarked(PreciseAllocation&, HeapCell*) const final { return false; }

    void markAuxiliary(const void*) final { }

    void reportExtraMemoryVisited(size_t) final { }
#if ENABLE(RESOURCE_USAGE)
    void reportExternalMemoryVisited(size_t) final { }
#endif

    bool mutatorIsStopped() const final { return true; }

    void didRace(const VisitRaceKey&) final { }

    void visitAsConstraint(const JSCell*) final { }

    void addParallelConstraintTask(RefPtr<SharedTask<void(AbstractSlotVisitor&)>>) final { }
    void addParallelConstraintTask(RefPtr<SharedTask<void(SlotVisitor&)>>) final { }

private:
    ConcurrentPtrHashSet m_opaqueRootStorage;
    const JSCell* m_from { nullptr };
    JSCell* m_to { nullptr };
    bool m_foundTarget { false };
};

} // namespace JSC

#endif // ASSERT_ENABLED
