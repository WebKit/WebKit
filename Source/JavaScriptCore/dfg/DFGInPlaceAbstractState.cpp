/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include "DFGBasicBlock.h"
#include "JSCJSValueInlines.h"
#include <wtf/RecursableLambda.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC { namespace DFG {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InPlaceAbstractState);

namespace DFGInPlaceAbstractStateInternal {
static constexpr bool verbose = false;
}

InPlaceAbstractState::InPlaceAbstractState(Graph& graph)
    : m_graph(graph)
    , m_abstractValues(*graph.m_abstractValuesCache)
    , m_variables(OperandsLike, graph.block(0)->variablesAtHead)
    , m_tupleAbstractValues(graph.m_tupleData.size())
    , m_block(nullptr)
{
}

InPlaceAbstractState::~InPlaceAbstractState() = default;

void InPlaceAbstractState::beginBasicBlock(BasicBlock* basicBlock)
{
    ASSERT(!m_block);
    
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->valuesAtHead.numberOfLocals());
    ASSERT(basicBlock->variablesAtTail.numberOfLocals() == basicBlock->valuesAtTail.numberOfLocals());
    ASSERT(basicBlock->variablesAtHead.numberOfLocals() == basicBlock->variablesAtTail.numberOfLocals());
    ASSERT(basicBlock->variablesAtHead.numberOfTmps() == basicBlock->valuesAtHead.numberOfTmps());
    ASSERT(basicBlock->variablesAtTail.numberOfTmps() == basicBlock->valuesAtTail.numberOfTmps());
    ASSERT(basicBlock->variablesAtHead.numberOfTmps() == basicBlock->variablesAtTail.numberOfTmps());

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
    m_branchDirection = InvalidBranchDirection;
    m_structureClobberState = basicBlock->cfaStructureClobberStateAtHead;
}

