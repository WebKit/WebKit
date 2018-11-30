/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "ArithProfile.h"
#include "ArrayConstructor.h"
#include "BasicBlockLocation.h"
#include "BuiltinNames.h"
#include "BytecodeStructs.h"
#include "CallLinkStatus.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "CommonSlowPaths.h"
#include "DFGAbstractHeap.h"
#include "DFGArrayMode.h"
#include "DFGCFG.h"
#include "DFGCapabilities.h"
#include "DFGClobberize.h"
#include "DFGClobbersExitState.h"
#include "DFGGraph.h"
#include "DFGJITCode.h"
#include "FunctionCodeBlock.h"
#include "GetByIdStatus.h"
#include "Heap.h"
#include "InByIdStatus.h"
#include "InstanceOfStatus.h"
#include "JSCInlines.h"
#include "JSFixedArray.h"
#include "JSImmutableButterfly.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "NumberConstructor.h"
#include "ObjectConstructor.h"
#include "OpcodeInlines.h"
#include "PreciseJumpTargets.h"
#include "PutByIdFlags.h"
#include "PutByIdStatus.h"
#include "RegExpPrototype.h"
#include "StackAlignment.h"
#include "StringConstructor.h"
#include "StructureStubInfo.h"
#include "Watchdog.h"
#include <wtf/CommaPrinter.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace DFG {

namespace DFGByteCodeParserInternal {
#ifdef NDEBUG
static const bool verbose = false;
#else
static const bool verbose = true;
#endif
} // namespace DFGByteCodeParserInternal

#define VERBOSE_LOG(...) do { \
if (DFGByteCodeParserInternal::verbose && Options::verboseDFGBytecodeParsing()) \
dataLog(__VA_ARGS__); \
} while (false)

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
        , m_numLocals(m_codeBlock->numCalleeLocals())
        , m_parameterSlots(0)
        , m_numPassedVarArgs(0)
        , m_inlineStackTop(0)
        , m_currentInstruction(0)
        , m_hasDebuggerEnabled(graph.hasDebuggerEnabled())
    {
        ASSERT(m_profiledBlock);
    }
    
    // Parse a full CodeBlock of bytecode.
    void parse();
    
private:
    struct InlineStackEntry;

    // Just parse from m_currentIndex to the end of the current CodeBlock.
    void parseCodeBlock();
    
    void ensureLocals(unsigned newNumLocals)
    {
        VERBOSE_LOG("   ensureLocals: trying to raise m_numLocals from ", m_numLocals, " to ", newNumLocals, "\n");
        if (newNumLocals <= m_numLocals)
            return;
        m_numLocals = newNumLocals;
        for (size_t i = 0; i < m_graph.numBlocks(); ++i)
            m_graph.block(i)->ensureLocals(newNumLocals);
    }

    // Helper for min and max.
    template<typename ChecksFunctor>
    bool handleMinMax(VirtualRegister result, NodeType op, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& insertChecks);
    
    void refineStatically(CallLinkStatus&, Node* callTarget);
    // Blocks can either be targetable (i.e. in the m_blockLinkingTargets of one InlineStackEntry) with a well-defined bytecodeBegin,
    // or they can be untargetable, with bytecodeBegin==UINT_MAX, to be managed manually and not by the linkBlock machinery.
    // This is used most notably when doing polyvariant inlining (it requires a fair bit of control-flow with no bytecode analog).
    // It is also used when doing an early return from an inlined callee: it is easier to fix the bytecode index later on if needed
    // than to move the right index all the way to the treatment of op_ret.
    BasicBlock* allocateTargetableBlock(unsigned bytecodeIndex);
    BasicBlock* allocateUntargetableBlock();
    // An untargetable block can be given a bytecodeIndex to be later managed by linkBlock, but only once, and it can never go in the other direction
    void makeBlockTargetable(BasicBlock*, unsigned bytecodeIndex);
    void addJumpTo(BasicBlock*);
    void addJumpTo(unsigned bytecodeIndex);
    // Handle calls. This resolves issues surrounding inlining and intrinsics.
    enum Terminality { Terminal, NonTerminal };
    Terminality handleCall(
        VirtualRegister result, NodeType op, InlineCallFrame::Kind, unsigned instructionSize,
        Node* callTarget, int argumentCountIncludingThis, int registerOffset, CallLinkStatus,
        SpeculatedType prediction);
    template<typename CallOp>
    Terminality handleCall(const Instruction* pc, NodeType op, CallMode);
    template<typename CallOp>
    Terminality handleVarargsCall(const Instruction* pc, NodeType op, CallMode);
    void emitFunctionChecks(CallVariant, Node* callTarget, VirtualRegister thisArgumnt);
    void emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis);
    Node* getArgumentCount();
    template<typename ChecksFunctor>
    bool handleRecursiveTailCall(Node* callTargetNode, CallVariant, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& emitFunctionCheckIfNeeded);
    unsigned inliningCost(CallVariant, int argumentCountIncludingThis, InlineCallFrame::Kind); // Return UINT_MAX if it's not an inlining candidate. By convention, intrinsics have a cost of 1.
    // Handle inlining. Return true if it succeeded, false if we need to plant a call.
    bool handleVarargsInlining(Node* callTargetNode, VirtualRegister result, const CallLinkStatus&, int registerOffset, VirtualRegister thisArgument, VirtualRegister argumentsArgument, unsigned argumentsOffset, NodeType callOp, InlineCallFrame::Kind);
    unsigned getInliningBalance(const CallLinkStatus&, CodeSpecializationKind);
    enum class CallOptimizationResult { OptimizedToJump, Inlined, DidNothing };
    CallOptimizationResult handleCallVariant(Node* callTargetNode, VirtualRegister result, CallVariant, int registerOffset, VirtualRegister thisArgument, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind, SpeculatedType prediction, unsigned& inliningBalance, BasicBlock* continuationBlock, bool needsToCheckCallee);
    CallOptimizationResult handleInlining(Node* callTargetNode, VirtualRegister result, const CallLinkStatus&, int registerOffset, VirtualRegister thisArgument, int argumentCountIncludingThis, unsigned nextOffset, NodeType callOp, InlineCallFrame::Kind, SpeculatedType prediction);
    template<typename ChecksFunctor>
    void inlineCall(Node* callTargetNode, VirtualRegister result, CallVariant, int registerOffset, int argumentCountIncludingThis, InlineCallFrame::Kind, BasicBlock* continuationBlock, const ChecksFunctor& insertChecks);
    // Handle intrinsic functions. Return true if it succeeded, false if we need to plant a call.
    template<typename ChecksFunctor>
    bool handleIntrinsicCall(Node* callee, VirtualRegister result, Intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleDOMJITCall(Node* callee, VirtualRegister result, const DOMJIT::Signature*, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleIntrinsicGetter(VirtualRegister result, SpeculatedType prediction, const GetByIdVariant& intrinsicVariant, Node* thisNode, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleTypedArrayConstructor(VirtualRegister result, InternalFunction*, int registerOffset, int argumentCountIncludingThis, TypedArrayType, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleConstantInternalFunction(Node* callTargetNode, VirtualRegister result, InternalFunction*, int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind, SpeculatedType, const ChecksFunctor& insertChecks);
    Node* handlePutByOffset(Node* base, unsigned identifier, PropertyOffset, const InferredType::Descriptor&, Node* value);
    Node* handleGetByOffset(SpeculatedType, Node* base, unsigned identifierNumber, PropertyOffset, const InferredType::Descriptor&, NodeType = GetByOffset);
    bool handleDOMJITGetter(VirtualRegister result, const GetByIdVariant&, Node* thisNode, unsigned identifierNumber, SpeculatedType prediction);
    bool handleModuleNamespaceLoad(VirtualRegister result, SpeculatedType, Node* base, GetByIdStatus);

    template<typename Bytecode>
    void handlePutByVal(Bytecode, unsigned instructionSize);
    template <typename Bytecode>
    void handlePutAccessorById(NodeType, Bytecode);
    template <typename Bytecode>
    void handlePutAccessorByVal(NodeType, Bytecode);
    template <typename Bytecode>
    void handleNewFunc(NodeType, Bytecode);
    template <typename Bytecode>
    void handleNewFuncExp(NodeType, Bytecode);

    // Create a presence ObjectPropertyCondition based on some known offset and structure set. Does not
    // check the validity of the condition, but it may return a null one if it encounters a contradiction.
    ObjectPropertyCondition presenceLike(
        JSObject* knownBase, UniquedStringImpl*, PropertyOffset, const StructureSet&);
    
    // Attempt to watch the presence of a property. It will watch that the property is present in the same
    // way as in all of the structures in the set. It may emit code instead of just setting a watchpoint.
    // Returns true if this all works out.
    bool checkPresenceLike(JSObject* knownBase, UniquedStringImpl*, PropertyOffset, const StructureSet&);
    void checkPresenceLike(Node* base, UniquedStringImpl*, PropertyOffset, const StructureSet&);
    
    // Works with both GetByIdVariant and the setter form of PutByIdVariant.
    template<typename VariantType>
    Node* load(SpeculatedType, Node* base, unsigned identifierNumber, const VariantType&);

    Node* store(Node* base, unsigned identifier, const PutByIdVariant&, Node* value);

    template<typename Op>
    void parseGetById(const Instruction*);
    void handleGetById(
        VirtualRegister destination, SpeculatedType, Node* base, unsigned identifierNumber, GetByIdStatus, AccessType, unsigned instructionSize);
    void emitPutById(
        Node* base, unsigned identifierNumber, Node* value,  const PutByIdStatus&, bool isDirect);
    void handlePutById(
        Node* base, unsigned identifierNumber, Node* value, const PutByIdStatus&,
        bool isDirect, unsigned intructionSize);
    
    // Either register a watchpoint or emit a check for this condition. Returns false if the
    // condition no longer holds, and therefore no reasonable check can be emitted.
    bool check(const ObjectPropertyCondition&);
    
    GetByOffsetMethod promoteToConstant(GetByOffsetMethod);
    
    // Either register a watchpoint or emit a check for this condition. It must be a Presence
    // condition. It will attempt to promote a Presence condition to an Equivalence condition.
    // Emits code for the loaded value that the condition guards, and returns a node containing
    // the loaded value. Returns null if the condition no longer holds.
    GetByOffsetMethod planLoad(const ObjectPropertyCondition&);
    Node* load(SpeculatedType, unsigned identifierNumber, const GetByOffsetMethod&, NodeType = GetByOffset);
    Node* load(SpeculatedType, const ObjectPropertyCondition&, NodeType = GetByOffset);
    
    // Calls check() for each condition in the set: that is, it either emits checks or registers
    // watchpoints (or a combination of the two) to make the conditions hold. If any of those
    // conditions are no longer checkable, returns false.
    bool check(const ObjectPropertyConditionSet&);
    
    // Calls check() for those conditions that aren't the slot base, and calls load() for the slot
    // base. Does a combination of watchpoint registration and check emission to guard the
    // conditions, and emits code to load the value from the slot base. Returns a node containing
    // the loaded value. Returns null if any of the conditions were no longer checkable.
    GetByOffsetMethod planLoad(const ObjectPropertyConditionSet&);
    Node* load(SpeculatedType, const ObjectPropertyConditionSet&, NodeType = GetByOffset);

    void prepareToParseBlock();
    void clearCaches();

    // Parse a single basic block of bytecode instructions.
    void parseBlock(unsigned limit);
    // Link block successors.
    void linkBlock(BasicBlock*, Vector<BasicBlock*>& possibleTargets);
    void linkBlocks(Vector<BasicBlock*>& unlinkedBlocks, Vector<BasicBlock*>& possibleTargets);
    
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
                if (operand.offset() == CallFrameSlot::callee)
                    return weakJSConstant(callee);
            }
        } else if (operand.offset() == CallFrameSlot::callee) {
            // We have to do some constant-folding here because this enables CreateThis folding. Note
            // that we don't have such watchpoint-based folding for inlined uses of Callee, since in that
            // case if the function is a singleton then we already know it.
            if (FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(*m_vm, m_codeBlock->ownerExecutable())) {
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

        // We can't exit anymore because our OSR exit state has changed.
        m_exitOK = false;

        DelayedSetLocal delayed(currentCodeOrigin(), operand, value, setMode);
        
        if (setMode == NormalSet) {
            m_setLocalQueue.append(delayed);
            return nullptr;
        }
        
        return delayed.execute(this);
    }
    
    void processSetLocalQueue()
    {
        for (unsigned i = 0; i < m_setLocalQueue.size(); ++i)
            m_setLocalQueue[i].execute(this);
        m_setLocalQueue.shrink(0);
    }

    Node* set(VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        return setDirect(m_inlineStackTop->remapOperand(operand), value, setMode);
    }
    
    Node* injectLazyOperandSpeculation(Node* node)
    {
        ASSERT(node->op() == GetLocal);
        ASSERT(node->origin.semantic.bytecodeIndex == m_currentIndex);
        ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
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
        SetForScope<CodeOrigin> originChange(m_currentSemanticOrigin, semanticOrigin);

        unsigned local = operand.toLocal();
        
        if (setMode != ImmediateNakedSet) {
            ArgumentPosition* argumentPosition = findArgumentPositionForLocal(operand);
            if (argumentPosition)
                flushDirect(operand, argumentPosition);
            else if (m_graph.needsScopeRegister() && operand == m_codeBlock->scopeRegister())
                flush(operand);
        }

        VariableAccessData* variableAccessData = newVariableAccessData(operand);
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadIndexingType));
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
        SetForScope<CodeOrigin> originChange(m_currentSemanticOrigin, semanticOrigin);

        unsigned argument = operand.toArgument();
        ASSERT(argument < m_numArguments);
        
        VariableAccessData* variableAccessData = newVariableAccessData(operand);

        // Always flush arguments, except for 'this'. If 'this' is created by us,
        // then make sure that it's never unboxed.
        if (argument || m_graph.needsFlushedThis()) {
            if (setMode != ImmediateNakedSet)
                flushDirect(operand);
        }
        
        if (!argument && m_codeBlock->specializationKind() == CodeForConstruct)
            variableAccessData->mergeShouldNeverUnbox(true);
        
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex, BadIndexingType));
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
            if (operand.offset() < static_cast<int>(inlineCallFrame->stackOffset + CallFrame::headerSizeInRegisters))
                continue;
            if (operand.offset() >= static_cast<int>(inlineCallFrame->stackOffset + CallFrame::thisArgumentOffset() + inlineCallFrame->argumentsWithFixup.size()))
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

    template<typename AddFlushDirectFunc>
    void flushImpl(InlineCallFrame* inlineCallFrame, const AddFlushDirectFunc& addFlushDirect)
    {
        int numArguments;
        if (inlineCallFrame) {
            ASSERT(!m_graph.hasDebuggerEnabled());
            numArguments = inlineCallFrame->argumentsWithFixup.size();
            if (inlineCallFrame->isClosureCall)
                addFlushDirect(inlineCallFrame, remapOperand(inlineCallFrame, VirtualRegister(CallFrameSlot::callee)));
            if (inlineCallFrame->isVarargs())
                addFlushDirect(inlineCallFrame, remapOperand(inlineCallFrame, VirtualRegister(CallFrameSlot::argumentCount)));
        } else
            numArguments = m_graph.baselineCodeBlockFor(inlineCallFrame)->numParameters();

        for (unsigned argument = numArguments; argument--;)
            addFlushDirect(inlineCallFrame, remapOperand(inlineCallFrame, virtualRegisterForArgument(argument)));

        if (m_graph.needsScopeRegister())
            addFlushDirect(nullptr, m_graph.m_codeBlock->scopeRegister());
    }

    template<typename AddFlushDirectFunc, typename AddPhantomLocalDirectFunc>
    void flushForTerminalImpl(CodeOrigin origin, const AddFlushDirectFunc& addFlushDirect, const AddPhantomLocalDirectFunc& addPhantomLocalDirect)
    {
        origin.walkUpInlineStack(
            [&] (CodeOrigin origin) {
                unsigned bytecodeIndex = origin.bytecodeIndex;
                InlineCallFrame* inlineCallFrame = origin.inlineCallFrame;
                flushImpl(inlineCallFrame, addFlushDirect);

                CodeBlock* codeBlock = m_graph.baselineCodeBlockFor(inlineCallFrame);
                FullBytecodeLiveness& fullLiveness = m_graph.livenessFor(codeBlock);
                const FastBitVector& livenessAtBytecode = fullLiveness.getLiveness(bytecodeIndex);

                for (unsigned local = codeBlock->numCalleeLocals(); local--;) {
                    if (livenessAtBytecode[local])
                        addPhantomLocalDirect(inlineCallFrame, remapOperand(inlineCallFrame, virtualRegisterForLocal(local)));
                }
            });
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
        addFlushOrPhantomLocal<Flush>(operand, argumentPosition);
    }

    template<NodeType nodeType>
    void addFlushOrPhantomLocal(VirtualRegister operand, ArgumentPosition* argumentPosition)
    {
        ASSERT(!operand.isConstant());
        
        Node* node = m_currentBlock->variablesAtTail.operand(operand);
        
        VariableAccessData* variable;
        
        if (node)
            variable = node->variableAccessData();
        else
            variable = newVariableAccessData(operand);
        
        node = addToGraph(nodeType, OpInfo(variable));
        m_currentBlock->variablesAtTail.operand(operand) = node;
        if (argumentPosition)
            argumentPosition->addVariable(variable);
    }

    void phantomLocalDirect(VirtualRegister operand)
    {
        addFlushOrPhantomLocal<PhantomLocal>(operand, findArgumentPosition(operand));
    }

    void flush(InlineStackEntry* inlineStackEntry)
    {
        auto addFlushDirect = [&] (InlineCallFrame*, VirtualRegister reg) { flushDirect(reg); };
        flushImpl(inlineStackEntry->m_inlineCallFrame, addFlushDirect);
    }

    void flushForTerminal()
    {
        auto addFlushDirect = [&] (InlineCallFrame*, VirtualRegister reg) { flushDirect(reg); };
        auto addPhantomLocalDirect = [&] (InlineCallFrame*, VirtualRegister reg) { phantomLocalDirect(reg); };
        flushForTerminalImpl(currentCodeOrigin(), addFlushDirect, addPhantomLocalDirect);
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

    bool allInlineFramesAreTailCalls()
    {
        return !inlineCallFrame() || !inlineCallFrame()->getCallerSkippingTailCalls();
    }

    CodeOrigin currentCodeOrigin()
    {
        return CodeOrigin(m_currentIndex, inlineCallFrame());
    }

    NodeOrigin currentNodeOrigin()
    {
        CodeOrigin semantic;
        CodeOrigin forExit;

        if (m_currentSemanticOrigin.isSet())
            semantic = m_currentSemanticOrigin;
        else
            semantic = currentCodeOrigin();

        forExit = currentCodeOrigin();

        return NodeOrigin(semantic, forExit, m_exitOK);
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
        VERBOSE_LOG("        appended ", node, " ", Graph::opName(node->op()), "\n");

        m_hasAnyForceOSRExits |= (node->op() == ForceOSRExit);

        m_currentBlock->append(node);
        if (clobbersExitState(m_graph, node))
            m_exitOK = false;
        return node;
    }
    
    Node* addToGraph(NodeType op, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            op, currentNodeOrigin(), Edge(child1), Edge(child2),
            Edge(child3));
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, Edge child1, Edge child2 = Edge(), Edge child3 = Edge())
    {
        Node* result = m_graph.addNode(
            op, currentNodeOrigin(), child1, child2, child3);
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, OpInfo info, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            op, currentNodeOrigin(), info, Edge(child1), Edge(child2),
            Edge(child3));
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, OpInfo info, Edge child1, Edge child2 = Edge(), Edge child3 = Edge())
    {
        Node* result = m_graph.addNode(op, currentNodeOrigin(), info, child1, child2, child3);
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, OpInfo info1, OpInfo info2, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            op, currentNodeOrigin(), info1, info2,
            Edge(child1), Edge(child2), Edge(child3));
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, OpInfo info1, OpInfo info2, Edge child1, Edge child2 = Edge(), Edge child3 = Edge())
    {
        Node* result = m_graph.addNode(
            op, currentNodeOrigin(), info1, info2, child1, child2, child3);
        return addToGraph(result);
    }
    
    Node* addToGraph(Node::VarArgTag, NodeType op, OpInfo info1, OpInfo info2 = OpInfo())
    {
        Node* result = m_graph.addNode(
            Node::VarArg, op, currentNodeOrigin(), info1, info2,
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

    void addVarArgChild(Edge child)
    {
        m_graph.m_varArgChildren.append(child);
        m_numPassedVarArgs++;
    }
    
    Node* addCallWithoutSettingResult(
        NodeType op, OpInfo opInfo, Node* callee, int argCount, int registerOffset,
        OpInfo prediction)
    {
        addVarArgChild(callee);
        size_t parameterSlots = Graph::parameterSlotsForArgCount(argCount);

        if (parameterSlots > m_parameterSlots)
            m_parameterSlots = parameterSlots;

        for (int i = 0; i < argCount; ++i)
            addVarArgChild(get(virtualRegisterForArgument(i, registerOffset)));

        return addToGraph(Node::VarArg, op, opInfo, prediction);
    }
    
    Node* addCall(
        VirtualRegister result, NodeType op, const DOMJIT::Signature* signature, Node* callee, int argCount, int registerOffset,
        SpeculatedType prediction)
    {
        if (op == TailCall) {
            if (allInlineFramesAreTailCalls())
                return addCallWithoutSettingResult(op, OpInfo(signature), callee, argCount, registerOffset, OpInfo());
            op = TailCallInlinedCaller;
        }


        Node* call = addCallWithoutSettingResult(
            op, OpInfo(signature), callee, argCount, registerOffset, OpInfo(prediction));
        if (result.isValid())
            set(result, call);
        return call;
    }
    
    Node* cellConstantWithStructureCheck(JSCell* object, Structure* structure)
    {
        // FIXME: This should route to emitPropertyCheck, not the other way around. But currently,
        // this gets no profit from using emitPropertyCheck() since we'll non-adaptively watch the
        // object's structure as soon as we make it a weakJSCosntant.
        Node* objectNode = weakJSConstant(object);
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(structure)), objectNode);
        return objectNode;
    }
    
    SpeculatedType getPredictionWithoutOSRExit(unsigned bytecodeIndex)
    {
        SpeculatedType prediction;
        {
            ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
            prediction = m_inlineStackTop->m_profiledBlock->valueProfilePredictionForBytecodeOffset(locker, bytecodeIndex);
        }

        if (prediction != SpecNone)
            return prediction;

        // If we have no information about the values this
        // node generates, we check if by any chance it is
        // a tail call opcode. In that case, we walk up the
        // inline frames to find a call higher in the call
        // chain and use its prediction. If we only have
        // inlined tail call frames, we use SpecFullTop
        // to avoid a spurious OSR exit.
        auto instruction = m_inlineStackTop->m_profiledBlock->instructions().at(bytecodeIndex);
        OpcodeID opcodeID = instruction->opcodeID();

        switch (opcodeID) {
        case op_tail_call:
        case op_tail_call_varargs:
        case op_tail_call_forward_arguments: {
            // Things should be more permissive to us returning BOTTOM instead of TOP here.
            // Currently, this will cause us to Force OSR exit. This is bad because returning
            // TOP will cause anything that transitively touches this speculated type to
            // also become TOP during prediction propagation.
            // https://bugs.webkit.org/show_bug.cgi?id=164337
            if (!inlineCallFrame())
                return SpecFullTop;

            CodeOrigin* codeOrigin = inlineCallFrame()->getCallerSkippingTailCalls();
            if (!codeOrigin)
                return SpecFullTop;

            InlineStackEntry* stack = m_inlineStackTop;
            while (stack->m_inlineCallFrame != codeOrigin->inlineCallFrame)
                stack = stack->m_caller;

            bytecodeIndex = codeOrigin->bytecodeIndex;
            CodeBlock* profiledBlock = stack->m_profiledBlock;
            ConcurrentJSLocker locker(profiledBlock->m_lock);
            return profiledBlock->valueProfilePredictionForBytecodeOffset(locker, bytecodeIndex);
        }

        default:
            return SpecNone;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return SpecNone;
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
    
    ArrayMode getArrayMode(Array::Action action)
    {
        CodeBlock* codeBlock = m_inlineStackTop->m_profiledBlock;
        ArrayProfile* profile = codeBlock->getArrayProfile(codeBlock->bytecodeOffset(m_currentInstruction));
        return getArrayMode(*profile, action);
    }

    ArrayMode getArrayMode(ArrayProfile& profile, Array::Action action)
    {
        ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        profile.computeUpdatedPrediction(locker, m_inlineStackTop->m_profiledBlock);
        bool makeSafe = profile.outOfBounds(locker);
        return ArrayMode::fromObserved(locker, &profile, action, makeSafe);
    }

    Node* makeSafe(Node* node)
    {
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInt32InDFG);
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
            node->mergeFlags(NodeMayNegZeroInDFG);
        
        if (!isX86() && node->op() == ArithMod)
            return node;

        {
            ArithProfile* arithProfile = m_inlineStackTop->m_profiledBlock->arithProfileForBytecodeOffset(m_currentIndex);
            if (arithProfile) {
                switch (node->op()) {
                case ArithAdd:
                case ArithSub:
                case ValueAdd:
                    if (arithProfile->didObserveDouble())
                        node->mergeFlags(NodeMayHaveDoubleResult);
                    if (arithProfile->didObserveNonNumeric())
                        node->mergeFlags(NodeMayHaveNonNumericResult);
                    if (arithProfile->didObserveBigInt())
                        node->mergeFlags(NodeMayHaveBigIntResult);
                    break;
                
                case ArithMul: {
                    if (arithProfile->didObserveInt52Overflow())
                        node->mergeFlags(NodeMayOverflowInt52);
                    if (arithProfile->didObserveInt32Overflow() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
                        node->mergeFlags(NodeMayOverflowInt32InBaseline);
                    if (arithProfile->didObserveNegZeroDouble() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
                        node->mergeFlags(NodeMayNegZeroInBaseline);
                    if (arithProfile->didObserveDouble())
                        node->mergeFlags(NodeMayHaveDoubleResult);
                    if (arithProfile->didObserveNonNumeric())
                        node->mergeFlags(NodeMayHaveNonNumericResult);
                    break;
                }
                case ValueNegate:
                case ArithNegate: {
                    if (arithProfile->lhsObservedType().sawNumber() || arithProfile->didObserveDouble())
                        node->mergeFlags(NodeMayHaveDoubleResult);
                    if (arithProfile->didObserveNegZeroDouble() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
                        node->mergeFlags(NodeMayNegZeroInBaseline);
                    if (arithProfile->didObserveInt32Overflow() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
                        node->mergeFlags(NodeMayOverflowInt32InBaseline);
                    if (arithProfile->didObserveNonNumeric())
                        node->mergeFlags(NodeMayHaveNonNumericResult);
                    if (arithProfile->didObserveBigInt())
                        node->mergeFlags(NodeMayHaveBigIntResult);
                    break;
                }
                
                default:
                    break;
                }
            }
        }
        
        if (m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex)) {
            switch (node->op()) {
            case UInt32ToNumber:
            case ArithAdd:
            case ArithSub:
            case ValueAdd:
            case ArithMod: // for ArithMod "MayOverflow" means we tried to divide by zero, or we saw double.
                node->mergeFlags(NodeMayOverflowInt32InBaseline);
                break;
                
            default:
                break;
            }
        }
        
        return node;
    }
    
    Node* makeDivSafe(Node* node)
    {
        ASSERT(node->op() == ArithDiv);
        
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInt32InDFG);
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
        node->mergeFlags(NodeMayOverflowInt32InBaseline | NodeMayNegZeroInBaseline);
        
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

    bool needsDynamicLookup(ResolveType, OpcodeID);

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
    // True if it's OK to OSR exit right now.
    bool m_exitOK { false };

    FrozenValue* m_constantUndefined;
    FrozenValue* m_constantNull;
    FrozenValue* m_constantNaN;
    FrozenValue* m_constantOne;
    Vector<Node*, 16> m_constants;

    HashMap<InlineCallFrame*, Vector<ArgumentPosition*>, WTF::DefaultHash<InlineCallFrame*>::Hash, WTF::NullableHashTraits<InlineCallFrame*>> m_inlineCallFrameToArgumentPositions;

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

    struct InlineStackEntry {
        ByteCodeParser* m_byteCodeParser;
        
        CodeBlock* m_codeBlock;
        CodeBlock* m_profiledBlock;
        InlineCallFrame* m_inlineCallFrame;
        
        ScriptExecutable* executable() { return m_codeBlock->ownerScriptExecutable(); }
        
        QueryableExitProfile m_exitProfile;
        
        // Remapping of identifier and constant numbers from the code block being
        // inlined (inline callee) to the code block that we're inlining into
        // (the machine code block, which is the transitive, though not necessarily
        // direct, caller).
        Vector<unsigned> m_identifierRemap;
        Vector<unsigned> m_switchRemap;
        
        // These are blocks whose terminal is a Jump, Branch or Switch, and whose target has not yet been linked.
        // Their terminal instead refers to a bytecode index, and the right BB can be found in m_blockLinkingTargets.
        Vector<BasicBlock*> m_unlinkedBlocks;
        
        // Potential block linking targets. Must be sorted by bytecodeBegin, and
        // cannot have two blocks that have the same bytecodeBegin.
        Vector<BasicBlock*> m_blockLinkingTargets;

        // Optional: a continuation block for returns to jump to. It is set by early returns if it does not exist.
        BasicBlock* m_continuationBlock;

        VirtualRegister m_returnValue;
        
        // Speculations about variable types collected from the profiled code block,
        // which are based on OSR exit profiles that past DFG compilations of this
        // code block had gathered.
        LazyOperandValueProfileParser m_lazyOperands;
        
        ICStatusMap m_baselineMap;
        ICStatusContext m_optimizedContext;
        
        // Pointers to the argument position trackers for this slice of code.
        Vector<ArgumentPosition*> m_argumentPositions;
        
        InlineStackEntry* m_caller;
        
        InlineStackEntry(
            ByteCodeParser*,
            CodeBlock*,
            CodeBlock* profiledBlock,
            JSFunction* callee, // Null if this is a closure call.
            VirtualRegister returnValueVR,
            VirtualRegister inlineCallFrameStart,
            int argumentCountIncludingThis,
            InlineCallFrame::Kind,
            BasicBlock* continuationBlock);
        
        ~InlineStackEntry();
        
        VirtualRegister remapOperand(VirtualRegister operand) const
        {
            if (!m_inlineCallFrame)
                return operand;
            
            ASSERT(!operand.isConstant());

            return VirtualRegister(operand.offset() + m_inlineCallFrame->stackOffset);
        }
    };
    
    InlineStackEntry* m_inlineStackTop;
    
    ICStatusContextStack m_icContextStack;
    
    struct DelayedSetLocal {
        CodeOrigin m_origin;
        VirtualRegister m_operand;
        Node* m_value;
        SetMode m_setMode;
        
        DelayedSetLocal() { }
        DelayedSetLocal(const CodeOrigin& origin, VirtualRegister operand, Node* value, SetMode setMode)
            : m_origin(origin)
            , m_operand(operand)
            , m_value(value)
            , m_setMode(setMode)
        {
            RELEASE_ASSERT(operand.isValid());
        }
        
        Node* execute(ByteCodeParser* parser)
        {
            if (m_operand.isArgument())
                return parser->setArgument(m_origin, m_operand, m_value, m_setMode);
            return parser->setLocal(m_origin, m_operand, m_value, m_setMode);
        }
    };
    
    Vector<DelayedSetLocal, 2> m_setLocalQueue;

    const Instruction* m_currentInstruction;
    bool m_hasDebuggerEnabled;
    bool m_hasAnyForceOSRExits { false };
};

BasicBlock* ByteCodeParser::allocateTargetableBlock(unsigned bytecodeIndex)
{
    ASSERT(bytecodeIndex != UINT_MAX);
    Ref<BasicBlock> block = adoptRef(*new BasicBlock(bytecodeIndex, m_numArguments, m_numLocals, 1));
    BasicBlock* blockPtr = block.ptr();
    // m_blockLinkingTargets must always be sorted in increasing order of bytecodeBegin
    if (m_inlineStackTop->m_blockLinkingTargets.size())
        ASSERT(m_inlineStackTop->m_blockLinkingTargets.last()->bytecodeBegin < bytecodeIndex);
    m_inlineStackTop->m_blockLinkingTargets.append(blockPtr);
    m_graph.appendBlock(WTFMove(block));
    return blockPtr;
}

BasicBlock* ByteCodeParser::allocateUntargetableBlock()
{
    Ref<BasicBlock> block = adoptRef(*new BasicBlock(UINT_MAX, m_numArguments, m_numLocals, 1));
    BasicBlock* blockPtr = block.ptr();
    m_graph.appendBlock(WTFMove(block));
    return blockPtr;
}

void ByteCodeParser::makeBlockTargetable(BasicBlock* block, unsigned bytecodeIndex)
{
    RELEASE_ASSERT(block->bytecodeBegin == UINT_MAX);
    block->bytecodeBegin = bytecodeIndex;
    // m_blockLinkingTargets must always be sorted in increasing order of bytecodeBegin
    if (m_inlineStackTop->m_blockLinkingTargets.size())
        ASSERT(m_inlineStackTop->m_blockLinkingTargets.last()->bytecodeBegin < bytecodeIndex);
    m_inlineStackTop->m_blockLinkingTargets.append(block);
}

void ByteCodeParser::addJumpTo(BasicBlock* block)
{
    ASSERT(!m_currentBlock->terminal());
    Node* jumpNode = addToGraph(Jump);
    jumpNode->targetBlock() = block;
    m_currentBlock->didLink();
}

void ByteCodeParser::addJumpTo(unsigned bytecodeIndex)
{
    ASSERT(!m_currentBlock->terminal());
    addToGraph(Jump, OpInfo(bytecodeIndex));
    m_inlineStackTop->m_unlinkedBlocks.append(m_currentBlock);
}

template<typename CallOp>
ByteCodeParser::Terminality ByteCodeParser::handleCall(const Instruction* pc, NodeType op, CallMode callMode)
{
    auto bytecode = pc->as<CallOp>();
    Node* callTarget = get(bytecode.callee);
    int registerOffset = -static_cast<int>(bytecode.argv);

    CallLinkStatus callLinkStatus = CallLinkStatus::computeFor(
        m_inlineStackTop->m_profiledBlock, currentCodeOrigin(),
        m_inlineStackTop->m_baselineMap, m_icContextStack);

    InlineCallFrame::Kind kind = InlineCallFrame::kindFor(callMode);

    return handleCall(bytecode.dst, op, kind, pc->size(), callTarget,
        bytecode.argc, registerOffset, callLinkStatus, getPrediction());
}

void ByteCodeParser::refineStatically(CallLinkStatus& callLinkStatus, Node* callTarget)
{
    if (callTarget->isCellConstant())
        callLinkStatus.setProvenConstantCallee(CallVariant(callTarget->asCell()));
}

ByteCodeParser::Terminality ByteCodeParser::handleCall(
    VirtualRegister result, NodeType op, InlineCallFrame::Kind kind, unsigned instructionSize,
    Node* callTarget, int argumentCountIncludingThis, int registerOffset,
    CallLinkStatus callLinkStatus, SpeculatedType prediction)
{
    ASSERT(registerOffset <= 0);

    refineStatically(callLinkStatus, callTarget);
    
    VERBOSE_LOG("    Handling call at ", currentCodeOrigin(), ": ", callLinkStatus, "\n");
    
    // If we have profiling information about this call, and it did not behave too polymorphically,
    // we may be able to inline it, or in the case of recursive tail calls turn it into a jump.
    if (callLinkStatus.canOptimize()) {
        addToGraph(FilterCallLinkStatus, OpInfo(m_graph.m_plan.recordedStatuses().addCallLinkStatus(currentCodeOrigin(), callLinkStatus)), callTarget);

        VirtualRegister thisArgument = virtualRegisterForArgument(0, registerOffset);
        auto optimizationResult = handleInlining(callTarget, result, callLinkStatus, registerOffset, thisArgument,
            argumentCountIncludingThis, m_currentIndex + instructionSize, op, kind, prediction);
        if (optimizationResult == CallOptimizationResult::OptimizedToJump)
            return Terminal;
        if (optimizationResult == CallOptimizationResult::Inlined) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedCall();
            return NonTerminal;
        }
    }
    
    Node* callNode = addCall(result, op, nullptr, callTarget, argumentCountIncludingThis, registerOffset, prediction);
    ASSERT(callNode->op() != TailCallVarargs && callNode->op() != TailCallForwardVarargs);
    return callNode->op() == TailCall ? Terminal : NonTerminal;
}

template<typename CallOp>
ByteCodeParser::Terminality ByteCodeParser::handleVarargsCall(const Instruction* pc, NodeType op, CallMode callMode)
{
    auto bytecode = pc->as<CallOp>();
    int firstFreeReg = bytecode.firstFree.offset();
    int firstVarArgOffset = bytecode.firstVarArg;
    
    SpeculatedType prediction = getPrediction();
    
    Node* callTarget = get(bytecode.callee);
    
    CallLinkStatus callLinkStatus = CallLinkStatus::computeFor(
        m_inlineStackTop->m_profiledBlock, currentCodeOrigin(),
        m_inlineStackTop->m_baselineMap, m_icContextStack);
    refineStatically(callLinkStatus, callTarget);
    
    VERBOSE_LOG("    Varargs call link status at ", currentCodeOrigin(), ": ", callLinkStatus, "\n");
    
    if (callLinkStatus.canOptimize()) {
        addToGraph(FilterCallLinkStatus, OpInfo(m_graph.m_plan.recordedStatuses().addCallLinkStatus(currentCodeOrigin(), callLinkStatus)), callTarget);

        if (handleVarargsInlining(callTarget, bytecode.dst,
            callLinkStatus, firstFreeReg, bytecode.thisValue, bytecode.arguments,
            firstVarArgOffset, op,
            InlineCallFrame::varargsKindFor(callMode))) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedCall();
            return NonTerminal;
        }
    }
    
    CallVarargsData* data = m_graph.m_callVarargsData.add();
    data->firstVarArgOffset = firstVarArgOffset;
    
    Node* thisChild = get(bytecode.thisValue);
    Node* argumentsChild = nullptr;
    if (op != TailCallForwardVarargs)
        argumentsChild = get(bytecode.arguments);

    if (op == TailCallVarargs || op == TailCallForwardVarargs) {
        if (allInlineFramesAreTailCalls()) {
            addToGraph(op, OpInfo(data), OpInfo(), callTarget, thisChild, argumentsChild);
            return Terminal;
        }
        op = op == TailCallVarargs ? TailCallVarargsInlinedCaller : TailCallForwardVarargsInlinedCaller;
    }

    Node* call = addToGraph(op, OpInfo(data), OpInfo(prediction), callTarget, thisChild, argumentsChild);
    if (bytecode.dst.isValid())
        set(bytecode.dst, call);
    return NonTerminal;
}

void ByteCodeParser::emitFunctionChecks(CallVariant callee, Node* callTarget, VirtualRegister thisArgumentReg)
{
    Node* thisArgument;
    if (thisArgumentReg.isValid())
        thisArgument = get(thisArgumentReg);
    else
        thisArgument = nullptr;

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
    addToGraph(CheckCell, OpInfo(m_graph.freeze(calleeCell)), callTargetForCheck);
    if (thisArgument)
        addToGraph(Phantom, thisArgument);
}

Node* ByteCodeParser::getArgumentCount()
{
    Node* argumentCount;
    if (m_inlineStackTop->m_inlineCallFrame && !m_inlineStackTop->m_inlineCallFrame->isVarargs())
        argumentCount = jsConstant(m_graph.freeze(jsNumber(m_inlineStackTop->m_inlineCallFrame->argumentCountIncludingThis))->value());
    else
        argumentCount = addToGraph(GetArgumentCountIncludingThis, OpInfo(m_inlineStackTop->m_inlineCallFrame), OpInfo(SpecInt32Only));
    return argumentCount;
}

void ByteCodeParser::emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis)
{
    for (int i = 0; i < argumentCountIncludingThis; ++i)
        addToGraph(Phantom, get(virtualRegisterForArgument(i, registerOffset)));
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleRecursiveTailCall(Node* callTargetNode, CallVariant callVariant, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& emitFunctionCheckIfNeeded)
{
    if (UNLIKELY(!Options::optimizeRecursiveTailCalls()))
        return false;

    auto targetExecutable = callVariant.executable();
    InlineStackEntry* stackEntry = m_inlineStackTop;
    do {
        if (targetExecutable != stackEntry->executable())
            continue;
        VERBOSE_LOG("   We found a recursive tail call, trying to optimize it into a jump.\n");

        if (auto* callFrame = stackEntry->m_inlineCallFrame) {
            // Some code may statically use the argument count from the InlineCallFrame, so it would be invalid to loop back if it does not match.
            // We "continue" instead of returning false in case another stack entry further on the stack has the right number of arguments.
            if (argumentCountIncludingThis != static_cast<int>(callFrame->argumentCountIncludingThis))
                continue;
        } else {
            // We are in the machine code entry (i.e. the original caller).
            // If we have more arguments than the number of parameters to the function, it is not clear where we could put them on the stack.
            if (argumentCountIncludingThis > m_codeBlock->numParameters())
                return false;
        }

        // If an InlineCallFrame is not a closure, it was optimized using a constant callee.
        // Check if this is the same callee that we try to inline here.
        if (stackEntry->m_inlineCallFrame && !stackEntry->m_inlineCallFrame->isClosureCall) {
            if (stackEntry->m_inlineCallFrame->calleeConstant() != callVariant.function())
                continue;
        }

        // We must add some check that the profiling information was correct and the target of this call is what we thought.
        emitFunctionCheckIfNeeded();
        // We flush everything, as if we were in the backedge of a loop (see treatment of op_jmp in parseBlock).
        flushForTerminal();

        // We must set the callee to the right value
        if (stackEntry->m_inlineCallFrame) {
            if (stackEntry->m_inlineCallFrame->isClosureCall)
                setDirect(stackEntry->remapOperand(VirtualRegister(CallFrameSlot::callee)), callTargetNode, NormalSet);
        } else
            addToGraph(SetCallee, callTargetNode);

        // We must set the arguments to the right values
        if (!stackEntry->m_inlineCallFrame)
            addToGraph(SetArgumentCountIncludingThis, OpInfo(argumentCountIncludingThis));
        int argIndex = 0;
        for (; argIndex < argumentCountIncludingThis; ++argIndex) {
            Node* value = get(virtualRegisterForArgument(argIndex, registerOffset));
            setDirect(stackEntry->remapOperand(virtualRegisterForArgument(argIndex)), value, NormalSet);
        }
        Node* undefined = addToGraph(JSConstant, OpInfo(m_constantUndefined));
        for (; argIndex < stackEntry->m_codeBlock->numParameters(); ++argIndex)
            setDirect(stackEntry->remapOperand(virtualRegisterForArgument(argIndex)), undefined, NormalSet);

        // We must repeat the work of op_enter here as we will jump right after it.
        // We jump right after it and not before it, because of some invariant saying that a CFG root cannot have predecessors in the IR.
        for (int i = 0; i < stackEntry->m_codeBlock->numVars(); ++i)
            setDirect(stackEntry->remapOperand(virtualRegisterForLocal(i)), undefined, NormalSet);

        // We want to emit the SetLocals with an exit origin that points to the place we are jumping to.
        unsigned oldIndex = m_currentIndex;
        auto oldStackTop = m_inlineStackTop;
        m_inlineStackTop = stackEntry;
        m_currentIndex = opcodeLengths[op_enter];
        m_exitOK = true;
        processSetLocalQueue();
        m_currentIndex = oldIndex;
        m_inlineStackTop = oldStackTop;
        m_exitOK = false;

        BasicBlock** entryBlockPtr = tryBinarySearch<BasicBlock*, unsigned>(stackEntry->m_blockLinkingTargets, stackEntry->m_blockLinkingTargets.size(), opcodeLengths[op_enter], getBytecodeBeginForBlock);
        RELEASE_ASSERT(entryBlockPtr);
        addJumpTo(*entryBlockPtr);
        return true;
        // It would be unsound to jump over a non-tail call: the "tail" call is not really a tail call in that case.
    } while (stackEntry->m_inlineCallFrame && stackEntry->m_inlineCallFrame->kind == InlineCallFrame::TailCall && (stackEntry = stackEntry->m_caller));

    // The tail call was not recursive
    return false;
}

unsigned ByteCodeParser::inliningCost(CallVariant callee, int argumentCountIncludingThis, InlineCallFrame::Kind kind)
{
    CallMode callMode = InlineCallFrame::callModeFor(kind);
    CodeSpecializationKind specializationKind = specializationKindFor(callMode);
    VERBOSE_LOG("Considering inlining ", callee, " into ", currentCodeOrigin(), "\n");
    
    if (m_hasDebuggerEnabled) {
        VERBOSE_LOG("    Failing because the debugger is in use.\n");
        return UINT_MAX;
    }

    FunctionExecutable* executable = callee.functionExecutable();
    if (!executable) {
        VERBOSE_LOG("    Failing because there is no function executable.\n");
        return UINT_MAX;
    }
    
    // Do we have a code block, and does the code block's size match the heuristics/requirements for
    // being an inline candidate? We might not have a code block (1) if code was thrown away,
    // (2) if we simply hadn't actually made this call yet or (3) code is a builtin function and
    // specialization kind is construct. In the former 2 cases, we could still theoretically attempt
    // to inline it if we had a static proof of what was being called; this might happen for example
    // if you call a global function, where watchpointing gives us static information. Overall,
    // it's a rare case because we expect that any hot callees would have already been compiled.
    CodeBlock* codeBlock = executable->baselineCodeBlockFor(specializationKind);
    if (!codeBlock) {
        VERBOSE_LOG("    Failing because no code block available.\n");
        return UINT_MAX;
    }

    if (!Options::useArityFixupInlining()) {
        if (codeBlock->numParameters() > argumentCountIncludingThis) {
            VERBOSE_LOG("    Failing because of arity mismatch.\n");
            return UINT_MAX;
        }
    }

    CapabilityLevel capabilityLevel = inlineFunctionForCapabilityLevel(
        codeBlock, specializationKind, callee.isClosureCall());
    VERBOSE_LOG("    Call mode: ", callMode, "\n");
    VERBOSE_LOG("    Is closure call: ", callee.isClosureCall(), "\n");
    VERBOSE_LOG("    Capability level: ", capabilityLevel, "\n");
    VERBOSE_LOG("    Might inline function: ", mightInlineFunctionFor(codeBlock, specializationKind), "\n");
    VERBOSE_LOG("    Might compile function: ", mightCompileFunctionFor(codeBlock, specializationKind), "\n");
    VERBOSE_LOG("    Is supported for inlining: ", isSupportedForInlining(codeBlock), "\n");
    VERBOSE_LOG("    Is inlining candidate: ", codeBlock->ownerScriptExecutable()->isInliningCandidate(), "\n");
    if (!canInline(capabilityLevel)) {
        VERBOSE_LOG("    Failing because the function is not inlineable.\n");
        return UINT_MAX;
    }
    
    // Check if the caller is already too large. We do this check here because that's just
    // where we happen to also have the callee's code block, and we want that for the
    // purpose of unsetting SABI.
    if (!isSmallEnoughToInlineCodeInto(m_codeBlock)) {
        codeBlock->m_shouldAlwaysBeInlined = false;
        VERBOSE_LOG("    Failing because the caller is too large.\n");
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
            VERBOSE_LOG("    Failing because depth exceeded.\n");
            return UINT_MAX;
        }
        
        if (entry->executable() == executable) {
            ++recursion;
            if (recursion >= Options::maximumInliningRecursion()) {
                VERBOSE_LOG("    Failing because recursion detected.\n");
                return UINT_MAX;
            }
        }
    }
    
    VERBOSE_LOG("    Inlining should be possible.\n");
    
    // It might be possible to inline.
    return codeBlock->instructionCount();
}

