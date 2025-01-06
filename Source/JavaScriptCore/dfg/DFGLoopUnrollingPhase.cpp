/*
 * Cloneright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "CodeOrigin.h"
#include "DFGBasicBlockInlines.h"
#include "DFGBlockInsertionSet.h"
#include "DFGCFAPhase.h"
#include "DFGGraph.h"
#include "DFGNaturalLoops.h"
#include "DFGNodeOrigin.h"
#include "DFGNodeType.h"
#include "DFGPhase.h"
#include <wtf/IndexMap.h>

namespace JSC {
namespace DFG {

class LoopUnrollingPhase : public Phase {
public:
    using NaturalLoop = CPSNaturalLoop;

    using ComparisonFunction = bool (*)(CheckedInt32, CheckedInt32);
    using UpdateFunction = CheckedInt32 (*)(CheckedInt32, CheckedInt32);

    struct LoopData {
        uint32_t loopSize() { return loop->size(); }
        BasicBlock* loopBody(uint32_t i) { return loop->at(i).node(); }
        BasicBlock* header() const { return loop->header().node(); }
        Node* condition() const { return tail->terminal()->child1().node(); }
        bool isInductionVariable(Node* node) { return node->operand() == inductionVariable->operand(); }
        void dump(PrintStream& out) const;

        const NaturalLoop* loop { nullptr };
        BasicBlock* preHeader { nullptr };
        BasicBlock* tail { nullptr };
        BasicBlock* next { nullptr };

        // for (i = initialValue; condition(i, operand); i = update(i, updateValue)) { ... }
        Node* inductionVariable { nullptr };
        CheckedInt32 initialValue { INT_MIN };
        CheckedInt32 operand { INT_MIN };
        Node* update { nullptr };
        CheckedInt32 updateValue { INT_MIN };
        CheckedUint32 iterationCount { 0 };
    };

    LoopUnrollingPhase(Graph& graph)
        : Phase(graph, "Loop Unrolling"_s)
        , m_blockInsertionSet(graph)
    {
    }

    bool run()
    {
        bool changed = false;

        if (UNLIKELY(Options::verboseLoopUnrolling())) {
            dataLogLn("Graph before Loop Unrolling Phase:");
            m_graph.dump();
        }

        uint32_t unrolledCount = 0;
        while (true) {
            const NaturalLoop* loop = selectDeepestNestedLoop();
            if (!loop || unrolledCount >= Options::maxLoopUnrollingCount())
                break;
            if (!tryUnroll(loop))
                break;

            ++unrolledCount;
            changed = true;
        }

        dataLogLnIf(Options::verboseLoopUnrolling(), "Successfully unrolled ", unrolledCount, " loops.");
        return changed;
    }

    const NaturalLoop* selectDeepestNestedLoop()
    {
        m_graph.ensureCPSNaturalLoops();

        const NaturalLoop* target = nullptr;
        int32_t maxDepth = INT_MIN;
        uint32_t loopCount = m_graph.m_cpsNaturalLoops->numLoops();
        IndexMap<const NaturalLoop*, int32_t> depthCache(loopCount, INT_MIN);

        for (uint32_t loopIndex = loopCount; loopIndex--;) {
            const NaturalLoop& loop = m_graph.m_cpsNaturalLoops->loop(loopIndex);
            ASSERT(loop.index() == loopIndex && depthCache[loopIndex] == INT_MIN);

            int32_t depth = 0;
            const NaturalLoop* current = &loop;
            while (current) {
                int32_t cachedDepth = depthCache[current->index()];
                if (cachedDepth != INT_MIN) {
                    depth += cachedDepth;
                    break;
                }
                ++depth;
                current = m_graph.m_cpsNaturalLoops->innerMostOuterLoop(*current);
            }
            depthCache[loopIndex] = depth;

            if (depth > maxDepth) {
                maxDepth = depth;
                target = &loop;
            }
        }
        return target;
    }

    bool tryUnroll(const NaturalLoop* loop)
    {
        if (UNLIKELY(Options::verboseLoopUnrolling())) {
            const NaturalLoop* outerLoop = m_graph.m_cpsNaturalLoops->innerMostOuterLoop(*loop);
            dataLogLnIf(Options::verboseLoopUnrolling(), "\nTry unroll innerMostLoop=", *loop, " with innerMostOuterLoop=", outerLoop ? *outerLoop : NaturalLoop());
        }

        LoopData data = { loop };

        if (!shouldUnrollLoop(data))
            return false;

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
        // process appends the HeaderBodyTailGraph copies in reverse order (from n_th to 1st).

        if (!locatePreHeader(data))
            return false;
        dataLogLnIf(Options::verboseLoopUnrolling(), "\tFound PreHeader with LoopData=", data);

        if (!locateTail(data))
            return false;
        dataLogLnIf(Options::verboseLoopUnrolling(), "\tFound Tail with LoopData=", data);

        if (!identifyInductionVariable(data))
            return false;
        dataLogLnIf(Options::verboseLoopUnrolling(), "\tFound InductionVariable with LoopData=", data);

        if (!canCloneLoop(data))
            return false;

        BasicBlock* header = data.header();
        unrollLoop(data);

        if (UNLIKELY(Options::verboseLoopUnrolling())) {
            dataLogLn("\tGraph after Loop Unrolling for loop");
            m_graph.dump();
        }

        if (UNLIKELY(Options::printEachUnrolledLoop()))
            dataLogLn("\tIn function ", m_graph.m_codeBlock->inferredName(), ", successfully unrolled the loop header=", *header);
        return true;
    }

    bool locatePreHeader(LoopData& data)
    {
        BasicBlock* preHeader = nullptr;
        BasicBlock* header = data.header();

        // This is guaranteed because we expect the CFG not to have unreachable code. Therefore, a
        // loop header must have a predecessor. (Also, we don't allow the root block to be a loop,
        // which cuts out the one other way of having a loop header with only one predecessor.)
        DFG_ASSERT(m_graph, header->at(0), header->predecessors.size() > 1, header->predecessors.size());

        uint32_t preHeaderCount = 0;
        for (uint32_t i = header->predecessors.size(); i--;) {
            BasicBlock* predecessor = header->predecessors[i];
            if (m_graph.m_cpsDominators->dominates(header, predecessor))
                continue;

            preHeader = predecessor;
            ++preHeaderCount;
        }

        if (preHeaderCount != 1)
            return false;

        data.preHeader = preHeader;
        return true;
    }

    bool locateTail(LoopData& data)
    {
        BasicBlock* header = data.header();
        BasicBlock* tail = nullptr;

        for (BasicBlock* predecessor : header->predecessors) {
            if (!m_graph.m_cpsDominators->dominates(header, predecessor))
                continue;

            if (tail) {
                dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *header, " since it contains two tails: ", *predecessor, " and ", *tail);
                return false;
            }

            tail = predecessor;
        }

        if (!tail) {
            dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *header, " since it has no tail");
            return false;
        }

        // PreHeader                          PreHeader
        //  |                                  |
        // Header <---                        Header_0
        //  |        |       unrolled to       |
        //  |       Tail  =================>  Branch_0
        //  |        |                         |
        // Branch ----                        Tail_0
        //  |                                  |
        // Next                               ...
        //                                     |
        //                                    Header_n
        //                                     |
        //                                    Branch_n
        //                                     |
        //                                    Next
        //
        // FIXME: This is not supported yet. We should do it only if it's profitable.
        if (!tail->terminal()->isBranch()) {
            dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *header, " since it has a non-branch tail");
            return false;
        }

        for (BasicBlock* successor : tail->successors()) {
            if (data.loop->contains(successor))
                continue;
            data.next = successor;
        }
        data.tail = tail;
        return true;
    }

    bool isSupportedConditionOp(NodeType op);
    bool isSupportedUpdateOp(NodeType op);

    ComparisonFunction comparisonFunction(Node* condition);
    UpdateFunction updateFunction(Node* update);

    bool identifyInductionVariable(LoopData& data)
    {
        Node* condition = data.condition();
        auto isConditionValid = [&]() ALWAYS_INLINE_LAMBDA {
            if (!isSupportedConditionOp(condition->op()))
                return false;

            // Condition left
            Edge update = condition->child1();
            if (!isSupportedUpdateOp(update->op()) || update.useKind() != Int32Use)
                return false;
            // FIXME: Currently, we assume the left operand is the induction variable.
            if (update->child1()->op() != GetLocal)
                return false;
            if (!update->child2()->isInt32Constant())
                return false;

            // Condition right
            Edge operand = condition->child2();
            if (!operand->isInt32Constant() || operand.useKind() != Int32Use)
                return false;

            data.operand = condition->child2()->asInt32();
            data.update = condition->child1().node();
            data.updateValue = update->child2()->asInt32();
            data.inductionVariable = condition->child1()->child1().node();
            return true;
        };
        if (!isConditionValid()) {
            dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since the invalid loop condition node D@", condition->index());
            return false;
        }

        auto isInitialValueValid = [&]() ALWAYS_INLINE_LAMBDA {
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
        if (!isInitialValueValid()) {
            dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since the initial value is invalid");
            return false;
        }

        auto isInductionVariableValid = [&]() ALWAYS_INLINE_LAMBDA {
            uint32_t updateCount = 0;
            for (uint32_t i = 0; i < data.loopSize(); ++i) {
                BasicBlock* body = data.loopBody(i);
                for (Node* node : *body) {
                    if (node->op() != SetLocal || !data.isInductionVariable(node))
                        continue;
                    dataLogLnIf(Options::verboseLoopUnrolling(), "Induction variable ", data.inductionVariable->index(), " is updated at node ", node->index(), " at ", *body);
                    ++updateCount;
                    // FIXME: Maybe we can extend this and do better here?
                    if (updateCount != 1)
                        return false;
                    if (!m_graph.m_cpsDominators->dominates(data.tail, body))
                        return false;
                }
            }
            return true;
        };
        if (!isInductionVariableValid()) {
            dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since the induction variable is invalid");
            return false;
        }

        // Compute the number of iterations in the loop.
        {
            CheckedUint32 iterationCount = 0;
            auto compare = comparisonFunction(condition);
            auto update = updateFunction(data.update);
            for (CheckedInt32 i = data.initialValue; compare(i, data.operand);) {
                if (iterationCount > Options::maxLoopUnrollingIterationCount()) {
                    dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since maxLoopUnrollingIterationCount =", Options::maxLoopUnrollingIterationCount());
                    return false;
                }
                i = update(i, data.updateValue);
                if (i.hasOverflowed()) {
                    dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since the induction variable overflowed after the update");
                    return false;
                }
                ++iterationCount;
            }
            data.iterationCount = iterationCount;
        }
        return true;
    }

    bool shouldUnrollLoop(LoopData& data)
    {
        uint32_t totalNodeCount = 0;
        for (uint32_t i = 0; i < data.loopSize(); ++i) {
            BasicBlock* body = data.loopBody(i);
            if (!body->isReachable) {
                dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since block ", *body, " is not reachable");
                return false;
            }

            // FIXME: We may also need to check whether the block is valid using CFA.
            // If the block is unreachable or invalid in the CFG, we can directly
            // ignore the loop, avoiding unnecessary cloneability checks for nodes in invalid blocks.

            totalNodeCount += body->size();
            if (totalNodeCount > Options::maxLoopUnrollingBodyNodeSize()) {
                dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " and loop node count=", totalNodeCount, " since maxLoopUnrollingBodyNodeSize =", Options::maxLoopUnrollingBodyNodeSize());
                return false;
            }
        }
        return true;
    }

    bool canCloneLoop(LoopData& data)
    {
        HashSet<Node*> cloneableCache;
        for (uint32_t i = 0; i < data.loopSize(); ++i) {
            BasicBlock* body = data.loopBody(i);
            for (Node* node : *body) {
                if (!isNodeCloneable(cloneableCache, node)) {
                    dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since D@", node->index(), " with op ", node->op(), " is not cloneable");
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

    void unrollLoop(LoopData& data)
    {
        BasicBlock* const header = data.header();
        BasicBlock* const tail = data.tail;

        const NodeOrigin tailTerminalOrigin = tail->terminal()->origin;
        const CodeOrigin tailTerminalOriginSemantic = tailTerminalOrigin.semantic;
        dataLogLnIf(Options::verboseLoopUnrolling(), "tailTerminalOriginSemantic ", tailTerminalOriginSemantic);

        // Mapping from the origin to the clones.
        UncheckedKeyHashMap<BasicBlock*, BasicBlock*> blockClones;
        UncheckedKeyHashMap<Node*, Node*> nodeClones;

        auto replaceOperands = [&](auto& nodes) ALWAYS_INLINE_LAMBDA {
            for (uint32_t i = 0; i < nodes.size(); ++i) {
                if (auto& node = nodes.at(i)) {
                    auto itr = nodeClones.find(node);
                    if (itr != nodeClones.end())
                        node = itr->value;
                }
            }
        };

#if ASSERT_ENABLED
        m_graph.initializeNodeOwners(); // This is only used for the debug assertion in cloneNodeImpl.
#endif

        BasicBlock* next = data.next;
        for (uint32_t cloneCount = data.iterationCount - 1; cloneCount--;) {
            blockClones.clear();
            nodeClones.clear();

            // 1. Initialize all block clones.
            for (uint32_t i = 0; i < data.loopSize(); ++i) {
                BasicBlock* body = data.loopBody(i);
                blockClones.add(body, makeBlock(body->executionCount));
            }

            for (uint32_t i = 0; i < data.loopSize(); ++i) {
                BasicBlock* const body = data.loopBody(i);
                BasicBlock* const clone = blockClones.get(body);

                // 2. Clone Phis.
                clone->phis.resize(body->phis.size());
                for (size_t i = 0; i < body->phis.size(); ++i) {
                    Node* bodyPhi = body->phis[i];
                    Node* phiClone = m_graph.addNode(bodyPhi->prediction(), bodyPhi->op(), bodyPhi->origin, OpInfo(bodyPhi->variableAccessData()));
                    nodeClones.add(bodyPhi, phiClone);
                    clone->phis[i] = phiClone;
                }

                // 3. Clone nodes.
                for (uint32_t i = 0; i < body->size(); ++i) {
                    Node* bodyNode = body->at(i);
                    // Ignore the branch nodes at the end of the tail since we can directly jump to next (See step 5).
                    if (body == tail && bodyNode->origin.semantic == tailTerminalOriginSemantic)
                        continue;
                    cloneNode(nodeClones, clone, bodyNode);
                }

                // 4. Clone variables and tail and head.
                clone->variablesAtTail = body->variablesAtTail;
                replaceOperands(clone->variablesAtTail);
                clone->variablesAtHead = body->variablesAtHead;
                replaceOperands(clone->variablesAtHead);

                // 5. Clone successors. (predecessors will be fixed in resetReachability below)
                if (body == tail)
                    clone->appendNode(m_graph, SpecNone, Jump, tailTerminalOrigin, OpInfo(next));
                else {
                    for (uint32_t i = 0; i < body->numSuccessors(); ++i) {
                        auto& successor = clone->successor(i);
                        ASSERT(successor == body->successor(i));
                        if (data.loop->contains(successor))
                            successor = blockClones.get(successor);
                    }
                }
            }

            next = blockClones.get(header);
        }

        // 6. Replace the original loop tail branch with a jump to the last header clone.
        {
            for (Node* node : *tail) {
                if (node->origin.semantic == tailTerminalOriginSemantic)
                    node->removeWithoutChecks();
            }
            tail->appendNode(m_graph, SpecNone, Jump, tailTerminalOrigin, OpInfo(next));
        }

        // Done clone.
        if (!m_blockInsertionSet.execute()) {
            m_graph.invalidateCFG();
            m_graph.dethread();
        }
        m_graph.resetReachability();
        m_graph.killUnreachableBlocks();
        ASSERT(m_graph.m_form == LoadStore);
    }

    bool isNodeCloneable(HashSet<Node*>& cloneableCache, Node*);
    Node* cloneNode(UncheckedKeyHashMap<Node*, Node*>& nodeClones, BasicBlock* into, Node*);
    Node* cloneNodeImpl(UncheckedKeyHashMap<Node*, Node*>& nodeClones, BasicBlock* into, Node*);

private:
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

    out.print("iterationCount=", iterationCount);
}

bool LoopUnrollingPhase::isNodeCloneable(HashSet<Node*>& cloneableCache, Node* node)
{
    if (cloneableCache.contains(node))
        return true;

    bool result = true;
    switch (node->op()) {
    case Phi:
        break;
    case ValueRep:
    case DoubleRep:
    case JSConstant:
    case LoopHint:
    case PhantomLocal:
    case SetArgumentDefinitely:
    case Jump:
    case Branch:
    case MovHint:
    case ExitOK:
    case ZombieHint:
    case InvalidationPoint:
    case Check:
    case CheckVarargs:
    case Flush:
    case GetLocal:
    case SetLocal:
    case GetButterfly:
    case CheckArray:
    case AssertNotEmpty:
    case CheckStructure:
    case FilterCallLinkStatus:
    case ArrayifyToStructure:
    case NewArrayWithConstantSize:
    case NewArrayWithSize:
    case ValueToInt32:
    case ArithAdd:
    case ArithSub:
    case ArithMul:
    case ArithDiv:
    case ArithMod:
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitNot:
    case ArithBitRShift:
    case ArithBitLShift:
    case ArithBitXor:
    case BitURShift:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq:
    case CompareStrictEq:
    case PutByVal:
    case PutByValAlias:
    case GetByVal: {
        m_graph.doToChildrenWithCheck(node, [&](Edge& edge) {
            if (isNodeCloneable(cloneableCache, edge.node()))
                return IterationStatus::Continue;
            result = false;
            return IterationStatus::Done;
        });
        break;
    }
    default:
        result = false;
    }

    if (result)
        cloneableCache.add(node);
    return result;
}

Node* LoopUnrollingPhase::cloneNode(UncheckedKeyHashMap<Node*, Node*>& nodeClones, BasicBlock* into, Node* node)
{
    ASSERT(node);
    auto itr = nodeClones.find(node);
    if (itr != nodeClones.end())
        return itr->value;
    Node* result = cloneNodeImpl(nodeClones, into, node);
    ASSERT(result);
    nodeClones.add(node, result);
    return result;
}

Node* LoopUnrollingPhase::cloneNodeImpl(UncheckedKeyHashMap<Node*, Node*>& nodeClones, BasicBlock* into, Node* node)
{
#if ASSERT_ENABLED
    m_graph.doToChildren(node, [&](Edge& e) {
        ASSERT(e.node()->owner == node->owner);
    });
#endif

    auto cloneEdge = [&](Edge& edge) ALWAYS_INLINE_LAMBDA {
        return edge ? Edge(cloneNode(nodeClones, into, edge.node()), edge.useKind()) : Edge();
    };

    switch (node->op()) {
    case Phi: {
        // Phi nodes should already be cloned in the step 2 of unrollLoop.
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
    case Branch: {
        Node* clone = into->cloneAndAppend(m_graph, node);
        clone->setOpInfo(OpInfo(m_graph.m_branchData.add(WTFMove(*node->branchData()))));
        clone->child1() = cloneEdge(node->child1());
        return clone;
    }
    case PutByVal:
    case GetByVal:
    case PutByValAlias:
    case CheckVarargs: {
        if (node->hasVarArgs()) {
            size_t firstChild = m_graph.m_varArgChildren.size();

            uint32_t validChildrenCount = 0;
            m_graph.doToChildren(node, [&](Edge& edge) {
                m_graph.m_varArgChildren.append(cloneEdge(edge));
                ++validChildrenCount;
            });

            uint32_t expectedCount = m_graph.numChildren(node);
            for (uint32_t i = validChildrenCount; i < expectedCount; ++i)
                m_graph.m_varArgChildren.append(Edge());

            Node* clone = into->cloneAndAppend(m_graph, node);
            clone->children.setFirstChild(firstChild);
            return clone;
        }
        FALLTHROUGH;
    }
    case ValueRep:
    case DoubleRep:
    case ExitOK:
    case LoopHint:
    case GetButterfly:
    case JSConstant:
    case Jump:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq:
    case CompareStrictEq:
    case CheckStructure:
    case ArithBitNot:
    case ArrayifyToStructure:
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitRShift:
    case ArithBitLShift:
    case ArithBitXor:
    case BitURShift:
    case ArithAdd:
    case ArithSub:
    case ArithMul:
    case ArithDiv:
    case ArithMod:
    case CheckArray:
    case FilterCallLinkStatus:
    case GetLocal:
    case MovHint:
    case Flush:
    case ZombieHint:
    case SetLocal:
    case PhantomLocal:
    case Check:
    case AssertNotEmpty:
    case SetArgumentDefinitely:
    case NewArrayWithSize:
    case NewArrayWithConstantSize:
    case ValueToInt32:
    case InvalidationPoint: {
        Node* clone = into->cloneAndAppend(m_graph, node);
        clone->child1() = cloneEdge(node->child1());
        clone->child2() = cloneEdge(node->child2());
        clone->child3() = cloneEdge(node->child3());
        return clone;
    }
    default:
        dataLogLnIf(Options::verboseLoopUnrolling(), "Could not clone node: ", node, " into ", into);
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

// FIXME: Add more condition and update operations if they are profitable.
bool LoopUnrollingPhase::isSupportedConditionOp(NodeType op)
{
    switch (op) {
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq:
    case CompareStrictEq:
        return true;
    default:
        return false;
    }
}

bool LoopUnrollingPhase::isSupportedUpdateOp(NodeType op)
{
    switch (op) {
    case ArithAdd:
    case ArithSub:
    case ArithMul:
    case ArithDiv:
        return true;
    default:
        return false;
    }
}

LoopUnrollingPhase::ComparisonFunction LoopUnrollingPhase::comparisonFunction(Node* condition)
{
    switch (condition->op()) {
    case CompareLess:
        return [](auto a, auto b) { return a < b; };
    case CompareLessEq:
        return [](auto a, auto b) { return a <= b; };
    case CompareGreater:
        return [](auto a, auto b) { return a > b; };
    case CompareGreaterEq:
        return [](auto a, auto b) { return a >= b; };
    case CompareEq:
    case CompareStrictEq:
        return [](auto a, auto b) { return a == b; };
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return [](auto, auto) { return false; };
    }
}

LoopUnrollingPhase::UpdateFunction LoopUnrollingPhase::updateFunction(Node* update)
{
    switch (update->op()) {
    case ArithAdd:
        return [](auto a, auto b) { return a + b; };
    case ArithSub:
        return [](auto a, auto b) { return a - b; };
    case ArithMul:
        return [](auto a, auto b) { return a * b; };
    case ArithDiv:
        return [](auto a, auto b) { return a / b; };
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return [](auto, auto) { return CheckedInt32(); };
    }
}

}
} // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
