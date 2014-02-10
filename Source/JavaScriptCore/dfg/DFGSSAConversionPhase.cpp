/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGSSAConversionPhase.h"

#include "DFGBasicBlockInlines.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class SSAConversionPhase : public Phase {
    static const bool verbose = false;
    static const bool dumpGraph = false;
    
public:
    SSAConversionPhase(Graph& graph)
        : Phase(graph, "SSA conversion")
        , m_insertionSet(graph)
        , m_changed(false)
    {
    }
    
    bool run()
    {
        RELEASE_ASSERT(m_graph.m_form == ThreadedCPS);
        
        if (dumpGraph) {
            dataLog("Graph dump at top of SSA conversion:\n");
            m_graph.dump();
        }
        
        // Figure out which SetLocal's need flushing. Need to do this while the
        // Phi graph is still intact.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                Node* node = block->at(nodeIndex);
                if (node->op() != Flush)
                    continue;
                addFlushedLocalOp(node);
            }
        }
        while (!m_flushedLocalOpWorklist.isEmpty()) {
            Node* node = m_flushedLocalOpWorklist.takeLast();
            ASSERT(m_flushedLocalOps.contains(node));
            DFG_NODE_DO_TO_CHILDREN(m_graph, node, addFlushedLocalEdge);
        }
        
        // Eliminate all duplicate or self-pointing Phi edges. This means that
        // we transform:
        //
        // p: Phi(@n1, @n2, @n3)
        //
        // into:
        //
        // p: Phi(@x)
        //
        // if each @ni in {@n1, @n2, @n3} is either equal to @p to is equal
        // to @x, for exactly one other @x. Additionally, trivial Phis (i.e.
        // p: Phi(@x)) are forwarded, so that if have an edge to such @p, we
        // replace it with @x. This loop does this for Phis only; later we do
        // such forwarding for Phi references found in other nodes.
        //
        // See Aycock and Horspool in CC'00 for a better description of what
        // we're doing here.
        do {
            m_changed = false;
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;
                for (unsigned phiIndex = block->phis.size(); phiIndex--;) {
                    Node* phi = block->phis[phiIndex];
                    if (phi->variableAccessData()->isCaptured())
                        continue;
                    forwardPhiChildren(phi);
                    deduplicateChildren(phi);
                }
            }
        } while (m_changed);
        
        // For each basic block, for each local live at the head of that block,
        // figure out what node we should be referring to instead of that local.
        // If it turns out to be a non-trivial Phi, make sure that we create an
        // SSA Phi and Upsilons in predecessor blocks. We reuse
        // BasicBlock::variablesAtHead for tracking which nodes to refer to.
        Operands<bool> nonTrivialPhis(OperandsLike, m_graph.block(0)->variablesAtHead);
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;

            nonTrivialPhis.fill(false);
            for (unsigned i = block->phis.size(); i--;) {
                Node* phi = block->phis[i];
                if (!phi->children.justOneChild())
                    nonTrivialPhis.operand(phi->local()) = true;
            }
                
            for (unsigned i = block->variablesAtHead.size(); i--;) {
                Node* node = block->variablesAtHead[i];
                if (!node)
                    continue;
                
                if (verbose)
                    dataLog("At block #", blockIndex, " for operand r", block->variablesAtHead.operandForIndex(i), " have node ", node, "\n");
                
                VariableAccessData* variable = node->variableAccessData();
                if (variable->isCaptured()) {
                    // Poison this entry in variablesAtHead because we don't
                    // want anyone to try to refer to it, if the variable is
                    // captured.
                    block->variablesAtHead[i] = 0;
                    continue;
                }
                
                switch (node->op()) {
                case Phi:
                case SetArgument:
                    break;
                case Flush:
                case GetLocal:
                case PhantomLocal:
                    node = node->child1().node();
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                RELEASE_ASSERT(node->op() == Phi || node->op() == SetArgument);
                
                bool isFlushed = m_flushedLocalOps.contains(node);
                
                if (node->op() == Phi) {
                    if (!nonTrivialPhis.operand(node->local())) {
                        Edge edge = node->children.justOneChild();
                        ASSERT(edge);
                        if (verbose)
                            dataLog("    One child: ", edge, ", ", RawPointer(edge.node()), "\n");
                        node = edge.node(); // It's something from a different basic block.
                    } else {
                        if (verbose)
                            dataLog("    Non-trivial.\n");
                        // It's a non-trivial Phi.
                        FlushFormat format = variable->flushFormat();
                        NodeFlags result = resultFor(format);
                        UseKind useKind = useKindFor(format);
                        
                        node = m_insertionSet.insertNode(0, SpecNone, Phi, CodeOrigin());
                        if (verbose)
                            dataLog("    Inserted new node: ", node, "\n");
                        node->mergeFlags(result);
                        RELEASE_ASSERT((node->flags() & NodeResultMask) == result);
                        
                        for (unsigned j = block->predecessors.size(); j--;) {
                            BasicBlock* predecessor = block->predecessors[j];
                            predecessor->appendNonTerminal(
                                m_graph, SpecNone, Upsilon, predecessor->last()->codeOrigin,
                                OpInfo(node), Edge(predecessor->variablesAtTail[i], useKind));
                        }
                        
                        if (isFlushed) {
                            // Do nothing. For multiple reasons.
                            
                            // Reason #1: If the local is flushed then we don't need to bother
                            // with a MovHint since every path to this point in the code will
                            // have flushed the bytecode variable using a SetLocal and hence
                            // the Availability::flushedAt() will agree, and that will be
                            // sufficient for figuring out how to recover the variable's value.
                            
                            // Reason #2: If we had inserted a MovHint and the Phi function had
                            // died (because the only user of the value was the "flush" - i.e.
                            // some asynchronous runtime thingy) then the MovHint would turn
                            // into a ZombieHint, which would fool us into thinking that the
                            // variable is dead.
                            
                            // Reason #3: If we had inserted a MovHint then even if the Phi
                            // stayed alive, we would still end up generating inefficient code
                            // since we would be telling the OSR exit compiler to use some SSA
                            // value for the bytecode variable rather than just telling it that
                            // the value was already on the stack.
                        } else {
                            m_insertionSet.insertNode(
                                0, SpecNone, MovHint, CodeOrigin(),
                                OpInfo(variable->local().offset()), Edge(node));
                        }
                    }
                }
                
                block->variablesAtHead[i] = node;
            }

            m_insertionSet.execute(block);
        }
        
        if (verbose) {
            dataLog("Variables at head after SSA Phi insertion:\n");
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;
                dataLog("    ", *block, ": ", block->variablesAtHead, "\n");
            }
        }
        
        // At this point variablesAtHead in each block refers to either:
        //
        // 1) A new SSA phi in the current block.
        // 2) A SetArgument, which will soon get converted into a GetArgument.
        // 3) An old CPS phi in a different block.
        //
        // We don't have to do anything for (1) and (2), but we do need to
        // do a replacement for (3).
        
        // Clear all replacements, since other phases may have used them.
        m_graph.clearReplacements();
        
        if (dumpGraph) {
            dataLog("Graph just before identifying replacements:\n");
            m_graph.dump();
        }
        
        // For all of the old CPS Phis, figure out what they correspond to in SSA.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            if (verbose)
                dataLog("Dealing with block #", blockIndex, "\n");
            for (unsigned phiIndex = block->phis.size(); phiIndex--;) {
                Node* phi = block->phis[phiIndex];
                if (verbose) {
                    dataLog(
                        "Considering ", phi, " (", RawPointer(phi), "), for r",
                        phi->local(), ", and its replacement in ", *block, ", ",
                        block->variablesAtHead.operand(phi->local()), "\n");
                }
                ASSERT(phi != block->variablesAtHead.operand(phi->local()));
                phi->misc.replacement = block->variablesAtHead.operand(phi->local());
            }
        }
        
        // Now make sure that all variablesAtHead in each block points to the
        // canonical SSA value. Prior to this, variablesAtHead[local] may point to
        // an old CPS Phi in a different block.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (size_t i = block->variablesAtHead.size(); i--;) {
                Node* node = block->variablesAtHead[i];
                if (!node)
                    continue;
                while (node->misc.replacement) {
                    ASSERT(node != node->misc.replacement);
                    node = node->misc.replacement;
                }
                block->variablesAtHead[i] = node;
            }
        }
        
        if (verbose) {
            dataLog("Variables at head after convergence:\n");
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;
                dataLog("    ", *block, ": ", block->variablesAtHead, "\n");
            }
        }
        
        // Convert operations over locals into operations over SSA nodes.
        // - GetLocal over captured variables lose their phis.
        // - GetLocal over uncaptured variables die and get replaced with references
        //   to the node specified by variablesAtHead.
        // - SetLocal gets NodeMustGenerate if it's flushed, or turns into a
        //   Check otherwise.
        // - Flush loses its children but remains, because we want to know when a
        //   flushed SetLocal's value is no longer needed. This also makes it simpler
        //   to reason about the format of a local, since we can just do a backwards
        //   analysis (see FlushLivenessAnalysisPhase). As part of the backwards
        //   analysis, we say that the type of a local can be either int32, double,
        //   value, or dead.
        // - PhantomLocal becomes Phantom, and its child is whatever is specified
        //   by variablesAtHead.
        // - SetArgument turns into GetArgument unless it's a captured variable.
        // - Upsilons get their children fixed to refer to the true value of that local
        //   at the end of the block. Prior to this loop, Upsilons will refer to
        //   variableAtTail[operand], which may be any of Flush, PhantomLocal, GetLocal,
        //   SetLocal, SetArgument, or Phi. We accomplish this by setting the
        //   replacement pointers of all of those nodes to refer to either
        //   variablesAtHead[operand], or the child of the SetLocal.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            for (unsigned phiIndex = block->phis.size(); phiIndex--;) {
                block->phis[phiIndex]->misc.replacement =
                    block->variablesAtHead.operand(block->phis[phiIndex]->local());
            }
            for (unsigned nodeIndex = block->size(); nodeIndex--;)
                ASSERT(!block->at(nodeIndex)->misc.replacement);
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                m_graph.performSubstitution(node);
                
                switch (node->op()) {
                case SetLocal: {
                    VariableAccessData* variable = node->variableAccessData();
                    if (variable->isCaptured() || m_flushedLocalOps.contains(node))
                        node->mergeFlags(NodeMustGenerate);
                    else
                        node->setOpAndDefaultFlags(Check);
                    node->misc.replacement = node->child1().node(); // Only for Upsilons.
                    break;
                }
                    
                case GetLocal: {
                    // It seems tempting to just do forwardPhi(GetLocal), except that we
                    // could have created a new (SSA) Phi, and the GetLocal could still be
                    // referring to an old (CPS) Phi. Uses variablesAtHead to tell us what
                    // to refer to.
                    node->children.reset();
                    VariableAccessData* variable = node->variableAccessData();
                    if (variable->isCaptured())
                        break;
                    node->convertToPhantom();
                    node->misc.replacement = block->variablesAtHead.operand(variable->local());
                    break;
                }
                    
                case Flush: {
                    node->children.reset();
                    // This is only for Upsilons. An Upsilon will only refer to a Flush if
                    // there were no SetLocals or GetLocals in the block.
                    node->misc.replacement = block->variablesAtHead.operand(node->local());
                    break;
                }
                    
                case PhantomLocal: {
                    VariableAccessData* variable = node->variableAccessData();
                    if (variable->isCaptured())
                        break;
                    node->child1().setNode(block->variablesAtHead.operand(variable->local()));
                    node->convertToPhantom();
                    // This is only for Upsilons. An Upsilon will only refer to a
                    // PhantomLocal if there were no SetLocals or GetLocals in the block.
                    node->misc.replacement = block->variablesAtHead.operand(variable->local());
                    break;
                }
                    
                case SetArgument: {
                    VariableAccessData* variable = node->variableAccessData();
                    if (variable->isCaptured())
                        break;
                    node->setOpAndDefaultFlags(GetArgument);
                    node->mergeFlags(resultFor(node->variableAccessData()->flushFormat()));
                    break;
                }

                default:
                    break;
                }
            }
        }
        
        // Free all CPS phis and reset variables vectors.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned phiIndex = block->phis.size(); phiIndex--;)
                m_graph.m_allocator.free(block->phis[phiIndex]);
            block->phis.clear();
            block->variablesAtHead.clear();
            block->variablesAtTail.clear();
            block->valuesAtHead.clear();
            block->valuesAtHead.clear();
            block->ssa = adoptPtr(new BasicBlock::SSAData(block));
        }
        
        m_graph.m_arguments.clear();
        
        m_graph.m_form = SSA;
        return true;
    }