template<typename ChecksFunctor>
void ByteCodeParser::inlineCall(Node* callTargetNode, VirtualRegister result, CallVariant callee, int registerOffset, int argumentCountIncludingThis, InlineCallFrame::Kind kind, BasicBlock* continuationBlock, const ChecksFunctor& insertChecks)
{
    const Instruction* savedCurrentInstruction = m_currentInstruction;
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    
    ASSERT(inliningCost(callee, argumentCountIncludingThis, kind) != UINT_MAX);
    
    CodeBlock* codeBlock = callee.functionExecutable()->baselineCodeBlockFor(specializationKind);
    insertChecks(codeBlock);

    // FIXME: Don't flush constants!

    // arityFixupCount and numberOfStackPaddingSlots are different. While arityFixupCount does not consider about stack alignment,
    // numberOfStackPaddingSlots consider alignment. Consider the following case,
    //
    // before: [ ... ][arg0][header]
    // after:  [ ... ][ext ][arg1][arg0][header]
    //
    // In the above case, arityFixupCount is 1. But numberOfStackPaddingSlots is 2 because the stack needs to be aligned.
    // We insert extra slots to align stack.
    int arityFixupCount = std::max<int>(codeBlock->numParameters() - argumentCountIncludingThis, 0);
    int numberOfStackPaddingSlots = CommonSlowPaths::numberOfStackPaddingSlots(codeBlock, argumentCountIncludingThis);
    ASSERT(!(numberOfStackPaddingSlots % stackAlignmentRegisters()));
    int registerOffsetAfterFixup = registerOffset - numberOfStackPaddingSlots;
    
    int inlineCallFrameStart = m_inlineStackTop->remapOperand(VirtualRegister(registerOffsetAfterFixup)).offset() + CallFrame::headerSizeInRegisters;
    
    ensureLocals(
        VirtualRegister(inlineCallFrameStart).toLocal() + 1 +
        CallFrame::headerSizeInRegisters + codeBlock->numCalleeLocals());
    
    size_t argumentPositionStart = m_graph.m_argumentPositions.size();

    if (result.isValid())
        result = m_inlineStackTop->remapOperand(result);

    VariableAccessData* calleeVariable = nullptr;
    if (callee.isClosureCall()) {
        Node* calleeSet = set(
            VirtualRegister(registerOffsetAfterFixup + CallFrameSlot::callee), callTargetNode, ImmediateNakedSet);
        
        calleeVariable = calleeSet->variableAccessData();
        calleeVariable->mergeShouldNeverUnbox(true);
    }

    if (arityFixupCount) {
        // Note: we do arity fixup in two phases:
        // 1. We get all the values we need and MovHint them to the expected locals.
        // 2. We SetLocal them inside the callee's CodeOrigin. This way, if we exit, the callee's
        //    frame is already set up. If any SetLocal exits, we have a valid exit state.
        //    This is required because if we didn't do this in two phases, we may exit in
        //    the middle of arity fixup from the caller's CodeOrigin. This is unsound because if
        //    we did the SetLocals in the caller's frame, the memcpy may clobber needed parts
        //    of the frame right before exiting. For example, consider if we need to pad two args:
        //    [arg3][arg2][arg1][arg0]
        //    [fix ][fix ][arg3][arg2][arg1][arg0]
        //    We memcpy starting from arg0 in the direction of arg3. If we were to exit at a type check
        //    for arg3's SetLocal in the caller's CodeOrigin, we'd exit with a frame like so:
        //    [arg3][arg2][arg1][arg2][arg1][arg0]
        //    And the caller would then just end up thinking its argument are:
        //    [arg3][arg2][arg1][arg2]
        //    which is incorrect.

        Node* undefined = addToGraph(JSConstant, OpInfo(m_constantUndefined));
        // The stack needs to be aligned due to the JS calling convention. Thus, we have a hole if the count of arguments is not aligned.
        // We call this hole "extra slot". Consider the following case, the number of arguments is 2. If this argument
        // count does not fulfill the stack alignment requirement, we already inserted extra slots.
        //
        // before: [ ... ][ext ][arg1][arg0][header]
        //
        // In the above case, one extra slot is inserted. If the code's parameter count is 3, we will fixup arguments.
        // At that time, we can simply use this extra slots. So the fixuped stack is the following.
        //
        // before: [ ... ][ext ][arg1][arg0][header]
        // after:  [ ... ][arg2][arg1][arg0][header]
        //
        // In such cases, we do not need to move frames.
        if (registerOffsetAfterFixup != registerOffset) {
            for (int index = 0; index < argumentCountIncludingThis; ++index) {
                Node* value = get(virtualRegisterForArgument(index, registerOffset));
                VirtualRegister argumentToSet = m_inlineStackTop->remapOperand(virtualRegisterForArgument(index, registerOffsetAfterFixup));
                addToGraph(MovHint, OpInfo(argumentToSet.offset()), value);
                m_setLocalQueue.append(DelayedSetLocal { currentCodeOrigin(), argumentToSet, value, ImmediateNakedSet });
            }
        }
        for (int index = 0; index < arityFixupCount; ++index) {
            VirtualRegister argumentToSet = m_inlineStackTop->remapOperand(virtualRegisterForArgument(argumentCountIncludingThis + index, registerOffsetAfterFixup));
            addToGraph(MovHint, OpInfo(argumentToSet.offset()), undefined);
            m_setLocalQueue.append(DelayedSetLocal { currentCodeOrigin(), argumentToSet, undefined, ImmediateNakedSet });
        }

        // At this point, it's OK to OSR exit because we finished setting up
        // our callee's frame. We emit an ExitOK below from the callee's CodeOrigin.
    }

    InlineStackEntry inlineStackEntry(this, codeBlock, codeBlock, callee.function(), result,
        (VirtualRegister)inlineCallFrameStart, argumentCountIncludingThis, kind, continuationBlock);

    // This is where the actual inlining really happens.
    unsigned oldIndex = m_currentIndex;
    m_currentIndex = 0;

    // At this point, it's again OK to OSR exit.
    m_exitOK = true;
    addToGraph(ExitOK);

    processSetLocalQueue();

    InlineVariableData inlineVariableData;
    inlineVariableData.inlineCallFrame = m_inlineStackTop->m_inlineCallFrame;
    inlineVariableData.argumentPositionStart = argumentPositionStart;
    inlineVariableData.calleeVariable = 0;
    
    RELEASE_ASSERT(
        m_inlineStackTop->m_inlineCallFrame->isClosureCall
        == callee.isClosureCall());
    if (callee.isClosureCall()) {
        RELEASE_ASSERT(calleeVariable);
        inlineVariableData.calleeVariable = calleeVariable;
    }
    
    m_graph.m_inlineVariableData.append(inlineVariableData);

    parseCodeBlock();
    clearCaches(); // Reset our state now that we're back to the outer code.
    
    m_currentIndex = oldIndex;
    m_exitOK = false;

    linkBlocks(inlineStackEntry.m_unlinkedBlocks, inlineStackEntry.m_blockLinkingTargets);
    
    // Most functions have at least one op_ret and thus set up the continuation block.
    // In some rare cases, a function ends in op_unreachable, forcing us to allocate a new continuationBlock here.
    if (inlineStackEntry.m_continuationBlock)
        m_currentBlock = inlineStackEntry.m_continuationBlock;
    else
        m_currentBlock = allocateUntargetableBlock();
    ASSERT(!m_currentBlock->terminal());

    prepareToParseBlock();
    m_currentInstruction = savedCurrentInstruction;
}

ByteCodeParser::CallOptimizationResult ByteCodeParser::handleCallVariant(Node* callTargetNode, VirtualRegister result, CallVariant callee, int registerOffset, VirtualRegister thisArgument, int argumentCountIncludingThis, unsigned nextOffset, InlineCallFrame::Kind kind, SpeculatedType prediction, unsigned& inliningBalance, BasicBlock* continuationBlock, bool needsToCheckCallee)
{
    VERBOSE_LOG("    Considering callee ", callee, "\n");

    bool didInsertChecks = false;
    auto insertChecksWithAccounting = [&] () {
        if (needsToCheckCallee)
            emitFunctionChecks(callee, callTargetNode, thisArgument);
        didInsertChecks = true;
    };

    if (kind == InlineCallFrame::TailCall && ByteCodeParser::handleRecursiveTailCall(callTargetNode, callee, registerOffset, argumentCountIncludingThis, insertChecksWithAccounting)) {
        RELEASE_ASSERT(didInsertChecks);
        return CallOptimizationResult::OptimizedToJump;
    }
    RELEASE_ASSERT(!didInsertChecks);

    if (!inliningBalance)
        return CallOptimizationResult::DidNothing;

    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);

    auto endSpecialCase = [&] () {
        RELEASE_ASSERT(didInsertChecks);
        addToGraph(Phantom, callTargetNode);
        emitArgumentPhantoms(registerOffset, argumentCountIncludingThis);
        inliningBalance--;
        if (continuationBlock) {
            m_currentIndex = nextOffset;
            m_exitOK = true;
            processSetLocalQueue();
            addJumpTo(continuationBlock);
        }
    };

    if (InternalFunction* function = callee.internalFunction()) {
        if (handleConstantInternalFunction(callTargetNode, result, function, registerOffset, argumentCountIncludingThis, specializationKind, prediction, insertChecksWithAccounting)) {
            endSpecialCase();
            return CallOptimizationResult::Inlined;
        }
        RELEASE_ASSERT(!didInsertChecks);
        return CallOptimizationResult::DidNothing;
    }

    Intrinsic intrinsic = callee.intrinsicFor(specializationKind);
    if (intrinsic != NoIntrinsic) {
        if (handleIntrinsicCall(callTargetNode, result, intrinsic, registerOffset, argumentCountIncludingThis, prediction, insertChecksWithAccounting)) {
            endSpecialCase();
            return CallOptimizationResult::Inlined;
        }
        RELEASE_ASSERT(!didInsertChecks);
        // We might still try to inline the Intrinsic because it might be a builtin JS function.
    }

    if (Options::useDOMJIT()) {
        if (const DOMJIT::Signature* signature = callee.signatureFor(specializationKind)) {
            if (handleDOMJITCall(callTargetNode, result, signature, registerOffset, argumentCountIncludingThis, prediction, insertChecksWithAccounting)) {
                endSpecialCase();
                return CallOptimizationResult::Inlined;
            }
            RELEASE_ASSERT(!didInsertChecks);
        }
    }
    
    unsigned myInliningCost = inliningCost(callee, argumentCountIncludingThis, kind);
    if (myInliningCost > inliningBalance)
        return CallOptimizationResult::DidNothing;

    auto insertCheck = [&] (CodeBlock*) {
        if (needsToCheckCallee)
            emitFunctionChecks(callee, callTargetNode, thisArgument);
    };
    inlineCall(callTargetNode, result, callee, registerOffset, argumentCountIncludingThis, kind, continuationBlock, insertCheck);
    inliningBalance -= myInliningCost;
    return CallOptimizationResult::Inlined;
}

bool ByteCodeParser::handleVarargsInlining(Node* callTargetNode, VirtualRegister result,
    const CallLinkStatus& callLinkStatus, int firstFreeReg, VirtualRegister thisArgument,
    VirtualRegister argumentsArgument, unsigned argumentsOffset,
    NodeType callOp, InlineCallFrame::Kind kind)
{
    VERBOSE_LOG("Handling inlining (Varargs)...\nStack: ", currentCodeOrigin(), "\n");
    if (callLinkStatus.maxNumArguments() > Options::maximumVarargsForInlining()) {
        VERBOSE_LOG("Bailing inlining: too many arguments for varargs inlining.\n");
        return false;
    }
    if (callLinkStatus.couldTakeSlowPath() || callLinkStatus.size() != 1) {
        VERBOSE_LOG("Bailing inlining: polymorphic inlining is not yet supported for varargs.\n");
        return false;
    }

    CallVariant callVariant = callLinkStatus[0];

    unsigned mandatoryMinimum;
    if (FunctionExecutable* functionExecutable = callVariant.functionExecutable())
        mandatoryMinimum = functionExecutable->parameterCount();
    else
        mandatoryMinimum = 0;
    
    // includes "this"
    unsigned maxNumArguments = std::max(callLinkStatus.maxNumArguments(), mandatoryMinimum + 1);

    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    if (inliningCost(callVariant, maxNumArguments, kind) > getInliningBalance(callLinkStatus, specializationKind)) {
        VERBOSE_LOG("Bailing inlining: inlining cost too high.\n");
        return false;
    }
    
    int registerOffset = firstFreeReg + 1;
    registerOffset -= maxNumArguments; // includes "this"
    registerOffset -= CallFrame::headerSizeInRegisters;
    registerOffset = -WTF::roundUpToMultipleOf(stackAlignmentRegisters(), -registerOffset);
    
    auto insertChecks = [&] (CodeBlock* codeBlock) {
        emitFunctionChecks(callVariant, callTargetNode, thisArgument);
        
        int remappedRegisterOffset =
        m_inlineStackTop->remapOperand(VirtualRegister(registerOffset)).offset();
        
        ensureLocals(VirtualRegister(remappedRegisterOffset).toLocal());
        
        int argumentStart = registerOffset + CallFrame::headerSizeInRegisters;
        int remappedArgumentStart =
        m_inlineStackTop->remapOperand(VirtualRegister(argumentStart)).offset();
        
        LoadVarargsData* data = m_graph.m_loadVarargsData.add();
        data->start = VirtualRegister(remappedArgumentStart + 1);
        data->count = VirtualRegister(remappedRegisterOffset + CallFrameSlot::argumentCount);
        data->offset = argumentsOffset;
        data->limit = maxNumArguments;
        data->mandatoryMinimum = mandatoryMinimum;
        
        if (callOp == TailCallForwardVarargs)
            addToGraph(ForwardVarargs, OpInfo(data));
        else
            addToGraph(LoadVarargs, OpInfo(data), get(argumentsArgument));
        
        // LoadVarargs may OSR exit. Hence, we need to keep alive callTargetNode, thisArgument
        // and argumentsArgument for the baseline JIT. However, we only need a Phantom for
        // callTargetNode because the other 2 are still in use and alive at this point.
        addToGraph(Phantom, callTargetNode);
        
        // In DFG IR before SSA, we cannot insert control flow between after the
        // LoadVarargs and the last SetArgument. This isn't a problem once we get to DFG
        // SSA. Fortunately, we also have other reasons for not inserting control flow
        // before SSA.
        
        VariableAccessData* countVariable = newVariableAccessData(VirtualRegister(remappedRegisterOffset + CallFrameSlot::argumentCount));
        // This is pretty lame, but it will force the count to be flushed as an int. This doesn't
        // matter very much, since our use of a SetArgument and Flushes for this local slot is
        // mostly just a formality.
        countVariable->predict(SpecInt32Only);
        countVariable->mergeIsProfitableToUnbox(true);
        Node* setArgumentCount = addToGraph(SetArgument, OpInfo(countVariable));
        m_currentBlock->variablesAtTail.setOperand(countVariable->local(), setArgumentCount);
        
        set(VirtualRegister(argumentStart), get(thisArgument), ImmediateNakedSet);
        for (unsigned argument = 1; argument < maxNumArguments; ++argument) {
            VariableAccessData* variable = newVariableAccessData(VirtualRegister(remappedArgumentStart + argument));
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
                ConcurrentJSLocker locker(codeBlock->m_lock);
                ValueProfile& profile = codeBlock->valueProfileForArgument(argument);
                variable->predict(profile.computeUpdatedPrediction(locker));
            }
            
            Node* setArgument = addToGraph(SetArgument, OpInfo(variable));
            m_currentBlock->variablesAtTail.setOperand(variable->local(), setArgument);
        }
    };

    // Intrinsics and internal functions can only be inlined if we're not doing varargs. This is because
    // we currently don't have any way of getting profiling information for arguments to non-JS varargs
    // calls. The prediction propagator won't be of any help because LoadVarargs obscures the data flow,
    // and there are no callsite value profiles and native function won't have callee value profiles for
    // those arguments. Even worse, if the intrinsic decides to exit, it won't really have anywhere to
    // exit to: LoadVarargs is effectful and it's part of the op_call_varargs, so we can't exit without
    // calling LoadVarargs twice.
    inlineCall(callTargetNode, result, callVariant, registerOffset, maxNumArguments, kind, nullptr, insertChecks);

    VERBOSE_LOG("Successful inlining (varargs, monomorphic).\nStack: ", currentCodeOrigin(), "\n");
    return true;
}

