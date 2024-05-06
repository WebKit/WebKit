/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "DFGLoopUnrolling.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractInterpreterInlines.h"
#include "DFGAtTailAbstractState.h"
#include "DFGBlockInsertionSet.h"
#include "DFGClobberSet.h"
#include "DFGClobberize.h"
#include "DFGControlEquivalenceAnalysis.h"
#include "DFGEdgeDominates.h"
#include "DFGGraph.h"
#include "DFGMayExit.h"
#include "DFGNaturalLoops.h"
#include "DFGPhase.h"
#include "DFGSafeToExecute.h"
#include "JSCInlines.h"
#include <queue>

namespace JSC { namespace DFG {

class UnrollingPhase : public Phase {
    static constexpr bool verbose = true;

    using NaturalLoop = CPSNaturalLoop;

    // loop-unrolling-constant-small.js
    struct LoopUpdate_IPlusC_LT_D {
        UnrollingPhase* phase;
        Node* i { nullptr };
        int32_t c { 0 };
        Node* d { nullptr };
        Node* update { nullptr };

        static std::optional<LoopUpdate_IPlusC_LT_D> make(UnrollingPhase* phase, Node* condition)
        {
            if (condition->op() == CompareLess
                && condition->child1()->op() == ArithAdd
                && condition->child1()->child2()->isInt32Constant()
                && condition->child1()->child1()->op() == GetLocal
                // TODO is safe to hoist i
                // FIXME TODO OOPS: this should really be some kind of isHoistable check; check if live at preheader, check if updated in loop
                && (condition->child2()->op() == JSConstant || condition->child2()->op() == GetLocal)) {
                return {
                    LoopUpdate_IPlusC_LT_D {
                        .phase = phase,
                        .i = condition->child1()->child1().node(),
                        .c = condition->child1()->child2().node()->asInt32(),
                        .d = condition->child2().node(),
                        .update = condition->child1().node(),
                    }
                };
            }

            return { };
        }

