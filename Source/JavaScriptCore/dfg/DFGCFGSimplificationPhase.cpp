/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DFGCFGSimplificationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractState.h"
#include "DFGBasicBlock.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGValidate.h"

namespace JSC { namespace DFG {

class CFGSimplificationPhase : public Phase {
public:
    CFGSimplificationPhase(Graph& graph)
        : Phase(graph, "CFG simplification")
    {
    }
    
    bool run()
    {
        const bool extremeLogging = false;

        bool outerChanged = false;
        bool innerChanged;
        
        do {
            innerChanged = false;
            for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
                BasicBlock* block = m_graph.m_blocks[blockIndex].get();
                if (!block)
                    continue;
                ASSERT(block->isReachable);
            
                switch (m_graph[block->last()].op()) {
                case Jump: {
                    // Successor with one predecessor -> merge.
                    if (m_graph.m_blocks[m_graph.successor(block, 0)]->m_predecessors.size() == 1) {
                        ASSERT(m_graph.m_blocks[m_graph.successor(block, 0)]->m_predecessors[0]
                               == blockIndex);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                        dataLog("CFGSimplify: Jump merge on Block #%u to Block #%u.\n",
                                blockIndex, m_graph.successor(block, 0));
#endif
                        if (extremeLogging)
                            m_graph.dump();
                        mergeBlocks(blockIndex, m_graph.successor(block, 0), NoBlock);
                        innerChanged = outerChanged = true;
                        break;
                    } else {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                        dataLog("Not jump merging on Block #%u to Block #%u because predecessors = ",
                                blockIndex, m_graph.successor(block, 0));
                        for (unsigned i = 0; i < m_graph.m_blocks[m_graph.successor(block, 0)]->m_predecessors.size(); ++i) {
                            if (i)
                                dataLog(", ");
                            dataLog("#%u", m_graph.m_blocks[m_graph.successor(block, 0)]->m_predecessors[i]);
                        }
                        dataLog(".\n");
#endif
                    }
                
                    // FIXME: Block only has a jump -> remove. This is tricky though because of
                    // liveness. What we really want is to slam in a phantom at the end of the
                    // block, after the terminal. But we can't right now. :-(
                    // Idea: what if I slam the ghosties into my successor? Nope, that's
                    // suboptimal, because if my successor has multiple predecessors then we'll
                    // be keeping alive things on other predecessor edges unnecessarily.
                    // What we really need is the notion of end-of-block ghosties!
                    break;
                }
                
                case Branch: {
                    // Branch on constant -> jettison the not-taken block and merge.
                    if (m_graph[m_graph[block->last()].child1()].hasConstant()) {
                        bool condition =
                            m_graph.valueOfJSConstant(m_graph[block->last()].child1().index()).toBoolean();
                        BasicBlock* targetBlock = m_graph.m_blocks[
                            m_graph.successorForCondition(block, condition)].get();
                        if (targetBlock->m_predecessors.size() == 1) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                            dataLog("CFGSimplify: Known condition (%s) branch merge on Block #%u to Block #%u, jettisoning Block #%u.\n",
                                    condition ? "true" : "false",
                                    blockIndex, m_graph.successorForCondition(block, condition),
                                    m_graph.successorForCondition(block, !condition));
#endif
                            if (extremeLogging)
                                m_graph.dump();
                            mergeBlocks(
                                blockIndex,
                                m_graph.successorForCondition(block, condition),
                                m_graph.successorForCondition(block, !condition));
                        } else {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                            dataLog("CFGSimplify: Known condition (%s) branch->jump conversion on Block #%u to Block #%u, jettisoning Block #%u.\n",
                                    condition ? "true" : "false",
                                    blockIndex, m_graph.successorForCondition(block, condition),
                                    m_graph.successorForCondition(block, !condition));
#endif
                            if (extremeLogging)
                                m_graph.dump();
                            BlockIndex takenBlockIndex = m_graph.successorForCondition(block, condition);
                            BlockIndex notTakenBlockIndex = m_graph.successorForCondition(block, !condition);
                        
                            ASSERT(m_graph[block->last()].isTerminal());
                            CodeOrigin boundaryCodeOrigin = m_graph[block->last()].codeOrigin;
                            m_graph[block->last()].setOpAndDefaultFlags(Phantom);
                            ASSERT(m_graph[block->last()].refCount() == 1);
                        
                            jettisonBlock(blockIndex, notTakenBlockIndex, boundaryCodeOrigin);
                        
                            NodeIndex jumpNodeIndex = m_graph.size();
                            Node jump(Jump, boundaryCodeOrigin, OpInfo(takenBlockIndex));
                            jump.ref();
                            m_graph.append(jump);
                            block->append(jumpNodeIndex);
                        }
                        innerChanged = outerChanged = true;
                        break;
                    }
                    
                    if (m_graph.successor(block, 0) == m_graph.successor(block, 1)) {
                        BlockIndex targetBlockIndex = m_graph.successor(block, 0);
                        BasicBlock* targetBlock = m_graph.m_blocks[targetBlockIndex].get();
                        ASSERT(targetBlock);
                        ASSERT(targetBlock->isReachable);
                        if (targetBlock->m_predecessors.size() == 1) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                            dataLog("CFGSimplify: Branch to same successor merge on Block #%u to Block #%u.\n",
                                    blockIndex, targetBlockIndex);
#endif
                            mergeBlocks(blockIndex, targetBlockIndex, NoBlock);
                        } else {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                            dataLog("CFGSimplify: Branch->jump conversion to same successor on Block #%u to Block #%u.\n",
                                    blockIndex, targetBlockIndex);
#endif
                            ASSERT(m_graph[block->last()].isTerminal());
                            Node& branch = m_graph[block->last()];
                            ASSERT(branch.isTerminal());
                            ASSERT(branch.op() == Branch);
                            branch.setOpAndDefaultFlags(Phantom);
                            ASSERT(branch.refCount() == 1);
                            
                            Node jump(Jump, branch.codeOrigin, OpInfo(targetBlockIndex));
                            jump.ref();
                            NodeIndex jumpNodeIndex = m_graph.size();
                            m_graph.append(jump);
                            block->append(jumpNodeIndex);
                        }
                        innerChanged = outerChanged = true;
                        break;
                    }
                    
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                    dataLog("Not branch simplifying on Block #%u because the successors differ and the condition is not known.\n",
                            blockIndex);
#endif
                
                    // Branch to same destination -> jump.
                    // FIXME: this will currently not be hit because of the lack of jump-only
                    // block simplification.
                    
                    break;
                }
                
                default:
                    break;
                }
            }
            
