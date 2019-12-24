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
#include "DFGInPlaceAbstractState.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGBasicBlock.h"
#include "GetByStatus.h"
#include "JSCInlines.h"
#include "PutByIdStatus.h"
#include "StringObject.h"
#include "SuperSampler.h"

namespace JSC { namespace DFG {

namespace DFGInPlaceAbstractStateInternal {
static constexpr bool verbose = false;
}

InPlaceAbstractState::InPlaceAbstractState(Graph& graph)
    : m_graph(graph)
    , m_abstractValues(*graph.m_abstractValuesCache)
    , m_variables(OperandsLike, graph.block(0)->variablesAtHead)
    , m_block(nullptr)
{
}

InPlaceAbstractState::~InPlaceAbstractState() { }

void InPlaceAbstractState::beginBasicBlock(BasicBlock* basicBlock)
{
    ASSERT(!m_block);
    
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->valuesAtHead.numberOfLocals());
    ASSERT(basicBlock->variablesAtTail.numberOfLocals() == basicBlock->valuesAtTail.numberOfLocals());
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->variablesAtTail.numberOfLocals());

    m_abstractValues.resize();

    AbstractValueClobberEpoch epoch = AbstractValueClobberEpoch::first(basicBlock->cfaStructureClobberStateAtHead);
    m_epochAtHead = epoch;
    m_effectEpoch = epoch;

    m_block = basicBlock;

    m_activeVariables.clearRange(0, std::min(m_variables.size(), m_activeVariables.size()));
    if (m_variables.size() > m_activeVariables.size())
        m_activeVariables.resize(m_variables.size());
    
    if (m_graph.m_form == SSA) {
        for (NodeAbstractValuePair& entry : basicBlock->ssa->valuesAtHead) {
            if (entry.node.isStillValid()) {
                AbstractValue& value = m_abstractValues.at(entry.node);
                value = entry.value;
                value.m_effectEpoch = epoch;
            }
        }
    }
    basicBlock->cfaShouldRevisit = false;
    basicBlock->cfaHasVisited = true;
    m_isValid = true;
    m_shouldTryConstantFolding = false;
    m_branchDirection = InvalidBranchDirection;
    m_structureClobberState = basicBlock->cfaStructureClobberStateAtHead;
}

static void setLiveValues(Vector<NodeAbstractValuePair>& values, const Vector<NodeFlowProjection>& live)
{
    values.shrink(0);
    values.reserveCapacity(live.size());
    for (NodeFlowProjection node : live)
        values.uncheckedAppend(NodeAbstractValuePair { node, AbstractValue() });
}

Operands<AbstractValue>& InPlaceAbstractState::variablesForDebugging()
{
    activateAllVariables();
    return m_variables;
}

void InPlaceAbstractState::activateAllVariables()
{
    for (size_t i = m_variables.size(); i--;)
        activateVariableIfNecessary(i);
}

