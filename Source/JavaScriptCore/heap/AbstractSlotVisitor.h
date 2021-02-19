/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "JSCJSValue.h"
#include "MarkStack.h"
#include "RootMarkReason.h"
#include "VisitRaceKey.h"
#include <wtf/ConcurrentPtrHashSet.h>
#include <wtf/SharedTask.h>
#include <wtf/text/CString.h>

namespace JSC {

class ConservativeRoots;
class Heap;
class HeapCell;
class JSCell;
class MarkedBlock;
class MarkingConstraint;
class MarkingConstraintSolver;
class PreciseAllocation;
class SlotVisitor;
class VM;
class VerifierSlotVisitor;
template<typename T> class Weak;
template<typename T, typename Traits> class WriteBarrierBase;

class AbstractSlotVisitor {
    WTF_MAKE_NONCOPYABLE(AbstractSlotVisitor);
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Context {
    public:
        Context(AbstractSlotVisitor&, HeapCell*);
        ~Context();
        HeapCell* cell() const { return m_cell; }
    private:
        AbstractSlotVisitor& m_visitor;
        HeapCell* m_cell;
        Context* m_previous;
    };

    class SuppressGCVerifierScope {
    public:
        SuppressGCVerifierScope(AbstractSlotVisitor& visitor)
            : m_visitor(visitor)
        {
            ASSERT(!m_visitor.m_suppressVerifier);
            m_visitor.m_suppressVerifier = true;
        }
        ~SuppressGCVerifierScope() { m_visitor.m_suppressVerifier = false; }
    private:
        AbstractSlotVisitor& m_visitor;
    };

    class DefaultMarkingViolationAssertionScope {
    public:
        DefaultMarkingViolationAssertionScope(AbstractSlotVisitor&) { }
    };

    virtual ~AbstractSlotVisitor() = default;

    MarkStackArray& collectorMarkStack() { return m_collectorStack; }
    MarkStackArray& mutatorMarkStack() { return m_mutatorStack; }
    const MarkStackArray& collectorMarkStack() const { return m_collectorStack; }
    const MarkStackArray& mutatorMarkStack() const { return m_mutatorStack; }

    bool isEmpty() { return m_collectorStack.isEmpty() && m_mutatorStack.isEmpty(); }

    VM& vm();
    const VM& vm() const;
    Heap* heap() const;

    virtual void append(const ConservativeRoots&) = 0;

    template<typename T, typename Traits> void append(const WriteBarrierBase<T, Traits>&);
    template<typename T, typename Traits> void appendHidden(const WriteBarrierBase<T, Traits>&);
    template<typename Iterator> void append(Iterator begin , Iterator end);
    ALWAYS_INLINE void appendValues(const WriteBarrierBase<Unknown, RawValueTraits<Unknown>>*, size_t count);
    ALWAYS_INLINE void appendValuesHidden(const WriteBarrierBase<Unknown, RawValueTraits<Unknown>>*, size_t count);

    // These don't require you to prove that you have a WriteBarrier<>. That makes sense
    // for:
    //
    // - roots.
    // - sophisticated data structures that barrier through other means (like DFG::Plan and
    //   friends).
    //
    // If you are not a root and you don't know what kind of barrier you have, then you
    // shouldn't call these methods.
    ALWAYS_INLINE void appendUnbarriered(JSValue);
    ALWAYS_INLINE void appendUnbarriered(JSValue*, size_t);
    virtual void appendUnbarriered(JSCell*) = 0;

    inline bool addOpaqueRoot(void*); // Returns true if the root was new.
    inline bool containsOpaqueRoot(void*) const;
    void setIgnoreNewOpaqueRoots(bool value) { m_ignoreNewOpaqueRoots = value; }

    virtual bool isFirstVisit() const = 0;

    virtual bool isMarked(const void*) const = 0;
    virtual bool isMarked(MarkedBlock&, HeapCell*) const = 0;
    virtual bool isMarked(PreciseAllocation&, HeapCell*) const = 0;

    template<typename T>
    void append(const Weak<T>&);

    ALWAYS_INLINE void appendHiddenUnbarriered(JSValue);
    virtual void appendHiddenUnbarriered(JSCell*) = 0;

    size_t visitCount() const { return m_visitCount; }

    void addToVisitCount(size_t value) { m_visitCount += value; }

    virtual void addParallelConstraintTask(RefPtr<SharedTask<void(AbstractSlotVisitor&)>>) = 0;
    virtual void addParallelConstraintTask(RefPtr<SharedTask<void(SlotVisitor&)>>) = 0;

    virtual void markAuxiliary(const void* base) = 0;

    virtual void reportExtraMemoryVisited(size_t) = 0;
#if ENABLE(RESOURCE_USAGE)
    virtual void reportExternalMemoryVisited(size_t) = 0;
#endif

    virtual void dump(PrintStream&) const = 0;

    RootMarkReason rootMarkReason() const { return m_rootMarkReason; }
    void setRootMarkReason(RootMarkReason reason) { m_rootMarkReason = reason; }

    virtual bool mutatorIsStopped() const = 0;

    virtual void didRace(const VisitRaceKey&) = 0;
    void didRace(JSCell* cell, const char* reason) { didRace(VisitRaceKey(cell, reason)); }

    virtual void visitAsConstraint(const JSCell*) = 0;

    const char* codeName() const { return m_codeName.data(); }

protected:
    inline AbstractSlotVisitor(Heap&, CString codeName, ConcurrentPtrHashSet&);

    HeapCell* parentCell() const;
    void reset();

    MarkStackArray m_collectorStack;
    MarkStackArray m_mutatorStack;

    size_t m_visitCount { 0 };

    Heap& m_heap;
    Context* m_context { nullptr };
    CString m_codeName;

    MarkingConstraint* m_currentConstraint { nullptr };
    MarkingConstraintSolver* m_currentSolver { nullptr };
    ConcurrentPtrHashSet& m_opaqueRoots;

    RootMarkReason m_rootMarkReason { RootMarkReason::None };
    bool m_suppressVerifier { false };
    bool m_ignoreNewOpaqueRoots { false }; // Useful as a debugging mode.

    friend class MarkingConstraintSolver;
};

class SetRootMarkReasonScope {
public:
    SetRootMarkReasonScope(AbstractSlotVisitor& visitor, RootMarkReason reason)
        : m_visitor(visitor)
        , m_previousReason(visitor.rootMarkReason())
    {
        m_visitor.setRootMarkReason(reason);
    }

    ~SetRootMarkReasonScope()
    {
        m_visitor.setRootMarkReason(m_previousReason);
    }

private:
    AbstractSlotVisitor& m_visitor;
    RootMarkReason m_previousReason;
};

} // namespace JSC
