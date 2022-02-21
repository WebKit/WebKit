/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "DFGValidate.h"

#if ENABLE(DFG_JIT)

#include "ButterflyInlines.h"
#include "CacheableIdentifierInlines.h"
#include "DFGClobberize.h"
#include "DFGClobbersExitState.h"
#include "DFGDominators.h"
#include "DFGMayExit.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include <wtf/Assertions.h>

namespace JSC { namespace DFG {

namespace {

class Validate {
public:
    Validate(Graph& graph, GraphDumpMode graphDumpMode, CString graphDumpBeforePhase)
        : m_graph(graph)
        , m_graphDumpMode(graphDumpMode)
        , m_graphDumpBeforePhase(graphDumpBeforePhase)
    {
    }
    
    #define VALIDATE(context, assertion) do { \
        if (!(assertion)) { \
            startCrashing(); \
            dataLogF("\n\n\nAt "); \
            reportValidationContext context; \
            dataLogF(": validation failed: %s (%s:%d).\n", #assertion, __FILE__, __LINE__); \
            dumpGraphIfAppropriate(); \
            WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion); \
            CRASH(); \
        } \
    } while (0)
    
    #define V_EQUAL(context, left, right) do { \
        if (left != right) { \
            startCrashing(); \
            dataLogF("\n\n\nAt "); \
            reportValidationContext context; \
            dataLogF(": validation failed: (%s = ", #left); \
            dataLog(left); \
            dataLogF(") == (%s = ", #right); \
            dataLog(right); \
            dataLogF(") (%s:%d).\n", __FILE__, __LINE__); \
            dataLog("\n\n\n"); \
            m_graph.baselineCodeBlockFor(nullptr)->dumpBytecode(); \
            dumpGraphIfAppropriate(); \
            WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #left " == " #right); \
            CRASH(); \
        } \
    } while (0)

    #define notSet (static_cast<size_t>(-1))
        
    void validate()
    {
        if (m_graph.m_isValidating)
            return;

        auto isValidating = SetForScope(m_graph.m_isValidating, true);

        // NB. This code is not written for performance, since it is not intended to run
        // in release builds.

        VALIDATE((m_graph.block(0)), m_graph.isRoot(m_graph.block(0)));
        VALIDATE((m_graph.block(0)), m_graph.block(0) == m_graph.m_roots[0]);

        for (BasicBlock* block : m_graph.m_roots)
            VALIDATE((block), block->predecessors.isEmpty());

        // Validate that all local variables at the head of all entrypoints are dead.
        for (BasicBlock* entrypoint : m_graph.m_roots) {
            for (unsigned i = 0; i < entrypoint->variablesAtHead.numberOfLocals(); ++i)
                V_EQUAL((virtualRegisterForLocal(i), entrypoint), static_cast<Node*>(nullptr), entrypoint->variablesAtHead.local(i));
        }
        
        // Validate ref counts and uses.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            VALIDATE((block), block->isReachable);
            for (size_t i = 0; i < block->numNodes(); ++i)
                m_myRefCounts.add(block->node(i), 0);
        }
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (size_t i = 0; i < block->numNodes(); ++i) {
                Node* node = block->node(i);
                m_acceptableNodes.add(node);
                if (!node->shouldGenerate())
                    continue;
                if (node->op() == Upsilon) {
                    VALIDATE((node), m_graph.m_form == SSA);
                    if (node->phi()->shouldGenerate())
                        m_myRefCounts.find(node)->value++;
                }
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    // Phi children in LoadStore form are invalid.
                    if (m_graph.m_form == LoadStore && block->isPhiIndex(i))
                        continue;
                    
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    
                    m_myRefCounts.find(edge.node())->value++;

                    validateEdgeWithDoubleResultIfNecessary(node, edge);
                    validateEdgeWithInt52ResultIfNecessary(node, edge);
                    
                    if (m_graph.m_form == SSA) {
                        // In SSA, all edges must hasResult().
                        VALIDATE((node, edge), edge->hasResult());
                        continue;
                    }
                    
                    // Unless I'm a Flush, Phantom, GetLocal, or Phi, my children should hasResult().
                    switch (node->op()) {
                    case Flush:
                    case GetLocal:
                        VALIDATE((node, edge), edge->hasVariableAccessData(m_graph));
                        VALIDATE((node, edge), edge->variableAccessData() == node->variableAccessData());
                        break;
                    case PhantomLocal:
                        VALIDATE((node, edge), edge->hasVariableAccessData(m_graph));
                        VALIDATE((node, edge), edge->variableAccessData() == node->variableAccessData());
                        VALIDATE((node, edge), edge->op() != SetLocal);
                        break;
                    case Phi:
                        VALIDATE((node, edge), edge->hasVariableAccessData(m_graph));
                        if (m_graph.m_unificationState == LocallyUnified)
                            break;
                        VALIDATE((node, edge), edge->variableAccessData() == node->variableAccessData());
                        break;
                    default:
                        VALIDATE((node, edge), edge->hasResult());
                        break;
                    }
                }
            }
        }
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (size_t i = 0; i < block->numNodes(); ++i) {
                Node* node = block->node(i);
                if (m_graph.m_refCountState == ExactRefCount)
                    V_EQUAL((node), m_myRefCounts.get(node), node->adjustedRefCount());
            }
            
