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

#include "DFGEpoch.h"
#include "DFGForAllKills.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGMayExit.h"
#include "DFGPhase.h"
#include "JSCJSValueInlines.h"
#include "OperandsInlines.h"

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
        , m_state(OperandsLike, graph.block(0)->variablesAtHead)
        , m_changed(false)
    {
    }

    bool run()
    {
        dataLogIf(DFGMovHintRemovalPhaseInternal::verbose, "Graph before MovHint removal:\n", m_graph);

        for (BasicBlock* block : m_graph.blocksInNaturalOrder())
            handleBlock(block);

        m_insertionSet.execute(m_graph.block(0));

        return m_changed;
    }

private:
    void handleBlock(BasicBlock* block)
    {
        dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "Handing block ", pointerDump(block));

        // A MovHint is unnecessary if the local dies before it is used. We answer this question by
        // maintaining the current exit epoch, and associating an epoch with each local. When a
        // local dies, it gets the current exit epoch. If a MovHint occurs in the same epoch as its
        // local, then it means there was no exit between the local's death and the MovHint - i.e.
        // the MovHint is unnecessary.

        Epoch currentEpoch = Epoch::first();

        m_state.fill(Epoch());

        // This is a heuristic. We found cases that BB ends with Jump, and successor no longer have liveness for locals.
        // However, at the end of the current BB, it is alive and we failed to kill that MovHint while we do not have exits.
        // This is a work-around for that case, we chase only one successor and collect a bit more precise liveness information.
        if (block->terminal()->isJump()) {
            auto* successor = block->successor(0);

            m_graph.forAllLiveInBytecode(
                successor->terminal()->origin.forExit,
                [&] (Operand reg) {
                    m_state.operand(reg) = currentEpoch;
                });

            // Assume that blocks after we exit.
            currentEpoch.bump();

            for (unsigned nodeIndex = successor->size(); nodeIndex--;) {
                Node* node = successor->at(nodeIndex);

                if (node->op() == MovHint)
                    m_state.operand(node->unlinkedOperand()) = Epoch();

                if (mayExit(m_graph, node) != DoesNotExit)
                    currentEpoch.bump();

                Node* before = block->terminal();
                if (nodeIndex)
                    before = successor->at(nodeIndex - 1);

                forAllKilledOperands(
                    m_graph, before, node,
                    [&] (Operand operand) {
                        // This function is a bit sloppy - it might claim to kill a local even if
                        // it's still live after. We need to protect against that.
                        if (!!m_state.operand(operand))
                            return;

                        dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "    Killed operand at ", node, ": ", operand);
                        m_state.operand(operand) = currentEpoch;
                    });
            }
        } else {
            m_graph.forAllLiveInBytecode(
                block->terminal()->origin.forExit,
                [&] (Operand reg) {
                    m_state.operand(reg) = currentEpoch;
                });

            // Assume that blocks after we exit.
            currentEpoch.bump();
        }

        dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "    Locals at ", block->terminal()->origin.forExit, ": ", m_state);

        for (unsigned nodeIndex = block->size(); nodeIndex--;) {
            Node* node = block->at(nodeIndex);

            if (node->op() == MovHint) {
                Epoch localEpoch = m_state.operand(node->unlinkedOperand());
                dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "    At ", node, " (", node->unlinkedOperand(), "): current = ", currentEpoch, ", local = ", localEpoch);
                if (!localEpoch || localEpoch == currentEpoch) {
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
                m_state.operand(node->unlinkedOperand()) = Epoch();
            }

            if (mayExit(m_graph, node) != DoesNotExit)
                currentEpoch.bump();

            if (nodeIndex) {
                forAllKilledOperands(
                    m_graph, block->at(nodeIndex - 1), node,
                    [&] (Operand operand) {
                        // This function is a bit sloppy - it might claim to kill a local even if
                        // it's still live after. We need to protect against that.
                        if (!!m_state.operand(operand))
                            return;

                        dataLogLnIf(DFGMovHintRemovalPhaseInternal::verbose, "    Killed operand at ", node, ": ", operand);
                        m_state.operand(operand) = currentEpoch;
                    });
            }
        }
    }

    InsertionSet m_insertionSet;
    UncheckedKeyHashMap<std::underlying_type_t<UseKind>, Node*, WTF::IntHash<std::underlying_type_t<UseKind>>, WTF::UnsignedWithZeroKeyHashTraits<std::underlying_type_t<UseKind>>> m_constants;
    Operands<Epoch> m_state;
    bool m_changed;
};

} // anonymous namespace

bool performMovHintRemoval(Graph& graph)
{
    return runPhase<MovHintRemovalPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