unsigned ByteCodeParser::getInliningBalance(const CallLinkStatus& callLinkStatus, CodeSpecializationKind specializationKind)
{
    unsigned inliningBalance = Options::maximumFunctionForCallInlineCandidateInstructionCount();
    if (specializationKind == CodeForConstruct)
        inliningBalance = std::min(inliningBalance, Options::maximumFunctionForConstructInlineCandidateInstructionCount());
    if (callLinkStatus.isClosureCall())
        inliningBalance = std::min(inliningBalance, Options::maximumFunctionForClosureCallInlineCandidateInstructionCount());
    return inliningBalance;
}

ByteCodeParser::CallOptimizationResult ByteCodeParser::handleInlining(
    Node* callTargetNode, VirtualRegister result, const CallLinkStatus& callLinkStatus,
    int registerOffset, VirtualRegister thisArgument,
    int argumentCountIncludingThis,
    unsigned nextOffset, NodeType callOp, InlineCallFrame::Kind kind, SpeculatedType prediction)
{
    VERBOSE_LOG("Handling inlining...\nStack: ", currentCodeOrigin(), "\n");
    
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    unsigned inliningBalance = getInliningBalance(callLinkStatus, specializationKind);

    // First check if we can avoid creating control flow. Our inliner does some CFG
    // simplification on the fly and this helps reduce compile times, but we can only leverage
    // this in cases where we don't need control flow diamonds to check the callee.
    if (!callLinkStatus.couldTakeSlowPath() && callLinkStatus.size() == 1) {
        return handleCallVariant(
            callTargetNode, result, callLinkStatus[0], registerOffset, thisArgument,
            argumentCountIncludingThis, nextOffset, kind, prediction, inliningBalance, nullptr, true);
    }

    // We need to create some kind of switch over callee. For now we only do this if we believe that
    // we're in the top tier. We have two reasons for this: first, it provides us an opportunity to
    // do more detailed polyvariant/polymorphic profiling; and second, it reduces compile times in
    // the DFG. And by polyvariant profiling we mean polyvariant profiling of *this* call. Note that
    // we could improve that aspect of this by doing polymorphic inlining but having the profiling
    // also.
    if (!m_graph.m_plan.isFTL() || !Options::usePolymorphicCallInlining()) {
        VERBOSE_LOG("Bailing inlining (hard).\nStack: ", currentCodeOrigin(), "\n");
        return CallOptimizationResult::DidNothing;
    }
    
    // If the claim is that this did not originate from a stub, then we don't want to emit a switch
    // statement. Whenever the non-stub profiling says that it could take slow path, it really means that
    // it has no idea.
    if (!Options::usePolymorphicCallInliningForNonStubStatus()
        && !callLinkStatus.isBasedOnStub()) {
        VERBOSE_LOG("Bailing inlining (non-stub polymorphism).\nStack: ", currentCodeOrigin(), "\n");
        return CallOptimizationResult::DidNothing;
    }

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
        VERBOSE_LOG("Bailing inlining (mix).\nStack: ", currentCodeOrigin(), "\n");
        return CallOptimizationResult::DidNothing;
    }

    VERBOSE_LOG("Doing hard inlining...\nStack: ", currentCodeOrigin(), "\n");

    // This makes me wish that we were in SSA all the time. We need to pick a variable into which to
    // store the callee so that it will be accessible to all of the blocks we're about to create. We
    // get away with doing an immediate-set here because we wouldn't have performed any side effects
    // yet.
    VERBOSE_LOG("Register offset: ", registerOffset);
    VirtualRegister calleeReg(registerOffset + CallFrameSlot::callee);
    calleeReg = m_inlineStackTop->remapOperand(calleeReg);
    VERBOSE_LOG("Callee is going to be ", calleeReg, "\n");
    setDirect(calleeReg, callTargetNode, ImmediateSetWithFlush);

    // It's OK to exit right now, even though we set some locals. That's because those locals are not
    // user-visible.
    m_exitOK = true;
    addToGraph(ExitOK);
    
    SwitchData& data = *m_graph.m_switchData.add();
    data.kind = SwitchCell;
    addToGraph(Switch, OpInfo(&data), thingToSwitchOn);
    m_currentBlock->didLink();
    
    BasicBlock* continuationBlock = allocateUntargetableBlock();
    VERBOSE_LOG("Adding untargetable block ", RawPointer(continuationBlock), " (continuation)\n");
    
    // We may force this true if we give up on inlining any of the edges.
    bool couldTakeSlowPath = callLinkStatus.couldTakeSlowPath();
    
    VERBOSE_LOG("About to loop over functions at ", currentCodeOrigin(), ".\n");

    unsigned oldOffset = m_currentIndex;
    for (unsigned i = 0; i < callLinkStatus.size(); ++i) {
        m_currentIndex = oldOffset;
        BasicBlock* calleeEntryBlock = allocateUntargetableBlock();
        m_currentBlock = calleeEntryBlock;
        prepareToParseBlock();

        // At the top of each switch case, we can exit.
        m_exitOK = true;
        
        Node* myCallTargetNode = getDirect(calleeReg);
        
        auto inliningResult = handleCallVariant(
            myCallTargetNode, result, callLinkStatus[i], registerOffset,
            thisArgument, argumentCountIncludingThis, nextOffset, kind, prediction,
            inliningBalance, continuationBlock, false);
        
        if (inliningResult == CallOptimizationResult::DidNothing) {
            // That failed so we let the block die. Nothing interesting should have been added to
            // the block. We also give up on inlining any of the (less frequent) callees.
            ASSERT(m_graph.m_blocks.last() == m_currentBlock);
            m_graph.killBlockAndItsContents(m_currentBlock);
            m_graph.m_blocks.removeLast();
            VERBOSE_LOG("Inlining of a poly call failed, we will have to go through a slow path\n");

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
        data.cases.append(SwitchCase(m_graph.freeze(thingToCaseOn), calleeEntryBlock));
        VERBOSE_LOG("Finished optimizing ", callLinkStatus[i], " at ", currentCodeOrigin(), ".\n");
    }

    // Slow path block
    m_currentBlock = allocateUntargetableBlock();
    m_currentIndex = oldOffset;
    m_exitOK = true;
    data.fallThrough = BranchTarget(m_currentBlock);
    prepareToParseBlock();
    Node* myCallTargetNode = getDirect(calleeReg);
    if (couldTakeSlowPath) {
        addCall(
            result, callOp, nullptr, myCallTargetNode, argumentCountIncludingThis,
            registerOffset, prediction);
        VERBOSE_LOG("We added a call in the slow path\n");
    } else {
        addToGraph(CheckBadCell);
        addToGraph(Phantom, myCallTargetNode);
        emitArgumentPhantoms(registerOffset, argumentCountIncludingThis);
        
        set(result, addToGraph(BottomValue));
        VERBOSE_LOG("couldTakeSlowPath was false\n");
    }

    m_currentIndex = nextOffset;
    m_exitOK = true; // Origin changed, so it's fine to exit again.
    processSetLocalQueue();

    if (Node* terminal = m_currentBlock->terminal())
        ASSERT_UNUSED(terminal, terminal->op() == TailCall || terminal->op() == TailCallVarargs || terminal->op() == TailCallForwardVarargs);
    else {
        addJumpTo(continuationBlock);
    }

    prepareToParseBlock();
    
    m_currentIndex = oldOffset;
    m_currentBlock = continuationBlock;
    m_exitOK = true;
    
    VERBOSE_LOG("Done inlining (hard).\nStack: ", currentCodeOrigin(), "\n");
    return CallOptimizationResult::Inlined;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleMinMax(VirtualRegister result, NodeType op, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& insertChecks)
{
    ASSERT(op == ArithMin || op == ArithMax);

    if (argumentCountIncludingThis == 1) {
        insertChecks();
        double limit = op == ArithMax ? -std::numeric_limits<double>::infinity() : +std::numeric_limits<double>::infinity();
        set(result, addToGraph(JSConstant, OpInfo(m_graph.freeze(jsDoubleNumber(limit)))));
        return true;
    }
     
    if (argumentCountIncludingThis == 2) {
        insertChecks();
        Node* resultNode = get(VirtualRegister(virtualRegisterForArgument(1, registerOffset)));
        addToGraph(Phantom, Edge(resultNode, NumberUse));
        set(result, resultNode);
        return true;
    }
    
    if (argumentCountIncludingThis == 3) {
        insertChecks();
        set(result, addToGraph(op, get(virtualRegisterForArgument(1, registerOffset)), get(virtualRegisterForArgument(2, registerOffset))));
        return true;
    }
    
    // Don't handle >=3 arguments for now.
    return false;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleIntrinsicCall(Node* callee, VirtualRegister result, Intrinsic intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks)
{
    VERBOSE_LOG("       The intrinsic is ", intrinsic, "\n");

    if (!isOpcodeShape<OpCallShape>(m_currentInstruction))
        return false;

    // It so happens that the code below doesn't handle the invalid result case. We could fix that, but
    // it would only benefit intrinsics called as setters, like if you do:
    //
    //     o.__defineSetter__("foo", Math.pow)
    //
    // Which is extremely amusing, but probably not worth optimizing.
    if (!result.isValid())
        return false;
    
    switch (intrinsic) {

    // Intrinsic Functions:

    case AbsIntrinsic: {
        if (argumentCountIncludingThis == 1) { // Math.abs()
            insertChecks();
            set(result, addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }

        if (!MacroAssembler::supportsFloatingPointAbs())
            return false;

        insertChecks();
        Node* node = addToGraph(ArithAbs, get(virtualRegisterForArgument(1, registerOffset)));
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInt32InDFG);
        set(result, node);
        return true;
    }

    case MinIntrinsic:
        return handleMinMax(result, ArithMin, registerOffset, argumentCountIncludingThis, insertChecks);
        
    case MaxIntrinsic:
        return handleMinMax(result, ArithMax, registerOffset, argumentCountIncludingThis, insertChecks);

#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
    case capitalizedName##Intrinsic:
    FOR_EACH_DFG_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY
    {
        if (argumentCountIncludingThis == 1) {
            insertChecks();
            set(result, addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }
        Arith::UnaryType type = Arith::UnaryType::Sin;
        switch (intrinsic) {
#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
        case capitalizedName##Intrinsic: \
            type = Arith::UnaryType::capitalizedName; \
            break;
    FOR_EACH_DFG_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        insertChecks();
        set(result, addToGraph(ArithUnary, OpInfo(static_cast<std::underlying_type<Arith::UnaryType>::type>(type)), get(virtualRegisterForArgument(1, registerOffset))));
        return true;
    }

    case FRoundIntrinsic:
    case SqrtIntrinsic: {
        if (argumentCountIncludingThis == 1) {
            insertChecks();
            set(result, addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }

        NodeType nodeType = Unreachable;
        switch (intrinsic) {
        case FRoundIntrinsic:
            nodeType = ArithFRound;
            break;
        case SqrtIntrinsic:
            nodeType = ArithSqrt;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        insertChecks();
        set(result, addToGraph(nodeType, get(virtualRegisterForArgument(1, registerOffset))));
        return true;
    }

    case PowIntrinsic: {
        if (argumentCountIncludingThis < 3) {
            // Math.pow() and Math.pow(x) return NaN.
            insertChecks();
            set(result, addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }
        insertChecks();
        VirtualRegister xOperand = virtualRegisterForArgument(1, registerOffset);
        VirtualRegister yOperand = virtualRegisterForArgument(2, registerOffset);
        set(result, addToGraph(ArithPow, get(xOperand), get(yOperand)));
        return true;
    }
        
    case ArrayPushIntrinsic: {
#if USE(JSVALUE32_64)
        if (isX86()) {
            if (argumentCountIncludingThis > 2)
                return false;
        }
#endif

        if (static_cast<unsigned>(argumentCountIncludingThis) >= MIN_SPARSE_ARRAY_INDEX)
            return false;
        
        ArrayMode arrayMode = getArrayMode(Array::Write);
        if (!arrayMode.isJSArray())
            return false;
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage: {
            insertChecks();

            addVarArgChild(nullptr); // For storage.
            for (int i = 0; i < argumentCountIncludingThis; ++i)
                addVarArgChild(get(virtualRegisterForArgument(i, registerOffset)));
            Node* arrayPush = addToGraph(Node::VarArg, ArrayPush, OpInfo(arrayMode.asWord()), OpInfo(prediction));
            set(result, arrayPush);
            
            return true;
        }
            
        default:
            return false;
        }
    }

    case ArraySliceIntrinsic: {
#if USE(JSVALUE32_64)
        if (isX86()) {
            // There aren't enough registers for this to be done easily.
            return false;
        }
#endif
        if (argumentCountIncludingThis < 1)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantCache)
            || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache))
            return false;

        ArrayMode arrayMode = getArrayMode(Array::Read);
        if (!arrayMode.isJSArray())
            return false;

        if (!arrayMode.isJSArrayWithOriginalStructure())
            return false;

        switch (arrayMode.type()) {
        case Array::Double:
        case Array::Int32:
        case Array::Contiguous: {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);

            Structure* arrayPrototypeStructure = globalObject->arrayPrototype()->structure(*m_vm);
            Structure* objectPrototypeStructure = globalObject->objectPrototype()->structure(*m_vm);

            // FIXME: We could easily relax the Array/Object.prototype transition as long as we OSR exitted if we saw a hole.
            // https://bugs.webkit.org/show_bug.cgi?id=173171
            if (globalObject->arraySpeciesWatchpoint().state() == IsWatched
                && globalObject->havingABadTimeWatchpoint()->isStillValid()
                && arrayPrototypeStructure->transitionWatchpointSetIsStillValid()
                && objectPrototypeStructure->transitionWatchpointSetIsStillValid()
                && globalObject->arrayPrototypeChainIsSane()) {

                m_graph.watchpoints().addLazily(globalObject->arraySpeciesWatchpoint());
                m_graph.watchpoints().addLazily(globalObject->havingABadTimeWatchpoint());
                m_graph.registerAndWatchStructureTransition(arrayPrototypeStructure);
                m_graph.registerAndWatchStructureTransition(objectPrototypeStructure);

                insertChecks();

                Node* array = get(virtualRegisterForArgument(0, registerOffset));
                // We do a few things here to prove that we aren't skipping doing side-effects in an observable way:
                // 1. We ensure that the "constructor" property hasn't been changed (because the observable
                // effects of slice require that we perform a Get(array, "constructor") and we can skip
                // that if we're an original array structure. (We can relax this in the future by using
                // TryGetById and CheckCell).
                //
                // 2. We check that the array we're calling slice on has the same global object as the lexical
                // global object that this code is running in. This requirement is necessary because we setup the
                // watchpoints above on the lexical global object. This means that code that calls slice on
                // arrays produced by other global objects won't get this optimization. We could relax this
                // requirement in the future by checking that the watchpoint hasn't fired at runtime in the code
                // we generate instead of registering it as a watchpoint that would invalidate the compilation.
                //
                // 3. By proving we're an original array structure, we guarantee that the incoming array
                // isn't a subclass of Array.

                StructureSet structureSet;
                structureSet.add(globalObject->originalArrayStructureForIndexingType(ArrayWithInt32));
                structureSet.add(globalObject->originalArrayStructureForIndexingType(ArrayWithContiguous));
                structureSet.add(globalObject->originalArrayStructureForIndexingType(ArrayWithDouble));
                structureSet.add(globalObject->originalArrayStructureForIndexingType(CopyOnWriteArrayWithInt32));
                structureSet.add(globalObject->originalArrayStructureForIndexingType(CopyOnWriteArrayWithContiguous));
                structureSet.add(globalObject->originalArrayStructureForIndexingType(CopyOnWriteArrayWithDouble));
                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(structureSet)), array);

                addVarArgChild(array);
                if (argumentCountIncludingThis >= 2)
                    addVarArgChild(get(virtualRegisterForArgument(1, registerOffset))); // Start index.
                if (argumentCountIncludingThis >= 3)
                    addVarArgChild(get(virtualRegisterForArgument(2, registerOffset))); // End index.
                addVarArgChild(addToGraph(GetButterfly, array));

                Node* arraySlice = addToGraph(Node::VarArg, ArraySlice, OpInfo(), OpInfo());
                set(result, arraySlice);
                return true;
            }

            return false;
        }
        default:
            return false;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    case ArrayIndexOfIntrinsic: {
        if (argumentCountIncludingThis < 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType)
            || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantCache)
            || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
            || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        ArrayMode arrayMode = getArrayMode(Array::Read);
        if (!arrayMode.isJSArray())
            return false;

        if (!arrayMode.isJSArrayWithOriginalStructure())
            return false;

        // We do not want to convert arrays into one type just to perform indexOf.
        if (arrayMode.doesConversion())
            return false;

        switch (arrayMode.type()) {
        case Array::Double:
        case Array::Int32:
        case Array::Contiguous: {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);

            Structure* arrayPrototypeStructure = globalObject->arrayPrototype()->structure(*m_vm);
            Structure* objectPrototypeStructure = globalObject->objectPrototype()->structure(*m_vm);

            // FIXME: We could easily relax the Array/Object.prototype transition as long as we OSR exitted if we saw a hole.
            // https://bugs.webkit.org/show_bug.cgi?id=173171
            if (arrayPrototypeStructure->transitionWatchpointSetIsStillValid()
                && objectPrototypeStructure->transitionWatchpointSetIsStillValid()
                && globalObject->arrayPrototypeChainIsSane()) {

                m_graph.registerAndWatchStructureTransition(arrayPrototypeStructure);
                m_graph.registerAndWatchStructureTransition(objectPrototypeStructure);

                insertChecks();

                Node* array = get(virtualRegisterForArgument(0, registerOffset));
                addVarArgChild(array);
                addVarArgChild(get(virtualRegisterForArgument(1, registerOffset))); // Search element.
                if (argumentCountIncludingThis >= 3)
                    addVarArgChild(get(virtualRegisterForArgument(2, registerOffset))); // Start index.
                addVarArgChild(nullptr);

                Node* node = addToGraph(Node::VarArg, ArrayIndexOf, OpInfo(arrayMode.asWord()), OpInfo());
                set(result, node);
                return true;
            }

            return false;
        }
        default:
            return false;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;

    }
        
    case ArrayPopIntrinsic: {
        if (argumentCountIncludingThis != 1)
            return false;
        
        ArrayMode arrayMode = getArrayMode(Array::Write);
        if (!arrayMode.isJSArray())
            return false;
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage: {
            insertChecks();
            Node* arrayPop = addToGraph(ArrayPop, OpInfo(arrayMode.asWord()), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)));
            set(result, arrayPop);
            return true;
        }
            
        default:
            return false;
        }
    }
        
    case AtomicsAddIntrinsic:
    case AtomicsAndIntrinsic:
    case AtomicsCompareExchangeIntrinsic:
    case AtomicsExchangeIntrinsic:
    case AtomicsIsLockFreeIntrinsic:
    case AtomicsLoadIntrinsic:
    case AtomicsOrIntrinsic:
    case AtomicsStoreIntrinsic:
    case AtomicsSubIntrinsic:
    case AtomicsXorIntrinsic: {
        if (!is64Bit())
            return false;
        
        NodeType op = LastNodeType;
        Array::Action action = Array::Write;
        unsigned numArgs = 0; // Number of actual args; we add one for the backing store pointer.
        switch (intrinsic) {
        case AtomicsAddIntrinsic:
            op = AtomicsAdd;
            numArgs = 3;
            break;
        case AtomicsAndIntrinsic:
            op = AtomicsAnd;
            numArgs = 3;
            break;
        case AtomicsCompareExchangeIntrinsic:
            op = AtomicsCompareExchange;
            numArgs = 4;
            break;
        case AtomicsExchangeIntrinsic:
            op = AtomicsExchange;
            numArgs = 3;
            break;
        case AtomicsIsLockFreeIntrinsic:
            // This gets no backing store, but we need no special logic for this since this also does
            // not need varargs.
            op = AtomicsIsLockFree;
            numArgs = 1;
            break;
        case AtomicsLoadIntrinsic:
            op = AtomicsLoad;
            numArgs = 2;
            action = Array::Read;
            break;
        case AtomicsOrIntrinsic:
            op = AtomicsOr;
            numArgs = 3;
            break;
        case AtomicsStoreIntrinsic:
            op = AtomicsStore;
            numArgs = 3;
            break;
        case AtomicsSubIntrinsic:
            op = AtomicsSub;
            numArgs = 3;
            break;
        case AtomicsXorIntrinsic:
            op = AtomicsXor;
            numArgs = 3;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        if (static_cast<unsigned>(argumentCountIncludingThis) < 1 + numArgs)
            return false;
        
        insertChecks();
        
        Vector<Node*, 3> args;
        for (unsigned i = 0; i < numArgs; ++i)
            args.append(get(virtualRegisterForArgument(1 + i, registerOffset)));
        
        Node* resultNode;
        if (numArgs + 1 <= 3) {
            while (args.size() < 3)
                args.append(nullptr);
            resultNode = addToGraph(op, OpInfo(ArrayMode(Array::SelectUsingPredictions, action).asWord()), OpInfo(prediction), args[0], args[1], args[2]);
        } else {
            for (Node* node : args)
                addVarArgChild(node);
            addVarArgChild(nullptr);
            resultNode = addToGraph(Node::VarArg, op, OpInfo(ArrayMode(Array::SelectUsingPredictions, action).asWord()), OpInfo(prediction));
        }
        
        set(result, resultNode);
        return true;
    }

    case ParseIntIntrinsic: {
        if (argumentCountIncludingThis < 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        VirtualRegister valueOperand = virtualRegisterForArgument(1, registerOffset);
        Node* parseInt;
        if (argumentCountIncludingThis == 2)
            parseInt = addToGraph(ParseInt, OpInfo(), OpInfo(prediction), get(valueOperand));
        else {
            ASSERT(argumentCountIncludingThis > 2);
            VirtualRegister radixOperand = virtualRegisterForArgument(2, registerOffset);
            parseInt = addToGraph(ParseInt, OpInfo(), OpInfo(prediction), get(valueOperand), get(radixOperand));
        }
        set(result, parseInt);
        return true;
    }

    case CharCodeAtIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        VirtualRegister thisOperand = virtualRegisterForArgument(0, registerOffset);
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringCharCodeAt, OpInfo(ArrayMode(Array::String, Array::Read).asWord()), get(thisOperand), get(indexOperand));

        set(result, charCode);
        return true;
    }

    case CharAtIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        VirtualRegister thisOperand = virtualRegisterForArgument(0, registerOffset);
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringCharAt, OpInfo(ArrayMode(Array::String, Array::Read).asWord()), get(thisOperand), get(indexOperand));

        set(result, charCode);
        return true;
    }
    case Clz32Intrinsic: {
        insertChecks();
        if (argumentCountIncludingThis == 1)
            set(result, addToGraph(JSConstant, OpInfo(m_graph.freeze(jsNumber(32)))));
        else {
            Node* operand = get(virtualRegisterForArgument(1, registerOffset));
            set(result, addToGraph(ArithClz32, operand));
        }
        return true;
    }
    case FromCharCodeIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        VirtualRegister indexOperand = virtualRegisterForArgument(1, registerOffset);
        Node* charCode = addToGraph(StringFromCharCode, get(indexOperand));

        set(result, charCode);

        return true;
    }

    case RegExpExecIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        insertChecks();
        Node* regExpExec = addToGraph(RegExpExec, OpInfo(0), OpInfo(prediction), addToGraph(GetGlobalObject, callee), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)));
        set(result, regExpExec);
        
        return true;
    }
        
    case RegExpTestIntrinsic:
    case RegExpTestFastIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        if (intrinsic == RegExpTestIntrinsic) {
            // Don't inline intrinsic if we exited due to one of the primordial RegExp checks failing.
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell))
                return false;

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();
            Structure* regExpStructure = globalObject->regExpStructure();
            m_graph.registerStructure(regExpStructure);
            ASSERT(regExpStructure->storedPrototype().isObject());
            ASSERT(regExpStructure->storedPrototype().asCell()->classInfo(*m_vm) == RegExpPrototype::info());

            FrozenValue* regExpPrototypeObjectValue = m_graph.freeze(regExpStructure->storedPrototype());
            Structure* regExpPrototypeStructure = regExpPrototypeObjectValue->structure();

            auto isRegExpPropertySame = [&] (JSValue primordialProperty, UniquedStringImpl* propertyUID) {
                JSValue currentProperty;
                if (!m_graph.getRegExpPrototypeProperty(regExpStructure->storedPrototypeObject(), regExpPrototypeStructure, propertyUID, currentProperty))
                    return false;
                
                return currentProperty == primordialProperty;
            };

            // Check that RegExp.exec is still the primordial RegExp.prototype.exec
            if (!isRegExpPropertySame(globalObject->regExpProtoExecFunction(), m_vm->propertyNames->exec.impl()))
                return false;

            // Check that regExpObject is actually a RegExp object.
            Node* regExpObject = get(virtualRegisterForArgument(0, registerOffset));
            addToGraph(Check, Edge(regExpObject, RegExpObjectUse));

            // Check that regExpObject's exec is actually the primodial RegExp.prototype.exec.
            UniquedStringImpl* execPropertyID = m_vm->propertyNames->exec.impl();
            unsigned execIndex = m_graph.identifiers().ensure(execPropertyID);
            Node* actualProperty = addToGraph(TryGetById, OpInfo(execIndex), OpInfo(SpecFunction), Edge(regExpObject, CellUse));
            FrozenValue* regExpPrototypeExec = m_graph.freeze(globalObject->regExpProtoExecFunction());
            addToGraph(CheckCell, OpInfo(regExpPrototypeExec), Edge(actualProperty, CellUse));
        }

        insertChecks();
        Node* regExpObject = get(virtualRegisterForArgument(0, registerOffset));
        Node* regExpExec = addToGraph(RegExpTest, OpInfo(0), OpInfo(prediction), addToGraph(GetGlobalObject, callee), regExpObject, get(virtualRegisterForArgument(1, registerOffset)));
        set(result, regExpExec);
        
        return true;
    }

    case RegExpMatchFastIntrinsic: {
        RELEASE_ASSERT(argumentCountIncludingThis == 2);

        insertChecks();
        Node* regExpMatch = addToGraph(RegExpMatchFast, OpInfo(0), OpInfo(prediction), addToGraph(GetGlobalObject, callee), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)));
        set(result, regExpMatch);
        return true;
    }

    case ObjectCreateIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        set(result, addToGraph(ObjectCreate, get(virtualRegisterForArgument(1, registerOffset))));
        return true;
    }

    case ObjectGetPrototypeOfIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        set(result, addToGraph(GetPrototypeOf, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(1, registerOffset))));
        return true;
    }

    case ObjectIsIntrinsic: {
        if (argumentCountIncludingThis < 3)
            return false;

        insertChecks();
        set(result, addToGraph(SameValue, get(virtualRegisterForArgument(1, registerOffset)), get(virtualRegisterForArgument(2, registerOffset))));
        return true;
    }

    case ReflectGetPrototypeOfIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        set(result, addToGraph(GetPrototypeOf, OpInfo(0), OpInfo(prediction), Edge(get(virtualRegisterForArgument(1, registerOffset)), ObjectUse)));
        return true;
    }

    case IsTypedArrayViewIntrinsic: {
        ASSERT(argumentCountIncludingThis == 2);

        insertChecks();
        set(result, addToGraph(IsTypedArrayView, OpInfo(prediction), get(virtualRegisterForArgument(1, registerOffset))));
        return true;
    }

    case StringPrototypeValueOfIntrinsic: {
        insertChecks();
        Node* value = get(virtualRegisterForArgument(0, registerOffset));
        set(result, addToGraph(StringValueOf, value));
        return true;
    }

    case StringPrototypeReplaceIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;

        // Don't inline intrinsic if we exited due to "search" not being a RegExp or String object.
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        // Don't inline intrinsic if we exited due to one of the primordial RegExp checks failing.
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell))
            return false;

        JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();
        Structure* regExpStructure = globalObject->regExpStructure();
        m_graph.registerStructure(regExpStructure);
        ASSERT(regExpStructure->storedPrototype().isObject());
        ASSERT(regExpStructure->storedPrototype().asCell()->classInfo(*m_vm) == RegExpPrototype::info());

        FrozenValue* regExpPrototypeObjectValue = m_graph.freeze(regExpStructure->storedPrototype());
        Structure* regExpPrototypeStructure = regExpPrototypeObjectValue->structure();

        auto isRegExpPropertySame = [&] (JSValue primordialProperty, UniquedStringImpl* propertyUID) {
            JSValue currentProperty;
            if (!m_graph.getRegExpPrototypeProperty(regExpStructure->storedPrototypeObject(), regExpPrototypeStructure, propertyUID, currentProperty))
                return false;

            return currentProperty == primordialProperty;
        };

        // Check that searchRegExp.exec is still the primordial RegExp.prototype.exec
        if (!isRegExpPropertySame(globalObject->regExpProtoExecFunction(), m_vm->propertyNames->exec.impl()))
            return false;

        // Check that searchRegExp.global is still the primordial RegExp.prototype.global
        if (!isRegExpPropertySame(globalObject->regExpProtoGlobalGetter(), m_vm->propertyNames->global.impl()))
            return false;

        // Check that searchRegExp.unicode is still the primordial RegExp.prototype.unicode
        if (!isRegExpPropertySame(globalObject->regExpProtoUnicodeGetter(), m_vm->propertyNames->unicode.impl()))
            return false;

        // Check that searchRegExp[Symbol.match] is still the primordial RegExp.prototype[Symbol.replace]
        if (!isRegExpPropertySame(globalObject->regExpProtoSymbolReplaceFunction(), m_vm->propertyNames->replaceSymbol.impl()))
            return false;

        insertChecks();

        Node* resultNode = addToGraph(StringReplace, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)), get(virtualRegisterForArgument(2, registerOffset)));
        set(result, resultNode);
        return true;
    }
        
    case StringPrototypeReplaceRegExpIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;
        
        insertChecks();
        Node* resultNode = addToGraph(StringReplaceRegExp, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)), get(virtualRegisterForArgument(2, registerOffset)));
        set(result, resultNode);
        return true;
    }
        
    case RoundIntrinsic:
    case FloorIntrinsic:
    case CeilIntrinsic:
    case TruncIntrinsic: {
        if (argumentCountIncludingThis == 1) {
            insertChecks();
            set(result, addToGraph(JSConstant, OpInfo(m_constantNaN)));
            return true;
        }
        insertChecks();
        Node* operand = get(virtualRegisterForArgument(1, registerOffset));
        NodeType op;
        if (intrinsic == RoundIntrinsic)
            op = ArithRound;
        else if (intrinsic == FloorIntrinsic)
            op = ArithFloor;
        else if (intrinsic == CeilIntrinsic)
            op = ArithCeil;
        else {
            ASSERT(intrinsic == TruncIntrinsic);
            op = ArithTrunc;
        }
        Node* roundNode = addToGraph(op, OpInfo(0), OpInfo(prediction), operand);
        set(result, roundNode);
        return true;
    }
    case IMulIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;
        insertChecks();
        VirtualRegister leftOperand = virtualRegisterForArgument(1, registerOffset);
        VirtualRegister rightOperand = virtualRegisterForArgument(2, registerOffset);
        Node* left = get(leftOperand);
        Node* right = get(rightOperand);
        set(result, addToGraph(ArithIMul, left, right));
        return true;
    }

    case RandomIntrinsic: {
        if (argumentCountIncludingThis != 1)
            return false;
        insertChecks();
        set(result, addToGraph(ArithRandom));
        return true;
    }
        
    case DFGTrueIntrinsic: {
        insertChecks();
        set(result, jsConstant(jsBoolean(true)));
        return true;
    }

    case FTLTrueIntrinsic: {
        insertChecks();
        set(result, jsConstant(jsBoolean(m_graph.m_plan.isFTL())));
        return true;
    }
        
    case OSRExitIntrinsic: {
        insertChecks();
        addToGraph(ForceOSRExit);
        set(result, addToGraph(JSConstant, OpInfo(m_constantUndefined)));
        return true;
    }
        
    case IsFinalTierIntrinsic: {
        insertChecks();
        set(result,
            jsConstant(jsBoolean(Options::useFTLJIT() ? m_graph.m_plan.isFTL() : true)));
        return true;
    }
        
    case SetInt32HeapPredictionIntrinsic: {
        insertChecks();
        for (int i = 1; i < argumentCountIncludingThis; ++i) {
            Node* node = get(virtualRegisterForArgument(i, registerOffset));
            if (node->hasHeapPrediction())
                node->setHeapPrediction(SpecInt32Only);
        }
        set(result, addToGraph(JSConstant, OpInfo(m_constantUndefined)));
        return true;
    }
        
    case CheckInt32Intrinsic: {
        insertChecks();
        for (int i = 1; i < argumentCountIncludingThis; ++i) {
            Node* node = get(virtualRegisterForArgument(i, registerOffset));
            addToGraph(Phantom, Edge(node, Int32Use));
        }
        set(result, jsConstant(jsBoolean(true)));
        return true;
    }
        
    case FiatInt52Intrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        insertChecks();
        VirtualRegister operand = virtualRegisterForArgument(1, registerOffset);
        if (enableInt52())
            set(result, addToGraph(FiatInt52, get(operand)));
        else
            set(result, get(operand));
        return true;
    }

    case JSMapGetIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        Node* map = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        Node* normalizedKey = addToGraph(NormalizeMapKey, key);
        Node* hash = addToGraph(MapHash, normalizedKey);
        Node* bucket = addToGraph(GetMapBucket, Edge(map, MapObjectUse), Edge(normalizedKey), Edge(hash));
        Node* resultNode = addToGraph(LoadValueFromMapBucket, OpInfo(BucketOwnerType::Map), OpInfo(prediction), bucket);
        set(result, resultNode);
        return true;
    }

    case JSSetHasIntrinsic:
    case JSMapHasIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        Node* mapOrSet = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        Node* normalizedKey = addToGraph(NormalizeMapKey, key);
        Node* hash = addToGraph(MapHash, normalizedKey);
        UseKind useKind = intrinsic == JSSetHasIntrinsic ? SetObjectUse : MapObjectUse;
        Node* bucket = addToGraph(GetMapBucket, OpInfo(0), Edge(mapOrSet, useKind), Edge(normalizedKey), Edge(hash));
        JSCell* sentinel = nullptr;
        if (intrinsic == JSMapHasIntrinsic)
            sentinel = m_vm->sentinelMapBucket.get();
        else
            sentinel = m_vm->sentinelSetBucket.get();

        FrozenValue* frozenPointer = m_graph.freeze(sentinel);
        Node* invertedResult = addToGraph(CompareEqPtr, OpInfo(frozenPointer), bucket);
        Node* resultNode = addToGraph(LogicalNot, invertedResult);
        set(result, resultNode);
        return true;
    }

    case JSSetAddIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        insertChecks();
        Node* base = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        Node* normalizedKey = addToGraph(NormalizeMapKey, key);
        Node* hash = addToGraph(MapHash, normalizedKey);
        addToGraph(SetAdd, base, normalizedKey, hash);
        set(result, base);
        return true;
    }

    case JSMapSetIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;

        insertChecks();
        Node* base = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        Node* value = get(virtualRegisterForArgument(2, registerOffset));

        Node* normalizedKey = addToGraph(NormalizeMapKey, key);
        Node* hash = addToGraph(MapHash, normalizedKey);

        addVarArgChild(base);
        addVarArgChild(normalizedKey);
        addVarArgChild(value);
        addVarArgChild(hash);
        addToGraph(Node::VarArg, MapSet, OpInfo(0), OpInfo(0));
        set(result, base);
        return true;
    }

    case JSSetBucketHeadIntrinsic:
    case JSMapBucketHeadIntrinsic: {
        ASSERT(argumentCountIncludingThis == 2);

        insertChecks();
        Node* map = get(virtualRegisterForArgument(1, registerOffset));
        UseKind useKind = intrinsic == JSSetBucketHeadIntrinsic ? SetObjectUse : MapObjectUse;
        Node* resultNode = addToGraph(GetMapBucketHead, Edge(map, useKind));
        set(result, resultNode);
        return true;
    }

    case JSSetBucketNextIntrinsic:
    case JSMapBucketNextIntrinsic: {
        ASSERT(argumentCountIncludingThis == 2);

        insertChecks();
        Node* bucket = get(virtualRegisterForArgument(1, registerOffset));
        BucketOwnerType type = intrinsic == JSSetBucketNextIntrinsic ? BucketOwnerType::Set : BucketOwnerType::Map;
        Node* resultNode = addToGraph(GetMapBucketNext, OpInfo(type), bucket);
        set(result, resultNode);
        return true;
    }

    case JSSetBucketKeyIntrinsic:
    case JSMapBucketKeyIntrinsic: {
        ASSERT(argumentCountIncludingThis == 2);

        insertChecks();
        Node* bucket = get(virtualRegisterForArgument(1, registerOffset));
        BucketOwnerType type = intrinsic == JSSetBucketKeyIntrinsic ? BucketOwnerType::Set : BucketOwnerType::Map;
        Node* resultNode = addToGraph(LoadKeyFromMapBucket, OpInfo(type), OpInfo(prediction), bucket);
        set(result, resultNode);
        return true;
    }

    case JSMapBucketValueIntrinsic: {
        ASSERT(argumentCountIncludingThis == 2);

        insertChecks();
        Node* bucket = get(virtualRegisterForArgument(1, registerOffset));
        Node* resultNode = addToGraph(LoadValueFromMapBucket, OpInfo(BucketOwnerType::Map), OpInfo(prediction), bucket);
        set(result, resultNode);
        return true;
    }

    case JSWeakMapGetIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* map = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        addToGraph(Check, Edge(key, ObjectUse));
        Node* hash = addToGraph(MapHash, key);
        Node* holder = addToGraph(WeakMapGet, Edge(map, WeakMapObjectUse), Edge(key, ObjectUse), Edge(hash, Int32Use));
        Node* resultNode = addToGraph(ExtractValueFromWeakMapGet, OpInfo(), OpInfo(prediction), holder);

        set(result, resultNode);
        return true;
    }

    case JSWeakMapHasIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* map = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        addToGraph(Check, Edge(key, ObjectUse));
        Node* hash = addToGraph(MapHash, key);
        Node* holder = addToGraph(WeakMapGet, Edge(map, WeakMapObjectUse), Edge(key, ObjectUse), Edge(hash, Int32Use));
        Node* invertedResult = addToGraph(IsEmpty, holder);
        Node* resultNode = addToGraph(LogicalNot, invertedResult);

        set(result, resultNode);
        return true;
    }

    case JSWeakSetHasIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* map = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        addToGraph(Check, Edge(key, ObjectUse));
        Node* hash = addToGraph(MapHash, key);
        Node* holder = addToGraph(WeakMapGet, Edge(map, WeakSetObjectUse), Edge(key, ObjectUse), Edge(hash, Int32Use));
        Node* invertedResult = addToGraph(IsEmpty, holder);
        Node* resultNode = addToGraph(LogicalNot, invertedResult);

        set(result, resultNode);
        return true;
    }

    case JSWeakSetAddIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* base = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        addToGraph(Check, Edge(key, ObjectUse));
        Node* hash = addToGraph(MapHash, key);
        addToGraph(WeakSetAdd, Edge(base, WeakSetObjectUse), Edge(key, ObjectUse), Edge(hash, Int32Use));
        set(result, base);
        return true;
    }

    case JSWeakMapSetIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* base = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        Node* value = get(virtualRegisterForArgument(2, registerOffset));

        addToGraph(Check, Edge(key, ObjectUse));
        Node* hash = addToGraph(MapHash, key);

        addVarArgChild(Edge(base, WeakMapObjectUse));
        addVarArgChild(Edge(key, ObjectUse));
        addVarArgChild(Edge(value));
        addVarArgChild(Edge(hash, Int32Use));
        addToGraph(Node::VarArg, WeakMapSet, OpInfo(0), OpInfo(0));
        set(result, base);
        return true;
    }

    case DataViewGetInt8:
    case DataViewGetUint8:
    case DataViewGetInt16:
    case DataViewGetUint16:
    case DataViewGetInt32:
    case DataViewGetUint32:
    case DataViewGetFloat32:
    case DataViewGetFloat64: {
        if (!is64Bit())
            return false;

        // To inline data view accesses, we assume the architecture we're running on:
        // - Is little endian.
        // - Allows unaligned loads/stores without crashing. 

        if (argumentCountIncludingThis < 2)
            return false;
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();

        uint8_t byteSize;
        NodeType op = DataViewGetInt;
        bool isSigned = false;
        switch (intrinsic) {
        case DataViewGetInt8:
            isSigned = true;
            FALLTHROUGH;
        case DataViewGetUint8:
            byteSize = 1;
            break;

        case DataViewGetInt16:
            isSigned = true;
            FALLTHROUGH;
        case DataViewGetUint16:
            byteSize = 2;
            break;

        case DataViewGetInt32:
            isSigned = true;
            FALLTHROUGH;
        case DataViewGetUint32:
            byteSize = 4;
            break;

        case DataViewGetFloat32:
            byteSize = 4;
            op = DataViewGetFloat;
            break;
        case DataViewGetFloat64:
            byteSize = 8;
            op = DataViewGetFloat;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        TriState isLittleEndian = MixedTriState;
        Node* littleEndianChild = nullptr;
        if (byteSize > 1) {
            if (argumentCountIncludingThis < 3)
                isLittleEndian = FalseTriState;
            else {
                littleEndianChild = get(virtualRegisterForArgument(2, registerOffset));
                if (littleEndianChild->hasConstant()) {
                    JSValue constant = littleEndianChild->constant()->value();
                    isLittleEndian = constant.pureToBoolean();
                    if (isLittleEndian != MixedTriState)
                        littleEndianChild = nullptr;
                } else
                    isLittleEndian = MixedTriState;
            }
        }

        DataViewData data { };
        data.isLittleEndian = isLittleEndian;
        data.isSigned = isSigned;
        data.byteSize = byteSize;

        set(VirtualRegister(result),
            addToGraph(op, OpInfo(data.asQuadWord), OpInfo(prediction), get(virtualRegisterForArgument(0, registerOffset)), get(virtualRegisterForArgument(1, registerOffset)), littleEndianChild));
        return true;
    }

    case DataViewSetInt8:
    case DataViewSetUint8:
    case DataViewSetInt16:
    case DataViewSetUint16:
    case DataViewSetInt32:
    case DataViewSetUint32:
    case DataViewSetFloat32:
    case DataViewSetFloat64: {
        if (!is64Bit())
            return false;

        if (argumentCountIncludingThis < 3)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();

        uint8_t byteSize;
        bool isFloatingPoint = false;
        bool isSigned = false;
        switch (intrinsic) {
        case DataViewSetInt8:
            isSigned = true;
            FALLTHROUGH;
        case DataViewSetUint8:
            byteSize = 1;
            break;

        case DataViewSetInt16:
            isSigned = true;
            FALLTHROUGH;
        case DataViewSetUint16:
            byteSize = 2;
            break;

        case DataViewSetInt32:
            isSigned = true;
            FALLTHROUGH;
        case DataViewSetUint32:
            byteSize = 4;
            break;

        case DataViewSetFloat32:
            isFloatingPoint = true;
            byteSize = 4;
            break;
        case DataViewSetFloat64:
            isFloatingPoint = true;
            byteSize = 8;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        TriState isLittleEndian = MixedTriState;
        Node* littleEndianChild = nullptr;
        if (byteSize > 1) {
            if (argumentCountIncludingThis < 4)
                isLittleEndian = FalseTriState;
            else {
                littleEndianChild = get(virtualRegisterForArgument(3, registerOffset));
                if (littleEndianChild->hasConstant()) {
                    JSValue constant = littleEndianChild->constant()->value();
                    isLittleEndian = constant.pureToBoolean();
                    if (isLittleEndian != MixedTriState)
                        littleEndianChild = nullptr;
                } else
                    isLittleEndian = MixedTriState;
            }
        }

        DataViewData data { };
        data.isLittleEndian = isLittleEndian;
        data.isSigned = isSigned;
        data.byteSize = byteSize;
        data.isFloatingPoint = isFloatingPoint;

        addVarArgChild(get(virtualRegisterForArgument(0, registerOffset)));
        addVarArgChild(get(virtualRegisterForArgument(1, registerOffset)));
        addVarArgChild(get(virtualRegisterForArgument(2, registerOffset)));
        addVarArgChild(littleEndianChild);

        addToGraph(Node::VarArg, DataViewSet, OpInfo(data.asQuadWord), OpInfo());
        return true;
    }

    case HasOwnPropertyIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        // This can be racy, that's fine. We know that once we observe that this is created,
        // that it will never be destroyed until the VM is destroyed. It's unlikely that
        // we'd ever get to the point where we inline this as an intrinsic without the
        // cache being created, however, it's possible if we always throw exceptions inside
        // hasOwnProperty.
        if (!m_vm->hasOwnPropertyCache())
            return false;

        insertChecks();
        Node* object = get(virtualRegisterForArgument(0, registerOffset));
        Node* key = get(virtualRegisterForArgument(1, registerOffset));
        Node* resultNode = addToGraph(HasOwnProperty, object, key);
        set(result, resultNode);
        return true;
    }

    case StringPrototypeSliceIntrinsic: {
        if (argumentCountIncludingThis < 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* thisString = get(virtualRegisterForArgument(0, registerOffset));
        Node* start = get(virtualRegisterForArgument(1, registerOffset));
        Node* end = nullptr;
        if (argumentCountIncludingThis > 2)
            end = get(virtualRegisterForArgument(2, registerOffset));
        Node* resultNode = addToGraph(StringSlice, thisString, start, end);
        set(result, resultNode);
        return true;
    }

    case StringPrototypeToLowerCaseIntrinsic: {
        if (argumentCountIncludingThis != 1)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* thisString = get(virtualRegisterForArgument(0, registerOffset));
        Node* resultNode = addToGraph(ToLowerCase, thisString);
        set(result, resultNode);
        return true;
    }

    case NumberPrototypeToStringIntrinsic: {
        if (argumentCountIncludingThis != 1 && argumentCountIncludingThis != 2)
            return false;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
            return false;

        insertChecks();
        Node* thisNumber = get(virtualRegisterForArgument(0, registerOffset));
        if (argumentCountIncludingThis == 1) {
            Node* resultNode = addToGraph(ToString, thisNumber);
            set(result, resultNode);
        } else {
            Node* radix = get(virtualRegisterForArgument(1, registerOffset));
            Node* resultNode = addToGraph(NumberToStringWithRadix, thisNumber, radix);
            set(result, resultNode);
        }
        return true;
    }

    case NumberIsIntegerIntrinsic: {
        if (argumentCountIncludingThis < 2)
            return false;

        insertChecks();
        Node* input = get(virtualRegisterForArgument(1, registerOffset));
        Node* resultNode = addToGraph(NumberIsInteger, input);
        set(result, resultNode);
        return true;
    }

    case CPUMfenceIntrinsic:
    case CPURdtscIntrinsic:
    case CPUCpuidIntrinsic:
    case CPUPauseIntrinsic: {
#if CPU(X86_64)
        if (!m_graph.m_plan.isFTL())
            return false;
        insertChecks();
        set(result,
            addToGraph(CPUIntrinsic, OpInfo(intrinsic), OpInfo()));
        return true;
#else
        return false;
#endif
    }


    default:
        return false;
    }
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleDOMJITCall(Node* callTarget, VirtualRegister result, const DOMJIT::Signature* signature, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks)
{
    if (argumentCountIncludingThis != static_cast<int>(1 + signature->argumentCount))
        return false;
    if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
        return false;

    // FIXME: Currently, we only support functions which arguments are up to 2.
    // Eventually, we should extend this. But possibly, 2 or 3 can cover typical use cases.
    // https://bugs.webkit.org/show_bug.cgi?id=164346
    ASSERT_WITH_MESSAGE(argumentCountIncludingThis <= JSC_DOMJIT_SIGNATURE_MAX_ARGUMENTS_INCLUDING_THIS, "Currently CallDOM does not support an arbitrary length arguments.");

    insertChecks();
    addCall(result, Call, signature, callTarget, argumentCountIncludingThis, registerOffset, prediction);
    return true;
}


template<typename ChecksFunctor>
bool ByteCodeParser::handleIntrinsicGetter(VirtualRegister result, SpeculatedType prediction, const GetByIdVariant& variant, Node* thisNode, const ChecksFunctor& insertChecks)
{
    switch (variant.intrinsic()) {
    case TypedArrayByteLengthIntrinsic: {
        insertChecks();

        TypedArrayType type = (*variant.structureSet().begin())->classInfo()->typedArrayStorageType;
        Array::Type arrayType = toArrayType(type);
        size_t logSize = logElementSize(type);

        variant.structureSet().forEach([&] (Structure* structure) {
            TypedArrayType curType = structure->classInfo()->typedArrayStorageType;
            ASSERT(logSize == logElementSize(curType));
            arrayType = refineTypedArrayType(arrayType, curType);
            ASSERT(arrayType != Array::Generic);
        });

        Node* lengthNode = addToGraph(GetArrayLength, OpInfo(ArrayMode(arrayType, Array::Read).asWord()), thisNode);

        if (!logSize) {
            set(result, lengthNode);
            return true;
        }

        // We can use a BitLShift here because typed arrays will never have a byteLength
        // that overflows int32.
        Node* shiftNode = jsConstant(jsNumber(logSize));
        set(result, addToGraph(BitLShift, lengthNode, shiftNode));

        return true;
    }

    case TypedArrayLengthIntrinsic: {
        insertChecks();

        TypedArrayType type = (*variant.structureSet().begin())->classInfo()->typedArrayStorageType;
        Array::Type arrayType = toArrayType(type);

        variant.structureSet().forEach([&] (Structure* structure) {
            TypedArrayType curType = structure->classInfo()->typedArrayStorageType;
            arrayType = refineTypedArrayType(arrayType, curType);
            ASSERT(arrayType != Array::Generic);
        });

        set(result, addToGraph(GetArrayLength, OpInfo(ArrayMode(arrayType, Array::Read).asWord()), thisNode));

        return true;

    }

    case TypedArrayByteOffsetIntrinsic: {
        insertChecks();

        TypedArrayType type = (*variant.structureSet().begin())->classInfo()->typedArrayStorageType;
        Array::Type arrayType = toArrayType(type);

        variant.structureSet().forEach([&] (Structure* structure) {
            TypedArrayType curType = structure->classInfo()->typedArrayStorageType;
            arrayType = refineTypedArrayType(arrayType, curType);
            ASSERT(arrayType != Array::Generic);
        });

        set(result, addToGraph(GetTypedArrayByteOffset, OpInfo(ArrayMode(arrayType, Array::Read).asWord()), thisNode));

        return true;
    }

    case UnderscoreProtoIntrinsic: {
        insertChecks();

        bool canFold = !variant.structureSet().isEmpty();
        JSValue prototype;
        variant.structureSet().forEach([&] (Structure* structure) {
            auto getPrototypeMethod = structure->classInfo()->methodTable.getPrototype;
            MethodTable::GetPrototypeFunctionPtr defaultGetPrototype = JSObject::getPrototype;
            if (getPrototypeMethod != defaultGetPrototype) {
                canFold = false;
                return;
            }

            if (structure->hasPolyProto()) {
                canFold = false;
                return;
            }
            if (!prototype)
                prototype = structure->storedPrototype();
            else if (prototype != structure->storedPrototype())
                canFold = false;
        });

        // OK, only one prototype is found. We perform constant folding here.
        // This information is important for super's constructor call to get new.target constant.
        if (prototype && canFold) {
            set(result, weakJSConstant(prototype));
            return true;
        }

        set(result, addToGraph(GetPrototypeOf, OpInfo(0), OpInfo(prediction), thisNode));
        return true;
    }

    default:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static void blessCallDOMGetter(Node* node)
{
    DOMJIT::CallDOMGetterSnippet* snippet = node->callDOMGetterData()->snippet;
    if (snippet && !snippet->effect.mustGenerate())
        node->clearFlags(NodeMustGenerate);
}

bool ByteCodeParser::handleDOMJITGetter(VirtualRegister result, const GetByIdVariant& variant, Node* thisNode, unsigned identifierNumber, SpeculatedType prediction)
{
    if (!variant.domAttribute())
        return false;

    auto domAttribute = variant.domAttribute().value();

    // We do not need to actually look up CustomGetterSetter here. Checking Structures or registering watchpoints are enough,
    // since replacement of CustomGetterSetter always incurs Structure transition.
    if (!check(variant.conditionSet()))
        return false;
    addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.structureSet())), thisNode);
    
    // We do not need to emit CheckCell thingy here. When the custom accessor is replaced to different one, Structure transition occurs.
    addToGraph(CheckSubClass, OpInfo(domAttribute.classInfo), thisNode);
    
    bool wasSeenInJIT = true;
    addToGraph(FilterGetByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByIdStatus(currentCodeOrigin(), GetByIdStatus(GetByIdStatus::Custom, wasSeenInJIT, variant))), thisNode);

    CallDOMGetterData* callDOMGetterData = m_graph.m_callDOMGetterData.add();
    callDOMGetterData->customAccessorGetter = variant.customAccessorGetter();
    ASSERT(callDOMGetterData->customAccessorGetter);

    if (const auto* domJIT = domAttribute.domJIT) {
        callDOMGetterData->domJIT = domJIT;
        Ref<DOMJIT::CallDOMGetterSnippet> snippet = domJIT->compiler()();
        callDOMGetterData->snippet = snippet.ptr();
        m_graph.m_domJITSnippets.append(WTFMove(snippet));
    }
    DOMJIT::CallDOMGetterSnippet* callDOMGetterSnippet = callDOMGetterData->snippet;
    callDOMGetterData->identifierNumber = identifierNumber;

    Node* callDOMGetterNode = nullptr;
    // GlobalObject of thisNode is always used to create a DOMWrapper.
    if (callDOMGetterSnippet && callDOMGetterSnippet->requireGlobalObject) {
        Node* globalObject = addToGraph(GetGlobalObject, thisNode);
        callDOMGetterNode = addToGraph(CallDOMGetter, OpInfo(callDOMGetterData), OpInfo(prediction), thisNode, globalObject);
    } else
        callDOMGetterNode = addToGraph(CallDOMGetter, OpInfo(callDOMGetterData), OpInfo(prediction), thisNode);
    blessCallDOMGetter(callDOMGetterNode);
    set(result, callDOMGetterNode);
    return true;
}