private:
    void forwardPhiChildren(Node* node)
    {
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge& edge = node->children.child(i);
            if (!edge)
                break;
            m_changed |= forwardPhiEdge(edge);
        }
    }
    
    Node* forwardPhi(Node* node)
    {
        for (;;) {
            switch (node->op()) {
            case Phi: {
                Edge edge = node->children.justOneChild();
                if (!edge)
                    return node;
                node = edge.node();
                break;
            }
            case GetLocal:
            case SetLocal:
                if (node->variableAccessData()->isCaptured())
                    return node;
                node = node->child1().node();
                break;
            default:
                return node;
            }
        }
    }
    
    bool forwardPhiEdge(Edge& edge)
    {
        Node* newNode = forwardPhi(edge.node());
        if (newNode == edge.node())
            return false;
        edge.setNode(newNode);
        return true;
    }
    
    void deduplicateChildren(Node* node)
    {
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge edge = node->children.child(i);
            if (!edge)
                break;
            if (edge == node) {
                node->children.removeEdge(i--);
                m_changed = true;
                continue;
            }
            for (unsigned j = i + 1; j < AdjacencyList::Size; ++j) {
                if (node->children.child(j) == edge) {
                    node->children.removeEdge(j--);
                    m_changed = true;
                }
            }
        }
    }
    
    void addFlushedLocalOp(Node* node)
    {
        if (m_flushedLocalOps.contains(node))
            return;
        m_flushedLocalOps.add(node);
        m_flushedLocalOpWorklist.append(node);
    }

    void addFlushedLocalEdge(Node*, Edge edge)
    {
        addFlushedLocalOp(edge.node());
    }
    
    InsertionSet m_insertionSet;
    HashSet<Node*> m_flushedLocalOps;
    Vector<Node*> m_flushedLocalOpWorklist;
    bool m_changed;
};

bool performSSAConversion(Graph& graph)
{
    SamplingRegion samplingRegion("DFG SSA Conversion Phase");
    return runPhase<SSAConversionPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