            if (innerChanged) {
                // Here's the reason for this pass:
                // Blocks: A, B, C, D, E, F
                // A -> B, C
                // B -> F
                // C -> D, E
                // D -> F
                // E -> F
                //
                // Assume that A's branch is determined to go to B. Then the rest of this phase
                // is smart enough to simplify down to:
                // A -> B
                // B -> F
                // C -> D, E
                // D -> F
                // E -> F
                //
                // We will also merge A and B. But then we don't have any other mechanism to
                // remove D, E as predecessors for F. Worse, the rest of this phase does not
                // know how to fix the Phi functions of F to ensure that they no longer refer
                // to variables in D, E. In general, we need a way to handle Phi simplification
                // upon:
                // 1) Removal of a predecessor due to branch simplification. The branch
                //    simplifier already does that.
                // 2) Invalidation of a predecessor because said predecessor was rendered
                //    unreachable. We do this here.
                //
                // This implies that when a block is unreachable, we must inspect its
                // successors' Phi functions to remove any references from them into the
                // removed block.
                
                m_graph.resetReachability();

                for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
                    BasicBlock* block = m_graph.m_blocks[blockIndex].get();
                    if (!block)
                        continue;
                    if (block->isReachable)
                        continue;
                    
                    killUnreachable(blockIndex);
                }
            }
            
            validate(m_graph);
        } while (innerChanged);
        
        return outerChanged;
    }

