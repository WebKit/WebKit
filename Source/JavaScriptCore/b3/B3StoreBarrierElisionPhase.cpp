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

#include "config.h"
#include "B3StoreBarrierElisionPhase.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3StoreBarrierUtils.h"
#include "B3Value.h"
#include <wtf/HashMap.h>

namespace JSC { namespace B3 {

namespace {

// Local epoch type. Kept private to this phase to avoid a dependency on DFGEpoch
// (which is gated on ENABLE(DFG_JIT)). Two values share an epoch iff no may-GC point
// was observed between the points where they were tagged.
class Epoch {
public:
    Epoch() = default;

    static Epoch first() { return Epoch(1); }

    Epoch next() const { return Epoch(m_value + 1); }
    void bump() { *this = next(); }

    bool isPrimordial() const { return !m_value; }

    friend bool operator==(const Epoch&, const Epoch&) = default;

private:
    explicit Epoch(unsigned value)
        : m_value(value)
    {
    }

    unsigned m_value { 0 };
};

class StoreBarrierElision {
public:
    StoreBarrierElision(Procedure& proc)
        : m_proc(proc)
    {
    }

    bool run()
    {
        bool changed = false;
        for (BasicBlock* block : m_proc) {
            m_valueEpoch.clear();
            m_currentEpoch = Epoch::first();
            for (Value* value : *block) {
                if (isStoreBarrier(value)) {
                    Value* cell = value->child(0);
                    auto it = m_valueEpoch.find(cell);
                    if (it != m_valueEpoch.end() && it->value == m_currentEpoch) {
                        value->replaceWithNop();
                        changed = true;
                    }
                    // A barrier itself does not GC: do not bump the epoch.
                    continue;
                }

                // Anything stored or passed by value into the heap may now be visible to
                // the concurrent collector. Conservatively reset its epoch to primordial
                // so future stores through that value still emit a real barrier.
                handleEscapes(value);

                bool isAllocator = isFreshAllocation(value);
                if (mayGC(value))
                    m_currentEpoch.bump();
                if (isAllocator)
                    m_valueEpoch.set(value, m_currentEpoch);
            }
        }
        return changed;
    }

private:
    void handleEscapes(Value* value)
    {
        switch (value->opcode()) {
        case Store:
        case Store8:
        case Store16:
        case AtomicXchgAdd:
        case AtomicXchgAnd:
        case AtomicXchgOr:
        case AtomicXchgSub:
        case AtomicXchgXor:
        case AtomicXchg:
        case AtomicWeakCAS:
        case AtomicStrongCAS:
            // child(0) is the value being stored; remaining children are address parts.
            escape(value->child(0));
            return;
        case CCall:
        case Patchpoint:
            // child(0) is the callee for CCall; everything after is an argument that may
            // be retained by callee. For Patchpoints all children are stackmap entries.
            for (unsigned i = (value->opcode() == CCall ? 1 : 0); i < value->numChildren(); ++i)
                escape(value->child(i));
            return;
        default:
            return;
        }
    }

    void escape(Value* value)
    {
        auto it = m_valueEpoch.find(value);
        if (it != m_valueEpoch.end())
            it->value = Epoch();
    }

    Procedure& m_proc;
    UncheckedKeyHashMap<Value*, Epoch> m_valueEpoch;
    Epoch m_currentEpoch;
};

} // anonymous namespace

bool storeBarrierElision(Procedure& proc)
{
    if (!proc.usesStoreBarriers())
        return false;
    PhaseScope phaseScope(proc, "storeBarrierElision"_s);
    StoreBarrierElision phase(proc);
    return phase.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
