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
#include "B3StoreBarrierClusteringPhase.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3InsertionSetInlines.h"
#include "B3Origin.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3StoreBarrierUtils.h"
#include "B3Value.h"
#include <wtf/BitVector.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

namespace {

class StoreBarrierClustering {
public:
    StoreBarrierClustering(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
    {
    }

    bool run()
    {
        bool changed = false;
        for (BasicBlock* block : m_proc) {
            if (cluster(block))
                changed = true;
        }
        return changed;
    }

private:
    bool cluster(BasicBlock* block)
    {
        // Backward pass: identify "barrier points". A barrier point is the position of the
        // latest barrier before a GC point (or terminal, or back-edge). Earlier barriers in
        // the same no-GC zone fold into the cluster anchored at the barrier point.
        BitVector barrierPoints;
        barrierPoints.ensureSize(block->size());
        bool futureGC = true; // Back-edge could see GC.
        for (size_t i = block->size(); i--; ) {
            Value* value = block->at(i);
            if (value->effects().terminal || mayGC(value)) {
                futureGC = true;
                continue;
            }
            if (isStoreBarrier(value) && futureGC) {
                barrierPoints.set(i);
                futureGC = false;
            }
        }

        // Forward pass: collect barriers and, at each barrier point, emit a deduplicated
        // cluster (FencedStoreBarrier leader, narrow StoreBarrier followers).
        bool changed = false;
        Vector<Pending, 8> needed;
        for (size_t i = 0; i < block->size(); ++i) {
            Value* value = block->at(i);
            if (!isStoreBarrier(value))
                continue;

            needed.append(Pending { value->child(0), value->child(1), value->origin() });
            if (!barrierPoints.quickGet(i)) {
                value->replaceWithNop();
                changed = true;
                continue;
            }

            // We're at the cluster anchor. Replace the anchor barrier itself with Nop too
            // — we'll re-emit the whole cluster via the insertion set below.
            value->replaceWithNop();

            // Stable sort by cell pointer so duplicate-by-cell entries are adjacent. Using
            // pointer identity is sound: B3 SSA guarantees a single Value* per definition.
            std::sort(needed.begin(), needed.end(), [](const Pending& a, const Pending& b) {
                return a.cell < b.cell;
            });
            // Deduplicate adjacent same-cell entries.
            unsigned out = 0;
            for (unsigned in = 0; in < needed.size(); ++in) {
                if (out && needed[out - 1].cell == needed[in].cell)
                    continue;
                if (out != in)
                    needed[out] = needed[in];
                ++out;
            }
            needed.shrink(out);

            for (unsigned k = 0; k < needed.size(); ++k) {
                Opcode opcode = !k ? FencedStoreBarrier : StoreBarrier;
                m_insertionSet.insert<Value>(i, opcode, needed[k].origin, needed[k].cell, needed[k].vm);
            }
            needed.shrinkToFit();
            needed.clear();
            changed = true;
        }
        m_insertionSet.execute(block);
        return changed;
    }

    struct Pending {
        Value* cell;
        Value* vm;
        Origin origin;
    };

    Procedure& m_proc;
    InsertionSet m_insertionSet;
};

} // anonymous namespace

bool storeBarrierClustering(Procedure& proc)
{
    if (!proc.usesStoreBarriers())
        return false;
    PhaseScope phaseScope(proc, "storeBarrierClustering"_s);
    StoreBarrierClustering phase(proc);
    return phase.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