bool ByteCodeParser::handleModuleNamespaceLoad(VirtualRegister result, SpeculatedType prediction, Node* base, GetByIdStatus getById)
{
    if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell))
        return false;
    addToGraph(CheckCell, OpInfo(m_graph.freeze(getById.moduleNamespaceObject())), Edge(base, CellUse));

    addToGraph(FilterGetByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByIdStatus(currentCodeOrigin(), getById)), base);

    // Ideally we wouldn't have to do this Phantom. But:
    //
    // For the constant case: we must do it because otherwise we would have no way of knowing
    // that the scope is live at OSR here.
    //
    // For the non-constant case: GetClosureVar could be DCE'd, but baseline's implementation
    // won't be able to handle an Undefined scope.
    addToGraph(Phantom, base);

    // Constant folding in the bytecode parser is important for performance. This may not
    // have executed yet. If it hasn't, then we won't have a prediction. Lacking a
    // prediction, we'd otherwise think that it has to exit. Then when it did execute, we
    // would recompile. But if we can fold it here, we avoid the exit.
    m_graph.freeze(getById.moduleEnvironment());
    if (JSValue value = m_graph.tryGetConstantClosureVar(getById.moduleEnvironment(), getById.scopeOffset())) {
        set(result, weakJSConstant(value));
        return true;
    }
    set(result, addToGraph(GetClosureVar, OpInfo(getById.scopeOffset().offset()), OpInfo(prediction), weakJSConstant(getById.moduleEnvironment())));
    return true;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleTypedArrayConstructor(
    VirtualRegister result, InternalFunction* function, int registerOffset,
    int argumentCountIncludingThis, TypedArrayType type, const ChecksFunctor& insertChecks)
{
    if (!isTypedView(type))
        return false;
    
    if (function->classInfo() != constructorClassInfoForType(type))
        return false;
    
    if (function->globalObject(*m_vm) != m_inlineStackTop->m_codeBlock->globalObject())
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
    
    if (!function->globalObject(*m_vm)->typedArrayStructureConcurrently(type))
        return false;

    insertChecks();
    set(result,
        addToGraph(NewTypedArray, OpInfo(type), get(virtualRegisterForArgument(1, registerOffset))));
    return true;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleConstantInternalFunction(
    Node* callTargetNode, VirtualRegister result, InternalFunction* function, int registerOffset,
    int argumentCountIncludingThis, CodeSpecializationKind kind, SpeculatedType prediction, const ChecksFunctor& insertChecks)
{
    VERBOSE_LOG("    Handling constant internal function ", JSValue(function), "\n");
    
    // It so happens that the code below assumes that the result operand is valid. It's extremely
    // unlikely that the result operand would be invalid - you'd have to call this via a setter call.
    if (!result.isValid())
        return false;

    if (kind == CodeForConstruct) {
        Node* newTargetNode = get(virtualRegisterForArgument(0, registerOffset));
        // We cannot handle the case where new.target != callee (i.e. a construct from a super call) because we
        // don't know what the prototype of the constructed object will be.
        // FIXME: If we have inlined super calls up to the call site, however, we should be able to figure out the structure. https://bugs.webkit.org/show_bug.cgi?id=152700
        if (newTargetNode != callTargetNode)
            return false;
    }

    if (function->classInfo() == ArrayConstructor::info()) {
        if (function->globalObject(*m_vm) != m_inlineStackTop->m_codeBlock->globalObject())
            return false;
        
        insertChecks();
        if (argumentCountIncludingThis == 2) {
            set(result,
                addToGraph(NewArrayWithSize, OpInfo(ArrayWithUndecided), get(virtualRegisterForArgument(1, registerOffset))));
            return true;
        }
        
        for (int i = 1; i < argumentCountIncludingThis; ++i)
            addVarArgChild(get(virtualRegisterForArgument(i, registerOffset)));
        set(result,
            addToGraph(Node::VarArg, NewArray, OpInfo(ArrayWithUndecided), OpInfo(argumentCountIncludingThis - 1)));
        return true;
    }

    if (function->classInfo() == NumberConstructor::info()) {
        if (kind == CodeForConstruct)
            return false;

        insertChecks();
        if (argumentCountIncludingThis <= 1)
            set(result, jsConstant(jsNumber(0)));
        else
            set(result, addToGraph(ToNumber, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgument(1, registerOffset))));

        return true;
    }
    
    if (function->classInfo() == StringConstructor::info()) {
        insertChecks();
        
        Node* resultNode;
        
        if (argumentCountIncludingThis <= 1)
            resultNode = jsConstant(m_vm->smallStrings.emptyString());
        else
            resultNode = addToGraph(CallStringConstructor, get(virtualRegisterForArgument(1, registerOffset)));
        
        if (kind == CodeForConstruct)
            resultNode = addToGraph(NewStringObject, OpInfo(m_graph.registerStructure(function->globalObject(*m_vm)->stringObjectStructure())), resultNode);
        
        set(result, resultNode);
        return true;
    }

    // FIXME: This should handle construction as well. https://bugs.webkit.org/show_bug.cgi?id=155591
    if (function->classInfo() == ObjectConstructor::info() && kind == CodeForCall) {
        insertChecks();

        Node* resultNode;
        if (argumentCountIncludingThis <= 1)
            resultNode = addToGraph(NewObject, OpInfo(m_graph.registerStructure(function->globalObject(*m_vm)->objectStructureForObjectConstructor())));
        else
            resultNode = addToGraph(CallObjectConstructor, OpInfo(m_graph.freeze(function->globalObject(*m_vm))), OpInfo(prediction), get(virtualRegisterForArgument(1, registerOffset)));
        set(result, resultNode);
        return true;
    }

    for (unsigned typeIndex = 0; typeIndex < NumberOfTypedArrayTypes; ++typeIndex) {
        bool handled = handleTypedArrayConstructor(
            result, function, registerOffset, argumentCountIncludingThis,
            indexToTypedArrayType(typeIndex), insertChecks);
        if (handled)
            return true;
    }
    
    return false;
}

