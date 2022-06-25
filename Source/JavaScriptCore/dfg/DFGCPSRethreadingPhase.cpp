/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "DFGCPSRethreadingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCJSValueInlines.h"

namespace JSC { namespace DFG {

class CPSRethreadingPhase : public Phase {
    static constexpr bool verbose = false;
public:
    CPSRethreadingPhase(Graph& graph)
        : Phase(graph, "CPS rethreading")
    {
    }
    
    bool run()
    {
        RELEASE_ASSERT(m_graph.m_refCountState == EverythingIsLive);
        
        if (m_graph.m_form == ThreadedCPS)
            return false;
        
        clearIsLoadedFrom();
        freeUnnecessaryNodes();
        m_graph.clearReplacements();
        canonicalizeLocalsInBlocks();
        specialCaseArguments();
        propagatePhis<OperandKind::Local>();
        propagatePhis<OperandKind::Argument>();
        propagatePhis<OperandKind::Tmp>();
        computeIsFlushed();
        
        m_graph.m_form = ThreadedCPS;
        return true;
    }

private:
    
    void clearIsLoadedFrom()
    {
        for (unsigned i = 0; i < m_graph.m_variableAccessData.size(); ++i)
            m_graph.m_variableAccessData[i].setIsLoadedFrom(false);
    }
    
    void freeUnnecessaryNodes()
    {
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            ASSERT(block->isReachable);
            
            unsigned fromIndex = 0;
            unsigned toIndex = 0;
            while (fromIndex < block->size()) {
                Node* node = block->at(fromIndex++);
                switch (node->op()) {
                case GetLocal:
                case Flush:
                case PhantomLocal:
                    node->children.setChild1(Edge());
                    break;
                case Phantom:
                    if (!node->child1()) {
                        m_graph.deleteNode(node);
                        continue;
                    }
                    switch (node->child1()->op()) {
                    case SetArgumentMaybe:
                        DFG_CRASH(m_graph, node, "Invalid Phantom(@SetArgumentMaybe)");
                        break;
                    case Phi:
                    case SetArgumentDefinitely:
                    case SetLocal:
                        node->convertPhantomToPhantomLocal();
                        break;
                    default:
                        ASSERT(node->child1()->hasResult());
                        break;
                    }
                    break;
                default:
                    break;
                }
                block->at(toIndex++) = node;
            }
            block->resize(toIndex);
            
            for (unsigned phiIndex = block->phis.size(); phiIndex--;)
                m_graph.deleteNode(block->phis[phiIndex]);
            block->phis.resize(0);
        }
    }
    
    template<OperandKind operandKind>
    void clearVariables()
    {
        ASSERT(
            m_block->variablesAtHead.sizeFor<operandKind>()
            == m_block->variablesAtTail.sizeFor<operandKind>());
        
        for (unsigned i = m_block->variablesAtHead.sizeFor<operandKind>(); i--;) {
            m_block->variablesAtHead.atFor<operandKind>(i) = nullptr;
            m_block->variablesAtTail.atFor<operandKind>(i) = nullptr;
        }
    }
    
    ALWAYS_INLINE Node* addPhiSilently(BasicBlock* block, const NodeOrigin& origin, VariableAccessData* variable)
    {
        Node* result = m_graph.addNode(Phi, origin, OpInfo(variable));
        block->phis.append(result);
        return result;
    }
    
    template<OperandKind operandKind>
    ALWAYS_INLINE Node* addPhi(BasicBlock* block, const NodeOrigin& origin, VariableAccessData* variable, size_t index)
    {
        Node* result = addPhiSilently(block, origin, variable);
        phiStackFor<operandKind>().append(PhiStackEntry(block, index, result));
        return result;
    }
    
    template<OperandKind operandKind>
    ALWAYS_INLINE Node* addPhi(const NodeOrigin& origin, VariableAccessData* variable, size_t index)
    {
        return addPhi<operandKind>(m_block, origin, variable, index);
    }
    
