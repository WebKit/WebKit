/*
 * Copyright (C) 2011-2015 Apple Inc. All rights reserved.
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
#include "BasicBlockLocation.h"
#include "CallLinkStatus.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGArrayMode.h"
#include "DFGCapabilities.h"
#include "DFGGraph.h"
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
        , m_hasDebuggerEnabled(graph.hasDebuggerEnabled())
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
    template<typename ChecksFunctor>
    bool handleMinMax(int resultOperand, NodeType op, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& insertChecks);
    
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
    void handleVarargsCall(Instruction* pc, NodeType op, CodeSpecializationKind);
    void emitFunctionChecks(CallVariant, Node* callTarget, VirtualRegister thisArgumnt);
    void emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis);
    unsigned inliningCost(CallVariant, int argumentCountIncludingThis, CodeSpecializationKind); // Return UINT_MAX if it's not an inlining candidate. By convention, intrinsics have a cost of 1.
    // Handle inlining. Return true if it succeeded, false if we need to plant a call.
    bool handleInlining(Node* callTargetNode, int resultOperand, const CallLinkStatus&, int registerOffset, VirtualRegister thisArgument, VirtualRegister argumentsArgument, unsigned argumentsOffset, int argumentCountIncludingThis, unsigned nextOffset, NodeType callOp, InlineCallFrame::Kind, SpeculatedType prediction);
    enum CallerLinkability { CallerDoesNormalLinking, CallerLinksManually };
    template<typename ChecksFunctor>
    bool attemptToInlineCall(Node* callTargetNode, int resultOperand, CallVariant, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind, CallerLinkability, SpeculatedType prediction, unsigned& inliningBalance, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    void inlineCall(Node* callTargetNode, int resultOperand, CallVariant, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind, CallerLinkability, const ChecksFunctor& insertChecks);
    void cancelLinkingForBlock(InlineStackEntry*, BasicBlock*); // Only works when the given block is the last one to have been added for that inline stack entry.
    // Handle intrinsic functions. Return true if it succeeded, false if we need to plant a call.
    template<typename ChecksFunctor>
    bool handleIntrinsic(int resultOperand, Intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleTypedArrayConstructor(int resultOperand, InternalFunction*, int registerOffset, int argumentCountIncludingThis, TypedArrayType, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleConstantInternalFunction(int resultOperand, InternalFunction*, int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind, const ChecksFunctor& insertChecks);
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

    void prepareToParseBlock();
    void clearCaches();

    // Parse a single basic block of bytecode instructions.
    bool parseBlock(unsigned limit);
    // Link block successors.
    void linkBlock(BasicBlock*, Vector<BasicBlock*>& possibleTargets);
    void linkBlocks(Vector<UnlinkedBlock>& unlinkedBlocks, Vector<BasicBlock*>& possibleTargets);
    
    VariableAccessData* newVariableAccessData(VirtualRegister operand)
    {
        ASSERT(!operand.isConstant());
        
        m_graph.m_variableAccessData.append(VariableAccessData(operand));
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
                const CodeBlock& codeBlock = *m_inlineStackTop->m_codeBlock;
                JSValue value = codeBlock.getConstant(operand.offset());
                SourceCodeRepresentation sourceCodeRepresentation = codeBlock.constantSourceCodeRepresentation(operand.offset());
                if (constantIndex >= oldSize) {
                    m_constants.grow(constantIndex + 1);
                    for (unsigned i = oldSize; i < m_constants.size(); ++i)
                        m_constants[i] = nullptr;
                }

                Node* constantNode = nullptr;
                if (sourceCodeRepresentation == SourceCodeRepresentation::Double)
                    constantNode = addToGraph(DoubleConstant, OpInfo(m_graph.freezeStrong(jsDoubleNumber(value.asNumber()))));
                else
                    constantNode = addToGraph(JSConstant, OpInfo(m_graph.freezeStrong(value)));
                m_constants[constantIndex] = constantNode;
            }
            ASSERT(m_constants[constantIndex]);
            return m_constants[constantIndex];
        }
        
        if (inlineCallFrame()) {
            if (!inlineCallFrame()->isClosureCall) {
                JSFunction* callee = inlineCallFrame()->calleeConstant();
                if (operand.offset() == JSStack::Callee)
                    return weakJSConstant(callee);
            }
        } else if (operand.offset() == JSStack::Callee) {
            // We have to do some constant-folding here because this enables CreateThis folding. Note
            // that we don't have such watchpoint-based folding for inlined uses of Callee, since in that
            // case if the function is a singleton then we already know it.
            if (FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(m_codeBlock->ownerExecutable())) {
                InferredValue* singleton = executable->singletonFunction();
                if (JSValue value = singleton->inferredValue()) {
                    m_graph.watchpoints().addLazily(singleton);
                    JSFunction* function = jsCast<JSFunction*>(value);
                    return weakJSConstant(function);
                }
            }
            return addToGraph(GetCallee);
        }
        
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

        DelayedSetLocal delayed(currentCodeOrigin(), operand, value);
        
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

        Node* node = m_currentBlock->variablesAtTail.local(local);
        
        // This has two goals: 1) link together variable access datas, and 2)
        // try to avoid creating redundant GetLocals. (1) is required for
        // correctness - no other phase will ensure that block-local variable
        // access data unification is done correctly. (2) is purely opportunistic
        // and is meant as an compile-time optimization only.
        
        VariableAccessData* variable;
        
        if (node) {
            variable = node->variableAccessData();
            
            switch (node->op()) {
            case GetLocal:
                return node;
            case SetLocal:
                return node->child1().node();
            default:
                break;
            }
        } else
            variable = newVariableAccessData(operand);
        
        node = injectLazyOperandSpeculation(addToGraph(GetLocal, OpInfo(variable)));
        m_currentBlock->variablesAtTail.local(local) = node;
        return node;
    }

    Node* setLocal(const CodeOrigin& semanticOrigin, VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        CodeOrigin oldSemanticOrigin = m_currentSemanticOrigin;
        m_currentSemanticOrigin = semanticOrigin;

        unsigned local = operand.toLocal();
        
        if (setMode != ImmediateNakedSet) {
            ArgumentPosition* argumentPosition = findArgumentPositionForLocal(operand);
            if (argumentPosition)
                flushDirect(operand, argumentPosition);
            else if (m_hasDebuggerEnabled && operand == m_codeBlock->scopeRegister())
                flush(operand);
        }

        VariableAccessData* variableAccessData = newVariableAccessData(operand);
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadIndexingType));
        Node* node = addToGraph(SetLocal, OpInfo(variableAccessData), value);
        m_currentBlock->variablesAtTail.local(local) = node;

        m_currentSemanticOrigin = oldSemanticOrigin;
        return node;
    }

    // Used in implementing get/set, above, where the operand is an argument.
    Node* getArgument(VirtualRegister operand)
    {
        unsigned argument = operand.toArgument();
        ASSERT(argument < m_numArguments);
        
        Node* node = m_currentBlock->variablesAtTail.argument(argument);

        VariableAccessData* variable;
        
        if (node) {
            variable = node->variableAccessData();
            
            switch (node->op()) {
            case GetLocal:
                return node;
            case SetLocal:
                return node->child1().node();
            default:
                break;
            }
        } else
            variable = newVariableAccessData(operand);
        
        node = injectLazyOperandSpeculation(addToGraph(GetLocal, OpInfo(variable)));
        m_currentBlock->variablesAtTail.argument(argument) = node;
        return node;
    }
    Node* setArgument(const CodeOrigin& semanticOrigin, VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        CodeOrigin oldSemanticOrigin = m_currentSemanticOrigin;
        m_currentSemanticOrigin = semanticOrigin;

        unsigned argument = operand.toArgument();
        ASSERT(argument < m_numArguments);
        
        VariableAccessData* variableAccessData = newVariableAccessData(operand);

        // Always flush arguments, except for 'this'. If 'this' is created by us,
        // then make sure that it's never unboxed.
        if (argument) {
            if (setMode != ImmediateNakedSet)
                flushDirect(operand);
        } else if (m_codeBlock->specializationKind() == CodeForConstruct)
            variableAccessData->mergeShouldNeverUnbox(true);
        
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadIndexingType));
        Node* node = addToGraph(SetLocal, OpInfo(variableAccessData), value);
        m_currentBlock->variablesAtTail.argument(argument) = node;

        m_currentSemanticOrigin = oldSemanticOrigin;
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
        ASSERT(!operand.isConstant());
        
        Node* node = m_currentBlock->variablesAtTail.operand(operand);
        
        VariableAccessData* variable;
        
        if (node)
            variable = node->variableAccessData();
        else
            variable = newVariableAccessData(operand);
        
        node = addToGraph(Flush, OpInfo(variable));
        m_currentBlock->variablesAtTail.operand(operand) = node;
        if (argumentPosition)
            argumentPosition->addVariable(variable);
    }
    
    void flush(InlineStackEntry* inlineStackEntry)
    {
        int numArguments;
        if (InlineCallFrame* inlineCallFrame = inlineStackEntry->m_inlineCallFrame) {
            ASSERT(!m_hasDebuggerEnabled);
            numArguments = inlineCallFrame->arguments.size();
            if (inlineCallFrame->isClosureCall)
                flushDirect(inlineStackEntry->remapOperand(VirtualRegister(JSStack::Callee)));
            if (inlineCallFrame->isVarargs())
                flushDirect(inlineStackEntry->remapOperand(VirtualRegister(JSStack::ArgumentCount)));
        } else
            numArguments = inlineStackEntry->m_codeBlock->numParameters();
        for (unsigned argument = numArguments; argument-- > 1;)
            flushDirect(inlineStackEntry->remapOperand(virtualRegisterForArgument(argument)));
        if (m_hasDebuggerEnabled)
            flush(m_codeBlock->scopeRegister());
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

    NodeOrigin currentNodeOrigin()
    {
        // FIXME: We should set the forExit origin only on those nodes that can exit.
        // https://bugs.webkit.org/show_bug.cgi?id=145204
        if (m_currentSemanticOrigin.isSet())
            return NodeOrigin(m_currentSemanticOrigin, currentCodeOrigin());
        return NodeOrigin(currentCodeOrigin());
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
    
    Node* addToGraph(Node* node)
    {
        if (Options::verboseDFGByteCodeParsing())
            dataLog("        appended ", node, " ", Graph::opName(node->op()), "\n");
        m_currentBlock->append(node);
        return node;
    }
    
    Node* addToGraph(NodeType op, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentNodeOrigin(), Edge(child1), Edge(child2),
            Edge(child3));
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, Edge child1, Edge child2 = Edge(), Edge child3 = Edge())
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentNodeOrigin(), child1, child2, child3);
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, OpInfo info, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentNodeOrigin(), info, Edge(child1), Edge(child2),
            Edge(child3));
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, OpInfo info1, OpInfo info2, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentNodeOrigin(), info1, info2,
            Edge(child1), Edge(child2), Edge(child3));
        return addToGraph(result);
    }
    
    Node* addToGraph(Node::VarArgTag, NodeType op, OpInfo info1, OpInfo info2)
    {
        Node* result = m_graph.addNode(
            SpecNone, Node::VarArg, op, currentNodeOrigin(), info1, info2,
            m_graph.m_varArgChildren.size() - m_numPassedVarArgs, m_numPassedVarArgs);
        addToGraph(result);
        
        m_numPassedVarArgs = 0;
        
        return result;
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

        for (int i = 0; i < argCount; ++i)
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
            set(resultReg, call);
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
        bool makeSafe = profile->outOfBounds(locker);
        return ArrayMode::fromObserved(locker, profile, action, makeSafe);
    }
    
    ArrayMode getArrayMode(ArrayProfile* profile)
    {
        return getArrayMode(profile, Array::Read);
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
    
    void noticeArgumentsUse()
    {
        // All of the arguments in this function need to be formatted as JSValues because we will
        // load from them in a random-access fashion and we don't want to have to switch on
        // format.
        
        for (ArgumentPosition* argument : m_inlineStackTop->m_argumentPositions)
            argument->mergeShouldNeverUnbox(true);
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
    // The semantic origin of the current node if different from the current Index.
    CodeOrigin m_currentSemanticOrigin;

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
        CodeOrigin m_origin;
        VirtualRegister m_operand;
        Node* m_value;
        
        DelayedSetLocal() { }
        DelayedSetLocal(const CodeOrigin& origin, VirtualRegister operand, Node* value)
            : m_origin(origin)
            , m_operand(operand)
            , m_value(value)
        {
        }
        
        Node* execute(ByteCodeParser* parser, SetMode setMode = NormalSet)
        {
            if (m_operand.isArgument())
                return parser->setArgument(m_origin, m_operand, m_value, setMode);
            return parser->setLocal(m_origin, m_operand, m_value, setMode);
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
    bool m_hasDebuggerEnabled;
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
    
    if (callTarget->isCellConstant())
        callLinkStatus.setProvenConstantCallee(CallVariant(callTarget->asCell()));
    
    if (Options::verboseDFGByteCodeParsing())
        dataLog("    Handling call at ", currentCodeOrigin(), ": ", callLinkStatus, "\n");
    
    if (!callLinkStatus.canOptimize()) {
        // Oddly, this conflates calls that haven't executed with calls that behaved sufficiently polymorphically
        // that we cannot optimize them.
        
        addCall(result, op, OpInfo(), callTarget, argumentCountIncludingThis, registerOffset, prediction);
        return;
    }
    
    unsigned nextOffset = m_currentIndex + instructionSize;
    
    OpInfo callOpInfo;
    
    if (handleInlining(callTarget, result, callLinkStatus, registerOffset, virtualRegisterForArgument(0, registerOffset), VirtualRegister(), 0, argumentCountIncludingThis, nextOffset, op, kind, prediction)) {
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedCall();
        return;
    }
    
#if ENABLE(FTL_NATIVE_CALL_INLINING)
    if (isFTL(m_graph.m_plan.mode) && Options::optimizeNativeCalls() && callLinkStatus.size() == 1 && !callLinkStatus.couldTakeSlowPath()) {
        CallVariant callee = callLinkStatus[0];
        JSFunction* function = callee.function();
        CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
        if (function && function->isHostFunction()) {
            emitFunctionChecks(callee, callTarget, virtualRegisterForArgument(0, registerOffset));
            callOpInfo = OpInfo(m_graph.freeze(function));

            if (op == Call)
                op = NativeCall;
            else {
                ASSERT(op == Construct);
                op = NativeConstruct;
            }
        }
    }
#endif
    
    addCall(result, op, callOpInfo, callTarget, argumentCountIncludingThis, registerOffset, prediction);
}

void ByteCodeParser::handleVarargsCall(Instruction* pc, NodeType op, CodeSpecializationKind kind)
{
    ASSERT(OPCODE_LENGTH(op_call_varargs) == OPCODE_LENGTH(op_construct_varargs));
    
    int result = pc[1].u.operand;
    int callee = pc[2].u.operand;
    int thisReg = pc[3].u.operand;
    int arguments = pc[4].u.operand;
    int firstFreeReg = pc[5].u.operand;
    int firstVarArgOffset = pc[6].u.operand;
    
    SpeculatedType prediction = getPrediction();
    
    Node* callTarget = get(VirtualRegister(callee));
    
    CallLinkStatus callLinkStatus = CallLinkStatus::computeFor(
        m_inlineStackTop->m_profiledBlock, currentCodeOrigin(),
        m_inlineStackTop->m_callLinkInfos, m_callContextMap);
    if (callTarget->isCellConstant())
        callLinkStatus.setProvenConstantCallee(CallVariant(callTarget->asCell()));
    
    if (Options::verboseDFGByteCodeParsing())
        dataLog("    Varargs call link status at ", currentCodeOrigin(), ": ", callLinkStatus, "\n");
    
    if (callLinkStatus.canOptimize()
        && handleInlining(callTarget, result, callLinkStatus, firstFreeReg, VirtualRegister(thisReg), VirtualRegister(arguments), firstVarArgOffset, 0, m_currentIndex + OPCODE_LENGTH(op_call_varargs), op, InlineCallFrame::varargsKindFor(kind), prediction)) {
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedCall();
        return;
    }
    
    CallVarargsData* data = m_graph.m_callVarargsData.add();
    data->firstVarArgOffset = firstVarArgOffset;
    
    Node* thisChild = get(VirtualRegister(thisReg));
    
    Node* call = addToGraph(op, OpInfo(data), OpInfo(prediction), callTarget, get(VirtualRegister(arguments)), thisChild);
    VirtualRegister resultReg(result);
    if (resultReg.isValid())
        set(resultReg, call);
}

void ByteCodeParser::emitFunctionChecks(CallVariant callee, Node* callTarget, VirtualRegister thisArgumentReg)
{
    Node* thisArgument;
    if (thisArgumentReg.isValid())
        thisArgument = get(thisArgumentReg);
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

void ByteCodeParser::emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis)
{
    for (int i = 0; i < argumentCountIncludingThis; ++i)
        addToGraph(Phantom, get(virtualRegisterForArgument(i, registerOffset)));
}

unsigned ByteCodeParser::inliningCost(CallVariant callee, int argumentCountIncludingThis, CodeSpecializationKind kind)
{
    if (verbose)
        dataLog("Considering inlining ", callee, " into ", currentCodeOrigin(), "\n");
    
    if (m_hasDebuggerEnabled) {
        if (verbose)
            dataLog("    Failing because the debugger is in use.\n");
        return UINT_MAX;
    }

    FunctionExecutable* executable = callee.functionExecutable();
    if (!executable) {
        if (verbose)
            dataLog("    Failing because there is no function executable.\n");
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
    // being an inline candidate? We might not have a code block (1) if code was thrown away,
    // (2) if we simply hadn't actually made this call yet or (3) code is a builtin function and
    // specialization kind is construct. In the former 2 cases, we could still theoretically attempt
    // to inline it if we had a static proof of what was being called; this might happen for example
    // if you call a global function, where watchpointing gives us static information. Overall,
    // it's a rare case because we expect that any hot callees would have already been compiled.
    CodeBlock* codeBlock = executable->baselineCodeBlockFor(kind);
    if (!codeBlock) {
        if (verbose)
            dataLog("    Failing because no code block available.\n");
        return UINT_MAX;
    }
    CapabilityLevel capabilityLevel = inlineFunctionForCapabilityLevel(
        codeBlock, kind, callee.isClosureCall());
    if (verbose) {
        dataLog("    Kind: ", kind, "\n");
        dataLog("    Is closure call: ", callee.isClosureCall(), "\n");
        dataLog("    Capability level: ", capabilityLevel, "\n");
        dataLog("    Might inline function: ", mightInlineFunctionFor(codeBlock, kind), "\n");
        dataLog("    Might compile function: ", mightCompileFunctionFor(codeBlock, kind), "\n");
        dataLog("    Is supported for inlining: ", isSupportedForInlining(codeBlock), "\n");
        dataLog("    Needs activation: ", codeBlock->ownerExecutable()->needsActivation(), "\n");
        dataLog("    Is inlining candidate: ", codeBlock->ownerExecutable()->isInliningCandidate(), "\n");
    }
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
    
    // FIXME: We currently inline functions that have run in LLInt but not in Baseline. These
    // functions have very low fidelity profiling, and presumably they weren't very hot if they
    // haven't gotten to Baseline yet. Consider not inlining these functions.
    // https://bugs.webkit.org/show_bug.cgi?id=145503
    
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

template<typename ChecksFunctor>
void ByteCodeParser::inlineCall(Node* callTargetNode, int resultOperand, CallVariant callee, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind kind, CallerLinkability callerLinkability, const ChecksFunctor& insertChecks)
{
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    
    ASSERT(inliningCost(callee, argumentCountIncludingThis, specializationKind) != UINT_MAX);
    
    CodeBlock* codeBlock = callee.functionExecutable()->baselineCodeBlockFor(specializationKind);
    insertChecks(codeBlock);

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
        
        calleeVariable->mergeShouldNeverUnbox(true);
        
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
        if (Options::verboseDFGByteCodeParsing())
            dataLog("    Allowing parsing to continue in last inlined block.\n");
        
        ASSERT(lastBlock->isEmpty() || !lastBlock->terminal());
        
        // If we created new blocks then the last block needs linking, but in the
        // caller. It doesn't need to be linked to, but it needs outgoing links.
        if (!inlineStackEntry.m_unlinkedBlocks.isEmpty()) {
            // For debugging purposes, set the bytecodeBegin. Note that this doesn't matter
            // for release builds because this block will never serve as a potential target
            // in the linker's binary search.
            if (Options::verboseDFGByteCodeParsing())
                dataLog("        Repurposing last block from ", lastBlock->bytecodeBegin, " to ", m_currentIndex, "\n");
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
    
    if (Options::verboseDFGByteCodeParsing())
        dataLog("    Creating new block after inlining.\n");

    // If we get to this point then all blocks must end in some sort of terminals.
    ASSERT(lastBlock->terminal());

    // Need to create a new basic block for the continuation at the caller.
    RefPtr<BasicBlock> block = adoptRef(new BasicBlock(nextOffset, m_numArguments, m_numLocals, PNaN));

    // Link the early returns to the basic block we're about to create.
    for (size_t i = 0; i < inlineStackEntry.m_unlinkedBlocks.size(); ++i) {
        if (!inlineStackEntry.m_unlinkedBlocks[i].m_needsEarlyReturnLinking)
            continue;
        BasicBlock* blockToLink = inlineStackEntry.m_unlinkedBlocks[i].m_block;
        ASSERT(!blockToLink->isLinked);
        Node* node = blockToLink->terminal();
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

template<typename ChecksFunctor>
bool ByteCodeParser::attemptToInlineCall(Node* callTargetNode, int resultOperand, CallVariant callee, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind kind, CallerLinkability callerLinkability, SpeculatedType prediction, unsigned& inliningBalance, const ChecksFunctor& insertChecks)
{
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    
    if (!inliningBalance)
        return false;
    
    bool didInsertChecks = false;
    auto insertChecksWithAccounting = [&] () {
        insertChecks(nullptr);
        didInsertChecks = true;
    };
    
    if (verbose)
        dataLog("    Considering callee ", callee, "\n");
    
    // Intrinsics and internal functions can only be inlined if we're not doing varargs. This is because
    // we currently don't have any way of getting profiling information for arguments to non-JS varargs
    // calls. The prediction propagator won't be of any help because LoadVarargs obscures the data flow,
    // and there are no callsite value profiles and native function won't have callee value profiles for
    // those arguments. Even worse, if the intrinsic decides to exit, it won't really have anywhere to
    // exit to: LoadVarargs is effectful and it's part of the op_call_varargs, so we can't exit without
    // calling LoadVarargs twice.
    if (!InlineCallFrame::isVarargs(kind)) {
        if (InternalFunction* function = callee.internalFunction()) {
            if (handleConstantInternalFunction(resultOperand, function, registerOffset, argumentCountIncludingThis, specializationKind, insertChecksWithAccounting)) {
                RELEASE_ASSERT(didInsertChecks);
                addToGraph(Phantom, callTargetNode);
                emitArgumentPhantoms(registerOffset, argumentCountIncludingThis);
                inliningBalance--;
                return true;
            }
            RELEASE_ASSERT(!didInsertChecks);
            return false;
        }
    
        Intrinsic intrinsic = callee.intrinsicFor(specializationKind);
        if (intrinsic != NoIntrinsic) {
            if (handleIntrinsic(resultOperand, intrinsic, registerOffset, argumentCountIncludingThis, prediction, insertChecksWithAccounting)) {
                RELEASE_ASSERT(didInsertChecks);
                addToGraph(Phantom, callTargetNode);
                emitArgumentPhantoms(registerOffset, argumentCountIncludingThis);
                inliningBalance--;
                return true;
            }
            RELEASE_ASSERT(!didInsertChecks);
            return false;
        }
    }
    
    unsigned myInliningCost = inliningCost(callee, argumentCountIncludingThis, specializationKind);
    if (myInliningCost > inliningBalance)
        return false;

    Instruction* savedCurrentInstruction = m_currentInstruction;
    inlineCall(callTargetNode, resultOperand, callee, registerOffset, argumentCountIncludingThis, nextOffset, kind, callerLinkability, insertChecks);
    inliningBalance -= myInliningCost;
    m_currentInstruction = savedCurrentInstruction;
    return true;
}

bool ByteCodeParser::handleInlining(
    Node* callTargetNode, int resultOperand, const CallLinkStatus& callLinkStatus,
    int registerOffsetOrFirstFreeReg, VirtualRegister thisArgument,
    VirtualRegister argumentsArgument, unsigned argumentsOffset, int argumentCountIncludingThis,
    unsigned nextOffset, NodeType callOp, InlineCallFrame::Kind kind, SpeculatedType prediction)
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
    
    if (InlineCallFrame::isVarargs(kind)
        && callLinkStatus.maxNumArguments() > Options::maximumVarargsForInlining()) {
        if (verbose)
            dataLog("Bailing inlining because of varargs.\n");
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
        int registerOffset;
        
        // Only used for varargs calls.
        unsigned mandatoryMinimum = 0;
        unsigned maxNumArguments = 0;

        if (InlineCallFrame::isVarargs(kind)) {
            if (FunctionExecutable* functionExecutable = callLinkStatus[0].functionExecutable())
                mandatoryMinimum = functionExecutable->parameterCount();
            else
                mandatoryMinimum = 0;
            
            // includes "this"
            maxNumArguments = std::max(
                callLinkStatus.maxNumArguments(),
                mandatoryMinimum + 1);
            
            // We sort of pretend that this *is* the number of arguments that were passed.
            argumentCountIncludingThis = maxNumArguments;
            
            registerOffset = registerOffsetOrFirstFreeReg + 1;
            registerOffset -= maxNumArguments; // includes "this"
            registerOffset -= JSStack::CallFrameHeaderSize;
            registerOffset = -WTF::roundUpToMultipleOf(
                stackAlignmentRegisters(),
                -registerOffset);
        } else
            registerOffset = registerOffsetOrFirstFreeReg;
        
        bool result = attemptToInlineCall(
            callTargetNode, resultOperand, callLinkStatus[0], registerOffset,
            argumentCountIncludingThis, nextOffset, kind, CallerDoesNormalLinking, prediction,
            inliningBalance, [&] (CodeBlock* codeBlock) {
                emitFunctionChecks(callLinkStatus[0], callTargetNode, thisArgument);

                // If we have a varargs call, we want to extract the arguments right now.
                if (InlineCallFrame::isVarargs(kind)) {
                    int remappedRegisterOffset =
                        m_inlineStackTop->remapOperand(VirtualRegister(registerOffset)).offset();
                    
                    ensureLocals(VirtualRegister(remappedRegisterOffset).toLocal());
                    
                    int argumentStart = registerOffset + JSStack::CallFrameHeaderSize;
                    int remappedArgumentStart =
                        m_inlineStackTop->remapOperand(VirtualRegister(argumentStart)).offset();

                    LoadVarargsData* data = m_graph.m_loadVarargsData.add();
                    data->start = VirtualRegister(remappedArgumentStart + 1);
                    data->count = VirtualRegister(remappedRegisterOffset + JSStack::ArgumentCount);
                    data->offset = argumentsOffset;
                    data->limit = maxNumArguments;
                    data->mandatoryMinimum = mandatoryMinimum;
            
                    addToGraph(LoadVarargs, OpInfo(data), get(argumentsArgument));

                    // LoadVarargs may OSR exit. Hence, we need to keep alive callTargetNode, thisArgument
                    // and argumentsArgument for the baseline JIT. However, we only need a Phantom for
                    // callTargetNode because the other 2 are still in use and alive at this point.
                    addToGraph(Phantom, callTargetNode);

                    // In DFG IR before SSA, we cannot insert control flow between after the
                    // LoadVarargs and the last SetArgument. This isn't a problem once we get to DFG
                    // SSA. Fortunately, we also have other reasons for not inserting control flow
                    // before SSA.
            
                    VariableAccessData* countVariable = newVariableAccessData(
                        VirtualRegister(remappedRegisterOffset + JSStack::ArgumentCount));
                    // This is pretty lame, but it will force the count to be flushed as an int. This doesn't
                    // matter very much, since our use of a SetArgument and Flushes for this local slot is
                    // mostly just a formality.
                    countVariable->predict(SpecInt32);
                    countVariable->mergeIsProfitableToUnbox(true);
                    Node* setArgumentCount = addToGraph(SetArgument, OpInfo(countVariable));
                    m_currentBlock->variablesAtTail.setOperand(countVariable->local(), setArgumentCount);

                    set(VirtualRegister(argumentStart), get(thisArgument), ImmediateNakedSet);
                    for (unsigned argument = 1; argument < maxNumArguments; ++argument) {
                        VariableAccessData* variable = newVariableAccessData(
                            VirtualRegister(remappedArgumentStart + argument));
                        variable->mergeShouldNeverUnbox(true); // We currently have nowhere to put the type check on the LoadVarargs. LoadVarargs is effectful, so after it finishes, we cannot exit.
                        
                        // For a while it had been my intention to do things like this inside the
                        // prediction injection phase. But in this case it's really best to do it here,
                        // because it's here that we have access to the variable access datas for the
                        // inlining we're about to do.
                        //
                        // Something else that's interesting here is that we'd really love to get
                        // predictions from the arguments loaded at the callsite, rather than the
                        // arguments received inside the callee. But that probably won't matter for most
                        // calls.
                        if (codeBlock && argument < static_cast<unsigned>(codeBlock->numParameters())) {
                            ConcurrentJITLocker locker(codeBlock->m_lock);
                            if (ValueProfile* profile = codeBlock->valueProfileForArgument(argument))
                                variable->predict(profile->computeUpdatedPrediction(locker));
                        }
                        
                        Node* setArgument = addToGraph(SetArgument, OpInfo(variable));
                        m_currentBlock->variablesAtTail.setOperand(variable->local(), setArgument);
                    }
                }
            });
        if (verbose) {
            dataLog("Done inlining (simple).\n");
            dataLog("Stack: ", currentCodeOrigin(), "\n");
            dataLog("Result: ", result, "\n");
        }
        return result;
    }
    
    // We need to create some kind of switch over callee. For now we only do this if we believe that
    // we're in the top tier. We have two reasons for this: first, it provides us an opportunity to
    // do more detailed polyvariant/polymorphic profiling; and second, it reduces compile times in
    // the DFG. And by polyvariant profiling we mean polyvariant profiling of *this* call. Note that
    // we could improve that aspect of this by doing polymorphic inlining but having the profiling
    // also.
    if (!isFTL(m_graph.m_plan.mode) || !Options::enablePolymorphicCallInlining()
        || InlineCallFrame::isVarargs(kind)) {
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
        if (callLinkStatus[i].isClosureCall())
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
        // where it would be beneficial. It might be best to handle these cases as if all calls were
        // closure calls.
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
    
    int registerOffset = registerOffsetOrFirstFreeReg;
    
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
    
    // We may force this true if we give up on inlining any of the edges.
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
            myCallTargetNode, resultOperand, callLinkStatus[i], registerOffset,
            argumentCountIncludingThis, nextOffset, kind, CallerLinksManually, prediction,
            inliningBalance, [&] (CodeBlock*) { });
        
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
            thingToCaseOn = callLinkStatus[i].nonExecutableCallee();
        else {
            ASSERT(allAreClosureCalls);
            thingToCaseOn = callLinkStatus[i].executable();
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
            dataLog("Finished inlining ", callLinkStatus[i], " at ", currentCodeOrigin(), ".\n");
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
        emitArgumentPhantoms(registerOffset, argumentCountIncludingThis);
        
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
        landingBlocks[i]->terminal()->targetBlock() = continuationBlock.get();
    
    m_currentIndex = oldOffset;
    
    if (verbose) {
        dataLog("Done inlining (hard).\n");
        dataLog("Stack: ", currentCodeOrigin(), "\n");
    }
    return true;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleMinMax(int resultOperand, NodeType op, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& insertChecks)
{
    if (argumentCountIncludingThis == 1) { // Math.min()
        insertChecks();
        set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
        return true;
    }
     
    if (argumentCountIncludingThis == 2) { // Math.min(x)
        insertChecks();
        Node* result = get(VirtualRegister(virtualRegisterForArgument(1, registerOffset)));
        addToGraph(Phantom, Edge(result, NumberUse));
        set(VirtualRegister(resultOperand), result);
        return true;
    }
    
    if (argumentCountIncludingThis == 3) { // Math.min(x, y)
        insertChecks();
        set(VirtualRegister(resultOperand), addToGraph(op, get(virtualRegisterForArgument(1, registerOffset)), get(virtualRegisterForArgument(2, registerOffset))));
        return true;
    }
    
    // Don't handle >=3 arguments for now.
    return false;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleIntrinsic(int resultOperand, Intrinsic intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks)
{
    switch (intrinsic) {
    case AbsIntrinsic: {
        if (argumentCountIncludingThis == 1) { // Math.abs()
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }

        if (!MacroAssembler::supportsFloatingPointAbs())
            return false;

        insertChecks();
        Node* node = addToGraph(ArithAbs, get(virtualRegisterForArgument(1, registerOffset)));
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInDFG);
        set(VirtualRegister(resultOperand), node);
        return true;
    }

    case MinIntrinsic:
        return handleMinMax(resultOperand, ArithMin, registerOffset, argumentCountIncludingThis, insertChecks);
        
    case MaxIntrinsic:
        return handleMinMax(resultOperand, ArithMax, registerOffset, argumentCountIncludingThis, insertChecks);

    case SqrtIntrinsic:
    case CosIntrinsic:
    case SinIntrinsic:
    case LogIntrinsic: {
        if (argumentCountIncludingThis == 1) {
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }
        
        switch (intrinsic) {
        case SqrtIntrinsic:
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(ArithSqrt, get(virtualRegisterForArgument(1, registerOffset))));
            return true;
            
        case CosIntrinsic:
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(ArithCos, get(virtualRegisterForArgument(1, registerOffset))));
            return true;
            
        case SinIntrinsic:
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(ArithSin, get(virtualRegisterForArgument(1, registerOffset))));
            return true;

        case LogIntrinsic:
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(ArithLog, get(virtualRegisterForArgument(1, registerOffset))));
            return true;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }

    case PowIntrinsic: {
        if (argumentCountIncludingThis < 3) {
            // Math.pow() and Math.pow(x) return NaN.
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }
        insertChecks();
        VirtualRegister xOperand = virtualRegisterForArgument(1, registerOffset);
        VirtualRegister yOperand = virtualRegisterForArgument(2, registerOffset);
        set(VirtualRegister(resultOperand), addToGraph(ArithPow, get(xOperand), get(yOperand)));
        return true;
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
            insertChecks();
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
            insertChecks();
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

        insertChecks();
        VirtualRegister thisOperand = virtualRegisterForArgument(0, registerOffset);
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringCharCodeAt, OpInfo(ArrayMode(Array::String).asWord()), get(thisOperand), get(indexOperand));

        set(VirtualRegister(resultOperand), charCode);
        return true;
    }

    case CharAtIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        VirtualRegister thisOperand = virtualRegisterForArgument(0, registerOffset);
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringCharAt, OpInfo(ArrayMode(Array::String).asWord()), get(thisOperand), get(indexOperand));

        set(VirtualRegister(resultOperand), charCode);
        return true;
    }
    case Clz32Intrinsic: {
        insertChecks();
        if (argumentCountIncludingThis == 1)
            set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_graph.freeze(jsNumber(32)))));
        else {
            Node* operand = get(virtualRegisterForArgument(1, registerOffset));
            set(VirtualRegister(resultOperand), addToGraph(ArithClz32, operand));
        }
        return true;
    }
    case FromCharCodeIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringFromCharCode, get(indexOperand));

        set(VirtualRegister(resultOperand), charCode);

        return true;
    }

    case RegExpExecIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        insertChecks();
        Node* regExpExec = addToGraph(RegExpExec, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)));
        set(VirtualRegister(resultOperand), regExpExec);
        
        return true;
    }
        
    case RegExpTestIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        insertChecks();
        Node* regExpExec = addToGraph(RegExpTest, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)));
        set(VirtualRegister(resultOperand), regExpExec);
        
        return true;
    }
    case RoundIntrinsic: {
        if (argumentCountIncludingThis == 1) {
            insertChecks();
            set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }
        if (argumentCountIncludingThis == 2) {
            insertChecks();
            Node* operand = get(virtualRegisterForArgument(1, registerOffset));
            Node* roundNode = addToGraph(ArithRound, OpInfo(0), OpInfo(prediction), operand);
            set(VirtualRegister(resultOperand), roundNode);
            return true;
        }
        return false;
    }
    case IMulIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;
        insertChecks();
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
        insertChecks();
        VirtualRegister operand = virtualRegisterForArgument(1, registerOffset);
        set(VirtualRegister(resultOperand), addToGraph(ArithFRound, get(operand)));
        return true;
    }
        
    case DFGTrueIntrinsic: {
        insertChecks();
        set(VirtualRegister(resultOperand), jsConstant(jsBoolean(true)));
        return true;
    }
        
    case OSRExitIntrinsic: {
        insertChecks();
        addToGraph(ForceOSRExit);
        set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantUndefined)));
        return true;
    }
        
    case IsFinalTierIntrinsic: {
        insertChecks();
        set(VirtualRegister(resultOperand),
            jsConstant(jsBoolean(Options::useFTLJIT() ? isFTL(m_graph.m_plan.mode) : true)));
        return true;
    }
        
    case SetInt32HeapPredictionIntrinsic: {
        insertChecks();
        for (int i = 1; i < argumentCountIncludingThis; ++i) {
            Node* node = get(virtualRegisterForArgument(i, registerOffset));
            if (node->hasHeapPrediction())
                node->setHeapPrediction(SpecInt32);
        }
        set(VirtualRegister(resultOperand), addToGraph(JSConstant, OpInfo(m_constantUndefined)));
        return true;
    }
        
    case CheckInt32Intrinsic: {
        insertChecks();
        for (int i = 1; i < argumentCountIncludingThis; ++i) {
            Node* node = get(virtualRegisterForArgument(i, registerOffset));
            addToGraph(Phantom, Edge(node, Int32Use));
        }
        set(VirtualRegister(resultOperand), jsConstant(jsBoolean(true)));
        return true;
    }
        
    case FiatInt52Intrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        insertChecks();
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

template<typename ChecksFunctor>
bool ByteCodeParser::handleTypedArrayConstructor(
    int resultOperand, InternalFunction* function, int registerOffset,
    int argumentCountIncludingThis, TypedArrayType type, const ChecksFunctor& insertChecks)
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

    insertChecks();
    set(VirtualRegister(resultOperand),
        addToGraph(NewTypedArray, OpInfo(type), get(virtualRegisterForArgument(1, registerOffset))));
    return true;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleConstantInternalFunction(
    int resultOperand, InternalFunction* function, int registerOffset,
    int argumentCountIncludingThis, CodeSpecializationKind kind, const ChecksFunctor& insertChecks)
{
    if (verbose)
        dataLog("    Handling constant internal function ", JSValue(function), "\n");
    
    // If we ever find that we have a lot of internal functions that we specialize for,
    // then we should probably have some sort of hashtable dispatch, or maybe even
    // dispatch straight through the MethodTable of the InternalFunction. But for now,
    // it seems that this case is hit infrequently enough, and the number of functions
    // we know about is small enough, that having just a linear cascade of if statements
    // is good enough.
    
    if (function->classInfo() == ArrayConstructor::info()) {
        if (function->globalObject() != m_inlineStackTop->m_codeBlock->globalObject())
            return false;
        
        insertChecks();
        if (argumentCountIncludingThis == 2) {
            set(VirtualRegister(resultOperand),
                addToGraph(NewArrayWithSize, OpInfo(ArrayWithUndecided), get(virtualRegisterForArgument(1, registerOffset))));
            return true;
        }
        
        // FIXME: Array constructor should use "this" as newTarget.
        for (int i = 1; i < argumentCountIncludingThis; ++i)
            addVarArgChild(get(virtualRegisterForArgument(i, registerOffset)));
        set(VirtualRegister(resultOperand),
            addToGraph(Node::VarArg, NewArray, OpInfo(ArrayWithUndecided), OpInfo(0)));
        return true;
    }
    
    if (function->classInfo() == StringConstructor::info()) {
        insertChecks();
        
        Node* result;
        
        if (argumentCountIncludingThis <= 1)
            result = jsConstant(m_vm->smallStrings.emptyString());
        else
            result = addToGraph(CallStringConstructor, get(virtualRegisterForArgument(1, registerOffset)));
        
        if (kind == CodeForConstruct)
            result = addToGraph(NewStringObject, OpInfo(function->globalObject()->stringObjectStructure()), result);
        
        set(VirtualRegister(resultOperand), result);
        return true;
    }
    
    for (unsigned typeIndex = 0; typeIndex < NUMBER_OF_TYPED_ARRAY_TYPES; ++typeIndex) {
        bool result = handleTypedArrayConstructor(
            resultOperand, function, registerOffset, argumentCountIncludingThis,
            indexToTypedArrayType(typeIndex), insertChecks);
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
    
    StorageAccessData* data = m_graph.m_storageAccessData.add();
    data->offset = offset;
    data->identifierNumber = identifierNumber;
    
    Node* getByOffset = addToGraph(op, OpInfo(data), OpInfo(prediction), propertyStorage, base);

    return getByOffset;
}

Node* ByteCodeParser::handlePutByOffset(Node* base, unsigned identifier, PropertyOffset offset, Node* value)
{
    Node* propertyStorage;
    if (isInlineOffset(offset))
        propertyStorage = base;
    else
        propertyStorage = addToGraph(GetButterfly, base);
    
    StorageAccessData* data = m_graph.m_storageAccessData.add();
    data->offset = offset;
    data->identifierNumber = identifier;
    
    Node* result = addToGraph(PutByOffset, OpInfo(data), propertyStorage, base, value);
    
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
    
    if (!getByIdStatus.isSimple() || !getByIdStatus.numVariants() || !Options::enableAccessInlining()) {
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
    if (!putByIdStatus.isSimple() || !putByIdStatus.numVariants() || !Options::enableAccessInlining()) {
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

        StorageAccessData* data = m_graph.m_storageAccessData.add();
        data->offset = variant.offset();
        data->identifierNumber = identifierNumber;
        
        addToGraph(
            PutByOffset,
            OpInfo(data),
            propertyStorage,
            base,
            value);

        // FIXME: PutStructure goes last until we fix either
        // https://bugs.webkit.org/show_bug.cgi?id=142921 or
        // https://bugs.webkit.org/show_bug.cgi?id=142924.
        addToGraph(PutStructure, OpInfo(transition), base);

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
                virtualRegisterForArgument(argument));
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
            NEXT_OPCODE(op_enter);
        }
            
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

            JSFunction* function = callee->dynamicCastConstant<JSFunction*>();
            if (!function) {
                JSCell* cachedFunction = currentInstruction[4].u.jsCell.unvalidatedGet();
                if (cachedFunction
                    && cachedFunction != JSCell::seenMultipleCalleeObjects()
                    && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell)) {
                    ASSERT(cachedFunction->inherits(JSFunction::info()));

                    FrozenValue* frozen = m_graph.freeze(cachedFunction);
                    addToGraph(CheckCell, OpInfo(frozen), callee);
                    set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(JSConstant, OpInfo(frozen)));

                    function = static_cast<JSFunction*>(cachedFunction);
                }
            }

            bool alreadyEmitted = false;
            if (function) {
                if (FunctionRareData* rareData = function->rareData()) {
                    if (Structure* structure = rareData->allocationStructure()) {
                        m_graph.freeze(rareData);
                        m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                        // The callee is still live up to this point.
                        addToGraph(Phantom, callee);
                        set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(NewObject, OpInfo(structure)));
                        alreadyEmitted = true;
                    }
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

        case op_check_tdz: {
            Node* op = get(VirtualRegister(currentInstruction[1].u.operand));
            addToGraph(CheckNotEmpty, op);
            NEXT_OPCODE(op_check_tdz);
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

        case op_is_object_or_null: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(IsObjectOrNull, value));
            NEXT_OPCODE(op_is_object_or_null);
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
            ArrayMode arrayMode = getArrayMode(currentInstruction[4].u.arrayProfile, Array::Read);
            Node* property = get(VirtualRegister(currentInstruction[3].u.operand));
            Node* getByVal = addToGraph(GetByVal, OpInfo(arrayMode.asWord()), OpInfo(prediction), base, property);
            set(VirtualRegister(currentInstruction[1].u.operand), getByVal);

            NEXT_OPCODE(op_get_by_val);
        }

        case op_put_by_val_direct:
        case op_put_by_val: {
            Node* base = get(VirtualRegister(currentInstruction[1].u.operand));

            ArrayMode arrayMode = getArrayMode(currentInstruction[4].u.arrayProfile, Array::Write);
            
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
            
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
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
            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();
            addToGraph(
                PutGlobalVar,
                OpInfo(globalObject->assertVariableIsInThisObject(currentInstruction[1].u.variablePointer)),
                weakJSConstant(globalObject), value);
            NEXT_OPCODE(op_init_global_const);
        }

        case op_profile_type: {
            Node* valueToProfile = get(VirtualRegister(currentInstruction[1].u.operand));
            addToGraph(ProfileType, OpInfo(currentInstruction[2].u.location), valueToProfile);
            NEXT_OPCODE(op_profile_type);
        }

        case op_profile_control_flow: {
            BasicBlockLocation* basicBlockLocation = currentInstruction[1].u.basicBlockLocation;
            addToGraph(ProfileControlFlow, OpInfo(basicBlockLocation));
            NEXT_OPCODE(op_profile_control_flow);
        }

        // === Block terminators. ===

        case op_jmp: {
            int relativeOffset = currentInstruction[1].u.operand;
            addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
            if (relativeOffset <= 0)
                flushForTerminal();
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
            addToGraph(Switch, OpInfo(&data), get(VirtualRegister(currentInstruction[3].u.operand)));
            flushIfTerminal(data);
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
            addToGraph(Switch, OpInfo(&data), get(VirtualRegister(currentInstruction[3].u.operand)));
            flushIfTerminal(data);
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
            addToGraph(Switch, OpInfo(&data), get(VirtualRegister(currentInstruction[3].u.operand)));
            flushIfTerminal(data);
            LAST_OPCODE(op_switch_string);
        }

        case op_ret:
            if (inlineCallFrame()) {
                flushForReturn();
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
            flushForReturn();
            LAST_OPCODE(op_ret);
            
        case op_end:
            ASSERT(!inlineCallFrame());
            addToGraph(Return, get(VirtualRegister(currentInstruction[1].u.operand)));
            flushForReturn();
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
            // Verify that handleCall(), which could have inlined the callee, didn't trash m_currentInstruction
            ASSERT(m_currentInstruction == currentInstruction);
            NEXT_OPCODE(op_call);
            
        case op_construct:
            handleCall(currentInstruction, Construct, CodeForConstruct);
            NEXT_OPCODE(op_construct);
            
        case op_call_varargs: {
            handleVarargsCall(currentInstruction, CallVarargs, CodeForCall);
            NEXT_OPCODE(op_call_varargs);
        }
            
        case op_construct_varargs: {
            handleVarargsCall(currentInstruction, ConstructVarargs, CodeForConstruct);
            NEXT_OPCODE(op_construct_varargs);
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
            ResolveType resolveType = static_cast<ResolveType>(currentInstruction[4].u.operand);
            unsigned depth = currentInstruction[5].u.operand;

            // get_from_scope and put_to_scope depend on this watchpoint forcing OSR exit, so they don't add their own watchpoints.
            if (needsVarInjectionChecks(resolveType))
                addToGraph(VarInjectionWatchpoint);

            switch (resolveType) {
            case GlobalProperty:
            case GlobalVar:
            case GlobalPropertyWithVarInjectionChecks:
            case GlobalVarWithVarInjectionChecks:
                set(VirtualRegister(dst), weakJSConstant(m_inlineStackTop->m_codeBlock->globalObject()));
                if (resolveType == GlobalPropertyWithVarInjectionChecks || resolveType == GlobalVarWithVarInjectionChecks)
                    addToGraph(Phantom, getDirect(m_inlineStackTop->remapOperand(VirtualRegister(currentInstruction[2].u.operand))));
                break;
            case LocalClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* localBase = get(VirtualRegister(currentInstruction[2].u.operand));
                addToGraph(Phantom, localBase); // OSR exit cannot handle resolve_scope on a DCE'd scope.
                
                // We have various forms of constant folding here. This is necessary to avoid
                // spurious recompiles in dead-but-foldable code.
                if (SymbolTable* symbolTable = currentInstruction[6].u.symbolTable.get()) {
                    InferredValue* singleton = symbolTable->singletonScope();
                    if (JSValue value = singleton->inferredValue()) {
                        m_graph.watchpoints().addLazily(singleton);
                        set(VirtualRegister(dst), weakJSConstant(value));
                        break;
                    }
                }
                if (JSScope* scope = localBase->dynamicCastConstant<JSScope*>()) {
                    for (unsigned n = depth; n--;)
                        scope = scope->next();
                    set(VirtualRegister(dst), weakJSConstant(scope));
                    break;
                }
                for (unsigned n = depth; n--;)
                    localBase = addToGraph(SkipScope, localBase);
                set(VirtualRegister(dst), localBase);
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
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
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
                GetByIdStatus status = GetByIdStatus::computeFor(structure, uid);
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
                WatchpointSet* watchpointSet;
                ScopeOffset offset;
                {
                    ConcurrentJITLocker locker(globalObject->symbolTable()->m_lock);
                    SymbolTableEntry entry = globalObject->symbolTable()->get(locker, uid);
                    watchpointSet = entry.watchpointSet();
                    offset = entry.scopeOffset();
                }
                if (watchpointSet && watchpointSet->state() == IsWatched) {
                    // This has a fun concurrency story. There is the possibility of a race in two
                    // directions:
                    //
                    // We see that the set IsWatched, but in the meantime it gets invalidated: this is
                    // fine because if we saw that it IsWatched then we add a watchpoint. If it gets
                    // invalidated, then this compilation is invalidated. Note that in the meantime we
                    // may load an absurd value from the global object. It's fine to load an absurd
                    // value if the compilation is invalidated anyway.
                    //
                    // We see that the set IsWatched, but the value isn't yet initialized: this isn't
                    // possible because of the ordering of operations.
                    //
                    // Here's how we order operations:
                    //
                    // Main thread stores to the global object: always store a value first, and only
                    // after that do we touch the watchpoint set. There is a fence in the touch, that
                    // ensures that the store to the global object always happens before the touch on the
                    // set.
                    //
                    // Compilation thread: always first load the state of the watchpoint set, and then
                    // load the value. The WatchpointSet::state() method does fences for us to ensure
                    // that the load of the state happens before our load of the value.
                    //
                    // Finalizing compilation: this happens on the main thread and synchronously checks
                    // validity of all watchpoint sets.
                    //
                    // We will only perform optimizations if the load of the state yields IsWatched. That
                    // means that at least one store would have happened to initialize the original value
                    // of the variable (that is, the value we'd like to constant fold to). There may be
                    // other stores that happen after that, but those stores will invalidate the
                    // watchpoint set and also the compilation.
                    
                    // Note that we need to use the operand, which is a direct pointer at the global,
                    // rather than looking up the global by doing variableAt(offset). That's because the
                    // internal data structures of JSSegmentedVariableObject are not thread-safe even
                    // though accessing the global itself is. The segmentation involves a vector spine
                    // that resizes with malloc/free, so if new globals unrelated to the one we are
                    // reading are added, we might access freed memory if we do variableAt().
                    WriteBarrier<Unknown>* pointer = bitwise_cast<WriteBarrier<Unknown>*>(operand);
                    
                    ASSERT(globalObject->findVariableIndex(pointer) == offset);
                    
                    JSValue value = pointer->get();
                    if (value) {
                        m_graph.watchpoints().addLazily(watchpointSet);
                        set(VirtualRegister(dst), weakJSConstant(value));
                        break;
                    }
                }
                
                SpeculatedType prediction = getPrediction();
                set(VirtualRegister(dst), addToGraph(GetGlobalVar, OpInfo(operand), OpInfo(prediction)));
                break;
            }
            case LocalClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(VirtualRegister(scope));
                
                // Ideally we wouldn't have to do this Phantom. But:
                //
                // For the constant case: we must do it because otherwise we would have no way of knowing
                // that the scope is live at OSR here.
                //
                // For the non-constant case: GetClosureVar could be DCE'd, but baseline's implementation
                // won't be able to handle an Undefined scope.
                addToGraph(Phantom, scopeNode);
                
                // Constant folding in the bytecode parser is important for performance. This may not
                // have executed yet. If it hasn't, then we won't have a prediction. Lacking a
                // prediction, we'd otherwise think that it has to exit. Then when it did execute, we
                // would recompile. But if we can fold it here, we avoid the exit.
                if (JSValue value = m_graph.tryGetConstantClosureVar(scopeNode, ScopeOffset(operand))) {
                    set(VirtualRegister(dst), weakJSConstant(value));
                    break;
                }
                SpeculatedType prediction = getPrediction();
                set(VirtualRegister(dst),
                    addToGraph(GetClosureVar, OpInfo(operand), OpInfo(prediction), scopeNode));
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
            unsigned identifierNumber = currentInstruction[2].u.operand;
            if (identifierNumber != UINT_MAX)
                identifierNumber = m_inlineStackTop->m_identifierRemap[identifierNumber];
            unsigned value = currentInstruction[3].u.operand;
            ResolveType resolveType = ResolveModeAndType(currentInstruction[4].u.operand).type();
            UniquedStringImpl* uid;
            if (identifierNumber != UINT_MAX)
                uid = m_graph.identifiers()[identifierNumber];
            else
                uid = nullptr;
            
            Structure* structure = nullptr;
            WatchpointSet* watchpoints = nullptr;
            uintptr_t operand;
            {
                ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks || resolveType == LocalClosureVar)
                    watchpoints = currentInstruction[5].u.watchpointSet;
                else
                    structure = currentInstruction[5].u.structure.get();
                operand = reinterpret_cast<uintptr_t>(currentInstruction[6].u.pointer);
            }

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                PutByIdStatus status;
                if (uid)
                    status = PutByIdStatus::computeFor(globalObject, structure, uid, false);
                else
                    status = PutByIdStatus(PutByIdStatus::TakesSlowPath);
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
                if (watchpoints) {
                    SymbolTableEntry entry = globalObject->symbolTable()->get(uid);
                    ASSERT_UNUSED(entry, watchpoints == entry.watchpointSet());
                }
                Node* valueNode = get(VirtualRegister(value));
                addToGraph(PutGlobalVar, OpInfo(operand), weakJSConstant(globalObject), valueNode);
                if (watchpoints && watchpoints->state() != IsInvalidated) {
                    // Must happen after the store. See comment for GetGlobalVar.
                    addToGraph(NotifyWrite, OpInfo(watchpoints));
                }
                // Keep scope alive until after put.
                addToGraph(Phantom, get(VirtualRegister(scope)));
                break;
            }
            case LocalClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(VirtualRegister(scope));
                Node* valueNode = get(VirtualRegister(value));

                addToGraph(PutClosureVar, OpInfo(operand), scopeNode, valueNode);

                if (watchpoints && watchpoints->state() != IsInvalidated) {
                    // Must happen after the store. See comment for GetGlobalVar.
                    addToGraph(NotifyWrite, OpInfo(watchpoints));
                }
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
            
        case op_create_lexical_environment: {
            FrozenValue* symbolTable = m_graph.freezeStrong(m_graph.symbolTableFor(currentNodeOrigin().semantic));
            Node* lexicalEnvironment = addToGraph(CreateActivation, OpInfo(symbolTable), get(VirtualRegister(currentInstruction[2].u.operand)));
            set(VirtualRegister(currentInstruction[1].u.operand), lexicalEnvironment);
            set(VirtualRegister(currentInstruction[2].u.operand), lexicalEnvironment);
            NEXT_OPCODE(op_create_lexical_environment);
        }
            
        case op_get_scope: {
            // Help the later stages a bit by doing some small constant folding here. Note that this
            // only helps for the first basic block. It's extremely important not to constant fold
            // loads from the scope register later, as that would prevent the DFG from tracking the
            // bytecode-level liveness of the scope register.
            Node* callee = get(VirtualRegister(JSStack::Callee));
            Node* result;
            if (JSFunction* function = callee->dynamicCastConstant<JSFunction*>())
                result = weakJSConstant(function->scope());
            else
                result = addToGraph(GetScope, callee);
            set(VirtualRegister(currentInstruction[1].u.operand), result);
            NEXT_OPCODE(op_get_scope);
        }
            
        case op_create_direct_arguments: {
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateDirectArguments);
            set(VirtualRegister(currentInstruction[1].u.operand), createArguments);
            NEXT_OPCODE(op_create_direct_arguments);
        }
            
        case op_create_scoped_arguments: {
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateScopedArguments, get(VirtualRegister(currentInstruction[2].u.operand)));
            set(VirtualRegister(currentInstruction[1].u.operand), createArguments);
            NEXT_OPCODE(op_create_scoped_arguments);
        }

        case op_create_out_of_band_arguments: {
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateClonedArguments);
            set(VirtualRegister(currentInstruction[1].u.operand), createArguments);
            NEXT_OPCODE(op_create_out_of_band_arguments);
        }
            
        case op_get_from_arguments: {
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(
                    GetFromArguments,
                    OpInfo(currentInstruction[3].u.operand),
                    OpInfo(getPrediction()),
                    get(VirtualRegister(currentInstruction[2].u.operand))));
            NEXT_OPCODE(op_get_from_arguments);
        }
            
        case op_put_to_arguments: {
            addToGraph(
                PutToArguments,
                OpInfo(currentInstruction[2].u.operand),
                get(VirtualRegister(currentInstruction[1].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand)));
            NEXT_OPCODE(op_put_to_arguments);
        }
            
        case op_new_func: {
            FunctionExecutable* decl = m_inlineStackTop->m_profiledBlock->functionDecl(currentInstruction[3].u.operand);
            FrozenValue* frozen = m_graph.freezeStrong(decl);
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(NewFunction, OpInfo(frozen), get(VirtualRegister(currentInstruction[2].u.operand))));
            NEXT_OPCODE(op_new_func);
        }

        case op_new_func_exp: {
            FunctionExecutable* expr = m_inlineStackTop->m_profiledBlock->functionExpr(currentInstruction[3].u.operand);
            FrozenValue* frozen = m_graph.freezeStrong(expr);
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(NewFunction, OpInfo(frozen), get(VirtualRegister(currentInstruction[2].u.operand))));
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

        case op_to_string: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(ToString, value));
            NEXT_OPCODE(op_to_string);
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
            ArrayMode arrayMode = getArrayMode(currentInstruction[4].u.arrayProfile, Array::Read);
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

        case op_get_property_enumerator: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetPropertyEnumerator, 
                get(VirtualRegister(currentInstruction[2].u.operand))));
            NEXT_OPCODE(op_get_property_enumerator);
        }

        case op_enumerator_structure_pname: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetEnumeratorStructurePname,
                get(VirtualRegister(currentInstruction[2].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_enumerator_structure_pname);
        }

        case op_enumerator_generic_pname: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(GetEnumeratorGenericPname,
                get(VirtualRegister(currentInstruction[2].u.operand)),
                get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_enumerator_generic_pname);
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
    Node* node = block->terminal();
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
    
    if (m_caller) {
        // Inline case.
        ASSERT(codeBlock != byteCodeParser->m_codeBlock);
        ASSERT(inlineCallFrameStart.isValid());
        ASSERT(callsiteBlockHead);
        
        m_inlineCallFrame = byteCodeParser->m_graph.m_plan.inlineCallFrames->add();
        byteCodeParser->m_graph.freeze(codeBlock->ownerExecutable());
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
        m_inlineCallFrame->arguments.resizeToFit(argumentCountIncludingThis); // Set the number of arguments including this, but don't configure the value recoveries, yet.
        m_inlineCallFrame->kind = kind;
        
        byteCodeParser->buildOperandMapsIfNecessary();
        
        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        m_constantBufferRemap.resize(codeBlock->numberOfConstantBuffers());
        m_switchRemap.resize(codeBlock->numberOfSwitchJumpTables());

        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i) {
            UniquedStringImpl* rep = codeBlock->identifier(i).impl();
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
    
    if (UNLIKELY(Options::dumpSourceAtDFGTime())) {
        Vector<DeferredSourceDump>& deferredSourceDump = m_graph.m_plan.callback->ensureDeferredSourceDump();
        if (inlineCallFrame()) {
            DeferredSourceDump dump(codeBlock->baselineVersion(), m_codeBlock, JITCode::DFGJIT, inlineCallFrame()->caller);
            deferredSourceDump.append(dump);
        } else
            deferredSourceDump.append(DeferredSourceDump(codeBlock->baselineVersion()));
    }

    if (Options::dumpBytecodeAtDFGTime()) {
        dataLog("Parsing ", *codeBlock);
        if (inlineCallFrame()) {
            dataLog(
                " for inlining at ", CodeBlockWithJITType(m_codeBlock, JITCode::DFGJIT),
                " ", inlineCallFrame()->caller);
        }
        dataLog(
            ": needsActivation = ", codeBlock->needsActivation(),
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
            ASSERT(m_currentBlock->isEmpty() || m_currentBlock->terminal() || (m_currentIndex == codeBlock->instructions().size() && inlineCallFrame()) || !shouldContinueParsing);

            if (!shouldContinueParsing) {
                if (Options::verboseDFGByteCodeParsing())
                    dataLog("Done parsing ", *codeBlock, "\n");
                return;
            }
            
            m_currentBlock = 0;
        } while (m_currentIndex < limit);
    }

    // Should have reached the end of the instructions.
    ASSERT(m_currentIndex == codeBlock->instructions().size());
    
    if (Options::verboseDFGByteCodeParsing())
        dataLog("Done parsing ", *codeBlock, " (fell off end)\n");
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