Node* ByteCodeParser::handleGetByOffset(
    SpeculatedType prediction, Node* base, unsigned identifierNumber, PropertyOffset offset,
    const InferredType::Descriptor& inferredType, NodeType op)
{
    Node* propertyStorage;
    if (isInlineOffset(offset))
        propertyStorage = base;
    else
        propertyStorage = addToGraph(GetButterfly, base);
    
    StorageAccessData* data = m_graph.m_storageAccessData.add();
    data->offset = offset;
    data->identifierNumber = identifierNumber;
    data->inferredType = inferredType;
    m_graph.registerInferredType(inferredType);
    
    Node* getByOffset = addToGraph(op, OpInfo(data), OpInfo(prediction), propertyStorage, base);

    return getByOffset;
}

Node* ByteCodeParser::handlePutByOffset(
    Node* base, unsigned identifier, PropertyOffset offset, const InferredType::Descriptor& inferredType,
    Node* value)
{
    Node* propertyStorage;
    if (isInlineOffset(offset))
        propertyStorage = base;
    else
        propertyStorage = addToGraph(GetButterfly, base);
    
    StorageAccessData* data = m_graph.m_storageAccessData.add();
    data->offset = offset;
    data->identifierNumber = identifier;
    data->inferredType = inferredType;
    m_graph.registerInferredType(inferredType);
    
    Node* result = addToGraph(PutByOffset, OpInfo(data), propertyStorage, base, value);
    
    return result;
}

bool ByteCodeParser::check(const ObjectPropertyCondition& condition)
{
    if (!condition)
        return false;
    
    if (m_graph.watchCondition(condition))
        return true;
    
    Structure* structure = condition.object()->structure(*m_vm);
    if (!condition.structureEnsuresValidity(structure))
        return false;
    
    addToGraph(
        CheckStructure,
        OpInfo(m_graph.addStructureSet(structure)),
        weakJSConstant(condition.object()));
    return true;
}

GetByOffsetMethod ByteCodeParser::promoteToConstant(GetByOffsetMethod method)
{
    if (method.kind() == GetByOffsetMethod::LoadFromPrototype
        && method.prototype()->structure()->dfgShouldWatch()) {
        if (JSValue constant = m_graph.tryGetConstantProperty(method.prototype()->value(), method.prototype()->structure(), method.offset()))
            return GetByOffsetMethod::constant(m_graph.freeze(constant));
    }
    
    return method;
}

bool ByteCodeParser::needsDynamicLookup(ResolveType type, OpcodeID opcode)
{
    ASSERT(opcode == op_resolve_scope || opcode == op_get_from_scope || opcode == op_put_to_scope);

    JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();
    if (needsVarInjectionChecks(type) && globalObject->varInjectionWatchpoint()->hasBeenInvalidated())
        return true;

    switch (type) {
    case GlobalProperty:
    case GlobalVar:
    case GlobalLexicalVar:
    case ClosureVar:
    case LocalClosureVar:
    case ModuleVar:
        return false;

    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        // The heuristic for UnresolvedProperty scope accesses is we will ForceOSRExit if we
        // haven't exited from from this access before to let the baseline JIT try to better
        // cache the access. If we've already exited from this operation, it's unlikely that
        // the baseline will come up with a better ResolveType and instead we will compile
        // this as a dynamic scope access.

        // We only track our heuristic through resolve_scope since resolve_scope will
        // dominate unresolved gets/puts on that scope.
        if (opcode != op_resolve_scope)
            return true;

        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, InadequateCoverage)) {
            // We've already exited so give up on getting better ResolveType information.
            return true;
        }

        // We have not exited yet, so let's have the baseline get better ResolveType information for us.
        // This type of code is often seen when we tier up in a loop but haven't executed the part
        // of a function that comes after the loop.
        return false;
    }

    case Dynamic:
        return true;

    case GlobalPropertyWithVarInjectionChecks:
    case GlobalVarWithVarInjectionChecks:
    case GlobalLexicalVarWithVarInjectionChecks:
    case ClosureVarWithVarInjectionChecks:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

GetByOffsetMethod ByteCodeParser::planLoad(const ObjectPropertyCondition& condition)
{
    VERBOSE_LOG("Planning a load: ", condition, "\n");
    
    // We might promote this to Equivalence, and a later DFG pass might also do such promotion
    // even if we fail, but for simplicity this cannot be asked to load an equivalence condition.
    // None of the clients of this method will request a load of an Equivalence condition anyway,
    // and supporting it would complicate the heuristics below.
    RELEASE_ASSERT(condition.kind() == PropertyCondition::Presence);
    
    // Here's the ranking of how to handle this, from most preferred to least preferred:
    //
    // 1) Watchpoint on an equivalence condition and return a constant node for the loaded value.
    //    No other code is emitted, and the structure of the base object is never registered.
    //    Hence this results in zero code and we won't jettison this compilation if the object
    //    transitions, even if the structure is watchable right now.
    //
    // 2) Need to emit a load, and the current structure of the base is going to be watched by the
    //    DFG anyway (i.e. dfgShouldWatch). Watch the structure and emit the load. Don't watch the
    //    condition, since the act of turning the base into a constant in IR will cause the DFG to
    //    watch the structure anyway and doing so would subsume watching the condition.
    //
    // 3) Need to emit a load, and the current structure of the base is watchable but not by the
    //    DFG (i.e. transitionWatchpointSetIsStillValid() and !dfgShouldWatchIfPossible()). Watch
    //    the condition, and emit a load.
    //
    // 4) Need to emit a load, and the current structure of the base is not watchable. Emit a
    //    structure check, and emit a load.
    //
    // 5) The condition does not hold. Give up and return null.
    
    // First, try to promote Presence to Equivalence. We do this before doing anything else
    // because it's the most profitable. Also, there are cases where the presence is watchable but
    // we don't want to watch it unless it became an equivalence (see the relationship between
    // (1), (2), and (3) above).
    ObjectPropertyCondition equivalenceCondition = condition.attemptToMakeEquivalenceWithoutBarrier(*m_vm);
    if (m_graph.watchCondition(equivalenceCondition))
        return GetByOffsetMethod::constant(m_graph.freeze(equivalenceCondition.requiredValue()));
    
    // At this point, we'll have to materialize the condition's base as a constant in DFG IR. Once
    // we do this, the frozen value will have its own idea of what the structure is. Use that from
    // now on just because it's less confusing.
    FrozenValue* base = m_graph.freeze(condition.object());
    Structure* structure = base->structure();
    
    // Check if the structure that we've registered makes the condition hold. If not, just give
    // up. This is case (5) above.
    if (!condition.structureEnsuresValidity(structure))
        return GetByOffsetMethod();
    
    // If the structure is watched by the DFG already, then just use this fact to emit the load.
    // This is case (2) above.
    if (structure->dfgShouldWatch())
        return promoteToConstant(GetByOffsetMethod::loadFromPrototype(base, condition.offset()));
    
    // If we can watch the condition right now, then we can emit the load after watching it. This
    // is case (3) above.
    if (m_graph.watchCondition(condition))
        return promoteToConstant(GetByOffsetMethod::loadFromPrototype(base, condition.offset()));
    
    // We can't watch anything but we know that the current structure satisfies the condition. So,
    // check for that structure and then emit the load.
    addToGraph(
        CheckStructure, 
        OpInfo(m_graph.addStructureSet(structure)),
        addToGraph(JSConstant, OpInfo(base)));
    return promoteToConstant(GetByOffsetMethod::loadFromPrototype(base, condition.offset()));
}

Node* ByteCodeParser::load(
    SpeculatedType prediction, unsigned identifierNumber, const GetByOffsetMethod& method,
    NodeType op)
{
    switch (method.kind()) {
    case GetByOffsetMethod::Invalid:
        return nullptr;
    case GetByOffsetMethod::Constant:
        return addToGraph(JSConstant, OpInfo(method.constant()));
    case GetByOffsetMethod::LoadFromPrototype: {
        Node* baseNode = addToGraph(JSConstant, OpInfo(method.prototype()));
        return handleGetByOffset(
            prediction, baseNode, identifierNumber, method.offset(), InferredType::Top, op);
    }
    case GetByOffsetMethod::Load:
        // Will never see this from planLoad().
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

Node* ByteCodeParser::load(
    SpeculatedType prediction, const ObjectPropertyCondition& condition, NodeType op)
{
    GetByOffsetMethod method = planLoad(condition);
    return load(prediction, m_graph.identifiers().ensure(condition.uid()), method, op);
}

bool ByteCodeParser::check(const ObjectPropertyConditionSet& conditionSet)
{
    for (const ObjectPropertyCondition& condition : conditionSet) {
        if (!check(condition))
            return false;
    }
    return true;
}

GetByOffsetMethod ByteCodeParser::planLoad(const ObjectPropertyConditionSet& conditionSet)
{
    VERBOSE_LOG("conditionSet = ", conditionSet, "\n");
    
    GetByOffsetMethod result;
    for (const ObjectPropertyCondition& condition : conditionSet) {
        switch (condition.kind()) {
        case PropertyCondition::Presence:
            RELEASE_ASSERT(!result); // Should only see exactly one of these.
            result = planLoad(condition);
            if (!result)
                return GetByOffsetMethod();
            break;
        default:
            if (!check(condition))
                return GetByOffsetMethod();
            break;
        }
    }
    if (!result) {
        // We have a unset property.
        ASSERT(!conditionSet.numberOfConditionsWithKind(PropertyCondition::Presence));
        return GetByOffsetMethod::constant(m_constantUndefined);
    }
    return result;
}

Node* ByteCodeParser::load(
    SpeculatedType prediction, const ObjectPropertyConditionSet& conditionSet, NodeType op)
{
    GetByOffsetMethod method = planLoad(conditionSet);
    return load(
        prediction,
        m_graph.identifiers().ensure(conditionSet.slotBaseCondition().uid()),
        method, op);
}

ObjectPropertyCondition ByteCodeParser::presenceLike(
    JSObject* knownBase, UniquedStringImpl* uid, PropertyOffset offset, const StructureSet& set)
{
    if (set.isEmpty())
        return ObjectPropertyCondition();
    unsigned attributes;
    PropertyOffset firstOffset = set[0]->getConcurrently(uid, attributes);
    if (firstOffset != offset)
        return ObjectPropertyCondition();
    for (unsigned i = 1; i < set.size(); ++i) {
        unsigned otherAttributes;
        PropertyOffset otherOffset = set[i]->getConcurrently(uid, otherAttributes);
        if (otherOffset != offset || otherAttributes != attributes)
            return ObjectPropertyCondition();
    }
    return ObjectPropertyCondition::presenceWithoutBarrier(knownBase, uid, offset, attributes);
}

bool ByteCodeParser::checkPresenceLike(
    JSObject* knownBase, UniquedStringImpl* uid, PropertyOffset offset, const StructureSet& set)
{
    return check(presenceLike(knownBase, uid, offset, set));
}

void ByteCodeParser::checkPresenceLike(
    Node* base, UniquedStringImpl* uid, PropertyOffset offset, const StructureSet& set)
{
    if (JSObject* knownBase = base->dynamicCastConstant<JSObject*>(*m_vm)) {
        if (checkPresenceLike(knownBase, uid, offset, set))
            return;
    }

    addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(set)), base);
}

template<typename VariantType>
Node* ByteCodeParser::load(
    SpeculatedType prediction, Node* base, unsigned identifierNumber, const VariantType& variant)
{
    // Make sure backwards propagation knows that we've used base.
    addToGraph(Phantom, base);
    
    bool needStructureCheck = true;
    
    UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
    
    if (JSObject* knownBase = base->dynamicCastConstant<JSObject*>(*m_vm)) {
        // Try to optimize away the structure check. Note that it's not worth doing anything about this
        // if the base's structure is watched.
        Structure* structure = base->constant()->structure();
        if (!structure->dfgShouldWatch()) {
            if (!variant.conditionSet().isEmpty()) {
                // This means that we're loading from a prototype or we have a property miss. We expect
                // the base not to have the property. We can only use ObjectPropertyCondition if all of
                // the structures in the variant.structureSet() agree on the prototype (it would be
                // hilariously rare if they didn't). Note that we are relying on structureSet() having
                // at least one element. That will always be true here because of how GetByIdStatus/PutByIdStatus work.

                // FIXME: right now, if we have an OPCS, we have mono proto. However, this will
                // need to be changed in the future once we have a hybrid data structure for
                // poly proto:
                // https://bugs.webkit.org/show_bug.cgi?id=177339
                JSObject* prototype = variant.structureSet()[0]->storedPrototypeObject();
                bool allAgree = true;
                for (unsigned i = 1; i < variant.structureSet().size(); ++i) {
                    if (variant.structureSet()[i]->storedPrototypeObject() != prototype) {
                        allAgree = false;
                        break;
                    }
                }
                if (allAgree) {
                    ObjectPropertyCondition condition = ObjectPropertyCondition::absenceWithoutBarrier(
                        knownBase, uid, prototype);
                    if (check(condition))
                        needStructureCheck = false;
                }
            } else {
                // This means we're loading directly from base. We can avoid all of the code that follows
                // if we can prove that the property is a constant. Otherwise, we try to prove that the
                // property is watchably present, in which case we get rid of the structure check.

                ObjectPropertyCondition presenceCondition =
                    presenceLike(knownBase, uid, variant.offset(), variant.structureSet());
                if (presenceCondition) {
                    ObjectPropertyCondition equivalenceCondition =
                        presenceCondition.attemptToMakeEquivalenceWithoutBarrier(*m_vm);
                    if (m_graph.watchCondition(equivalenceCondition))
                        return weakJSConstant(equivalenceCondition.requiredValue());
                    
                    if (check(presenceCondition))
                        needStructureCheck = false;
                }
            }
        }
    }

    if (needStructureCheck)
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.structureSet())), base);

    if (variant.isPropertyUnset()) {
        if (m_graph.watchConditions(variant.conditionSet()))
            return jsConstant(jsUndefined());
        return nullptr;
    }

    SpeculatedType loadPrediction;
    NodeType loadOp;
    if (variant.callLinkStatus() || variant.intrinsic() != NoIntrinsic) {
        loadPrediction = SpecCellOther;
        loadOp = GetGetterSetterByOffset;
    } else {
        loadPrediction = prediction;
        loadOp = GetByOffset;
    }
    
    Node* loadedValue;
    if (!variant.conditionSet().isEmpty())
        loadedValue = load(loadPrediction, variant.conditionSet(), loadOp);
    else {
        if (needStructureCheck && base->hasConstant()) {
            // We did emit a structure check. That means that we have an opportunity to do constant folding
            // here, since we didn't do it above.
            JSValue constant = m_graph.tryGetConstantProperty(
                base->asJSValue(), *m_graph.addStructureSet(variant.structureSet()), variant.offset());
            if (constant)
                return weakJSConstant(constant);
        }

        InferredType::Descriptor inferredType;
        if (needStructureCheck) {
            for (Structure* structure : variant.structureSet()) {
                InferredType::Descriptor thisType = m_graph.inferredTypeForProperty(structure, uid);
                inferredType.merge(thisType);
            }
        } else
            inferredType = InferredType::Top;
        
        loadedValue = handleGetByOffset(
            loadPrediction, base, identifierNumber, variant.offset(), inferredType, loadOp);
    }

    return loadedValue;
}

Node* ByteCodeParser::store(Node* base, unsigned identifier, const PutByIdVariant& variant, Node* value)
{
    RELEASE_ASSERT(variant.kind() == PutByIdVariant::Replace);

    checkPresenceLike(base, m_graph.identifiers()[identifier], variant.offset(), variant.structure());
    return handlePutByOffset(base, identifier, variant.offset(), variant.requiredType(), value);
}

void ByteCodeParser::handleGetById(
    VirtualRegister destination, SpeculatedType prediction, Node* base, unsigned identifierNumber,
    GetByIdStatus getByIdStatus, AccessType type, unsigned instructionSize)
{
    // Attempt to reduce the set of things in the GetByIdStatus.
    if (base->op() == NewObject) {
        bool ok = true;
        for (unsigned i = m_currentBlock->size(); i--;) {
            Node* node = m_currentBlock->at(i);
            if (node == base)
                break;
            if (writesOverlap(m_graph, node, JSCell_structureID)) {
                ok = false;
                break;
            }
        }
        if (ok)
            getByIdStatus.filter(base->structure().get());
    }
    
    NodeType getById;
    if (type == AccessType::Get)
        getById = getByIdStatus.makesCalls() ? GetByIdFlush : GetById;
    else if (type == AccessType::TryGet)
        getById = TryGetById;
    else
        getById = getByIdStatus.makesCalls() ? GetByIdDirectFlush : GetByIdDirect;

    if (getById != TryGetById && getByIdStatus.isModuleNamespace()) {
        if (handleModuleNamespaceLoad(destination, prediction, base, getByIdStatus)) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedGetById();
            return;
        }
    }

    // Special path for custom accessors since custom's offset does not have any meanings.
    // So, this is completely different from Simple one. But we have a chance to optimize it when we use DOMJIT.
    if (Options::useDOMJIT() && getByIdStatus.isCustom()) {
        ASSERT(getByIdStatus.numVariants() == 1);
        ASSERT(!getByIdStatus.makesCalls());
        GetByIdVariant variant = getByIdStatus[0];
        ASSERT(variant.domAttribute());
        if (handleDOMJITGetter(destination, variant, base, identifierNumber, prediction)) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedGetById();
            return;
        }
    }

    ASSERT(type == AccessType::Get || type == AccessType::GetDirect ||  !getByIdStatus.makesCalls());
    if (!getByIdStatus.isSimple() || !getByIdStatus.numVariants() || !Options::useAccessInlining()) {
        set(destination,
            addToGraph(getById, OpInfo(identifierNumber), OpInfo(prediction), base));
        return;
    }
    
    // FIXME: If we use the GetByIdStatus for anything then we should record it and insert a node
    // after everything else (like the GetByOffset or whatever) that will filter the recorded
    // GetByIdStatus. That means that the constant folder also needs to do the same!
    
    if (getByIdStatus.numVariants() > 1) {
        if (getByIdStatus.makesCalls() || !m_graph.m_plan.isFTL()
            || !Options::usePolymorphicAccessInlining()
            || getByIdStatus.numVariants() > Options::maxPolymorphicAccessInliningListSize()) {
            set(destination,
                addToGraph(getById, OpInfo(identifierNumber), OpInfo(prediction), base));
            return;
        }

        addToGraph(FilterGetByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByIdStatus(currentCodeOrigin(), getByIdStatus)), base);

        Vector<MultiGetByOffsetCase, 2> cases;
        
        // 1) Emit prototype structure checks for all chains. This could sort of maybe not be
        //    optimal, if there is some rarely executed case in the chain that requires a lot
        //    of checks and those checks are not watchpointable.
        for (const GetByIdVariant& variant : getByIdStatus.variants()) {
            if (variant.intrinsic() != NoIntrinsic) {
                set(destination,
                    addToGraph(getById, OpInfo(identifierNumber), OpInfo(prediction), base));
                return;
            }

            if (variant.conditionSet().isEmpty()) {
                cases.append(
                    MultiGetByOffsetCase(
                        *m_graph.addStructureSet(variant.structureSet()),
                        GetByOffsetMethod::load(variant.offset())));
                continue;
            }

            GetByOffsetMethod method = planLoad(variant.conditionSet());
            if (!method) {
                set(destination,
                    addToGraph(getById, OpInfo(identifierNumber), OpInfo(prediction), base));
                return;
            }
            
            cases.append(MultiGetByOffsetCase(*m_graph.addStructureSet(variant.structureSet()), method));
        }

        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedGetById();
    
        // 2) Emit a MultiGetByOffset
        MultiGetByOffsetData* data = m_graph.m_multiGetByOffsetData.add();
        data->cases = cases;
        data->identifierNumber = identifierNumber;
        set(destination,
            addToGraph(MultiGetByOffset, OpInfo(data), OpInfo(prediction), base));
        return;
    }

    addToGraph(FilterGetByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByIdStatus(currentCodeOrigin(), getByIdStatus)), base);

    ASSERT(getByIdStatus.numVariants() == 1);
    GetByIdVariant variant = getByIdStatus[0];
    
    Node* loadedValue = load(prediction, base, identifierNumber, variant);
    if (!loadedValue) {
        set(destination,
            addToGraph(getById, OpInfo(identifierNumber), OpInfo(prediction), base));
        return;
    }

    if (UNLIKELY(m_graph.compilation()))
        m_graph.compilation()->noticeInlinedGetById();

    ASSERT(type == AccessType::Get || type == AccessType::GetDirect || !variant.callLinkStatus());
    if (!variant.callLinkStatus() && variant.intrinsic() == NoIntrinsic) {
        set(destination, loadedValue);
        return;
    }
    
    Node* getter = addToGraph(GetGetter, loadedValue);

    if (handleIntrinsicGetter(destination, prediction, variant, base,
            [&] () {
                addToGraph(CheckCell, OpInfo(m_graph.freeze(variant.intrinsicFunction())), getter);
            })) {
        addToGraph(Phantom, base);
        return;
    }

    ASSERT(variant.intrinsic() == NoIntrinsic);

    // Make a call. We don't try to get fancy with using the smallest operand number because
    // the stack layout phase should compress the stack anyway.
    
    unsigned numberOfParameters = 0;
    numberOfParameters++; // The 'this' argument.
    numberOfParameters++; // True return PC.
    
    // Start with a register offset that corresponds to the last in-use register.
    int registerOffset = virtualRegisterForLocal(
        m_inlineStackTop->m_profiledBlock->numCalleeLocals() - 1).offset();
    registerOffset -= numberOfParameters;
    registerOffset -= CallFrame::headerSizeInRegisters;
    
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
    int nextRegister = registerOffset + CallFrame::headerSizeInRegisters;
    set(VirtualRegister(nextRegister++), base, ImmediateNakedSet);

    // We've set some locals, but they are not user-visible. It's still OK to exit from here.
    m_exitOK = true;
    addToGraph(ExitOK);
    
    handleCall(
        destination, Call, InlineCallFrame::GetterCall, instructionSize,
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
    const PutByIdStatus& putByIdStatus, bool isDirect, unsigned instructionSize)
{
    if (!putByIdStatus.isSimple() || !putByIdStatus.numVariants() || !Options::useAccessInlining()) {
        if (!putByIdStatus.isSet())
            addToGraph(ForceOSRExit);
        emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
        return;
    }
    
    if (putByIdStatus.numVariants() > 1) {
        if (!m_graph.m_plan.isFTL() || putByIdStatus.makesCalls()
            || !Options::usePolymorphicAccessInlining()
            || putByIdStatus.numVariants() > Options::maxPolymorphicAccessInliningListSize()) {
            emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
            return;
        }
        
        if (!isDirect) {
            for (unsigned variantIndex = putByIdStatus.numVariants(); variantIndex--;) {
                if (putByIdStatus[variantIndex].kind() != PutByIdVariant::Transition)
                    continue;
                if (!check(putByIdStatus[variantIndex].conditionSet())) {
                    emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
                    return;
                }
            }
        }
        
        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedPutById();

        addToGraph(FilterPutByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByIdStatus(currentCodeOrigin(), putByIdStatus)), base);

        for (const PutByIdVariant& variant : putByIdStatus.variants()) {
            m_graph.registerInferredType(variant.requiredType());
            for (Structure* structure : variant.oldStructure())
                m_graph.registerStructure(structure);
            if (variant.kind() == PutByIdVariant::Transition)
                m_graph.registerStructure(variant.newStructure());
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
        addToGraph(FilterPutByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByIdStatus(currentCodeOrigin(), putByIdStatus)), base);

        store(base, identifierNumber, variant, value);
        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedPutById();
        return;
    }
    
    case PutByIdVariant::Transition: {
        addToGraph(FilterPutByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByIdStatus(currentCodeOrigin(), putByIdStatus)), base);

        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.oldStructure())), base);
        if (!check(variant.conditionSet())) {
            emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
            return;
        }

        ASSERT(variant.oldStructureForTransition()->transitionWatchpointSetHasBeenInvalidated());
    
        Node* propertyStorage;
        Transition* transition = m_graph.m_transitions.add(
            m_graph.registerStructure(variant.oldStructureForTransition()), m_graph.registerStructure(variant.newStructure()));

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
        data->inferredType = variant.requiredType();
        m_graph.registerInferredType(data->inferredType);
        
        // NOTE: We could GC at this point because someone could insert an operation that GCs.
        // That's fine because:
        // - Things already in the structure will get scanned because we haven't messed with
        //   the object yet.
        // - The value we are fixing to put is going to be kept live by OSR exit handling. So
        //   if the GC does a conservative scan here it will see the new value.
        
        addToGraph(
            PutByOffset,
            OpInfo(data),
            propertyStorage,
            base,
            value);
        
        if (variant.reallocatesStorage())
            addToGraph(NukeStructureAndSetButterfly, base, propertyStorage);

        // FIXME: PutStructure goes last until we fix either
        // https://bugs.webkit.org/show_bug.cgi?id=142921 or
        // https://bugs.webkit.org/show_bug.cgi?id=142924.
        addToGraph(PutStructure, OpInfo(transition), base);

        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedPutById();
        return;
    }
        
    case PutByIdVariant::Setter: {
        addToGraph(FilterPutByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByIdStatus(currentCodeOrigin(), putByIdStatus)), base);

        Node* loadedValue = load(SpecCellOther, base, identifierNumber, variant);
        if (!loadedValue) {
            emitPutById(base, identifierNumber, value, putByIdStatus, isDirect);
            return;
        }
        
        Node* setter = addToGraph(GetSetter, loadedValue);
        
        // Make a call. We don't try to get fancy with using the smallest operand number because
        // the stack layout phase should compress the stack anyway.
    
        unsigned numberOfParameters = 0;
        numberOfParameters++; // The 'this' argument.
        numberOfParameters++; // The new value.
        numberOfParameters++; // True return PC.
    
        // Start with a register offset that corresponds to the last in-use register.
        int registerOffset = virtualRegisterForLocal(
            m_inlineStackTop->m_profiledBlock->numCalleeLocals() - 1).offset();
        registerOffset -= numberOfParameters;
        registerOffset -= CallFrame::headerSizeInRegisters;
    
        // Get the alignment right.
        registerOffset = -WTF::roundUpToMultipleOf(
            stackAlignmentRegisters(),
            -registerOffset);
    
        ensureLocals(
            m_inlineStackTop->remapOperand(
                VirtualRegister(registerOffset)).toLocal());
    
        int nextRegister = registerOffset + CallFrame::headerSizeInRegisters;
        set(VirtualRegister(nextRegister++), base, ImmediateNakedSet);
        set(VirtualRegister(nextRegister++), value, ImmediateNakedSet);

        // We've set some locals, but they are not user-visible. It's still OK to exit from here.
        m_exitOK = true;
        addToGraph(ExitOK);
    
        handleCall(
            VirtualRegister(), Call, InlineCallFrame::SetterCall,
            instructionSize, setter, numberOfParameters - 1, registerOffset,
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
    m_constants.shrink(0);
}

template<typename Op>
void ByteCodeParser::parseGetById(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    SpeculatedType prediction = getPrediction();
    
    Node* base = get(bytecode.base);
    unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];
    
    UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
    GetByIdStatus getByIdStatus = GetByIdStatus::computeFor(
        m_inlineStackTop->m_profiledBlock,
        m_inlineStackTop->m_baselineMap, m_icContextStack,
        currentCodeOrigin(), uid);

    AccessType type = AccessType::Get;
    unsigned opcodeLength = currentInstruction->size();
    if (Op::opcodeID == op_try_get_by_id)
        type = AccessType::TryGet;
    else if (Op::opcodeID == op_get_by_id_direct)
        type = AccessType::GetDirect;

    handleGetById(
        bytecode.dst, prediction, base, identifierNumber, getByIdStatus, type, opcodeLength);

}

static uint64_t makeDynamicVarOpInfo(unsigned identifierNumber, unsigned getPutInfo)
{
    static_assert(sizeof(identifierNumber) == 4,
        "We cannot fit identifierNumber into the high bits of m_opInfo");
    return static_cast<uint64_t>(identifierNumber) | (static_cast<uint64_t>(getPutInfo) << 32);
}

// The idiom:
//     if (true) { ...; goto label; } else label: continue
// Allows using NEXT_OPCODE as a statement, even in unbraced if+else, while containing a `continue`.
// The more common idiom:
//     do { ...; } while (false)
// Doesn't allow using `continue`.
#define NEXT_OPCODE(name) \
    if (true) { \
        m_currentIndex += currentInstruction->size(); \
        goto WTF_CONCAT(NEXT_OPCODE_, __LINE__); /* Need a unique label: usable more than once per function. */ \
    } else \
        WTF_CONCAT(NEXT_OPCODE_, __LINE__): \
    continue

#define LAST_OPCODE_LINKED(name) do { \
        m_currentIndex += currentInstruction->size(); \
        m_exitOK = false; \
        return; \
    } while (false)

#define LAST_OPCODE(name) \
    do { \
        if (m_currentBlock->terminal()) { \
            switch (m_currentBlock->terminal()->op()) { \
            case Jump: \
            case Branch: \
            case Switch: \
                ASSERT(!m_currentBlock->isLinked); \
                m_inlineStackTop->m_unlinkedBlocks.append(m_currentBlock); \
                break;\
            default: break; \
            } \
        } \
        LAST_OPCODE_LINKED(name); \
    } while (false)