            bool foundTerminal = false;
            for (size_t i = 0 ; i < block->size(); ++i) {
                Node* node = block->at(i);
                if (node->isTerminal()) {
                    foundTerminal = true;
                    for (size_t j = i + 1; j < block->size(); ++j) {
                        node = block->at(j);
                        VALIDATE((node), node->op() == Phantom || node->op() == PhantomLocal || node->op() == Flush || node->op() == Check);
                        m_graph.doToChildren(
                            node,
                            [&] (Edge edge) {
                                VALIDATE((node, edge), shouldNotHaveTypeCheck(edge.useKind()));
                            });
                    }
                    break;
                }
            }
            VALIDATE((block), foundTerminal);
            
            for (size_t i = 0; i < block->size(); ++i) {
                Node* node = block->at(i);

                VALIDATE((node), node->origin.isSet());
                VALIDATE((node), node->origin.semantic.isSet() == node->origin.forExit.isSet());
                VALIDATE((node), !(!node->origin.forExit.isSet() && node->origin.exitOK));
                VALIDATE((node), !(mayExit(m_graph, node) == Exits && !node->origin.exitOK));

                if (i) {
                    Node* previousNode = block->at(i - 1);
                    VALIDATE(
                        (node),
                        !clobbersExitState(m_graph, previousNode)
                        || !node->origin.exitOK
                        || node->op() == ExitOK
                        || node->origin.forExit != previousNode->origin.forExit);
                    VALIDATE(
                        (node),
                        !(!previousNode->origin.exitOK && node->origin.exitOK)
                        || node->op() == ExitOK
                        || node->origin.forExit != previousNode->origin.forExit);
                }
                
                VALIDATE((node), !node->hasStructure() || !!node->structure().get());
                VALIDATE((node), !node->hasCellOperand() || node->cellOperand()->value().isCell());
                VALIDATE((node), !node->hasCellOperand() || !!node->cellOperand()->value());
                
                if (!(node->flags() & NodeHasVarArgs)) {
                    if (!node->child2())
                        VALIDATE((node), !node->child3());
                    if (!node->child1())
                        VALIDATE((node), !node->child2());
                }

                if (node->hasCacheableIdentifier()) {
                    auto* uid = node->cacheableIdentifier().uid();
                    VALIDATE((node), uid->isSymbol() || !parseIndex(*uid));
                }
                 
                switch (node->op()) {
                case Identity:
                case IdentityWithProfile:
                    VALIDATE((node), canonicalResultRepresentation(node->result()) == canonicalResultRepresentation(node->child1()->result()));
                    break;
                case SetLocal:
                case PutStack:
                case Upsilon:
                    VALIDATE((node), !!node->child1());
                    switch (node->child1().useKind()) {
                    case UntypedUse:
                    case CellUse:
                    case KnownCellUse:
                    case Int32Use:
                    case KnownInt32Use:
                    case Int52RepUse:
                    case DoubleRepUse:
                    case BooleanUse:
                    case KnownBooleanUse:
                        break;
                    default:
                        VALIDATE((node), !"Bad use kind");
                        break;
                    }
                    break;
                case MakeRope:
                case ValueAdd:
                case ValueSub:
                case ValueMul:
                case ValueDiv:
                case ValueMod:
                case ValuePow:
                case ArithAdd:
                case ArithSub:
                case ArithMul:
                case ArithIMul:
                case ArithDiv:
                case ArithMod:
                case ArithMin:
                case ArithMax:
                case ArithPow:
                case CompareLess:
                case CompareLessEq:
                case CompareGreater:
                case CompareGreaterEq:
                case CompareBelow:
                case CompareBelowEq:
                case CompareEq:
                case CompareStrictEq:
                case SameValue:
                case StrCat:
                    VALIDATE((node), !!node->child1());
                    VALIDATE((node), !!node->child2());
                    break;
                case CompareEqPtr:
                    VALIDATE((node), !!node->child1());
                    VALIDATE((node), !!node->cellOperand()->value() && node->cellOperand()->value().isCell());
                    break;
                case CheckArrayOrEmpty:
                    VALIDATE((node), is64Bit());
                    VALIDATE((node), !!node->child1());
                    VALIDATE((node), node->child1().useKind() == CellUse);
                    break;
                case CheckStructureOrEmpty:
                    VALIDATE((node), is64Bit());
                    VALIDATE((node), !!node->child1());
                    VALIDATE((node), node->child1().useKind() == CellUse);
                    break;
                case CheckStructure:
                case StringFromCharCode:
                    VALIDATE((node), !!node->child1());
                    break;
                case PutStructure:
                    VALIDATE((node), !node->transition()->previous->dfgShouldWatch());
                    break;
                case MultiPutByOffset:
                    for (unsigned i = node->multiPutByOffsetData().variants.size(); i--;) {
                        const PutByVariant& variant = node->multiPutByOffsetData().variants[i];
                        if (variant.kind() != PutByVariant::Transition)
                            continue;
                        VALIDATE((node), !variant.oldStructureForTransition()->dfgShouldWatch());
                    }
                    break;
                case MultiDeleteByOffset:
                    for (unsigned i = node->multiDeleteByOffsetData().variants.size(); i--;) {
                        const DeleteByVariant& variant = node->multiDeleteByOffsetData().variants[i];
                        VALIDATE((node), !variant.newStructure() || !variant.oldStructure()->dfgShouldWatch());
                    }
                    break;
                case MaterializeNewObject:
                    for (RegisteredStructure structure : node->structureSet()) {
                        // This only supports structures that are JSFinalObject or JSArray.
                        VALIDATE(
                            (node),
                            structure->classInfo() == JSFinalObject::info()
                            || structure->classInfo() == JSArray::info());

                        // We only support certain indexing shapes.
                        VALIDATE((node), !hasAnyArrayStorage(structure->indexingType()));
                    }
                    break;
                case DoubleConstant:
                case Int52Constant:
                    VALIDATE((node), node->isNumberConstant());
                    break;
                case GetByOffset:
                case PutByOffset:
                    // FIXME: We should be able to validate that GetByOffset and PutByOffset are
                    // using the same object for storage and base. I think this means finally
                    // splitting these nodes into two node types, one for inline and one for
                    // out-of-line. The out-of-line one will require that the first node is storage,
                    // while the inline one will not take a storage child at all.
                    // https://bugs.webkit.org/show_bug.cgi?id=159602
                    break;
                case HasOwnProperty: {
                    VALIDATE((node), !!m_graph.m_vm.hasOwnPropertyCache());
                    break;
                }
                case GetVectorLength: {
                    Array::Type type = node->arrayMode().type();
                    VALIDATE((node), type == Array::ArrayStorage || type == Array::SlowPutArrayStorage);
                    break;
                }
                case CPUIntrinsic: {
                    switch (node->intrinsic()) {
                    case CPUMfenceIntrinsic:
                    case CPURdtscIntrinsic:
                    case CPUCpuidIntrinsic:
                    case CPUPauseIntrinsic:
                        break;
                    default:
                        VALIDATE((node), false);
                        break;
                    }
                    break;
                }
                case GetArgumentCountIncludingThis: {
                    if (InlineCallFrame* inlineCallFrame = node->argumentsInlineCallFrame())
                        VALIDATE((node), inlineCallFrame->isVarargs());
                    break;
                }
                case NewArray:
                    VALIDATE((node), node->vectorLengthHint() >= node->numChildren());
                    break;
                case NewArrayBuffer:
                    VALIDATE((node), node->vectorLengthHint() >= node->castOperand<JSImmutableButterfly*>()->length());
                    break;
                case GetByVal:
                    switch (node->arrayMode().type()) {
                    case Array::Int32:
                    case Array::Double:
                    case Array::Contiguous:
                        // We rely on being an original array structure because we are SaneChain, and we need 
                        // Array.prototype to be our prototype, so we can return undefined when we go OOB.
                        if (node->arrayMode().isOutOfBoundsSaneChain())
                            VALIDATE((node), node->arrayMode().isJSArrayWithOriginalStructure());
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        
        switch (m_graph.m_form) {
        case LoadStore:
        case ThreadedCPS:
            validateCPS();
            break;
            
        case SSA:
            validateSSA();
            break;
        }

        // Validate clobbered states.
        struct DefLambdaAdaptor {
            Function<void(PureValue)> pureValue;
            Function<void(HeapLocation, LazyNode)> locationAndNode;

            void operator()(PureValue value) const
            {
                pureValue(value);
            }

            void operator()(HeapLocation location, LazyNode node) const
            {
                locationAndNode(location, node);
            }
        };
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                clobberize(m_graph, node,
                    [&] (AbstractHeap) { },
                    [&] (AbstractHeap heap)
                    {
                        // CSE assumes that HEAP TOP is never written.
                        // If this assumption is weakened, you need to update clobbering
                        // in CSE accordingly.
                        if (heap.kind() == Stack)
                            VALIDATE((node), !heap.payload().isTop());
                    },
                    DefLambdaAdaptor {
                        [&] (PureValue) { },
                        [&] (HeapLocation location, LazyNode)
                        {
                            VALIDATE((node), location.heap().kind() != SideState);

                            // More specific kinds should be used instead.
                            VALIDATE((node), location.heap().kind() != World);
                            VALIDATE((node), location.heap().kind() != Heap);
                        }
                });
            }
        }

        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            // We expect the predecessor list to be de-duplicated.
            HashSet<BasicBlock*> predecessors;
            for (BasicBlock* predecessor : block->predecessors)
                predecessors.add(predecessor);
            VALIDATE((block), predecessors.size() == block->predecessors.size());
        }
    }
    
private:
    Graph& m_graph;
    GraphDumpMode m_graphDumpMode;
    CString m_graphDumpBeforePhase;
    
