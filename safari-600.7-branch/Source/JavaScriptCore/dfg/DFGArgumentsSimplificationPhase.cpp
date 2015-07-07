/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "DFGArgumentsSimplificationPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlock.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGValidate.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCInlines.h"
#include <wtf/HashSet.h>
#include <wtf/HashMap.h>

namespace JSC { namespace DFG {

namespace {

struct ArgumentsAliasingData {
    InlineCallFrame* callContext;
    bool callContextSet;
    bool multipleCallContexts;
    
    bool assignedFromArguments;
    bool assignedFromManyThings;
    
    bool escapes;
    
    ArgumentsAliasingData()
        : callContext(0)
        , callContextSet(false)
        , multipleCallContexts(false)
        , assignedFromArguments(false)
        , assignedFromManyThings(false)
        , escapes(false)
    {
    }
    
    void mergeCallContext(InlineCallFrame* newCallContext)
    {
        if (multipleCallContexts)
            return;
        
        if (!callContextSet) {
            callContext = newCallContext;
            callContextSet = true;
            return;
        }
        
        if (callContext == newCallContext)
            return;
        
        multipleCallContexts = true;
    }
    
    bool callContextIsValid()
    {
        return callContextSet && !multipleCallContexts;
    }
    
    void mergeArgumentsAssignment()
    {
        assignedFromArguments = true;
    }
    
    void mergeNonArgumentsAssignment()
    {
        assignedFromManyThings = true;
    }
    
    bool argumentsAssignmentIsValid()
    {
        return assignedFromArguments && !assignedFromManyThings;
    }
    
    bool isValid()
    {
        return callContextIsValid() && argumentsAssignmentIsValid() && !escapes;
    }
};

} // end anonymous namespace

class ArgumentsSimplificationPhase : public Phase {
public:
    ArgumentsSimplificationPhase(Graph& graph)
        : Phase(graph, "arguments simplification")
    {
    }
    
    bool run()
    {
        if (!m_graph.m_hasArguments)
            return false;
        
        bool changed = false;
        
        // Record which arguments are known to escape no matter what.
        for (InlineCallFrameSet::iterator iter = m_graph.m_plan.inlineCallFrames->begin(); !!iter; ++iter)
            pruneObviousArgumentCreations(*iter);
        pruneObviousArgumentCreations(0); // the machine call frame.
        
        // Create data for variable access datas that we will want to analyze.
        for (unsigned i = m_graph.m_variableAccessData.size(); i--;) {
            VariableAccessData* variableAccessData = &m_graph.m_variableAccessData[i];
            if (!variableAccessData->isRoot())
                continue;
            if (variableAccessData->isCaptured())
                continue;
            m_argumentsAliasing.add(variableAccessData, ArgumentsAliasingData());
        }
        
        // Figure out which variables are live, using a conservative approximation of
        // liveness.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                Node* node = block->at(indexInBlock);
                switch (node->op()) {
                case GetLocal:
                case Flush:
                case PhantomLocal:
                    m_isLive.add(node->variableAccessData());
                    break;
                default:
                    break;
                }
            }
        }
        