void ByteCodeParser::parseBlock(unsigned limit)
{
    auto& instructions = m_inlineStackTop->m_codeBlock->instructions();
    unsigned blockBegin = m_currentIndex;

    // If we are the first basic block, introduce markers for arguments. This allows
    // us to track if a use of an argument may use the actual argument passed, as
    // opposed to using a value we set explicitly.
    if (m_currentBlock == m_graph.block(0) && !inlineCallFrame()) {
        auto addResult = m_graph.m_rootToArguments.add(m_currentBlock, ArgumentsVector());
        RELEASE_ASSERT(addResult.isNewEntry);
        ArgumentsVector& entrypointArguments = addResult.iterator->value;
        entrypointArguments.resize(m_numArguments);

        // We will emit SetArgument nodes. They don't exit, but we're at the top of an op_enter so
        // exitOK = true.
        m_exitOK = true;
        for (unsigned argument = 0; argument < m_numArguments; ++argument) {
            VariableAccessData* variable = newVariableAccessData(
                virtualRegisterForArgument(argument));
            variable->mergeStructureCheckHoistingFailed(
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache));
            variable->mergeCheckArrayHoistingFailed(
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType));
            
            Node* setArgument = addToGraph(SetArgument, OpInfo(variable));
            entrypointArguments[argument] = setArgument;
            m_currentBlock->variablesAtTail.setArgumentFirstTime(argument, setArgument);
        }
    }

    CodeBlock* codeBlock = m_inlineStackTop->m_codeBlock;

    auto jumpTarget = [&](int target) {
        if (target)
            return target;
        return codeBlock->outOfLineJumpOffset(m_currentInstruction);
    };

    while (true) {
        // We're staring at a new bytecode instruction. So we once again have a place that we can exit
        // to.
        m_exitOK = true;
        
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
                addJumpTo(m_currentIndex);
            return;
        }
        
        // Switch on the current bytecode opcode.
        const Instruction* currentInstruction = instructions.at(m_currentIndex).ptr();
        m_currentInstruction = currentInstruction; // Some methods want to use this, and we'd rather not thread it through calls.
        OpcodeID opcodeID = currentInstruction->opcodeID();
        
        VERBOSE_LOG("    parsing ", currentCodeOrigin(), ": ", opcodeID, "\n");
        
        if (UNLIKELY(m_graph.compilation())) {
            addToGraph(CountExecution, OpInfo(m_graph.compilation()->executionCounterFor(
                Profiler::OriginStack(*m_vm->m_perBytecodeProfiler, m_codeBlock, currentCodeOrigin()))));
        }
        
        switch (opcodeID) {

        // === Function entry opcodes ===

        case op_enter: {
            Node* undefined = addToGraph(JSConstant, OpInfo(m_constantUndefined));
            // Initialize all locals to undefined.
            for (int i = 0; i < m_inlineStackTop->m_codeBlock->numVars(); ++i)
                set(virtualRegisterForLocal(i), undefined, ImmediateNakedSet);

            NEXT_OPCODE(op_enter);
        }
            
        case op_to_this: {
            Node* op1 = getThis();
            if (op1->op() != ToThis) {
                auto& metadata = currentInstruction->as<OpToThis>().metadata(codeBlock);
                Structure* cachedStructure = metadata.cachedStructure.get();
                if (metadata.toThisStatus != ToThisOK
                    || !cachedStructure
                    || cachedStructure->classInfo()->methodTable.toThis != JSObject::info()->methodTable.toThis
                    || m_inlineStackTop->m_profiledBlock->couldTakeSlowCase(m_currentIndex)
                    || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
                    || (op1->op() == GetLocal && op1->variableAccessData()->structureCheckHoistingFailed())) {
                    setThis(addToGraph(ToThis, OpInfo(), OpInfo(getPrediction()), op1));
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
            auto bytecode = currentInstruction->as<OpCreateThis>();
            Node* callee = get(VirtualRegister(bytecode.callee));

            JSFunction* function = callee->dynamicCastConstant<JSFunction*>(*m_vm);
            if (!function) {
                JSCell* cachedFunction = bytecode.metadata(codeBlock).cachedCallee.unvalidatedGet();
                if (cachedFunction
                    && cachedFunction != JSCell::seenMultipleCalleeObjects()
                    && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell)) {
                    ASSERT(cachedFunction->inherits<JSFunction>(*m_vm));

                    FrozenValue* frozen = m_graph.freeze(cachedFunction);
                    addToGraph(CheckCell, OpInfo(frozen), callee);

                    function = static_cast<JSFunction*>(cachedFunction);
                }
            }

            bool alreadyEmitted = false;
            if (function) {
                if (FunctionRareData* rareData = function->rareData()) {
                    if (rareData->allocationProfileWatchpointSet().isStillValid()) {
                        Structure* structure = rareData->objectAllocationStructure();
                        JSObject* prototype = rareData->objectAllocationPrototype();
                        if (structure
                            && (structure->hasMonoProto() || prototype)
                            && rareData->allocationProfileWatchpointSet().isStillValid()) {

                            m_graph.freeze(rareData);
                            m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                            
                            // The callee is still live up to this point.
                            addToGraph(Phantom, callee);
                            Node* object = addToGraph(NewObject, OpInfo(m_graph.registerStructure(structure)));
                            if (structure->hasPolyProto()) {
                                StorageAccessData* data = m_graph.m_storageAccessData.add();
                                data->offset = knownPolyProtoOffset;
                                data->identifierNumber = m_graph.identifiers().ensure(m_graph.m_vm.propertyNames->builtinNames().polyProtoName().impl());
                                InferredType::Descriptor inferredType = InferredType::Top;
                                data->inferredType = inferredType;
                                m_graph.registerInferredType(inferredType);
                                ASSERT(isInlineOffset(knownPolyProtoOffset));
                                addToGraph(PutByOffset, OpInfo(data), object, object, weakJSConstant(prototype));
                            }
                            set(VirtualRegister(bytecode.dst), object);
                            alreadyEmitted = true;
                        }
                    }
                }
            }
            if (!alreadyEmitted) {
                set(VirtualRegister(bytecode.dst),
                    addToGraph(CreateThis, OpInfo(bytecode.inlineCapacity), callee));
            }
            NEXT_OPCODE(op_create_this);
        }

        case op_new_object: {
            auto bytecode = currentInstruction->as<OpNewObject>();
            set(bytecode.dst,
                addToGraph(NewObject,
                    OpInfo(m_graph.registerStructure(bytecode.metadata(codeBlock).objectAllocationProfile.structure()))));
            NEXT_OPCODE(op_new_object);
        }
            
        case op_new_array: {
            auto bytecode = currentInstruction->as<OpNewArray>();
            int startOperand = bytecode.argv.offset();
            int numOperands = bytecode.argc;
            ArrayAllocationProfile& profile = bytecode.metadata(codeBlock).arrayAllocationProfile;
            for (int operandIdx = startOperand; operandIdx > startOperand - numOperands; --operandIdx)
                addVarArgChild(get(VirtualRegister(operandIdx)));
            unsigned vectorLengthHint = std::max<unsigned>(profile.vectorLengthHint(), numOperands);
            set(bytecode.dst, addToGraph(Node::VarArg, NewArray, OpInfo(profile.selectIndexingType()), OpInfo(vectorLengthHint)));
            NEXT_OPCODE(op_new_array);
        }

        case op_new_array_with_spread: {
            auto bytecode = currentInstruction->as<OpNewArrayWithSpread>();
            int startOperand = bytecode.argv.offset();
            int numOperands = bytecode.argc;
            const BitVector& bitVector = m_inlineStackTop->m_profiledBlock->unlinkedCodeBlock()->bitVector(bytecode.bitVector);
            for (int operandIdx = startOperand; operandIdx > startOperand - numOperands; --operandIdx)
                addVarArgChild(get(VirtualRegister(operandIdx)));

            BitVector* copy = m_graph.m_bitVectors.add(bitVector);
            ASSERT(*copy == bitVector);

            set(bytecode.dst,
                addToGraph(Node::VarArg, NewArrayWithSpread, OpInfo(copy)));
            NEXT_OPCODE(op_new_array_with_spread);
        }

        case op_spread: {
            auto bytecode = currentInstruction->as<OpSpread>();
            set(bytecode.dst,
                addToGraph(Spread, get(bytecode.argument)));
            NEXT_OPCODE(op_spread);
        }
            
        case op_new_array_with_size: {
            auto bytecode = currentInstruction->as<OpNewArrayWithSize>();
            ArrayAllocationProfile& profile = bytecode.metadata(codeBlock).arrayAllocationProfile;
            set(bytecode.dst, addToGraph(NewArrayWithSize, OpInfo(profile.selectIndexingType()), get(bytecode.length)));
            NEXT_OPCODE(op_new_array_with_size);
        }
            
        case op_new_array_buffer: {
            auto bytecode = currentInstruction->as<OpNewArrayBuffer>();
            // Unfortunately, we can't allocate a new JSImmutableButterfly if the profile tells us new information because we
            // cannot allocate from compilation threads.
            WTF::loadLoadFence();
            FrozenValue* frozen = get(VirtualRegister(bytecode.immutableButterfly))->constant();
            WTF::loadLoadFence();
            JSImmutableButterfly* immutableButterfly = frozen->cast<JSImmutableButterfly*>();
            NewArrayBufferData data { };
            data.indexingMode = immutableButterfly->indexingMode();
            data.vectorLengthHint = immutableButterfly->toButterfly()->vectorLength();

            set(VirtualRegister(bytecode.dst), addToGraph(NewArrayBuffer, OpInfo(frozen), OpInfo(data.asQuadWord)));
            NEXT_OPCODE(op_new_array_buffer);
        }
            
        case op_new_regexp: {
            auto bytecode = currentInstruction->as<OpNewRegexp>();
            ASSERT(bytecode.regexp.isConstant());
            FrozenValue* frozenRegExp = m_graph.freezeStrong(m_inlineStackTop->m_codeBlock->getConstant(bytecode.regexp.offset()));
            set(bytecode.dst, addToGraph(NewRegexp, OpInfo(frozenRegExp), jsConstant(jsNumber(0))));
            NEXT_OPCODE(op_new_regexp);
        }

        case op_get_rest_length: {
            auto bytecode = currentInstruction->as<OpGetRestLength>();
            InlineCallFrame* inlineCallFrame = this->inlineCallFrame();
            Node* length;
            if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
                unsigned argumentsLength = inlineCallFrame->argumentCountIncludingThis - 1;
                JSValue restLength;
                if (argumentsLength <= bytecode.numParametersToSkip)
                    restLength = jsNumber(0);
                else
                    restLength = jsNumber(argumentsLength - bytecode.numParametersToSkip);

                length = jsConstant(restLength);
            } else
                length = addToGraph(GetRestLength, OpInfo(bytecode.numParametersToSkip));
            set(bytecode.dst, length);
            NEXT_OPCODE(op_get_rest_length);
        }

        case op_create_rest: {
            auto bytecode = currentInstruction->as<OpCreateRest>();
            noticeArgumentsUse();
            Node* arrayLength = get(bytecode.arraySize);
            set(bytecode.dst,
                addToGraph(CreateRest, OpInfo(bytecode.numParametersToSkip), arrayLength));
            NEXT_OPCODE(op_create_rest);
        }
            
        // === Bitwise operations ===

        case op_bitnot: {
            auto bytecode = currentInstruction->as<OpBitnot>();
            Node* op1 = get(bytecode.operand);
            set(bytecode.dst, addToGraph(ArithBitNot, op1));
            NEXT_OPCODE(op_bitnot);
        }

        case op_bitand: {
            auto bytecode = currentInstruction->as<OpBitand>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            if (isInt32Speculation(getPrediction()))
                set(bytecode.dst, addToGraph(ArithBitAnd, op1, op2));
            else
                set(bytecode.dst, addToGraph(ValueBitAnd, op1, op2));
            NEXT_OPCODE(op_bitand);
        }

        case op_bitor: {
            auto bytecode = currentInstruction->as<OpBitor>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            if (isInt32Speculation(getPrediction()))
                set(bytecode.dst, addToGraph(ArithBitOr, op1, op2));
            else
                set(bytecode.dst, addToGraph(ValueBitOr, op1, op2));
            NEXT_OPCODE(op_bitor);
        }

        case op_bitxor: {
            auto bytecode = currentInstruction->as<OpBitxor>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            if (isInt32Speculation(getPrediction()))
                set(bytecode.dst, addToGraph(ArithBitXor, op1, op2));
            else
                set(bytecode.dst, addToGraph(ValueBitXor, op1, op2));
            NEXT_OPCODE(op_bitor);
        }

        case op_rshift: {
            auto bytecode = currentInstruction->as<OpRshift>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(BitRShift, op1, op2));
            NEXT_OPCODE(op_rshift);
        }

        case op_lshift: {
            auto bytecode = currentInstruction->as<OpLshift>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(BitLShift, op1, op2));
            NEXT_OPCODE(op_lshift);
        }

        case op_urshift: {
            auto bytecode = currentInstruction->as<OpUrshift>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(BitURShift, op1, op2));
            NEXT_OPCODE(op_urshift);
        }
            
        case op_unsigned: {
            auto bytecode = currentInstruction->as<OpUnsigned>();
            set(bytecode.dst, makeSafe(addToGraph(UInt32ToNumber, get(bytecode.operand))));
            NEXT_OPCODE(op_unsigned);
        }

        // === Increment/Decrement opcodes ===

        case op_inc: {
            auto bytecode = currentInstruction->as<OpInc>();
            Node* op = get(bytecode.srcDst);
            set(bytecode.srcDst, makeSafe(addToGraph(ArithAdd, op, addToGraph(JSConstant, OpInfo(m_constantOne)))));
            NEXT_OPCODE(op_inc);
        }

        case op_dec: {
            auto bytecode = currentInstruction->as<OpDec>();
            Node* op = get(bytecode.srcDst);
            set(bytecode.srcDst, makeSafe(addToGraph(ArithSub, op, addToGraph(JSConstant, OpInfo(m_constantOne)))));
            NEXT_OPCODE(op_dec);
        }

        // === Arithmetic operations ===

        case op_add: {
            auto bytecode = currentInstruction->as<OpAdd>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(bytecode.dst, makeSafe(addToGraph(ArithAdd, op1, op2)));
            else
                set(bytecode.dst, makeSafe(addToGraph(ValueAdd, op1, op2)));
            NEXT_OPCODE(op_add);
        }

        case op_sub: {
            auto bytecode = currentInstruction->as<OpSub>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(bytecode.dst, makeSafe(addToGraph(ArithSub, op1, op2)));
            else
                set(bytecode.dst, makeSafe(addToGraph(ValueSub, op1, op2)));
            NEXT_OPCODE(op_sub);
        }

        case op_negate: {
            auto bytecode = currentInstruction->as<OpNegate>();
            Node* op1 = get(bytecode.operand);
            if (op1->hasNumberResult())
                set(bytecode.dst, makeSafe(addToGraph(ArithNegate, op1)));
            else
                set(bytecode.dst, makeSafe(addToGraph(ValueNegate, op1)));
            NEXT_OPCODE(op_negate);
        }

        case op_mul: {
            // Multiply requires that the inputs are not truncated, unfortunately.
            auto bytecode = currentInstruction->as<OpMul>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, makeSafe(addToGraph(ArithMul, op1, op2)));
            NEXT_OPCODE(op_mul);
        }

        case op_mod: {
            auto bytecode = currentInstruction->as<OpMod>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, makeSafe(addToGraph(ArithMod, op1, op2)));
            NEXT_OPCODE(op_mod);
        }

        case op_pow: {
            // FIXME: ArithPow(Untyped, Untyped) should be supported as the same to ArithMul, ArithSub etc.
            // https://bugs.webkit.org/show_bug.cgi?id=160012
            auto bytecode = currentInstruction->as<OpPow>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(ArithPow, op1, op2));
            NEXT_OPCODE(op_pow);
        }

        case op_div: {
            auto bytecode = currentInstruction->as<OpDiv>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, makeDivSafe(addToGraph(ArithDiv, op1, op2)));
            NEXT_OPCODE(op_div);
        }

        // === Misc operations ===

        case op_debug: {
            // This is a nop in the DFG/FTL because when we set a breakpoint in the debugger,
            // we will jettison all optimized CodeBlocks that contains the breakpoint.
            addToGraph(Check); // We add a nop here so that basic block linking doesn't break.
            NEXT_OPCODE(op_debug);
        }

        case op_mov: {
            auto bytecode = currentInstruction->as<OpMov>();
            Node* op = get(bytecode.src);
            set(bytecode.dst, op);
            NEXT_OPCODE(op_mov);
        }

        case op_check_tdz: {
            auto bytecode = currentInstruction->as<OpCheckTdz>();
            addToGraph(CheckNotEmpty, get(bytecode.target));
            NEXT_OPCODE(op_check_tdz);
        }

        case op_overrides_has_instance: {
            auto bytecode = currentInstruction->as<OpOverridesHasInstance>();
            JSFunction* defaultHasInstanceSymbolFunction = m_inlineStackTop->m_codeBlock->globalObjectFor(currentCodeOrigin())->functionProtoHasInstanceSymbolFunction();

            Node* constructor = get(VirtualRegister(bytecode.constructor));
            Node* hasInstanceValue = get(VirtualRegister(bytecode.hasInstanceValue));

            set(VirtualRegister(bytecode.dst), addToGraph(OverridesHasInstance, OpInfo(m_graph.freeze(defaultHasInstanceSymbolFunction)), constructor, hasInstanceValue));
            NEXT_OPCODE(op_overrides_has_instance);
        }

        case op_identity_with_profile: {
            auto bytecode = currentInstruction->as<OpIdentityWithProfile>();
            Node* srcDst = get(bytecode.srcDst);
            SpeculatedType speculation = static_cast<SpeculatedType>(bytecode.topProfile) << 32 | static_cast<SpeculatedType>(bytecode.bottomProfile);
            set(bytecode.srcDst, addToGraph(IdentityWithProfile, OpInfo(speculation), srcDst));
            NEXT_OPCODE(op_identity_with_profile);
        }

        case op_instanceof: {
            auto bytecode = currentInstruction->as<OpInstanceof>();
            
            InstanceOfStatus status = InstanceOfStatus::computeFor(
                m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap,
                m_currentIndex);
            
            Node* value = get(bytecode.value);
            Node* prototype = get(bytecode.prototype);

            // Only inline it if it's Simple with a commonPrototype; bottom/top or variable
            // prototypes both get handled by the IC. This makes sense for bottom (unprofiled)
            // instanceof ICs because the profit of this optimization is fairly low. So, in the
            // absence of any information, it's better to avoid making this be the cause of a
            // recompilation.
            if (JSObject* commonPrototype = status.commonPrototype()) {
                addToGraph(CheckCell, OpInfo(m_graph.freeze(commonPrototype)), prototype);
                
                bool allOK = true;
                MatchStructureData* data = m_graph.m_matchStructureData.add();
                for (const InstanceOfVariant& variant : status.variants()) {
                    if (!check(variant.conditionSet())) {
                        allOK = false;
                        break;
                    }
                    for (Structure* structure : variant.structureSet()) {
                        MatchStructureVariant matchVariant;
                        matchVariant.structure = m_graph.registerStructure(structure);
                        matchVariant.result = variant.isHit();
                        
                        data->variants.append(WTFMove(matchVariant));
                    }
                }
                
                if (allOK) {
                    Node* match = addToGraph(MatchStructure, OpInfo(data), value);
                    set(bytecode.dst, match);
                    NEXT_OPCODE(op_instanceof);
                }
            }
            
            set(bytecode.dst, addToGraph(InstanceOf, value, prototype));
            NEXT_OPCODE(op_instanceof);
        }

        case op_instanceof_custom: {
            auto bytecode = currentInstruction->as<OpInstanceofCustom>();
            Node* value = get(bytecode.value);
            Node* constructor = get(bytecode.constructor);
            Node* hasInstanceValue = get(bytecode.hasInstanceValue);
            set(bytecode.dst, addToGraph(InstanceOfCustom, value, constructor, hasInstanceValue));
            NEXT_OPCODE(op_instanceof_custom);
        }
        case op_is_empty: {
            auto bytecode = currentInstruction->as<OpIsEmpty>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsEmpty, value));
            NEXT_OPCODE(op_is_empty);
        }
        case op_is_undefined: {
            auto bytecode = currentInstruction->as<OpIsUndefined>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsUndefined, value));
            NEXT_OPCODE(op_is_undefined);
        }

        case op_is_boolean: {
            auto bytecode = currentInstruction->as<OpIsBoolean>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsBoolean, value));
            NEXT_OPCODE(op_is_boolean);
        }

        case op_is_number: {
            auto bytecode = currentInstruction->as<OpIsNumber>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsNumber, value));
            NEXT_OPCODE(op_is_number);
        }

        case op_is_cell_with_type: {
            auto bytecode = currentInstruction->as<OpIsCellWithType>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsCellWithType, OpInfo(bytecode.type), value));
            NEXT_OPCODE(op_is_cell_with_type);
        }

        case op_is_object: {
            auto bytecode = currentInstruction->as<OpIsObject>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsObject, value));
            NEXT_OPCODE(op_is_object);
        }

        case op_is_object_or_null: {
            auto bytecode = currentInstruction->as<OpIsObjectOrNull>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsObjectOrNull, value));
            NEXT_OPCODE(op_is_object_or_null);
        }

        case op_is_function: {
            auto bytecode = currentInstruction->as<OpIsFunction>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(IsFunction, value));
            NEXT_OPCODE(op_is_function);
        }

        case op_not: {
            auto bytecode = currentInstruction->as<OpNot>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(LogicalNot, value));
            NEXT_OPCODE(op_not);
        }
            
        case op_to_primitive: {
            auto bytecode = currentInstruction->as<OpToPrimitive>();
            Node* value = get(bytecode.src);
            set(bytecode.dst, addToGraph(ToPrimitive, value));
            NEXT_OPCODE(op_to_primitive);
        }
            
        case op_strcat: {
            auto bytecode = currentInstruction->as<OpStrcat>();
            int startOperand = bytecode.src.offset();
            int numOperands = bytecode.count;
#if CPU(X86)
            // X86 doesn't have enough registers to compile MakeRope with three arguments. The
            // StrCat we emit here may be turned into a MakeRope. Rather than try to be clever,
            // we just make StrCat dumber on this processor.
            const unsigned maxArguments = 2;
#else
            const unsigned maxArguments = 3;
#endif
            Node* operands[AdjacencyList::Size];
            unsigned indexInOperands = 0;
            for (unsigned i = 0; i < AdjacencyList::Size; ++i)
                operands[i] = 0;
            for (int operandIdx = 0; operandIdx < numOperands; ++operandIdx) {
                if (indexInOperands == maxArguments) {
                    operands[0] = addToGraph(StrCat, operands[0], operands[1], operands[2]);
                    for (unsigned i = 1; i < AdjacencyList::Size; ++i)
                        operands[i] = 0;
                    indexInOperands = 1;
                }
                
                ASSERT(indexInOperands < AdjacencyList::Size);
                ASSERT(indexInOperands < maxArguments);
                operands[indexInOperands++] = get(VirtualRegister(startOperand - operandIdx));
            }
            set(bytecode.dst, addToGraph(StrCat, operands[0], operands[1], operands[2]));
            NEXT_OPCODE(op_strcat);
        }

        case op_less: {
            auto bytecode = currentInstruction->as<OpLess>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareLess, op1, op2));
            NEXT_OPCODE(op_less);
        }

        case op_lesseq: {
            auto bytecode = currentInstruction->as<OpLesseq>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareLessEq, op1, op2));
            NEXT_OPCODE(op_lesseq);
        }

        case op_greater: {
            auto bytecode = currentInstruction->as<OpGreater>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareGreater, op1, op2));
            NEXT_OPCODE(op_greater);
        }

        case op_greatereq: {
            auto bytecode = currentInstruction->as<OpGreatereq>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareGreaterEq, op1, op2));
            NEXT_OPCODE(op_greatereq);
        }

        case op_below: {
            auto bytecode = currentInstruction->as<OpBelow>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareBelow, op1, op2));
            NEXT_OPCODE(op_below);
        }

        case op_beloweq: {
            auto bytecode = currentInstruction->as<OpBeloweq>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareBelowEq, op1, op2));
            NEXT_OPCODE(op_beloweq);
        }

        case op_eq: {
            auto bytecode = currentInstruction->as<OpEq>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareEq, op1, op2));
            NEXT_OPCODE(op_eq);
        }

        case op_eq_null: {
            auto bytecode = currentInstruction->as<OpEqNull>();
            Node* value = get(bytecode.operand);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            set(bytecode.dst, addToGraph(CompareEq, value, nullConstant));
            NEXT_OPCODE(op_eq_null);
        }

        case op_stricteq: {
            auto bytecode = currentInstruction->as<OpStricteq>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(CompareStrictEq, op1, op2));
            NEXT_OPCODE(op_stricteq);
        }

        case op_neq: {
            auto bytecode = currentInstruction->as<OpNeq>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            set(bytecode.dst, addToGraph(LogicalNot, addToGraph(CompareEq, op1, op2)));
            NEXT_OPCODE(op_neq);
        }

        case op_neq_null: {
            auto bytecode = currentInstruction->as<OpNeqNull>();
            Node* value = get(bytecode.operand);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            set(bytecode.dst, addToGraph(LogicalNot, addToGraph(CompareEq, value, nullConstant)));
            NEXT_OPCODE(op_neq_null);
        }

        case op_nstricteq: {
            auto bytecode = currentInstruction->as<OpNstricteq>();
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* invertedResult;
            invertedResult = addToGraph(CompareStrictEq, op1, op2);
            set(bytecode.dst, addToGraph(LogicalNot, invertedResult));
            NEXT_OPCODE(op_nstricteq);
        }

        // === Property access operations ===

        case op_get_by_val: {
            auto bytecode = currentInstruction->as<OpGetByVal>();
            SpeculatedType prediction = getPredictionWithoutOSRExit();

            Node* base = get(bytecode.base);
            Node* property = get(bytecode.property);
            bool compiledAsGetById = false;
            GetByIdStatus getByIdStatus;
            unsigned identifierNumber = 0;
            {
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                ByValInfo* byValInfo = m_inlineStackTop->m_baselineMap.get(CodeOrigin(currentCodeOrigin().bytecodeIndex)).byValInfo;
                // FIXME: When the bytecode is not compiled in the baseline JIT, byValInfo becomes null.
                // At that time, there is no information.
                if (byValInfo
                    && byValInfo->stubInfo
                    && !byValInfo->tookSlowPath
                    && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                    && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                    && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell)) {
                    compiledAsGetById = true;
                    identifierNumber = m_graph.identifiers().ensure(byValInfo->cachedId.impl());
                    UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];

                    if (Symbol* symbol = byValInfo->cachedSymbol.get()) {
                        FrozenValue* frozen = m_graph.freezeStrong(symbol);
                        addToGraph(CheckCell, OpInfo(frozen), property);
                    } else {
                        ASSERT(!uid->isSymbol());
                        addToGraph(CheckStringIdent, OpInfo(uid), property);
                    }

                    getByIdStatus = GetByIdStatus::computeForStubInfo(
                        locker, m_inlineStackTop->m_profiledBlock,
                        byValInfo->stubInfo, currentCodeOrigin(), uid);
                }
            }

            if (compiledAsGetById)
                handleGetById(bytecode.dst, prediction, base, identifierNumber, getByIdStatus, AccessType::Get, currentInstruction->size());
            else {
                ArrayMode arrayMode = getArrayMode(bytecode.metadata(codeBlock).arrayProfile, Array::Read);
                // FIXME: We could consider making this not vararg, since it only uses three child
                // slots.
                // https://bugs.webkit.org/show_bug.cgi?id=184192
                addVarArgChild(base);
                addVarArgChild(property);
                addVarArgChild(0); // Leave room for property storage.
                Node* getByVal = addToGraph(Node::VarArg, GetByVal, OpInfo(arrayMode.asWord()), OpInfo(prediction));
                m_exitOK = false; // GetByVal must be treated as if it clobbers exit state, since FixupPhase may make it generic.
                set(bytecode.dst, getByVal);
            }

            NEXT_OPCODE(op_get_by_val);
        }

        case op_get_by_val_with_this: {
            auto bytecode = currentInstruction->as<OpGetByValWithThis>();
            SpeculatedType prediction = getPrediction();

            Node* base = get(bytecode.base);
            Node* thisValue = get(bytecode.thisValue);
            Node* property = get(bytecode.property);
            Node* getByValWithThis = addToGraph(GetByValWithThis, OpInfo(), OpInfo(prediction), base, thisValue, property);
            set(bytecode.dst, getByValWithThis);

            NEXT_OPCODE(op_get_by_val_with_this);
        }

        case op_put_by_val_direct:
            handlePutByVal(currentInstruction->as<OpPutByValDirect>(), currentInstruction->size());
            NEXT_OPCODE(op_put_by_val_direct);

        case op_put_by_val: {
            handlePutByVal(currentInstruction->as<OpPutByVal>(), currentInstruction->size());
            NEXT_OPCODE(op_put_by_val);
        }

        case op_put_by_val_with_this: {
            auto bytecode = currentInstruction->as<OpPutByValWithThis>();
            Node* base = get(bytecode.base);
            Node* thisValue = get(bytecode.thisValue);
            Node* property = get(bytecode.property);
            Node* value = get(bytecode.value);

            addVarArgChild(base);
            addVarArgChild(thisValue);
            addVarArgChild(property);
            addVarArgChild(value);
            addToGraph(Node::VarArg, PutByValWithThis, OpInfo(0), OpInfo(0));

            NEXT_OPCODE(op_put_by_val_with_this);
        }

        case op_define_data_property: {
            auto bytecode = currentInstruction->as<OpDefineDataProperty>();
            Node* base = get(bytecode.base);
            Node* property = get(bytecode.property);
            Node* value = get(bytecode.value);
            Node* attributes = get(bytecode.attributes);

            addVarArgChild(base);
            addVarArgChild(property);
            addVarArgChild(value);
            addVarArgChild(attributes);
            addToGraph(Node::VarArg, DefineDataProperty, OpInfo(0), OpInfo(0));

            NEXT_OPCODE(op_define_data_property);
        }

        case op_define_accessor_property: {
            auto bytecode = currentInstruction->as<OpDefineAccessorProperty>();
            Node* base = get(bytecode.base);
            Node* property = get(bytecode.property);
            Node* getter = get(bytecode.getter);
            Node* setter = get(bytecode.setter);
            Node* attributes = get(bytecode.attributes);

            addVarArgChild(base);
            addVarArgChild(property);
            addVarArgChild(getter);
            addVarArgChild(setter);
            addVarArgChild(attributes);
            addToGraph(Node::VarArg, DefineAccessorProperty, OpInfo(0), OpInfo(0));

            NEXT_OPCODE(op_define_accessor_property);
        }

        case op_get_by_id_direct: {
            parseGetById<OpGetByIdDirect>(currentInstruction);
            NEXT_OPCODE(op_get_by_id_direct);
        }
        case op_try_get_by_id: {
            parseGetById<OpTryGetById>(currentInstruction);
            NEXT_OPCODE(op_try_get_by_id);
        }
        case op_get_by_id: {
            parseGetById<OpGetById>(currentInstruction);
            NEXT_OPCODE(op_get_by_id);
        }
        case op_get_by_id_with_this: {
            SpeculatedType prediction = getPrediction();

            auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
            Node* base = get(bytecode.base);
            Node* thisValue = get(bytecode.thisValue);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];

            set(bytecode.dst,
                addToGraph(GetByIdWithThis, OpInfo(identifierNumber), OpInfo(prediction), base, thisValue));

            NEXT_OPCODE(op_get_by_id_with_this);
        }
        case op_put_by_id: {
            auto bytecode = currentInstruction->as<OpPutById>();
            Node* value = get(bytecode.value);
            Node* base = get(bytecode.base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];
            bool direct = bytecode.metadata(codeBlock).flags & PutByIdIsDirect;

            PutByIdStatus putByIdStatus = PutByIdStatus::computeFor(
                m_inlineStackTop->m_profiledBlock,
                m_inlineStackTop->m_baselineMap, m_icContextStack,
                currentCodeOrigin(), m_graph.identifiers()[identifierNumber]);
            
            handlePutById(base, identifierNumber, value, putByIdStatus, direct, currentInstruction->size());
            NEXT_OPCODE(op_put_by_id);
        }

        case op_put_by_id_with_this: {
            auto bytecode = currentInstruction->as<OpPutByIdWithThis>();
            Node* base = get(bytecode.base);
            Node* thisValue = get(bytecode.thisValue);
            Node* value = get(bytecode.value);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];

            addToGraph(PutByIdWithThis, OpInfo(identifierNumber), base, thisValue, value);
            NEXT_OPCODE(op_put_by_id_with_this);
        }

        case op_put_getter_by_id:
            handlePutAccessorById(PutGetterById, currentInstruction->as<OpPutGetterById>());
            NEXT_OPCODE(op_put_getter_by_id);
        case op_put_setter_by_id: {
            handlePutAccessorById(PutSetterById, currentInstruction->as<OpPutSetterById>());
            NEXT_OPCODE(op_put_setter_by_id);
        }

        case op_put_getter_setter_by_id: {
            auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
            Node* base = get(bytecode.base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];
            Node* getter = get(bytecode.getter);
            Node* setter = get(bytecode.setter);
            addToGraph(PutGetterSetterById, OpInfo(identifierNumber), OpInfo(bytecode.attributes), base, getter, setter);
            NEXT_OPCODE(op_put_getter_setter_by_id);
        }

        case op_put_getter_by_val:
            handlePutAccessorByVal(PutGetterByVal, currentInstruction->as<OpPutGetterByVal>());
            NEXT_OPCODE(op_put_getter_by_val);
        case op_put_setter_by_val: {
            handlePutAccessorByVal(PutSetterByVal, currentInstruction->as<OpPutSetterByVal>());
            NEXT_OPCODE(op_put_setter_by_val);
        }

        case op_del_by_id: {
            auto bytecode = currentInstruction->as<OpDelById>();
            Node* base = get(bytecode.base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];
            set(bytecode.dst, addToGraph(DeleteById, OpInfo(identifierNumber), base));
            NEXT_OPCODE(op_del_by_id);
        }

        case op_del_by_val: {
            auto bytecode = currentInstruction->as<OpDelByVal>();
            Node* base = get(bytecode.base);
            Node* key = get(bytecode.property);
            set(bytecode.dst, addToGraph(DeleteByVal, base, key));
            NEXT_OPCODE(op_del_by_val);
        }

        case op_profile_type: {
            auto bytecode = currentInstruction->as<OpProfileType>();
            auto& metadata = bytecode.metadata(codeBlock);
            Node* valueToProfile = get(bytecode.target);
            addToGraph(ProfileType, OpInfo(metadata.typeLocation), valueToProfile);
            NEXT_OPCODE(op_profile_type);
        }

        case op_profile_control_flow: {
            auto bytecode = currentInstruction->as<OpProfileControlFlow>();
            BasicBlockLocation* basicBlockLocation = bytecode.metadata(codeBlock).basicBlockLocation;
            addToGraph(ProfileControlFlow, OpInfo(basicBlockLocation));
            NEXT_OPCODE(op_profile_control_flow);
        }

        // === Block terminators. ===

        case op_jmp: {
            ASSERT(!m_currentBlock->terminal());
            auto bytecode = currentInstruction->as<OpJmp>();
            int relativeOffset = jumpTarget(bytecode.target);
            addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
            if (relativeOffset <= 0)
                flushForTerminal();
            LAST_OPCODE(op_jmp);
        }

        case op_jtrue: {
            auto bytecode = currentInstruction->as<OpJtrue>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* condition = get(bytecode.condition);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jtrue);
        }

        case op_jfalse: {
            auto bytecode = currentInstruction->as<OpJfalse>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* condition = get(bytecode.condition);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jfalse);
        }

        case op_jeq_null: {
            auto bytecode = currentInstruction->as<OpJeqNull>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* value = get(bytecode.value);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            Node* condition = addToGraph(CompareEq, value, nullConstant);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jeq_null);
        }

        case op_jneq_null: {
            auto bytecode = currentInstruction->as<OpJneqNull>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* value = get(bytecode.value);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            Node* condition = addToGraph(CompareEq, value, nullConstant);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jneq_null);
        }

        case op_jless: {
            auto bytecode = currentInstruction->as<OpJless>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jless);
        }

        case op_jlesseq: {
            auto bytecode = currentInstruction->as<OpJlesseq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jlesseq);
        }

        case op_jgreater: {
            auto bytecode = currentInstruction->as<OpJgreater>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jgreater);
        }

        case op_jgreatereq: {
            auto bytecode = currentInstruction->as<OpJgreatereq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jgreatereq);
        }

        case op_jeq: {
            auto bytecode = currentInstruction->as<OpJeq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jeq);
        }

        case op_jstricteq: {
            auto bytecode = currentInstruction->as<OpJstricteq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareStrictEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jstricteq);
        }

        case op_jnless: {
            auto bytecode = currentInstruction->as<OpJnless>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jnless);
        }

        case op_jnlesseq: {
            auto bytecode = currentInstruction->as<OpJnlesseq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jnlesseq);
        }

        case op_jngreater: {
            auto bytecode = currentInstruction->as<OpJngreater>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jngreater);
        }

        case op_jngreatereq: {
            auto bytecode = currentInstruction->as<OpJngreatereq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jngreatereq);
        }

        case op_jneq: {
            auto bytecode = currentInstruction->as<OpJneq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jneq);
        }

        case op_jnstricteq: {
            auto bytecode = currentInstruction->as<OpJnstricteq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareStrictEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
            LAST_OPCODE(op_jnstricteq);
        }

        case op_jbelow: {
            auto bytecode = currentInstruction->as<OpJbelow>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareBelow, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jbelow);
        }

        case op_jbeloweq: {
            auto bytecode = currentInstruction->as<OpJbeloweq>();
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* op1 = get(bytecode.lhs);
            Node* op2 = get(bytecode.rhs);
            Node* condition = addToGraph(CompareBelowEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex + relativeOffset, m_currentIndex + currentInstruction->size())), condition);
            LAST_OPCODE(op_jbeloweq);
        }

        case op_switch_imm: {
            auto bytecode = currentInstruction->as<OpSwitchImm>();
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchImm;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[bytecode.tableIndex];
            data.fallThrough.setBytecodeIndex(m_currentIndex + jumpTarget(bytecode.defaultOffset));
            SimpleJumpTable& table = m_codeBlock->switchJumpTable(data.switchTableIndex);
            for (unsigned i = 0; i < table.branchOffsets.size(); ++i) {
                if (!table.branchOffsets[i])
                    continue;
                unsigned target = m_currentIndex + table.branchOffsets[i];
                if (target == data.fallThrough.bytecodeIndex())
                    continue;
                data.cases.append(SwitchCase::withBytecodeIndex(m_graph.freeze(jsNumber(static_cast<int32_t>(table.min + i))), target));
            }
            addToGraph(Switch, OpInfo(&data), get(bytecode.scrutinee));
            flushIfTerminal(data);
            LAST_OPCODE(op_switch_imm);
        }
            
        case op_switch_char: {
            auto bytecode = currentInstruction->as<OpSwitchChar>();
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchChar;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[bytecode.tableIndex];
            data.fallThrough.setBytecodeIndex(m_currentIndex + jumpTarget(bytecode.defaultOffset));
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
            addToGraph(Switch, OpInfo(&data), get(bytecode.scrutinee));
            flushIfTerminal(data);
            LAST_OPCODE(op_switch_char);
        }

        case op_switch_string: {
            auto bytecode = currentInstruction->as<OpSwitchString>();
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchString;
            data.switchTableIndex = bytecode.tableIndex;
            data.fallThrough.setBytecodeIndex(m_currentIndex + jumpTarget(bytecode.defaultOffset));
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
            addToGraph(Switch, OpInfo(&data), get(bytecode.scrutinee));
            flushIfTerminal(data);
            LAST_OPCODE(op_switch_string);
        }

        case op_ret: {
            auto bytecode = currentInstruction->as<OpRet>();
            ASSERT(!m_currentBlock->terminal());
            if (!inlineCallFrame()) {
                // Simple case: we are just producing a return
                addToGraph(Return, get(bytecode.value));
                flushForReturn();
                LAST_OPCODE(op_ret);
            }

            flushForReturn();
            if (m_inlineStackTop->m_returnValue.isValid())
                setDirect(m_inlineStackTop->m_returnValue, get(bytecode.value), ImmediateSetWithFlush);

            if (!m_inlineStackTop->m_continuationBlock && m_currentIndex + currentInstruction->size() != m_inlineStackTop->m_codeBlock->instructions().size()) {
                // This is an early return from an inlined function and we do not have a continuation block, so we must allocate one.
                // It is untargetable, because we do not know the appropriate index.
                // If this block turns out to be a jump target, parseCodeBlock will fix its bytecodeIndex before putting it in m_blockLinkingTargets
                m_inlineStackTop->m_continuationBlock = allocateUntargetableBlock();
            }

            if (m_inlineStackTop->m_continuationBlock)
                addJumpTo(m_inlineStackTop->m_continuationBlock);
            else {
                // We are returning from an inlined function, and do not need to jump anywhere, so we just keep the current block
                m_inlineStackTop->m_continuationBlock = m_currentBlock;
            }
            LAST_OPCODE_LINKED(op_ret);
        }
        case op_end:
            ASSERT(!inlineCallFrame());
            addToGraph(Return, get(currentInstruction->as<OpEnd>().value));
            flushForReturn();
            LAST_OPCODE(op_end);

        case op_throw:
            addToGraph(Throw, get(currentInstruction->as<OpThrow>().value));
            flushForTerminal();
            LAST_OPCODE(op_throw);
            
        case op_throw_static_error: {
            auto bytecode = currentInstruction->as<OpThrowStaticError>();
            addToGraph(ThrowStaticError, OpInfo(bytecode.errorType), get(bytecode.message));
            flushForTerminal();
            LAST_OPCODE(op_throw_static_error);
        }

        case op_catch: {
            auto bytecode = currentInstruction->as<OpCatch>();
            m_graph.m_hasExceptionHandlers = true;

            if (inlineCallFrame()) {
                // We can't do OSR entry into an inlined frame.
                NEXT_OPCODE(op_catch);
            }

            if (m_graph.m_plan.mode() == FTLForOSREntryMode) {
                NEXT_OPCODE(op_catch);
            }

            RELEASE_ASSERT(!m_currentBlock->size() || (m_graph.compilation() && m_currentBlock->size() == 1 && m_currentBlock->at(0)->op() == CountExecution));

            ValueProfileAndOperandBuffer* buffer = bytecode.metadata(codeBlock).buffer;

            if (!buffer) {
                NEXT_OPCODE(op_catch); // This catch has yet to execute. Note: this load can be racy with the main thread.
            }

            // We're now committed to compiling this as an entrypoint.
            m_currentBlock->isCatchEntrypoint = true;
            m_graph.m_roots.append(m_currentBlock);

            Vector<SpeculatedType> argumentPredictions(m_numArguments);
            Vector<SpeculatedType> localPredictions;
            HashSet<unsigned, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> seenArguments;

            {
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);

                buffer->forEach([&] (ValueProfileAndOperand& profile) {
                    VirtualRegister operand(profile.m_operand);
                    SpeculatedType prediction = profile.m_profile.computeUpdatedPrediction(locker);
                    if (operand.isLocal())
                        localPredictions.append(prediction);
                    else {
                        RELEASE_ASSERT(operand.isArgument());
                        RELEASE_ASSERT(static_cast<uint32_t>(operand.toArgument()) < argumentPredictions.size());
                        if (validationEnabled())
                            seenArguments.add(operand.toArgument());
                        argumentPredictions[operand.toArgument()] = prediction;
                    }
                });

                if (validationEnabled()) {
                    for (unsigned argument = 0; argument < m_numArguments; ++argument)
                        RELEASE_ASSERT(seenArguments.contains(argument));
                }
            }

            Vector<std::pair<VirtualRegister, Node*>> localsToSet;
            localsToSet.reserveInitialCapacity(buffer->m_size); // Note: This will reserve more than the number of locals we see below because the buffer includes arguments.

            // We're not allowed to exit here since we would not properly recover values.
            // We first need to bootstrap the catch entrypoint state.
            m_exitOK = false; 

            unsigned numberOfLocals = 0;
            buffer->forEach([&] (ValueProfileAndOperand& profile) {
                VirtualRegister operand(profile.m_operand);
                if (operand.isArgument())
                    return;
                ASSERT(operand.isLocal());
                Node* value = addToGraph(ExtractCatchLocal, OpInfo(numberOfLocals), OpInfo(localPredictions[numberOfLocals]));
                ++numberOfLocals;
                addToGraph(MovHint, OpInfo(profile.m_operand), value);
                localsToSet.uncheckedAppend(std::make_pair(operand, value));
            });
            if (numberOfLocals)
                addToGraph(ClearCatchLocals);

            if (!m_graph.m_maxLocalsForCatchOSREntry)
                m_graph.m_maxLocalsForCatchOSREntry = 0;
            m_graph.m_maxLocalsForCatchOSREntry = std::max(numberOfLocals, *m_graph.m_maxLocalsForCatchOSREntry);

            // We could not exit before this point in the program because we would not know how to do value
            // recovery for live locals. The above IR sets up the necessary state so we can recover values
            // during OSR exit. 
            //
            // The nodes that follow here all exit to the following bytecode instruction, not
            // the op_catch. Exiting to op_catch is reserved for when an exception is thrown.
            // The SetArgument nodes that follow below may exit because we may hoist type checks
            // to them. The SetLocal nodes that follow below may exit because we may choose
            // a flush format that speculates on the type of the local.
            m_exitOK = true; 
            addToGraph(ExitOK);

            {
                auto addResult = m_graph.m_rootToArguments.add(m_currentBlock, ArgumentsVector());
                RELEASE_ASSERT(addResult.isNewEntry);
                ArgumentsVector& entrypointArguments = addResult.iterator->value;
                entrypointArguments.resize(m_numArguments);

                unsigned exitBytecodeIndex = m_currentIndex + currentInstruction->size();

                for (unsigned argument = 0; argument < argumentPredictions.size(); ++argument) {
                    VariableAccessData* variable = newVariableAccessData(virtualRegisterForArgument(argument));
                    variable->predict(argumentPredictions[argument]);

                    variable->mergeStructureCheckHoistingFailed(
                        m_inlineStackTop->m_exitProfile.hasExitSite(exitBytecodeIndex, BadCache));
                    variable->mergeCheckArrayHoistingFailed(
                        m_inlineStackTop->m_exitProfile.hasExitSite(exitBytecodeIndex, BadIndexingType));

                    Node* setArgument = addToGraph(SetArgument, OpInfo(variable));
                    setArgument->origin.forExit.bytecodeIndex = exitBytecodeIndex;
                    m_currentBlock->variablesAtTail.setArgumentFirstTime(argument, setArgument);
                    entrypointArguments[argument] = setArgument;
                }
            }

            for (const std::pair<VirtualRegister, Node*>& pair : localsToSet) {
                DelayedSetLocal delayed { currentCodeOrigin(), pair.first, pair.second, ImmediateNakedSet };
                m_setLocalQueue.append(delayed);
            }

            NEXT_OPCODE(op_catch);
        }

        case op_call:
            handleCall<OpCall>(currentInstruction, Call, CallMode::Regular);
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleCall, which may have inlined the callee, trashed m_currentInstruction");
            NEXT_OPCODE(op_call);

        case op_tail_call: {
            flushForReturn();
            Terminality terminality = handleCall<OpTailCall>(currentInstruction, TailCall, CallMode::Tail);
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleCall, which may have inlined the callee, trashed m_currentInstruction");
            // If the call is terminal then we should not parse any further bytecodes as the TailCall will exit the function.
            // If the call is not terminal, however, then we want the subsequent op_ret/op_jmp to update metadata and clean
            // things up.
            if (terminality == NonTerminal)
                NEXT_OPCODE(op_tail_call);
            else
                LAST_OPCODE_LINKED(op_tail_call);
            // We use LAST_OPCODE_LINKED instead of LAST_OPCODE because if the tail call was optimized, it may now be a jump to a bytecode index in a different InlineStackEntry.
        }

        case op_construct:
            handleCall<OpConstruct>(currentInstruction, Construct, CallMode::Construct);
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleCall, which may have inlined the callee, trashed m_currentInstruction");
            NEXT_OPCODE(op_construct);
            
        case op_call_varargs: {
            handleVarargsCall<OpCallVarargs>(currentInstruction, CallVarargs, CallMode::Regular);
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleVarargsCall, which may have inlined the callee, trashed m_currentInstruction");
            NEXT_OPCODE(op_call_varargs);
        }

        case op_tail_call_varargs: {
            flushForReturn();
            Terminality terminality = handleVarargsCall<OpTailCallVarargs>(currentInstruction, TailCallVarargs, CallMode::Tail);
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleVarargsCall, which may have inlined the callee, trashed m_currentInstruction");
            // If the call is terminal then we should not parse any further bytecodes as the TailCall will exit the function.
            // If the call is not terminal, however, then we want the subsequent op_ret/op_jmp to update metadata and clean
            // things up.
            if (terminality == NonTerminal)
                NEXT_OPCODE(op_tail_call_varargs);
            else
                LAST_OPCODE(op_tail_call_varargs);
        }

        case op_tail_call_forward_arguments: {
            // We need to make sure that we don't unbox our arguments here since that won't be
            // done by the arguments object creation node as that node may not exist.
            noticeArgumentsUse();
            flushForReturn();
            Terminality terminality = handleVarargsCall<OpTailCallForwardArguments>(currentInstruction, TailCallForwardVarargs, CallMode::Tail);
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleVarargsCall, which may have inlined the callee, trashed m_currentInstruction");
            // If the call is terminal then we should not parse any further bytecodes as the TailCall will exit the function.
            // If the call is not terminal, however, then we want the subsequent op_ret/op_jmp to update metadata and clean
            // things up.
            if (terminality == NonTerminal)
                NEXT_OPCODE(op_tail_call_forward_arguments);
            else
                LAST_OPCODE(op_tail_call_forward_arguments);
        }
            
        case op_construct_varargs: {
            handleVarargsCall<OpConstructVarargs>(currentInstruction, ConstructVarargs, CallMode::Construct);
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleVarargsCall, which may have inlined the callee, trashed m_currentInstruction");
            NEXT_OPCODE(op_construct_varargs);
        }
            
        case op_call_eval: {
            auto bytecode = currentInstruction->as<OpCallEval>();
            int registerOffset = -bytecode.argv;
            addCall(bytecode.dst, CallEval, nullptr, get(bytecode.callee), bytecode.argc, registerOffset, getPrediction());
            NEXT_OPCODE(op_call_eval);
        }
            
        case op_jneq_ptr: {
            auto bytecode = currentInstruction->as<OpJneqPtr>();
            Special::Pointer specialPointer = bytecode.specialPointer;
            ASSERT(pointerIsCell(specialPointer));
            JSCell* actualPointer = static_cast<JSCell*>(
                actualPointerFor(m_inlineStackTop->m_codeBlock, specialPointer));
            FrozenValue* frozenPointer = m_graph.freeze(actualPointer);
            unsigned relativeOffset = jumpTarget(bytecode.target);
            Node* child = get(bytecode.value);
            if (bytecode.metadata(codeBlock).hasJumped) {
                Node* condition = addToGraph(CompareEqPtr, OpInfo(frozenPointer), child);
                addToGraph(Branch, OpInfo(branchData(m_currentIndex + currentInstruction->size(), m_currentIndex + relativeOffset)), condition);
                LAST_OPCODE(op_jneq_ptr);
            }
            addToGraph(CheckCell, OpInfo(frozenPointer), child);
            NEXT_OPCODE(op_jneq_ptr);
        }

        case op_resolve_scope: {
            auto bytecode = currentInstruction->as<OpResolveScope>();
            auto& metadata = bytecode.metadata(codeBlock);
            unsigned depth = metadata.localScopeDepth;

            if (needsDynamicLookup(metadata.resolveType, op_resolve_scope)) {
                unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.var];
                set(bytecode.dst, addToGraph(ResolveScope, OpInfo(identifierNumber), get(bytecode.scope)));
                NEXT_OPCODE(op_resolve_scope);
            }

            // get_from_scope and put_to_scope depend on this watchpoint forcing OSR exit, so they don't add their own watchpoints.
            if (needsVarInjectionChecks(metadata.resolveType))
                m_graph.watchpoints().addLazily(m_inlineStackTop->m_codeBlock->globalObject()->varInjectionWatchpoint());

            switch (metadata.resolveType) {
            case GlobalProperty:
            case GlobalVar:
            case GlobalPropertyWithVarInjectionChecks:
            case GlobalVarWithVarInjectionChecks:
            case GlobalLexicalVar:
            case GlobalLexicalVarWithVarInjectionChecks: {
                JSScope* constantScope = JSScope::constantScopeForCodeBlock(metadata.resolveType, m_inlineStackTop->m_codeBlock);
                RELEASE_ASSERT(constantScope);
                RELEASE_ASSERT(metadata.constantScope.get() == constantScope);
                set(bytecode.dst, weakJSConstant(constantScope));
                addToGraph(Phantom, get(bytecode.scope));
                break;
            }
            case ModuleVar: {
                // Since the value of the "scope" virtual register is not used in LLInt / baseline op_resolve_scope with ModuleVar,
                // we need not to keep it alive by the Phantom node.
                // Module environment is already strongly referenced by the CodeBlock.
                set(bytecode.dst, weakJSConstant(metadata.lexicalEnvironment.get()));
                break;
            }
            case LocalClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* localBase = get(bytecode.scope);
                addToGraph(Phantom, localBase); // OSR exit cannot handle resolve_scope on a DCE'd scope.
                
                // We have various forms of constant folding here. This is necessary to avoid
                // spurious recompiles in dead-but-foldable code.
                if (SymbolTable* symbolTable = metadata.symbolTable.get()) {
                    InferredValue* singleton = symbolTable->singletonScope();
                    if (JSValue value = singleton->inferredValue()) {
                        m_graph.watchpoints().addLazily(singleton);
                        set(bytecode.dst, weakJSConstant(value));
                        break;
                    }
                }
                if (JSScope* scope = localBase->dynamicCastConstant<JSScope*>(*m_vm)) {
                    for (unsigned n = depth; n--;)
                        scope = scope->next();
                    set(bytecode.dst, weakJSConstant(scope));
                    break;
                }
                for (unsigned n = depth; n--;)
                    localBase = addToGraph(SkipScope, localBase);
                set(bytecode.dst, localBase);
                break;
            }
            case UnresolvedProperty:
            case UnresolvedPropertyWithVarInjectionChecks: {
                addToGraph(Phantom, get(bytecode.scope));
                addToGraph(ForceOSRExit);
                set(bytecode.dst, addToGraph(JSConstant, OpInfo(m_constantNull)));
                break;
            }
            case Dynamic:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            NEXT_OPCODE(op_resolve_scope);
        }
        case op_resolve_scope_for_hoisting_func_decl_in_eval: {
            auto bytecode = currentInstruction->as<OpResolveScopeForHoistingFuncDeclInEval>();
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];

            set(bytecode.dst, addToGraph(ResolveScopeForHoistingFuncDeclInEval, OpInfo(identifierNumber), get(bytecode.scope)));

            NEXT_OPCODE(op_resolve_scope_for_hoisting_func_decl_in_eval);
        }

        case op_get_from_scope: {
            auto bytecode = currentInstruction->as<OpGetFromScope>();
            auto& metadata = bytecode.metadata(codeBlock);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.var];
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
            ResolveType resolveType = metadata.getPutInfo.resolveType();

            Structure* structure = 0;
            WatchpointSet* watchpoints = 0;
            uintptr_t operand;
            {
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks || resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                    watchpoints = metadata.watchpointSet;
                else if (resolveType != UnresolvedProperty && resolveType != UnresolvedPropertyWithVarInjectionChecks)
                    structure = metadata.structure.get();
                operand = metadata.operand;
            }

            if (needsDynamicLookup(resolveType, op_get_from_scope)) {
                uint64_t opInfo1 = makeDynamicVarOpInfo(identifierNumber, metadata.getPutInfo.operand());
                SpeculatedType prediction = getPrediction();
                set(bytecode.dst,
                    addToGraph(GetDynamicVar, OpInfo(opInfo1), OpInfo(prediction), get(bytecode.scope)));
                NEXT_OPCODE(op_get_from_scope);
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
                    set(bytecode.dst, addToGraph(GetByIdFlush, OpInfo(identifierNumber), OpInfo(prediction), get(bytecode.scope)));
                    break;
                }

                Node* base = weakJSConstant(globalObject);
                Node* result = load(prediction, base, identifierNumber, status[0]);
                addToGraph(Phantom, get(bytecode.scope));
                set(bytecode.dst, result);
                break;
            }
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks:
            case GlobalLexicalVar:
            case GlobalLexicalVarWithVarInjectionChecks: {
                addToGraph(Phantom, get(bytecode.scope));
                WatchpointSet* watchpointSet;
                ScopeOffset offset;
                JSSegmentedVariableObject* scopeObject = jsCast<JSSegmentedVariableObject*>(JSScope::constantScopeForCodeBlock(resolveType, m_inlineStackTop->m_codeBlock));
                {
                    ConcurrentJSLocker locker(scopeObject->symbolTable()->m_lock);
                    SymbolTableEntry entry = scopeObject->symbolTable()->get(locker, uid);
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
                    
                    ASSERT(scopeObject->findVariableIndex(pointer) == offset);
                    
                    JSValue value = pointer->get();
                    if (value) {
                        m_graph.watchpoints().addLazily(watchpointSet);
                        set(bytecode.dst, weakJSConstant(value));
                        break;
                    }
                }
                
                SpeculatedType prediction = getPrediction();
                NodeType nodeType;
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks)
                    nodeType = GetGlobalVar;
                else
                    nodeType = GetGlobalLexicalVariable;
                Node* value = addToGraph(nodeType, OpInfo(operand), OpInfo(prediction));
                if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                    addToGraph(CheckNotEmpty, value);
                set(bytecode.dst, value);
                break;
            }
            case LocalClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(bytecode.scope);
                
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
                    set(bytecode.dst, weakJSConstant(value));
                    break;
                }
                SpeculatedType prediction = getPrediction();
                set(bytecode.dst,
                    addToGraph(GetClosureVar, OpInfo(operand), OpInfo(prediction), scopeNode));
                break;
            }
            case UnresolvedProperty:
            case UnresolvedPropertyWithVarInjectionChecks:
            case ModuleVar:
            case Dynamic:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            NEXT_OPCODE(op_get_from_scope);
        }

        case op_put_to_scope: {
            auto bytecode = currentInstruction->as<OpPutToScope>();
            auto& metadata = bytecode.metadata(codeBlock);
            unsigned identifierNumber = bytecode.var;
            if (identifierNumber != UINT_MAX)
                identifierNumber = m_inlineStackTop->m_identifierRemap[identifierNumber];
            ResolveType resolveType = metadata.getPutInfo.resolveType();
            UniquedStringImpl* uid;
            if (identifierNumber != UINT_MAX)
                uid = m_graph.identifiers()[identifierNumber];
            else
                uid = nullptr;
            
            Structure* structure = nullptr;
            WatchpointSet* watchpoints = nullptr;
            uintptr_t operand;
            {
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks || resolveType == LocalClosureVar || resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                    watchpoints = metadata.watchpointSet;
                else if (resolveType != UnresolvedProperty && resolveType != UnresolvedPropertyWithVarInjectionChecks)
                    structure = metadata.structure.get();
                operand = metadata.operand;
            }

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            if (needsDynamicLookup(resolveType, op_put_to_scope)) {
                ASSERT(identifierNumber != UINT_MAX);
                uint64_t opInfo1 = makeDynamicVarOpInfo(identifierNumber, metadata.getPutInfo.operand());
                addToGraph(PutDynamicVar, OpInfo(opInfo1), OpInfo(), get(bytecode.scope), get(bytecode.value));
                NEXT_OPCODE(op_put_to_scope);
            }

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
                    addToGraph(PutById, OpInfo(identifierNumber), get(bytecode.scope), get(bytecode.value));
                    break;
                }
                Node* base = weakJSConstant(globalObject);
                store(base, identifierNumber, status[0], get(bytecode.value));
                // Keep scope alive until after put.
                addToGraph(Phantom, get(bytecode.scope));
                break;
            }
            case GlobalLexicalVar:
            case GlobalLexicalVarWithVarInjectionChecks:
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks: {
                if (!isInitialization(metadata.getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                    SpeculatedType prediction = SpecEmpty;
                    Node* value = addToGraph(GetGlobalLexicalVariable, OpInfo(operand), OpInfo(prediction));
                    addToGraph(CheckNotEmpty, value);
                }

                JSSegmentedVariableObject* scopeObject = jsCast<JSSegmentedVariableObject*>(JSScope::constantScopeForCodeBlock(resolveType, m_inlineStackTop->m_codeBlock));
                if (watchpoints) {
                    SymbolTableEntry entry = scopeObject->symbolTable()->get(uid);
                    ASSERT_UNUSED(entry, watchpoints == entry.watchpointSet());
                }
                Node* valueNode = get(bytecode.value);
                addToGraph(PutGlobalVariable, OpInfo(operand), weakJSConstant(scopeObject), valueNode);
                if (watchpoints && watchpoints->state() != IsInvalidated) {
                    // Must happen after the store. See comment for GetGlobalVar.
                    addToGraph(NotifyWrite, OpInfo(watchpoints));
                }
                // Keep scope alive until after put.
                addToGraph(Phantom, get(bytecode.scope));
                break;
            }
            case LocalClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(bytecode.scope);
                Node* valueNode = get(bytecode.value);

                addToGraph(PutClosureVar, OpInfo(operand), scopeNode, valueNode);

                if (watchpoints && watchpoints->state() != IsInvalidated) {
                    // Must happen after the store. See comment for GetGlobalVar.
                    addToGraph(NotifyWrite, OpInfo(watchpoints));
                }
                break;
            }

            case ModuleVar:
                // Need not to keep "scope" and "value" register values here by Phantom because
                // they are not used in LLInt / baseline op_put_to_scope with ModuleVar.
                addToGraph(ForceOSRExit);
                break;

            case Dynamic:
            case UnresolvedProperty:
            case UnresolvedPropertyWithVarInjectionChecks:
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
            NEXT_OPCODE(op_loop_hint);
        }
        
        case op_check_traps: {
            addToGraph(Options::usePollingTraps() ? CheckTraps : InvalidationPoint);
            NEXT_OPCODE(op_check_traps);
        }

        case op_nop: {
            addToGraph(Check); // We add a nop here so that basic block linking doesn't break.
            NEXT_OPCODE(op_nop);
        }

        case op_super_sampler_begin: {
            addToGraph(SuperSamplerBegin);
            NEXT_OPCODE(op_super_sampler_begin);
        }

        case op_super_sampler_end: {
            addToGraph(SuperSamplerEnd);
            NEXT_OPCODE(op_super_sampler_end);
        }

        case op_create_lexical_environment: {
            auto bytecode = currentInstruction->as<OpCreateLexicalEnvironment>();
            ASSERT(bytecode.symbolTable.isConstant() && bytecode.initialValue.isConstant());
            FrozenValue* symbolTable = m_graph.freezeStrong(m_inlineStackTop->m_codeBlock->getConstant(bytecode.symbolTable.offset()));
            FrozenValue* initialValue = m_graph.freezeStrong(m_inlineStackTop->m_codeBlock->getConstant(bytecode.initialValue.offset()));
            Node* scope = get(bytecode.scope);
            Node* lexicalEnvironment = addToGraph(CreateActivation, OpInfo(symbolTable), OpInfo(initialValue), scope);
            set(bytecode.dst, lexicalEnvironment);
            NEXT_OPCODE(op_create_lexical_environment);
        }

        case op_push_with_scope: {
            auto bytecode = currentInstruction->as<OpPushWithScope>();
            Node* currentScope = get(bytecode.currentScope);
            Node* object = get(bytecode.newScope);
            set(bytecode.dst, addToGraph(PushWithScope, currentScope, object));
            NEXT_OPCODE(op_push_with_scope);
        }

        case op_get_parent_scope: {
            auto bytecode = currentInstruction->as<OpGetParentScope>();
            Node* currentScope = get(bytecode.scope);
            Node* newScope = addToGraph(SkipScope, currentScope);
            set(bytecode.dst, newScope);
            addToGraph(Phantom, currentScope);
            NEXT_OPCODE(op_get_parent_scope);
        }

        case op_get_scope: {
            // Help the later stages a bit by doing some small constant folding here. Note that this
            // only helps for the first basic block. It's extremely important not to constant fold
            // loads from the scope register later, as that would prevent the DFG from tracking the
            // bytecode-level liveness of the scope register.
            auto bytecode = currentInstruction->as<OpGetScope>();
            Node* callee = get(VirtualRegister(CallFrameSlot::callee));
            Node* result;
            if (JSFunction* function = callee->dynamicCastConstant<JSFunction*>(*m_vm))
                result = weakJSConstant(function->scope());
            else
                result = addToGraph(GetScope, callee);
            set(bytecode.dst, result);
            NEXT_OPCODE(op_get_scope);
        }

        case op_argument_count: {
            auto bytecode = currentInstruction->as<OpArgumentCount>();
            Node* sub = addToGraph(ArithSub, OpInfo(Arith::Unchecked), OpInfo(SpecInt32Only), getArgumentCount(), addToGraph(JSConstant, OpInfo(m_constantOne)));
            set(bytecode.dst, sub);
            NEXT_OPCODE(op_argument_count);
        }

        case op_create_direct_arguments: {
            auto bytecode = currentInstruction->as<OpCreateDirectArguments>();
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateDirectArguments);
            set(bytecode.dst, createArguments);
            NEXT_OPCODE(op_create_direct_arguments);
        }
            
        case op_create_scoped_arguments: {
            auto bytecode = currentInstruction->as<OpCreateScopedArguments>();
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateScopedArguments, get(bytecode.scope));
            set(bytecode.dst, createArguments);
            NEXT_OPCODE(op_create_scoped_arguments);
        }

        case op_create_cloned_arguments: {
            auto bytecode = currentInstruction->as<OpCreateClonedArguments>();
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateClonedArguments);
            set(bytecode.dst, createArguments);
            NEXT_OPCODE(op_create_cloned_arguments);
        }
            
        case op_get_from_arguments: {
            auto bytecode = currentInstruction->as<OpGetFromArguments>();
            set(bytecode.dst,
                addToGraph(
                    GetFromArguments,
                    OpInfo(bytecode.index),
                    OpInfo(getPrediction()),
                    get(bytecode.arguments)));
            NEXT_OPCODE(op_get_from_arguments);
        }
            
        case op_put_to_arguments: {
            auto bytecode = currentInstruction->as<OpPutToArguments>();
            addToGraph(
                PutToArguments,
                OpInfo(bytecode.index),
                get(bytecode.arguments),
                get(bytecode.value));
            NEXT_OPCODE(op_put_to_arguments);
        }

        case op_get_argument: {
            auto bytecode = currentInstruction->as<OpGetArgument>();
            InlineCallFrame* inlineCallFrame = this->inlineCallFrame();
            Node* argument;
            int32_t argumentIndexIncludingThis = bytecode.index;
            if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
                int32_t argumentCountIncludingThisWithFixup = inlineCallFrame->argumentsWithFixup.size();
                if (argumentIndexIncludingThis < argumentCountIncludingThisWithFixup)
                    argument = get(virtualRegisterForArgument(argumentIndexIncludingThis));
                else
                    argument = addToGraph(JSConstant, OpInfo(m_constantUndefined));
            } else
                argument = addToGraph(GetArgument, OpInfo(argumentIndexIncludingThis), OpInfo(getPrediction()));
            set(bytecode.dst, argument);
            NEXT_OPCODE(op_get_argument);
        }
        case op_new_async_generator_func:
            handleNewFunc(NewAsyncGeneratorFunction, currentInstruction->as<OpNewAsyncGeneratorFunc>());
            NEXT_OPCODE(op_new_async_generator_func);
        case op_new_func:
            handleNewFunc(NewFunction, currentInstruction->as<OpNewFunc>());
            NEXT_OPCODE(op_new_func);
        case op_new_generator_func:
            handleNewFunc(NewGeneratorFunction, currentInstruction->as<OpNewGeneratorFunc>());
            NEXT_OPCODE(op_new_generator_func);
        case op_new_async_func:
            handleNewFunc(NewAsyncFunction, currentInstruction->as<OpNewAsyncFunc>());
            NEXT_OPCODE(op_new_async_func);

        case op_new_func_exp:
            handleNewFuncExp(NewFunction, currentInstruction->as<OpNewFuncExp>());
            NEXT_OPCODE(op_new_func_exp);
        case op_new_generator_func_exp:
            handleNewFuncExp(NewGeneratorFunction, currentInstruction->as<OpNewGeneratorFuncExp>());
            NEXT_OPCODE(op_new_generator_func_exp);
        case op_new_async_generator_func_exp:
            handleNewFuncExp(NewAsyncGeneratorFunction, currentInstruction->as<OpNewAsyncGeneratorFuncExp>());
            NEXT_OPCODE(op_new_async_generator_func_exp);
        case op_new_async_func_exp:
            handleNewFuncExp(NewAsyncFunction, currentInstruction->as<OpNewAsyncFuncExp>());
            NEXT_OPCODE(op_new_async_func_exp);

        case op_set_function_name: {
            auto bytecode = currentInstruction->as<OpSetFunctionName>();
            Node* func = get(bytecode.function);
            Node* name = get(bytecode.name);
            addToGraph(SetFunctionName, func, name);
            NEXT_OPCODE(op_set_function_name);
        }

        case op_typeof: {
            auto bytecode = currentInstruction->as<OpTypeof>();
            set(bytecode.dst, addToGraph(TypeOf, get(bytecode.value)));
            NEXT_OPCODE(op_typeof);
        }

        case op_to_number: {
            auto bytecode = currentInstruction->as<OpToNumber>();
            SpeculatedType prediction = getPrediction();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(ToNumber, OpInfo(0), OpInfo(prediction), value));
            NEXT_OPCODE(op_to_number);
        }

        case op_to_string: {
            auto bytecode = currentInstruction->as<OpToString>();
            Node* value = get(bytecode.operand);
            set(bytecode.dst, addToGraph(ToString, value));
            NEXT_OPCODE(op_to_string);
        }

        case op_to_object: {
            auto bytecode = currentInstruction->as<OpToObject>();
            SpeculatedType prediction = getPrediction();
            Node* value = get(bytecode.operand);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.message];
            set(bytecode.dst, addToGraph(ToObject, OpInfo(identifierNumber), OpInfo(prediction), value));
            NEXT_OPCODE(op_to_object);
        }

        case op_in_by_val: {
            auto bytecode = currentInstruction->as<OpInByVal>();
            ArrayMode arrayMode = getArrayMode(bytecode.metadata(codeBlock).arrayProfile, Array::Read);
            set(bytecode.dst, addToGraph(InByVal, OpInfo(arrayMode.asWord()), get(bytecode.base), get(bytecode.property)));
            NEXT_OPCODE(op_in_by_val);
        }

        case op_in_by_id: {
            auto bytecode = currentInstruction->as<OpInById>();
            Node* base = get(bytecode.base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];

            InByIdStatus status = InByIdStatus::computeFor(
                m_inlineStackTop->m_profiledBlock,
                m_inlineStackTop->m_baselineMap, m_icContextStack,
                currentCodeOrigin(), uid);

            if (status.isSimple()) {
                bool allOK = true;
                MatchStructureData* data = m_graph.m_matchStructureData.add();
                for (const InByIdVariant& variant : status.variants()) {
                    if (!check(variant.conditionSet())) {
                        allOK = false;
                        break;
                    }
                    for (Structure* structure : variant.structureSet()) {
                        MatchStructureVariant matchVariant;
                        matchVariant.structure = m_graph.registerStructure(structure);
                        matchVariant.result = variant.isHit();

                        data->variants.append(WTFMove(matchVariant));
                    }
                }

                if (allOK) {
                    addToGraph(FilterInByIdStatus, OpInfo(m_graph.m_plan.recordedStatuses().addInByIdStatus(currentCodeOrigin(), status)), base);

                    Node* match = addToGraph(MatchStructure, OpInfo(data), base);
                    set(bytecode.dst, match);
                    NEXT_OPCODE(op_in_by_id);
                }
            }

            set(bytecode.dst, addToGraph(InById, OpInfo(identifierNumber), base));
            NEXT_OPCODE(op_in_by_id);
        }

        case op_get_enumerable_length: {
            auto bytecode = currentInstruction->as<OpGetEnumerableLength>();
            set(bytecode.dst, addToGraph(GetEnumerableLength, get(bytecode.base)));
            NEXT_OPCODE(op_get_enumerable_length);
        }

        case op_has_generic_property: {
            auto bytecode = currentInstruction->as<OpHasGenericProperty>();
            set(bytecode.dst, addToGraph(HasGenericProperty, get(bytecode.base), get(bytecode.property)));
            NEXT_OPCODE(op_has_generic_property);
        }

        case op_has_structure_property: {
            auto bytecode = currentInstruction->as<OpHasStructureProperty>();
            set(bytecode.dst, addToGraph(HasStructureProperty, 
                get(bytecode.base),
                get(bytecode.property),
                get(bytecode.enumerator)));
            NEXT_OPCODE(op_has_structure_property);
        }

        case op_has_indexed_property: {
            auto bytecode = currentInstruction->as<OpHasIndexedProperty>();
            Node* base = get(bytecode.base);
            ArrayMode arrayMode = getArrayMode(bytecode.metadata(codeBlock).arrayProfile, Array::Read);
            Node* property = get(bytecode.property);
            Node* hasIterableProperty = addToGraph(HasIndexedProperty, OpInfo(arrayMode.asWord()), OpInfo(static_cast<uint32_t>(PropertySlot::InternalMethodType::GetOwnProperty)), base, property);
            set(bytecode.dst, hasIterableProperty);
            NEXT_OPCODE(op_has_indexed_property);
        }

        case op_get_direct_pname: {
            auto bytecode = currentInstruction->as<OpGetDirectPname>();
            SpeculatedType prediction = getPredictionWithoutOSRExit();
            
            Node* base = get(bytecode.base);
            Node* property = get(bytecode.property);
            Node* index = get(bytecode.index);
            Node* enumerator = get(bytecode.enumerator);

            addVarArgChild(base);
            addVarArgChild(property);
            addVarArgChild(index);
            addVarArgChild(enumerator);
            set(bytecode.dst, addToGraph(Node::VarArg, GetDirectPname, OpInfo(0), OpInfo(prediction)));

            NEXT_OPCODE(op_get_direct_pname);
        }

        case op_get_property_enumerator: {
            auto bytecode = currentInstruction->as<OpGetPropertyEnumerator>();
            set(bytecode.dst, addToGraph(GetPropertyEnumerator, get(bytecode.base)));
            NEXT_OPCODE(op_get_property_enumerator);
        }

        case op_enumerator_structure_pname: {
            auto bytecode = currentInstruction->as<OpEnumeratorStructurePname>();
            set(bytecode.dst, addToGraph(GetEnumeratorStructurePname,
                get(bytecode.enumerator),
                get(bytecode.index)));
            NEXT_OPCODE(op_enumerator_structure_pname);
        }

        case op_enumerator_generic_pname: {
            auto bytecode = currentInstruction->as<OpEnumeratorGenericPname>();
            set(bytecode.dst, addToGraph(GetEnumeratorGenericPname,
                get(bytecode.enumerator),
                get(bytecode.index)));
            NEXT_OPCODE(op_enumerator_generic_pname);
        }
            
        case op_to_index_string: {
            auto bytecode = currentInstruction->as<OpToIndexString>();
            set(bytecode.dst, addToGraph(ToIndexString, get(bytecode.index)));
            NEXT_OPCODE(op_to_index_string);
        }
            
        case op_log_shadow_chicken_prologue: {
            auto bytecode = currentInstruction->as<OpLogShadowChickenPrologue>();
            if (!m_inlineStackTop->m_inlineCallFrame)
                addToGraph(LogShadowChickenPrologue, get(bytecode.scope));
            NEXT_OPCODE(op_log_shadow_chicken_prologue);
        }

        case op_log_shadow_chicken_tail: {
            auto bytecode = currentInstruction->as<OpLogShadowChickenTail>();
            if (!m_inlineStackTop->m_inlineCallFrame) {
                // FIXME: The right solution for inlining is to elide these whenever the tail call
                // ends up being inlined.
                // https://bugs.webkit.org/show_bug.cgi?id=155686
                addToGraph(LogShadowChickenTail, get(bytecode.thisValue), get(bytecode.scope));
            }
            NEXT_OPCODE(op_log_shadow_chicken_tail);
        }
            
        case op_unreachable: {
            flushForTerminal();
            addToGraph(Unreachable);
            LAST_OPCODE(op_unreachable);
        }

        default:
            // Parse failed! This should not happen because the capabilities checker
            // should have caught it.
            RELEASE_ASSERT_NOT_REACHED();
            return;
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
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    VERBOSE_LOG("Marking ", RawPointer(block), " as linked (actually did linking)\n");
    block->didLink();
}