static void setLiveValues(Vector<NodeAbstractValuePair>& values, const Vector<NodeFlowProjection>& live)
{
    values = live.map([](auto node) {
        return NodeAbstractValuePair { node, AbstractValue() };
    });
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
        for (size_t i = 0; i < entrypoint->valuesAtHead.numberOfTmps(); ++i) {
            entrypoint->valuesAtHead.tmp(i).clear();
            entrypoint->valuesAtTail.tmp(i).clear();
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
        block->cfaStructureClobberStateAtHead = StructuresAreWatched;
        block->cfaStructureClobberStateAtTail = StructuresAreWatched;
        for (size_t i = 0; i < block->valuesAtHead.size(); ++i) {
            block->valuesAtHead[i].clear();
            block->valuesAtTail[i].clear();
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
                destination = atIndex(index);
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
            
            block->valuesAtTail[i] = atIndex(i);
        }

        for (NodeAbstractValuePair& valueAtTail : block->ssa->valuesAtTail) {
            NodeFlowProjection& node = valueAtTail.node;
            if (node->isTuple()) {
                ASSERT(hasClearedAbstractState(node));
                valueAtTail.value = AbstractValue();
                continue;
            }
            valueAtTail.value = forNode(node);
        }
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
    m_block = nullptr;
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
    dataLogLnIf(DFGInPlaceAbstractStateInternal::verbose, "   Merging from ", pointerDump(from), " to ", pointerDump(to));
    ASSERT(from->variablesAtTail.numberOfArguments() == to->variablesAtHead.numberOfArguments());
    ASSERT(from->variablesAtTail.numberOfLocals() == to->variablesAtHead.numberOfLocals());
    ASSERT(from->variablesAtTail.numberOfTmps() == to->variablesAtHead.numberOfTmps());
    
    bool changed = false;
    
    changed |= checkAndSet(
        to->cfaStructureClobberStateAtHead,
        DFG::merge(from->cfaStructureClobberStateAtTail, to->cfaStructureClobberStateAtHead));
    
    switch (m_graph.m_form) {
    case ThreadedCPS: {
        for (size_t index = 0; index < from->variablesAtTail.size(); ++index) {
            AbstractValue& destination = to->valuesAtHead.at(index);
            changed |= mergeVariableBetweenBlocks(destination, from->valuesAtTail.at(index), to->variablesAtHead.at(index), from->variablesAtTail.at(index));
        }
        break;
    }
        
    case SSA: {
        for (size_t i = from->valuesAtTail.size(); i--;)
            changed |= to->valuesAtHead[i].merge(from->valuesAtTail[i]);

        for (NodeAbstractValuePair& entry : to->ssa->valuesAtHead) {
            NodeFlowProjection node = entry.node;
            dataLogLnIf(DFGInPlaceAbstractStateInternal::verbose, "      Merging for ", node, ": from ", forNode(node), " to ", entry.value);
#if ASSERT_ENABLED
            unsigned valueCountInFromBlock = 0;
            for (NodeAbstractValuePair& fromBlockValueAtTail : from->ssa->valuesAtTail) {
                if (fromBlockValueAtTail.node == node) {
                    if (node->isTuple())
                        ASSERT(hasClearedAbstractState(node) && !fromBlockValueAtTail.value);
                    ++valueCountInFromBlock;
                }
            }
            ASSERT(valueCountInFromBlock == 1);
#endif

            if (!node->isTuple())
                changed |= entry.value.merge(forNode(node));

            dataLogLnIf(DFGInPlaceAbstractStateInternal::verbose, "         Result: ", entry.value);
        }
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (!to->cfaHasVisited)
        changed = true;
    
    dataLogLnIf(DFGInPlaceAbstractStateInternal::verbose, "      Will revisit: ", changed);
    to->cfaShouldRevisit |= changed;
    
    return changed;
}

inline bool InPlaceAbstractState::mergeToSuccessors(BasicBlock* basicBlock)
{
    auto terminalAndIndex = basicBlock->findTerminal();
    Node* terminal = terminalAndIndex.node;
    unsigned terminalIndex = terminalAndIndex.index;
    
    ASSERT(terminal->isTerminal());
    
    switch (terminal->op()) {
    case Jump: {
        ASSERT(basicBlock->cfaBranchDirection == InvalidBranchDirection);
        return merge(basicBlock, terminal->targetBlock());
    }

    case Branch: {
        ASSERT(basicBlock->cfaBranchDirection != InvalidBranchDirection);
        bool changed = false;
        using ValueVector = Vector<std::tuple<Node*, AbstractValue>, 2>;
        using VariableVector = Vector<std::tuple<Operand, AbstractValue>, 2>;
        ValueVector trueNodes;
        ValueVector falseNodes;

        auto collectSparseConditionalPropagationValues = recursableLambda([&](auto self, Edge edge, ValueVector& trueNodes, ValueVector& falseNodes) -> void {
            auto constant = [&](Node* node, JSValue constant) {
                AbstractValue value;
                value.set(m_graph, *m_graph.freeze(constant), basicBlock->cfaStructureClobberStateAtTail);
                value.fixTypeForRepresentation(m_graph, node);
                return value;
            };

            switch (edge.useKind()) {
            case Int32Use:
                falseNodes.append({ edge.node(), constant(edge.node(), jsNumber(0)) });
                break;
            case DoubleRepUse: {
                AbstractValue trueValue = forNode(edge.node());
                // We know the input is not NaN if it has a truthy value.
                if (trueValue.filter(~SpecDoubleNaN) == FiltrationOK)
                    trueNodes.append({ edge.node(), WTFMove(trueValue) });
                break;
            }
            case StringUse:
                falseNodes.append({ edge.node(), constant(edge.node(), m_graph.m_vm.smallStrings.emptyString()) });
                break;
            case ObjectOrOtherUse: {
                if (!m_graph.isWatchingMasqueradesAsUndefinedWatchpointSet(edge.node()))
                    break;
                AbstractValue trueValue = forNode(edge);
                AbstractValue falseValue = trueValue;
                if (trueValue.filter(SpecObject) == FiltrationOK)
                    trueNodes.append({ edge.node(), WTFMove(trueValue) });
                if (falseValue.filter(SpecOther) == FiltrationOK)
                    falseNodes.append({ edge.node(), WTFMove(falseValue) });
                break;
            }
            case BooleanUse:
            case KnownBooleanUse:
                falseNodes.append({ edge.node(), constant(edge.node(), jsBoolean(false)) });
                trueNodes.append({ edge.node(), constant(edge.node(), jsBoolean(true)) });

                switch (edge->op()) {
                case CompareEq:
                case CompareStrictEq: {
                    Node* compare = edge.node();

                    bool isValidCompare = [&]() {
                        if (compare->op() == CompareStrictEq)
                            return true;
                        return compare->isBinaryUseKind(Int32Use)
#if USE(JSVALUE64)
                            || compare->isBinaryUseKind(Int52RepUse)
#endif
                            || compare->isBinaryUseKind(DoubleRepUse)
                            || compare->isBinaryUseKind(StringUse)
                            || compare->isBinaryUseKind(StringIdentUse)
                            || compare->isBinaryUseKind(BooleanUse)
                            || compare->isBinaryUseKind(ObjectUse)
                            || compare->isBinaryUseKind(SymbolUse);
                    }();
                    if (!isValidCompare)
                        break;

                    AbstractValue lhs = forNode(compare->child1());
                    AbstractValue rhs = forNode(compare->child2());
                    if ((lhs.m_type & SpecDoubleNaN) || (rhs.m_type & SpecDoubleNaN)) {
                        // We can't say anything useful about this comparison if either
                        // input could be NaN, since it doesn't tell us any information
                        // about the underlying values.
                        break;
                    }

                    if (lhs.filter(rhs) == FiltrationOK) {
                        if (JSValue value = lhs.value()) {
                            ASSERT(!(value.isDouble() && std::isnan(value.asDouble()))); // This is handled by the above NaN type check.
                            // 0.0 == -0.0, so we can't have this filter produce a constant
                            // if that constant is zero or negative zero.
                            if (value.isDouble() && value.asNumber() == 0.0)
                                lhs.clobberValue();
                        }
                        trueNodes.append({ compare->child1().node(), lhs });
                        trueNodes.append({ compare->child2().node(), WTFMove(lhs) });
                    }
                    break;
                }
                case LogicalNot: {
                    Node* notNode = edge.node();
                    trueNodes.append({ notNode, constant(notNode, jsBoolean(true)) });
                    falseNodes.append({ notNode, constant(notNode, jsBoolean(false)) });
                    self(edge->child1(), falseNodes, trueNodes);
                    break;
                }
                case IsEmpty:
                case TypeOfIsUndefined:
                case TypeOfIsObject:
                case IsUndefinedOrNull:
                case IsBoolean:
                case IsNumber:
                case IsBigInt:
                case NumberIsInteger:
                case IsObject:
                case TypeOfIsFunction:
                case IsCallable:
                case IsCellWithType:
                case IsTypedArrayView: {
                    SpeculatedType type = SpecNone;
                    bool canInvertTypeCheck = true;
                    switch (edge->op()) {
                    case IsEmpty:
                        type = SpecEmpty;
                        break;
                    case TypeOfIsUndefined: {
                        if (!m_graph.isWatchingMasqueradesAsUndefinedWatchpointSet(edge.node()))
                            break;
                        type = SpecOther;
                        // We can't invert the result of this type check because if TypeOfIsUndefined is true, the input
                        // is definitely SpecOther. But because `null` is also SpecOther, if TypeOfIsUndefined is false,
                        // then ~SpecOther is not guaranteed to hold (specifically, when the input is `null`).
                        canInvertTypeCheck = false;
                        break;
                    }
                    case TypeOfIsObject:
                        type = ((SpecObject - SpecFunction) | SpecOther);
                        canInvertTypeCheck = false;
                        break;
                    case IsUndefinedOrNull:
                        type = SpecOther;
                        break;
                    case IsBoolean:
                        type = SpecBoolean;
                        break;
                    case IsNumber:
                        type = SpecFullNumber;
                        break;
                    case IsBigInt:
                        type = SpecBigInt;
                        break;
                    case NumberIsInteger:
                        type = SpecFullNumber;
                        canInvertTypeCheck = false;
                        break;
                    case IsObject:
                        type = SpecObject;
                        break;
                    case TypeOfIsFunction:
                    case IsCallable:
                        type = SpecTypeofMightBeFunction;
                        canInvertTypeCheck = false;
                        break;
                    case IsCellWithType: {
                        std::optional<SpeculatedType> filter = edge->speculatedTypeForQuery();
                        if (!filter) {
                            type = SpecCell;
                            canInvertTypeCheck = false;
                            break;
                        }
                        type = filter.value();
                        canInvertTypeCheck = false;
                        break;
                    }
                    case IsTypedArrayView:
                        type = SpecTypedArrayView;
                        break;
                    default:
                        break;
                    }

                    if (type) {
                        Node* argument = edge->child1().node();
                        AbstractValue trueValue = forNode(argument);
                        AbstractValue falseValue = trueValue;
                        if (trueValue.filter(type) == FiltrationOK)
                            trueNodes.append({ argument, WTFMove(trueValue) });
                        if (canInvertTypeCheck && falseValue.filter(~type) == FiltrationOK)
                            falseNodes.append({ argument, WTFMove(falseValue) });
                    }
                    break;
                }
                default:
                    break;
                }

                break;
            default:
                break;
            }
        });

        if (basicBlock->cfaBranchDirection == TakeBoth && basicBlock->cfaDidFinish)
            collectSparseConditionalPropagationValues(terminal->child1(), trueNodes, falseNodes);

        auto applyChanges = [&](const ValueVector& nodes) -> std::tuple<ValueVector, VariableVector> {
            switch (m_graph.m_form) {
            case SSA: {
                ValueVector valuesToRestore;
                valuesToRestore.reserveInitialCapacity(nodes.size());
                for (auto& [node, refinedValue] : nodes) {
                    AbstractValue& value = forNode(node);
                    valuesToRestore.append({ node, value });
                    value = refinedValue;
                }
                return std::tuple { WTFMove(valuesToRestore), VariableVector { } };
            }
            case ThreadedCPS: {
                auto forEachOperand = [&](const ValueVector& nodes, auto func) {
                    if (nodes.isEmpty())
                        return;

                    Vector<Operand, 16> toSkip;
                    for (unsigned index = terminalIndex, count = 0; index-- && count < 16; ++count) {
                        auto handleNode = [&](Node* node, Operand operand) {
                            size_t index = nodes.findIf(
                                [&](auto& entry) {
                                    return std::get<0>(entry) == node;
                                });
                            if (index == notFound)
                                return;
                            if (toSkip.contains(operand))
                                return;
                            func(operand, std::get<1>(nodes[index]));
                        };

                        Node* node = basicBlock->at(index);
                        if (node->op() == GetLocal)
                            handleNode(node, node->operand());
                        else if (node->op() == Flush)
                            handleNode(node->child1().node(), node->operand());
                        else if (node->op() == SetLocal) {
                            Operand operand = node->operand();
                            handleNode(node->child1().node(), operand);
                            toSkip.append(operand);
                        }
                    }
                };

                VariableVector variablesToRestore;
                forEachOperand(nodes, [&](Operand operand, const AbstractValue& newValue) {
                    AbstractValue& value = basicBlock->valuesAtTail.operand(operand);
                    variablesToRestore.append({ operand, value });
                    value = newValue;
                });
                return std::tuple { ValueVector { }, WTFMove(variablesToRestore) };
            }
            default:
                return std::tuple { ValueVector { }, VariableVector { } };
            }
        };

        auto restoreState = [&](const std::tuple<ValueVector, VariableVector>& entry) {
            switch (m_graph.m_form) {
            case SSA: {
                for (auto& [node, value] : std::get<0>(entry))
                    forNode(node) = value;
                break;
            }
            case ThreadedCPS: {
                for (auto& [operand, value] : std::get<1>(entry))
                    basicBlock->valuesAtTail.operand(operand) = value;
                break;
            }
            default:
                break;
            }
        };

        if (basicBlock->cfaBranchDirection != TakeFalse) {
            auto state = applyChanges(trueNodes);
            changed |= merge(basicBlock, terminal->branchData()->taken.block);
            restoreState(state);
        }

        if (basicBlock->cfaBranchDirection != TakeTrue) {
            auto state = applyChanges(falseNodes);
            changed |= merge(basicBlock, terminal->branchData()->notTaken.block);
            restoreState(state);
        }

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

