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
#include "DFGInPlaceAbstractState.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGBasicBlock.h"
#include "GetByIdStatus.h"
#include "JSCInlines.h"
#include "PutByIdStatus.h"
#include "StringObject.h"

namespace JSC { namespace DFG {

InPlaceAbstractState::InPlaceAbstractState(Graph& graph)
    : m_graph(graph)
    , m_variables(m_graph.m_codeBlock->numParameters(), graph.m_localVars)
    , m_block(0)
{
}

InPlaceAbstractState::~InPlaceAbstractState() { }

void InPlaceAbstractState::beginBasicBlock(BasicBlock* basicBlock)
{
    ASSERT(!m_block);
    
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->valuesAtHead.numberOfLocals());
    ASSERT(basicBlock->variablesAtTail.numberOfLocals() == basicBlock->valuesAtTail.numberOfLocals());
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->variablesAtTail.numberOfLocals());
    
    for (size_t i = 0; i < basicBlock->size(); i++)
        forNode(basicBlock->at(i)).clear();

    m_variables = basicBlock->valuesAtHead;
    m_haveStructures = false;
    for (size_t i = 0; i < m_variables.numberOfArguments(); ++i) {
        if (m_variables.argument(i).hasClobberableState()) {
            m_haveStructures = true;
            break;
        }
    }
    for (size_t i = 0; i < m_variables.numberOfLocals(); ++i) {
        if (m_variables.local(i).hasClobberableState()) {
            m_haveStructures = true;
            break;
        }
    }
    
    if (m_graph.m_form == SSA) {
        HashMap<Node*, AbstractValue>::iterator iter = basicBlock->ssa->valuesAtHead.begin();
        HashMap<Node*, AbstractValue>::iterator end = basicBlock->ssa->valuesAtHead.end();
        for (; iter != end; ++iter) {
            forNode(iter->key) = iter->value;
            if (iter->value.hasClobberableState())
                m_haveStructures = true;
        }
    }
    
    basicBlock->cfaShouldRevisit = false;
    basicBlock->cfaHasVisited = true;
    m_block = basicBlock;
    m_isValid = true;
    m_foundConstants = false;
    m_branchDirection = InvalidBranchDirection;
}

static void setLiveValues(HashMap<Node*, AbstractValue>& values, HashSet<Node*>& live)
{
    values.clear();
    
    HashSet<Node*>::iterator iter = live.begin();
    HashSet<Node*>::iterator end = live.end();
    for (; iter != end; ++iter)
        values.add(*iter, AbstractValue());
}

void InPlaceAbstractState::initialize()
{
    BasicBlock* root = m_graph.block(0);
    root->cfaShouldRevisit = true;
    root->cfaHasVisited = false;
    root->cfaFoundConstants = false;
    for (size_t i = 0; i < root->valuesAtHead.numberOfArguments(); ++i) {
        root->valuesAtTail.argument(i).clear();
        if (m_graph.m_form == SSA) {
            root->valuesAtHead.argument(i).makeHeapTop();
            continue;
        }
        
        Node* node = root->variablesAtHead.argument(i);
        ASSERT(node->op() == SetArgument);
        if (!node->variableAccessData()->shouldUnboxIfPossible()) {
            root->valuesAtHead.argument(i).makeHeapTop();
            continue;
        }
        
        SpeculatedType prediction =
            node->variableAccessData()->argumentAwarePrediction();
        if (isInt32Speculation(prediction))
            root->valuesAtHead.argument(i).setType(SpecInt32);
        else if (isBooleanSpeculation(prediction))
            root->valuesAtHead.argument(i).setType(SpecBoolean);
        else if (isCellSpeculation(prediction))
            root->valuesAtHead.argument(i).setType(SpecCell);
        else
            root->valuesAtHead.argument(i).makeHeapTop();
    }
    for (size_t i = 0; i < root->valuesAtHead.numberOfLocals(); ++i) {
        Node* node = root->variablesAtHead.local(i);
        if (node && node->variableAccessData()->isCaptured())
            root->valuesAtHead.local(i).makeHeapTop();
        else
            root->valuesAtHead.local(i).clear();
        root->valuesAtTail.local(i).clear();
    }
    for (BlockIndex blockIndex = 1 ; blockIndex < m_graph.numBlocks(); ++blockIndex) {
        BasicBlock* block = m_graph.block(blockIndex);
        if (!block)
            continue;
        ASSERT(block->isReachable);
        block->cfaShouldRevisit = false;
        block->cfaHasVisited = false;
        block->cfaFoundConstants = false;
        for (size_t i = 0; i < block->valuesAtHead.numberOfArguments(); ++i) {
            block->valuesAtHead.argument(i).clear();
            block->valuesAtTail.argument(i).clear();
        }
        for (size_t i = 0; i < block->valuesAtHead.numberOfLocals(); ++i) {
            block->valuesAtHead.local(i).clear();
            block->valuesAtTail.local(i).clear();
        }
        if (!block->isOSRTarget)
            continue;
        if (block->bytecodeBegin != m_graph.m_plan.osrEntryBytecodeIndex)
            continue;
        for (size_t i = 0; i < m_graph.m_mustHandleAbstractValues.size(); ++i) {
            AbstractValue value = m_graph.m_mustHandleAbstractValues[i];
            int operand = m_graph.m_mustHandleAbstractValues.operandForIndex(i);
            block->valuesAtHead.operand(operand).merge(value);
        }
        block->cfaShouldRevisit = true;
    }
    if (m_graph.m_form == SSA) {
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            setLiveValues(block->ssa->valuesAtHead, block->ssa->liveAtHead);
            setLiveValues(block->ssa->valuesAtTail, block->ssa->liveAtTail);
        }
    }
}