    template<OperandKind operandKind>
    void canonicalizeGetLocalFor(Node* node, VariableAccessData* variable, size_t idx)
    {
        ASSERT(!node->child1());
        
        if (Node* otherNode = m_block->variablesAtTail.atFor<operandKind>(idx)) {
            ASSERT(otherNode->variableAccessData() == variable);
            
            switch (otherNode->op()) {
            case Flush:
            case PhantomLocal:
                otherNode = otherNode->child1().node();
                if (otherNode->op() == Phi) {
                    // We need to have a GetLocal, so this might as well be the one.
                    node->children.setChild1(Edge(otherNode));
                    m_block->variablesAtTail.atFor<operandKind>(idx) = node;
                    return;
                }
                ASSERT(otherNode->op() != SetArgumentMaybe);
                ASSERT(otherNode->op() == SetLocal || otherNode->op() == SetArgumentDefinitely);
                break;
            default:
                break;
            }
            
            ASSERT(otherNode->op() != SetArgumentMaybe);
            ASSERT(otherNode->op() == SetLocal || otherNode->op() == SetArgumentDefinitely || otherNode->op() == GetLocal);
            ASSERT(otherNode->variableAccessData() == variable);
            
            if (otherNode->op() == SetArgumentDefinitely) {
                variable->setIsLoadedFrom(true);
                node->children.setChild1(Edge(otherNode));
                m_block->variablesAtTail.atFor<operandKind>(idx) = node;
                return;
            }
            
            if (otherNode->op() == GetLocal) {
                // Replace all references to this GetLocal with otherNode.
                node->replaceWith(m_graph, otherNode);
                return;
            }
            
            ASSERT(otherNode->op() == SetLocal);
            node->replaceWith(m_graph, otherNode->child1().node());
            return;
        }
        
        variable->setIsLoadedFrom(true);
        Node* phi = addPhi<operandKind>(node->origin, variable, idx);
        node->children.setChild1(Edge(phi));
        m_block->variablesAtHead.atFor<operandKind>(idx) = phi;
        m_block->variablesAtTail.atFor<operandKind>(idx) = node;
    }
    
    void canonicalizeGetLocal(Node* node)
    {
        VariableAccessData* variable = node->variableAccessData();
        switch (variable->operand().kind()) {
        case OperandKind::Argument: {
            canonicalizeGetLocalFor<OperandKind::Argument>(node, variable, variable->operand().toArgument());
            break;
        }
        case OperandKind::Local: {
            canonicalizeGetLocalFor<OperandKind::Local>(node, variable, variable->operand().toLocal());
            break;
        }
        case OperandKind::Tmp: {
            canonicalizeGetLocalFor<OperandKind::Tmp>(node, variable, variable->operand().value());
            break;
        }
        }
    }
    
    template<NodeType nodeType, OperandKind operandKind>
    void canonicalizeFlushOrPhantomLocalFor(Node* node, VariableAccessData* variable, size_t idx)
    {
        ASSERT(!node->child1());
        
        if (Node* otherNode = m_block->variablesAtTail.atFor<operandKind>(idx)) {
            ASSERT(otherNode->variableAccessData() == variable);
            
            switch (otherNode->op()) {
            case Flush:
            case PhantomLocal:
            case GetLocal:
                ASSERT(otherNode->child1().node());
                otherNode = otherNode->child1().node();
                break;
            default:
                break;
            }
            
            ASSERT(otherNode->op() == Phi || otherNode->op() == SetLocal || otherNode->op() == SetArgumentDefinitely || otherNode->op() == SetArgumentMaybe);
            
            if (nodeType == PhantomLocal && otherNode->op() == SetLocal) {
                // PhantomLocal(SetLocal) doesn't make sense. PhantomLocal means: at this
                // point I know I would have been interested in the value of this variable
                // for the purpose of OSR. PhantomLocal(SetLocal) means: at this point I
                // know that I would have read the value written by that SetLocal. This is
                // redundant and inefficient, since really it just means that we want to
                // keep the last MovHinted value of that local alive.
                
                node->remove(m_graph);
                return;
            }
            
            variable->setIsLoadedFrom(true);
            // There is nothing wrong with having redundant Flush's. It just needs to
            // be linked appropriately. Note that if there had already been a previous
            // use at tail then we don't override it. It's fine for variablesAtTail to
            // omit Flushes and PhantomLocals. On the other hand, having it refer to a
            // Flush or a PhantomLocal if just before it the last use was a GetLocal would
            // seriously confuse the CFA.
            node->children.setChild1(Edge(otherNode));
            return;
        }
        
        variable->setIsLoadedFrom(true);
        node->children.setChild1(Edge(addPhi<operandKind>(node->origin, variable, idx)));
        m_block->variablesAtHead.atFor<operandKind>(idx) = node;
        m_block->variablesAtTail.atFor<operandKind>(idx) = node;
    }

    template<NodeType nodeType>
    void canonicalizeFlushOrPhantomLocal(Node* node)
    {
        VariableAccessData* variable = node->variableAccessData();
        switch (variable->operand().kind()) {
        case OperandKind::Argument: {
            canonicalizeFlushOrPhantomLocalFor<nodeType, OperandKind::Argument>(node, variable, variable->operand().toArgument());
            break;
        }
        case OperandKind::Local: {
            canonicalizeFlushOrPhantomLocalFor<nodeType, OperandKind::Local>(node, variable, variable->operand().toLocal());
            break;
        }
        case OperandKind::Tmp: {
            canonicalizeFlushOrPhantomLocalFor<nodeType, OperandKind::Tmp>(node, variable, variable->operand().value());
            break;
        }
        }
    }
    
