/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "DFGBlockInsertionSet.h"
#include "DFGCFAPhase.h"
#include "DFGCloneHelper.h"
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
        bool isOperandConstant() const { return std::holds_alternative<CheckedInt32>(operand); }

        Node* condition() const
        {
            if (tail && tail->terminal()->isBranch())
                return tail->terminal()->child1().node();
            return nullptr;
        }

        bool isInductionVariable(Node* node) { return node->operand() == inductionVariable->operand(); }
        void dump(PrintStream& out) const;

        const NaturalLoop* loop { nullptr };
        BasicBlock* preHeader { nullptr };
        BasicBlock* tail { nullptr };
        BasicBlock* next { nullptr };

        // for (i = initialValue; condition(i, operand); i = update(i, updateValue)) { ... }
        Node* inductionVariable { nullptr };
        CheckedInt32 initialValue { INT_MIN };
        std::variant<Node*, CheckedInt32> operand { INT_MIN };
        Node* update { nullptr };
        CheckedInt32 updateValue { INT_MIN };
        CheckedUint32 iterationCount { 0 };

        std::optional<bool> inverseCondition { };
    };

    LoopUnrollingPhase(Graph& graph)
        : Phase(graph, "Loop Unrolling"_s)
        , m_blockInsertionSet(graph)
    {
    }

    bool run()
    {
        dataLogIf(Options::verboseLoopUnrolling(), "Graph before Loop Unrolling Phase:\n", m_graph);

        uint32_t unrolledCount = 0;
        while (true) {
            auto loops = populateCandidateLoops();
            if (loops.isEmpty() || unrolledCount >= Options::maxLoopUnrollingCount())
                break;

            bool unrolled = false;
            for (auto [loop, depth] : loops) {
                if (!loop)
                    break;
                BasicBlock* header = loop->header().node();
                if (m_unrolledLoopHeaders.contains(header)) {
                    dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping the loop with header ", header, " since it's already unrolled. Looking for anther candidate.");
                    continue;
                }
                if (tryUnroll(loop)) {
                    unrolled = true;
                    ++unrolledCount;
                    break;
                }
            }
            if (!unrolled)
                break;
        }

        dataLogLnIf(Options::verboseLoopUnrolling(), "Successfully unrolled ", unrolledCount, " loops.");
        return !!unrolledCount;
    }

    Vector<std::tuple<const NaturalLoop*, int32_t>, 16> populateCandidateLoops()
    {
        m_graph.ensureCPSNaturalLoops();

        uint32_t loopCount = m_graph.m_cpsNaturalLoops->numLoops();
        Vector<std::tuple<const NaturalLoop*, int32_t>, 16> loops(loopCount, std::tuple { nullptr, INT_MIN });
        for (uint32_t loopIndex = loopCount; loopIndex--;) {
            const NaturalLoop& loop = m_graph.m_cpsNaturalLoops->loop(loopIndex);
            ASSERT(loop.index() == loopIndex && std::get<1>(loops[loopIndex]) == INT_MIN);

            int32_t depth = 0;
            const NaturalLoop* current = &loop;
            while (current) {
                int32_t cachedDepth = std::get<1>(loops[current->index()]);
                if (cachedDepth != INT_MIN) {
                    depth += cachedDepth;
                    break;
                }
                ++depth;
                current = m_graph.m_cpsNaturalLoops->innerMostOuterLoop(*current);
            }
            loops[loopIndex] = std::tuple { &loop, depth };
        }
        std::sort(loops.begin(), loops.end(), [&](const auto& lhs, const auto& rhs) {
            return std::get<1>(lhs) > std::get<1>(rhs);
        });
        return loops;
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

        BasicBlock* header = data.header();
        unrollLoop(data);

        dataLogIf(Options::verboseLoopUnrolling(), "\tGraph after Loop Unrolling for loop\n", m_graph);
        dataLogLnIf(Options::printEachUnrolledLoop(), "\tIn function ", m_graph.m_codeBlock->inferredName(), ", successfully unrolled the loop header=", *header);

        m_unrolledLoopHeaders.add(header);
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

        // PreHeader
        //  |
        // Header <----------
        //  |               |
        // Body             |
        //  |    True/False |
        // Tail -------------
        //  | False/True
        // Next
        //
        // Determine if the condition should be inverted based on whether the "not taken" branch points into the loop.
        Node* terminal = tail->terminal();
        ASSERT(terminal->op() == Branch);
        if (data.loop->contains(terminal->branchData()->notTaken.block)) {
            // If tail's branch is both jumping into the loop, then it is not a tail.
            // This happens when we already unrolled this loop before.
            if (data.loop->contains(terminal->branchData()->taken.block))
                return false;
            data.inverseCondition = true;
        } else
            data.inverseCondition = false;

        return true;
    }

    bool isSupportedConditionOp(NodeType op);
    bool isSupportedUpdateOp(NodeType op);

    ComparisonFunction comparisonFunction(Node* condition, bool inverseCondition);
    UpdateFunction updateFunction(Node* update);

    bool identifyInductionVariable(LoopData& data)
    {
        Node* condition = data.condition();
        ASSERT(condition);
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
            if (operand->isInt32Constant() && operand.useKind() == Int32Use)
                data.operand = operand->asInt32();
            else if (Options::usePartialLoopUnrolling())
                data.operand = operand.node();
            else
                return false;
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

        // Compute the number of iterations in the loop, if it has a constant iteration count.
        if (data.isOperandConstant()) {
            CheckedUint32 iterationCount = 0;
            auto compare = comparisonFunction(condition, data.inverseCondition.value());
            auto update = updateFunction(data.update);
            for (CheckedInt32 i = data.initialValue; compare(i, std::get<CheckedInt32>(data.operand));) {
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
                if (iterationCount.hasOverflowed()) {
                    dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since the iteration count overflowed after the update");
                    return false;
                }
            }
            if (!iterationCount) {
                dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since the iteration count is zero");
                return false;
            }
            data.iterationCount = iterationCount;
        }
        return true;
    }

    bool shouldUnrollLoop(LoopData& data)
    {
        if (Options::disallowLoopUnrollingForNonInnermost() && !data.loop->isInnerMostLoop())
            return false;

        uint32_t materialNodeCount = 0;
        uint32_t maxLoopUnrollingBodyNodeSize = data.isOperandConstant() ? Options::maxLoopUnrollingBodyNodeSize() : Options::maxPartialLoopUnrollingBodyNodeSize();
        for (uint32_t i = 0; i < data.loopSize(); ++i) {
            BasicBlock* body = data.loopBody(i);
            if (!body->isReachable) {
                dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " since block ", *body, " is not reachable");
                return false;
            }

            // FIXME: We may also need to check whether the block is valid using CFA.
            // If the block is unreachable or invalid in the CFG, we can directly
            // ignore the loop, avoiding unnecessary cloneability checks for nodes in invalid blocks.

            HashSet<Node*> cloneableCache;
            for (Node* node : *body) {
                // Count only nodes that would generate real code. Helps avoid overestimating loop body size due to Phantom or ExitOK, etc.
                if (isMaterialNode(node))
                    ++materialNodeCount;

                if (materialNodeCount > maxLoopUnrollingBodyNodeSize) {
                    dataLogLnIf(Options::verboseLoopUnrolling(), "Skipping loop with header ", *data.header(), " and loop node count=", materialNodeCount, " since maxLoopUnrollingBodyNodeSize =", Options::maxLoopUnrollingBodyNodeSize());
                    return false;
                }

                if (!CloneHelper::isNodeCloneable(m_graph, cloneableCache, node)) {
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
        dataLogLnIf(Options::verboseLoopUnrolling(), " unroll ", data.isOperandConstant() ?  " with constant iterations" : " partially");

        BasicBlock* const header = data.header();
        BasicBlock* const tail = data.tail;
        BasicBlock* const next = data.next;

        dataLogLnIf(Options::verboseLoopUnrolling(), "tailTerminalOriginSemantic ", tail->terminal()->origin.semantic);

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

        //  ### Constant ###         ### Partial ###
        //
        //  PreHeader                 PreHeader
        //   |                          |
        //  BodyGraph_0 <----       -> BodyGraph_0 --
        //   |    |      |  |       |   |           |
        //   |T   --------  |F      |T  |T          |F
        //   |       F      |       |   |           |
        //  BodyGraph_1 -----       -- BodyGraph_1  |
        //   |T                         |F          |
        //  Next                       Next <--------
        auto convertTailBranchToNextJump = [&](BasicBlock* tail, BasicBlock* taken) {
            BasicBlock* notTaken = next;
            auto* terminal = tail->terminal();
            if (data.isOperandConstant()) {
                // Why don't we use Jump instead of Branch? The reason is tail's original terminal was Branch.
                // If we change this from Branch to Jump, we need to preserve how variables are used (via GetLocal, MovHint, SetLocal)
                // not to confuse these variables liveness, it involves what blocks are used for successors of this tail block.
                // Here, we can simplify the problem by using Branch and using the original "header" successors as never-taken branch.
                // FTL's subsequent pass will optimize this and convert this Branch to Jump and/or eliminate this Branch and merge
                // multiple blocks easily since its condition is constant boolean True. But we do not need to do the complicated analysis
                // here. So let's just use Branch.
                ASSERT(tail->terminal()->isBranch());
                auto* constant = m_graph.addNode(SpecBoolean, JSConstant, tail->terminal()->origin, OpInfo(m_graph.freezeStrong(jsBoolean(true))));
                tail->insertBeforeTerminal(constant);
                terminal->child1() = Edge(constant, KnownBooleanUse);
                notTaken = header;
            }

            terminal->branchData()->taken = BranchTarget(taken);
            terminal->branchData()->notTaken = BranchTarget(notTaken);
        };

#if ASSERT_ENABLED
        m_graph.initializeNodeOwners(); // This is only used for the debug assertion in cloneNodeImpl.
#endif

        CloneHelper helper(m_graph, nodeClones);
        BasicBlock* taken = next;
        uint32_t cloneCount = 0;
        if (data.isOperandConstant()) {
            ASSERT(!data.iterationCount.hasOverflowed() && data.iterationCount);
            cloneCount = data.iterationCount - 1;
        } else
            cloneCount = Options::maxPartialLoopUnrollingIterationCount() - 1;
        while (cloneCount--) {
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
                for (Node* node : *body)
                    helper.cloneNode(clone, node);

                // 4. Clone variables and tail and head.
                clone->variablesAtTail = body->variablesAtTail;
                replaceOperands(clone->variablesAtTail);
                clone->variablesAtHead = body->variablesAtHead;
                replaceOperands(clone->variablesAtHead);

                // 5. Clone successors. (predecessors will be fixed in resetReachability below)
                if (body == tail) {
                    ASSERT(tail->terminal()->isBranch());
                    bool isTakenNextInPartialMode = taken == next && !data.isOperandConstant();
                    convertTailBranchToNextJump(clone, isTakenNextInPartialMode ? header : taken);
                } else {
                    for (uint32_t i = 0; i < body->numSuccessors(); ++i) {
                        auto& successor = clone->successor(i);
                        ASSERT(successor == body->successor(i));
                        if (data.loop->contains(successor))
                            successor = blockClones.get(successor);
                    }
                }
            }

            taken = blockClones.get(header);
        }

        // 6. Replace the original loop tail branch with a jump to the last header clone.
        convertTailBranchToNextJump(tail, taken);

        // Done clone.
        if (!m_blockInsertionSet.execute()) {
            m_graph.invalidateCFG();
            m_graph.dethread();
        }
        m_graph.resetReachability();
        m_graph.killUnreachableBlocks();
        ASSERT(m_graph.m_form == LoadStore);
    }

    // Returns true if the node would emit code when lowered to B3.
    // Used to estimate unrolling cost more precisely, skipping Phantom-like ops.
    bool isMaterialNode(Node*);

private:
    BlockInsertionSet m_blockInsertionSet;
    UncheckedKeyHashSet<BasicBlock*> m_unrolledLoopHeaders;
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
    if (tail) {
        out.print(*tail, " with branch condition=");
        Node* condition = this->condition();
        if (condition)
            out.print(condition, "<", condition->op(), ">");
        else
            out.print("<null>");
    } else
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
    if (isOperandConstant())
        out.print("operand=", std::get<CheckedInt32>(operand), ", ");
    else
        out.print("operand=", std::get<Node*>(operand), ", ");

    out.print("update=");
    if (update)
        out.print(update, "<", update->op(), ">");
    else
        out.print("<null>");
    out.print(", ");

    out.print("updateValue=", updateValue, ", ");

    out.print("iterationCount=", iterationCount, ", ");

    out.print("inverseCondition=", inverseCondition);
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

LoopUnrollingPhase::ComparisonFunction LoopUnrollingPhase::comparisonFunction(Node* condition, bool inverseCondition)
{
    static const ComparisonFunction less = [](auto a, auto b) { return a < b; };
    static const ComparisonFunction lessEq = [](auto a, auto b) { return a <= b; };
    static const ComparisonFunction greater = [](auto a, auto b) { return a > b; };
    static const ComparisonFunction greaterEq = [](auto a, auto b) { return a >= b; };
    static const ComparisonFunction equal = [](auto a, auto b) { return a == b; };
    static const ComparisonFunction notEqual = [](auto a, auto b) { return a != b; };

    switch (condition->op()) {
    case CompareLess:
        return inverseCondition ? greaterEq : less;
    case CompareLessEq:
        return inverseCondition ? greater : lessEq;
    case CompareGreater:
        return inverseCondition ? lessEq : greater;
    case CompareGreaterEq:
        return inverseCondition ? less : greaterEq;
    case CompareEq:
    case CompareStrictEq:
        return inverseCondition ? notEqual : equal;
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

bool LoopUnrollingPhase::isMaterialNode(Node* node)
{
    switch (node->op()) {
    // This aligns with DFGDCEPhase.
    case Check:
    case Phantom:
        if (node->children.isEmpty())
            return false;
        break;
    case CheckVarargs: {
        bool isEmpty = true;
        m_graph.doToChildren(node, [&] (Edge edge) {
            isEmpty &= !edge;
        });
        if (isEmpty)
            return false;
        break;
    }
    // This aligns with LowerDFGToB3::compileNode.
    case PhantomLocal:
    case MovHint:
    case ZombieHint:
    case ExitOK:
    case PhantomNewObject:
    case PhantomNewArrayWithConstantSize:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewInternalFieldObject:
    case PhantomCreateActivation:
    case PhantomDirectArguments:
    case PhantomCreateRest:
    case PhantomSpread:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
    case PhantomClonedArguments:
    case PhantomNewRegExp:
    case PutHint:
    case BottomValue:
    case KillStack:
    case InitializeEntrypointArguments:
        return false;
    default:
        break;
    }
    return true;
}

}
} // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