bool InPlaceAbstractState::endBasicBlock(MergeMode mergeMode)
{
    ASSERT(m_block);
    
    BasicBlock* block = m_block; // Save the block for successor merging.
    
    block->cfaFoundConstants = m_foundConstants;
    block->cfaDidFinish = m_isValid;
    block->cfaBranchDirection = m_branchDirection;
    
    if (!m_isValid) {
        reset();
        return false;
    }
    
    bool changed = false;
    
    if (mergeMode != DontMerge || !ASSERT_DISABLED) {
        switch (m_graph.m_form) {
        case ThreadedCPS: {
            for (size_t argument = 0; argument < block->variablesAtTail.numberOfArguments(); ++argument) {
                AbstractValue& destination = block->valuesAtTail.argument(argument);
                changed |= mergeStateAtTail(destination, m_variables.argument(argument), block->variablesAtTail.argument(argument));
            }
            
            for (size_t local = 0; local < block->variablesAtTail.numberOfLocals(); ++local) {
                AbstractValue& destination = block->valuesAtTail.local(local);
                changed |= mergeStateAtTail(destination, m_variables.local(local), block->variablesAtTail.local(local));
            }
            break;
        }
            
        case SSA: {
            for (size_t i = 0; i < block->valuesAtTail.size(); ++i)
                changed |= block->valuesAtTail[i].merge(m_variables[i]);
            
            HashSet<Node*>::iterator iter = block->ssa->liveAtTail.begin();
            HashSet<Node*>::iterator end = block->ssa->liveAtTail.end();
            for (; iter != end; ++iter) {
                Node* node = *iter;
                changed |= block->ssa->valuesAtTail.find(node)->value.merge(forNode(node));
            }
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    
    ASSERT(mergeMode != DontMerge || !changed);
    
    reset();
    
    if (mergeMode != MergeToSuccessors)
        return changed;
    
    return mergeToSuccessors(block);
}

void InPlaceAbstractState::reset()
{
    m_block = 0;
    m_isValid = false;
    m_branchDirection = InvalidBranchDirection;
}

bool InPlaceAbstractState::mergeStateAtTail(AbstractValue& destination, AbstractValue& inVariable, Node* node)
{
    if (!node)
        return false;
        
    AbstractValue source;
    
    if (node->variableAccessData()->isCaptured()) {
        // If it's captured then we know that whatever value was stored into the variable last is the
        // one we care about. This is true even if the variable at tail is dead, which might happen if
        // the last thing we did to the variable was a GetLocal and then ended up not using the
        // GetLocal's result.
        
        source = inVariable;
    } else {
        switch (node->op()) {
        case Phi:
        case SetArgument:
        case PhantomLocal:
        case Flush:
            // The block transfers the value from head to tail.
            source = inVariable;
            break;
            
        case GetLocal:
            // The block refines the value with additional speculations.
            source = forNode(node);
            break;
            
        case SetLocal:
            // The block sets the variable, and potentially refines it, both
            // before and after setting it.
            source = forNode(node->child1());
            if (node->variableAccessData()->flushFormat() == FlushedDouble) {
                ASSERT(!(source.m_type & ~SpecFullNumber));
                ASSERT(!!(source.m_type & ~SpecDouble) == !!(source.m_type & SpecMachineInt));
                if (!(source.m_type & ~SpecDouble)) {
                    source.merge(SpecInt52AsDouble);
                    source.filter(SpecDouble);
                }
            }
            break;
        
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    if (destination == source) {
        // Abstract execution did not change the output value of the variable, for this
        // basic block, on this iteration.
        return false;
    }
    
    // Abstract execution reached a new conclusion about the speculations reached about
    // this variable after execution of this basic block. Update the state, and return
    // true to indicate that the fixpoint must go on!
    destination = source;
    return true;
}

bool InPlaceAbstractState::merge(BasicBlock* from, BasicBlock* to)
{
    ASSERT(from->variablesAtTail.numberOfArguments() == to->variablesAtHead.numberOfArguments());
    ASSERT(from->variablesAtTail.numberOfLocals() == to->variablesAtHead.numberOfLocals());
    
    bool changed = false;
    
    switch (m_graph.m_form) {
    case ThreadedCPS: {
        for (size_t argument = 0; argument < from->variablesAtTail.numberOfArguments(); ++argument) {
            AbstractValue& destination = to->valuesAtHead.argument(argument);
            changed |= mergeVariableBetweenBlocks(destination, from->valuesAtTail.argument(argument), to->variablesAtHead.argument(argument), from->variablesAtTail.argument(argument));
        }
        
        for (size_t local = 0; local < from->variablesAtTail.numberOfLocals(); ++local) {
            AbstractValue& destination = to->valuesAtHead.local(local);
            changed |= mergeVariableBetweenBlocks(destination, from->valuesAtTail.local(local), to->variablesAtHead.local(local), from->variablesAtTail.local(local));
        }
        break;
    }
        
    case SSA: {
        for (size_t i = from->valuesAtTail.size(); i--;)
            changed |= to->valuesAtHead[i].merge(from->valuesAtTail[i]);
        
        HashSet<Node*>::iterator iter = to->ssa->liveAtHead.begin();
        HashSet<Node*>::iterator end = to->ssa->liveAtHead.end();
        for (; iter != end; ++iter) {
            Node* node = *iter;
            changed |= to->ssa->valuesAtHead.find(node)->value.merge(
                from->ssa->valuesAtTail.find(node)->value);
        }
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (!to->cfaHasVisited)
        changed = true;
    
    to->cfaShouldRevisit |= changed;
    
    return changed;
}

inline bool InPlaceAbstractState::mergeToSuccessors(BasicBlock* basicBlock)
{
    Node* terminal = basicBlock->last();
    
    ASSERT(terminal->isTerminal());
    
    switch (terminal->op()) {
    case Jump: {
        ASSERT(basicBlock->cfaBranchDirection == InvalidBranchDirection);
        return merge(basicBlock, terminal->targetBlock());
    }
        
    case Branch: {
        ASSERT(basicBlock->cfaBranchDirection != InvalidBranchDirection);
        bool changed = false;
        if (basicBlock->cfaBranchDirection != TakeFalse)
            changed |= merge(basicBlock, terminal->branchData()->taken.block);
        if (basicBlock->cfaBranchDirection != TakeTrue)
            changed |= merge(basicBlock, terminal->branchData()->notTaken.block);
        return changed;
    }
        
    case Switch: {
        // FIXME: It would be cool to be sparse conditional for Switch's. Currently
        // we're not. However I somehow doubt that this will ever be a big deal.
        ASSERT(basicBlock->cfaBranchDirection == InvalidBranchDirection);
        SwitchData* data = terminal->switchData();
        bool changed = merge(basicBlock, data->fallThrough.block);
        for (unsigned i = data->cases.size(); i--;)
            changed |= merge(basicBlock, data->cases[i].target.block);
        return changed;
    }
        
    case Return:
    case Unreachable:
        ASSERT(basicBlock->cfaBranchDirection == InvalidBranchDirection);
        return false;

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
}

inline bool InPlaceAbstractState::mergeVariableBetweenBlocks(AbstractValue& destination, AbstractValue& source, Node* destinationNode, Node* sourceNode)
{
    if (!destinationNode)
        return false;
    
    ASSERT_UNUSED(sourceNode, sourceNode);
    
    // FIXME: We could do some sparse conditional propagation here!
    
    return destination.merge(source);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