        Node* insertUnrollPreCondition(HashMap<Node*, Node*>& cloneMap, NodeOrigin origin, BasicBlock* into) const
        {
            auto& graph = phase->m_graph;
            auto* hoistedI = phase->cloneNode(cloneMap, into, i);
            auto* hoistedD = phase->cloneNode(cloneMap, into, d);

            for (auto* v : cloneMap.values())
                v->origin = origin;

            auto* condition = into->appendNode(graph, SpecBoolean, CompareLess, origin,
                Edge(into->appendNode(graph, SpecNone, ArithAdd, origin, OpInfo(Arith::Unchecked),
                    Edge(hoistedI, Int32Use),
                    Edge(into->appendNode(graph, SpecNone, JSConstant, origin, OpInfo(graph.freeze(jsNumber(((int)Options::loopFactorFTLLoopUnrolling()) * c)))), Int32Use) // TODO overflow
                ), Int32Use),
                Edge(hoistedD, Int32Use)
            );

            // TODO: exits?
            for (auto* edge : {
                &condition->child1().node()->child1(),
                &condition->child1().node()->child2(),
                &condition->child1(),
                &condition->child2()
            }) {
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            return condition;
        }

        Node* insertUnrollPostCondition(HashMap<Node*, Node*>& cloneMap, NodeOrigin origin, BasicBlock* into, int loopFactor) const
        {
            auto& graph = phase->m_graph;
            auto* hoistedI = phase->cloneNode(cloneMap, into, i);
            auto* hoistedD = phase->cloneNode(cloneMap, into, d);

            for (auto* v : cloneMap.values())
                v->origin = origin;

            auto* condition = into->appendNode(graph, SpecBoolean, CompareLess, origin,
                Edge(into->appendNode(graph, SpecNone, ArithAdd, origin, OpInfo(Arith::Unchecked),
                    Edge(hoistedI, Int32Use),
                    Edge(into->appendNode(graph, SpecNone, JSConstant, origin, OpInfo(graph.freeze(jsNumber((loopFactor) * c)))), Int32Use) // TODO overflow
                ), Int32Use),
                Edge(hoistedD, Int32Use)
            );

            // TODO: exits?
            for (auto* edge : {
                &condition->child1().node()->child1(),
                &condition->child1().node()->child2(),
                &condition->child1(),
                &condition->child2()
            }) {
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            return condition;
        }

        Node* rewriteVariableAccess(BasicBlock* into, Node* access, int iteration) const
        {
            auto& graph = phase->m_graph;

            dataLogLnIf(verbose, "Rewrite acccess: D@", access->index(), " iteration ", iteration, " ", iteration * c);
            RELEASE_ASSERT(access->op() == GetLocal || access->op() == SetLocal);

            auto* updated = into->appendNode(graph, SpecNone, ArithAdd, access->origin, OpInfo(Arith::Unchecked),
                Edge(into->appendNode(graph, SpecNone, GetLocal, access->origin, OpInfo(inductionVariable()->variableAccessData())), Int32Use),
                Edge(into->appendNode(graph, SpecNone, JSConstant, access->origin, OpInfo(graph.freeze(jsNumber((iteration * c))))), Int32Use) // TODO overflow
            );

            // TODO: exits?
            for (auto* edge : {
                &updated->child1(),
                &updated->child2()
            }) {
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            return updated;
        }

        void dump()
        {
            dataLogLn("body; check ", i, " + ", c, " < ", d, "; update ", i, " = ", update);
        }

        Node* inductionVariable() const { return i; }

        bool updateInductionVariableIsValid(Node* updatedValue) const
        {
            dataLogLnIf(verbose, "we update the induction variable ", i, " with ", updatedValue, " we expect ", update);
            return updatedValue == update;
        }
    };

    // loop-unrolling-array-clone-small.js
    struct LoopUpdate_I_NZ {
        UnrollingPhase* phase;
        Node* i { nullptr };
        int32_t c { 0 };
        Node* update { nullptr };

        static std::optional<LoopUpdate_I_NZ> make(UnrollingPhase* phase, Node* condition)
        {
            if (condition->op() == GetLocal) {
                return { LoopUpdate_I_NZ {
                    .phase = phase,
                    .i = condition,
                } };
            }

            return { };
        }

        Node* insertUnrollPreCondition(HashMap<Node*, Node*>& cloneMap, NodeOrigin origin, BasicBlock* into) const
        {
            auto& graph = phase->m_graph;
            auto* hoistedI = phase->cloneNode(cloneMap, into, i);

            for (auto* v : cloneMap.values())
                v->origin = origin;

            auto* condition = into->appendNode(graph, SpecBoolean, CompareGreater, origin,
                Edge(into->appendNode(graph, SpecNone, ArithAdd, origin, OpInfo(Arith::Unchecked),
                    Edge(hoistedI, Int32Use),
                    Edge(into->appendNode(graph, SpecNone, JSConstant, origin, OpInfo(graph.freeze(jsNumber(((int)Options::loopFactorFTLLoopUnrolling()) * c)))), Int32Use) // TODO overflow
                ), Int32Use),
                Edge(into->appendNode(graph, SpecNone, JSConstant, origin, OpInfo(graph.freeze(jsNumber(0)))), Int32Use)
            );

            // TODO: exits?
            for (auto* edge : {
                &condition->child1()->child1(),
                &condition->child1()->child2(),
                &condition->child1(),
                &condition->child2()
            }) {
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            if (condition->child1()->child1()->child1()) {
                auto* edge = &condition->child1()->child1()->child1();
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            return condition;
        }

        Node* insertUnrollPostCondition(HashMap<Node*, Node*>& cloneMap, NodeOrigin origin, BasicBlock* into, int loopFactor) const
        {
            auto& graph = phase->m_graph;
            auto* hoistedI = phase->cloneNode(cloneMap, into, i);

            for (auto* v : cloneMap.values())
                v->origin = origin;

            auto* condition = into->appendNode(graph, SpecBoolean, CompareGreater, origin,
                Edge(into->appendNode(graph, SpecNone, ArithAdd, origin, OpInfo(Arith::Unchecked),
                    Edge(hoistedI, Int32Use),
                    Edge(into->appendNode(graph, SpecNone, JSConstant, origin, OpInfo(graph.freeze(jsNumber((-1 + loopFactor) * c)))), Int32Use) // TODO overflow
                ), Int32Use),
                Edge(into->appendNode(graph, SpecNone, JSConstant, origin, OpInfo(graph.freeze(jsNumber(0)))), Int32Use)
            );

            // TODO: exits?
            for (auto* edge : {
                &condition->child1()->child1(),
                &condition->child1()->child2(),
                &condition->child1(),
                &condition->child2()
            }) {
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            if (condition->child1()->child1()->child1()) {
                auto* edge = &condition->child1()->child1()->child1();
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            return condition;
        }

        Node* rewriteVariableAccess(BasicBlock* into, Node* access, int iteration) const
        {
            auto& graph = phase->m_graph;

            dataLogLnIf(verbose, "Rewrite acccess: D@", access->index(), " iteration ", iteration, " ", iteration * c);
            RELEASE_ASSERT(access->op() == GetLocal || access->op() == SetLocal);

            auto* updated = into->appendNode(graph, SpecNone, ArithAdd, access->origin, OpInfo(Arith::Unchecked),
                Edge(into->appendNode(graph, SpecNone, GetLocal, access->origin, OpInfo(inductionVariable()->variableAccessData())), Int32Use),
                Edge(into->appendNode(graph, SpecNone, JSConstant, access->origin, OpInfo(graph.freeze(jsNumber((iteration * c))))), Int32Use) // TODO overflow
            );

            // TODO: exits?
            for (auto* edge : {
                &updated->child1(),
                &updated->child2()
            }) {
                edge->setUseKind(Int32Use);
                edge->setProofStatus(IsProved);
            }

            return updated;
        }

        void dump()
        {
            dataLogLn("body; update ", i, " = ", update, "; check ", i, ";");
        }

        Node* inductionVariable() const { return i; }

        bool updateInductionVariableIsValid(Node* updatedValue)
        {
            dataLogLnIf(verbose, "we update the induction variable ", i, " with ", updatedValue);
            if (update && update != updatedValue)
                return false;

            if (!(updatedValue->op() == ArithAdd
                && updatedValue->child2()->isInt32Constant()
                && updatedValue->child1()->op() == GetLocal
                && updatedValue->child1()->operand() == i->operand())) {
                return false;
            }
            c = updatedValue->child2()->asInt32();
            if (c >= 0)
                return false;
            update = updatedValue;
            return true;
        }
    };

    struct InvalidLoopUpdate {
        Node* insertUnrollPreCondition(HashMap<Node*, Node*>&, NodeOrigin, BasicBlock*) { RELEASE_ASSERT_NOT_REACHED(); return nullptr; }
        Node* insertUnrollPostCondition(HashMap<Node*, Node*>&, NodeOrigin, BasicBlock*, int) { RELEASE_ASSERT_NOT_REACHED(); return nullptr; }
        void dump() { dataLogLn("INVALID"); }
        Node* inductionVariable() const { RELEASE_ASSERT_NOT_REACHED(); return nullptr; }
        bool updateInductionVariableIsValid(Node*) const { RELEASE_ASSERT_NOT_REACHED(); return false; }
        Node* rewriteVariableAccess(BasicBlock*, Node*, int) const { RELEASE_ASSERT_NOT_REACHED(); return nullptr; }
    };
    constexpr static InvalidLoopUpdate invalidLoopUpdate { };

    using LoopUpdate = std::variant<InvalidLoopUpdate, LoopUpdate_IPlusC_LT_D, LoopUpdate_I_NZ>;

    struct LoopData {
        BasicBlock* preHeader { nullptr };
        BasicBlock* tail { nullptr };
        Node* inductionVariable { nullptr };
        LoopUpdate update { invalidLoopUpdate };

        bool shouldUnroll { false };
    };

    bool canCloneNode(Node* n)
    {
        switch (n->op()) {
        case ExitOK:
        case Jump:
        case JSConstant:
        case GetLocal:
        case LoopHint:
        case InvalidationPoint:
        case Phi:
            return true;
        case SetLocal:
        case Branch:
        case MovHint:
        case ZombieHint:
        case PhantomLocal:
        case GetButterfly:
        case Flush:
        case CheckArray:
        case FilterCallLinkStatus:
        case ArithBitNot:
        case CheckStructure:
            return (n->op() == PhantomLocal && n->child1()->op() == Phi) || canCloneNode(n->child1().node());
        case ArrayifyToStructure:
        case ArithAdd:
        case ArithBitAnd:
        case ArithBitOr:
        case ArithBitRShift:
        case ArithBitLShift:
        case ArithBitXor:
        case BitURShift:
        case CompareLess:
            return canCloneNode(n->child1().node()) && canCloneNode(n->child2().node());
        case PutByVal:
        case GetByVal: {
            bool valid = true;
            m_graph.doToChildren(n, [&](Edge& e) {
                valid &= canCloneNode(e.node());
            });
            return valid;
        }
        default:
            return false;
        }
    }

    Node* cloneNode(HashMap<Node*, Node*>& cloneMap, BasicBlock* into, Node* n, BasicBlock* expectedBB = nullptr)
    {
        if (!n)
            return n;
        if (cloneMap.contains(n)) // todo double hash
            return cloneMap.get(n);
        if (expectedBB && n->owner != expectedBB)
            return n;
        Node* result = cloneNodeImpl(cloneMap, into, n);
        ASSERT(result);
        cloneMap.add(n, result);
        return result;
    }

    Node* cloneNodeImpl(HashMap<Node*, Node*>& m, BasicBlock* into, Node* n)
    {
        BasicBlock* b = n->owner;
        switch (n->op()) {
        case Phi: {
            ASSERT_NOT_REACHED(); // Should already be in the map
            return nullptr;
        }
        case ExitOK:
        case LoopHint:
        case InvalidationPoint:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin);
        case GetButterfly:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin,
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()));
        case PutByVal:
        case GetByVal: {
            int firstChild = m_graph.m_varArgChildren.size();
            int children = 0;
            m_graph.doToChildren(n, [&](Edge& e) {
                m_graph.m_varArgChildren.append(Edge(cloneNode(m, into, e.node(), b), e.useKind()));
                ++children;
            });
            ASSERT(children);
            // Leave room for the other possible children.
            for (int i = children; i < 5; ++i)
                m_graph.m_varArgChildren.append(Edge());
            return into->appendNode(m_graph, n->prediction(), Node::VarArg, n->op(), n->origin,
                OpInfo(n->arrayMode().asWord()), n->hasECMAMode() ? OpInfo(n->ecmaMode()) : OpInfo(n->arrayMode().speculation()),
                firstChild, children);
        }
        case JSConstant:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->constant()));
        case Jump:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->successor(0)));
        case Branch:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(m_graph.m_branchData.add(BranchData(*n->branchData()))),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind())
            );
        case CompareLess:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin,
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
                Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind())
            );
        case CheckStructure:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(&n->structureSet()),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind())
            );
        case ArithBitNot:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind())
            );
        case ArrayifyToStructure:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->structure()),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
                Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind())
            );
        case ArithBitAnd:
        case ArithBitOr:
        case ArithBitRShift:
        case ArithBitLShift:
        case ArithBitXor:
        case BitURShift:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
                Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind())
            );
        case ArithAdd:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->arithMode()),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
                Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind())
            );
        case CheckArray:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->flags()),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind())
            );
        case FilterCallLinkStatus:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->callLinkStatus()),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind())
            );
        case GetLocal:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin,
                OpInfo(n->variableAccessData()),
                n->child1() ? Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()) : Edge());
        case MovHint:
        case Flush:
        case ZombieHint:
            return into->appendNode(m_graph, n->prediction(), n->op(), n->origin,
                n->hasUnlinkedOperand() ? OpInfo(n->unlinkedOperand()) : OpInfo(n->variableAccessData()),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()));
        case SetLocal:
        case PhantomLocal: {
            auto* v = into->appendNode(m_graph, n->prediction(), n->op(), n->origin,
                OpInfo(n->variableAccessData()),
                Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
                n->child2() ? Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind()) : Edge());
            return v;
        }
        default:
            dataLogLn("Could not clone node: ", n, " into ", into);
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