void InPlaceAbstractState::initialize()
{
    for (BasicBlock* entrypoint : m_graph.m_roots) {
        entrypoint->cfaShouldRevisit = true;
        entrypoint->cfaHasVisited = false;
        entrypoint->cfaThinksShouldTryConstantFolding = false;
        entrypoint->cfaStructureClobberStateAtHead = StructuresAreWatched;
        entrypoint->cfaStructureClobberStateAtTail = StructuresAreWatched;

        if (m_graph.m_form == SSA)  {
            for (size_t i = 0; i < entrypoint->valuesAtHead.numberOfArguments(); ++i) {
                entrypoint->valuesAtHead.argument(i).clear();
                entrypoint->valuesAtTail.argument(i).clear();
            }
        } else {
            const ArgumentsVector& arguments = m_graph.m_rootToArguments.find(entrypoint)->value;
            for (size_t i = 0; i < entrypoint->valuesAtHead.numberOfArguments(); ++i) {
                entrypoint->valuesAtTail.argument(i).clear();

                FlushFormat format;
                Node* node = arguments[i];
                if (!node)
                    format = FlushedJSValue;
                else {
                    ASSERT(node->op() == SetArgumentDefinitely);
                    format = node->variableAccessData()->flushFormat();
                }

                switch (format) {
                case FlushedInt32:
                    entrypoint->valuesAtHead.argument(i).setNonCellType(SpecInt32Only);
                    break;
                case FlushedBoolean:
                    entrypoint->valuesAtHead.argument(i).setNonCellType(SpecBoolean);
                    break;
                case FlushedCell:
                    entrypoint->valuesAtHead.argument(i).setType(m_graph, SpecCellCheck);
                    break;
                case FlushedJSValue:
                    entrypoint->valuesAtHead.argument(i).makeBytecodeTop();
                    break;
                default:
                    DFG_CRASH(m_graph, nullptr, "Bad flush format for argument");
                    break;
                }
            }
        }

        for (size_t i = 0; i < entrypoint->valuesAtHead.numberOfLocals(); ++i) {
            entrypoint->valuesAtHead.local(i).clear();
            entrypoint->valuesAtTail.local(i).clear();
        }
    }

    for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
        if (m_graph.isRoot(block)) {
            // We bootstrapped the CFG roots above.
            continue;
        }

        ASSERT(block->isReachable);
        block->cfaShouldRevisit = false;
        block->cfaHasVisited = false;
        block->cfaThinksShouldTryConstantFolding = false;
        block->cfaStructureClobberStateAtHead = StructuresAreWatched;
        block->cfaStructureClobberStateAtTail = StructuresAreWatched;
        for (size_t i = 0; i < block->valuesAtHead.numberOfArguments(); ++i) {
            block->valuesAtHead.argument(i).clear();
            block->valuesAtTail.argument(i).clear();
        }
        for (size_t i = 0; i < block->valuesAtHead.numberOfLocals(); ++i) {
            block->valuesAtHead.local(i).clear();
            block->valuesAtTail.local(i).clear();
        }
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

bool InPlaceAbstractState::endBasicBlock()
{
    ASSERT(m_block);
    
    BasicBlock* block = m_block; // Save the block for successor merging.
    
    block->cfaThinksShouldTryConstantFolding = m_shouldTryConstantFolding;
    block->cfaDidFinish = m_isValid;
    block->cfaBranchDirection = m_branchDirection;
    
    if (!m_isValid) {
        reset();
        return false;
    }

    AbstractValueClobberEpoch epochAtHead = m_epochAtHead;
    AbstractValueClobberEpoch currentEpoch = m_effectEpoch;

    block->cfaStructureClobberStateAtTail = m_structureClobberState;
    
    switch (m_graph.m_form) {
    case ThreadedCPS: {
        ASSERT(block->variablesAtTail.size() == block->valuesAtTail.size());
        ASSERT(block->variablesAtTail.size() == m_variables.size());
        for (size_t index = m_variables.size(); index--;) {
            Node* node = block->variablesAtTail[index];
            if (!node)
                continue;
            AbstractValue& destination = block->valuesAtTail[index];
            
            if (!m_activeVariables[index]) {
                destination = block->valuesAtHead[index];
                destination.fastForwardFromTo(epochAtHead, currentEpoch);
                continue;
            }
            
            switch (node->op()) {
            case Phi:
            case SetArgumentDefinitely:
            case SetArgumentMaybe:
            case PhantomLocal:
            case Flush: {
                // The block transfers the value from head to tail.
                destination = variableAt(index);
                break;
            }
                
            case GetLocal: {
                // The block refines the value with additional speculations.
                destination = forNode(node);

                // We need to make sure that we don't broaden the type beyond what the flush
                // format says it will be. The value may claim to have changed abstract state
                // but it's type cannot change without a store. For example:
                //
                // Block #1:
                // 0: GetLocal(loc42, FlushFormatInt32)
                // 1: PutStructure(Check: Cell: @0, ArrayStructure)
                // ...
                // 2: Branch(T: #1, F: #2)
                //
                // In this case the AbstractState of @0 will say it's an SpecArray but the only
                // reason that would have happened is because we would have exited the cell check.

                FlushFormat flushFormat = node->variableAccessData()->flushFormat();
                destination.filter(typeFilterFor(flushFormat));
                break;
            }
            case SetLocal: {
                // The block sets the variable, and potentially refines it, both
                // before and after setting it. Since the SetLocal already did
                // a type check based on the flush format's type, we're only interested
                // in refinements within that type hierarchy. Otherwise, we may end up
                // saying that any GetLocals reachable from this basic block load something
                // outside of that hierarchy, e.g:
                //
                // a: JSConstant(jsNumber(0))
                // b: SetLocal(Int32:@a, loc1, FlushedInt32)
                // c: ArrayifyToStructure(Cell:@a)
                // d: Jump(...)
                //
                // In this example, we can't trust whatever type ArrayifyToStructure sets
                // @a to. We're only interested in the subset of that type that intersects
                // with Int32.
                AbstractValue value = forNode(node->child1());
                value.filter(typeFilterFor(node->variableAccessData()->flushFormat()));
                destination = value;
                break;
            }
                
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
        break;
    }

    case SSA: {
        for (size_t i = 0; i < block->valuesAtTail.size(); ++i) {
            AbstractValue& destination = block->valuesAtTail[i];

            if (!m_activeVariables[i]) {
                destination = block->valuesAtHead[i];
                destination.fastForwardFromTo(epochAtHead, currentEpoch);
                continue;
            }
            
            block->valuesAtTail[i] = variableAt(i);
        }

        for (NodeAbstractValuePair& valueAtTail : block->ssa->valuesAtTail)
            valueAtTail.value = forNode(valueAtTail.node);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    reset();
    
    return mergeToSuccessors(block);
}

void InPlaceAbstractState::reset()
{
    m_block = 0;
    m_isValid = false;
    m_branchDirection = InvalidBranchDirection;
    m_structureClobberState = StructuresAreWatched;
}

void InPlaceAbstractState::activateVariable(size_t variableIndex)
{
    AbstractValue& value = m_variables[variableIndex];
    value = m_block->valuesAtHead[variableIndex];
    value.m_effectEpoch = m_epochAtHead;
    m_activeVariables[variableIndex] = true;
}

bool InPlaceAbstractState::merge(BasicBlock* from, BasicBlock* to)
{
    if (DFGInPlaceAbstractStateInternal::verbose)
        dataLog("   Merging from ", pointerDump(from), " to ", pointerDump(to), "\n");
    ASSERT(from->variablesAtTail.numberOfArguments() == to->variablesAtHead.numberOfArguments());
    ASSERT(from->variablesAtTail.numberOfLocals() == to->variablesAtHead.numberOfLocals());
    
    bool changed = false;
    
    changed |= checkAndSet(
        to->cfaStructureClobberStateAtHead,
        DFG::merge(from->cfaStructureClobberStateAtTail, to->cfaStructureClobberStateAtHead));
    
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

        for (NodeAbstractValuePair& entry : to->ssa->valuesAtHead) {
            NodeFlowProjection node = entry.node;
            if (DFGInPlaceAbstractStateInternal::verbose)
                dataLog("      Merging for ", node, ": from ", forNode(node), " to ", entry.value, "\n");
#ifndef NDEBUG
            unsigned valueCountInFromBlock = 0;
            for (NodeAbstractValuePair& fromBlockValueAtTail : from->ssa->valuesAtTail) {
                if (fromBlockValueAtTail.node == node) {
                    ASSERT(fromBlockValueAtTail.value == forNode(node));
                    ++valueCountInFromBlock;
                }
            }
            ASSERT(valueCountInFromBlock == 1);
#endif

            changed |= entry.value.merge(forNode(node));

            if (DFGInPlaceAbstractStateInternal::verbose)
                dataLog("         Result: ", entry.value, "\n");
        }
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (!to->cfaHasVisited)
        changed = true;
    
    if (DFGInPlaceAbstractStateInternal::verbose)
        dataLog("      Will revisit: ", changed, "\n");
    to->cfaShouldRevisit |= changed;
    
    return changed;
}

inline bool InPlaceAbstractState::mergeToSuccessors(BasicBlock* basicBlock)
{
    Node* terminal = basicBlock->terminal();
    
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
    
    case EntrySwitch: {
        EntrySwitchData* data = terminal->entrySwitchData();
        bool changed = false;
        for (unsigned i = data->cases.size(); i--;)
            changed |= merge(basicBlock, data->cases[i]);
        return changed;
    }

    case Return:
    case TailCall:
    case DirectTailCall:
    case TailCallVarargs:
    case TailCallForwardVarargs:
    case Unreachable:
    case Throw:
    case ThrowStaticError:
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