    HashMap<Node*, unsigned> m_myRefCounts;
    HashSet<Node*> m_acceptableNodes;
    
    void validateCPS()
    {
        VALIDATE((), !m_graph.m_rootToArguments.isEmpty()); // We should have at least one root.
        VALIDATE((), m_graph.m_rootToArguments.size() == m_graph.m_roots.size());
        for (BasicBlock* root : m_graph.m_rootToArguments.keys())
            VALIDATE((), m_graph.m_roots.contains(root));

        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            HashSet<Node*> phisInThisBlock;
            HashSet<Node*> nodesInThisBlock;
            
            for (size_t i = 0; i < block->numNodes(); ++i) {
                Node* node = block->node(i);
                nodesInThisBlock.add(node);
                if (block->isPhiIndex(i))
                    phisInThisBlock.add(node);
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    VALIDATE((node, edge), m_acceptableNodes.contains(edge.node()));
                }
            }

            {
                HashSet<Node*> seenNodes;
                for (size_t i = 0; i < block->size(); ++i) {
                    Node* node = block->at(i);
                    m_graph.doToChildren(node, [&] (const Edge& edge) {
                        Node* child = edge.node();
                        VALIDATE((node, edge), block->isInPhis(child) || seenNodes.contains(child));
                    });
                    seenNodes.add(node);
                }
            }
            
