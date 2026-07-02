/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#include "DFGAvailabilityMap.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "JSCJSValueInlines.h"
#include "OperandsInlines.h"
#include <wtf/ListDump.h>

namespace JSC { namespace DFG {

AvailabilityMap AvailabilityMap::filterByLiveness(Graph& graph, CodeOrigin where)
{
    AvailabilityMap filtered;
    filtered.m_locals = Operands<Availability>(OperandsLike, m_locals, Availability::unavailable());
    graph.forAllLiveInBytecode(
        where,
        [&] (Operand reg) {
            filtered.m_locals.operand(reg) = m_locals.operand(reg);
        });

    // filter the heap.
    if (m_heap.isEmpty())
        return filtered;

    NodeSet possibleNodes;

    for (unsigned i = filtered.m_locals.size(); i--;) {
        if (filtered.m_locals[i].hasNode())
            possibleNodes.addVoid(filtered.m_locals[i].node());
    }

    closeOverNodes(
        [&] (Node* node) -> bool {
            return possibleNodes.contains(node);
        },
        [&] (Node* node) -> bool {
            return possibleNodes.add(node).isNewEntry;
        });

    for (auto pair : m_heap) {
        if (possibleNodes.contains(pair.key.base()))
            filtered.m_heap.add(pair.key, pair.value);
    }

    return filtered;
}

void AvailabilityMap::pruneByLiveness(Graph& graph, CodeOrigin where)
{
    *this = filterByLiveness(graph, where);
}

void AvailabilityMap::clear()
{
    m_locals.fill(Availability());
    m_heap.clear();
}

void AvailabilityMap::dump(PrintStream& out) const
{
    out.print("{locals = ", m_locals, "; heap = ", mapDump(m_heap), "}");
}

void AvailabilityMap::merge(const AvailabilityMap& other)
{
    for (unsigned i = other.m_locals.size(); i--;)
        m_locals[i] = other.m_locals[i].merge(m_locals[i]);
    
    for (auto pair : other.m_heap) {
        auto result = m_heap.add(pair.key, Availability());
        result.iterator->value = pair.value.merge(result.iterator->value);
    }
}

void AvailabilityMap::validateAvailability(Graph& graph, Node* where) const
{
    if (where->origin.exitOK) {
        for (auto& heapPair : m_heap) {
            if (heapPair.value.isFlushUseful()) {
                FlushedAt heapFlushLocation = heapPair.value.flushedAt();
                // We should always have an SSA value to use for materializations in the heap. The only exception is
                // Arguments allocations because those could have values propagated via Load/ForwardVarargs, which
                // don't have a reasonable way to SSA track individual entries.
                if (!heapPair.key.base()->isPhantomArgumentsAllocation())
                    DFG_ASSERT(graph, where, heapPair.value.hasNode());

                // If we don't have a location on the stack yet then there's not much to validate.
                if (!heapFlushLocation.virtualRegister().isValid()) {
                    DFG_ASSERT(graph, where, graph.m_planStage < PlanStage::AfterStackLayout);
                    continue;
                }

                for (unsigned i = 0; i < m_locals.size(); ++i) {
                    // Look for our flush in the locals, it should be there and match our heap location.
                    Availability localAvailability = m_locals[i];

                    if (localAvailability.flushedAt().virtualRegister() == heapFlushLocation.virtualRegister()) {
                        if (localAvailability.hasNode() && heapPair.value.node() != localAvailability.node())
                            DFG_CRASH(graph, where, toCString("Materialization flushed availability ", heapPair.value, " and local flushed availability ", localAvailability, " disagree on which DFG node the same stack slot holds in ", *this).data());

                        if (heapFlushLocation.format() != localAvailability.flushedAt().format())
                            DFG_CRASH(graph, where, toCString("Materialization should be flushed (", heapPair.value, ") but corresponding local doesn't exist or match (", localAvailability, ") in ", *this).data());
                        break;
                    }
                }
            }
        }
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

