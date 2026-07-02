/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
#include "DFGMovHintRemovalPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGMayExit.h"
#include "DFGPhase.h"
#include "JSCJSValueInlines.h"
#include "OperandsInlines.h"
#include <wtf/IndexMap.h>

namespace JSC { namespace DFG {

namespace {

namespace DFGMovHintRemovalPhaseInternal {
static constexpr bool verbose = false;
}

class MovHintRemovalPhase : public Phase {
public:
    MovHintRemovalPhase(Graph& graph)
        : Phase(graph, "MovHint removal"_s)
        , m_insertionSet(graph)
        , m_changed(false)
    {
    }

    bool run()
    {
        dataLogIf(DFGMovHintRemovalPhaseInternal::verbose, "Graph before MovHint removal:\n", m_graph);

        // First figure out where various locals are live across the whole
        // graph. This is a backward "bytecode liveness restricted to OSR exit
        // sites" analysis:
        //   Use:  any node which may exit. The locals that are live in
        //         bytecode at the node's exit origin are use-d here. For
        //         exception-only exits we use the matching catch handler's
        //         origin.
        //         Additionally, the bytecode-live set at every block's
        //         terminal is treated as a use even when the terminal
        //         itself doesn't exit. MovHints anchor phantom allocations
        //         to bytecode locals in the OSR availability map; the
        //         availability map is used by FTL's exit emission and
        //         ObjectAllocationSinking, both walk from bytecode-live locals
        //         through the heap closure to pick up heap promotions for sunk
        //         allocations. If we kill a MovHint that anchors a phantom to
        //         a bytecode-live local just because the next FTL-exit-OK node
        //         has it bytecode-dead, the closure can no longer reach the
        //         phantom, and the heap promotion (e.g. StructurePLoc) is
        //         pruned at the use site.
        //   Def:  MovHint kills its destination local.
        IndexMap<BasicBlock*, Operands<bool>> liveAtHead(m_graph.numBlocks());
        IndexMap<BasicBlock*, Operands<bool>> liveAtTail(m_graph.numBlocks());

        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            liveAtHead[block] = Operands<bool>(OperandsLike, block->variablesAtHead, false);
            liveAtTail[block] = Operands<bool>(OperandsLike, block->variablesAtHead, false);
        }

        bool changed;
        do {
            changed = false;
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;

                Operands<bool> live = liveAtTail[block];
                m_graph.forAllLiveInBytecode(
                    block->terminal()->origin.forExit,
                    [&](Operand operand) {
                        live.operand(operand) = true;
                    });
                for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                    Node* node = block->at(nodeIndex);
                    if (node->op() == MovHint)
                        live.operand(node->unlinkedOperand()) = false;
                    defineLiveOperands(node, live);
                }

                if (live == liveAtHead[block])
                    continue;

                liveAtHead[block] = live;
                changed = true;

                for (BasicBlock* predecessor : block->predecessors) {
                    for (size_t i = live.size(); i--;)
                        liveAtTail[predecessor][i] |= live[i];
                }
            }
        } while (changed);

        for (BasicBlock* block : m_graph.blocksInNaturalOrder())
            handleBlock(block, liveAtTail[block]);

        m_insertionSet.execute(m_graph.block(0));

        return m_changed;
    }

private:
    void defineLiveOperands(Node* node, Operands<bool>& live)
    {
        switch (mayExit(m_graph, node)) {
        case DoesNotExit:
            return;

        case Exits: {
            m_graph.forAllLiveInBytecode(
                node->origin.forExit,
                [&](Operand operand) {
                    live.operand(operand) = true;
                });
            return;
        }

        case ExitsForExceptions: {
            // Exception-only exits divert to the matching catch handler;
            // the locals that need to remain available are those the
            // handler will read, not those at the throwing site's exit
            // origin. If there's no handler in this machine frame, the
            // exception unwinds out and no local needs to stay alive on
            // this exit edge.
            CodeOrigin catchOrigin;
            HandlerInfo* handler = nullptr;
            if (m_graph.willCatchExceptionInMachineFrame(node->origin.forExit, catchOrigin, handler)) {
                m_graph.forAllLiveInBytecode(
                    catchOrigin,
                    [&](Operand operand) {
                        live.operand(operand) = true;
                    });
            }
            return;
        }
        }
    }

    void handleBlock(BasicBlock* block, const Operands<bool>& liveAtTail)
    {
        dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "Handing block ", pointerDump(block));

        Operands<bool> live = liveAtTail;
        m_graph.forAllLiveInBytecode(
            block->terminal()->origin.forExit,
            [&](Operand operand) {
                live.operand(operand) = true;
            });

        dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "    Locals at ", block->terminal()->origin.forExit, ": ", live);

        for (unsigned nodeIndex = block->size(); nodeIndex--;) {
            Node* node = block->at(nodeIndex);

            if (node->op() == MovHint) {
                bool isAlive = live.operand(node->unlinkedOperand());
                dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "    At ", node, " (", node->unlinkedOperand(), "): live: ", isAlive);
                if (!isAlive) {
                    // Now, MovHint will put bottom value to dead locals. This means that if you insert a new DFG node which introduce
                    // a new OSR exit, then it gets confused with the already-determined-dead locals. So this phase runs at very end of
                    // DFG pipeline, and we do not insert a node having a new OSR exit (if it is existing OSR exit, or if it does not exit,
                    // then it is totally fine).
                    node->setOpAndDefaultFlags(ZombieHint);
                    UseKind useKind = node->child1().useKind();
                    Node* constant = m_constants.ensure(static_cast<std::underlying_type_t<UseKind>>(useKind), [&]() -> Node* {
                        return m_insertionSet.insertBottomConstantForUse(0, m_graph.block(0)->at(0)->origin, useKind).node();
                    }).iterator->value;
                    node->child1() = Edge(constant, useKind);
                    m_changed = true;
                }
                live.operand(node->unlinkedOperand()) = false;
            }
            defineLiveOperands(node, live);
        }
    }

    InsertionSet m_insertionSet;
    UncheckedKeyHashMap<std::underlying_type_t<UseKind>, Node*, WTF::IntHash<std::underlying_type_t<UseKind>>, WTF::UnsignedWithZeroKeyHashTraits<std::underlying_type_t<UseKind>>> m_constants;
    bool m_changed;
};

} // anonymous namespace

bool performMovHintRemoval(Graph& graph)
{
    return runPhase<MovHintRemovalPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