void ByteCodeParser::linkBlocks(Vector<BasicBlock*>& unlinkedBlocks, Vector<BasicBlock*>& possibleTargets)
{
    for (size_t i = 0; i < unlinkedBlocks.size(); ++i) {
        VERBOSE_LOG("Attempting to link ", RawPointer(unlinkedBlocks[i]), "\n");
        linkBlock(unlinkedBlocks[i], possibleTargets);
    }
}

ByteCodeParser::InlineStackEntry::InlineStackEntry(
    ByteCodeParser* byteCodeParser,
    CodeBlock* codeBlock,
    CodeBlock* profiledBlock,
    JSFunction* callee, // Null if this is a closure call.
    VirtualRegister returnValueVR,
    VirtualRegister inlineCallFrameStart,
    int argumentCountIncludingThis,
    InlineCallFrame::Kind kind,
    BasicBlock* continuationBlock)
    : m_byteCodeParser(byteCodeParser)
    , m_codeBlock(codeBlock)
    , m_profiledBlock(profiledBlock)
    , m_continuationBlock(continuationBlock)
    , m_returnValue(returnValueVR)
    , m_caller(byteCodeParser->m_inlineStackTop)
{
    {
        m_exitProfile.initialize(m_profiledBlock->unlinkedCodeBlock());

        ConcurrentJSLocker locker(m_profiledBlock->m_lock);
        m_lazyOperands.initialize(locker, m_profiledBlock->lazyOperandValueProfiles());
        
        // We do this while holding the lock because we want to encourage StructureStubInfo's
        // to be potentially added to operations and because the profiled block could be in the
        // middle of LLInt->JIT tier-up in which case we would be adding the info's right now.
        if (m_profiledBlock->hasBaselineJITProfiling())
            m_profiledBlock->getICStatusMap(locker, m_baselineMap);
    }
    
    CodeBlock* optimizedBlock = m_profiledBlock->replacement();
    m_optimizedContext.optimizedCodeBlock = optimizedBlock;
    if (Options::usePolyvariantDevirtualization() && optimizedBlock) {
        ConcurrentJSLocker locker(optimizedBlock->m_lock);
        optimizedBlock->getICStatusMap(locker, m_optimizedContext.map);
    }
    byteCodeParser->m_icContextStack.append(&m_optimizedContext);
    
    int argumentCountIncludingThisWithFixup = std::max<int>(argumentCountIncludingThis, codeBlock->numParameters());

    if (m_caller) {
        // Inline case.
        ASSERT(codeBlock != byteCodeParser->m_codeBlock);
        ASSERT(inlineCallFrameStart.isValid());

        m_inlineCallFrame = byteCodeParser->m_graph.m_plan.inlineCallFrames()->add();
        m_optimizedContext.inlineCallFrame = m_inlineCallFrame;

        // The owner is the machine code block, and we already have a barrier on that when the
        // plan finishes.
        m_inlineCallFrame->baselineCodeBlock.setWithoutWriteBarrier(codeBlock->baselineVersion());
        m_inlineCallFrame->setStackOffset(inlineCallFrameStart.offset() - CallFrame::headerSizeInRegisters);
        m_inlineCallFrame->argumentCountIncludingThis = argumentCountIncludingThis;
        if (callee) {
            m_inlineCallFrame->calleeRecovery = ValueRecovery::constant(callee);
            m_inlineCallFrame->isClosureCall = false;
        } else
            m_inlineCallFrame->isClosureCall = true;
        m_inlineCallFrame->directCaller = byteCodeParser->currentCodeOrigin();
        m_inlineCallFrame->argumentsWithFixup.resizeToFit(argumentCountIncludingThisWithFixup); // Set the number of arguments including this, but don't configure the value recoveries, yet.
        m_inlineCallFrame->kind = kind;
        
        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        m_switchRemap.resize(codeBlock->numberOfSwitchJumpTables());

        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i) {
            UniquedStringImpl* rep = codeBlock->identifier(i).impl();
            unsigned index = byteCodeParser->m_graph.identifiers().ensure(rep);
            m_identifierRemap[i] = index;
        }
        for (unsigned i = 0; i < codeBlock->numberOfSwitchJumpTables(); ++i) {
            m_switchRemap[i] = byteCodeParser->m_codeBlock->numberOfSwitchJumpTables();
            byteCodeParser->m_codeBlock->addSwitchJumpTable() = codeBlock->switchJumpTable(i);
        }
    } else {
        // Machine code block case.
        ASSERT(codeBlock == byteCodeParser->m_codeBlock);
        ASSERT(!callee);
        ASSERT(!returnValueVR.isValid());
        ASSERT(!inlineCallFrameStart.isValid());

        m_inlineCallFrame = 0;

        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        m_switchRemap.resize(codeBlock->numberOfSwitchJumpTables());
        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i)
            m_identifierRemap[i] = i;
        for (size_t i = 0; i < codeBlock->numberOfSwitchJumpTables(); ++i)
            m_switchRemap[i] = i;
    }
    
    m_argumentPositions.resize(argumentCountIncludingThisWithFixup);
    for (int i = 0; i < argumentCountIncludingThisWithFixup; ++i) {
        byteCodeParser->m_graph.m_argumentPositions.append(ArgumentPosition());
        ArgumentPosition* argumentPosition = &byteCodeParser->m_graph.m_argumentPositions.last();
        m_argumentPositions[i] = argumentPosition;
    }
    byteCodeParser->m_inlineCallFrameToArgumentPositions.add(m_inlineCallFrame, m_argumentPositions);
    
    byteCodeParser->m_inlineStackTop = this;
}