private:
    void killUnreachable(BlockIndex blockIndex)
    {
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        
        ASSERT(block);
        ASSERT(!block->isReachable);
        
        // 1) Remove references from other blocks to this block.
        for (unsigned i = m_graph.numSuccessors(block); i--;)
            fixPhis(blockIndex, m_graph.successor(block, i));
        
        // 2) Kill the block
        m_graph.m_blocks[blockIndex].clear();
    }
    
    void keepOperandAlive(BasicBlock* block, CodeOrigin codeOrigin, int operand)
    {
        NodeIndex nodeIndex = block->variablesAtTail.operand(operand);
        if (nodeIndex == NoNode)
            return;
        if (m_graph[nodeIndex].variableAccessData()->isCaptured())
            return;
        if (m_graph[nodeIndex].op() == SetLocal)
            nodeIndex = m_graph[nodeIndex].child1().index();
        Node& node = m_graph[nodeIndex];
        if (!node.shouldGenerate())
            return;
        ASSERT(m_graph[nodeIndex].op() != SetLocal);
        NodeIndex phantomNodeIndex = m_graph.size();
        Node phantom(Phantom, codeOrigin, nodeIndex);
        m_graph.append(phantom);
        m_graph.ref(phantomNodeIndex);
        block->append(phantomNodeIndex);
    }
    
    void fixPossibleGetLocal(BasicBlock* block, Edge& edge, bool changeRef)
    {
        Node& child = m_graph[edge];
        if (child.op() != GetLocal)
            return;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("    Considering GetLocal at @%u, local r%d.\n", edge.index(), child.local());
#endif
        if (child.variableAccessData()->isCaptured()) {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("        It's captured.\n");
#endif
            return;
        }
        NodeIndex originalNodeIndex = block->variablesAtTail.operand(child.local());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("        Dealing with original @%u.\n", originalNodeIndex);
#endif
        ASSERT(originalNodeIndex != NoNode);
        Node* originalNode = &m_graph[originalNodeIndex];
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("        Original has local r%d.\n", originalNode->local());
#endif
        ASSERT(child.local() == originalNode->local());
        if (changeRef)
            ASSERT(originalNode->shouldGenerate());
        // Possibilities:
        // SetLocal -> the secondBlock is getting the value of something that is immediately
        //     available in the first block with a known NodeIndex.
        // GetLocal -> the secondBlock is getting the value of something that the first
        //     block also gets.
        // Phi -> the secondBlock is asking for keep-alive on an operand that the first block
        //     was also asking for keep-alive on.
        // SetArgument -> the secondBlock is asking for keep-alive on an operand that the
        //     first block was keeping alive by virtue of the firstBlock being the root and
        //     the operand being an argument.
        // Flush -> the secondBlock is asking for keep-alive on an operand that the first
        //     block was forcing to be alive, so the second block should refer child of
        //     the flush.
        if (originalNode->op() == Flush) {
            originalNodeIndex = originalNode->child1().index();
            originalNode = &m_graph[originalNodeIndex];
        }
        switch (originalNode->op()) {
        case SetLocal: {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("        It's a SetLocal.\n");
#endif
            m_graph.changeIndex(edge, originalNode->child1().index(), changeRef);
            break;
        }
        case GetLocal: {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("        It's a GetLocal.\n");
#endif
            m_graph.changeIndex(edge, originalNodeIndex, changeRef);
            break;
        }
        case Phi:
        case SetArgument: {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("        It's Phi/SetArgument.\n");
#endif
            // Keep the GetLocal!
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void jettisonBlock(BlockIndex blockIndex, BlockIndex jettisonedBlockIndex, CodeOrigin boundaryCodeOrigin)
    {
        BasicBlock* block = m_graph.m_blocks[blockIndex].get();
        BasicBlock* jettisonedBlock = m_graph.m_blocks[jettisonedBlockIndex].get();
        
        for (size_t i = 0; i < jettisonedBlock->variablesAtHead.numberOfArguments(); ++i)
            keepOperandAlive(block, boundaryCodeOrigin, argumentToOperand(i));
        for (size_t i = 0; i < jettisonedBlock->variablesAtHead.numberOfLocals(); ++i)
            keepOperandAlive(block, boundaryCodeOrigin, i);
        
        fixJettisonedPredecessors(blockIndex, jettisonedBlockIndex);
    }
    
    void fixPhis(BlockIndex sourceBlockIndex, BlockIndex destinationBlockIndex)
    {
        BasicBlock* sourceBlock = m_graph.m_blocks[sourceBlockIndex].get();
        BasicBlock* destinationBlock = m_graph.m_blocks[destinationBlockIndex].get();
        if (!destinationBlock) {
            // If we're trying to kill off the source block and the destination block is already
            // dead, then we're done!
            return;
        }
        for (size_t i = 0; i < destinationBlock->phis.size(); ++i) {
            NodeIndex phiNodeIndex = destinationBlock->phis[i];
            Node& phiNode = m_graph[phiNodeIndex];
            NodeIndex myNodeIndex = sourceBlock->variablesAtTail.operand(phiNode.local());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("Considering removing reference from phi @%u to @%u on local r%d:",
                    phiNodeIndex, myNodeIndex, phiNode.local());
#endif
            if (myNodeIndex == NoNode) {
                // This will happen if there is a phi in the destination that refers into
                // the destination itself.
                continue;
            }
            Node& myNode = m_graph[myNodeIndex];
            if (myNode.op() == GetLocal)
                myNodeIndex = myNode.child1().index();
            for (unsigned j = 0; j < AdjacencyList::Size; ++j)
                removePotentiallyDeadPhiReference(myNodeIndex, phiNode, j, sourceBlock->isReachable);
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
            dataLog("\n");
#endif
        }
    }
    
    void fixJettisonedPredecessors(BlockIndex blockIndex, BlockIndex jettisonedBlockIndex)
    {
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog("Fixing predecessors and phis due to jettison of Block #%u from Block #%u.\n",
                jettisonedBlockIndex, blockIndex);
#endif
        BasicBlock* jettisonedBlock = m_graph.m_blocks[jettisonedBlockIndex].get();
        for (unsigned i = 0; i < jettisonedBlock->m_predecessors.size(); ++i) {
            if (jettisonedBlock->m_predecessors[i] != blockIndex)
                continue;
            jettisonedBlock->m_predecessors[i] = jettisonedBlock->m_predecessors.last();
            jettisonedBlock->m_predecessors.removeLast();
            break;
        }
        
        fixPhis(blockIndex, jettisonedBlockIndex);
    }
    
    void removePotentiallyDeadPhiReference(NodeIndex myNodeIndex, Node& phiNode, unsigned edgeIndex, bool changeRef)
    {
        if (phiNode.children.child(edgeIndex).indexUnchecked() != myNodeIndex)
            return;
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        dataLog(" Removing reference at child %u.", edgeIndex);
#endif
        if (changeRef && phiNode.shouldGenerate())
            m_graph.deref(myNodeIndex);
        phiNode.children.removeEdgeFromBag(edgeIndex);
    }
    
    struct OperandSubstitution {
        OperandSubstitution()
            : oldChild(NoNode)
            , newChild(NoNode)
        {
        }
        
        explicit OperandSubstitution(NodeIndex oldChild)
            : oldChild(oldChild)
            , newChild(oldChild)
        {
        }
        
        OperandSubstitution(NodeIndex oldChild, NodeIndex newChild)
            : oldChild(oldChild)
            , newChild(newChild)
        {
            ASSERT((oldChild == NoNode) == (newChild == NoNode));
        }
        
        void dump(FILE* out)
        {
            if (oldChild == NoNode)
                fprintf(out, "-");
            else
                fprintf(out, "@%u -> @%u", oldChild, newChild);
        }
        
        NodeIndex oldChild;
        NodeIndex newChild;
    };
    
    NodeIndex skipGetLocal(NodeIndex nodeIndex)
    {
        if (nodeIndex == NoNode)
            return NoNode;
        Node& node = m_graph[nodeIndex];
        if (node.op() == GetLocal)
            return node.child1().index();
        return nodeIndex;
    }
    
    void recordPossibleIncomingReference(
        BasicBlock* secondBlock, Operands<OperandSubstitution>& substitutions, int operand)
    {
        substitutions.operand(operand) = OperandSubstitution(
            skipGetLocal(secondBlock->variablesAtTail.operand(operand)));
    }
    
    void recordNewTarget(Operands<OperandSubstitution>& substitutions, int operand, NodeIndex nodeIndex)
    {
        ASSERT(m_graph[nodeIndex].op() == SetLocal
               || m_graph[nodeIndex].op() == SetArgument
               || m_graph[nodeIndex].op() == Flush
               || m_graph[nodeIndex].op() == Phi);
        substitutions.operand(operand).newChild = nodeIndex;
    }
    
    void fixTailOperand(
        BasicBlock* firstBlock, BasicBlock* secondBlock, int operand,
        Operands<OperandSubstitution>& substitutions)
    {
        NodeIndex atSecondTail = secondBlock->variablesAtTail.operand(operand);
        
        if (atSecondTail == NoNode) {
            // If the variable is dead at the end of the second block, then do nothing; essentially
            // this means that we want the tail state to reflect whatever the first block did.
            return;
        }

        Node& secondNode = m_graph[atSecondTail];
        
        switch (secondNode.op()) {
        case SetLocal:
        case Flush: {
            // The second block did interesting things to the variables, so update the tail
            // accordingly.
            firstBlock->variablesAtTail.operand(operand) = atSecondTail;
            break;
        }
            
        case Phi: {
            // Keep what was in the first block.
            ASSERT(firstBlock->variablesAtTail.operand(operand) != NoNode);
            recordNewTarget(substitutions, operand, skipGetLocal(firstBlock->variablesAtTail.operand(operand)));
            break;
        }

        case GetLocal: {
            // If it's a GetLocal on a captured var, then definitely keep what was
            // in the second block. In particular, it's possible that the first
            // block doesn't even know about this variable.
            if (secondNode.variableAccessData()->isCaptured()) {
                firstBlock->variablesAtTail.operand(operand) = atSecondTail;
                recordNewTarget(substitutions, operand, secondNode.child1().index());
                break;
            }
            
            // It's possible that the second block had a GetLocal and the first block
            // had a SetArgument or a Phi. Then update the tail. Otherwise keep what was in the
            // first block.
            NodeIndex atFirstTail = firstBlock->variablesAtTail.operand(operand);
            ASSERT(atFirstTail != NoNode);
            switch (m_graph[atFirstTail].op()) {
            case SetArgument:
            case Phi:
                firstBlock->variablesAtTail.operand(operand) = atSecondTail;
                recordNewTarget(substitutions, operand, secondNode.child1().index());
                break;

            default:
                // Keep what was in the first block, and adjust the substitution to account for
                // the fact that successors will refer to the child of the GetLocal.
                ASSERT(firstBlock->variablesAtTail.operand(operand) != NoNode);
                recordNewTarget(substitutions, operand, skipGetLocal(firstBlock->variablesAtTail.operand(operand)));
                break;
            }
            break;
        }
            
        default:
            ASSERT_NOT_REACHED();
        }
    }
    
    void mergeBlocks(
        BlockIndex firstBlockIndex, BlockIndex secondBlockIndex, BlockIndex jettisonedBlockIndex)
    {
        // This will add all of the nodes in secondBlock to firstBlock, but in so doing
        // it will also ensure that any GetLocals from the second block that refer to
        // SetLocals in the first block are relinked. If jettisonedBlock is not NoBlock,
        // then Phantoms are inserted for anything that the jettisonedBlock would have
        // kept alive.
        
        BasicBlock* firstBlock = m_graph.m_blocks[firstBlockIndex].get();
        BasicBlock* secondBlock = m_graph.m_blocks[secondBlockIndex].get();
        
        // Remove the terminal of firstBlock since we don't need it anymore. Well, we don't
        // really remove it; we actually turn it into a Phantom.
        ASSERT(m_graph[firstBlock->last()].isTerminal());
        CodeOrigin boundaryCodeOrigin = m_graph[firstBlock->last()].codeOrigin;
        m_graph[firstBlock->last()].setOpAndDefaultFlags(Phantom);
        ASSERT(m_graph[firstBlock->last()].refCount() == 1);
        
        if (jettisonedBlockIndex != NoBlock) {
            BasicBlock* jettisonedBlock = m_graph.m_blocks[jettisonedBlockIndex].get();
            
            // Time to insert ghosties for things that need to be kept alive in case we OSR
            // exit prior to hitting the firstBlock's terminal, and end up going down a
            // different path than secondBlock.
            
            for (size_t i = 0; i < jettisonedBlock->variablesAtHead.numberOfArguments(); ++i)
                keepOperandAlive(firstBlock, boundaryCodeOrigin, argumentToOperand(i));
            for (size_t i = 0; i < jettisonedBlock->variablesAtHead.numberOfLocals(); ++i)
                keepOperandAlive(firstBlock, boundaryCodeOrigin, i);
        }
        
        for (size_t i = 0; i < secondBlock->phis.size(); ++i)
            firstBlock->phis.append(secondBlock->phis[i]);

        // Before we start changing the second block's graph, record what nodes would
        // be referenced by successors of the second block.
        Operands<OperandSubstitution> substitutions(
            secondBlock->variablesAtTail.numberOfArguments(),
            secondBlock->variablesAtTail.numberOfLocals());
        for (size_t i = 0; i < secondBlock->variablesAtTail.numberOfArguments(); ++i)
            recordPossibleIncomingReference(secondBlock, substitutions, argumentToOperand(i));
        for (size_t i = 0; i < secondBlock->variablesAtTail.numberOfLocals(); ++i)
            recordPossibleIncomingReference(secondBlock, substitutions, i);

        for (size_t i = 0; i < secondBlock->size(); ++i) {
            NodeIndex nodeIndex = secondBlock->at(i);
            Node& node = m_graph[nodeIndex];
            
            bool childrenAlreadyFixed = false;
            
            switch (node.op()) {
            case Phantom: {
                if (!node.child1())
                    break;
                
                ASSERT(node.shouldGenerate());
                Node& possibleLocalOp = m_graph[node.child1()];
                if (possibleLocalOp.op() != GetLocal
                    && possibleLocalOp.hasLocal()
                    && !possibleLocalOp.variableAccessData()->isCaptured()) {
                    NodeIndex setLocalIndex =
                        firstBlock->variablesAtTail.operand(possibleLocalOp.local());
                    Node& setLocal = m_graph[setLocalIndex];
                    if (setLocal.op() == SetLocal) {
                        m_graph.changeEdge(node.children.child1(), setLocal.child1());
                        ASSERT(!node.child2());
                        ASSERT(!node.child3());
                        childrenAlreadyFixed = true;
                    }
                }
                break;
            }
                
            case Flush:
            case GetLocal: {
                // A Flush could use a GetLocal, SetLocal, SetArgument, or a Phi.
                // If it uses a GetLocal, it'll be taken care of below. If it uses a
                // SetLocal or SetArgument, then it must be using a node from the
                // same block. But if it uses a Phi, then we should redirect it to
                // use whatever the first block advertised as a tail operand.
                // Similarly for GetLocal; it could use any of those except for
                // GetLocal. If it uses a Phi then it should be redirected to use a
                // Phi from the tail operand.
                if (m_graph[node.child1()].op() != Phi)
                    break;
                
                NodeIndex atFirstIndex = firstBlock->variablesAtTail.operand(node.local());
                m_graph.changeEdge(node.children.child1(), Edge(skipGetLocal(atFirstIndex)), node.shouldGenerate());
                childrenAlreadyFixed = true;
                break;
            }
                
            default:
                break;
            }
            
            if (!childrenAlreadyFixed) {
                bool changeRef = node.shouldGenerate();
            
                // If the child is a GetLocal, then we might like to fix it.
                if (node.flags() & NodeHasVarArgs) {
                    for (unsigned childIdx = node.firstChild();
                         childIdx < node.firstChild() + node.numChildren();
                         ++childIdx)
                        fixPossibleGetLocal(firstBlock, m_graph.m_varArgChildren[childIdx], changeRef);
                } else if (!!node.child1()) {
                    fixPossibleGetLocal(firstBlock, node.children.child1(), changeRef);
                    if (!!node.child2()) {
                        fixPossibleGetLocal(firstBlock, node.children.child2(), changeRef);
                        if (!!node.child3())
                            fixPossibleGetLocal(firstBlock, node.children.child3(), changeRef);
                    }
                }
            }

            firstBlock->append(nodeIndex);
        }
        
        ASSERT(m_graph[firstBlock->last()].isTerminal());
        
        // Fix the predecessors of my new successors. This is tricky, since we are going to reset
        // all predecessors anyway due to reachability analysis. But we need to fix the
        // predecessors eagerly to ensure that we know what they are in case the next block we
        // consider in this phase wishes to query the predecessors of one of the blocks we
        // affected.
        for (unsigned i = m_graph.numSuccessors(firstBlock); i--;) {
            BasicBlock* successor = m_graph.m_blocks[m_graph.successor(firstBlock, i)].get();
            for (unsigned j = 0; j < successor->m_predecessors.size(); ++j) {
                if (successor->m_predecessors[j] == secondBlockIndex)
                    successor->m_predecessors[j] = firstBlockIndex;
            }
        }
        
        // Fix the predecessors of my former successors. Again, we'd rather not do this, but it's
        // an unfortunate necessity. See above comment.
        if (jettisonedBlockIndex != NoBlock)
            fixJettisonedPredecessors(firstBlockIndex, jettisonedBlockIndex);
        
        // Fix up the variables at tail.
        for (size_t i = 0; i < secondBlock->variablesAtHead.numberOfArguments(); ++i)
            fixTailOperand(firstBlock, secondBlock, argumentToOperand(i), substitutions);
        for (size_t i = 0; i < secondBlock->variablesAtHead.numberOfLocals(); ++i)
            fixTailOperand(firstBlock, secondBlock, i, substitutions);
        
        // Fix up the references from our new successors.
        for (unsigned i = m_graph.numSuccessors(firstBlock); i--;) {
            BasicBlock* successor = m_graph.m_blocks[m_graph.successor(firstBlock, i)].get();
            for (unsigned j = 0; j < successor->phis.size(); ++j) {
                NodeIndex phiNodeIndex = successor->phis[j];
                Node& phiNode = m_graph[phiNodeIndex];
                bool changeRef = phiNode.shouldGenerate();
                OperandSubstitution substitution = substitutions.operand(phiNode.local());
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
                dataLog("    Performing operand substitution @%u -> @%u.\n",
                        substitution.oldChild, substitution.newChild);
#endif
                if (!phiNode.child1())
                    continue;
                if (phiNode.child1().index() == substitution.oldChild)
                    m_graph.changeIndex(phiNode.children.child1(), substitution.newChild, changeRef);
                if (!phiNode.child2())
                    continue;
                if (phiNode.child2().index() == substitution.oldChild)
                    m_graph.changeIndex(phiNode.children.child2(), substitution.newChild, changeRef);
                if (!phiNode.child3())
                    continue;
                if (phiNode.child3().index() == substitution.oldChild)
                    m_graph.changeIndex(phiNode.children.child3(), substitution.newChild, changeRef);
            }
        }
        
        firstBlock->valuesAtTail = secondBlock->valuesAtTail;
        
        m_graph.m_blocks[secondBlockIndex].clear();
    }
};

bool performCFGSimplification(Graph& graph)
{
    SamplingRegion samplingRegion("DFG CFG Simplification Phase");
    return runPhase<CFGSimplificationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