        // Figure out which variables alias the arguments and nothing else, and are
        // used only for GetByVal and GetArrayLength accesses. At the same time,
        // identify uses of CreateArguments that are not consistent with the arguments
        // being aliased only to variables that satisfy these constraints.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                Node* node = block->at(indexInBlock);
                switch (node->op()) {
                case CreateArguments: {
                    // Ignore this op. If we see a lone CreateArguments then we want to
                    // completely ignore it because:
                    // 1) The default would be to see that the child is a GetLocal on the
                    //    arguments register and conclude that we have an arguments escape.
                    // 2) The fact that a CreateArguments exists does not mean that it
                    //    will continue to exist after we're done with this phase. As far
                    //    as this phase is concerned, a CreateArguments only "exists" if it
                    //    is used in a manner that necessitates its existance.
                    break;
                }
                    
                case TearOffArguments: {
                    // Ignore arguments tear off, because it's only relevant if we actually
                    // need to create the arguments.
                    break;
                }
                    
                case SetLocal: {
                    Node* source = node->child1().node();
                    VariableAccessData* variableAccessData = node->variableAccessData();
                    VirtualRegister argumentsRegister =
                        m_graph.uncheckedArgumentsRegisterFor(node->origin.semantic);
                    if (source->op() != CreateArguments && source->op() != PhantomArguments) {
                        // Make sure that the source of the SetLocal knows that if it's
                        // a variable that we think is aliased to the arguments, then it
                        // may escape at this point. In future, we could track transitive
                        // aliasing. But not yet.
                        observeBadArgumentsUse(source);
                        
                        // If this is an assignment to the arguments register, then
                        // pretend as if the arguments were created. We don't want to
                        // optimize code that explicitly assigns to the arguments,
                        // because that seems too ugly.
                        
                        // But, before getting rid of CreateArguments, we will have
                        // an assignment to the arguments registers with JSValue().
                        // That's because CSE will refuse to get rid of the
                        // init_lazy_reg since it treats CreateArguments as reading
                        // local variables. That could be fixed, but it's easier to
                        // work around this here.
                        if (source->op() == JSConstant
                            && !source->valueOfJSConstant(codeBlock()))
                            break;
                        
                        // If the variable is totally dead, then ignore it.
                        if (!m_isLive.contains(variableAccessData))
                            break;
                        
                        if (argumentsRegister.isValid()
                            && (variableAccessData->local() == argumentsRegister
                                || variableAccessData->local() == unmodifiedArgumentsRegister(argumentsRegister))) {
                            m_createsArguments.add(node->origin.semantic.inlineCallFrame);
                            break;
                        }

                        if (variableAccessData->isCaptured())
                            break;
                        
                        // Make sure that if it's a variable that we think is aliased to
                        // the arguments, that we know that it might actually not be.
                        ArgumentsAliasingData& data =
                            m_argumentsAliasing.find(variableAccessData)->value;
                        data.mergeNonArgumentsAssignment();
                        data.mergeCallContext(node->origin.semantic.inlineCallFrame);
                        break;
                    }
                    if (argumentsRegister.isValid()
                        && (variableAccessData->local() == argumentsRegister
                            || variableAccessData->local() == unmodifiedArgumentsRegister(argumentsRegister))) {
                        if (node->origin.semantic.inlineCallFrame == source->origin.semantic.inlineCallFrame)
                            break;
                        m_createsArguments.add(source->origin.semantic.inlineCallFrame);
                        break;
                    }
                    if (variableAccessData->isCaptured()) {
                        m_createsArguments.add(source->origin.semantic.inlineCallFrame);
                        break;
                    }
                    ArgumentsAliasingData& data =
                        m_argumentsAliasing.find(variableAccessData)->value;
                    data.mergeArgumentsAssignment();
                    // This ensures that the variable's uses are in the same context as
                    // the arguments it is aliasing.
                    data.mergeCallContext(node->origin.semantic.inlineCallFrame);
                    data.mergeCallContext(source->origin.semantic.inlineCallFrame);
                    break;
                }
                    
                case GetLocal:
                case Phi: /* FIXME: https://bugs.webkit.org/show_bug.cgi?id=108555 */ {
                    VariableAccessData* variableAccessData = node->variableAccessData();
                    if (variableAccessData->isCaptured())
                        break;
                    ArgumentsAliasingData& data =
                        m_argumentsAliasing.find(variableAccessData)->value;
                    data.mergeCallContext(node->origin.semantic.inlineCallFrame);
                    break;
                }
                    
                case Flush: {
                    VariableAccessData* variableAccessData = node->variableAccessData();
                    if (variableAccessData->isCaptured())
                        break;
                    ArgumentsAliasingData& data =
                        m_argumentsAliasing.find(variableAccessData)->value;
                    data.mergeCallContext(node->origin.semantic.inlineCallFrame);
                    
                    // If a variable is used in a flush then by definition it escapes.
                    data.escapes = true;
                    break;
                }
                    
                case SetArgument: {
                    VariableAccessData* variableAccessData = node->variableAccessData();
                    if (variableAccessData->isCaptured())
                        break;
                    ArgumentsAliasingData& data =
                        m_argumentsAliasing.find(variableAccessData)->value;
                    data.mergeNonArgumentsAssignment();
                    data.mergeCallContext(node->origin.semantic.inlineCallFrame);
                    break;
                }
                    
                case GetByVal: {
                    if (node->arrayMode().type() != Array::Arguments) {
                        observeBadArgumentsUses(node);
                        break;
                    }

                    // That's so awful and pretty much impossible since it would
                    // imply that the arguments were predicted integer, but it's
                    // good to be defensive and thorough.
                    observeBadArgumentsUse(node->child2().node());
                    observeProperArgumentsUse(node, node->child1());
                    break;
                }
                    
                case GetArrayLength: {
                    if (node->arrayMode().type() != Array::Arguments) {
                        observeBadArgumentsUses(node);
                        break;
                    }
                        
                    observeProperArgumentsUse(node, node->child1());
                    break;
                }
                    
                case Phantom:
                case HardPhantom:
                    // We don't care about phantom uses, since phantom uses are all about
                    // just keeping things alive for OSR exit. If something - like the
                    // CreateArguments - is just being kept alive, then this transformation
                    // will not break this, since the Phantom will now just keep alive a
                    // PhantomArguments and OSR exit will still do the right things.
                    break;
                    
                case CheckStructure:
                case StructureTransitionWatchpoint:
                case CheckArray:
                    // We don't care about these because if we get uses of the relevant
                    // variable then we can safely get rid of these, too. This of course
                    // relies on there not being any information transferred by the CFA
                    // from a CheckStructure on one variable to the information about the
                    // structures of another variable.
                    break;
                    
                case MovHint:
                    // We don't care about MovHints at all, since they represent what happens
                    // in bytecode. We rematerialize arguments objects on OSR exit anyway.
                    break;
                    
                default:
                    observeBadArgumentsUses(node);
                    break;
                }
            }
        }

        // Now we know which variables are aliased to arguments. But if any of them are
        // found to have escaped, or were otherwise invalidated, then we need to mark
        // the arguments as requiring creation. This is a property of SetLocals to
        // variables that are neither the correct arguments register nor are marked as
        // being arguments-aliased.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                Node* node = block->at(indexInBlock);
                if (node->op() != SetLocal)
                    continue;
                Node* source = node->child1().node();
                if (source->op() != CreateArguments)
                    continue;
                VariableAccessData* variableAccessData = node->variableAccessData();
                if (variableAccessData->isCaptured()) {
                    // The captured case would have already been taken care of in the
                    // previous pass.
                    continue;
                }
                
                ArgumentsAliasingData& data =
                    m_argumentsAliasing.find(variableAccessData)->value;
                if (data.isValid())
                    continue;
                
                m_createsArguments.add(source->origin.semantic.inlineCallFrame);
            }
        }
        
        InsertionSet insertionSet(m_graph);
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); indexInBlock++) {
                Node* node = block->at(indexInBlock);
                switch (node->op()) {
                case SetLocal: {
                    Node* source = node->child1().node();
                    if (source->op() != CreateArguments)
                        break;
                    
                    if (m_createsArguments.contains(source->origin.semantic.inlineCallFrame))
                        break;
                    
                    VariableAccessData* variableAccessData = node->variableAccessData();
                    
                    if (variableAccessData->mergeIsArgumentsAlias(true)) {
                        changed = true;
                        
                        // Make sure that the variable knows, that it may now hold non-cell values.
                        variableAccessData->predict(SpecEmpty);
                    }
                    
                    // Make sure that the SetLocal doesn't check that the input is a Cell.
                    if (node->child1().useKind() != UntypedUse) {
                        node->child1().setUseKind(UntypedUse);
                        changed = true;
                    }
                    break;
                }
                    
                case Flush: {
                    VariableAccessData* variableAccessData = node->variableAccessData();
                    
                    if (variableAccessData->isCaptured()
                        || !m_argumentsAliasing.find(variableAccessData)->value.isValid()
                        || m_createsArguments.contains(node->origin.semantic.inlineCallFrame))
                        break;
                    
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                    
                case Phantom:
                case HardPhantom: {
                    // It's highly likely that we will have a Phantom referencing either
                    // CreateArguments, or a local op for the arguments register, or a
                    // local op for an arguments-aliased variable. In any of those cases,
                    // we should remove the phantom reference, since:
                    // 1) Phantoms only exist to aid OSR exit. But arguments simplification
                    //    has its own OSR exit story, which is to inform OSR exit to reify
                    //    the arguments as necessary.
                    // 2) The Phantom may keep the CreateArguments node alive, which is
                    //    precisely what we don't want.
                    for (unsigned i = 0; i < AdjacencyList::Size; ++i)
                        detypeArgumentsReferencingPhantomChild(node, i);
                    break;
                }
                    
                case CheckStructure:
                case StructureTransitionWatchpoint:
                case CheckArray: {
                    // We can just get rid of this node, if it references a phantom argument.
                    if (!isOKToOptimize(node->child1().node()))
                        break;
                    node->convertToPhantom();
                    break;
                }
                    
                case GetByVal: {
                    if (node->arrayMode().type() != Array::Arguments)
                        break;

                    // This can be simplified to GetMyArgumentByVal if we know that
                    // it satisfies either condition (1) or (2):
                    // 1) Its first child is a valid ArgumentsAliasingData and the
                    //    InlineCallFrame* is not marked as creating arguments.
                    // 2) Its first child is CreateArguments and its InlineCallFrame*
                    //    is not marked as creating arguments.
                    
                    if (!isOKToOptimize(node->child1().node()))
                        break;
                    
                    insertionSet.insertNode(
                        indexInBlock, SpecNone, Phantom, node->origin, node->child1());
                    
                    node->child1() = node->child2();
                    node->child2() = Edge();
                    node->setOpAndDefaultFlags(GetMyArgumentByVal);
                    changed = true;
                    --indexInBlock; // Force reconsideration of this op now that it's a GetMyArgumentByVal.
                    break;
                }
                    
                case GetArrayLength: {
                    if (node->arrayMode().type() != Array::Arguments)
                        break;
                    
                    if (!isOKToOptimize(node->child1().node()))
                        break;
                    
                    insertionSet.insertNode(
                        indexInBlock, SpecNone, Phantom, node->origin, node->child1());
                    
                    node->child1() = Edge();
                    node->setOpAndDefaultFlags(GetMyArgumentsLength);
                    changed = true;
                    --indexInBlock; // Force reconsideration of this op noew that it's a GetMyArgumentsLength.
                    break;
                }
                    
                case GetMyArgumentsLength:
                case GetMyArgumentsLengthSafe: {
                    if (m_createsArguments.contains(node->origin.semantic.inlineCallFrame)) {
                        ASSERT(node->op() == GetMyArgumentsLengthSafe);
                        break;
                    }
                    if (node->op() == GetMyArgumentsLengthSafe) {
                        node->setOp(GetMyArgumentsLength);
                        changed = true;
                    }
                    
                    NodeOrigin origin = node->origin;
                    if (!origin.semantic.inlineCallFrame)
                        break;
                    
                    // We know exactly what this will return. But only after we have checked
                    // that nobody has escaped our arguments.
                    insertionSet.insertNode(
                        indexInBlock, SpecNone, CheckArgumentsNotCreated, origin);
                    
                    m_graph.convertToConstant(
                        node, jsNumber(origin.semantic.inlineCallFrame->arguments.size() - 1));
                    changed = true;
                    break;
                }
                    
                case GetMyArgumentByVal:
                case GetMyArgumentByValSafe: {
                    if (m_createsArguments.contains(node->origin.semantic.inlineCallFrame)) {
                        ASSERT(node->op() == GetMyArgumentByValSafe);
                        break;
                    }
                    if (node->op() == GetMyArgumentByValSafe) {
                        node->setOp(GetMyArgumentByVal);
                        changed = true;
                    }
                    if (!node->origin.semantic.inlineCallFrame)
                        break;
                    if (!node->child1()->hasConstant())
                        break;
                    JSValue value = node->child1()->valueOfJSConstant(codeBlock());
                    if (!value.isInt32())
                        break;
                    int32_t index = value.asInt32();
                    if (index < 0
                        || static_cast<size_t>(index + 1) >=
                            node->origin.semantic.inlineCallFrame->arguments.size())
                        break;
                    
                    // We know which argument this is accessing. But only after we have checked
                    // that nobody has escaped our arguments. We also need to ensure that the
                    // index is kept alive. That's somewhat pointless since it's a constant, but
                    // it's important because this is one of those invariants that we like to
                    // have in the DFG. Note finally that we use the GetLocalUnlinked opcode
                    // here, since this is being done _after_ the prediction propagation phase
                    // has run - therefore it makes little sense to link the GetLocal operation
                    // into the VariableAccessData and Phi graphs.

                    NodeOrigin origin = node->origin;
                    AdjacencyList children = node->children;
                    
                    node->convertToGetLocalUnlinked(
                        VirtualRegister(
                            origin.semantic.inlineCallFrame->stackOffset +
                            m_graph.baselineCodeBlockFor(origin.semantic)->argumentIndexAfterCapture(index)));

                    insertionSet.insertNode(
                        indexInBlock, SpecNone, CheckArgumentsNotCreated, origin);
                    insertionSet.insertNode(
                        indexInBlock, SpecNone, Phantom, origin, children);
                    
                    changed = true;
                    break;
                }
                    
                case TearOffArguments: {
                    if (m_createsArguments.contains(node->origin.semantic.inlineCallFrame))
                        continue;
                    
                    node->convertToPhantom();
                    break;
                }
                    
                default:
                    break;
                }
            }
            insertionSet.execute(block);
        }
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                Node* node = block->at(indexInBlock);
                if (node->op() != CreateArguments)
                    continue;
                // If this is a CreateArguments for an InlineCallFrame* that does
                // not create arguments, then replace it with a PhantomArguments.
                // PhantomArguments is a non-executing node that just indicates
                // that the node should be reified as an arguments object on OSR
                // exit.
                if (m_createsArguments.contains(node->origin.semantic.inlineCallFrame))
                    continue;
                insertionSet.insertNode(
                    indexInBlock, SpecNone, Phantom, node->origin, node->children);
                node->setOpAndDefaultFlags(PhantomArguments);
                node->children.reset();
                changed = true;
            }
            insertionSet.execute(block);
        }
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = 0; indexInBlock < block->size(); ++indexInBlock) {
                Node* node = block->at(indexInBlock);
                if (node->op() != Phantom)
                    continue;
                for (unsigned i = 0; i < AdjacencyList::Size; ++i)
                    detypeArgumentsReferencingPhantomChild(node, i);
            }
        }
        
        if (changed) {
            m_graph.dethread();
            m_graph.m_form = LoadStore;
        }
        
        return changed;
    }
    