    void canonicalizeSet(Node* node)
    {
        m_block->variablesAtTail.setOperand(node->operand(), node);
    }
    
    void canonicalizeLocalsInBlock()
    {
        if (!m_block)
            return;
        ASSERT(m_block->isReachable);
        
        clearVariables<OperandKind::Argument>();
        clearVariables<OperandKind::Local>();
        clearVariables<OperandKind::Tmp>();
        
        // Assumes that all phi references have been removed. Assumes that things that
        // should be live have a non-zero ref count, but doesn't assume that the ref
        // counts are correct beyond that (more formally !!logicalRefCount == !!actualRefCount
        // but not logicalRefCount == actualRefCount). Assumes that it can break ref
        // counts.
        
        for (auto* node : *m_block) {
            m_graph.performSubstitution(node);
            
            // The rules for threaded CPS form:
            // 
            // Head variable: describes what is live at the head of the basic block.
            // Head variable links may refer to Flush, PhantomLocal, Phi, or SetArgumentDefinitely/SetArgumentMaybe.
            // SetArgumentDefinitely/SetArgumentMaybe may only appear in the root block.
            //
            // Tail variable: the last thing that happened to the variable in the block.
            // It may be a Flush, PhantomLocal, GetLocal, SetLocal, SetArgumentDefinitely/SetArgumentMaybe, or Phi.
            // SetArgumentDefinitely/SetArgumentMaybe may only appear in the root block. Note that if there ever
            // was a GetLocal to the variable, and it was followed by PhantomLocals and
            // Flushes but not SetLocals, then the tail variable will be the GetLocal.
            // This reflects the fact that you only care that the tail variable is a
            // Flush or PhantomLocal if nothing else interesting happened. Likewise, if
            // there ever was a SetLocal and it was followed by Flushes, then the tail
            // variable will be a SetLocal and not those subsequent Flushes.
            //
            // Child of GetLocal: the operation that the GetLocal keeps alive. It may be
            // a Phi from the current block. For arguments, it may be a SetArgumentDefinitely
            // but it can't be a SetArgumentMaybe.
            //
            // Child of SetLocal: must be a value producing node.
            //
            // Child of Flush: it may be a Phi from the current block or a SetLocal. For
            // arguments it may also be a SetArgumentDefinitely/SetArgumentMaybe.
            //
            // Child of PhantomLocal: it may be a Phi from the current block. For
            // arguments it may also be a SetArgumentDefinitely/SetArgumentMaybe.
            //
            // Children of Phi: other Phis in the same basic block, or any of the
            // following from predecessor blocks: SetLocal, Phi, or SetArgumentDefinitely/SetArgumentMaybe.
            // These are computed by looking at the tail variables of the predecessor blocks
            // and either using it directly (if it's a SetLocal, Phi, or SetArgumentDefinitely/SetArgumentMaybe) or
            // loading that nodes child (if it's a GetLocal, PhanomLocal, or Flush - all
            // of these will have children that are SetLocal, Phi, or SetArgumentDefinitely/SetArgumentMaybe).
            
            switch (node->op()) {
            case GetLocal:
                canonicalizeGetLocal(node);
                break;
                
            case SetLocal:
                canonicalizeSet(node);
                break;
                
            case Flush:
                canonicalizeFlushOrPhantomLocal<Flush>(node);
                break;
                
            case PhantomLocal:
                canonicalizeFlushOrPhantomLocal<PhantomLocal>(node);
                break;
                
            case SetArgumentDefinitely:
            case SetArgumentMaybe:
                canonicalizeSet(node);
                break;
                
            default:
                break;
            }
        }
    }
    
    void canonicalizeLocalsInBlocks()
    {
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            m_block = m_graph.block(blockIndex);
            canonicalizeLocalsInBlock();
        }
    }
    
    void specialCaseArguments()
    {
        // Normally, a SetArgumentDefinitely denotes the start of a live range for a local's value on the stack.
        // But those SetArguments used for the actual arguments to the machine CodeBlock get
        // special-cased. We could have instead used two different node types - one for the arguments
        // at the prologue case, and another for the other uses. But this seemed like IR overkill.

        for (auto& pair : m_graph.m_rootToArguments) {
            BasicBlock* entrypoint = pair.key;
            const ArgumentsVector& arguments = pair.value;
            for (unsigned i = arguments.size(); i--;)
                entrypoint->variablesAtHead.setArgumentFirstTime(i, arguments[i]);
        }
    }
    