public:
    UnrollingPhase(Graph& graph)
        : Phase(graph, "Loop Unrolling")
        , m_blockInsertionSet(graph)
    {
    }

    bool run()
    {
        DFG_ASSERT(m_graph, nullptr, m_graph.m_form == ThreadedCPS);

        if (verbose) {
            dataLog("Graph before Unrolling:\n");
            m_graph.dump();
        }
        m_graph.ensureCPSDominators();
        m_graph.ensureCPSNaturalLoops();
        m_data.resize(m_graph.m_cpsNaturalLoops->numLoops());

        m_graph.initializeNodeOwners();
        bool changed = false;

        // For each loop:
        // - Identify its pre-header.
        // - Identify its inductive variable, if it has one.
        for (unsigned loopIndex = m_graph.m_cpsNaturalLoops->numLoops(); loopIndex--;) {
            const NaturalLoop& loop = m_graph.m_cpsNaturalLoops->loop(loopIndex);

            if (loop.size() > 20) {
                dataLogLnIf(verbose, "Skipping loop with header ", loop.header().node(), " since it has size ", loop.size());
                continue;
            }

            LoopData& data = m_data[loop.index()];

            BasicBlock* header = loop.header().node();
            BasicBlock* preHeader = nullptr;
            unsigned numberOfPreHeaders = 0; // We're cool if this is 1.

            // This is guaranteed because we expect the CFG not to have unreachable code. Therefore, a
            // loop header must have a predecessor. (Also, we don't allow the root block to be a loop,
            // which cuts out the one other way of having a loop header with only one predecessor.)
            DFG_ASSERT(m_graph, header->at(0), header->predecessors.size() > 1, header->predecessors.size());

            for (unsigned i = header->predecessors.size(); i--;) {
                BasicBlock* predecessor = header->predecessors[i];
                if (m_graph.m_cpsDominators->dominates(header, predecessor))
                    continue;

                preHeader = predecessor;
                ++numberOfPreHeaders;
            }

            // We need to validate the pre-header. There are a bunch of things that could be wrong
            // about it:
            //
            // - There might be more than one. This means that pre-header creation either did not run,
            //   or some CFG transformation destroyed the pre-headers.
            //
            // - It may not be legal to exit at the pre-header. That would be a real bummer. Currently,
            //   Unrolling assumes that it can always hoist checks. See
            //   https://bugs.webkit.org/show_bug.cgi?id=148545. Though even with that fixed, we anyway
            //   would need to check if it's OK to exit at the pre-header since if we can't then we
            //   would have to restrict hoisting to non-exiting nodes.

            if (numberOfPreHeaders != 1)
                continue;

            // This is guaranteed because the header has multiple predecessors and critical edges are
            // broken. Therefore the predecessors must all have one successor, which implies that they
            // must end in a Jump.
            DFG_ASSERT(m_graph, preHeader->terminal(), preHeader->terminal()->op() == Jump, preHeader->terminal()->op());

            if (!preHeader->terminal()->origin.exitOK)
                continue;

            data.preHeader = preHeader;

            dataLogLnIf(verbose, "Examining loop: ", *preHeader, " ", *header);

            m_currentLoop = &loop;
            extractInductionVariable(loop);

            if (data.shouldUnroll)
                changed |= unrollLoop(loop);
            m_currentLoop = nullptr;

            if (changed)
                break; // TODO remove
        }

        if (verbose && changed) {
            dataLog("Graph after Unrolling:\n");
            m_graph.dump();
        }

        return changed;
    }

    void extractInductionVariable(const NaturalLoop& loop)
    {
        LoopData& data = m_data[loop.index()];
        BasicBlock* header = loop.header().node();
        BasicBlock* tail = nullptr;

        for (BasicBlock* b : header->predecessors) {
            if (loop.contains(b)) {
                if (tail) {
                    dataLogLnIf(verbose, "Loop with header ", *header, " contains two tails: ", *b, " and ", *tail);
                    return;
                }
                tail = b;
            }
        }

        if (!tail) {
            dataLogLnIf(verbose, "Loop with header ", *header, " has no tail");
            return;
        }
        dataLogLnIf(verbose, "Loop with header ", *header, " has tail ", *tail);

        while (!tail->terminal()->isBranch()) {
            dataLogLnIf(verbose, "Loop with header ", *header, " has tail ", *tail, " without a branch");
            if (tail->predecessors.size() != 1)
                return;
            tail = tail->predecessors[0];
        }

        Node* condition = tail->terminal()->child1().node();
        dataLogLnIf(verbose, "Loop with header ", *header, " has updated tail ", *tail, " condition ", condition);
        data.tail = tail;

        LoopUpdate update = extractLoopUpdate(loop);

        if (std::holds_alternative<InvalidLoopUpdate>(update)) {
            dataLogLnIf(verbose, "Loop with header ", *header, " could not match loop update condition");
            return;
        }

        std::visit([&](auto& u) {
            data.inductionVariable = u.inductionVariable();
        }, update);

        if (verbose) {
            std::visit([&](auto& u) {
                u.dump();
            }, update);
        }

        // The induction variable must be updated only once in the tail
        // FIXME OOPS TODO: initialized before preheader
        // OOPS: condition cannot exit
        {
            int updates = 0;
            bool valid = true;
            for (unsigned i = 0; i < loop.size(); ++i) {
                for (Node* n : *loop.at(i).node()) {
                    if (n->op() != SetLocal
                        || n->variableAccessData()->operand() != data.inductionVariable->variableAccessData()->operand())
                        continue;
                    ++updates;
                    if (updates != 1) {
                        dataLogLnIf(verbose, "we update the induction variable ", data.inductionVariable, " with ", n, " which leaves us with ", updates, " updates");
                        valid = false;
                    }
                    if (!m_graph.m_cpsDominators->dominates(tail, n->owner)) {
                        dataLogLnIf(verbose, "we update the induction variable ", data.inductionVariable, " with ", n, " which is not dominated by ", data.tail);
                        valid = false;
                    }
                    if (!std::visit([&](auto& update) { return update.updateInductionVariableIsValid(n->child1().node()); }, update))
                        valid = false;

                    if (!valid)
                        break;
                }
                if (!valid)
                    break;
            }

            if (!valid || updates != 1) {
                dataLogLnIf(verbose, "Give up, ", valid, " ", updates);
                return;
            }
        }

        data.shouldUnroll = true;
        data.update = update;
    }

    LoopUpdate extractLoopUpdate(const NaturalLoop& loop)
    {
        LoopData& data = m_data[loop.index()];
        BasicBlock* tail = data.tail;

        Node* condition = tail->terminal()->child1().node();

        if (auto update = LoopUpdate_IPlusC_LT_D::make(this, condition))
            return { *update };

        if (auto update = LoopUpdate_I_NZ::make(this, condition))
            return { *update };

        return { invalidLoopUpdate };
    }

    BasicBlock* makeBlock(unsigned executionCount = 0)
    {
        auto* block = m_blockInsertionSet.insert(m_graph.numBlocks(), executionCount);
        block->cfaHasVisited = false;
        block->cfaDidFinish = false;
        return block;
    }

    bool unrollLoop(const NaturalLoop& loop)
    {
        LoopData& data = m_data[loop.index()];
        ASSERT(data.shouldUnroll);
        dataLogLnIf(verbose, "Trying to unroll the loop with header ", loop.header().node());

        for (uint64_t i = 0; i < loop.size(); ++i) {
            BasicBlock* b = loop.at(i).node();
            if (!b->cfaDidFinish) {
                dataLogLnIf(verbose, "block ", *b, " does not have cfa info, perhaps it was hoised already?");
                return false;
            }

            for (Node* n : *b) {
                if (!canCloneNode(n)) {
                    dataLogLnIf(verbose, "block ", *b, " has node ", n, " which is not cloneable.");
                    return false;
                }
            }
        }

        BasicBlock* const preHeader = data.preHeader;
        BasicBlock* const header = loop.header().node();
        // OSR liveness is based on this, so we need to pick it carefully.
        const NodeOrigin originHeader = preHeader->at(0)->origin.withExitOK(false);
        const NodeOrigin originBody = data.tail->at(data.tail->size() - 1)->origin.withExitOK(false);

        if (verbose)
            dataLogLn("Inserted nodes (after loop body) will have origin ", originBody, " before loop body ", originHeader);

        /*
            We match against loops that look like this:

            Preheader:
            - initial condition?
            - Jump
                |
            Header:
            - do stuff
                |
            Loop body, potentially with a bunch of control flow
                |
            Tail:
            - Condition
            - branch
            Tail update:
            - SetLocal for inductive values
            - Perhaps more effectful operations
            - Jump to header

            For example, assume:
            if (i < D) {
                do {
                    otherVar = body(i, otherVar)
                    i = i + 1
                } while ( i < D )
            }

            We produce:

            if (i < D) {
                if (i + 1 < D) {                     *** Unrolled loop precondition ***
                    do {
                        otherVar = body(i, otherVar) *** Body1 ***
                        i = i + 1
                        body(i + 1, otherVar)        *** Body2 ***
                        i = i + 1
                    } while ( i + 1 < D && i < D )   *** Unrolled loop postcondition **
                }

                do {
                    otherVar = body(i, otherVar)
                    i = i + 1
                } while ( i < D )
            }

            */

        // Replace each successor s of this block bp with functor(s)
        auto replaceSuccessorsWithFunctor = [&loop] (BasicBlock* bp, auto functor) {
            for (unsigned i = 0; i < bp->numSuccessors(); ++i) {
                if (loop.contains(bp->successor(i)))
                    bp->successor(i) = functor(bp->successor(i));
            }

            if (bp->terminal()->isBranch()) {
                auto* targetData = bp->terminal()->branchData();
                if (loop.contains(targetData->taken.block))
                    targetData->taken.block = functor(targetData->taken.block);
                if (loop.contains(targetData->notTaken.block))
                    targetData->notTaken.block = functor(targetData->notTaken.block);

            } else {
                ASSERT(bp->terminal()->isJump());
                if (loop.contains(bp->terminal()->targetBlock()))
                    bp->terminal()->targetBlock() = functor(bp->terminal()->targetBlock());
            }
        };

        // Copy: body1; tail[condition -> jump]; body2; tail2[condition -> condition']
        HashMap<BasicBlock*, BasicBlock*> blockMap;
        // Once we decide we can't use the unrolled loop anymore, we copy the original loop's condition.
        // We copy the loop backwards just to make the jumps easier to track.
        BasicBlock* const goToOriginalLoop = makeBlock();
        BasicBlock* const unrolledLoopHeader = makeBlock();
        BasicBlock* nextLoopIteration = nullptr;
        const int loopFactor = Options::loopFactorFTLLoopUnrolling();
        for (int unrolledLoopIteration = loopFactor - 1; unrolledLoopIteration >= 0; --unrolledLoopIteration) {
            const bool isFinalLoopBodyCopy = unrolledLoopIteration == loopFactor - 1;
            const bool isFirstLoopBodyCopy = unrolledLoopIteration == 0;
            blockMap.clear();
            std::queue<BasicBlock*> queue;
            HashMap<Node*, Node*> nodeMap;
            queue.push(header);

            while (!queue.empty()) {
                BasicBlock* const b = queue.front();
                queue.pop();

                if (blockMap.contains(b) || !loop.contains(b))
                    continue;

                BasicBlock* bp = nullptr;
                if (b == header && isFirstLoopBodyCopy)
                    bp = unrolledLoopHeader;
                else
                    bp = makeBlock(b->executionCount);
                blockMap.add(b, bp);

                bp->variablesAtTail = b->variablesAtTail;
                bp->variablesAtHead = b->variablesAtHead;

                bool updateWasCommited = false;
                bool updateWouldHaveBeenCommited = false;

                for (size_t i = 0; i < b->phis.size(); ++i) {
                    auto* n = b->phis[i];
                    auto* val = m_graph.addNode(n->prediction(), n->op(), n->origin, OpInfo(n->variableAccessData()));
                    ASSERT(!val->child1()); // TODO
                    bp->phis[i] = val;
                }

                for (unsigned i = 0; i < b->size(); ++i) {
                    auto* const n = b->at(i);
                    // We will handle this guy below
                    if (b == data.tail && n->isTerminal())
                        continue;
                    // Skip the induction variable update
                    if (n->op() == SetLocal && n->operand() == data.inductionVariable->operand()) {
                        updateWouldHaveBeenCommited = true;
                        if (!isFinalLoopBodyCopy) {
                            nodeMap.add(n, nullptr);
                            continue;
                        }
                        updateWasCommited = true;
                    }
                    // Re-write accesses to the induction variable
                    if (n->op() == GetLocal && n->operand() == data.inductionVariable->operand() && !updateWasCommited) {
                        Node* r = std::visit([&](auto& u) {
                            int iteration = updateWouldHaveBeenCommited ? unrolledLoopIteration + 1 : unrolledLoopIteration;
                            return u.rewriteVariableAccess(bp, n, iteration);
                        }, data.update);
                        bool result = nodeMap.add(n, r).isNewEntry;
                        ASSERT_UNUSED(result, result);
                        continue;
                    }
                    Node* np = cloneNode(nodeMap, bp, n);
                    RELEASE_ASSERT(np);
                }

                for (auto* s : b->successors())
                    queue.push(s);
            }

            // The loops above all have control flow pointing to the original loop.
            // This points them to their correct sibbling block.
            auto replaceSuccessors = [&] (BasicBlock* b, HashMap<BasicBlock*, BasicBlock*>& blockMap) {
                BasicBlock* bp = blockMap.get(b);
                ASSERT(bp);
                ASSERT(bp != b);

                replaceSuccessorsWithFunctor(bp, [&] (BasicBlock* s) { return blockMap.get(s); });
            };

            auto replaceNodes = [&] (auto& iter) {
                for (unsigned i = 0; i < iter.size(); ++i) {
                    if (iter.at(i) && nodeMap.contains(iter.at(i)))
                        iter.at(i) = nodeMap.get(iter.at(i));
                }
            };

            // Fixup the unrolled loop cf edges so they point to the unrolled loop
            // Except: the new termination condition, which should go to goToOriginalLoop
            for (uint64_t i = 0; i < loop.size(); ++i) {
                BasicBlock* b = loop.at(i).node();
                if (!blockMap.contains(b))
                    continue;
                BasicBlock* bp = blockMap.get(b);
                ASSERT(bp);

                // These need to be updated
                replaceNodes(bp->variablesAtTail);
                replaceNodes(bp->variablesAtHead);

                // Update AI
                b->cfaHasVisited = false;
                b->cfaDidFinish = false;

                bp->predecessors = b->predecessors;
                for (unsigned i = 0; i < bp->predecessors.size(); ++i) {
                    if (loop.contains(bp->predecessors.at(i)))
                        bp->predecessors.at(i) = blockMap.get(bp->predecessors.at(i));
                }

                if (b == data.tail && isFinalLoopBodyCopy) {
                    HashMap<Node*, Node*> conditionNodeMap;
                    Node* postCondition = std::visit([&](auto& u) { return u.insertUnrollPostCondition(conditionNodeMap, originBody, bp, Options::loopFactorFTLLoopUnrolling()); }, data.update);
                    BranchData* branchData = m_graph.m_branchData.add();
                    branchData->taken = BranchTarget(unrolledLoopHeader);
                    branchData->notTaken = BranchTarget(goToOriginalLoop);
                    bp->appendNode(
                        m_graph, SpecNone, Branch, originBody, OpInfo(branchData), Edge(postCondition));
                    continue;
                }

                if (b == data.tail && !isFinalLoopBodyCopy) {
                    bp->appendNode(
                        m_graph, SpecNone, Jump, data.tail->terminal()->origin, OpInfo(nextLoopIteration));
                    continue;
                }

                replaceSuccessors(b, blockMap);
            }

            nextLoopIteration = blockMap.get(header);
        }

        // And we fill out the body of goToOriginalLoop
        dataLogLnIf(verbose, "Tail: ", data.tail->index, " -> ", RawPointer(blockMap.get(data.tail)));
        ASSERT(blockMap.get(data.tail));
        {
            HashMap<Node*, Node*> conditionNodeMap;
            Node* postCondition = std::visit([&](auto& u) { return u.insertUnrollPostCondition(conditionNodeMap, originBody, goToOriginalLoop, 0); }, data.update);
            BranchData* branchData = m_graph.m_branchData.add();
            branchData->taken = BranchTarget(data.tail->terminal()->branchData()->taken);
            branchData->notTaken = BranchTarget(data.tail->terminal()->branchData()->notTaken);

            auto& continueOriginalLoop = loop.contains(branchData->taken.block) ? branchData->taken : branchData->notTaken;
            auto& exitLoop = loop.contains(branchData->taken.block) ? branchData->notTaken : branchData->taken;

            dataLogLnIf(verbose, "goToOriginalLoop will use: exitLoop ", exitLoop.block->index, " continueOriginalLoop: ", continueOriginalLoop.block->index);

            ASSERT(loop.contains(branchData->taken.block) || loop.contains(branchData->notTaken.block));
            ASSERT(loop.contains(continueOriginalLoop.block));
            ASSERT(blockMap.contains(continueOriginalLoop.block));
            ASSERT(!loop.contains(exitLoop.block));

            // FIXME: We don't want to introduce irreducible control flow here
            // but we totally do: core_sha1_loop_unrolling.js
            auto newContinueOriginalLoop = BranchTarget(blockMap.get(continueOriginalLoop.block));
            exitLoop.block->predecessors.append(newContinueOriginalLoop.block);
            
            replaceSuccessorsWithFunctor(newContinueOriginalLoop.block, [&] (BasicBlock* s) -> BasicBlock* {
                for (BasicBlock* originalSuccessor : continueOriginalLoop.block->successors()) {
                    if (blockMap.get(originalSuccessor) == s)
                        return originalSuccessor;
                }
                ASSERT_NOT_REACHED();
                return nullptr;
            });

            continueOriginalLoop = newContinueOriginalLoop;

            goToOriginalLoop->appendNode(
                m_graph, SpecNone, Branch, originBody, OpInfo(branchData), Edge(postCondition));

            goToOriginalLoop->variablesAtHead = blockMap.get(data.tail)->variablesAtTail;
            goToOriginalLoop->variablesAtTail = blockMap.get(data.tail)->variablesAtTail;
        }

        // Insert unrolled loop precondition below the preheader; this will take us to the unrolled loop.
        BasicBlock* frontPorch;
        {
            frontPorch = makeBlock(header->executionCount);
            BasicBlock* oldLoop = header;
            ASSERT(unrolledLoopHeader);
            ASSERT(oldLoop);
            HashMap<Node*, Node*> emptyNodeMap;

            Node* condition = std::visit([&](auto& u) { return u.insertUnrollPreCondition(emptyNodeMap, originHeader, frontPorch); }, data.update);

            BranchData* branchData = m_graph.m_branchData.add();
            branchData->taken = BranchTarget(unrolledLoopHeader);
            branchData->notTaken = BranchTarget(oldLoop);
            frontPorch->appendNode(
                m_graph, SpecNone, Branch, originHeader, OpInfo(branchData), Edge(condition));

            frontPorch->predecessors.append(preHeader);
            header->replacePredecessor(preHeader, frontPorch);

            if (preHeader->terminal()->isBranch()) {
                auto* targetData = preHeader->terminal()->branchData();
                if (targetData->taken.block == header)
                    targetData->taken.block = frontPorch;
                else {
                    ASSERT(targetData->notTaken.block == header);
                    targetData->notTaken.block = frontPorch;
                }

            } else {
                ASSERT(preHeader->terminal()->isJump());
                ASSERT(preHeader->terminal()->targetBlock() == header);
                preHeader->terminal()->targetBlock() = frontPorch;
            }
            for (unsigned i = 0; i < preHeader->numSuccessors(); ++i) {
                if (preHeader->successor(i) == header)
                    preHeader->successor(i) = frontPorch;
            }

            frontPorch->variablesAtHead = preHeader->variablesAtTail;
            frontPorch->variablesAtTail = preHeader->variablesAtTail;
        }

        // Update AI
        preHeader->cfaDidFinish = false;

        Vector<BasicBlock*> loopBlocksCache;
        if (verbose) {
            for (uint64_t i = 0; i < loop.size(); ++i) {
                BasicBlock* b = loop.at(i).node();
                loopBlocksCache.append(b);
            }
        }
        m_blockInsertionSet.execute();
        if (verbose) {
            for (BasicBlock* b : loopBlocksCache) {
                ASSERT(blockMap.contains(b));
                BasicBlock* bp = blockMap.get(b);
                ASSERT(bp);
                dataLogLn(*b, " -> ", *bp);
            }
            dataLogLn("frontPorch: ", *frontPorch);
            dataLogLn("goToOriginalLoop: ", *goToOriginalLoop);
        }

        m_graph.dethread();
        m_graph.invalidateCFG();
        m_graph.invalidateNodeLiveness();
        m_graph.resetReachability();
        m_graph.killUnreachableBlocks();

        ASSERT(m_graph.m_form == LoadStore);

        return true;
    }

    const NaturalLoop* m_currentLoop;
    Vector<LoopData> m_data;
    BlockInsertionSet m_blockInsertionSet;
};

bool performDFGLoopUnrolling(Graph& graph)
{
    return runPhase<UnrollingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