private:
    HashSet<InlineCallFrame*,
            DefaultHash<InlineCallFrame*>::Hash,
            NullableHashTraits<InlineCallFrame*>> m_createsArguments;
    HashMap<VariableAccessData*, ArgumentsAliasingData,
            DefaultHash<VariableAccessData*>::Hash,
            NullableHashTraits<VariableAccessData*>> m_argumentsAliasing;
    HashSet<VariableAccessData*> m_isLive;

    void pruneObviousArgumentCreations(InlineCallFrame* inlineCallFrame)
    {
        ScriptExecutable* executable = m_graph.executableFor(inlineCallFrame);
        if (m_graph.m_executablesWhoseArgumentsEscaped.contains(executable)
            || executable->isStrictMode())
            m_createsArguments.add(inlineCallFrame);
    }
    
    void observeBadArgumentsUse(Node* node)
    {
        if (!node)
            return;
        
        switch (node->op()) {
        case CreateArguments: {
            m_createsArguments.add(node->origin.semantic.inlineCallFrame);
            break;
        }
            
        case GetLocal: {
            VirtualRegister argumentsRegister =
                m_graph.uncheckedArgumentsRegisterFor(node->origin.semantic);
            if (argumentsRegister.isValid()
                && (node->local() == argumentsRegister
                    || node->local() == unmodifiedArgumentsRegister(argumentsRegister))) {
                m_createsArguments.add(node->origin.semantic.inlineCallFrame);
                break;
            }
            
            VariableAccessData* variableAccessData = node->variableAccessData();
            if (variableAccessData->isCaptured())
                break;
            
            ArgumentsAliasingData& data = m_argumentsAliasing.find(variableAccessData)->value;
            data.escapes = true;
            break;
        }
            
        default:
            break;
        }
    }
    
    void observeBadArgumentsUses(Node* node)
    {
        for (unsigned i = m_graph.numChildren(node); i--;)
            observeBadArgumentsUse(m_graph.child(node, i).node());
    }
    
    void observeProperArgumentsUse(Node* node, Edge edge)
    {
        if (edge->op() != GetLocal) {
            // When can this happen? At least two cases that I can think
            // of:
            //
            // 1) Aliased use of arguments in the same basic block,
            //    like:
            //
            //    var a = arguments;
            //    var x = arguments[i];
            //
            // 2) If we're accessing arguments we got from the heap!
                            
            if (edge->op() == CreateArguments
                && node->origin.semantic.inlineCallFrame
                    != edge->origin.semantic.inlineCallFrame)
                m_createsArguments.add(edge->origin.semantic.inlineCallFrame);
            
            return;
        }
                        
        VariableAccessData* variableAccessData = edge->variableAccessData();
        if (edge->local() == m_graph.uncheckedArgumentsRegisterFor(edge->origin.semantic)
            && node->origin.semantic.inlineCallFrame != edge->origin.semantic.inlineCallFrame) {
            m_createsArguments.add(edge->origin.semantic.inlineCallFrame);
            return;
        }

        if (variableAccessData->isCaptured())
            return;
        
        ArgumentsAliasingData& data = m_argumentsAliasing.find(variableAccessData)->value;
        data.mergeCallContext(node->origin.semantic.inlineCallFrame);
    }
    
    bool isOKToOptimize(Node* source)
    {
        if (m_createsArguments.contains(source->origin.semantic.inlineCallFrame))
            return false;
        
        switch (source->op()) {
        case GetLocal: {
            VariableAccessData* variableAccessData = source->variableAccessData();
            VirtualRegister argumentsRegister =
                m_graph.uncheckedArgumentsRegisterFor(source->origin.semantic);
            if (!argumentsRegister.isValid())
                break;
            if (argumentsRegister == variableAccessData->local())
                return true;
            if (unmodifiedArgumentsRegister(argumentsRegister) == variableAccessData->local())
                return true;
            if (variableAccessData->isCaptured())
                break;
            ArgumentsAliasingData& data =
                m_argumentsAliasing.find(variableAccessData)->value;
            if (!data.isValid())
                break;
                            
            return true;
        }
                            
        case CreateArguments: {
            return true;
        }
                            
        default:
            break;
        }
        
        return false;
    }
    
    void detypeArgumentsReferencingPhantomChild(Node* node, unsigned edgeIndex)
    {
        Edge edge = node->children.child(edgeIndex);
        if (!edge)
            return;
        
        switch (edge->op()) {
        case GetLocal: {
            VariableAccessData* variableAccessData = edge->variableAccessData();
            if (!variableAccessData->isArgumentsAlias())
                break;
            node->children.child(edgeIndex).setUseKind(UntypedUse);
            break;
        }
            
        case PhantomArguments: {
            node->children.child(edgeIndex).setUseKind(UntypedUse);
            break;
        }
            
        default:
            break;
        }
    }
};

bool performArgumentsSimplification(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Arguments Simplification Phase");
    return runPhase<ArgumentsSimplificationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


