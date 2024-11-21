/*
 * Cloneright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#include "DFGLoopUnrollingPhase.h"
#include "DFGBlockInsertionSet.h"
#include "bytecode/CodeOrigin.h"
#include "dfg/DFGBasicBlock.h"
#include "dfg/DFGNodeOrigin.h"
#include "dfg/DFGNodeType.h"
#include "dfg/DFGUseKind.h"
#include "wtf/Assertions.h"
#include "wtf/Compiler.h"
#include "wtf/DataLog.h"
#include <climits>
#include <cstdint>

#if ENABLE(DFG_JIT)

#include "DFGCriticalEdgeBreakingPhase.h"
#include "DFGGraph.h"
#include "DFGNaturalLoops.h"
#include "DFGPhase.h"
#include "wtf/HashSet.h"

namespace JSC {
namespace DFG {

class LoopUnrollingPhase : public Phase {
public:
#if ASSERT_ENABLED
    static constexpr bool verbose = true;
#else
    static constexpr bool verbose = false;
#endif
    static constexpr bool numberOfInnerMostLoops = 1;

    using NaturalLoop = CPSNaturalLoop;

    struct LoopData {
        BasicBlock* header() const { return loop->header().node(); }
        Node* condition() const
        {
            ASSERT(tail->terminal()->isBranch());
            return tail->terminal()->child1().node();
        }
        bool isInductionVariable(Node* node) { return node->operand() == inductionVariable->operand(); }
        void dump(PrintStream& out) const;

        const NaturalLoop* loop { nullptr };
        BasicBlock* preHeader { nullptr };
        BasicBlock* tail { nullptr };
        BasicBlock* next { nullptr };

        // for (i = initialValue; condition(i, operand); i = update(i, updateValue)) { ... }
        Node* inductionVariable { nullptr };
        int32_t initialValue { INT_MIN };
        int32_t operand { INT_MIN };
        Node* update { nullptr };
        int32_t updateValue { INT_MIN };
        uint32_t numberOfIterations { 0 };
    };

    LoopUnrollingPhase(Graph& graph)
        : Phase(graph, "Loop Unrolling"_s)
        , m_blockInsertionSet(graph)
    {
    }

    bool canCloneNode(HashSet<Node*>&, Node* n);
    Node* cloneNode(HashMap<Node*, Node*>& cloneMap, BasicBlock* into, Node* n, BasicBlock* expectedBB = nullptr);
    Node* cloneNodeImpl(HashMap<Node*, Node*>& m, BasicBlock* into, Node* n);

    bool run()
    {
        bool changed = false;

        m_graph.ensureCPSNaturalLoops();

        if constexpr (verbose) {
            dataLogLn("Graph before Loop Unrolling Phase:");
            m_graph.dump();
        }

        m_data.resize(m_graph.m_cpsNaturalLoops->numLoops());

        // Find all inner most loops
        HashSet<const NaturalLoop*> innerMostLoops;
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block || !block->cfaHasVisited)
                continue;

            if (innerMostLoops.size() > numberOfInnerMostLoops)
                return false;

            const NaturalLoop* loop = m_graph.m_cpsNaturalLoops->innerMostLoopOf(block);
            if (!loop)
                continue;

            innerMostLoops.add(loop);
            dataLogLnIf(verbose, "found innerMostLoop=", *loop);
        }

        // For each inner most loop:
        for (const NaturalLoop* loop : innerMostLoops) {
            if (loop->size() > 5) {
                dataLogLnIf(verbose, "Skipping loop with header ", loop->header().node(), " since it has size ", loop->size());
                continue;
            }

            LoopData& data = m_data[loop->index()];
            data.loop = loop;

            // PreHeader                          PreHeader
            //  |                                  |
            // Header <---                        HeaderBodyTailGraph_0 <-- original loop
            //  |        |      unrolled to        |
            // Body      |   ================>    HeaderBodyTailGraph_1 <-- 1st copy
            //  |        |                         |
            // Tail ------                        ...
            //  |                                  |
            // Next                               HeaderBodyTailGraph_n <-- n_th copy
            //                                     |
            //                                    Next
            //
            // Note that NaturalLoop's body includes Header, Body, and Tail. The unrolling
            // process appends the HeaderBodyTailGraph copy reversely (from n_th to 1st).

            if (!findPreHeader(data))
                continue;
            dataLogLnIf(verbose, "after findPreHeader ", data);

            if (!findTail(data))
                continue;
            dataLogLnIf(verbose, "after findTail ", data);

            if (!findInductionVariable(data))
                continue;
            dataLogLnIf(verbose, "after findInductionVariable ", data);

            if (!isValid(data))
                continue;
            dataLogLnIf(verbose, "after isValid ", data);

            if (unrollLoop(data)) {
                if constexpr (verbose) {
                    dataLogLn("Graph after Loop Unrolling for loop header=", *data.header());
                    m_graph.dump();
                }
            }
        }

        return changed;
    }

    bool findPreHeader(LoopData& data)
    {
        BasicBlock* preHeader = nullptr;
        BasicBlock* header = data.header();

        // This is guaranteed because we expect the CFG not to have unreachable code. Therefore, a
        // loop header must have a predecessor. (Also, we don't allow the root block to be a loop,
        // which cuts out the one other way of having a loop header with only one predecessor.)
        DFG_ASSERT(m_graph, header->at(0), header->predecessors.size() > 1, header->predecessors.size());

        uint32_t numberOfPreHeaders = 0;
        for (uint32_t i = header->predecessors.size(); i--;) {
            BasicBlock* predecessor = header->predecessors[i];
            if (m_graph.m_cpsDominators->dominates(header, predecessor))
                continue;

            if (isJumpPadBlock(predecessor)) { // TODO: do we need this?
                ASSERT(predecessor->predecessors.size() == 1);
                predecessor = predecessor->predecessors[0];
            }

            preHeader = predecessor;
            ++numberOfPreHeaders;
        }

        if (numberOfPreHeaders != 1)
            return false;

        data.preHeader = preHeader;
        return true;
    }

    bool findTail(LoopData& data)
    {
        BasicBlock* header = data.header();
        BasicBlock* tail = nullptr;

        for (BasicBlock* predecessor : header->predecessors) {
            if (!m_graph.m_cpsDominators->dominates(header, predecessor))
                continue;

            if (isJumpPadBlock(predecessor)) { // TODO: do we still need this?
                ASSERT(predecessor->predecessors.size() == 1);
                predecessor = predecessor->predecessors[0];
            }

            if (tail) {
                dataLogLnIf(verbose, "Loop with header ", *header, " contains two tails: ", *predecessor, " and ", *tail);
                return false;
            }

            tail = predecessor;
        }

        if (!tail) {
            dataLogLnIf(verbose, "Loop with header ", *header, " has no tail");
            return false;
        }
        dataLogLnIf(verbose, "Loop with header ", *header, " has tail ", *tail);

        // TODO: May we don't need this
        while (!tail->terminal()->isBranch()) {
            dataLogLnIf(verbose, "Loop with header ", *header, " has tail ", *tail, " without a branch");
            if (tail->predecessors.size() != 1)
                return false;
            tail = tail->predecessors[0];
        }

        ASSERT(tail->successors().size() == 2);
        for (BasicBlock* successor : tail->successors()) {
            if (data.loop->contains(successor))
                continue;
            data.next = successor;
        }

        data.tail = tail;
        return true;
    }

    bool findInductionVariable(LoopData& data)
    {
        Node* condition = data.condition();
        auto isValidCondition = [&]() {
            // TODO: currently supported pattern: i + constant < constant
            if (condition->op() != CompareLess)
                return false;
            // Condition left
            Node* update = condition->child1().node();
            if (update->op() != ArithAdd)
                return false;
            if (update->child1()->op() != GetLocal)
                return false;
            if (!update->child2()->isInt32Constant())
                return false;
            // Condition right
            if (!condition->child2()->isInt32Constant())
                return false;

            data.operand = condition->child2()->asInt32();
            data.update = condition->child1().node();
            data.updateValue = update->child2()->asInt32();
            data.inductionVariable = condition->child1()->child1().node();
            return true;
        };
        if (!isValidCondition()) {
            dataLogLnIf(verbose, "Invalid loop condition node D@", condition->index());
            return false;
        }

        auto hasValidInitialValue = [&]() {
            Node* initialization = nullptr;
            for (Node* n : *data.preHeader) {
                if (n->op() != SetLocal || !data.isInductionVariable(n))
                    continue;
                initialization = n;
            }
            if (!initialization || !initialization->child1()->isInt32Constant())
                return false;
            data.initialValue = initialization->child1()->asInt32();
            return true;
        };
        if (!hasValidInitialValue()) {
            dataLogLnIf(verbose, "Invalid initial value");
            return false;
        }

        auto isValidInductionVariable = [&]() {
            // TODO condition cannot exit
            uint32_t updateCount = 0;
            for (uint32_t i = 0; i < data.loop->size(); ++i) {
                BasicBlock* body = data.loop->at(i).node();
                for (Node* node : *body) {
                    if (node->op() != SetLocal || !data.isInductionVariable(node))
                        continue;
                    dataLogLnIf(verbose, "Induction variable ", data.inductionVariable->index(), " is updated at node ", node->index(), " at ", *body);
                    ++updateCount;
                    if (updateCount != 1) {
                        dataLogLnIf(verbose, "Induction variable is updated multiple times at ", *body);
                        return false;
                    }
                    if (!m_graph.m_cpsDominators->dominates(data.tail, body)) {
                        dataLogLnIf(verbose, "Tail ", *data.tail, " doesn't dominate ", *body);
                        return false;
                    }
                }
            }
            return true;
        };
        if (!isValidInductionVariable()) {
            dataLogLnIf(verbose, "Invalid induction variable");
            return false;
        }

        // Compute the number of iterations in the loop.
        {
            auto getCompareFunction = [&](Node* condition) -> std::function<bool(int, int)> {
                switch (condition->op()) {
                case CompareLess:
                    return [](auto a, auto b) { return a < b; };
                // TODO: add more
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    return [](auto, auto) { return false; };
                }
            };
            auto compareFunction = getCompareFunction(condition);

            auto getUpdateFunction = [&](Node* update) -> std::function<int(int, int)> {
                switch (update->op()) {
                case ArithAdd:
                    return [](auto a, auto b) { return a + b; };
                // TODO: add more
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    return [](auto, auto) { return 0; };
                }
            };
            auto updateFunction = getUpdateFunction(data.update);

            uint32_t numberOfIterations = 0;
            for (int32_t i = data.initialValue; compareFunction(i, data.operand); i = updateFunction(i, data.updateValue)) {
                ++numberOfIterations;
            }
            data.numberOfIterations = numberOfIterations;
        }
        return true;
    }

    bool isValid(LoopData& data)
    {
        const NaturalLoop* const loop = data.loop;

        HashSet<Node*> cloneAble;
        for (uint32_t i = 0; i < loop->size(); ++i) {
            BasicBlock* body = loop->at(i).node();
            // TODO: We don't need this since it's before LICM.
            // if (!body->cfaDidFinish) {
            //     dataLogLnIf(verbose, "block ", *body, " does not have cfa info, perhaps it was hoisted already?");
            //     return false;
            // }

            for (Node* node : *body) {
                if (!canCloneNode(cloneAble, node)) {
                    dataLogLnIf(verbose, "block ", *body, " has node ", node, " which is not cloneAble.");
                    return false;
                }
            }
        }
        return true;
    }

    BasicBlock* makeBlock(uint32_t executionCount = 0)
    {
        auto* block = m_blockInsertionSet.insert(m_graph.numBlocks(), executionCount);
        block->cfaHasVisited = false;
        block->cfaDidFinish = false;
        return block;
    }

    bool unrollLoop(LoopData& data)
    {
        const NaturalLoop* const loop = data.loop;
        BasicBlock* const header = data.header();

        const NodeOrigin tailTerminalOrigin = data.tail->terminal()->origin;
        const CodeOrigin tailTerminalOriginSemantic = tailTerminalOrigin.semantic;
        dataLogLnIf(verbose, "tailTerminalOriginSemantic ", tailTerminalOriginSemantic);

        // Mapping from the origin to the clones.
        HashMap<BasicBlock*, BasicBlock*> blockClones;
        HashMap<Node*, Node*> nodeClones;

        auto replaceOperands = [&](auto& iter) {
            for (uint32_t i = 0; i < iter.size(); ++i) {
                if (iter.at(i) && nodeClones.contains(iter.at(i)))
                    iter.at(i) = nodeClones.get(iter.at(i));
            }
        };

        BasicBlock* next = data.next;
        for (uint32_t numberOfClones = data.numberOfIterations - 1; numberOfClones--;) {
            blockClones.clear();
            nodeClones.clear();

            // 1. Initialize all block clones.
            for (uint32_t i = 0; i < loop->size(); ++i) {
                BasicBlock* body = loop->at(i).node();
                blockClones.add(loop->at(i).node(), makeBlock(body->executionCount));
            }

            for (uint32_t i = 0; i < loop->size(); ++i) {
                BasicBlock* body = loop->at(i).node();
                BasicBlock* clone = blockClones.get(body);

                // 2. Clone Phis.
                clone->phis.resize(body->phis.size());
                for (size_t i = 0; i < body->phis.size(); ++i) {
                    Node* bodyPhi = body->phis[i];
                    Node* phiClone = m_graph.addNode(bodyPhi->prediction(), bodyPhi->op(), bodyPhi->origin, OpInfo(bodyPhi->variableAccessData()));
                    nodeClones.add(bodyPhi, phiClone);
                    ASSERT(!phiClone->child1()); // TODO: What's this?
                    clone->phis[i] = phiClone;
                }

                // 3. Clone nodes.
                for (uint32_t i = 0; i < body->size(); ++i) {
                    Node* bodyNode = body->at(i);
                    // Ignore the branch nodes at the end of the tail since we can directly jump to next (See step 5).
                    if (body == data.tail && bodyNode->origin.semantic == tailTerminalOriginSemantic)
                        continue;
                    cloneNode(nodeClones, clone, bodyNode);
                }

                // 4. Clone variables and tail and head.
                clone->variablesAtTail = body->variablesAtTail;
                replaceOperands(clone->variablesAtTail);
                clone->variablesAtHead = body->variablesAtHead;
                replaceOperands(clone->variablesAtHead);

                // 5. Clone successors. (predecessors will be fixed in resetReachability below)
                if (body == data.tail) {
                    clone->appendNode(m_graph, SpecNone, Jump, tailTerminalOrigin, OpInfo(next));
                } else {
                    for (uint32_t i = 0; i < body->numSuccessors(); ++i) {
                        auto& successor = clone->successor(i);
                        ASSERT(successor == body->successor(i));
                        if (loop->contains(successor))
                            successor = blockClones.get(successor);
                    }

                    if (clone->terminal()->isBranch()) {
                        auto& taken = clone->terminal()->branchData()->taken.block;
                        ASSERT(taken == body->terminal()->branchData()->taken.block);
                        if (loop->contains(taken))
                            taken = blockClones.get(taken);

                        auto& notTaken = clone->terminal()->branchData()->notTaken.block;
                        ASSERT(notTaken, body->terminal()->branchData()->notTaken.block);
                        if (loop->contains(notTaken))
                            notTaken = blockClones.get(notTaken);

                    } else {
                        ASSERT(clone->terminal()->isJump());
                        if (loop->contains(clone->terminal()->targetBlock()))
                            clone->terminal()->targetBlock() = blockClones.get(clone->terminal()->targetBlock());
                    }
                }
            }

            next = blockClones.get(header);
        }

        // 6. Replace the original loop tail branch with a jump to the last header clone.
        {
            for (Node* node : *header) {
                if (node->origin.semantic == tailTerminalOriginSemantic)
                    node->removeWithoutChecks();
            }
            header->appendNode(m_graph, SpecNone, Jump, tailTerminalOrigin, OpInfo(next));
        }

        // Done clone.
        m_blockInsertionSet.execute();

        if constexpr (verbose) {
            dataLogLn("Graph after m_blockInsertionSet.execute header=", *data.header());
            m_graph.dump();
        }

        // Disable AI for the original loop body.
        for (uint32_t i = 0; i < loop->size(); ++i) {
            BasicBlock* body = loop->at(i).node();
            body->cfaHasVisited = false;
            body->cfaDidFinish = false;
        }

        m_graph.dethread();
        m_graph.invalidateCFG();
        m_graph.resetReachability();
        m_graph.killUnreachableBlocks();
        m_graph.invalidateNodeLiveness();
        ASSERT(m_graph.m_form == LoadStore);

        return true;
    }

    Vector<LoopData> m_data;
    BlockInsertionSet m_blockInsertionSet;
};

bool performLoopUnrolling(Graph& graph)
{
    return runPhase<LoopUnrollingPhase>(graph);
}

void LoopUnrollingPhase::LoopData::dump(PrintStream& out) const
{
    out.print(*loop);

    out.print(" preHeader=");
    if (preHeader)
        out.print(*preHeader);
    else
        out.print("<null>");
    out.print(", ");

    out.print("tail=");
    if (tail)
        out.print(*tail);
    else
        out.print("<null>");
    out.print(", ");

    out.print("next=");
    if (tail)
        out.print(*next);
    else
        out.print("<null>");
    out.print(", ");

    out.print("inductionVariable=");
    if (inductionVariable)
        out.print("D@", inductionVariable->index());
    else
        out.print("<null>");
    out.print(", ");

    out.print("initValue=", initialValue, ", ");
    out.print("operand=", operand, ", ");

    out.print("update=");
    if (update)
        out.print("D@", update->index());
    else
        out.print("<null>");
    out.print(", ");

    out.print("updateValue=", updateValue, ", ");

    out.print("numberOfIterations=", numberOfIterations);
}

bool LoopUnrollingPhase::canCloneNode(HashSet<Node*>& cloneAble, Node* node)
{
    if (cloneAble.contains(node))
        return true;

    bool result = true;
    switch (node->op()) {
    case PhantomLocal:
        // TODO: what is this?
        result = (node->op() == PhantomLocal && node->child1()->op() == Phi) || canCloneNode(cloneAble, node->child1().node());
        break;

    case JSConstant:
    case LoopHint:

    case Jump:
    case Branch:

    case MovHint:
    case ExitOK:
    case ZombieHint:
    case InvalidationPoint:
    case Check:
    case Phi:
    case Flush:

    case GetLocal:
    case SetLocal:
    case GetButterfly:

    case CheckArray:
    case AssertNotEmpty:
    case CheckStructure:

    case FilterCallLinkStatus:
    case ArrayifyToStructure:

    case ArithAdd:
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitNot:
    case ArithBitRShift:
    case ArithBitLShift:
    case ArithBitXor:
    case BitURShift:

    case CompareLess:
    case PutByVal:
    case PutByValAlias:
    case GetByVal: {
        m_graph.doToChildren(node, [&](Edge& e) {
            result &= canCloneNode(cloneAble, e.node());
        });
        break;
    }
    default:
        result = false;
        break;
    }
    if (result)
        cloneAble.add(node);
    return result;
}

Node* LoopUnrollingPhase::cloneNode(HashMap<Node*, Node*>& nodeClones, BasicBlock* into, Node* bodyNode, BasicBlock* expectedBB)
{
    if (!bodyNode) // TODO: need this?
        return bodyNode;
    if (nodeClones.contains(bodyNode)) // TODO: double hash?
        return nodeClones.get(bodyNode);
    if (expectedBB && bodyNode->owner != expectedBB)
        return bodyNode;
    Node* result = cloneNodeImpl(nodeClones, into, bodyNode);
    ASSERT(result);
    nodeClones.add(bodyNode, result);
    return result;
}

Node* LoopUnrollingPhase::cloneNodeImpl(HashMap<Node*, Node*>& m, BasicBlock* into, Node* n)
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
    case GetByVal:
    case PutByValAlias: {
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
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()));
    case CompareLess:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin,
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
            Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind()));
    case CheckStructure:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(&n->structureSet()),
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()));
    case ArithBitNot:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(),
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()));
    case ArrayifyToStructure:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->structure()),
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
            Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind()));
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitRShift:
    case ArithBitLShift:
    case ArithBitXor:
    case BitURShift:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(),
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
            Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind()));
    case ArithAdd:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->arithMode()),
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()),
            Edge(cloneNode(m, into, n->child2().node(), b), n->child2().useKind()));
    case CheckArray:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->flags()),
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()));
    case FilterCallLinkStatus:
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->callLinkStatus()),
            Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()));
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
    case Check: {
        Edge edge = n->child1() ? Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind()) : Edge();
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->flags()), edge);
    }
    case AssertNotEmpty: {
        ASSERT(n->child1());
        Edge edge = Edge(cloneNode(m, into, n->child1().node(), b), n->child1().useKind());
        return into->appendNode(m_graph, n->prediction(), n->op(), n->origin, OpInfo(n->flags()), edge);
    }
    default:
        dataLogLn("Could not clone node: ", n, " into ", into);
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

}
} // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
