 /*
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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
#include "DFGByteCodeParser.h"

#if ENABLE(DFG_JIT)

#include "ArrayConstructor.h"
#include "CallLinkStatus.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGArrayMode.h"
#include "DFGCapabilities.h"
#include "DFGJITCode.h"
#include "GetByIdStatus.h"
#include "Heap.h"
#include "JSLexicalEnvironment.h"
#include "JSCInlines.h"
#include "PreciseJumpTargets.h"
#include "PutByIdStatus.h"
#include "StackAlignment.h"
#include "StringConstructor.h"
#include <wtf/CommaPrinter.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace DFG {

static const bool verbose = false;

class ConstantBufferKey {
public:
    ConstantBufferKey()
        : m_codeBlock(0)
        , m_index(0)
    {
    }
    
    ConstantBufferKey(WTF::HashTableDeletedValueType)
        : m_codeBlock(0)
        , m_index(1)
    {
    }
    
    ConstantBufferKey(CodeBlock* codeBlock, unsigned index)
        : m_codeBlock(codeBlock)
        , m_index(index)
    {
    }
    
    bool operator==(const ConstantBufferKey& other) const
    {
        return m_codeBlock == other.m_codeBlock
            && m_index == other.m_index;
    }
    
    unsigned hash() const
    {
        return WTF::PtrHash<CodeBlock*>::hash(m_codeBlock) ^ m_index;
    }
    
    bool isHashTableDeletedValue() const
    {
        return !m_codeBlock && m_index;
    }
    
    CodeBlock* codeBlock() const { return m_codeBlock; }
    unsigned index() const { return m_index; }
    
private:
    CodeBlock* m_codeBlock;
    unsigned m_index;
};

struct ConstantBufferKeyHash {
    static unsigned hash(const ConstantBufferKey& key) { return key.hash(); }
    static bool equal(const ConstantBufferKey& a, const ConstantBufferKey& b)
    {
        return a == b;
    }
    
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} } // namespace JSC::DFG

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::DFG::ConstantBufferKey> {
    typedef JSC::DFG::ConstantBufferKeyHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::DFG::ConstantBufferKey> : SimpleClassHashTraits<JSC::DFG::ConstantBufferKey> { };

} // namespace WTF

namespace JSC { namespace DFG {

// === ByteCodeParser ===
//
// This class is used to compile the dataflow graph from a CodeBlock.
class ByteCodeParser {
public:
    ByteCodeParser(Graph& graph)
        : m_vm(&graph.m_vm)
        , m_codeBlock(graph.m_codeBlock)
        , m_profiledBlock(graph.m_profiledBlock)
        , m_graph(graph)
        , m_currentBlock(0)
        , m_currentIndex(0)
        , m_constantUndefined(graph.freeze(jsUndefined()))
        , m_constantNull(graph.freeze(jsNull()))
        , m_constantNaN(graph.freeze(jsNumber(PNaN)))
        , m_constantOne(graph.freeze(jsNumber(1)))
        , m_numArguments(m_codeBlock->numParameters())
        , m_numLocals(m_codeBlock->m_numCalleeRegisters)
        , m_parameterSlots(0)
        , m_numPassedVarArgs(0)
        , m_inlineStackTop(0)
        , m_haveBuiltOperandMaps(false)
        , m_currentInstruction(0)
    {
        ASSERT(m_profiledBlock);
    }
    
    // Parse a full CodeBlock of bytecode.
    bool parse();
    
private:
    struct InlineStackEntry;

    // Just parse from m_currentIndex to the end of the current CodeBlock.
    void parseCodeBlock();
    
    void ensureLocals(unsigned newNumLocals)
    {
        if (newNumLocals <= m_numLocals)
            return;
        m_numLocals = newNumLocals;
        for (size_t i = 0; i < m_graph.numBlocks(); ++i)
            m_graph.block(i)->ensureLocals(newNumLocals);
    }

    // Helper for min and max.
    bool handleMinMax(int resultOperand, NodeType op, int registerOffset, int argumentCountIncludingThis);
    
    // Handle calls. This resolves issues surrounding inlining and intrinsics.
    void handleCall(
        int result, NodeType op, InlineCallFrame::Kind, unsigned instructionSize,
        Node* callTarget, int argCount, int registerOffset, CallLinkStatus,
        SpeculatedType prediction);
    void handleCall(
        int result, NodeType op, InlineCallFrame::Kind, unsigned instructionSize,
        Node* callTarget, int argCount, int registerOffset, CallLinkStatus);
    void handleCall(int result, NodeType op, CodeSpecializationKind, unsigned instructionSize, int callee, int argCount, int registerOffset);
    void handleCall(Instruction* pc, NodeType op, CodeSpecializationKind);
    void emitFunctionChecks(CallVariant, Node* callTarget, int registerOffset, CodeSpecializationKind);
    void undoFunctionChecks(CallVariant);
    void emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind);
    unsigned inliningCost(CallVariant, int argumentCountIncludingThis, CodeSpecializationKind); // Return UINT_MAX if it's not an inlining candidate. By convention, intrinsics have a cost of 1.
    // Handle inlining. Return true if it succeeded, false if we need to plant a call.
    bool handleInlining(Node* callTargetNode, int resultOperand, const CallLinkStatus&, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, NodeType callOp, InlineCallFrame::Kind, SpeculatedType prediction);
    enum CallerLinkability { CallerDoesNormalLinking, CallerLinksManually };
    bool attemptToInlineCall(Node* callTargetNode, int resultOperand, CallVariant, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind, CallerLinkability, SpeculatedType prediction, unsigned& inliningBalance);
    void inlineCall(Node* callTargetNode, int resultOperand, CallVariant, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind, CallerLinkability);
    void cancelLinkingForBlock(InlineStackEntry*, BasicBlock*); // Only works when the given block is the last one to have been added for that inline stack entry.
    // Handle intrinsic functions. Return true if it succeeded, false if we need to plant a call.
    bool handleIntrinsic(int resultOperand, Intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction);
    bool handleTypedArrayConstructor(int resultOperand, InternalFunction*, int registerOffset, int argumentCountIncludingThis, TypedArrayType);
    bool handleConstantInternalFunction(int resultOperand, InternalFunction*, int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind);
    Node* handlePutByOffset(Node* base, unsigned identifier, PropertyOffset, Node* value);
    Node* handleGetByOffset(SpeculatedType, Node* base, const StructureSet&, unsigned identifierNumber, PropertyOffset, NodeType op = GetByOffset);
    void handleGetById(
        int destinationOperand, SpeculatedType, Node* base, unsigned identifierNumber,
        const GetByIdStatus&);
    void emitPutById(
        Node* base, unsigned identifierNumber, Node* value,  const PutByIdStatus&, bool isDirect);
    void handlePutById(
        Node* base, unsigned identifierNumber, Node* value, const PutByIdStatus&,
        bool isDirect);
    void emitChecks(const ConstantStructureCheckVector&);

    Node* getScope(unsigned skipCount);
    
    void prepareToParseBlock();
    void clearCaches();

    // Parse a single basic block of bytecode instructions.
    bool parseBlock(unsigned limit);
    // Link block successors.
    void linkBlock(BasicBlock*, Vector<BasicBlock*>& possibleTargets);
    void linkBlocks(Vector<UnlinkedBlock>& unlinkedBlocks, Vector<BasicBlock*>& possibleTargets);
    
    VariableAccessData* newVariableAccessData(VirtualRegister operand, bool isCaptured)
    {
        ASSERT(!operand.isConstant());
        
        m_graph.m_variableAccessData.append(VariableAccessData(operand, isCaptured));
        return &m_graph.m_variableAccessData.last();
    }
    
    // Get/Set the operands/result of a bytecode instruction.
    Node* getDirect(VirtualRegister operand)
    {
        ASSERT(!operand.isConstant());

        // Is this an argument?
        if (operand.isArgument())
            return getArgument(operand);

        // Must be a local.
        return getLocal(operand);
    }

    Node* get(VirtualRegister operand)
    {
        if (operand.isConstant()) {
            unsigned constantIndex = operand.toConstantIndex();
            unsigned oldSize = m_constants.size();
            if (constantIndex >= oldSize || !m_constants[constantIndex]) {
                JSValue value = m_inlineStackTop->m_codeBlock->getConstant(operand.offset());
                if (constantIndex >= oldSize) {
                    m_constants.grow(constantIndex + 1);
                    for (unsigned i = oldSize; i < m_constants.size(); ++i)
                        m_constants[i] = nullptr;
                }
                m_constants[constantIndex] =
                    addToGraph(JSConstant, OpInfo(m_graph.freezeStrong(value)));
            }
            ASSERT(m_constants[constantIndex]);
            return m_constants[constantIndex];
        }
        
        if (inlineCallFrame()) {
            if (!inlineCallFrame()->isClosureCall) {
                JSFunction* callee = inlineCallFrame()->calleeConstant();
                if (operand.offset() == JSStack::Callee)
                    return weakJSConstant(callee);
                if (operand.offset() == JSStack::ScopeChain)
                    return weakJSConstant(callee->scope());
            }
        } else if (operand.offset() == JSStack::Callee)
            return addToGraph(GetCallee);
        else if (operand.offset() == JSStack::ScopeChain)
            return addToGraph(GetMyScope);
        
        return getDirect(m_inlineStackTop->remapOperand(operand));
    }
    
    enum SetMode {
        // A normal set which follows a two-phase commit that spans code origins. During
        // the current code origin it issues a MovHint, and at the start of the next
        // code origin there will be a SetLocal. If the local needs flushing, the second
        // SetLocal will be preceded with a Flush.
        NormalSet,
        
        // A set where the SetLocal happens immediately and there is still a Flush. This
        // is relevant when assigning to a local in tricky situations for the delayed
        // SetLocal logic but where we know that we have not performed any side effects
        // within this code origin. This is a safe replacement for NormalSet anytime we
        // know that we have not yet performed side effects in this code origin.
        ImmediateSetWithFlush,
        
        // A set where the SetLocal happens immediately and we do not Flush it even if
        // this is a local that is marked as needing it. This is relevant when
        // initializing locals at the top of a function.
        ImmediateNakedSet
    };
    Node* setDirect(VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        addToGraph(MovHint, OpInfo(operand.offset()), value);
        
        DelayedSetLocal delayed = DelayedSetLocal(operand, value);
        
        if (setMode == NormalSet) {
            m_setLocalQueue.append(delayed);
            return 0;
        }
        
        return delayed.execute(this, setMode);
    }
    
    void processSetLocalQueue()
    {
        for (unsigned i = 0; i < m_setLocalQueue.size(); ++i)
            m_setLocalQueue[i].execute(this);
        m_setLocalQueue.resize(0);
    }

    Node* set(VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        return setDirect(m_inlineStackTop->remapOperand(operand), value, setMode);
    }
    
    Node* injectLazyOperandSpeculation(Node* node)
    {
        ASSERT(node->op() == GetLocal);
        ASSERT(node->origin.semantic.bytecodeIndex == m_currentIndex);
        ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        LazyOperandValueProfileKey key(m_currentIndex, node->local());
        SpeculatedType prediction = m_inlineStackTop->m_lazyOperands.prediction(locker, key);
        node->variableAccessData()->predict(prediction);
        return node;
    }

    // Used in implementing get/set, above, where the operand is a local variable.
    Node* getLocal(VirtualRegister operand)
    {
        unsigned local = operand.toLocal();

        if (local < m_localWatchpoints.size()) {
            if (VariableWatchpointSet* set = m_localWatchpoints[local]) {
                if (JSValue value = set->inferredValue()) {
                    addToGraph(FunctionReentryWatchpoint, OpInfo(m_codeBlock->symbolTable()));
                    addToGraph(VariableWatchpoint, OpInfo(set));
                    return weakJSConstant(value);
                }
            }
        }

        Node* node = m_currentBlock->variablesAtTail.local(local);
        bool isCaptured = m_codeBlock->isCaptured(operand, inlineCallFrame());
        
        // This has two goals: 1) link together variable access datas, and 2)
        // try to avoid creating redundant GetLocals. (1) is required for
        // correctness - no other phase will ensure that block-local variable
        // access data unification is done correctly. (2) is purely opportunistic
        // and is meant as an compile-time optimization only.
        
        VariableAccessData* variable;
        
        if (node) {
            variable = node->variableAccessData();
            variable->mergeIsCaptured(isCaptured);
            
            if (!isCaptured) {
                switch (node->op()) {
                case GetLocal:
                    return node;
                case SetLocal:
                    return node->child1().node();
                default:
                    break;
                }
            }
        } else
            variable = newVariableAccessData(operand, isCaptured);
        
        node = injectLazyOperandSpeculation(addToGraph(GetLocal, OpInfo(variable)));
        m_currentBlock->variablesAtTail.local(local) = node;
        return node;
    }

    Node* setLocal(VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        unsigned local = operand.toLocal();
        bool isCaptured = m_codeBlock->isCaptured(operand, inlineCallFrame());
        
        if (setMode != ImmediateNakedSet) {
            ArgumentPosition* argumentPosition = findArgumentPositionForLocal(operand);
            if (isCaptured || argumentPosition)
                flushDirect(operand, argumentPosition);
        }

        VariableAccessData* variableAccessData = newVariableAccessData(operand, isCaptured);
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType));
        Node* node = addToGraph(SetLocal, OpInfo(variableAccessData), value);
        m_currentBlock->variablesAtTail.local(local) = node;
        return node;
    }

    // Used in implementing get/set, above, where the operand is an argument.
    Node* getArgument(VirtualRegister operand)
    {
        unsigned argument = operand.toArgument();
        ASSERT(argument < m_numArguments);
        
        Node* node = m_currentBlock->variablesAtTail.argument(argument);
        bool isCaptured = m_codeBlock->isCaptured(operand);

        VariableAccessData* variable;
        
        if (node) {
            variable = node->variableAccessData();
            variable->mergeIsCaptured(isCaptured);
            
            switch (node->op()) {
            case GetLocal:
                return node;
            case SetLocal:
                return node->child1().node();
            default:
                break;
            }
        } else
            variable = newVariableAccessData(operand, isCaptured);
        
        node = injectLazyOperandSpeculation(addToGraph(GetLocal, OpInfo(variable)));
        m_currentBlock->variablesAtTail.argument(argument) = node;
        return node;
    }
    Node* setArgument(VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        unsigned argument = operand.toArgument();
        ASSERT(argument < m_numArguments);
        
        bool isCaptured = m_codeBlock->isCaptured(operand);

        VariableAccessData* variableAccessData = newVariableAccessData(operand, isCaptured);

        // Always flush arguments, except for 'this'. If 'this' is created by us,
        // then make sure that it's never unboxed.
        if (argument) {
            if (setMode != ImmediateNakedSet)
                flushDirect(operand);
        } else if (m_codeBlock->specializationKind() == CodeForConstruct)
            variableAccessData->mergeShouldNeverUnbox(true);
        
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType));
        Node* node = addToGraph(SetLocal, OpInfo(variableAccessData), value);
        m_currentBlock->variablesAtTail.argument(argument) = node;
        return node;
    }
    
    ArgumentPosition* findArgumentPositionForArgument(int argument)
    {
        InlineStackEntry* stack = m_inlineStackTop;
        while (stack->m_inlineCallFrame)
            stack = stack->m_caller;
        return stack->m_argumentPositions[argument];
    }
    
    ArgumentPosition* findArgumentPositionForLocal(VirtualRegister operand)
    {
        for (InlineStackEntry* stack = m_inlineStackTop; ; stack = stack->m_caller) {
            InlineCallFrame* inlineCallFrame = stack->m_inlineCallFrame;
            if (!inlineCallFrame)
                break;
            if (operand.offset() < static_cast<int>(inlineCallFrame->stackOffset + JSStack::CallFrameHeaderSize))
                continue;
            if (operand.offset() == inlineCallFrame->stackOffset + CallFrame::thisArgumentOffset())
                continue;
            if (operand.offset() >= static_cast<int>(inlineCallFrame->stackOffset + CallFrame::thisArgumentOffset() + inlineCallFrame->arguments.size()))
                continue;
            int argument = VirtualRegister(operand.offset() - inlineCallFrame->stackOffset).toArgument();
            return stack->m_argumentPositions[argument];
        }
        return 0;
    }
    
    ArgumentPosition* findArgumentPosition(VirtualRegister operand)
    {
        if (operand.isArgument())
            return findArgumentPositionForArgument(operand.toArgument());
        return findArgumentPositionForLocal(operand);
    }

    void flush(VirtualRegister operand)
    {
        flushDirect(m_inlineStackTop->remapOperand(operand));
    }
    
    void flushDirect(VirtualRegister operand)
    {
        flushDirect(operand, findArgumentPosition(operand));
    }
    
    void flushDirect(VirtualRegister operand, ArgumentPosition* argumentPosition)
    {
        bool isCaptured = m_codeBlock->isCaptured(operand, inlineCallFrame());
        
        ASSERT(!operand.isConstant());
        
        Node* node = m_currentBlock->variablesAtTail.operand(operand);
        
        VariableAccessData* variable;
        
        if (node) {
            variable = node->variableAccessData();
            variable->mergeIsCaptured(isCaptured);
        } else
            variable = newVariableAccessData(operand, isCaptured);
        
        node = addToGraph(Flush, OpInfo(variable));
        m_currentBlock->variablesAtTail.operand(operand) = node;
        if (argumentPosition)
            argumentPosition->addVariable(variable);
    }
    
    void flush(InlineStackEntry* inlineStackEntry)
    {
        int numArguments;
        if (InlineCallFrame* inlineCallFrame = inlineStackEntry->m_inlineCallFrame) {
            numArguments = inlineCallFrame->arguments.size();
            if (inlineCallFrame->isClosureCall) {
                flushDirect(inlineStackEntry->remapOperand(VirtualRegister(JSStack::Callee)));
                flushDirect(inlineStackEntry->remapOperand(VirtualRegister(JSStack::ScopeChain)));
            }
        } else
            numArguments = inlineStackEntry->m_codeBlock->numParameters();
        for (unsigned argument = numArguments; argument-- > 1;)
            flushDirect(inlineStackEntry->remapOperand(virtualRegisterForArgument(argument)));
        for (int local = 0; local < inlineStackEntry->m_codeBlock->m_numVars; ++local) {
            if (!inlineStackEntry->m_codeBlock->isCaptured(virtualRegisterForLocal(local)))
                continue;
            flushDirect(inlineStackEntry->remapOperand(virtualRegisterForLocal(local)));
        }
    }

    void flushForTerminal()
    {
        for (InlineStackEntry* inlineStackEntry = m_inlineStackTop; inlineStackEntry; inlineStackEntry = inlineStackEntry->m_caller)
            flush(inlineStackEntry);
    }

    void flushForReturn()
    {
        flush(m_inlineStackTop);
    }
    
    void flushIfTerminal(SwitchData& data)
    {
        if (data.fallThrough.bytecodeIndex() > m_currentIndex)
            return;
        
        for (unsigned i = data.cases.size(); i--;) {
            if (data.cases[i].target.bytecodeIndex() > m_currentIndex)
                return;
        }
        
        flushForTerminal();
    }

    // Assumes that the constant should be strongly marked.
    Node* jsConstant(JSValue constantValue)
    {
        return addToGraph(JSConstant, OpInfo(m_graph.freezeStrong(constantValue)));
    }

    Node* weakJSConstant(JSValue constantValue)
    {
        return addToGraph(JSConstant, OpInfo(m_graph.freeze(constantValue)));
    }

    // Helper functions to get/set the this value.
    Node* getThis()
    {
        return get(m_inlineStackTop->m_codeBlock->thisRegister());
    }

    void setThis(Node* value)
    {
        set(m_inlineStackTop->m_codeBlock->thisRegister(), value);
    }

    InlineCallFrame* inlineCallFrame()
    {
        return m_inlineStackTop->m_inlineCallFrame;
    }

    CodeOrigin currentCodeOrigin()
    {
        return CodeOrigin(m_currentIndex, inlineCallFrame());
    }
    
    BranchData* branchData(unsigned taken, unsigned notTaken)
    {
        // We assume that branches originating from bytecode always have a fall-through. We
        // use this assumption to avoid checking for the creation of terminal blocks.
        ASSERT((taken > m_currentIndex) || (notTaken > m_currentIndex));
        BranchData* data = m_graph.m_branchData.add();
        *data = BranchData::withBytecodeIndices(taken, notTaken);
        return data;
    }
    
    Node* addToGraph(NodeType op, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, NodeOrigin(currentCodeOrigin()), Edge(child1), Edge(child2),
            Edge(child3));
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    Node* addToGraph(NodeType op, Edge child1, Edge child2 = Edge(), Edge child3 = Edge())
    {
        Node* result = m_graph.addNode(
            SpecNone, op, NodeOrigin(currentCodeOrigin()), child1, child2, child3);
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    Node* addToGraph(NodeType op, OpInfo info, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, NodeOrigin(currentCodeOrigin()), info, Edge(child1), Edge(child2),
            Edge(child3));
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    Node* addToGraph(NodeType op, OpInfo info1, OpInfo info2, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, NodeOrigin(currentCodeOrigin()), info1, info2,
            Edge(child1), Edge(child2), Edge(child3));
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    
    Node* addToGraph(Node::VarArgTag, NodeType op, OpInfo info1, OpInfo info2)
    {
        Node* result = m_graph.addNode(
            SpecNone, Node::VarArg, op, NodeOrigin(currentCodeOrigin()), info1, info2,
            m_graph.m_varArgChildren.size() - m_numPassedVarArgs, m_numPassedVarArgs);
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        
        m_numPassedVarArgs = 0;
        
        return result;
    }
    
    void removeLastNodeFromGraph(NodeType expectedNodeType)
    {
        Node* node = m_currentBlock->takeLast();
        RELEASE_ASSERT(node->op() == expectedNodeType);
        m_graph.m_allocator.free(node);
    }

    void addVarArgChild(Node* child)
    {
        m_graph.m_varArgChildren.append(Edge(child));
        m_numPassedVarArgs++;
    }
    
    Node* addCallWithoutSettingResult(
        NodeType op, OpInfo opInfo, Node* callee, int argCount, int registerOffset,
        SpeculatedType prediction)
    {
        addVarArgChild(callee);
        size_t parameterSlots = JSStack::CallFrameHeaderSize - JSStack::CallerFrameAndPCSize + argCount;
        if (parameterSlots > m_parameterSlots)
            m_parameterSlots = parameterSlots;

        int dummyThisArgument = op == Call || op == NativeCall || op == ProfiledCall ? 0 : 1;
        for (int i = 0 + dummyThisArgument; i < argCount; ++i)
            addVarArgChild(get(virtualRegisterForArgument(i, registerOffset)));

        return addToGraph(Node::VarArg, op, opInfo, OpInfo(prediction));
    }
    
    Node* addCall(
        int result, NodeType op, OpInfo opInfo, Node* callee, int argCount, int registerOffset,
        SpeculatedType prediction)
    {
        Node* call = addCallWithoutSettingResult(
            op, opInfo, callee, argCount, registerOffset, prediction);
        VirtualRegister resultReg(result);
        if (resultReg.isValid())
            set(VirtualRegister(result), call);
        return call;
    }
    
    Node* cellConstantWithStructureCheck(JSCell* object, Structure* structure)
    {
        Node* objectNode = weakJSConstant(object);
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(structure)), objectNode);
        return objectNode;
    }
    
    SpeculatedType getPredictionWithoutOSRExit(unsigned bytecodeIndex)
    {
        ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        return m_inlineStackTop->m_profiledBlock->valueProfilePredictionForBytecodeOffset(locker, bytecodeIndex);
    }

    SpeculatedType getPrediction(unsigned bytecodeIndex)
    {
        SpeculatedType prediction = getPredictionWithoutOSRExit(bytecodeIndex);
        
        if (prediction == SpecNone) {
            // We have no information about what values this node generates. Give up
            // on executing this code, since we're likely to do more damage than good.
            addToGraph(ForceOSRExit);
        }
        
        return prediction;
    }
    
    SpeculatedType getPredictionWithoutOSRExit()
    {
        return getPredictionWithoutOSRExit(m_currentIndex);
    }
    
    SpeculatedType getPrediction()
    {
        return getPrediction(m_currentIndex);
    }
    
    ArrayMode getArrayMode(ArrayProfile* profile, Array::Action action)
    {
        ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        profile->computeUpdatedPrediction(locker, m_inlineStackTop->m_profiledBlock);
        return ArrayMode::fromObserved(locker, profile, action, false);
    }
    
    ArrayMode getArrayMode(ArrayProfile* profile)
    {
        return getArrayMode(profile, Array::Read);
    }
    
    ArrayMode getArrayModeConsideringSlowPath(ArrayProfile* profile, Array::Action action)
    {
        ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        
        profile->computeUpdatedPrediction(locker, m_inlineStackTop->m_profiledBlock);
        
        bool makeSafe =
            m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex)
            || profile->outOfBounds(locker);
        
        ArrayMode result = ArrayMode::fromObserved(locker, profile, action, makeSafe);
        
        return result;
    }
    
    Node* makeSafe(Node* node)
    {
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInDFG);
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
            node->mergeFlags(NodeMayNegZeroInDFG);
        
        if (!isX86() && node->op() == ArithMod)
            return node;

        if (!m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex))
            return node;
        
        switch (node->op()) {
        case UInt32ToNumber:
        case ArithAdd:
        case ArithSub:
        case ValueAdd:
        case ArithMod: // for ArithMod "MayOverflow" means we tried to divide by zero, or we saw double.
            node->mergeFlags(NodeMayOverflowInBaseline);
            break;
            
        case ArithNegate:
            // Currently we can't tell the difference between a negation overflowing
            // (i.e. -(1 << 31)) or generating negative zero (i.e. -0). If it took slow
            // path then we assume that it did both of those things.
            node->mergeFlags(NodeMayOverflowInBaseline);
            node->mergeFlags(NodeMayNegZeroInBaseline);
            break;

        case ArithMul:
            // FIXME: We should detect cases where we only overflowed but never created
            // negative zero.
            // https://bugs.webkit.org/show_bug.cgi?id=132470
            if (m_inlineStackTop->m_profiledBlock->likelyToTakeDeepestSlowCase(m_currentIndex)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
                node->mergeFlags(NodeMayOverflowInBaseline | NodeMayNegZeroInBaseline);
            else if (m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
                node->mergeFlags(NodeMayNegZeroInBaseline);
            break;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        return node;
    }
    
    Node* makeDivSafe(Node* node)
    {
        ASSERT(node->op() == ArithDiv);
        
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInDFG);
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
            node->mergeFlags(NodeMayNegZeroInDFG);
        
        // The main slow case counter for op_div in the old JIT counts only when
        // the operands are not numbers. We don't care about that since we already
        // have speculations in place that take care of that separately. We only
        // care about when the outcome of the division is not an integer, which
        // is what the special fast case counter tells us.
        
        if (!m_inlineStackTop->m_profiledBlock->couldTakeSpecialFastCase(m_currentIndex))
            return node;
        
        // FIXME: It might be possible to make this more granular.
        node->mergeFlags(NodeMayOverflowInBaseline | NodeMayNegZeroInBaseline);
        
        return node;
    }
    
    void buildOperandMapsIfNecessary();
    
    VM* m_vm;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;
    Graph& m_graph;

    // The current block being generated.
    BasicBlock* m_currentBlock;
    // The bytecode index of the current instruction being generated.
    unsigned m_currentIndex;

    FrozenValue* m_constantUndefined;
    FrozenValue* m_constantNull;
    FrozenValue* m_constantNaN;
    FrozenValue* m_constantOne;
    Vector<Node*, 16> m_constants;

    // The number of arguments passed to the function.
    unsigned m_numArguments;
    // The number of locals (vars + temporaries) used in the function.
    unsigned m_numLocals;
    // The number of slots (in units of sizeof(Register)) that we need to
    // preallocate for arguments to outgoing calls from this frame. This
    // number includes the CallFrame slots that we initialize for the callee
    // (but not the callee-initialized CallerFrame and ReturnPC slots).
    // This number is 0 if and only if this function is a leaf.
    unsigned m_parameterSlots;
    // The number of var args passed to the next var arg node.
    unsigned m_numPassedVarArgs;

    HashMap<ConstantBufferKey, unsigned> m_constantBufferCache;
    
    Vector<VariableWatchpointSet*, 16> m_localWatchpoints;
    
    struct InlineStackEntry {
        ByteCodeParser* m_byteCodeParser;
        
        CodeBlock* m_codeBlock;
        CodeBlock* m_profiledBlock;
        InlineCallFrame* m_inlineCallFrame;
        
        ScriptExecutable* executable() { return m_codeBlock->ownerExecutable(); }
        
        QueryableExitProfile m_exitProfile;
        
        // Remapping of identifier and constant numbers from the code block being
        // inlined (inline callee) to the code block that we're inlining into
        // (the machine code block, which is the transitive, though not necessarily
        // direct, caller).
        Vector<unsigned> m_identifierRemap;
        Vector<unsigned> m_constantBufferRemap;
        Vector<unsigned> m_switchRemap;
        
        // Blocks introduced by this code block, which need successor linking.
        // May include up to one basic block that includes the continuation after
        // the callsite in the caller. These must be appended in the order that they
        // are created, but their bytecodeBegin values need not be in order as they
        // are ignored.
        Vector<UnlinkedBlock> m_unlinkedBlocks;
        
        // Potential block linking targets. Must be sorted by bytecodeBegin, and
        // cannot have two blocks that have the same bytecodeBegin.
        Vector<BasicBlock*> m_blockLinkingTargets;
        
        // If the callsite's basic block was split into two, then this will be
        // the head of the callsite block. It needs its successors linked to the
        // m_unlinkedBlocks, but not the other way around: there's no way for
        // any blocks in m_unlinkedBlocks to jump back into this block.
        BasicBlock* m_callsiteBlockHead;
        
        // Does the callsite block head need linking? This is typically true
        // but will be false for the machine code block's inline stack entry
        // (since that one is not inlined) and for cases where an inline callee
        // did the linking for us.
        bool m_callsiteBlockHeadNeedsLinking;
        
        VirtualRegister m_returnValue;
        
        // Speculations about variable types collected from the profiled code block,
        // which are based on OSR exit profiles that past DFG compilatins of this
        // code block had gathered.
        LazyOperandValueProfileParser m_lazyOperands;
        
        CallLinkInfoMap m_callLinkInfos;
        StubInfoMap m_stubInfos;
        
        // Did we see any returns? We need to handle the (uncommon but necessary)
        // case where a procedure that does not return was inlined.
        bool m_didReturn;
        
        // Did we have any early returns?
        bool m_didEarlyReturn;
        
        // Pointers to the argument position trackers for this slice of code.
        Vector<ArgumentPosition*> m_argumentPositions;
        
        InlineStackEntry* m_caller;
        
        InlineStackEntry(
            ByteCodeParser*,
            CodeBlock*,
            CodeBlock* profiledBlock,
            BasicBlock* callsiteBlockHead,
            JSFunction* callee, // Null if this is a closure call.
            VirtualRegister returnValueVR,
            VirtualRegister inlineCallFrameStart,
            int argumentCountIncludingThis,
            InlineCallFrame::Kind);
        
        ~InlineStackEntry()
        {
            m_byteCodeParser->m_inlineStackTop = m_caller;
        }
        
        VirtualRegister remapOperand(VirtualRegister operand) const
        {
            if (!m_inlineCallFrame)
                return operand;
            
            ASSERT(!operand.isConstant());

            return VirtualRegister(operand.offset() + m_inlineCallFrame->stackOffset);
        }
    };
    
    InlineStackEntry* m_inlineStackTop;
    
    struct DelayedSetLocal {
        VirtualRegister m_operand;
        Node* m_value;
        
        DelayedSetLocal() { }
        DelayedSetLocal(VirtualRegister operand, Node* value)
            : m_operand(operand)
            , m_value(value)
        {
        }
        
        Node* execute(ByteCodeParser* parser, SetMode setMode = NormalSet)
        {
            if (m_operand.isArgument())
                return parser->setArgument(m_operand, m_value, setMode);
            return parser->setLocal(m_operand, m_value, setMode);
        }
    };
    
    Vector<DelayedSetLocal, 2> m_setLocalQueue;

    // Have we built operand maps? We initialize them lazily, and only when doing
    // inlining.
    bool m_haveBuiltOperandMaps;
    // Mapping between identifier names and numbers.
    BorrowedIdentifierMap m_identifierMap;
    
    CodeBlock* m_dfgCodeBlock;
    CallLinkStatus::ContextMap m_callContextMap;
    StubInfoMap m_dfgStubInfos;
    
    Instruction* m_currentInstruction;
};

#define NEXT_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    continue

#define LAST_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    return shouldContinueParsing

void ByteCodeParser::handleCall(Instruction* pc, NodeType op, CodeSpecializationKind kind)
{
    ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_construct));
    handleCall(
        pc[1].u.operand, op, kind, OPCODE_LENGTH(op_call),
        pc[2].u.operand, pc[3].u.operand, -pc[4].u.operand);
}

void ByteCodeParser::handleCall(
    int result, NodeType op, CodeSpecializationKind kind, unsigned instructionSize,
    int callee, int argumentCountIncludingThis, int registerOffset)
{
    Node* callTarget = get(VirtualRegister(callee));
    
    CallLinkStatus callLinkStatus = CallLinkStatus::computeFor(
        m_inlineStackTop->m_profiledBlock, currentCodeOrigin(),
        m_inlineStackTop->m_callLinkInfos, m_callContextMap);
    
    handleCall(
        result, op, InlineCallFrame::kindFor(kind), instructionSize, callTarget,
        argumentCountIncludingThis, registerOffset, callLinkStatus);
}
    
void ByteCodeParser::handleCall(
    int result, NodeType op, InlineCallFrame::Kind kind, unsigned instructionSize,
    Node* callTarget, int argumentCountIncludingThis, int registerOffset,
    CallLinkStatus callLinkStatus)
{
    handleCall(
        result, op, kind, instructionSize, callTarget, argumentCountIncludingThis,
        registerOffset, callLinkStatus, getPrediction());
}

void ByteCodeParser::handleCall(
    int result, NodeType op, InlineCallFrame::Kind kind, unsigned instructionSize,
    Node* callTarget, int argumentCountIncludingThis, int registerOffset,
    CallLinkStatus callLinkStatus, SpeculatedType prediction)
{
    ASSERT(registerOffset <= 0);
    
    if (callTarget->hasConstant())
        callLinkStatus = CallLinkStatus(callTarget->asJSValue()).setIsProved(true);
    
    if ((!callLinkStatus.canOptimize() || callLinkStatus.size() != 1)
        && !isFTL(m_graph.m_plan.mode) && Options::useFTLJIT()
        && InlineCallFrame::isNormalCall(kind)
        && CallEdgeLog::isEnabled()
        && Options::dfgDoesCallEdgeProfiling()) {
        ASSERT(op == Call || op == Construct);
        if (op == Call)
            op = ProfiledCall;
        else
            op = ProfiledConstruct;
    }
    
    if (!callLinkStatus.canOptimize()) {
        // Oddly, this conflates calls that haven't executed with calls that behaved sufficiently polymorphically
        // that we cannot optimize them.
        
        addCall(result, op, OpInfo(), callTarget, argumentCountIncludingThis, registerOffset, prediction);
        return;
    }
    
    unsigned nextOffset = m_currentIndex + instructionSize;
    
    OpInfo callOpInfo;
    
    if (handleInlining(callTarget, result, callLinkStatus, registerOffset, argumentCountIncludingThis, nextOffset, op, kind, prediction)) {
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedCall();
        return;
    }
    
#if ENABLE(FTL_NATIVE_CALL_INLINING)
    if (isFTL(m_graph.m_plan.mode) && Options::optimizeNativeCalls() && callLinkStatus.size() == 1 && !callLinkStatus.couldTakeSlowPath()) {
        CallVariant callee = callLinkStatus[0].callee();
        JSFunction* function = callee.function();
        CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
        if (function && function->isHostFunction()) {
            emitFunctionChecks(callee, callTarget, registerOffset, specializationKind);
            callOpInfo = OpInfo(m_graph.freeze(function));

            if (op == Call || op == ProfiledCall)
                op = NativeCall;
            else {
                ASSERT(op == Construct || op == ProfiledConstruct);
                op = NativeConstruct;
            }
        }
    }
#endif
    
    addCall(result, op, callOpInfo, callTarget, argumentCountIncludingThis, registerOffset, prediction);
}

void ByteCodeParser::emitFunctionChecks(CallVariant callee, Node* callTarget, int registerOffset, CodeSpecializationKind kind)
{
    Node* thisArgument;
    if (kind == CodeForCall)
        thisArgument = get(virtualRegisterForArgument(0, registerOffset));
    else
        thisArgument = 0;

    JSCell* calleeCell;
    Node* callTargetForCheck;
    if (callee.isClosureCall()) {
        calleeCell = callee.executable();
        callTargetForCheck = addToGraph(GetExecutable, callTarget);
    } else {
        calleeCell = callee.nonExecutableCallee();
        callTargetForCheck = callTarget;
    }
    
    ASSERT(calleeCell);
    addToGraph(CheckCell, OpInfo(m_graph.freeze(calleeCell)), callTargetForCheck, thisArgument);
}

void ByteCodeParser::undoFunctionChecks(CallVariant callee)
{
    removeLastNodeFromGraph(CheckCell);
    if (callee.isClosureCall())
        removeLastNodeFromGraph(GetExecutable);
}

void ByteCodeParser::emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind kind)
{
    for (int i = kind == CodeForCall ? 0 : 1; i < argumentCountIncludingThis; ++i)
        addToGraph(Phantom, get(virtualRegisterForArgument(i, registerOffset)));
}

unsigned ByteCodeParser::inliningCost(CallVariant callee, int argumentCountIncludingThis, CodeSpecializationKind kind)
{
    if (verbose)
        dataLog("Considering inlining ", callee, " into ", currentCodeOrigin(), "\n");
    
    FunctionExecutable* executable = callee.functionExecutable();
    if (!executable) {
        if (verbose)
            dataLog("    Failing because there is no function executable.");
        return UINT_MAX;
    }
    
    // Does the number of arguments we're passing match the arity of the target? We currently
    // inline only if the number of arguments passed is greater than or equal to the number
    // arguments expected.
    if (static_cast<int>(executable->parameterCount()) + 1 > argumentCountIncludingThis) {
        if (verbose)
            dataLog("    Failing because of arity mismatch.\n");
        return UINT_MAX;
    }
    
    // Do we have a code block, and does the code block's size match the heuristics/requirements for
    // being an inline candidate? We might not have a code block if code was thrown away or if we
    // simply hadn't actually made this call yet. We could still theoretically attempt to inline it
    // if we had a static proof of what was being called; this might happen for example if you call a
    // global function, where watchpointing gives us static information. Overall, it's a rare case
    // because we expect that any hot callees would have already been compiled.
    CodeBlock* codeBlock = executable->baselineCodeBlockFor(kind);
    if (!codeBlock) {
        if (verbose)
            dataLog("    Failing because no code block available.\n");
        return UINT_MAX;
    }
    CapabilityLevel capabilityLevel = inlineFunctionForCapabilityLevel(
        codeBlock, kind, callee.isClosureCall());
    if (!canInline(capabilityLevel)) {
        if (verbose)
            dataLog("    Failing because the function is not inlineable.\n");
        return UINT_MAX;
    }
    
    // Check if the caller is already too large. We do this check here because that's just
    // where we happen to also have the callee's code block, and we want that for the
    // purpose of unsetting SABI.
    if (!isSmallEnoughToInlineCodeInto(m_codeBlock)) {
        codeBlock->m_shouldAlwaysBeInlined = false;
        if (verbose)
            dataLog("    Failing because the caller is too large.\n");
        return UINT_MAX;
    }
    
    // FIXME: this should be better at predicting how much bloat we will introduce by inlining
    // this function.
    // https://bugs.webkit.org/show_bug.cgi?id=127627
    
    // Have we exceeded inline stack depth, or are we trying to inline a recursive call to
    // too many levels? If either of these are detected, then don't inline. We adjust our
    // heuristics if we are dealing with a function that cannot otherwise be compiled.
    
    unsigned depth = 0;
    unsigned recursion = 0;
    
    for (InlineStackEntry* entry = m_inlineStackTop; entry; entry = entry->m_caller) {
        ++depth;
        if (depth >= Options::maximumInliningDepth()) {
            if (verbose)
                dataLog("    Failing because depth exceeded.\n");
            return UINT_MAX;
        }
        
        if (entry->executable() == executable) {
            ++recursion;
            if (recursion >= Options::maximumInliningRecursion()) {
                if (verbose)
                    dataLog("    Failing because recursion detected.\n");
                return UINT_MAX;
            }
        }
    }
    
    if (verbose)
        dataLog("    Inlining should be possible.\n");
    
    // It might be possible to inline.
    return codeBlock->instructionCount();
}

void ByteCodeParser::inlineCall(Node* callTargetNode, int resultOperand, CallVariant callee, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind kind, CallerLinkability callerLinkability)
{
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    
    ASSERT(inliningCost(callee, argumentCountIncludingThis, specializationKind) != UINT_MAX);
    
    CodeBlock* codeBlock = callee.functionExecutable()->baselineCodeBlockFor(specializationKind);

    // FIXME: Don't flush constants!
    
    int inlineCallFrameStart = m_inlineStackTop->remapOperand(VirtualRegister(registerOffset)).offset() + JSStack::CallFrameHeaderSize;
    
    ensureLocals(
        VirtualRegister(inlineCallFrameStart).toLocal() + 1 +
        JSStack::CallFrameHeaderSize + codeBlock->m_numCalleeRegisters);
    
    size_t argumentPositionStart = m_graph.m_argumentPositions.size();

    VirtualRegister resultReg(resultOperand);
    if (resultReg.isValid())
        resultReg = m_inlineStackTop->remapOperand(resultReg);
    
    InlineStackEntry inlineStackEntry(
        this, codeBlock, codeBlock, m_graph.lastBlock(), callee.function(), resultReg,
        (VirtualRegister)inlineCallFrameStart, argumentCountIncludingThis, kind);
    
    // This is where the actual inlining really happens.
    unsigned oldIndex = m_currentIndex;
    m_currentIndex = 0;

    InlineVariableData inlineVariableData;
    inlineVariableData.inlineCallFrame = m_inlineStackTop->m_inlineCallFrame;
    inlineVariableData.argumentPositionStart = argumentPositionStart;
    inlineVariableData.calleeVariable = 0;
    
    RELEASE_ASSERT(
        m_inlineStackTop->m_inlineCallFrame->isClosureCall
        == callee.isClosureCall());
    if (callee.isClosureCall()) {
        VariableAccessData* calleeVariable =
            set(VirtualRegister(JSStack::Callee), callTargetNode, ImmediateNakedSet)->variableAccessData();
        VariableAccessData* scopeVariable =
            set(VirtualRegister(JSStack::ScopeChain), addToGraph(GetScope, callTargetNode), ImmediateNakedSet)->variableAccessData();
        
        calleeVariable->mergeShouldNeverUnbox(true);
        scopeVariable->mergeShouldNeverUnbox(true);
        
        inlineVariableData.calleeVariable = calleeVariable;
    }
    
    m_graph.m_inlineVariableData.append(inlineVariableData);
    
    parseCodeBlock();
    clearCaches(); // Reset our state now that we're back to the outer code.
    
    m_currentIndex = oldIndex;
    
    // If the inlined code created some new basic blocks, then we have linking to do.
    if (inlineStackEntry.m_callsiteBlockHead != m_graph.lastBlock()) {
        
        ASSERT(!inlineStackEntry.m_unlinkedBlocks.isEmpty());
        if (inlineStackEntry.m_callsiteBlockHeadNeedsLinking)
            linkBlock(inlineStackEntry.m_callsiteBlockHead, inlineStackEntry.m_blockLinkingTargets);
        else
            ASSERT(inlineStackEntry.m_callsiteBlockHead->isLinked);
        
        if (callerLinkability == CallerDoesNormalLinking)
            cancelLinkingForBlock(inlineStackEntry.m_caller, inlineStackEntry.m_callsiteBlockHead);
        
        linkBlocks(inlineStackEntry.m_unlinkedBlocks, inlineStackEntry.m_blockLinkingTargets);
    } else
        ASSERT(inlineStackEntry.m_unlinkedBlocks.isEmpty());
    
    BasicBlock* lastBlock = m_graph.lastBlock();
    // If there was a return, but no early returns, then we're done. We allow parsing of
    // the caller to continue in whatever basic block we're in right now.
    if (!inlineStackEntry.m_didEarlyReturn && inlineStackEntry.m_didReturn) {
        ASSERT(lastBlock->isEmpty() || !lastBlock->last()->isTerminal());
        
        // If we created new blocks then the last block needs linking, but in the
        // caller. It doesn't need to be linked to, but it needs outgoing links.
        if (!inlineStackEntry.m_unlinkedBlocks.isEmpty()) {
            // For debugging purposes, set the bytecodeBegin. Note that this doesn't matter
            // for release builds because this block will never serve as a potential target
            // in the linker's binary search.
            lastBlock->bytecodeBegin = m_currentIndex;
            if (callerLinkability == CallerDoesNormalLinking) {
                if (verbose)
                    dataLog("Adding unlinked block ", RawPointer(m_graph.lastBlock()), " (one return)\n");
                m_inlineStackTop->m_caller->m_unlinkedBlocks.append(UnlinkedBlock(m_graph.lastBlock()));
            }
        }
        
        m_currentBlock = m_graph.lastBlock();
        return;
    }
    
    // If we get to this point then all blocks must end in some sort of terminals.
    ASSERT(lastBlock->last()->isTerminal());

    // Need to create a new basic block for the continuation at the caller.
    RefPtr<BasicBlock> block = adoptRef(new BasicBlock(nextOffset, m_numArguments, m_numLocals, PNaN));

    // Link the early returns to the basic block we're about to create.
    for (size_t i = 0; i < inlineStackEntry.m_unlinkedBlocks.size(); ++i) {
        if (!inlineStackEntry.m_unlinkedBlocks[i].m_needsEarlyReturnLinking)
            continue;
        BasicBlock* blockToLink = inlineStackEntry.m_unlinkedBlocks[i].m_block;
        ASSERT(!blockToLink->isLinked);
        Node* node = blockToLink->last();
        ASSERT(node->op() == Jump);
        ASSERT(!node->targetBlock());
        node->targetBlock() = block.get();
        inlineStackEntry.m_unlinkedBlocks[i].m_needsEarlyReturnLinking = false;
        if (verbose)
            dataLog("Marking ", RawPointer(blockToLink), " as linked (jumps to return)\n");
        blockToLink->didLink();
    }
    
    m_currentBlock = block.get();
    ASSERT(m_inlineStackTop->m_caller->m_blockLinkingTargets.isEmpty() || m_inlineStackTop->m_caller->m_blockLinkingTargets.last()->bytecodeBegin < nextOffset);
    if (verbose)
        dataLog("Adding unlinked block ", RawPointer(block.get()), " (many returns)\n");
    if (callerLinkability == CallerDoesNormalLinking) {
        m_inlineStackTop->m_caller->m_unlinkedBlocks.append(UnlinkedBlock(block.get()));
        m_inlineStackTop->m_caller->m_blockLinkingTargets.append(block.get());
    }
    m_graph.appendBlock(block);
    prepareToParseBlock();
}

void ByteCodeParser::cancelLinkingForBlock(InlineStackEntry* inlineStackEntry, BasicBlock* block)
{
    // It's possible that the callsite block head is not owned by the caller.
    if (!inlineStackEntry->m_unlinkedBlocks.isEmpty()) {
        // It's definitely owned by the caller, because the caller created new blocks.
        // Assert that this all adds up.
        ASSERT_UNUSED(block, inlineStackEntry->m_unlinkedBlocks.last().m_block == block);
        ASSERT(inlineStackEntry->m_unlinkedBlocks.last().m_needsNormalLinking);
        inlineStackEntry->m_unlinkedBlocks.last().m_needsNormalLinking = false;
    } else {
        // It's definitely not owned by the caller. Tell the caller that he does not
        // need to link his callsite block head, because we did it for him.
        ASSERT(inlineStackEntry->m_callsiteBlockHeadNeedsLinking);
        ASSERT_UNUSED(block, inlineStackEntry->m_callsiteBlockHead == block);
        inlineStackEntry->m_callsiteBlockHeadNeedsLinking = false;
    }
}

bool ByteCodeParser::attemptToInlineCall(Node* callTargetNode, int resultOperand, CallVariant callee, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind kind, CallerLinkability callerLinkability, SpeculatedType prediction, unsigned& inliningBalance)
{
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    
    if (!inliningBalance)
        return false;
    
    if (InternalFunction* function = callee.internalFunction()) {
        if (handleConstantInternalFunction(resultOperand, function, registerOffset, argumentCountIncludingThis, specializationKind)) {
            addToGraph(Phantom, callTargetNode);
            emitArgumentPhantoms(registerOffset, argumentCountIncludingThis, specializationKind);
            inliningBalance--;
            return true;
        }
        return false;
    }
    
    Intrinsic intrinsic = callee.intrinsicFor(specializationKind);
    if (intrinsic != NoIntrinsic) {
        if (handleIntrinsic(resultOperand, intrinsic, registerOffset, argumentCountIncludingThis, prediction)) {
            addToGraph(Phantom, callTargetNode);
            emitArgumentPhantoms(registerOffset, argumentCountIncludingThis, specializationKind);
            inliningBalance--;
            return true;
        }
        return false;
    }
    
    unsigned myInliningCost = inliningCost(callee, argumentCountIncludingThis, specializationKind);
    if (myInliningCost > inliningBalance)
        return false;
    
    inlineCall(callTargetNode, resultOperand, callee, registerOffset, argumentCountIncludingThis, nextOffset, kind, callerLinkability);
    inliningBalance -= myInliningCost;
    return true;
}

bool ByteCodeParser::handleInlining(Node* callTargetNode, int resultOperand, const CallLinkStatus& callLinkStatus, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, NodeType callOp, InlineCallFrame::Kind kind, SpeculatedType prediction)
{
    if (verbose) {
        dataLog("Handling inlining...\n");
        dataLog("Stack: ", currentCodeOrigin(), "\n");
    }
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    
    if (!callLinkStatus.size()) {
        if (verbose)
            dataLog("Bailing inlining.\n");
        return false;
    }
    
    unsigned inliningBalance = Options::maximumFunctionForCallInlineCandidateInstructionCount();
    if (specializationKind == CodeForConstruct)
        inliningBalance = std::min(inliningBalance, Options::maximumFunctionForConstructInlineCandidateInstructionCount());
    if (callLinkStatus.isClosureCall())
        inliningBalance = std::min(inliningBalance, Options::maximumFunctionForClosureCallInlineCandidateInstructionCount());
    
    // First check if we can avoid creating control flow. Our inliner does some CFG
    // simplification on the fly and this helps reduce compile times, but we can only leverage
    // this in cases where we don't need control flow diamonds to check the callee.
    if (!callLinkStatus.couldTakeSlowPath() && callLinkStatus.size() == 1) {
        emitFunctionChecks(
            callLinkStatus[0].callee(), callTargetNode, registerOffset, specializationKind);
        bool result = attemptToInlineCall(
            callTargetNode, resultOperand, callLinkStatus[0].callee(), registerOffset,
            argumentCountIncludingThis, nextOffset, kind, CallerDoesNormalLinking, prediction,
            inliningBalance);
        if (!result && !callLinkStatus.isProved())
            undoFunctionChecks(callLinkStatus[0].callee());
        if (verbose) {
            dataLog("Done inlining (simple).\n");
            dataLog("Stack: ", currentCodeOrigin(), "\n");
        }
        return result;
    }
    
    // We need to create some kind of switch over callee. For now we only do this if we believe that
    // we're in the top tier. We have two reasons for this: first, it provides us an opportunity to
    // do more detailed polyvariant/polymorphic profiling; and second, it reduces compile times in
    // the DFG. And by polyvariant profiling we mean polyvariant profiling of *this* call. Note that
    // we could improve that aspect of this by doing polymorphic inlining but having the profiling
    // also. Currently we opt against this, but it could be interesting. That would require having a
    // separate node for call edge profiling.
    // FIXME: Introduce the notion of a separate call edge profiling node.
    // https://bugs.webkit.org/show_bug.cgi?id=136033
    if (!isFTL(m_graph.m_plan.mode) || !Options::enablePolymorphicCallInlining()) {
        if (verbose) {
            dataLog("Bailing inlining (hard).\n");
            dataLog("Stack: ", currentCodeOrigin(), "\n");
        }
        return false;
    }
    
    unsigned oldOffset = m_currentIndex;
    
    bool allAreClosureCalls = true;
    bool allAreDirectCalls = true;
    for (unsigned i = callLinkStatus.size(); i--;) {
        if (callLinkStatus[i].callee().isClosureCall())
            allAreDirectCalls = false;
        else
            allAreClosureCalls = false;
    }
    
    Node* thingToSwitchOn;
    if (allAreDirectCalls)
        thingToSwitchOn = callTargetNode;
    else if (allAreClosureCalls)
        thingToSwitchOn = addToGraph(GetExecutable, callTargetNode);
    else {
        // FIXME: We should be able to handle this case, but it's tricky and we don't know of cases
        // where it would be beneficial. Also, CallLinkStatus would make all callees appear like
        // closure calls if any calls were closure calls - except for calls to internal functions.
        // So this will only arise if some callees are internal functions and others are closures.
        // https://bugs.webkit.org/show_bug.cgi?id=136020
        if (verbose) {
            dataLog("Bailing inlining (mix).\n");
            dataLog("Stack: ", currentCodeOrigin(), "\n");
        }
        return false;
    }
    
    if (verbose) {
        dataLog("Doing hard inlining...\n");
        dataLog("Stack: ", currentCodeOrigin(), "\n");
    }
    
    // This makes me wish that we were in SSA all the time. We need to pick a variable into which to
    // store the callee so that it will be accessible to all of the blocks we're about to create. We
    // get away with doing an immediate-set here because we wouldn't have performed any side effects
    // yet.
    if (verbose)
        dataLog("Register offset: ", registerOffset);
    VirtualRegister calleeReg(registerOffset + JSStack::Callee);
    calleeReg = m_inlineStackTop->remapOperand(calleeReg);
    if (verbose)
        dataLog("Callee is going to be ", calleeReg, "\n");
    setDirect(calleeReg, callTargetNode, ImmediateSetWithFlush);
    
    SwitchData& data = *m_graph.m_switchData.add();
    data.kind = SwitchCell;
    addToGraph(Switch, OpInfo(&data), thingToSwitchOn);
    
    BasicBlock* originBlock = m_currentBlock;
    if (verbose)
        dataLog("Marking ", RawPointer(originBlock), " as linked (origin of poly inline)\n");
    originBlock->didLink();
    cancelLinkingForBlock(m_inlineStackTop, originBlock);
    
    // Each inlined callee will have a landing block that it returns at. They should all have jumps
    // to the continuation block, which we create last.
    Vector<BasicBlock*> landingBlocks;
    
    // We make force this true if we give up on inlining any of the edges.
    bool couldTakeSlowPath = callLinkStatus.couldTakeSlowPath();
    
    if (verbose)
        dataLog("About to loop over functions at ", currentCodeOrigin(), ".\n");
    
    for (unsigned i = 0; i < callLinkStatus.size(); ++i) {
        m_currentIndex = oldOffset;
        RefPtr<BasicBlock> block = adoptRef(new BasicBlock(UINT_MAX, m_numArguments, m_numLocals, PNaN));
        m_currentBlock = block.get();
        m_graph.appendBlock(block);
        prepareToParseBlock();
        
        Node* myCallTargetNode = getDirect(calleeReg);
        
        bool inliningResult = attemptToInlineCall(
            myCallTargetNode, resultOperand, callLinkStatus[i].callee(), registerOffset,
            argumentCountIncludingThis, nextOffset, kind, CallerLinksManually, prediction,
            inliningBalance);
        
        if (!inliningResult) {
            // That failed so we let the block die. Nothing interesting should have been added to
            // the block. We also give up on inlining any of the (less frequent) callees.
            ASSERT(m_currentBlock == block.get());
            ASSERT(m_graph.m_blocks.last() == block);
            m_graph.killBlockAndItsContents(block.get());
            m_graph.m_blocks.removeLast();
            
            // The fact that inlining failed means we need a slow path.
            couldTakeSlowPath = true;
            break;
        }
        
        JSCell* thingToCaseOn;
        if (allAreDirectCalls)
            thingToCaseOn = callLinkStatus[i].callee().nonExecutableCallee();
        else {
            ASSERT(allAreClosureCalls);
            thingToCaseOn = callLinkStatus[i].callee().executable();
        }
        data.cases.append(SwitchCase(m_graph.freeze(thingToCaseOn), block.get()));
        m_currentIndex = nextOffset;
        processSetLocalQueue(); // This only comes into play for intrinsics, since normal inlined code will leave an empty queue.
        addToGraph(Jump);
        if (verbose)
            dataLog("Marking ", RawPointer(m_currentBlock), " as linked (tail of poly inlinee)\n");
        m_currentBlock->didLink();
        landingBlocks.append(m_currentBlock);

        if (verbose)
            dataLog("Finished inlining ", callLinkStatus[i].callee(), " at ", currentCodeOrigin(), ".\n");
    }
    
    RefPtr<BasicBlock> slowPathBlock = adoptRef(
        new BasicBlock(UINT_MAX, m_numArguments, m_numLocals, PNaN));
    m_currentIndex = oldOffset;
    data.fallThrough = BranchTarget(slowPathBlock.get());
    m_graph.appendBlock(slowPathBlock);
    if (verbose)
        dataLog("Marking ", RawPointer(slowPathBlock.get()), " as linked (slow path block)\n");
    slowPathBlock->didLink();
    prepareToParseBlock();
    m_currentBlock = slowPathBlock.get();
    Node* myCallTargetNode = getDirect(calleeReg);
    if (couldTakeSlowPath) {
        addCall(
            resultOperand, callOp, OpInfo(), myCallTargetNode, argumentCountIncludingThis,
            registerOffset, prediction);
    } else {
        addToGraph(CheckBadCell);
        addToGraph(Phantom, myCallTargetNode);
        emitArgumentPhantoms(registerOffset, argumentCountIncludingThis, specializationKind);
        
        set(VirtualRegister(resultOperand), addToGraph(BottomValue));
    }

    m_currentIndex = nextOffset;
    processSetLocalQueue();
    addToGraph(Jump);
    landingBlocks.append(m_currentBlock);
    
    RefPtr<BasicBlock> continuationBlock = adoptRef(
        new BasicBlock(UINT_MAX, m_numArguments, m_numLocals, PNaN));
    m_graph.appendBlock(continuationBlock);
    if (verbose)
        dataLog("Adding unlinked block ", RawPointer(continuationBlock.get()), " (continuation)\n");
    m_inlineStackTop->m_unlinkedBlocks.append(UnlinkedBlock(continuationBlock.get()));
    prepareToParseBlock();
    m_currentBlock = continuationBlock.get();
    
    for (unsigned i = landingBlocks.size(); i--;)
        landingBlocks[i]->last()->targetBlock() = continuationBlock.get();
    
    m_currentIndex = oldOffset;
    
    if (verbose) {
        dataLog("Done inlining (hard).\n");
        dataLog("Stack: ", currentCodeOrigin(), "\n");
    }
    return true;
}

bool ByteCodeParser::handleMinMax(int resultOperand, NodeType op, int registerOffset, int argumentCountIncludingThis)
{
    if (argumentCountIncludingThis == 1) { // Math.min()
        set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
        return true;
    }
     
    if (argumentCountIncludingThis == 2) { // Math.min(x)
        Node* result = get(VirtualRegister(virtualRegisterForArgument(1, registerOffset)));
        addToGraph(Phantom, Edge(result, NumberUse));
        set(VirtualRegister(resultOperand), result);
        return true;
    }
    
    if (argumentCountIncludingThis == 3) { // Math.min(x, y)
        set(VirtualRegister(resultOperand), addToGraph(op, get(virtualRegisterForArgument(1, registerOffset)), get(virtualRegisterForArgument(2, registerOffset))));
        return true;
    }
    
    // Don't handle >=3 arguments for now.
    return false;
}

bool ByteCodeParser::handleIntrinsic(int resultOperand, Intrinsic intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction)
{
    switch (intrinsic) {
    case AbsIntrinsic: {
        if (argumentCountIncludingThis == 1) { // Math.abs()
            set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }

        if (!MacroAssembler::supportsFloatingPointAbs())
            return false;

        Node* node = addToGraph(ArithAbs, get(virtualRegisterForArgument(1, registerOffset)));
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInDFG);
        set(VirtualRegister(resultOperand), node);
        return true;
    }

    case MinIntrinsic:
        return handleMinMax(resultOperand, ArithMin, registerOffset, argumentCountIncludingThis);
        
    case MaxIntrinsic:
        return handleMinMax(resultOperand, ArithMax, registerOffset, argumentCountIncludingThis);
        
    case SqrtIntrinsic:
    case CosIntrinsic:
    case SinIntrinsic: {
        if (argumentCountIncludingThis == 1) {
            set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }
        
        switch (intrinsic) {
        case SqrtIntrinsic:
            if (!MacroAssembler::supportsFloatingPointSqrt())
                return false;
            
            set(VirtualRegister(resultOperand), addToGraph(ArithSqrt, get(virtualRegisterForArgument(1, registerOffset))));
            return true;
            
        case CosIntrinsic:
            set(VirtualRegister(resultOperand), addToGraph(ArithCos, get(virtualRegisterForArgument(1, registerOffset))));
            return true;
            
        case SinIntrinsic:
            set(VirtualRegister(resultOperand), addToGraph(ArithSin, get(virtualRegisterForArgument(1, registerOffset))));
            return true;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
        
    case ArrayPushIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        ArrayMode arrayMode = getArrayMode(m_currentInstruction[OPCODE_LENGTH(op_call) - 2].u.arrayProfile);
        if (!arrayMode.isJSArray())
            return false;
        switch (arrayMode.type()) {
        case Array::Undecided:
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage: {
            Node* arrayPush = addToGraph(ArrayPush, OpInfo(arrayMode.asWord()), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)));
            set(VirtualRegister(resultOperand), arrayPush);
            
            return true;
        }
            
        default:
            return false;
        }
    }
        
    case ArrayPopIntrinsic: {
        if (argumentCountIncludingThis != 1)
            return false;
        
        ArrayMode arrayMode = getArrayMode(m_currentInstruction[OPCODE_LENGTH(op_call) - 2].u.arrayProfile);
        if (!arrayMode.isJSArray())
            return false;
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage: {
            Node* arrayPop = addToGraph(ArrayPop, OpInfo(arrayMode.asWord()), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)));
            set(VirtualRegister(resultOperand), arrayPop);
            return true;
        }
            
        default:
            return false;
        }
    }

    case CharCodeAtIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        VirtualRegister thisOperand = virtualRegisterForArgument(0, registerOffset);
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringCharCodeAt, OpInfo(ArrayMode(Array::String).asWord()), get(thisOperand), get(indexOperand));

        set(VirtualRegister(resultOperand), charCode);
        return true;
    }

    case CharAtIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        VirtualRegister thisOperand = virtualRegisterForArgument(0, registerOffset);
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringCharAt, OpInfo(ArrayMode(Array::String).asWord()), get(thisOperand), get(indexOperand));

        set(VirtualRegister(resultOperand), charCode);
        return true;
    }
    case FromCharCodeIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringFromCharCode, get(indexOperand));

        set(VirtualRegister(resultOperand), charCode);

        return true;
    }

    case RegExpExecIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        Node* regExpExec = addToGraph(RegExpExec, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)));
        set(VirtualRegister(resultOperand), regExpExec);
        
        return true;
    }
        
    case RegExpTestIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        Node* regExpExec = addToGraph(RegExpTest, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)));
        set(VirtualRegister(resultOperand), regExpExec);
        
        return true;
    }

    case IMulIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;
        VirtualRegister leftOperand = virtualRegisterForArgument(1, registerOffset);
        VirtualRegister rightOperand = virtualRegisterForArgument(2, registerOffset);
        Node* left = get(leftOperand);
        Node* right = get(rightOperand);
        set(VirtualRegister(resultOperand), addToGraph(ArithIMul, left, right));
        return true;
    }
        
    case FRoundIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        VirtualRegister operand = virtualRegisterForArgument(1, registerOffset);
        set(VirtualRegister(resultOperand), addToGraph(ArithFRound, get(operand)));
        return true;
    }
        
    case DFGTrueIntrinsic: {
        set(VirtualRegister(resultOperand), jsConstant(jsBoolean(true)));
        return true;
    }
        
    case OSRExitIntrinsic: {
        addToGraph(ForceOSRExit);
        set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantUndefined)));
        return true;
    }
        
    case IsFinalTierIntrinsic: {
        set(VirtualRegister(resultOperand),
            jsConstant(jsBoolean(Options::useFTLJIT() ? isFTL(m_graph.m_plan.mode) : true)));
        return true;
    }
        
    case SetInt32HeapPredictionIntrinsic: {
        for (int i = 1; i < argumentCountIncludingThis; ++i) {
            Node* node = get(virtualRegisterForArgument(i, registerOffset));
            if (node->hasHeapPrediction())
                node->setHeapPrediction(SpecInt32);
        }
        set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantUndefined)));
        return true;
    }
        
    case FiatInt52Intrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        VirtualRegister operand = virtualRegisterForArgument(1, registerOffset);
        if (enableInt52())
            set(VirtualRegister(resultOperand), addToGraph(FiatInt52, get(operand)));
        else
            set(VirtualRegister(resultOperand), get(operand));
        return true;
    }
        
    default:
        return false;
    }
}

bool ByteCodeParser::handleTypedArrayConstructor(
    int resultOperand, InternalFunction* function, int registerOffset,
    int argumentCountIncludingThis, TypedArrayType type)
{
    if (!isTypedView(type))
        return false;
    
    if (function->classInfo() != constructorClassInfoForType(type))
        return false;
    
    if (function->globalObject() != m_inlineStackTop->m_codeBlock->globalObject())
        return false;
    
    // We only have an intrinsic for the case where you say:
    //
    // new FooArray(blah);
    //
    // Of course, 'blah' could be any of the following:
    //
    // - Integer, indicating that you want to allocate an array of that length.
    //   This is the thing we're hoping for, and what we can actually do meaningful
    //   optimizations for.
    //
    // - Array buffer, indicating that you want to create a view onto that _entire_
    //   buffer.
    //
    // - Non-buffer object, indicating that you want to create a copy of that
    //   object by pretending that it quacks like an array.
    //
    // - Anything else, indicating that you want to have an exception thrown at
    //   you.
    //
    // The intrinsic, NewTypedArray, will behave as if it could do any of these
    // things up until we do Fixup. Thereafter, if child1 (i.e. 'blah') is
    // predicted Int32, then we lock it in as a normal typed array allocation.
    // Otherwise, NewTypedArray turns into a totally opaque function call that
    // may clobber the world - by virtue of it accessing properties on what could
    // be an object.
    //
    // Note that although the generic form of NewTypedArray sounds sort of awful,
    // it is actually quite likely to be more efficient than a fully generic
    // Construct. So, we might want to think about making NewTypedArray variadic,
    // or else making Construct not super slow.
    
    if (argumentCountIncludingThis != 2)
        return false;
    
    set(VirtualRegister(resultOperand),
        addToGraph(NewTypedArray, OpInfo(type), get(virtualRegisterForArgument(1, registerOffset))));
    return true;
}

bool ByteCodeParser::handleConstantInternalFunction(
    int resultOperand, InternalFunction* function, int registerOffset,
    int argumentCountIncludingThis, CodeSpecializationKind kind)
{
    // If we ever find that we have a lot of internal functions that we specialize for,
    // then we should probably have some sort of hashtable dispatch, or maybe even
    // dispatch straight through the MethodTable of the InternalFunction. But for now,
    // it seems that this case is hit infrequently enough, and the number of functions
    // we know about is small enough, that having just a linear cascade of if statements
    // is good enough.
    
    if (function->classInfo() == ArrayConstructor::info()) {
        if (function->globalObject() != m_inlineStackTop->m_codeBlock->globalObject())
            return false;
        
        if (argumentCountIncludingThis == 2) {
            set(VirtualRegister(resultOperand),
                addToGraph(NewArrayWithSize, OpInfo(ArrayWithUndecided), get(virtualRegisterForArgument(1, registerOffset))));
            return true;
        }
        
        for (int i = 1; i < argumentCountIncludingThis; ++i)
            addVarArgChild(get(virtualRegisterForArgument(i, registerOffset)));
        set(VirtualRegister(resultOperand),
            addToGraph(Node::VarArg, NewArray, OpInfo(ArrayWithUndecided), OpInfo(0)));
        return true;
    }
    
    if (function->classInfo() == StringConstructor::info()) {
        Node* result;
        
        if (argumentCountIncludingThis <= 1)
            result = jsConstant(m_vm->smallStrings.emptyString());
        else
            result = addToGraph(ToString, get(virtualRegisterForArgument(1, registerOffset)));
        
        if (kind == CodeForConstruct)
            result = addToGraph(NewStringObject, OpInfo(function->globalObject()->stringObjectStructure()), result);
        
        set(VirtualRegister(resultOperand), result);
        return true;
    }
    
    for (unsigned typeIndex = 0; typeIndex < NUMBER_OF_TYPED_ARRAY_TYPES; ++typeIndex) {
        bool result = handleTypedArrayConstructor(
            resultOperand, function, registerOffset, argumentCountIncludingThis,
            indexToTypedArrayType(typeIndex));
        if (result)
            return true;
    }
    
    return false;
}

Node* ByteCodeParser::handleGetByOffset(SpeculatedType prediction, Node* base, const StructureSet& structureSet, unsigned identifierNumber, PropertyOffset offset, NodeType op)
{
    if (base->hasConstant()) {
        if (JSValue constant = m_graph.tryGetConstantProperty(base->asJSValue(), structureSet, offset)) {
            addToGraph(Phantom, base);
            return weakJSConstant(constant);
        }
    }
    
    Node* propertyStorage;
    if (isInlineOffset(offset))
        propertyStorage = base;
    else
        propertyStorage = addToGraph(GetButterfly, base);
    Node* getByOffset = addToGraph(op, OpInfo(m_graph.m_storageAccessData.size()), OpInfo(prediction), propertyStorage, base);

    StorageAccessData storageAccessData;
    storageAccessData.offset = offset;
    storageAccessData.identifierNumber = identifierNumber;
    m_graph.m_storageAccessData.append(storageAccessData);

    return getByOffset;
}

Node* ByteCodeParser::handlePutByOffset(Node* base, unsigned identifier, PropertyOffset offset, Node* value)
{
    Node* propertyStorage;
    if (isInlineOffset(offset))
        propertyStorage = base;
    else
        propertyStorage = addToGraph(GetButterfly, base);
    Node* result = addToGraph(PutByOffset, OpInfo(m_graph.m_storageAccessData.size()), propertyStorage, base, value);
    
    StorageAccessData storageAccessData;
    storageAccessData.offset = offset;
    storageAccessData.identifierNumber = identifier;
    m_graph.m_storageAccessData.append(storageAccessData);

    return result;
}

void ByteCodeParser::emitChecks(const ConstantStructureCheckVector& vector)
{
    for (unsigned i = 0; i < vector.size(); ++i)
        cellConstantWithStructureCheck(vector[i].constant(), vector[i].structure());
}

void ByteCodeParser::handleGetById(
    int destinationOperand, SpeculatedType prediction, Node* base, unsigned identifierNumber,
    const GetByIdStatus& getByIdStatus)
{
    NodeType getById = getByIdStatus.makesCalls() ? GetByIdFlush : GetById;
    
    if (!getByIdStatus.isSimple() || !Options::enableAccessInlining()) {
        set(VirtualRegister(destinationOperand),
            addToGraph(getById, OpInfo(identifierNumber), OpInfo(prediction), base));
        return;
    }
    
    if (getByIdStatus.numVariants() > 1) {
        if (getByIdStatus.makesCalls() || !isFTL(m_graph.m_plan.mode)
            || !Options::enablePolymorphicAccessInlining()) {
            set(VirtualRegister(destinationOperand),
                addToGraph(getById, OpInfo(identifierNumber), OpInfo(prediction), base));
            return;
        }
        
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedGetById();
    
        // 1) Emit prototype structure checks for all chains. This could sort of maybe not be
        //    optimal, if there is some rarely executed case in the chain that requires a lot
        //    of checks and those checks are not watchpointable.
        for (unsigned variantIndex = getByIdStatus.numVariants(); variantIndex--;)
            emitChecks(getByIdStatus[variantIndex].constantChecks());
        
        // 2) Emit a MultiGetByOffset
        MultiGetByOffsetData* data = m_graph.m_multiGetByOffsetData.add();
        data->variants = getByIdStatus.variants();
        data->identifierNumber = identifierNumber;
        set(VirtualRegister(destinationOperand),
            addToGraph(MultiGetByOffset, OpInfo(data), OpInfo(prediction), base));
        return;
    }
    
    ASSERT(getByIdStatus.numVariants() == 1);
    GetByIdVariant variant = getByIdStatus[0];
                
    if (m_graph.compilation())
        m_graph.compilation()->noticeInlinedGetById();
    
    Node* originalBase = base;
                
    addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.structureSet())), base);
    
    emitChecks(variant.constantChecks());

    if (variant.alternateBase())
        base = weakJSConstant(variant.alternateBase());
    
    // Unless we want bugs like https://bugs.webkit.org/show_bug.cgi?id=88783, we need to
    // ensure that the base of the original get_by_id is kept alive until we're done with
    // all of the speculations. We only insert the Phantom if there had been a CheckStructure
    // on something other than the base following the CheckStructure on base.
    if (originalBase != base)
        addToGraph(Phantom, originalBase);
    
    Node* loadedValue = handleGetByOffset(
        variant.callLinkStatus() ? SpecCellOther : prediction,
        base, variant.baseStructure(), identifierNumber, variant.offset(),
        variant.callLinkStatus() ? GetGetterSetterByOffset : GetByOffset);
    
    if (!variant.callLinkStatus()) {
        set(VirtualRegister(destinationOperand), loadedValue);
        return;
    }
    
    Node* getter = addToGraph(GetGetter, loadedValue);
    
    // Make a call. We don't try to get fancy with using the smallest operand number because
    // the stack layout phase should compress the stack anyway.
    
    unsigned numberOfParameters = 0;
    numberOfParameters++; // The 'this' argument.
    numberOfParameters++; // True return PC.
    
    // Start with a register offset that corresponds to the last in-use register.
    int registerOffset = virtualRegisterForLocal(
        m_inlineStackTop->m_profiledBlock->m_numCalleeRegisters - 1).offset();
    registerOffset -= numberOfParameters;
    registerOffset -= JSStack::CallFrameHeaderSize;
    
    // Get the alignment right.
    registerOffset = -WTF::roundUpToMultipleOf(
        stackAlignmentRegisters(),
        -registerOffset);
    
    ensureLocals(
        m_inlineStackTop->remapOperand(
            VirtualRegister(registerOffset)).toLocal());
    
    // Issue SetLocals. This has two effects:
    // 1) That's how handleCall() sees the arguments.
    // 2) If we inline then this ensures that the arguments are flushed so that if you use
    //    the dreaded arguments object on the getter, the right things happen. Well, sort of -
    //    since we only really care about 'this' in this case. But we're not going to take that
    //    shortcut.
    int nextRegister = registerOffset + JSStack::CallFrameHeaderSize;
    set(VirtualRegister(nextRegister++), originalBase, ImmediateNakedSet);
    
    handleCall(
        destinationOperand, Call, InlineCallFrame::GetterCall, OPCODE_LENGTH(op_get_by_id),
        getter, numberOfParameters - 1, registerOffset, *variant.callLinkStatus(), prediction);
}

void ByteCodeParser::emitPutById(
    Node* base, unsigned identifierNumber, Node* value, const PutByIdStatus& putByIdStatus, bool isDirect)
{
    if (isDirect)
        addToGraph(PutByIdDirect, OpInfo(identifierNumber), base, value);
    else
        addToGraph(putByIdStatus.makesCalls() ? PutByIdFlush : PutById, OpInfo(identifierNumber), base, value);
}

void ByteCodeParser::handlePutById(
    Node* base, unsigned identifierNumber, Node* value,
    const PutByIdStatus& putByIdStatus, bool isDirect)
{
    if (!putByIdStatus.isSimple() || !Options::enableAccessInlining()) {
        if (!putByIdStatus.isSet())
            addToGraph(ForceOSRExit);
        emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
        return;
    }
    
    if (putByIdStatus.numVariants() > 1) {
        if (!isFTL(m_graph.m_plan.mode) || putByIdStatus.makesCalls()
            || !Options::enablePolymorphicAccessInlining()) {
            emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
            return;
        }
        
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedPutById();
        
        if (!isDirect) {
            for (unsigned variantIndex = putByIdStatus.numVariants(); variantIndex--;) {
                if (putByIdStatus[variantIndex].kind() != PutByIdVariant::Transition)
                    continue;
                emitChecks(putByIdStatus[variantIndex].constantChecks());
            }
        }
        
        MultiPutByOffsetData* data = m_graph.m_multiPutByOffsetData.add();
        data->variants = putByIdStatus.variants();
        data->identifierNumber = identifierNumber;
        addToGraph(MultiPutByOffset, OpInfo(data), base, value);
        return;
    }
    
    ASSERT(putByIdStatus.numVariants() == 1);
    const PutByIdVariant& variant = putByIdStatus[0];
    
    switch (variant.kind()) {
    case PutByIdVariant::Replace: {
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.structure())), base);
        handlePutByOffset(base, identifierNumber, variant.offset(), value);
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedPutById();
        return;
    }
    
    case PutByIdVariant::Transition: {
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.oldStructure())), base);
        emitChecks(variant.constantChecks());

        ASSERT(variant.oldStructureForTransition()->transitionWatchpointSetHasBeenInvalidated());
    
        Node* propertyStorage;
        Transition* transition = m_graph.m_transitions.add(
            variant.oldStructureForTransition(), variant.newStructure());

        if (variant.reallocatesStorage()) {

            // If we're growing the property storage then it must be because we're
            // storing into the out-of-line storage.
            ASSERT(!isInlineOffset(variant.offset()));

            if (!variant.oldStructureForTransition()->outOfLineCapacity()) {
                propertyStorage = addToGraph(
                    AllocatePropertyStorage, OpInfo(transition), base);
            } else {
                propertyStorage = addToGraph(
                    ReallocatePropertyStorage, OpInfo(transition),
                    base, addToGraph(GetButterfly, base));
            }
        } else {
            if (isInlineOffset(variant.offset()))
                propertyStorage = base;
            else
                propertyStorage = addToGraph(GetButterfly, base);
        }

        addToGraph(PutStructure, OpInfo(transition), base);

        addToGraph(
            PutByOffset,
            OpInfo(m_graph.m_storageAccessData.size()),
            propertyStorage,
            base,
            value);

        StorageAccessData storageAccessData;
        storageAccessData.offset = variant.offset();
        storageAccessData.identifierNumber = identifierNumber;
        m_graph.m_storageAccessData.append(storageAccessData);

        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedPutById();
        return;
    }
        
    case PutByIdVariant::Setter: {
        Node* originalBase = base;
        
        addToGraph(
            CheckStructure, OpInfo(m_graph.addStructureSet(variant.structure())), base);
        
        emitChecks(variant.constantChecks());
        
        if (variant.alternateBase())
            base = weakJSConstant(variant.alternateBase());
        
        Node* loadedValue = handleGetByOffset(
            SpecCellOther, base, variant.baseStructure(), identifierNumber, variant.offset(),
            GetGetterSetterByOffset);
        
        Node* setter = addToGraph(GetSetter, loadedValue);
        
        // Make a call. We don't try to get fancy with using the smallest operand number because
        // the stack layout phase should compress the stack anyway.
    
        unsigned numberOfParameters = 0;
        numberOfParameters++; // The 'this' argument.
        numberOfParameters++; // The new value.
        numberOfParameters++; // True return PC.
    
        // Start with a register offset that corresponds to the last in-use register.
        int registerOffset = virtualRegisterForLocal(
            m_inlineStackTop->m_profiledBlock->m_numCalleeRegisters - 1).offset();
        registerOffset -= numberOfParameters;
        registerOffset -= JSStack::CallFrameHeaderSize;
    
        // Get the alignment right.
        registerOffset = -WTF::roundUpToMultipleOf(
            stackAlignmentRegisters(),
            -registerOffset);
    
        ensureLocals(
            m_inlineStackTop->remapOperand(
                VirtualRegister(registerOffset)).toLocal());
    
        int nextRegister = registerOffset + JSStack::CallFrameHeaderSize;
        set(VirtualRegister(nextRegister++), originalBase, ImmediateNakedSet);
        set(VirtualRegister(nextRegister++), value, ImmediateNakedSet);
    
        handleCall(
            VirtualRegister().offset(), Call, InlineCallFrame::SetterCall,
            OPCODE_LENGTH(op_put_by_id), setter, numberOfParameters - 1, registerOffset,
            *variant.callLinkStatus(), SpecOther);
        return;
    }
    
    default: {
        emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
        return;
    } }
}

void ByteCodeParser::prepareToParseBlock()
{
    clearCaches();
    ASSERT(m_setLocalQueue.isEmpty());
}

void ByteCodeParser::clearCaches()
{
    m_constants.resize(0);
}

Node* ByteCodeParser::getScope(unsigned skipCount)
{
    Node* localBase = get(VirtualRegister(JSStack::ScopeChain));
    for (unsigned n = skipCount; n--;)
        localBase = addToGraph(SkipScope, localBase);
    return localBase;
}

bool ByteCodeParser::parseBlock(unsigned limit)
{
    bool shouldContinueParsing = true;

    Interpreter* interpreter = m_vm->interpreter;
    Instruction* instructionsBegin = m_inlineStackTop->m_codeBlock->instructions().begin();
    unsigned blockBegin = m_currentIndex;
    
    // If we are the first basic block, introduce markers for arguments. This allows
    // us to track if a use of an argument may use the actual argument passed, as
    // opposed to using a value we set explicitly.
    if (m_currentBlock == m_graph.block(0) && !inlineCallFrame()) {
        m_graph.m_arguments.resize(m_numArguments);
        for (unsigned argument = 0; argument < m_numArguments; ++argument) {
            VariableAccessData* variable = newVariableAccessData(
                virtualRegisterForArgument(argument), m_codeBlock->isCaptured(virtualRegisterForArgument(argument)));
            variable->mergeStructureCheckHoistingFailed(
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache));
            variable->mergeCheckArrayHoistingFailed(
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType));
            
            Node* setArgument = addToGraph(SetArgument, OpInfo(variable));
            m_graph.m_arguments[argument] = setArgument;
            m_currentBlock->variablesAtTail.setArgumentFirstTime(argument, setArgument);
        }
    }

    while (true) {
        processSetLocalQueue();
        
        // Don't extend over jump destinations.
        if (m_currentIndex == limit) {
            // Ordinarily we want to plant a jump. But refuse to do this if the block is
            // empty. This is a special case for inlining, which might otherwise create
            // some empty blocks in some cases. When parseBlock() returns with an empty
            // block, it will get repurposed instead of creating a new one. Note that this
            // logic relies on every bytecode resulting in one or more nodes, which would
            // be true anyway except for op_loop_hint, which emits a Phantom to force this
            // to be true.
            if (!m_currentBlock->isEmpty())
                addToGraph(Jump, OpInfo(m_currentIndex));
            return shouldContinueParsing;
        }
        
        // Switch on the current bytecode opcode.
        Instruction* currentInstruction = instructionsBegin + m_currentIndex;
        m_currentInstruction = currentInstruction; // Some methods want to use this, and we'd rather not thread it through calls.
        OpcodeID opcodeID = interpreter->getOpcodeID(currentInstruction->u.opcode);
        
        if (Options::verboseDFGByteCodeParsing())
            dataLog("    parsing ", currentCodeOrigin(), "\n");
        
        if (m_graph.compilation()) {
            addToGraph(CountExecution, OpInfo(m_graph.compilation()->executionCounterFor(
                Profiler::OriginStack(*m_vm->m_perBytecodeProfiler, m_codeBlock, currentCodeOrigin()))));
        }
        
        switch (opcodeID) {

        // === Function entry opcodes ===

        case op_enter: {
            Node* undefined = addToGraph(JSConstant, OpInfo(m_constantUndefined));
            // Initialize all locals to undefined.
            for (int i = 0; i < m_inlineStackTop->m_codeBlock->m_numVars; ++i)
                set(virtualRegisterForLocal(i), undefined, ImmediateNakedSet);
            if (m_inlineStackTop->m_codeBlock->specializationKind() == CodeForConstruct)
                set(virtualRegisterForArgument(0), undefined, ImmediateNakedSet);
            NEXT_OPCODE(op_enter);
        }
            
        case op_touch_entry:
            if (m_inlineStackTop->m_codeBlock->symbolTable()->m_functionEnteredOnce.isStillValid())
                addToGraph(ForceOSRExit);
            NEXT_OPCODE(op_touch_entry);
            
        case op_to_this: {
            Node* op1 = getThis();
            if (op1->op() != ToThis) {
                Structure* cachedStructure = currentInstruction[2].u.structure.get();
                if (currentInstruction[2].u.toThisStatus != ToThisOK
                    || !cachedStructure
                    || cachedStructure->classInfo()->methodTable.toThis != JSObject::info()->methodTable.toThis
                    || m_inlineStackTop->m_profiledBlock->couldTakeSlowCase(m_currentIndex)
                    || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
                    || (op1->op() == GetLocal && op1->variableAccessData()->structureCheckHoistingFailed())) {
                    setThis(addToGraph(ToThis, op1));
                } else {
                    addToGraph(
                        CheckStructure,
                        OpInfo(m_graph.addStructureSet(cachedStructure)),
                        op1);
                }
            }
            NEXT_OPCODE(op_to_this);
        }

        case op_create_this: {
            int calleeOperand = currentInstruction[2].u.operand;
            Node* callee = get(VirtualRegister(calleeOperand));
            bool alreadyEmitted = false;
            if (JSFunction* function = callee->dynamicCastConstant<JSFunction*>()) {
                if (Structure* structure = function->allocationStructure()) {
                    addToGraph(AllocationProfileWatchpoint, OpInfo(m_graph.freeze(function)));
                    // The callee is still live up to this point.
                    addToGraph(Phantom, callee);
                    set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(NewObject, OpInfo(structure)));
                    alreadyEmitted = true;
                }
            }
            if (!alreadyEmitted) {
                set(VirtualRegister(currentInstruction[1].u.operand),
                    addToGraph(CreateThis, OpInfo(currentInstruction[3].u.operand), callee));
            }
            NEXT_OPCODE(op_create_this);
        }

        case op_new_object: {
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(NewObject,
                    OpInfo(currentInstruction[3].u.objectAllocationProfile->structure())));
            NEXT_OPCODE(op_new_object);
        }
            
        case op_new_array: {
            int startOperand = currentInstruction[2].u.operand;
            int numOperands = currentInstruction[3].u.operand;
            ArrayAllocationProfile* profile = currentInstruction[4].u.arrayAllocationProfile;
            for (int operandIdx = startOperand; operandIdx > startOperand - numOperands; --operandIdx)
                addVarArgChild(get(VirtualRegister(operandIdx)));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(Node::VarArg, NewArray, OpInfo(profile->selectIndexingType()), OpInfo(0)));
            NEXT_OPCODE(op_new_array);
        }
            
        case op_new_array_with_size: {
            int lengthOperand = currentInstruction[2].u.operand;
            ArrayAllocationProfile* profile = currentInstruction[3].u.arrayAllocationProfile;
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(NewArrayWithSize, OpInfo(profile->selectIndexingType()), get(VirtualRegister(lengthOperand))));
            NEXT_OPCODE(op_new_array_with_size);
        }
            
        case op_new_array_buffer: {
            int startConstant = currentInstruction[2].u.operand;
            int numConstants = currentInstruction[3].u.operand;
            ArrayAllocationProfile* profile = currentInstruction[4].u.arrayAllocationProfile;
            NewArrayBufferData data;
            data.startConstant = m_inlineStackTop->m_constantBufferRemap[startConstant];
            data.numConstants = numConstants;
            data.indexingType = profile->selectIndexingType();

            // If this statement has never executed, we'll have the wrong indexing type in the profile.
            for (int i = 0; i < numConstants; ++i) {
                data.indexingType =
                    leastUpperBoundOfIndexingTypeAndValue(
                        data.indexingType,
                        m_codeBlock->constantBuffer(data.startConstant)[i]);
            }
            
            m_graph.m_newArrayBufferData.append(data);
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(NewArrayBuffer, OpInfo(&m_graph.m_newArrayBufferData.last())));
            NEXT_OPCODE(op_new_array_buffer);
        }
            
        case op_new_regexp: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(NewRegexp, OpInfo(currentInstruction[2].u.operand)));
            NEXT_OPCODE(op_new_regexp);
        }
            
        case op_get_callee: {
            JSCell* cachedFunction = currentInstruction[2].u.jsCell.get();
            if (!cachedFunction 
                || m_inlineStackTop->m_profiledBlock->couldTakeSlowCase(m_currentIndex)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell)) {
                set(VirtualRegister(currentInstruction[1].u.operand), get(VirtualRegister(JSStack::Callee)));
            } else {
                FrozenValue* frozen = m_graph.freeze(cachedFunction);
                ASSERT(cachedFunction->inherits(JSFunction::info()));
                Node* actualCallee = get(VirtualRegister(JSStack::Callee));
                addToGraph(CheckCell, OpInfo(frozen), actualCallee);
                set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(JSConstant, OpInfo(frozen)));
            }
            NEXT_OPCODE(op_get_callee);
        }

        // === Bitwise operations ===

        case op_bitand: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(BitAnd, op1, op2));
            NEXT_OPCODE(op_bitand);
        }

        case op_bitor: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(BitOr, op1, op2));
            NEXT_OPCODE(op_bitor);
        }

        case op_bitxor: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(BitXor, op1, op2));
            NEXT_OPCODE(op_bitxor);
        }

        case op_rshift: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(BitRShift, op1, op2));
            NEXT_OPCODE(op_rshift);
        }

        case op_lshift: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(BitLShift, op1, op2));
            NEXT_OPCODE(op_lshift);
        }

        case op_urshift: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(BitURShift, op1, op2));
            NEXT_OPCODE(op_urshift);
        }
            
        case op_unsigned: {
            set(VirtualRegister(currentInstruction[1].u.operand),
                makeSafe(addToGraph(UInt32ToNumber, get(VirtualRegister(currentInstruction[2].u.operand)))));
            NEXT_OPCODE(op_unsigned);
        }

        // === Increment/Decrement opcodes ===

        case op_inc: {
            int srcDst = currentInstruction[1].u.operand;
            VirtualRegister srcDstVirtualRegister = VirtualRegister(srcDst);
            Node* op = get(srcDstVirtualRegister);
            set(srcDstVirtualRegister, makeSafe(addToGraph(ArithAdd, op, addToGraph(JSConstant, OpInfo(m_constantOne)))));
            NEXT_OPCODE(op_inc);
        }

        case op_dec: {
            int srcDst = currentInstruction[1].u.operand;
            VirtualRegister srcDstVirtualRegister = VirtualRegister(srcDst);
            Node* op = get(srcDstVirtualRegister);
            set(srcDstVirtualRegister, makeSafe(addToGraph(ArithSub, op, addToGraph(JSConstant, OpInfo(m_constantOne)))));
            NEXT_OPCODE(op_dec);
        }

        // === Arithmetic operations ===

        case op_add: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(VirtualRegister(currentInstruction[1].u.operand), makeSafe(addToGraph(ArithAdd, op1, op2)));
            else
                set(VirtualRegister(currentInstruction[1].u.operand), makeSafe(addToGraph(ValueAdd, op1, op2)));
            NEXT_OPCODE(op_add);
        }

        case op_sub: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), makeSafe(addToGraph(ArithSub, op1, op2)));
            NEXT_OPCODE(op_sub);
        }

        case op_negate: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), makeSafe(addToGraph(ArithNegate, op1)));
            NEXT_OPCODE(op_negate);
        }

        case op_mul: {
            // Multiply requires that the inputs are not truncated, unfortunately.
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), makeSafe(addToGraph(ArithMul, op1, op2)));
            NEXT_OPCODE(op_mul);
        }

        case op_mod: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), makeSafe(addToGraph(ArithMod, op1, op2)));
            NEXT_OPCODE(op_mod);
        }

        case op_div: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), makeDivSafe(addToGraph(ArithDiv, op1, op2)));
            NEXT_OPCODE(op_div);
        }

        // === Misc operations ===

        case op_debug:
            addToGraph(Breakpoint);
            NEXT_OPCODE(op_debug);

        case op_profile_will_call: {
            addToGraph(ProfileWillCall);
            NEXT_OPCODE(op_profile_will_call);
        }

        case op_profile_did_call: {
            addToGraph(ProfileDidCall);
            NEXT_OPCODE(op_profile_did_call);
        }

        case op_mov: {
            Node* op = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), op);
            NEXT_OPCODE(op_mov);
        }
            
        case op_captured_mov: {
            Node* op = get(VirtualRegister(currentInstruction[2].u.operand));
            if (VariableWatchpointSet* set = currentInstruction[3].u.watchpointSet) {
                if (set->state() != IsInvalidated)
                    addToGraph(NotifyWrite, OpInfo(set), op);
            }
            set(VirtualRegister(currentInstruction[1].u.operand), op);
            NEXT_OPCODE(op_captured_mov);
        }

        case op_check_has_instance:
            addToGraph(CheckHasInstance, get(VirtualRegister(currentInstruction[3].u.operand)));
            NEXT_OPCODE(op_check_has_instance);

        case op_instanceof: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* prototype = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(InstanceOf, value, prototype));
            NEXT_OPCODE(op_instanceof);
        }
            
        case op_is_undefined: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(IsUndefined, value));
            NEXT_OPCODE(op_is_undefined);
        }

        case op_is_boolean: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(IsBoolean, value));
            NEXT_OPCODE(op_is_boolean);
        }

        case op_is_number: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(IsNumber, value));
            NEXT_OPCODE(op_is_number);
        }

        case op_is_string: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(IsString, value));
            NEXT_OPCODE(op_is_string);
        }

        case op_is_object: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(IsObject, value));
            NEXT_OPCODE(op_is_object);
        }

        case op_is_function: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(IsFunction, value));
            NEXT_OPCODE(op_is_function);
        }

        case op_not: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(LogicalNot, value));
            NEXT_OPCODE(op_not);
        }
            
        case op_to_primitive: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(ToPrimitive, value));
            NEXT_OPCODE(op_to_primitive);
        }
            
        case op_strcat: {
            int startOperand = currentInstruction[2].u.operand;
            int numOperands = currentInstruction[3].u.operand;
#if CPU(X86)
            // X86 doesn't have enough registers to compile MakeRope with three arguments.
            // Rather than try to be clever, we just make MakeRope dumber on this processor.
            const unsigned maxRopeArguments = 2;
#else
            const unsigned maxRopeArguments = 3;
#endif
            auto toStringNodes = std::make_unique<Node*[]>(numOperands);
            for (int i = 0; i < numOperands; i++)
                toStringNodes[i] = addToGraph(ToString, get(VirtualRegister(startOperand - i)));

            for (int i = 0; i < numOperands; i++)
                addToGraph(Phantom, toStringNodes[i]);

            Node* operands[AdjacencyList::Size];
            unsigned indexInOperands = 0;
            for (unsigned i = 0; i < AdjacencyList::Size; ++i)
                operands[i] = 0;
            for (int operandIdx = 0; operandIdx < numOperands; ++operandIdx) {
                if (indexInOperands == maxRopeArguments) {
                    operands[0] = addToGraph(MakeRope, operands[0], operands[1], operands[2]);
                    for (unsigned i = 1; i < AdjacencyList::Size; ++i)
                        operands[i] = 0;
                    indexInOperands = 1;
                }
                
                ASSERT(indexInOperands < AdjacencyList::Size);
                ASSERT(indexInOperands < maxRopeArguments);
                operands[indexInOperands++] = toStringNodes[operandIdx];
            }
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(MakeRope, operands[0], operands[1], operands[2]));
            NEXT_OPCODE(op_strcat);
        }

        case op_less: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareLess, op1, op2));
            NEXT_OPCODE(op_less);
        }

        case op_lesseq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareLessEq, op1, op2));
            NEXT_OPCODE(op_lesseq);
        }

        case op_greater: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareGreater, op1, op2));
            NEXT_OPCODE(op_greater);
        }

        case op_greatereq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareGreaterEq, op1, op2));
            NEXT_OPCODE(op_greatereq);
        }

        case op_eq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareEq, op1, op2));
            NEXT_OPCODE(op_eq);
        }

        case op_eq_null: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareEqConstant, value, addToGraph(JSConstant, OpInfo(m_constantNull))));
            NEXT_OPCODE(op_eq_null);
        }

        case op_stricteq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareStrictEq, op1, op2));
            NEXT_OPCODE(op_stricteq);
        }

        case op_neq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(LogicalNot, addToGraph(CompareEq, op1, op2)));
            NEXT_OPCODE(op_neq);
        }

        case op_neq_null: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(LogicalNot, addToGraph(CompareEqConstant, value, addToGraph(JSConstant, OpInfo(m_constantNull)))));
            NEXT_OPCODE(op_neq_null);
        }

        case op_nstricteq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            Node* invertedResult;
            invertedResult = addToGraph(CompareStrictEq, op1, op2);
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(LogicalNot, invertedResult));
            NEXT_OPCODE(op_nstricteq);
        }

        // === Property access operations ===

        case op_get_by_val: {
            SpeculatedType prediction = getPredictionWithoutOSRExit();
            
            Node* base = get(VirtualRegister(currentInstruction[2].u.operand));
            ArrayMode arrayMode = getArrayModeConsideringSlowPath(currentInstruction[4].u.arrayProfile, Array::Read);
            Node* property = get(VirtualRegister(currentInstruction[3].u.operand));
            Node* getByVal = addToGraph(GetByVal, OpInfo(arrayMode.asWord()), OpInfo(prediction), base, property);
            set(VirtualRegister(currentInstruction[1].u.operand), getByVal);

            NEXT_OPCODE(op_get_by_val);
        }

        case op_put_by_val_direct:
        case op_put_by_val: {
            Node* base = get(VirtualRegister(currentInstruction[1].u.operand));

            ArrayMode arrayMode = getArrayModeConsideringSlowPath(currentInstruction[4].u.arrayProfile, Array::Write);
            
            Node* property = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* value = get(VirtualRegister(currentInstruction[3].u.operand));
            
            addVarArgChild(base);
            addVarArgChild(property);
            addVarArgChild(value);
            addVarArgChild(0); // Leave room for property storage.
            addVarArgChild(0); // Leave room for length.
            addToGraph(Node::VarArg, opcodeID == op_put_by_val_direct ? PutByValDirect : PutByVal, OpInfo(arrayMode.asWord()), OpInfo(0));

            NEXT_OPCODE(op_put_by_val);
        }
            
        case op_get_by_id:
        case op_get_by_id_out_of_line:
        case op_get_array_length: {
            SpeculatedType prediction = getPrediction();
            
            Node* base = get(VirtualRegister(currentInstruction[2].u.operand));
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[3].u.operand];
            
            StringImpl* uid = m_graph.identifiers()[identifierNumber];
            GetByIdStatus getByIdStatus = GetByIdStatus::computeFor(
                m_inlineStackTop->m_profiledBlock, m_dfgCodeBlock,
                m_inlineStackTop->m_stubInfos, m_dfgStubInfos,
                currentCodeOrigin(), uid);
            
            handleGetById(
                currentInstruction[1].u.operand, prediction, base, identifierNumber, getByIdStatus);

            NEXT_OPCODE(op_get_by_id);
        }
        case op_put_by_id:
        case op_put_by_id_out_of_line:
        case op_put_by_id_transition_direct:
        case op_put_by_id_transition_normal:
        case op_put_by_id_transition_direct_out_of_line:
        case op_put_by_id_transition_normal_out_of_line: {
            Node* value = get(VirtualRegister(currentInstruction[3].u.operand));
            Node* base = get(VirtualRegister(currentInstruction[1].u.operand));
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[2].u.operand];
            bool direct = currentInstruction[8].u.operand;

            PutByIdStatus putByIdStatus = PutByIdStatus::computeFor(
                m_inlineStackTop->m_profiledBlock, m_dfgCodeBlock,
                m_inlineStackTop->m_stubInfos, m_dfgStubInfos,
                currentCodeOrigin(), m_graph.identifiers()[identifierNumber]);
            
            handlePutById(base, identifierNumber, value, putByIdStatus, direct);
            NEXT_OPCODE(op_put_by_id);
        }

        case op_init_global_const_nop: {
            NEXT_OPCODE(op_init_global_const_nop);
        }

        case op_init_global_const: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            addToGraph(
                PutGlobalVar,
                OpInfo(m_inlineStackTop->m_codeBlock->globalObject()->assertRegisterIsInThisObject(currentInstruction[1].u.registerPointer)),
                value);
            NEXT_OPCODE(op_init_global_const);
        }

        // === Block terminators. ===

        case op_jmp: {
            int relativeOffset = currentInstruction[1].u.operand;
            if (relativeOffset <= 0)
                flushForTerminal();
            addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
            LAST_OPCODE(op_jmp);
        }

        case op_jtrue: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* condition = get(VirtualRegister(currentInstruction[1].u.operand));
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + OPCODE_LENGTH(op_jtrue))), condition);
            LAST_OPCODE(op_jtrue);
        }

        case op_jfalse: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* condition = get(VirtualRegister(currentInstruction[1].u.operand));
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + OPCODE_LENGTH(op_jfalse), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jfalse);
        }

        case op_jeq_null: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* value = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* condition = addToGraph(CompareEqConstant, value, addToGraph(JSConstant, OpInfo(m_constantNull)));
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + OPCODE_LENGTH(op_jeq_null))), condition);
            LAST_OPCODE(op_jeq_null);
        }

        case op_jneq_null: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* value = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* condition = addToGraph(CompareEqConstant, value, addToGraph(JSConstant, OpInfo(m_constantNull)));
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + OPCODE_LENGTH(op_jneq_null), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jneq_null);
        }

        case op_jless: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + OPCODE_LENGTH(op_jless))), condition);
            LAST_OPCODE(op_jless);
        }

        case op_jlesseq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + OPCODE_LENGTH(op_jlesseq))), condition);
            LAST_OPCODE(op_jlesseq);
        }

        case op_jgreater: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + OPCODE_LENGTH(op_jgreater))), condition);
            LAST_OPCODE(op_jgreater);
        }

        case op_jgreatereq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + OPCODE_LENGTH(op_jgreatereq))), condition);
            LAST_OPCODE(op_jgreatereq);
        }

        case op_jnless: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + OPCODE_LENGTH(op_jnless), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jnless);
        }

        case op_jnlesseq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + OPCODE_LENGTH(op_jnlesseq), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jnlesseq);
        }

        case op_jngreater: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + OPCODE_LENGTH(op_jngreater), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jngreater);
        }

        case op_jngreatereq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + OPCODE_LENGTH(op_jngreatereq), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jngreatereq);
        }
            
        case op_switch_imm: {
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchImm;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[currentInstruction[1].u.operand];
            data.fallThrough.setBytecodeIndex(m_currentIndex + currentInstruction[2].u.operand);
            SimpleJumpTable& table = m_codeBlock->switchJumpTable(data.switchTableIndex);
            for (unsigned i = 0; i < table.branchOffsets.size(); ++i) {
                if (!table.branchOffsets[i])
                    continue;
                unsigned target = m_currentIndex + table.branchOffsets[i];
                if (target == data.fallThrough.bytecodeIndex())
                    continue;
                data.cases.append(SwitchCase::withBytecodeIndex(m_graph.freeze(jsNumber(static_cast<int32_t>(table.min + i))), target));
            }
            flushIfTerminal(data);
            addToGraph(Switch, OpInfo(&data), get(VirtualRegister(currentInstruction[3].u.operand)));
            LAST_OPCODE(op_switch_imm);
        }
            
        case op_switch_char: {
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchChar;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[currentInstruction[1].u.operand];
            data.fallThrough.setBytecodeIndex(m_currentIndex + currentInstruction[2].u.operand);
            SimpleJumpTable& table = m_codeBlock->switchJumpTable(data.switchTableIndex);
            for (unsigned i = 0; i < table.branchOffsets.size(); ++i) {
                if (!table.branchOffsets[i])
                    continue;
                unsigned target = m_currentIndex + table.branchOffsets[i];
                if (target == data.fallThrough.bytecodeIndex())
                    continue;
                data.cases.append(
                    SwitchCase::withBytecodeIndex(LazyJSValue::singleCharacterString(table.min + i), target));
            }
            flushIfTerminal(data);
            addToGraph(Switch, OpInfo(&data), get(VirtualRegister(currentInstruction[3].u.operand)));
            LAST_OPCODE(op_switch_char);
        }

        case op_switch_string: {
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchString;
            data.switchTableIndex = currentInstruction[1].u.operand;
            data.fallThrough.setBytecodeIndex(m_currentIndex + currentInstruction[2].u.operand);
            StringJumpTable& table = m_codeBlock->stringSwitchJumpTable(data.switchTableIndex);
            StringJumpTable::StringOffsetTable::iterator iter;
            StringJumpTable::StringOffsetTable::iterator end = table.offsetTable.end();
            for (iter = table.offsetTable.begin(); iter != end; ++iter) {
                unsigned target = m_currentIndex + iter->value.branchOffset;
                if (target == data.fallThrough.bytecodeIndex())
                    continue;
                data.cases.append(
                    SwitchCase::withBytecodeIndex(LazyJSValue::knownStringImpl(iter->key.get()), target));
            }
            flushIfTerminal(data);
            addToGraph(Switch, OpInfo(&data), get(VirtualRegister(currentInstruction[3].u.operand)));
            LAST_OPCODE(op_switch_string);
        }

        case op_ret:
            flushForReturn();
            if (inlineCallFrame()) {
                if (m_inlineStackTop->m_returnValue.isValid())
                    setDirect(m_inlineStackTop->m_returnValue, get(VirtualRegister(currentInstruction[1].u.operand)), ImmediateSetWithFlush);
                m_inlineStackTop->m_didReturn = true;
                if (m_inlineStackTop->m_unlinkedBlocks.isEmpty()) {
                    // If we're returning from the first block, then we're done parsing.
                    ASSERT(m_inlineStackTop->m_callsiteBlockHead == m_graph.lastBlock());
                    shouldContinueParsing = false;
                    LAST_OPCODE(op_ret);
                } else {
                    // If inlining created blocks, and we're doing a return, then we need some
                    // special linking.
                    ASSERT(m_inlineStackTop->m_unlinkedBlocks.last().m_block == m_graph.lastBlock());
                    m_inlineStackTop->m_unlinkedBlocks.last().m_needsNormalLinking = false;
                }
                if (m_currentIndex + OPCODE_LENGTH(op_ret) != m_inlineStackTop->m_codeBlock->instructions().size() || m_inlineStackTop->m_didEarlyReturn) {
                    ASSERT(m_currentIndex + OPCODE_LENGTH(op_ret) <= m_inlineStackTop->m_codeBlock->instructions().size());
                    addToGraph(Jump, OpInfo(0));
                    m_inlineStackTop->m_unlinkedBlocks.last().m_needsEarlyReturnLinking = true;
                    m_inlineStackTop->m_didEarlyReturn = true;
                }
                LAST_OPCODE(op_ret);
            }
            addToGraph(Return, get(VirtualRegister(currentInstruction[1].u.operand)));
            LAST_OPCODE(op_ret);
            
        case op_end:
            flushForReturn();
            ASSERT(!inlineCallFrame());
            addToGraph(Return, get(VirtualRegister(currentInstruction[1].u.operand)));
            LAST_OPCODE(op_end);

        case op_throw:
            addToGraph(Throw, get(VirtualRegister(currentInstruction[1].u.operand)));
            flushForTerminal();
            addToGraph(Unreachable);
            LAST_OPCODE(op_throw);
            
        case op_throw_static_error:
            addToGraph(ThrowReferenceError);
            flushForTerminal();
            addToGraph(Unreachable);
            LAST_OPCODE(op_throw_static_error);
            
        case op_call:
            handleCall(currentInstruction, Call, CodeForCall);
            NEXT_OPCODE(op_call);
            
        case op_construct:
            handleCall(currentInstruction, Construct, CodeForConstruct);
            NEXT_OPCODE(op_construct);
            
        case op_call_varargs: {
            int result = currentInstruction[1].u.operand;
            int callee = currentInstruction[2].u.operand;
            int thisReg = currentInstruction[3].u.operand;
            int arguments = currentInstruction[4].u.operand;
            int firstFreeReg = currentInstruction[5].u.operand;
            
            ASSERT(inlineCallFrame());
            ASSERT_UNUSED(arguments, arguments == m_inlineStackTop->m_codeBlock->argumentsRegister().offset());
            ASSERT(!m_inlineStackTop->m_codeBlock->symbolTable()->slowArguments());

            addToGraph(CheckArgumentsNotCreated);

            unsigned argCount = inlineCallFrame()->arguments.size();
            
            // Let's compute the register offset. We start with the last used register, and
            // then adjust for the things we want in the call frame.
            int registerOffset = firstFreeReg + 1;
            registerOffset -= argCount; // We will be passing some arguments.
            registerOffset -= JSStack::CallFrameHeaderSize; // We will pretend to have a call frame header.
            
            // Get the alignment right.
            registerOffset = -WTF::roundUpToMultipleOf(
                stackAlignmentRegisters(),
                -registerOffset);

            ensureLocals(
                m_inlineStackTop->remapOperand(
                    VirtualRegister(registerOffset)).toLocal());
            
            // The bytecode wouldn't have set up the arguments. But we'll do it and make it
            // look like the bytecode had done it.
            int nextRegister = registerOffset + JSStack::CallFrameHeaderSize;
            set(VirtualRegister(nextRegister++), get(VirtualRegister(thisReg)), ImmediateNakedSet);
            for (unsigned argument = 1; argument < argCount; ++argument)
                set(VirtualRegister(nextRegister++), get(virtualRegisterForArgument(argument)), ImmediateNakedSet);
            
            handleCall(
                result, Call, CodeForCall, OPCODE_LENGTH(op_call_varargs),
                callee, argCount, registerOffset);
            NEXT_OPCODE(op_call_varargs);
        }
            
        case op_jneq_ptr:
            // Statically speculate for now. It makes sense to let speculate-only jneq_ptr
            // support simmer for a while before making it more general, since it's
            // already gnarly enough as it is.
            ASSERT(pointerIsFunction(currentInstruction[2].u.specialPointer));
            addToGraph(
                CheckCell,
                OpInfo(m_graph.freeze(static_cast<JSCell*>(actualPointerFor(
                    m_inlineStackTop->m_codeBlock, currentInstruction[2].u.specialPointer)))),
                get(VirtualRegister(currentInstruction[1].u.operand)));
            addToGraph(Jump, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jneq_ptr)));
            LAST_OPCODE(op_jneq_ptr);

        case op_resolve_scope: {
            int dst = currentInstruction[1].u.operand;
            ResolveType resolveType = static_cast<ResolveType>(currentInstruction[3].u.operand);
            unsigned depth = currentInstruction[4].u.operand;

            // get_from_scope and put_to_scope depend on this watchpoint forcing OSR exit, so they don't add their own watchpoints.
            if (needsVarInjectionChecks(resolveType))
                addToGraph(VarInjectionWatchpoint);

            switch (resolveType) {
            case GlobalProperty:
            case GlobalVar:
            case GlobalPropertyWithVarInjectionChecks:
            case GlobalVarWithVarInjectionChecks:
                set(VirtualRegister(dst), weakJSConstant(m_inlineStackTop->m_codeBlock->globalObject()));
                break;
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                JSLexicalEnvironment* lexicalEnvironment = currentInstruction[5].u.lexicalEnvironment.get();
                if (lexicalEnvironment
                    && lexicalEnvironment->symbolTable()->m_functionEnteredOnce.isStillValid()) {
                    addToGraph(FunctionReentryWatchpoint, OpInfo(lexicalEnvironment->symbolTable()));
                    set(VirtualRegister(dst), weakJSConstant(lexicalEnvironment));
                    break;
                }
                set(VirtualRegister(dst), getScope(depth));
                break;
            }
            case Dynamic:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            NEXT_OPCODE(op_resolve_scope);
        }

        case op_get_from_scope: {
            int dst = currentInstruction[1].u.operand;
            int scope = currentInstruction[2].u.operand;
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[3].u.operand];
            StringImpl* uid = m_graph.identifiers()[identifierNumber];
            ResolveType resolveType = ResolveModeAndType(currentInstruction[4].u.operand).type();

            Structure* structure = 0;
            WatchpointSet* watchpoints = 0;
            uintptr_t operand;
            {
                ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks)
                    watchpoints = currentInstruction[5].u.watchpointSet;
                else
                    structure = currentInstruction[5].u.structure.get();
                operand = reinterpret_cast<uintptr_t>(currentInstruction[6].u.pointer);
            }

            UNUSED_PARAM(watchpoints); // We will use this in the future. For now we set it as a way of documenting the fact that that's what index 5 is in GlobalVar mode.

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                SpeculatedType prediction = getPrediction();
                GetByIdStatus status = GetByIdStatus::computeFor(*m_vm, structure, uid);
                if (status.state() != GetByIdStatus::Simple
                    || status.numVariants() != 1
                    || status[0].structureSet().size() != 1) {
                    set(VirtualRegister(dst), addToGraph(GetByIdFlush, OpInfo(identifierNumber), OpInfo(prediction), get(VirtualRegister(scope))));
                    break;
                }
                Node* base = cellConstantWithStructureCheck(globalObject, status[0].structureSet().onlyStructure());
                addToGraph(Phantom, get(VirtualRegister(scope)));
                set(VirtualRegister(dst), handleGetByOffset(prediction, base, status[0].structureSet(), identifierNumber, operand));
                break;
            }
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks: {
                addToGraph(Phantom, get(VirtualRegister(scope)));
                SymbolTableEntry entry = globalObject->symbolTable()->get(uid);
                VariableWatchpointSet* watchpointSet = entry.watchpointSet();
                JSValue inferredValue =
                    watchpointSet ? watchpointSet->inferredValue() : JSValue();
                if (!inferredValue) {
                    SpeculatedType prediction = getPrediction();
                    set(VirtualRegister(dst), addToGraph(GetGlobalVar, OpInfo(operand), OpInfo(prediction)));
                    break;
                }
                
                addToGraph(VariableWatchpoint, OpInfo(watchpointSet));
                set(VirtualRegister(dst), weakJSConstant(inferredValue));
                break;
            }
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(VirtualRegister(scope));
                if (JSLexicalEnvironment* lexicalEnvironment = m_graph.tryGetActivation(scopeNode)) {
                    SymbolTable* symbolTable = lexicalEnvironment->symbolTable();
                    ConcurrentJITLocker locker(symbolTable->m_lock);
                    SymbolTable::Map::iterator iter = symbolTable->find(locker, uid);
                    ASSERT(iter != symbolTable->end(locker));
                    VariableWatchpointSet* watchpointSet = iter->value.watchpointSet();
                    if (watchpointSet) {
                        if (JSValue value = watchpointSet->inferredValue()) {
                            addToGraph(Phantom, scopeNode);
                            addToGraph(VariableWatchpoint, OpInfo(watchpointSet));
                            set(VirtualRegister(dst), weakJSConstant(value));
                            break;
                        }
                    }
                }
                SpeculatedType prediction = getPrediction();
                set(VirtualRegister(dst),
                    addToGraph(GetClosureVar, OpInfo(operand), OpInfo(prediction), 
                        addToGraph(GetClosureRegisters, scopeNode)));
                break;
            }
            case Dynamic:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            NEXT_OPCODE(op_get_from_scope);
        }

        case op_put_to_scope: {
            unsigned scope = currentInstruction[1].u.operand;
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[2].u.operand];
            unsigned value = currentInstruction[3].u.operand;
            ResolveType resolveType = ResolveModeAndType(currentInstruction[4].u.operand).type();
            StringImpl* uid = m_graph.identifiers()[identifierNumber];

            Structure* structure = 0;
            VariableWatchpointSet* watchpoints = 0;
            uintptr_t operand;
            {
                ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks)
                    watchpoints = currentInstruction[5].u.watchpointSet;
                else
                    structure = currentInstruction[5].u.structure.get();
                operand = reinterpret_cast<uintptr_t>(currentInstruction[6].u.pointer);
            }

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                PutByIdStatus status = PutByIdStatus::computeFor(*m_vm, globalObject, structure, uid, false);
                if (status.numVariants() != 1
                    || status[0].kind() != PutByIdVariant::Replace
                    || status[0].structure().size() != 1) {
                    addToGraph(PutById, OpInfo(identifierNumber), get(VirtualRegister(scope)), get(VirtualRegister(value)));
                    break;
                }
                ASSERT(status[0].structure().onlyStructure() == structure);
                Node* base = cellConstantWithStructureCheck(globalObject, structure);
                addToGraph(Phantom, get(VirtualRegister(scope)));
                handlePutByOffset(base, identifierNumber, static_cast<PropertyOffset>(operand), get(VirtualRegister(value)));
                // Keep scope alive until after put.
                addToGraph(Phantom, get(VirtualRegister(scope)));
                break;
            }
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks: {
                SymbolTableEntry entry = globalObject->symbolTable()->get(uid);
                ASSERT(watchpoints == entry.watchpointSet());
                Node* valueNode = get(VirtualRegister(value));
                addToGraph(PutGlobalVar, OpInfo(operand), valueNode);
                if (watchpoints->state() != IsInvalidated)
                    addToGraph(NotifyWrite, OpInfo(watchpoints), valueNode);
                // Keep scope alive until after put.
                addToGraph(Phantom, get(VirtualRegister(scope)));
                break;
            }
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(VirtualRegister(scope));
                Node* scopeRegisters = addToGraph(GetClosureRegisters, scopeNode);
                addToGraph(PutClosureVar, OpInfo(operand), scopeNode, scopeRegisters, get(VirtualRegister(value)));
                break;
            }
            case Dynamic:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            NEXT_OPCODE(op_put_to_scope);
        }

        case op_loop_hint: {
            // Baseline->DFG OSR jumps between loop hints. The DFG assumes that Baseline->DFG
            // OSR can only happen at basic block boundaries. Assert that these two statements
            // are compatible.
            RELEASE_ASSERT(m_currentIndex == blockBegin);
            
            // We never do OSR into an inlined code block. That could not happen, since OSR
            // looks up the code block that is the replacement for the baseline JIT code
            // block. Hence, machine code block = true code block = not inline code block.
            if (!m_inlineStackTop->m_caller)
                m_currentBlock->isOSRTarget = true;

            addToGraph(LoopHint);
            
            if (m_vm->watchdog && m_vm->watchdog->isEnabled())
                addToGraph(CheckWatchdogTimer);
            
            NEXT_OPCODE(op_loop_hint);
        }
            
        case op_init_lazy_reg: {
            set(VirtualRegister(currentInstruction[1].u.operand), jsConstant(JSValue()));
            ASSERT(operandIsLocal(currentInstruction[1].u.operand));
            m_graph.m_lazyVars.set(VirtualRegister(currentInstruction[1].u.operand).toLocal());
            NEXT_OPCODE(op_init_lazy_reg);
        }
            
        case op_create_lexical_environment: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CreateActivation, get(VirtualRegister(currentInstruction[1].u.operand))));
            NEXT_OPCODE(op_create_lexical_environment);
        }
            
        case op_create_arguments: {
            m_graph.m_hasArguments = true;
            Node* createArguments = addToGraph(CreateArguments, get(VirtualRegister(currentInstruction[1].u.operand)));
            set(VirtualRegister(currentInstruction[1].u.operand), createArguments);
            set(unmodifiedArgumentsRegister(VirtualRegister(currentInstruction[1].u.operand)), createArguments);
            NEXT_OPCODE(op_create_arguments);
        }
            
        case op_tear_off_lexical_environment: {
            addToGraph(TearOffActivation, get(VirtualRegister(currentInstruction[1].u.operand)));
            NEXT_OPCODE(op_tear_off_lexical_environment);
        }

        case op_tear_off_arguments: {
            m_graph.m_hasArguments = true;
            addToGraph(TearOffArguments, get(VirtualRegister(currentInstruction[1].u.operand)), get(VirtualRegister(currentInstruction[2].u.operand)));
            NEXT_OPCODE(op_tear_off_arguments);
        }
            
        case op_get_arguments_length: {
            m_graph.m_hasArguments = true;
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetMyArgumentsLengthSafe));
            NEXT_OPCODE(op_get_arguments_length);
        }
            
        case op_get_argument_by_val: {
            m_graph.m_hasArguments = true;
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(
                    GetMyArgumentByValSafe, OpInfo(0), OpInfo(getPrediction()),
                    get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_get_argument_by_val);
        }
            
        case op_new_func: {
            if (!currentInstruction[3].u.operand) {
                set(VirtualRegister(currentInstruction[1].u.operand),
                    addToGraph(NewFunctionNoCheck, OpInfo(currentInstruction[2].u.operand)));
            } else {
                set(VirtualRegister(currentInstruction[1].u.operand),
                    addToGraph(
                        NewFunction,
                        OpInfo(currentInstruction[2].u.operand),
                        get(VirtualRegister(currentInstruction[1].u.operand))));
            }
            NEXT_OPCODE(op_new_func);
        }
            
        case op_new_captured_func: {
            Node* function = addToGraph(
                NewFunctionNoCheck, OpInfo(currentInstruction[2].u.operand));
            if (VariableWatchpointSet* set = currentInstruction[3].u.watchpointSet)
                addToGraph(NotifyWrite, OpInfo(set), function);
            set(VirtualRegister(currentInstruction[1].u.operand), function);
            NEXT_OPCODE(op_new_captured_func);
        }
            
        case op_new_func_exp: {
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(NewFunctionExpression, OpInfo(currentInstruction[2].u.operand)));
            NEXT_OPCODE(op_new_func_exp);
        }

        case op_typeof: {
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(TypeOf, get(VirtualRegister(currentInstruction[2].u.operand))));
            NEXT_OPCODE(op_typeof);
        }

        case op_to_number: {
            Node* node = get(VirtualRegister(currentInstruction[2].u.operand));
            addToGraph(Phantom, Edge(node, NumberUse));
            set(VirtualRegister(currentInstruction[1].u.operand), node);
            NEXT_OPCODE(op_to_number);
        }
            
        case op_in: {
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(In, get(VirtualRegister(currentInstruction[2].u.operand)), get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_in);
        }

        case op_get_enumerable_length: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetEnumerableLength, 
                get(VirtualRegister(currentInstruction[2].u.operand))));
            NEXT_OPCODE(op_get_enumerable_length);
        }

        case op_has_generic_property: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(HasGenericProperty, 
                get(VirtualRegister(currentInstruction[2].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_has_generic_property);
        }

        case op_has_structure_property: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(HasStructureProperty, 
                get(VirtualRegister(currentInstruction[2].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand)),
                get(VirtualRegister(currentInstruction[4].u.operand))));
            NEXT_OPCODE(op_has_structure_property);
        }

        case op_has_indexed_property: {
            Node* base = get(VirtualRegister(currentInstruction[2].u.operand));
            ArrayMode arrayMode = getArrayModeConsideringSlowPath(currentInstruction[4].u.arrayProfile, Array::Read);
            Node* property = get(VirtualRegister(currentInstruction[3].u.operand));
            Node* hasIterableProperty = addToGraph(HasIndexedProperty, OpInfo(arrayMode.asWord()), base, property);
            set(VirtualRegister(currentInstruction[1].u.operand), hasIterableProperty);
            NEXT_OPCODE(op_has_indexed_property);
        }

        case op_get_direct_pname: {
            SpeculatedType prediction = getPredictionWithoutOSRExit();
            
            Node* base = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* property = get(VirtualRegister(currentInstruction[3].u.operand));
            Node* index = get(VirtualRegister(currentInstruction[4].u.operand));
            Node* enumerator = get(VirtualRegister(currentInstruction[5].u.operand));

            addVarArgChild(base);
            addVarArgChild(property);
            addVarArgChild(index);
            addVarArgChild(enumerator);
            set(VirtualRegister(currentInstruction[1].u.operand), 
                addToGraph(Node::VarArg, GetDirectPname, OpInfo(0), OpInfo(prediction)));

            NEXT_OPCODE(op_get_direct_pname);
        }

        case op_get_structure_property_enumerator: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetStructurePropertyEnumerator, 
                get(VirtualRegister(currentInstruction[2].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_get_structure_property_enumerator);
        }

        case op_get_generic_property_enumerator: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetGenericPropertyEnumerator, 
                get(VirtualRegister(currentInstruction[2].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand)),
                get(VirtualRegister(currentInstruction[4].u.operand))));
            NEXT_OPCODE(op_get_generic_property_enumerator);
        }

        case op_next_enumerator_pname: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetEnumeratorPname, 
                get(VirtualRegister(currentInstruction[2].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_next_enumerator_pname);
        }

        case op_to_index_string: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(ToIndexString, 
                get(VirtualRegister(currentInstruction[2].u.operand))));
            NEXT_OPCODE(op_to_index_string);
        }

        default:
            // Parse failed! This should not happen because the capabilities checker
            // should have caught it.
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }
}

void ByteCodeParser::linkBlock(BasicBlock* block, Vector<BasicBlock*>& possibleTargets)
{
    ASSERT(!block->isLinked);
    ASSERT(!block->isEmpty());
    Node* node = block->last();
    ASSERT(node->isTerminal());
    
    switch (node->op()) {
    case Jump:
        node->targetBlock() = blockForBytecodeOffset(possibleTargets, node->targetBytecodeOffsetDuringParsing());
        break;
        
    case Branch: {
        BranchData* data = node->branchData();
        data->taken.block = blockForBytecodeOffset(possibleTargets, data->takenBytecodeIndex());
        data->notTaken.block = blockForBytecodeOffset(possibleTargets, data->notTakenBytecodeIndex());
        break;
    }
        
    case Switch: {
        SwitchData* data = node->switchData();
        for (unsigned i = node->switchData()->cases.size(); i--;)
            data->cases[i].target.block = blockForBytecodeOffset(possibleTargets, data->cases[i].target.bytecodeIndex());
        data->fallThrough.block = blockForBytecodeOffset(possibleTargets, data->fallThrough.bytecodeIndex());
        break;
    }
        
    default:
        break;
    }
    
    if (verbose)
        dataLog("Marking ", RawPointer(block), " as linked (actually did linking)\n");
    block->didLink();
}

void ByteCodeParser::linkBlocks(Vector<UnlinkedBlock>& unlinkedBlocks, Vector<BasicBlock*>& possibleTargets)
{
    for (size_t i = 0; i < unlinkedBlocks.size(); ++i) {
        if (verbose)
            dataLog("Attempting to link ", RawPointer(unlinkedBlocks[i].m_block), "\n");
        if (unlinkedBlocks[i].m_needsNormalLinking) {
            if (verbose)
                dataLog("    Does need normal linking.\n");
            linkBlock(unlinkedBlocks[i].m_block, possibleTargets);
            unlinkedBlocks[i].m_needsNormalLinking = false;
        }
    }
}

void ByteCodeParser::buildOperandMapsIfNecessary()
{
    if (m_haveBuiltOperandMaps)
        return;
    
    for (size_t i = 0; i < m_codeBlock->numberOfIdentifiers(); ++i)
        m_identifierMap.add(m_codeBlock->identifier(i).impl(), i);
    
    m_haveBuiltOperandMaps = true;
}

ByteCodeParser::InlineStackEntry::InlineStackEntry(
    ByteCodeParser* byteCodeParser,
    CodeBlock* codeBlock,
    CodeBlock* profiledBlock,
    BasicBlock* callsiteBlockHead,
    JSFunction* callee, // Null if this is a closure call.
    VirtualRegister returnValueVR,
    VirtualRegister inlineCallFrameStart,
    int argumentCountIncludingThis,
    InlineCallFrame::Kind kind)
    : m_byteCodeParser(byteCodeParser)
    , m_codeBlock(codeBlock)
    , m_profiledBlock(profiledBlock)
    , m_callsiteBlockHead(callsiteBlockHead)
    , m_returnValue(returnValueVR)
    , m_didReturn(false)
    , m_didEarlyReturn(false)
    , m_caller(byteCodeParser->m_inlineStackTop)
{
    {
        ConcurrentJITLocker locker(m_profiledBlock->m_lock);
        m_lazyOperands.initialize(locker, m_profiledBlock->lazyOperandValueProfiles());
        m_exitProfile.initialize(locker, profiledBlock->exitProfile());
        
        // We do this while holding the lock because we want to encourage StructureStubInfo's
        // to be potentially added to operations and because the profiled block could be in the
        // middle of LLInt->JIT tier-up in which case we would be adding the info's right now.
        if (m_profiledBlock->hasBaselineJITProfiling()) {
            m_profiledBlock->getStubInfoMap(locker, m_stubInfos);
            m_profiledBlock->getCallLinkInfoMap(locker, m_callLinkInfos);
        }
    }
    
    m_argumentPositions.resize(argumentCountIncludingThis);
    for (int i = 0; i < argumentCountIncludingThis; ++i) {
        byteCodeParser->m_graph.m_argumentPositions.append(ArgumentPosition());
        ArgumentPosition* argumentPosition = &byteCodeParser->m_graph.m_argumentPositions.last();
        m_argumentPositions[i] = argumentPosition;
    }
    
    // Track the code-block-global exit sites.
    if (m_exitProfile.hasExitSite(ArgumentsEscaped)) {
        byteCodeParser->m_graph.m_executablesWhoseArgumentsEscaped.add(
            codeBlock->ownerExecutable());
    }
        
    if (m_caller) {
        // Inline case.
        ASSERT(codeBlock != byteCodeParser->m_codeBlock);
        ASSERT(inlineCallFrameStart.isValid());
        ASSERT(callsiteBlockHead);
        
        m_inlineCallFrame = byteCodeParser->m_graph.m_plan.inlineCallFrames->add();
        initializeLazyWriteBarrierForInlineCallFrameExecutable(
            byteCodeParser->m_graph.m_plan.writeBarriers,
            m_inlineCallFrame->executable,
            byteCodeParser->m_codeBlock,
            m_inlineCallFrame,
            byteCodeParser->m_codeBlock->ownerExecutable(),
            codeBlock->ownerExecutable());
        m_inlineCallFrame->setStackOffset(inlineCallFrameStart.offset() - JSStack::CallFrameHeaderSize);
        if (callee) {
            m_inlineCallFrame->calleeRecovery = ValueRecovery::constant(callee);
            m_inlineCallFrame->isClosureCall = false;
        } else
            m_inlineCallFrame->isClosureCall = true;
        m_inlineCallFrame->caller = byteCodeParser->currentCodeOrigin();
        m_inlineCallFrame->arguments.resize(argumentCountIncludingThis); // Set the number of arguments including this, but don't configure the value recoveries, yet.
        m_inlineCallFrame->kind = kind;
        
        if (m_inlineCallFrame->caller.inlineCallFrame)
            m_inlineCallFrame->capturedVars = m_inlineCallFrame->caller.inlineCallFrame->capturedVars;
        else {
            for (int i = byteCodeParser->m_codeBlock->m_numVars; i--;) {
                if (byteCodeParser->m_codeBlock->isCaptured(virtualRegisterForLocal(i)))
                    m_inlineCallFrame->capturedVars.set(i);
            }
        }

        for (int i = argumentCountIncludingThis; i--;) {
            VirtualRegister argument = virtualRegisterForArgument(i);
            if (codeBlock->isCaptured(argument))
                m_inlineCallFrame->capturedVars.set(VirtualRegister(argument.offset() + m_inlineCallFrame->stackOffset).toLocal());
        }
        for (size_t i = codeBlock->m_numVars; i--;) {
            VirtualRegister local = virtualRegisterForLocal(i);
            if (codeBlock->isCaptured(local))
                m_inlineCallFrame->capturedVars.set(VirtualRegister(local.offset() + m_inlineCallFrame->stackOffset).toLocal());
        }

        byteCodeParser->buildOperandMapsIfNecessary();
        
        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        m_constantBufferRemap.resize(codeBlock->numberOfConstantBuffers());
        m_switchRemap.resize(codeBlock->numberOfSwitchJumpTables());

        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i) {
            StringImpl* rep = codeBlock->identifier(i).impl();
            BorrowedIdentifierMap::AddResult result = byteCodeParser->m_identifierMap.add(rep, byteCodeParser->m_graph.identifiers().numberOfIdentifiers());
            if (result.isNewEntry)
                byteCodeParser->m_graph.identifiers().addLazily(rep);
            m_identifierRemap[i] = result.iterator->value;
        }
        for (unsigned i = 0; i < codeBlock->numberOfConstantBuffers(); ++i) {
            // If we inline the same code block multiple times, we don't want to needlessly
            // duplicate its constant buffers.
            HashMap<ConstantBufferKey, unsigned>::iterator iter =
                byteCodeParser->m_constantBufferCache.find(ConstantBufferKey(codeBlock, i));
            if (iter != byteCodeParser->m_constantBufferCache.end()) {
                m_constantBufferRemap[i] = iter->value;
                continue;
            }
            Vector<JSValue>& buffer = codeBlock->constantBufferAsVector(i);
            unsigned newIndex = byteCodeParser->m_codeBlock->addConstantBuffer(buffer);
            m_constantBufferRemap[i] = newIndex;
            byteCodeParser->m_constantBufferCache.add(ConstantBufferKey(codeBlock, i), newIndex);
        }
        for (unsigned i = 0; i < codeBlock->numberOfSwitchJumpTables(); ++i) {
            m_switchRemap[i] = byteCodeParser->m_codeBlock->numberOfSwitchJumpTables();
            byteCodeParser->m_codeBlock->addSwitchJumpTable() = codeBlock->switchJumpTable(i);
        }
        m_callsiteBlockHeadNeedsLinking = true;
    } else {
        // Machine code block case.
        ASSERT(codeBlock == byteCodeParser->m_codeBlock);
        ASSERT(!callee);
        ASSERT(!returnValueVR.isValid());
        ASSERT(!inlineCallFrameStart.isValid());
        ASSERT(!callsiteBlockHead);

        m_inlineCallFrame = 0;

        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        m_constantBufferRemap.resize(codeBlock->numberOfConstantBuffers());
        m_switchRemap.resize(codeBlock->numberOfSwitchJumpTables());
        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i)
            m_identifierRemap[i] = i;
        for (size_t i = 0; i < codeBlock->numberOfConstantBuffers(); ++i)
            m_constantBufferRemap[i] = i;
        for (size_t i = 0; i < codeBlock->numberOfSwitchJumpTables(); ++i)
            m_switchRemap[i] = i;
        m_callsiteBlockHeadNeedsLinking = false;
    }
    
    byteCodeParser->m_inlineStackTop = this;
}

void ByteCodeParser::parseCodeBlock()
{
    clearCaches();
    
    CodeBlock* codeBlock = m_inlineStackTop->m_codeBlock;
    
    if (m_graph.compilation()) {
        m_graph.compilation()->addProfiledBytecodes(
            *m_vm->m_perBytecodeProfiler, m_inlineStackTop->m_profiledBlock);
    }
    
    bool shouldDumpBytecode = Options::dumpBytecodeAtDFGTime();
    if (shouldDumpBytecode) {
        dataLog("Parsing ", *codeBlock);
        if (inlineCallFrame()) {
            dataLog(
                " for inlining at ", CodeBlockWithJITType(m_codeBlock, JITCode::DFGJIT),
                " ", inlineCallFrame()->caller);
        }
        dataLog(
            ": captureCount = ", codeBlock->symbolTable() ? codeBlock->symbolTable()->captureCount() : 0,
            ", needsActivation = ", codeBlock->needsActivation(),
            ", isStrictMode = ", codeBlock->ownerExecutable()->isStrictMode(), "\n");
        codeBlock->baselineVersion()->dumpBytecode();
    }
    
    Vector<unsigned, 32> jumpTargets;
    computePreciseJumpTargets(codeBlock, jumpTargets);
    if (Options::dumpBytecodeAtDFGTime()) {
        dataLog("Jump targets: ");
        CommaPrinter comma;
        for (unsigned i = 0; i < jumpTargets.size(); ++i)
            dataLog(comma, jumpTargets[i]);
        dataLog("\n");
    }
    
    for (unsigned jumpTargetIndex = 0; jumpTargetIndex <= jumpTargets.size(); ++jumpTargetIndex) {
        // The maximum bytecode offset to go into the current basicblock is either the next jump target, or the end of the instructions.
        unsigned limit = jumpTargetIndex < jumpTargets.size() ? jumpTargets[jumpTargetIndex] : codeBlock->instructions().size();
        ASSERT(m_currentIndex < limit);

        // Loop until we reach the current limit (i.e. next jump target).
        do {
            if (!m_currentBlock) {
                // Check if we can use the last block.
                if (m_graph.numBlocks() && m_graph.lastBlock()->isEmpty()) {
                    // This must be a block belonging to us.
                    ASSERT(m_inlineStackTop->m_unlinkedBlocks.last().m_block == m_graph.lastBlock());
                    // Either the block is linkable or it isn't. If it's linkable then it's the last
                    // block in the blockLinkingTargets list. If it's not then the last block will
                    // have a lower bytecode index that the one we're about to give to this block.
                    if (m_inlineStackTop->m_blockLinkingTargets.isEmpty() || m_inlineStackTop->m_blockLinkingTargets.last()->bytecodeBegin != m_currentIndex) {
                        // Make the block linkable.
                        ASSERT(m_inlineStackTop->m_blockLinkingTargets.isEmpty() || m_inlineStackTop->m_blockLinkingTargets.last()->bytecodeBegin < m_currentIndex);
                        m_inlineStackTop->m_blockLinkingTargets.append(m_graph.lastBlock());
                    }
                    // Change its bytecode begin and continue.
                    m_currentBlock = m_graph.lastBlock();
                    m_currentBlock->bytecodeBegin = m_currentIndex;
                } else {
                    RefPtr<BasicBlock> block = adoptRef(new BasicBlock(m_currentIndex, m_numArguments, m_numLocals, PNaN));
                    m_currentBlock = block.get();
                    // This assertion checks two things:
                    // 1) If the bytecodeBegin is greater than currentIndex, then something has gone
                    //    horribly wrong. So, we're probably generating incorrect code.
                    // 2) If the bytecodeBegin is equal to the currentIndex, then we failed to do
                    //    a peephole coalescing of this block in the if statement above. So, we're
                    //    generating suboptimal code and leaving more work for the CFG simplifier.
                    if (!m_inlineStackTop->m_unlinkedBlocks.isEmpty()) {
                        unsigned lastBegin =
                            m_inlineStackTop->m_unlinkedBlocks.last().m_block->bytecodeBegin;
                        ASSERT_UNUSED(
                            lastBegin, lastBegin == UINT_MAX || lastBegin < m_currentIndex);
                    }
                    m_inlineStackTop->m_unlinkedBlocks.append(UnlinkedBlock(block.get()));
                    m_inlineStackTop->m_blockLinkingTargets.append(block.get());
                    // The first block is definitely an OSR target.
                    if (!m_graph.numBlocks())
                        block->isOSRTarget = true;
                    m_graph.appendBlock(block);
                    prepareToParseBlock();
                }
            }

            bool shouldContinueParsing = parseBlock(limit);

            // We should not have gone beyond the limit.
            ASSERT(m_currentIndex <= limit);
            
            // We should have planted a terminal, or we just gave up because
            // we realized that the jump target information is imprecise, or we
            // are at the end of an inline function, or we realized that we
            // should stop parsing because there was a return in the first
            // basic block.
            ASSERT(m_currentBlock->isEmpty() || m_currentBlock->last()->isTerminal() || (m_currentIndex == codeBlock->instructions().size() && inlineCallFrame()) || !shouldContinueParsing);

            if (!shouldContinueParsing)
                return;
            
            m_currentBlock = 0;
        } while (m_currentIndex < limit);
    }

    // Should have reached the end of the instructions.
    ASSERT(m_currentIndex == codeBlock->instructions().size());
}

bool ByteCodeParser::parse()
{
    // Set during construction.
    ASSERT(!m_currentIndex);
    
    if (Options::verboseDFGByteCodeParsing())
        dataLog("Parsing ", *m_codeBlock, "\n");
    
    m_dfgCodeBlock = m_graph.m_plan.profiledDFGCodeBlock.get();
    if (isFTL(m_graph.m_plan.mode) && m_dfgCodeBlock
        && Options::enablePolyvariantDevirtualization()) {
        if (Options::enablePolyvariantCallInlining())
            CallLinkStatus::computeDFGStatuses(m_dfgCodeBlock, m_callContextMap);
        if (Options::enablePolyvariantByIdInlining())
            m_dfgCodeBlock->getStubInfoMap(m_dfgStubInfos);
    }
    
    if (m_codeBlock->captureCount()) {
        SymbolTable* symbolTable = m_codeBlock->symbolTable();
        ConcurrentJITLocker locker(symbolTable->m_lock);
        SymbolTable::Map::iterator iter = symbolTable->begin(locker);
        SymbolTable::Map::iterator end = symbolTable->end(locker);
        for (; iter != end; ++iter) {
            VariableWatchpointSet* set = iter->value.watchpointSet();
            if (!set)
                continue;
            size_t index = static_cast<size_t>(VirtualRegister(iter->value.getIndex()).toLocal());
            while (m_localWatchpoints.size() <= index)
                m_localWatchpoints.append(nullptr);
            m_localWatchpoints[index] = set;
        }
    }
    
    InlineStackEntry inlineStackEntry(
        this, m_codeBlock, m_profiledBlock, 0, 0, VirtualRegister(), VirtualRegister(),
        m_codeBlock->numParameters(), InlineCallFrame::Call);
    
    parseCodeBlock();

    linkBlocks(inlineStackEntry.m_unlinkedBlocks, inlineStackEntry.m_blockLinkingTargets);
    m_graph.determineReachability();
    m_graph.killUnreachableBlocks();
    
    for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
        BasicBlock* block = m_graph.block(blockIndex);
        if (!block)
            continue;
        ASSERT(block->variablesAtHead.numberOfLocals() == m_graph.block(0)->variablesAtHead.numberOfLocals());
        ASSERT(block->variablesAtHead.numberOfArguments() == m_graph.block(0)->variablesAtHead.numberOfArguments());
        ASSERT(block->variablesAtTail.numberOfLocals() == m_graph.block(0)->variablesAtHead.numberOfLocals());
        ASSERT(block->variablesAtTail.numberOfArguments() == m_graph.block(0)->variablesAtHead.numberOfArguments());
    }
    
    m_graph.m_localVars = m_numLocals;
    m_graph.m_parameterSlots = m_parameterSlots;

    return true;
}

bool parse(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Parsing");
    return ByteCodeParser(graph).parse();
}

} } // namespace JSC::DFG

#endif