ByteCodeParser::InlineStackEntry::~InlineStackEntry()
{
    m_byteCodeParser->m_inlineStackTop = m_caller;
    RELEASE_ASSERT(m_byteCodeParser->m_icContextStack.last() == &m_optimizedContext);
    m_byteCodeParser->m_icContextStack.removeLast();
}

void ByteCodeParser::parseCodeBlock()
{
    clearCaches();
    
    CodeBlock* codeBlock = m_inlineStackTop->m_codeBlock;
    
    if (UNLIKELY(m_graph.compilation())) {
        m_graph.compilation()->addProfiledBytecodes(
            *m_vm->m_perBytecodeProfiler, m_inlineStackTop->m_profiledBlock);
    }
    
    if (UNLIKELY(Options::dumpSourceAtDFGTime())) {
        Vector<DeferredSourceDump>& deferredSourceDump = m_graph.m_plan.callback()->ensureDeferredSourceDump();
        if (inlineCallFrame()) {
            DeferredSourceDump dump(codeBlock->baselineVersion(), m_codeBlock, JITCode::DFGJIT, inlineCallFrame()->directCaller.bytecodeIndex);
            deferredSourceDump.append(dump);
        } else
            deferredSourceDump.append(DeferredSourceDump(codeBlock->baselineVersion()));
    }

    if (Options::dumpBytecodeAtDFGTime()) {
        dataLog("Parsing ", *codeBlock);
        if (inlineCallFrame()) {
            dataLog(
                " for inlining at ", CodeBlockWithJITType(m_codeBlock, JITCode::DFGJIT),
                " ", inlineCallFrame()->directCaller);
        }
        dataLog(
            ", isStrictMode = ", codeBlock->ownerScriptExecutable()->isStrictMode(), "\n");
        codeBlock->baselineVersion()->dumpBytecode();
    }
    
    Vector<InstructionStream::Offset, 32> jumpTargets;
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
            // There may already be a currentBlock in two cases:
            // - we may have just entered the loop for the first time
            // - we may have just returned from an inlined callee that had some early returns and
            //   so allocated a continuation block, and the instruction after the call is a jump target.
            // In both cases, we want to keep using it.
            if (!m_currentBlock) {
                m_currentBlock = allocateTargetableBlock(m_currentIndex);

                // The first block is definitely an OSR target.
                if (m_graph.numBlocks() == 1) {
                    m_currentBlock->isOSRTarget = true;
                    m_graph.m_roots.append(m_currentBlock);
                }
                prepareToParseBlock();
            }

            parseBlock(limit);

            // We should not have gone beyond the limit.
            ASSERT(m_currentIndex <= limit);

            if (m_currentBlock->isEmpty()) {
                // This case only happens if the last instruction was an inlined call with early returns
                // or polymorphic (creating an empty continuation block),
                // and then we hit the limit before putting anything in the continuation block.
                ASSERT(m_currentIndex == limit);
                makeBlockTargetable(m_currentBlock, m_currentIndex);
            } else {
                ASSERT(m_currentBlock->terminal() || (m_currentIndex == codeBlock->instructions().size() && inlineCallFrame()));
                m_currentBlock = nullptr;
            }
        } while (m_currentIndex < limit);
    }

    // Should have reached the end of the instructions.
    ASSERT(m_currentIndex == codeBlock->instructions().size());
    
    VERBOSE_LOG("Done parsing ", *codeBlock, " (fell off end)\n");
}

template <typename Bytecode>
void ByteCodeParser::handlePutByVal(Bytecode bytecode, unsigned instructionSize)
{
    Node* base = get(bytecode.base);
    Node* property = get(bytecode.property);
    Node* value = get(bytecode.value);
    bool isDirect = Bytecode::opcodeID == op_put_by_val_direct;
    bool compiledAsPutById = false;
    {
        unsigned identifierNumber = std::numeric_limits<unsigned>::max();
        PutByIdStatus putByIdStatus;
        {
            ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
            ByValInfo* byValInfo = m_inlineStackTop->m_baselineMap.get(CodeOrigin(currentCodeOrigin().bytecodeIndex)).byValInfo;
            // FIXME: When the bytecode is not compiled in the baseline JIT, byValInfo becomes null.
            // At that time, there is no information.
            if (byValInfo 
                && byValInfo->stubInfo
                && !byValInfo->tookSlowPath
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCell)) {
                compiledAsPutById = true;
                identifierNumber = m_graph.identifiers().ensure(byValInfo->cachedId.impl());
                UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];

                if (Symbol* symbol = byValInfo->cachedSymbol.get()) {
                    FrozenValue* frozen = m_graph.freezeStrong(symbol);
                    addToGraph(CheckCell, OpInfo(frozen), property);
                } else {
                    ASSERT(!uid->isSymbol());
                    addToGraph(CheckStringIdent, OpInfo(uid), property);
                }

                putByIdStatus = PutByIdStatus::computeForStubInfo(
                    locker, m_inlineStackTop->m_profiledBlock,
                    byValInfo->stubInfo, currentCodeOrigin(), uid);

            }
        }

        if (compiledAsPutById)
            handlePutById(base, identifierNumber, value, putByIdStatus, isDirect, instructionSize);
    }

    if (!compiledAsPutById) {
        ArrayMode arrayMode = getArrayMode(bytecode.metadata(m_inlineStackTop->m_codeBlock).arrayProfile, Array::Write);

        addVarArgChild(base);
        addVarArgChild(property);
        addVarArgChild(value);
        addVarArgChild(0); // Leave room for property storage.
        addVarArgChild(0); // Leave room for length.
        addToGraph(Node::VarArg, isDirect ? PutByValDirect : PutByVal, OpInfo(arrayMode.asWord()), OpInfo(0));
    }
}

template <typename Bytecode>
void ByteCodeParser::handlePutAccessorById(NodeType op, Bytecode bytecode)
{
    Node* base = get(bytecode.base);
    unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.property];
    Node* accessor = get(bytecode.accessor);
    addToGraph(op, OpInfo(identifierNumber), OpInfo(bytecode.attributes), base, accessor);
}

template <typename Bytecode>
void ByteCodeParser::handlePutAccessorByVal(NodeType op, Bytecode bytecode)
{
    Node* base = get(bytecode.base);
    Node* subscript = get(bytecode.property);
    Node* accessor = get(bytecode.accessor);
    addToGraph(op, OpInfo(bytecode.attributes), base, subscript, accessor);
}

template <typename Bytecode>
void ByteCodeParser::handleNewFunc(NodeType op, Bytecode bytecode)
{
    FunctionExecutable* decl = m_inlineStackTop->m_profiledBlock->functionDecl(bytecode.functionDecl);
    FrozenValue* frozen = m_graph.freezeStrong(decl);
    Node* scope = get(bytecode.scope);
    set(bytecode.dst, addToGraph(op, OpInfo(frozen), scope));
    // Ideally we wouldn't have to do this Phantom. But:
    //
    // For the constant case: we must do it because otherwise we would have no way of knowing
    // that the scope is live at OSR here.
    //
    // For the non-constant case: NewFunction could be DCE'd, but baseline's implementation
    // won't be able to handle an Undefined scope.
    addToGraph(Phantom, scope);
}

template <typename Bytecode>
void ByteCodeParser::handleNewFuncExp(NodeType op, Bytecode bytecode)
{
    FunctionExecutable* expr = m_inlineStackTop->m_profiledBlock->functionExpr(bytecode.functionDecl);
    FrozenValue* frozen = m_graph.freezeStrong(expr);
    Node* scope = get(bytecode.scope);
    set(bytecode.dst, addToGraph(op, OpInfo(frozen), scope));
    // Ideally we wouldn't have to do this Phantom. But:
    //
    // For the constant case: we must do it because otherwise we would have no way of knowing
    // that the scope is live at OSR here.
    //
    // For the non-constant case: NewFunction could be DCE'd, but baseline's implementation
    // won't be able to handle an Undefined scope.
    addToGraph(Phantom, scope);
}

void ByteCodeParser::parse()
{
    // Set during construction.
    ASSERT(!m_currentIndex);
    
    VERBOSE_LOG("Parsing ", *m_codeBlock, "\n");
    
    InlineStackEntry inlineStackEntry(
        this, m_codeBlock, m_profiledBlock, 0, VirtualRegister(), VirtualRegister(),
        m_codeBlock->numParameters(), InlineCallFrame::Call, nullptr);
    
    parseCodeBlock();
    linkBlocks(inlineStackEntry.m_unlinkedBlocks, inlineStackEntry.m_blockLinkingTargets);

    if (m_hasAnyForceOSRExits) {
        BlockSet blocksToIgnore;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            if (block->isOSRTarget && block->bytecodeBegin == m_graph.m_plan.osrEntryBytecodeIndex()) {
                blocksToIgnore.add(block);
                break;
            }
        }

        {
            bool isSafeToValidate = false;
            auto postOrder = m_graph.blocksInPostOrder(isSafeToValidate); // This algorithm doesn't rely on the predecessors list, which is not yet built.
            bool changed;
            do {
                changed = false;
                for (BasicBlock* block : postOrder) {
                    for (BasicBlock* successor : block->successors()) {
                        if (blocksToIgnore.contains(successor)) {
                            changed |= blocksToIgnore.add(block);
                            break;
                        }
                    }
                }
            } while (changed);
        }

        InsertionSet insertionSet(m_graph);
        Operands<VariableAccessData*> mapping(OperandsLike, m_graph.block(0)->variablesAtHead);

        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            if (blocksToIgnore.contains(block))
                continue;

            mapping.fill(nullptr);
            if (validationEnabled()) {
                // Verify that it's correct to fill mapping with nullptr.
                for (unsigned i = 0; i < block->variablesAtHead.size(); ++i) {
                    Node* node = block->variablesAtHead.at(i);
                    RELEASE_ASSERT(!node);
                }
            }

            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);

                if (node->hasVariableAccessData(m_graph))
                    mapping.operand(node->local()) = node->variableAccessData();

                if (node->op() == ForceOSRExit) {
                    NodeOrigin endOrigin = node->origin.withExitOK(true);

                    if (validationEnabled()) {
                        // This verifies that we don't need to change any of the successors's predecessor
                        // list after planting the Unreachable below. At this point in the bytecode
                        // parser, we haven't linked up the predecessor lists yet.
                        for (BasicBlock* successor : block->successors())
                            RELEASE_ASSERT(successor->predecessors.isEmpty());
                    }

                    block->resize(nodeIndex + 1);

                    insertionSet.insertNode(block->size(), SpecNone, ExitOK, endOrigin);

                    auto insertLivenessPreservingOp = [&] (InlineCallFrame* inlineCallFrame, NodeType op, VirtualRegister operand) {
                        VariableAccessData* variable = mapping.operand(operand);
                        if (!variable) {
                            variable = newVariableAccessData(operand);
                            mapping.operand(operand) = variable;
                        }

                        VirtualRegister argument = operand - (inlineCallFrame ? inlineCallFrame->stackOffset : 0);
                        if (argument.isArgument() && !argument.isHeader()) {
                            const Vector<ArgumentPosition*>& arguments = m_inlineCallFrameToArgumentPositions.get(inlineCallFrame);
                            arguments[argument.toArgument()]->addVariable(variable);
                        } insertionSet.insertNode(block->size(), SpecNone, op, endOrigin, OpInfo(variable));
                    };
                    auto addFlushDirect = [&] (InlineCallFrame* inlineCallFrame, VirtualRegister operand) {
                        insertLivenessPreservingOp(inlineCallFrame, Flush, operand);
                    };
                    auto addPhantomLocalDirect = [&] (InlineCallFrame* inlineCallFrame, VirtualRegister operand) {
                        insertLivenessPreservingOp(inlineCallFrame, PhantomLocal, operand);
                    };
                    flushForTerminalImpl(endOrigin.semantic, addFlushDirect, addPhantomLocalDirect);

                    insertionSet.insertNode(block->size(), SpecNone, Unreachable, endOrigin);
                    insertionSet.execute(block);
                    break;
                }
            }
        }
    } else if (validationEnabled()) {
        // Ensure our bookkeeping for ForceOSRExit nodes is working.
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block)
                RELEASE_ASSERT(node->op() != ForceOSRExit);
        }
    }
    
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
}

void parse(Graph& graph)
{
    ByteCodeParser(graph).parse();
}

} } // namespace JSC::DFG

#endif