    template<OperandKind operandKind>
    void propagatePhis()
    {
        Vector<PhiStackEntry, 128>& phiStack = phiStackFor<operandKind>();
        
        // Ensure that attempts to use this fail instantly.
        m_block = nullptr;
        
        while (!phiStack.isEmpty()) {
            PhiStackEntry entry = phiStack.last();
            phiStack.removeLast();
            
            BasicBlock* block = entry.m_block;
            PredecessorList& predecessors = block->predecessors;
            Node* currentPhi = entry.m_phi;
            VariableAccessData* variable = currentPhi->variableAccessData();
            size_t index = entry.m_index;
            
            if (verbose) {
                dataLog(" Iterating on phi from block ", block, " ");
                m_graph.dump(WTF::dataFile(), "", currentPhi);
            }

            for (size_t i = predecessors.size(); i--;) {
                BasicBlock* predecessorBlock = predecessors[i];
                
                Node* variableInPrevious = predecessorBlock->variablesAtTail.atFor<operandKind>(index);
                if (!variableInPrevious) {
                    variableInPrevious = addPhi<operandKind>(predecessorBlock, currentPhi->origin, variable, index);
                    predecessorBlock->variablesAtTail.atFor<operandKind>(index) = variableInPrevious;
                    predecessorBlock->variablesAtHead.atFor<operandKind>(index) = variableInPrevious;
                    dataLogLnIf(verbose, "    No variable in predecessor ", predecessorBlock, " creating a new phi: ", variableInPrevious);
                } else {
                    switch (variableInPrevious->op()) {
                    case GetLocal:
                    case PhantomLocal:
                    case Flush:
                        dataLogLnIf(verbose, "    Variable in predecessor ", predecessorBlock, " ", variableInPrevious, " needs to be forwarded to first child ", variableInPrevious->child1().node());
                        ASSERT(variableInPrevious->variableAccessData() == variableInPrevious->child1()->variableAccessData());
                        variableInPrevious = variableInPrevious->child1().node();
                        break;
                    default:
                        break;
                    }
                }
                
                ASSERT(
                    variableInPrevious->op() == SetLocal
                    || variableInPrevious->op() == Phi
                    || variableInPrevious->op() == SetArgumentDefinitely
                    || variableInPrevious->op() == SetArgumentMaybe);
          
                if (verbose)
                    m_graph.dump(WTF::dataFile(), "    Adding new variable from predecessor ", variableInPrevious);

                if (!currentPhi->child1()) {
                    currentPhi->children.setChild1(Edge(variableInPrevious));
                    continue;
                }
                if (!currentPhi->child2()) {
                    currentPhi->children.setChild2(Edge(variableInPrevious));
                    continue;
                }
                if (!currentPhi->child3()) {
                    currentPhi->children.setChild3(Edge(variableInPrevious));
                    continue;
                }
                
                Node* newPhi = addPhiSilently(block, currentPhi->origin, variable);
                newPhi->children = currentPhi->children;
                currentPhi->children.initialize(newPhi, variableInPrevious, nullptr);
            }
        }
    }
    
    struct PhiStackEntry {
        PhiStackEntry(BasicBlock* block, size_t index, Node* phi)
            : m_block(block)
            , m_index(index)
            , m_phi(phi)
        {
        }
        
        BasicBlock* m_block;
        size_t m_index;
        Node* m_phi;
    };
    
    template<OperandKind operandKind>
    Vector<PhiStackEntry, 128>& phiStackFor()
    {
        switch (operandKind) {
        case OperandKind::Argument: return m_argumentPhiStack;
        case OperandKind::Local: return m_localPhiStack;
        case OperandKind::Tmp: return m_tmpPhiStack;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void computeIsFlushed()
    {
        m_graph.clearFlagsOnAllNodes(NodeIsFlushed);
        
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
            switch (node->op()) {
            case SetLocal:
            case SetArgumentDefinitely:
            case SetArgumentMaybe:
                break;
                
            case Flush:
            case Phi:
                ASSERT(node->flags() & NodeIsFlushed);
                DFG_NODE_DO_TO_CHILDREN(m_graph, node, addFlushedLocalEdge);
                break;

            default:
                DFG_CRASH(m_graph, node, "Invalid node in flush graph");
                break;
            }
        }
    }
    
    void addFlushedLocalOp(Node* node)
    {
        if (node->mergeFlags(NodeIsFlushed))
            m_flushedLocalOpWorklist.append(node);
    }

    void addFlushedLocalEdge(Node*, Edge edge)
    {
        addFlushedLocalOp(edge.node());
    }

    BasicBlock* m_block;
    Vector<PhiStackEntry, 128> m_argumentPhiStack;
    Vector<PhiStackEntry, 128> m_localPhiStack;
    Vector<PhiStackEntry, 128> m_tmpPhiStack;
    Vector<Node*, 128> m_flushedLocalOpWorklist;
};

bool performCPSRethreading(Graph& graph)
{
    return runPhase<CPSRethreadingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