            for (size_t i = 0; i < block->phis.size(); ++i) {
                Node* node = block->phis[i];
                ASSERT(phisInThisBlock.contains(node));
                VALIDATE((node), node->op() == Phi);
                Operand operand = node->operand();
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    // Phi children in LoadStore form are invalid.
                    if (m_graph.m_form == LoadStore && block->isPhiIndex(i))
                        continue;
                    
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    
                    VALIDATE(
                        (node, edge),
                        edge->op() == SetLocal
                        || edge->op() == SetArgumentDefinitely
                        || edge->op() == SetArgumentMaybe
                        || edge->op() == Phi);
                    
                    if (phisInThisBlock.contains(edge.node()))
                        continue;
                    
                    if (nodesInThisBlock.contains(edge.node())) {
                        VALIDATE(
                            (node, edge),
                            edge->op() == SetLocal
                            || edge->op() == SetArgumentDefinitely
                            || edge->op() == SetArgumentMaybe);
                        
                        continue;
                    }
                    
                    // There must exist a predecessor block that has this node index in
                    // its tail variables.
                    bool found = false;
                    for (unsigned k = 0; k < block->predecessors.size(); ++k) {
                        BasicBlock* prevBlock = block->predecessors[k];
                        VALIDATE((block->predecessors[k]), prevBlock);
                        Node* prevNode = prevBlock->variablesAtTail.operand(operand);
                        // If we have a Phi that is not referring to *this* block then all predecessors
                        // must have that local available.
                        VALIDATE((operand, block, block->predecessors[k]), prevNode);
                        switch (prevNode->op()) {
                        case GetLocal:
                        case Flush:
                        case PhantomLocal:
                            prevNode = prevNode->child1().node();
                            break;
                        default:
                            break;
                        }
                        if (node->shouldGenerate()) {
                            VALIDATE((operand, block->predecessors[k], prevNode),
                                     prevNode->shouldGenerate());
                        }
                        VALIDATE(
                            (operand, block->predecessors[k], prevNode),
                            prevNode->op() == SetLocal
                            || prevNode->op() == SetArgumentDefinitely
                            || prevNode->op() == SetArgumentMaybe
                            || prevNode->op() == Phi);
                        if (prevNode == edge.node()) {
                            found = true;
                            break;
                        }
                        // At this point it cannot refer into this block.
                        VALIDATE((operand, block->predecessors[k], prevNode), !prevBlock->isInBlock(edge.node()));
                    }
                    
                    VALIDATE((node, edge), found);
                }
            }
            
            Operands<size_t> getLocalPositions(OperandsLike, block->variablesAtHead);
            Operands<size_t> setLocalPositions(OperandsLike, block->variablesAtHead);
            
            for (size_t i = 0; i < block->variablesAtHead.numberOfTmps(); ++i) {
                VALIDATE((Operand::tmp(i), block), !block->variablesAtHead.tmp(i) || block->variablesAtHead.tmp(i)->accessesStack(m_graph));
                if (m_graph.m_form == ThreadedCPS)
                    VALIDATE((Operand::tmp(i), block), !block->variablesAtTail.tmp(i) || block->variablesAtTail.tmp(i)->accessesStack(m_graph));

                getLocalPositions.tmp(i) = notSet;
                setLocalPositions.tmp(i) = notSet;
            }
            for (size_t i = 0; i < block->variablesAtHead.numberOfArguments(); ++i) {
                VALIDATE((virtualRegisterForArgumentIncludingThis(i), block), !block->variablesAtHead.argument(i) || block->variablesAtHead.argument(i)->accessesStack(m_graph));
                if (m_graph.m_form == ThreadedCPS)
                    VALIDATE((virtualRegisterForArgumentIncludingThis(i), block), !block->variablesAtTail.argument(i) || block->variablesAtTail.argument(i)->accessesStack(m_graph));
                
                getLocalPositions.argument(i) = notSet;
                setLocalPositions.argument(i) = notSet;
            }
            for (size_t i = 0; i < block->variablesAtHead.numberOfLocals(); ++i) {
                VALIDATE((virtualRegisterForLocal(i), block), !block->variablesAtHead.local(i) || block->variablesAtHead.local(i)->accessesStack(m_graph));
                if (m_graph.m_form == ThreadedCPS)
                    VALIDATE((virtualRegisterForLocal(i), block), !block->variablesAtTail.local(i) || block->variablesAtTail.local(i)->accessesStack(m_graph));

                getLocalPositions.local(i) = notSet;
                setLocalPositions.local(i) = notSet;
            }
            
            for (size_t i = 0; i < block->size(); ++i) {
                Node* node = block->at(i);
                ASSERT(nodesInThisBlock.contains(node));
                VALIDATE((node), node->op() != Phi);
                VALIDATE((node), node->origin.forExit.isSet());
                for (unsigned j = 0; j < m_graph.numChildren(node); ++j) {
                    Edge edge = m_graph.child(node, j);
                    if (!edge)
                        continue;
                    VALIDATE((node, edge), nodesInThisBlock.contains(edge.node()));
                    switch (node->op()) {
                    case PhantomLocal:
                    case GetLocal:
                    case Flush:
                        break;
                    default:
                        VALIDATE((node, edge), !phisInThisBlock.contains(edge.node()));
                        break;
                    }
                }
                
                switch (node->op()) {
                case Phi:
                case Upsilon:
                case AssertInBounds:
                case CheckInBounds:
                case CheckInBoundsInt52:
                case PhantomNewObject:
                case PhantomNewFunction:
                case PhantomNewGeneratorFunction:
                case PhantomNewAsyncFunction:
                case PhantomNewAsyncGeneratorFunction:
                case PhantomCreateActivation:
                case PhantomNewRegexp:
                case GetMyArgumentByVal:
                case GetMyArgumentByValOutOfBounds:
                case PutHint:
                case CheckStructureImmediate:
                case MaterializeCreateActivation:
                case PutStack:
                case KillStack:
                case GetStack:
                case EntrySwitch:
                case InitializeEntrypointArguments:
                    VALIDATE((node), !"unexpected node type in CPS");
                    break;
                case MaterializeNewObject: {
                    // CPS only allows array lengths to be constant. This constraint only exists
                    // because we don't have DFG support for anything more and we don't need any
                    // other kind of support for now.
                    ObjectMaterializationData& data = node->objectMaterializationData();
                    for (unsigned i = data.m_properties.size(); i--;) {
                        PromotedLocationDescriptor descriptor = data.m_properties[i];
                        Edge edge = m_graph.varArgChild(node, 1 + i);
                        switch (descriptor.kind()) {
                        case PublicLengthPLoc:
                        case VectorLengthPLoc:
                            VALIDATE((node, edge), edge->isInt32Constant());
                            break;
                        default:
                            break;
                        }
                    }

                    // CPS only allows one structure.
                    VALIDATE((node), node->structureSet().size() == 1);

                    // CPS disallows int32 and double arrays. Those require weird type checks and
                    // conversions. They are not needed in the DFG right now. We should add support
                    // for these if the DFG ever needs it.
                    for (RegisteredStructure structure : node->structureSet()) {
                        VALIDATE((node), !hasInt32(structure->indexingType()));
                        VALIDATE((node), !hasDouble(structure->indexingType()));
                    }
                    break;
                }
                case Phantom:
                    VALIDATE((node), m_graph.m_fixpointState != FixpointNotConverged);
                    break;
                default:
                    break;
                }
                
                if (!node->shouldGenerate())
                    continue;
                switch (node->op()) {
                case GetLocal:
                    // Ignore GetLocal's that we know to be dead, but that the graph
                    // doesn't yet know to be dead.
                    if (!m_myRefCounts.get(node))
                        break;
                    if (m_graph.m_form == ThreadedCPS) {
                        VALIDATE((node, block), getLocalPositions.operand(node->operand()) == notSet);
                        VALIDATE((node, block), !!node->child1());
                        VALIDATE((node, block), node->child1()->op() == SetArgumentDefinitely || node->child1()->op() == Phi);
                    }
                    getLocalPositions.operand(node->operand()) = i;
                    break;
                case SetLocal:
                    // Only record the first SetLocal. There may be multiple SetLocals
                    // because of flushing.
                    if (setLocalPositions.operand(node->operand()) != notSet)
                        break;
                    setLocalPositions.operand(node->operand()) = i;
                    break;
                case SetArgumentDefinitely:
                    // This acts like a reset. It's ok to have a second GetLocal for a local in the same
                    // block if we had a SetArgumentDefinitely for that local.
                    getLocalPositions.operand(node->operand()) = notSet;
                    setLocalPositions.operand(node->operand()) = notSet;
                    break;
                case SetArgumentMaybe:
                    break;
                case Flush:
                case PhantomLocal:
                    if (m_graph.m_form == ThreadedCPS) {
                        VALIDATE((node, block), 
                            node->child1()->op() == Phi
                            || node->child1()->op() == SetLocal
                            || node->child1()->op() == SetArgumentDefinitely
                            || node->child1()->op() == SetArgumentMaybe);
                        if (node->op() == PhantomLocal)
                            VALIDATE((node, block), node->child1()->op() != SetArgumentMaybe);
                    }
                    break;
                default:
                    break;
                }
            }
            
            if (m_graph.m_form == LoadStore)
                continue;
            
            for (size_t i = 0; i < block->variablesAtHead.numberOfTmps(); ++i) {
                checkOperand(
                    block, getLocalPositions, setLocalPositions, Operand::tmp(i));
            }

            for (size_t i = 0; i < block->variablesAtHead.numberOfArguments(); ++i) {
                checkOperand(
                    block, getLocalPositions, setLocalPositions, virtualRegisterForArgumentIncludingThis(i));
            }
            for (size_t i = 0; i < block->variablesAtHead.numberOfLocals(); ++i) {
                checkOperand(
                    block, getLocalPositions, setLocalPositions, virtualRegisterForLocal(i));
            }
        }

        if (m_graph.m_form == ThreadedCPS) {
            Vector<Node*> worklist;
            HashSet<Node*> seen;
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                for (Node* node : *block) {
                    if (node->op() == GetLocal || node->op() == PhantomLocal) {
                        worklist.append(node);
                        auto addResult = seen.add(node);
                        VALIDATE((node, block), addResult.isNewEntry);
                    }
                }
            }

            while (worklist.size()) {
                Node* node = worklist.takeLast();
                switch (node->op()) {
                case PhantomLocal:
                case GetLocal: {
                    Node* child = node->child1().node();
                    if (seen.add(child).isNewEntry)
                        worklist.append(child);
                    break;
                }
                case Phi: {
                    for (unsigned i = 0; i < m_graph.numChildren(node); ++i) {
                        Edge edge = m_graph.child(node, i);
                        if (!edge)
                            continue;
                        if (seen.add(edge.node()).isNewEntry)
                            worklist.append(edge.node());
                    }
                    break;
                }
                case SetLocal:
                case SetArgumentDefinitely:
                    break;
                case SetArgumentMaybe:
                    VALIDATE((node), !"Should not reach SetArgumentMaybe. GetLocal that has data flow that reaches a SetArgumentMaybe is invalid IR.");
                    break;
                default:
                    VALIDATE((node), !"Unexpected node type.");
                    break;
                }
            }
        }
    }
    
    void validateSSA()
    {
        // FIXME: Add more things here.
        // https://bugs.webkit.org/show_bug.cgi?id=123471
        
        VALIDATE((), m_graph.m_roots.size() == 1);
        VALIDATE((), m_graph.m_roots[0] == m_graph.block(0));
        VALIDATE((), !m_graph.m_argumentFormats.isEmpty()); // We always have at least one entrypoint.
        VALIDATE((), m_graph.m_rootToArguments.isEmpty()); // This is only used in CPS.

        m_graph.initializeNodeOwners();

        auto& dominators = m_graph.ensureSSADominators();

        if (Options::validateFTLOSRExitLiveness())
            validateOSRExitAvailability(m_graph);

        for (unsigned entrypointIndex : m_graph.m_entrypointIndexToCatchBytecodeIndex.keys())
            VALIDATE((), entrypointIndex > 0); // By convention, 0 is the entrypoint index for the op_enter entrypoint, which can not be in a catch.

        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            VALIDATE((block), block->phis.isEmpty());

            bool isOSRExited = false;
            
            HashSet<Node*> nodesInThisBlock;

            for (auto* node : *block) {
                switch (node->op()) {
                case Phi:
                    // Phi cannot exit, and it would be wrong to hoist anything to the Phi that could
                    // exit.
                    VALIDATE((node), !node->origin.exitOK);
                    break;
                    
                case GetLocal:
                case SetLocal:
                case SetArgumentDefinitely:
                case SetArgumentMaybe:
                case Phantom:
                    VALIDATE((node), !"bad node type for SSA");
                    break;

                default:
                    // FIXME: Add more things here.
                    // https://bugs.webkit.org/show_bug.cgi?id=123471
                    break;
                }

                if (isOSRExited)
                    continue;
                switch (node->op()) {
                case PhantomNewObject:
                case PhantomNewFunction:
                case PhantomNewGeneratorFunction:
                case PhantomNewAsyncFunction:
                case PhantomNewAsyncGeneratorFunction:
                case PhantomCreateActivation:
                case PhantomDirectArguments:
                case PhantomCreateRest:
                case PhantomClonedArguments:
                case PhantomNewRegexp:
                case MovHint:
                case Upsilon:
                case ForwardVarargs:
                case CallForwardVarargs:
                case TailCallForwardVarargs:
                case TailCallForwardVarargsInlinedCaller:
                case ConstructForwardVarargs:
                case GetMyArgumentByVal:
                case GetMyArgumentByValOutOfBounds:
                    break;

                case Check:
                case CheckVarargs:
                    // FIXME: This is probably not correct.
                    break;

                case PutHint:
                    VALIDATE((node), node->child1()->isPhantomAllocation());
                    break;

                case PhantomSpread:
                    VALIDATE((node), m_graph.m_form == SSA);
                    // We currently support PhantomSpread over PhantomCreateRest and PhantomNewArrayBuffer.
                    VALIDATE((node), node->child1()->op() == PhantomCreateRest || node->child1()->op() == PhantomNewArrayBuffer);
                    break;

                case PhantomNewArrayWithSpread: {
                    VALIDATE((node), m_graph.m_form == SSA);
                    BitVector* bitVector = node->bitVector();
                    for (unsigned i = 0; i < node->numChildren(); i++) {
                        Node* child = m_graph.varArgChild(node, i).node();
                        if (bitVector->get(i)) {
                            // We currently support PhantomSpread over PhantomCreateRest and PhantomNewArrayBuffer.
                            VALIDATE((node), child->op() == PhantomSpread);
                        } else
                            VALIDATE((node), !child->isPhantomAllocation());
                    }
                    break;
                }

                case PhantomNewArrayBuffer:
                    VALIDATE((node), m_graph.m_form == SSA);
                    VALIDATE((node), node->vectorLengthHint() >= node->castOperand<JSImmutableButterfly*>()->length());
                    break;

                case NewArrayWithSpread: {
                    BitVector* bitVector = node->bitVector();
                    for (unsigned i = 0; i < node->numChildren(); i++) {
                        Node* child = m_graph.varArgChild(node, i).node();
                        if (child->isPhantomAllocation()) {
                            VALIDATE((node), bitVector->get(i));
                            VALIDATE((node), m_graph.m_form == SSA);
                            VALIDATE((node), child->op() == PhantomSpread);
                        }
                    }
                    break;
                }

                case Spread:
                    VALIDATE((node), !node->child1()->isPhantomAllocation() || node->child1()->op() == PhantomCreateRest || node->child1()->op() == PhantomNewArrayBuffer);
                    break;

                case EntrySwitch:
                    VALIDATE((node), node->entrySwitchData()->cases.size() == m_graph.m_numberOfEntrypoints);
                    break;

                case InitializeEntrypointArguments:
                    VALIDATE((node), node->entrypointIndex() < m_graph.m_numberOfEntrypoints);
                    break;

                default:
                    m_graph.doToChildren(
                        node,
                        [&] (const Edge& edge) {
                            VALIDATE((node), !edge->isPhantomAllocation());
                        });
                    break;
                }

                isOSRExited |= node->isPseudoTerminal();

                m_graph.doToChildren(node, [&] (Edge child) {
                    VALIDATE((node), dominators.strictlyDominates(child->owner, block) || nodesInThisBlock.contains(child.node()));
                });
                nodesInThisBlock.add(node);
            }
        }
    }

    void validateEdgeWithDoubleResultIfNecessary(Node* node, Edge edge)
    {
        if (!edge->hasDoubleResult())
            return;

        if (m_graph.m_planStage < PlanStage::AfterFixup)
            return;
        
        VALIDATE((node, edge), edge.useKind() == DoubleRepUse || edge.useKind() == DoubleRepRealUse || edge.useKind() == DoubleRepAnyIntUse);
    }

    void validateEdgeWithInt52ResultIfNecessary(Node* node, Edge edge)
    {
        if (m_graph.m_planStage < PlanStage::AfterFixup)
            return;

        VALIDATE((node, edge), edge->hasInt52Result() == (edge.useKind() == Int52RepUse));
    }

    void checkOperand(
        BasicBlock* block, Operands<size_t>& getLocalPositions,
        Operands<size_t>& setLocalPositions, Operand operand)
    {
        if (getLocalPositions.operand(operand) == notSet)
            return;
        if (setLocalPositions.operand(operand) == notSet)
            return;
        
        VALIDATE(
            (block->at(getLocalPositions.operand(operand)),
             block->at(setLocalPositions.operand(operand)),
             block),
            getLocalPositions.operand(operand) < setLocalPositions.operand(operand));
    }
    
    void reportValidationContext() { }

    void reportValidationContext(Node* node)
    {
        dataLogF("@%u", node->index());
    }
    
    void reportValidationContext(BasicBlock* block)
    {
        dataLog("Block ", *block);
    }
    
    void reportValidationContext(Node* node, Edge edge)
    {
        dataLog(node, " -> ", edge);
    }
    
    void reportValidationContext(Operand operand, BasicBlock* block)
    {
        if (!block) {
            dataLog(operand, " in null Block ");
            return;
        }

        dataLog(operand, " in Block ", *block);
    }
    
    void reportValidationContext(
        Operand operand, BasicBlock* sourceBlock, BasicBlock* destinationBlock)
    {
        dataLog(operand, " in Block ", *sourceBlock, " -> ", *destinationBlock);
    }
    
    void reportValidationContext(
        Operand operand, BasicBlock* sourceBlock, Node* prevNode)
    {
        dataLog(prevNode, " for ", operand, " in Block ", *sourceBlock);
    }
    
    void reportValidationContext(Node* node, BasicBlock* block)
    {
        dataLog(node, " in Block ", *block);
    }
    
    void reportValidationContext(Node* node, Node* node2, BasicBlock* block)
    {
        dataLog(node, " and ", node2, " in Block ", *block);
    }
    
    void reportValidationContext(
        Node* node, BasicBlock* block, Node* expectedNode, Edge incomingEdge)
    {
        dataLog(node, " in Block ", *block, ", searching for ", expectedNode, " from ", incomingEdge);
    }
    
    void dumpGraphIfAppropriate()
    {
        if (m_graphDumpMode == DontDumpGraph)
            return;
        dataLog("\n");
        if (!m_graphDumpBeforePhase.isNull()) {
            dataLog("Before phase:\n");
            dataLog(m_graphDumpBeforePhase);
        }
        dataLog("At time of failure:\n");
        m_graph.dump();
    }
};

} // End anonymous namespace.

void validate(Graph& graph, GraphDumpMode graphDumpMode, CString graphDumpBeforePhase)
{
    Validate validationObject(graph, graphDumpMode, graphDumpBeforePhase);
    validationObject.validate();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

