/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#include "ArrayPrototype.h"
#include "BooleanConstructor.h"
#include "BuiltinNames.h"
#include "BytecodeGenerator.h"
#include "BytecodeOperandsForCheckpoint.h"
#include "CacheableIdentifierInlines.h"
#include "CallLinkStatus.h"
#include "CheckPrivateBrandStatus.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "CommonSlowPaths.h"
#include "DFGAbstractHeap.h"
#include "DFGArrayMode.h"
#include "DFGBackwardsPropagationPhase.h"
#include "DFGBlockSet.h"
#include "DFGCapabilities.h"
#include "DFGClobberize.h"
#include "DFGClobbersExitState.h"
#include "DFGGraph.h"
#include "DOMJITGetterSetter.h"
#include "DeleteByStatus.h"
#include "FunctionCodeBlock.h"
#include "GetByStatus.h"
#include "GetterSetter.h"
#include "Heap.h"
#include "InByStatus.h"
#include "InstanceOfStatus.h"
#include "JSArrayIterator.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSMapIterator.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSPromiseConstructor.h"
#include "JSSetIterator.h"
#include "NullSetterFunction.h"
#include "NumberConstructor.h"
#include "ObjectConstructor.h"
#include "OpcodeInlines.h"
#include "PreciseJumpTargets.h"
#include "PrivateFieldPutKind.h"
#include "PutByIdFlags.h"
#include "PutByStatus.h"
#include "RegExpPrototype.h"
#include "SetPrivateBrandStatus.h"
#include "StackAlignment.h"
#include "StringConstructor.h"
#include "StructureStubInfo.h"
#include "SymbolConstructor.h"
#include <wtf/CommaPrinter.h>
#include <wtf/HashMap.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace DFG {

namespace DFGByteCodeParserInternal {
#ifdef NDEBUG
static constexpr bool verbose = false;
#else
static constexpr bool verbose = true;
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
        , m_currentBlock(nullptr)
        , m_currentIndex(0)
        , m_constantUndefined(graph.freeze(jsUndefined()))
        , m_constantNull(graph.freeze(jsNull()))
        , m_constantNaN(graph.freeze(jsNumber(PNaN)))
        , m_constantOne(graph.freeze(jsNumber(1)))
        , m_numArguments(m_codeBlock->numParameters())
        , m_numLocals(m_codeBlock->numCalleeLocals())
        , m_numTmps(m_codeBlock->numTmps())
        , m_parameterSlots(0)
        , m_numPassedVarArgs(0)
        , m_inlineStackTop(nullptr)
        , m_currentInstruction(nullptr)
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

    void ensureTmps(unsigned newNumTmps)
    {
        VERBOSE_LOG("   ensureTmps: trying to raise m_numTmps from ", m_numTmps, " to ", newNumTmps, "\n");
        if (newNumTmps <= m_numTmps)
            return;
        m_numTmps = newNumTmps;
        for (size_t i = 0; i < m_graph.numBlocks(); ++i)
            m_graph.block(i)->ensureTmps(newNumTmps);
    }


    // Helper for min and max.
    template<typename ChecksFunctor>
    void handleMinMax(Operand result, NodeType op, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& insertChecks);
    
    void refineStatically(CallLinkStatus&, Node* callTarget);
    // Blocks can either be targetable (i.e. in the m_blockLinkingTargets of one InlineStackEntry) with a well-defined bytecodeBegin,
    // or they can be untargetable, with bytecodeBegin==UINT_MAX, to be managed manually and not by the linkBlock machinery.
    // This is used most notably when doing polyvariant inlining (it requires a fair bit of control-flow with no bytecode analog).
    // It is also used when doing an early return from an inlined callee: it is easier to fix the bytecode index later on if needed
    // than to move the right index all the way to the treatment of op_ret.
    BasicBlock* allocateTargetableBlock(BytecodeIndex);
    BasicBlock* allocateUntargetableBlock();
    // An untargetable block can be given a bytecodeIndex to be later managed by linkBlock, but only once, and it can never go in the other direction
    void makeBlockTargetable(BasicBlock*, BytecodeIndex);
    void addJumpTo(BasicBlock*);
    void addJumpTo(unsigned bytecodeIndex);
    // Handle calls. This resolves issues surrounding inlining and intrinsics.
    enum Terminality { Terminal, NonTerminal };
    Terminality handleCall(
        Operand result, NodeType op, InlineCallFrame::Kind, BytecodeIndex osrExitIndex,
        Node* callTarget, int argumentCountIncludingThis, int registerOffset, CallLinkStatus,
        SpeculatedType prediction, ECMAMode = ECMAMode::strict());
    template<typename CallOp>
    Terminality handleCall(const JSInstruction* pc, NodeType op, CallMode, BytecodeIndex osrExitIndex);
    template<typename CallOp>
    Terminality handleVarargsCall(const JSInstruction* pc, NodeType op, CallMode);
    void emitFunctionChecks(CallVariant, Node* callTarget, VirtualRegister thisArgumnt);
    void emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis);
    Node* getArgumentCount();
    template<typename ChecksFunctor>
    bool handleRecursiveTailCall(Node* callTargetNode, CallVariant, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& emitFunctionCheckIfNeeded);
    unsigned inliningCost(CallVariant, int argumentCountIncludingThis, InlineCallFrame::Kind); // Return UINT_MAX if it's not an inlining candidate. By convention, intrinsics have a cost of 1.
    // Handle inlining. Return true if it succeeded, false if we need to plant a call.
    bool handleVarargsInlining(Node* callTargetNode, Operand result, const CallLinkStatus&, int registerOffset, VirtualRegister thisArgument, VirtualRegister argumentsArgument, unsigned argumentsOffset, NodeType callOp, InlineCallFrame::Kind);
    unsigned getInliningBalance(const CallLinkStatus&, CodeSpecializationKind);
    enum class CallOptimizationResult { OptimizedToJump, Inlined, DidNothing };
    CallOptimizationResult handleCallVariant(Node* callTargetNode, Operand result, CallVariant, int registerOffset, VirtualRegister thisArgument, int argumentCountIncludingThis, BytecodeIndex osrExitIndex, InlineCallFrame::Kind, SpeculatedType prediction, unsigned& inliningBalance, BasicBlock* continuationBlock, bool needsToCheckCallee);
    CallOptimizationResult handleInlining(Node* callTargetNode, Operand result, const CallLinkStatus&, int registerOffset, VirtualRegister thisArgument, int argumentCountIncludingThis, BytecodeIndex osrExitIndex, NodeType callOp, InlineCallFrame::Kind, SpeculatedType prediction, ECMAMode);
    template<typename ChecksFunctor>
    void inlineCall(Node* callTargetNode, Operand result, CallVariant, int registerOffset, int argumentCountIncludingThis, InlineCallFrame::Kind, BasicBlock* continuationBlock, const ChecksFunctor& insertChecks);
    // Handle intrinsic functions. Return true if it succeeded, false if we need to plant a call.
    template<typename ChecksFunctor>
    bool handleIntrinsicCall(Node* callee, Operand result, CallVariant, Intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleDOMJITCall(Node* callee, Operand result, const DOMJIT::Signature*, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleIntrinsicGetter(Operand result, SpeculatedType prediction, const GetByVariant& intrinsicVariant, Node* thisNode, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleTypedArrayConstructor(Operand result, InternalFunction*, int registerOffset, int argumentCountIncludingThis, TypedArrayType, const ChecksFunctor& insertChecks);
    template<typename ChecksFunctor>
    bool handleConstantInternalFunction(Node* callTargetNode, Operand result, InternalFunction*, int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind, SpeculatedType, const ChecksFunctor& insertChecks);
    Node* handlePutByOffset(Node* base, unsigned identifier, PropertyOffset, Node* value);
    Node* handleGetByOffset(SpeculatedType, Node* base, unsigned identifierNumber, PropertyOffset, NodeType = GetByOffset);
    bool handleDOMJITGetter(Operand result, const GetByVariant&, Node* thisNode, unsigned identifierNumber, SpeculatedType prediction);
    bool handleModuleNamespaceLoad(VirtualRegister result, SpeculatedType, Node* base, GetByStatus);

    template<typename Bytecode>
    void handlePutByVal(Bytecode, BytecodeIndex osrExitIndex);
    template <typename Bytecode>
    void handlePutAccessorById(NodeType, Bytecode);
    template <typename Bytecode>
    void handlePutAccessorByVal(NodeType, Bytecode);
    template <typename Bytecode>
    void handleNewFunc(NodeType, Bytecode);
    template <typename Bytecode>
    void handleNewFuncExp(NodeType, Bytecode);
    template <typename Bytecode>
    void handleCreateInternalFieldObject(const ClassInfo*, NodeType createOp, NodeType newOp, Bytecode);

    // Create a presence ObjectPropertyCondition based on some known offset and structure set. Does not
    // check the validity of the condition, but it may return a null one if it encounters a contradiction.
    ObjectPropertyCondition presenceConditionIfConsistent(JSObject* knownBase, UniquedStringImpl*, PropertyOffset, const StructureSet&);
    
    // Attempt to watch the presence/replacement of a property. It will watch that the property is present in the same
    // way as in all of the structures in the set (or the base if it's a constant). It may emit code instead of just setting a watchpoint.
    // Returns the condition if one is found/watched.
    void checkReplacement(Node* base, UniquedStringImpl*, PropertyOffset, const StructureSet&);
    
    // Works with both GetByVariant and the setter form of PutByVariant.
    template<typename VariantType>
    Node* load(SpeculatedType, Node* base, unsigned identifierNumber, const VariantType&);

    Node* replace(Node* base, unsigned identifier, const PutByVariant&, Node* value);

    template<typename Op>
    void parseGetById(const JSInstruction*);
    void simplifyGetByStatus(Node* base, GetByStatus&);
    void handleGetById(
        VirtualRegister destination, SpeculatedType, Node* base, CacheableIdentifier, unsigned identifierNumber, GetByStatus, AccessType, BytecodeIndex osrExitIndex);
    void handleGetPrivateNameById(
        VirtualRegister destination, SpeculatedType prediction, Node* base, CacheableIdentifier, unsigned identifierNumber, GetByStatus);
    void emitPutById(
        Node* base, CacheableIdentifier, Node* value,  const PutByStatus&, bool isDirect, ECMAMode);
    void handlePutById(
        Node* base, CacheableIdentifier, unsigned identifierNumber, Node* value, const PutByStatus&,
        bool isDirect, BytecodeIndex osrExitIndex, ECMAMode);

    void handlePutPrivateNameById(
        Node* base, CacheableIdentifier, unsigned identifierNumber, Node* value, const PutByStatus&, PrivateFieldPutKind);

    void handleDeleteById(
        VirtualRegister destination, Node* base, CacheableIdentifier, unsigned identifierNumber, DeleteByStatus, ECMAMode);

    bool handleInByAsMatchStructure(VirtualRegister destination, Node* base, InByStatus);
    void handleInById(VirtualRegister destination, Node* base, CacheableIdentifier, InByStatus);

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
    
    BytecodeIndex nextOpcodeIndex() const { return BytecodeIndex(m_currentIndex.offset() + m_currentInstruction->size()); }
    BytecodeIndex nextCheckpoint() const { return m_currentIndex.withCheckpoint(m_currentIndex.checkpoint() + 1); }

    void progressToNextCheckpoint()
    {
        m_currentIndex = nextCheckpoint();
        // At this point, it's again OK to OSR exit.
        m_exitOK = true;
        processSetLocalQueue();
    }

    VariableAccessData* newVariableAccessData(Operand operand)
    {
        ASSERT(!operand.isConstant());
        
        m_graph.m_variableAccessData.append(operand);
        return &m_graph.m_variableAccessData.last();
    }
    
    // Get/Set the operands/result of a bytecode instruction.
    Node* getDirect(Operand operand)
    {
        ASSERT(!operand.isConstant());

        if (operand.isArgument())
            return getArgument(operand.virtualRegister());

        return getLocalOrTmp(operand);
    }

    Node* get(Operand operand)
    {
        if (operand.isConstant()) {
            unsigned constantIndex = operand.virtualRegister().toConstantIndex();
            unsigned oldSize = m_constants.size();
            if (constantIndex >= oldSize || !m_constants[constantIndex]) {
                const CodeBlock& codeBlock = *m_inlineStackTop->m_codeBlock;
                JSValue value = codeBlock.getConstant(operand.virtualRegister());
                SourceCodeRepresentation sourceCodeRepresentation = codeBlock.constantSourceCodeRepresentation(operand.virtualRegister());
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
                if (operand == VirtualRegister(CallFrameSlot::callee))
                    return weakJSConstant(callee);
            }
        } else if (operand == VirtualRegister(CallFrameSlot::callee)) {
            // We have to do some constant-folding here because this enables CreateThis folding. Note
            // that we don't have such watchpoint-based folding for inlined uses of Callee, since in that
            // case if the function is a singleton then we already know it.
            if (FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(m_codeBlock->ownerExecutable())) {
                if (JSFunction* function = executable->singleton().inferredValue()) {
                    m_graph.watchpoints().addLazily(executable);
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

    Node* setDirect(Operand operand, Node* value, SetMode setMode = NormalSet)
    {
        addToGraph(MovHint, OpInfo(operand), value);

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

    Node* set(Operand operand, Node* value, SetMode setMode = NormalSet)
    {
        return setDirect(m_inlineStackTop->remapOperand(operand), value, setMode);
    }
    
    Node* injectLazyOperandSpeculation(Node* node)
    {
        ASSERT(node->op() == GetLocal);
        ASSERT(node->origin.semantic.bytecodeIndex() == m_currentIndex);
        ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        LazyOperandValueProfileKey key(m_currentIndex, node->operand());
        SpeculatedType prediction = m_inlineStackTop->m_lazyOperands.prediction(locker, key);
        node->variableAccessData()->predict(prediction);
        return node;
    }

    // Used in implementing get/set, above, where the operand is a local variable.
    Node* getLocalOrTmp(Operand operand)
    {
        ASSERT(operand.isTmp() || operand.isLocal());
        Node*& node = m_currentBlock->variablesAtTail.operand(operand);
        
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
        return node;
    }
    Node* setLocalOrTmp(const CodeOrigin& semanticOrigin, Operand operand, Node* value, SetMode setMode = NormalSet)
    {
        ASSERT(operand.isTmp() || operand.isLocal());
        SetForScope originChange(m_currentSemanticOrigin, semanticOrigin);

        if (operand.isTmp() && static_cast<unsigned>(operand.value()) >= m_numTmps) {
            if (inlineCallFrame())
                dataLogLn(*inlineCallFrame());
            dataLogLn("Bad operand: ", operand, " but current number of tmps is: ", m_numTmps, " code block has: ", m_profiledBlock->numTmps(), " tmps.");
            CRASH();
        }

        if (setMode != ImmediateNakedSet && !operand.isTmp()) {
            VirtualRegister reg = operand.virtualRegister();
            ArgumentPosition* argumentPosition = findArgumentPositionForLocal(reg);
            if (argumentPosition)
                flushDirect(operand, argumentPosition);
            else if (m_graph.needsScopeRegister() && reg == m_codeBlock->scopeRegister())
                flush(operand);
        }

        VariableAccessData* variableAccessData = newVariableAccessData(operand);
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex(), BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex(), BadIndexingType));
        Node* node = addToGraph(SetLocal, OpInfo(variableAccessData), value);
        m_currentBlock->variablesAtTail.operand(operand) = node;
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
    Node* setArgument(const CodeOrigin& semanticOrigin, Operand operand, Node* value, SetMode setMode = NormalSet)
    {
        SetForScope originChange(m_currentSemanticOrigin, semanticOrigin);

        VirtualRegister reg = operand.virtualRegister();
        unsigned argument = reg.toArgument();
        ASSERT(argument < m_numArguments);
        
        VariableAccessData* variableAccessData = newVariableAccessData(reg);

        // Always flush arguments, except for 'this'. If 'this' is created by us,
        // then make sure that it's never unboxed.
        if (argument) {
            if (setMode != ImmediateNakedSet)
                flushDirect(reg);
        } else if (!argument) {
            if (setMode != ImmediateNakedSet)
                phantomLocalDirect(reg);
        }
        
        if (!argument && m_codeBlock->specializationKind() == CodeForConstruct)
            variableAccessData->mergeShouldNeverUnbox(true);
        
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex(), BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(semanticOrigin.bytecodeIndex(), BadIndexingType));
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
            if (operand.offset() >= static_cast<int>(inlineCallFrame->stackOffset + CallFrame::thisArgumentOffset() + inlineCallFrame->m_argumentsWithFixup.size()))
                continue;
            int argument = VirtualRegister(operand.offset() - inlineCallFrame->stackOffset).toArgument();
            return stack->m_argumentPositions[argument];
        }
        return nullptr;
    }
    
    ArgumentPosition* findArgumentPosition(Operand operand)
    {
        if (operand.isTmp())
            return nullptr;
        if (operand.isArgument())
            return findArgumentPositionForArgument(operand.toArgument());
        return findArgumentPositionForLocal(operand.virtualRegister());
    }

    template<typename AddFlushDirectFunc>
    void flushImpl(InlineCallFrame* inlineCallFrame, const AddFlushDirectFunc& addFlushDirect)
    {
        int numArguments;
        if (inlineCallFrame) {
            ASSERT(!m_graph.hasDebuggerEnabled());
            numArguments = inlineCallFrame->m_argumentsWithFixup.size();
            if (inlineCallFrame->isClosureCall)
                addFlushDirect(inlineCallFrame, remapOperand(inlineCallFrame, CallFrameSlot::callee));
            if (inlineCallFrame->isVarargs())
                addFlushDirect(inlineCallFrame, remapOperand(inlineCallFrame, CallFrameSlot::argumentCountIncludingThis));
        } else
            numArguments = m_graph.baselineCodeBlockFor(inlineCallFrame)->numParameters();

        for (unsigned argument = numArguments; argument--;)
            addFlushDirect(inlineCallFrame, remapOperand(inlineCallFrame, virtualRegisterForArgumentIncludingThis(argument)));

        if (m_graph.needsScopeRegister())
            addFlushDirect(nullptr, m_graph.m_codeBlock->scopeRegister());
    }

    template<typename AddFlushDirectFunc, typename AddPhantomLocalDirectFunc>
    void flushForTerminalImpl(CodeOrigin origin, const AddFlushDirectFunc& addFlushDirect, const AddPhantomLocalDirectFunc& addPhantomLocalDirect)
    {
        bool isCallerOrigin = false;
        origin.walkUpInlineStack(
            [&] (CodeOrigin origin) {
                BytecodeIndex bytecodeIndex = origin.bytecodeIndex();
                InlineCallFrame* inlineCallFrame = origin.inlineCallFrame();
                flushImpl(inlineCallFrame, addFlushDirect);

                CodeBlock* codeBlock = m_graph.baselineCodeBlockFor(inlineCallFrame);
                FullBytecodeLiveness& fullLiveness = m_graph.livenessFor(codeBlock);
                // Note: We don't need to handle tmps here because tmps are not required to be flushed to the stack.
                const auto& livenessAtBytecode = fullLiveness.getLiveness(bytecodeIndex, m_graph.appropriateLivenessCalculationPoint(origin, isCallerOrigin));
                for (unsigned local = codeBlock->numCalleeLocals(); local--;) {
                    if (livenessAtBytecode[local])
                        addPhantomLocalDirect(inlineCallFrame, remapOperand(inlineCallFrame, virtualRegisterForLocal(local)));
                }
                if (bytecodeIndex.checkpoint()) {
                    ASSERT(codeBlock->numTmps());
                    auto liveTmps = tmpLivenessForCheckpoint(*codeBlock, bytecodeIndex);
                    liveTmps.forEachSetBit([&](size_t tmp) {
                        addPhantomLocalDirect(inlineCallFrame, remapOperand(inlineCallFrame, Operand::tmp(tmp)));
                    });
                }
                isCallerOrigin = true;
            });
    }

    void flush(Operand operand)
    {
        flushDirect(m_inlineStackTop->remapOperand(operand));
    }
    
    void flushDirect(Operand operand)
    {
        flushDirect(operand, findArgumentPosition(operand));
    }

    void flushDirect(Operand operand, ArgumentPosition* argumentPosition)
    {
        addFlushOrPhantomLocal<Flush>(operand, argumentPosition);
    }

    template<NodeType nodeType>
    void addFlushOrPhantomLocal(Operand operand, ArgumentPosition* argumentPosition)
    {
        ASSERT(!operand.isConstant());
        
        Node*& node = m_currentBlock->variablesAtTail.operand(operand);
        
        VariableAccessData* variable;
        
        if (node)
            variable = node->variableAccessData();
        else
            variable = newVariableAccessData(operand);
        
        node = addToGraph(nodeType, OpInfo(variable));
        if (argumentPosition)
            argumentPosition->addVariable(variable);
    }

    void phantomLocalDirect(Operand operand)
    {
        addFlushOrPhantomLocal<PhantomLocal>(operand, findArgumentPosition(operand));
    }

    void flush(InlineStackEntry* inlineStackEntry)
    {
        auto addFlushDirect = [&] (InlineCallFrame*, Operand operand) { flushDirect(operand); };
        flushImpl(inlineStackEntry->m_inlineCallFrame, addFlushDirect);
    }

    void flushForTerminal()
    {
        auto addFlushDirect = [&] (InlineCallFrame*, Operand operand) { flushDirect(operand); };
        auto addPhantomLocalDirect = [&] (InlineCallFrame*, Operand operand) { phantomLocalDirect(operand); };
        flushForTerminalImpl(currentCodeOrigin(), addFlushDirect, addPhantomLocalDirect);
    }

    void flushForReturn()
    {
        flush(m_inlineStackTop);
    }
    
    void flushIfTerminal(SwitchData& data)
    {
        if (data.fallThrough.bytecodeIndex() > m_currentIndex.offset())
            return;
        
        for (unsigned i = data.cases.size(); i--;) {
            if (data.cases[i].target.bytecodeIndex() > m_currentIndex.offset())
                return;
        }
        
        flushForTerminal();
    }

    Node* jsConstant(FrozenValue* constantValue)
    {
        return addToGraph(JSConstant, OpInfo(constantValue));
    }

    // Assumes that the constant should be strongly marked.
    Node* jsConstant(JSValue constantValue)
    {
        return jsConstant(m_graph.freezeStrong(constantValue));
    }

    Node* weakJSConstant(JSValue constantValue)
    {
        return jsConstant(m_graph.freeze(constantValue));
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
        CodeOrigin semantic = m_currentSemanticOrigin.isSet() ? m_currentSemanticOrigin : currentCodeOrigin();
        CodeOrigin forExit = m_currentExitOrigin.isSet() ? m_currentExitOrigin : currentCodeOrigin();

        return NodeOrigin(semantic, forExit, m_exitOK);
    }
    
    BranchData* branchData(unsigned taken, unsigned notTaken)
    {
        // We assume that branches originating from bytecode always have a fall-through. We
        // use this assumption to avoid checking for the creation of terminal blocks.
        ASSERT((taken > m_currentIndex.offset()) || (notTaken > m_currentIndex.offset()));
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
    
    Node* addToGraph(NodeType op, Node* child1 = nullptr, Node* child2 = nullptr, Node* child3 = nullptr)
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
    Node* addToGraph(NodeType op, OpInfo info, Node* child1 = nullptr, Node* child2 = nullptr, Node* child3 = nullptr)
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
    Node* addToGraph(NodeType op, OpInfo info1, OpInfo info2, Node* child1 = nullptr, Node* child2 = nullptr, Node* child3 = nullptr)
    {
        Node* result = m_graph.addNode(
            op, currentNodeOrigin(), info1, info2,
            Edge(child1), Edge(child2), Edge(child3));
        return addToGraph(result);
    }
    Node* addToGraph(NodeType op, Operand operand, Node* child1)
    {
        ASSERT(op == MovHint);
        return addToGraph(op, OpInfo(operand.kind()), OpInfo(operand.value()), child1);
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
        OpInfo prediction, Node* thisValueForEval = nullptr, Node* scopeForEval = nullptr)
    {
        addVarArgChild(callee);
        size_t parameterSlots = Graph::parameterSlotsForArgCount(argCount);

        if (parameterSlots > m_parameterSlots)
            m_parameterSlots = parameterSlots;

        for (int i = 0; i < argCount; ++i)
            addVarArgChild(get(virtualRegisterForArgumentIncludingThis(i, registerOffset)));
        if (op == CallDirectEval) {
            addVarArgChild(Edge(thisValueForEval));
            addVarArgChild(Edge(scopeForEval, KnownCellUse));
        }

        return addToGraph(Node::VarArg, op, opInfo, prediction);
    }
    
    Node* addCall(
        Operand result, NodeType op, OpInfo opInfo, Node* callee, int argCount, int registerOffset,
        SpeculatedType prediction, Node* thisValueForEval = nullptr, Node* scopeForEval = nullptr)
    {
        if (op == TailCall) {
            if (allInlineFramesAreTailCalls())
                return addCallWithoutSettingResult(op, opInfo, callee, argCount, registerOffset, OpInfo());
            op = TailCallInlinedCaller;
        }


        Node* call = addCallWithoutSettingResult(op, opInfo, callee, argCount, registerOffset, OpInfo(prediction), thisValueForEval, scopeForEval);
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
    
    SpeculatedType getPredictionWithoutOSRExit(BytecodeIndex bytecodeIndex)
    {
        auto getValueProfilePredictionFromForCodeBlockAndBytecodeOffset = [&] (CodeBlock* codeBlock, const CodeOrigin& codeOrigin)
        {
            SpeculatedType prediction;
            {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                prediction = codeBlock->valueProfilePredictionForBytecodeIndex(locker, codeOrigin.bytecodeIndex());
            }
            auto* fuzzerAgent = m_vm->fuzzerAgent();
            if (UNLIKELY(fuzzerAgent))
                return fuzzerAgent->getPrediction(codeBlock, codeOrigin, prediction) & SpecBytecodeTop;
            return prediction;
        };

        SpeculatedType prediction = getValueProfilePredictionFromForCodeBlockAndBytecodeOffset(m_inlineStackTop->m_profiledBlock, CodeOrigin(bytecodeIndex, inlineCallFrame()));
        if (prediction != SpecNone)
            return prediction;

        // If we have no information about the values this
        // node generates, we check if by any chance it is
        // a tail call opcode. In that case, we walk up the
        // inline frames to find a call higher in the call
        // chain and use its prediction. If we only have
        // inlined tail call frames, we use SpecFullTop
        // to avoid a spurious OSR exit.
        auto instruction = m_inlineStackTop->m_profiledBlock->instructions().at(bytecodeIndex.offset());
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
            while (stack->m_inlineCallFrame != codeOrigin->inlineCallFrame())
                stack = stack->m_caller;

            return getValueProfilePredictionFromForCodeBlockAndBytecodeOffset(stack->m_profiledBlock, *codeOrigin);
        }

        default:
            return SpecNone;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return SpecNone;
    }

    SpeculatedType getPrediction(BytecodeIndex bytecodeIndex)
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
        ConcurrentJSLocker locker(codeBlock->m_lock);
        ArrayProfile* profile = codeBlock->getArrayProfile(locker, codeBlock->bytecodeIndex(m_currentInstruction));
        return getArrayMode(locker, *profile, action);
    }

    ArrayMode getArrayMode(ArrayProfile& profile, Array::Action action)
    {
        ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        return getArrayMode(locker, profile, action);
    }

    ArrayMode getArrayMode(const ConcurrentJSLocker& locker, ArrayProfile& profile, Array::Action action)
    {
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
        
        if (!isX86() && (node->op() == ArithMod || node->op() == ValueMod))
            return node;

        switch (node->op()) {
        case ArithAdd:
        case ArithSub:
        case ValueAdd: {
            ObservedResults observed;
            if (BinaryArithProfile* arithProfile = m_inlineStackTop->m_profiledBlock->binaryArithProfileForBytecodeIndex(m_currentIndex))
                observed = arithProfile->observedResults();
            else if (UnaryArithProfile* arithProfile = m_inlineStackTop->m_profiledBlock->unaryArithProfileForBytecodeIndex(m_currentIndex)) {
                // Happens for OpInc/OpDec
                observed = arithProfile->observedResults();
            } else
                break;

            if (observed.didObserveDouble())
                node->mergeFlags(NodeMayHaveDoubleResult);
            if (observed.didObserveNonNumeric())
                node->mergeFlags(NodeMayHaveNonNumericResult);
            if (observed.didObserveBigInt32())
                node->mergeFlags(NodeMayHaveBigInt32Result);
            if (observed.didObserveHeapBigInt() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BigInt32Overflow))
                node->mergeFlags(NodeMayHaveHeapBigIntResult);
            break;
        }
        case ValueMul:
        case ArithMul: {
            BinaryArithProfile* arithProfile = m_inlineStackTop->m_profiledBlock->binaryArithProfileForBytecodeIndex(m_currentIndex);
            if (!arithProfile)
                break;
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
            if (arithProfile->didObserveBigInt32())
                node->mergeFlags(NodeMayHaveBigInt32Result);
            if (arithProfile->didObserveHeapBigInt() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BigInt32Overflow))
                node->mergeFlags(NodeMayHaveHeapBigIntResult);
            break;
        }
        case ValueNegate:
        case ArithNegate:
        case Inc:
        case Dec: {
            UnaryArithProfile* arithProfile = m_inlineStackTop->m_profiledBlock->unaryArithProfileForBytecodeIndex(m_currentIndex);
            if (!arithProfile)
                break;
            if (arithProfile->argObservedType().sawNumber() || arithProfile->didObserveDouble())
                node->mergeFlags(NodeMayHaveDoubleResult);
            if (arithProfile->didObserveNegZeroDouble() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
                node->mergeFlags(NodeMayNegZeroInBaseline);
            if (arithProfile->didObserveInt32Overflow() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
                node->mergeFlags(NodeMayOverflowInt32InBaseline);
            if (arithProfile->didObserveNonNumeric())
                node->mergeFlags(NodeMayHaveNonNumericResult);
            if (arithProfile->didObserveBigInt32())
                node->mergeFlags(NodeMayHaveBigInt32Result);
            if (arithProfile->didObserveHeapBigInt() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BigInt32Overflow))
                node->mergeFlags(NodeMayHaveHeapBigIntResult);
            break;
        }

        default:
            break;
        }
        
        return node;
    }
    
    Node* makeDivSafe(Node* node)
    {
        ASSERT(node->op() == ArithDiv || node->op() == ValueDiv);
        
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflowInt32InDFG);
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
            node->mergeFlags(NodeMayNegZeroInDFG);
        
        // The main slow case counter for op_div in the old JIT counts only when
        // the operands are not numbers. We don't care about that since we already
        // have speculations in place that take care of that separately. We only
        // care about when the outcome of the division is not an integer, which
        // is what the special fast case counter tells us.
        
        if (!m_inlineStackTop->m_profiledBlock->couldTakeSpecialArithFastCase(m_currentIndex))
            return node;
        
        // FIXME: It might be possible to make this more granular.
        node->mergeFlags(NodeMayOverflowInt32InBaseline | NodeMayNegZeroInBaseline);

        BinaryArithProfile* arithProfile = m_inlineStackTop->m_profiledBlock->binaryArithProfileForBytecodeIndex(m_currentIndex);

        if (arithProfile->didObserveBigInt32())
            node->mergeFlags(NodeMayHaveBigInt32Result);
        if (arithProfile->didObserveHeapBigInt() || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BigInt32Overflow))
            node->mergeFlags(NodeMayHaveHeapBigIntResult);

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

    VM* const m_vm;
    CodeBlock* const m_codeBlock;
    CodeBlock* const m_profiledBlock;
    Graph& m_graph;

    // The current block being generated.
    BasicBlock* m_currentBlock;
    // The bytecode index of the current instruction being generated.
    BytecodeIndex m_currentIndex;
    // The semantic origin of the current node if different from the current Index.
    CodeOrigin m_currentSemanticOrigin;
    // The exit origin of the current node if different from the current Index.
    CodeOrigin m_currentExitOrigin;
    // True if it's OK to OSR exit right now.
    bool m_exitOK { false };

    FrozenValue* const m_constantUndefined;
    FrozenValue* const m_constantNull;
    FrozenValue* const m_constantNaN;
    FrozenValue* const m_constantOne;
    Vector<Node*, 16> m_constants;

    HashMap<InlineCallFrame*, Vector<ArgumentPosition*>, WTF::DefaultHash<InlineCallFrame*>, WTF::NullableHashTraits<InlineCallFrame*>> m_inlineCallFrameToArgumentPositions;

    // The number of arguments passed to the function.
    const unsigned m_numArguments;
    // The number of locals (vars + temporaries) used by the bytecode for the function.
    unsigned m_numLocals;
    // The max number of temps used for forwarding data to an OSR exit checkpoint.
    unsigned m_numTmps;
    // The number of slots (in units of sizeof(Register)) that we need to
    // preallocate for arguments to outgoing calls from this frame. This
    // number includes the CallFrame slots that we initialize for the callee
    // (but not the callee-initialized CallerFrame and ReturnPC slots).
    // This number is 0 if and only if this function is a leaf.
    unsigned m_parameterSlots;
    // The number of var args passed to the next var arg node.
    unsigned m_numPassedVarArgs;

    struct InlineStackEntry {
        ByteCodeParser* const m_byteCodeParser;
        
        CodeBlock* const m_codeBlock;
        CodeBlock* const m_profiledBlock;
        InlineCallFrame* m_inlineCallFrame;
        
        ScriptExecutable* executable() { return m_codeBlock->ownerExecutable(); }
        
        QueryableExitProfile m_exitProfile;
        
        // Remapping of identifier and constant numbers from the code block being
        // inlined (inline callee) to the code block that we're inlining into
        // (the machine code block, which is the transitive, though not necessarily
        // direct, caller).
        Vector<unsigned> m_identifierRemap;
        Vector<unsigned> m_switchRemap;
        Vector<unsigned> m_stringSwitchRemap;
        
        // These are blocks whose terminal is a Jump, Branch or Switch, and whose target has not yet been linked.
        // Their terminal instead refers to a bytecode index, and the right BB can be found in m_blockLinkingTargets.
        Vector<BasicBlock*> m_unlinkedBlocks;
        
        // Potential block linking targets. Must be sorted by bytecodeBegin, and
        // cannot have two blocks that have the same bytecodeBegin.
        Vector<BasicBlock*> m_blockLinkingTargets;

        // Optional: a continuation block for returns to jump to. It is set by early returns if it does not exist.
        BasicBlock* m_continuationBlock;

        Operand m_returnValue;
        
        // Speculations about variable types collected from the profiled code block,
        // which are based on OSR exit profiles that past DFG compilations of this
        // code block had gathered.
        LazyOperandValueProfileParser m_lazyOperands;
        
        ICStatusMap m_baselineMap;
        ICStatusContext m_optimizedContext;
        
        // Pointers to the argument position trackers for this slice of code.
        Vector<ArgumentPosition*> m_argumentPositions;
        
        InlineStackEntry* const m_caller;
        
        InlineStackEntry(
            ByteCodeParser*,
            CodeBlock*,
            CodeBlock* profiledBlock,
            JSFunction* callee, // Null if this is a closure call.
            Operand returnValue,
            VirtualRegister inlineCallFrameStart,
            int argumentCountIncludingThis,
            InlineCallFrame::Kind,
            BasicBlock* continuationBlock);
        
        ~InlineStackEntry();
        
        Operand remapOperand(Operand operand) const
        {
            if (!m_inlineCallFrame)
                return operand;

            if (operand.isTmp())
                return Operand::tmp(operand.value() + m_inlineCallFrame->tmpOffset);
            
            ASSERT(!operand.virtualRegister().isConstant());

            return operand.virtualRegister() + m_inlineCallFrame->stackOffset;
        }
    };
    
    InlineStackEntry* m_inlineStackTop;
    
    ICStatusContextStack m_icContextStack;
    
    struct DelayedSetLocal {
        DelayedSetLocal() { }
        DelayedSetLocal(const CodeOrigin& origin, Operand operand, Node* value, SetMode setMode)
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
            return parser->setLocalOrTmp(m_origin, m_operand, m_value, m_setMode);
        }

        CodeOrigin m_origin;
        Operand m_operand;
        Node* m_value { nullptr };
        SetMode m_setMode;
    };

    Vector<DelayedSetLocal, 2> m_setLocalQueue;

    const JSInstruction* m_currentInstruction;
    const bool m_hasDebuggerEnabled;
    bool m_hasAnyForceOSRExits { false };
};

BasicBlock* ByteCodeParser::allocateTargetableBlock(BytecodeIndex bytecodeIndex)
{
    ASSERT(bytecodeIndex);
    Ref<BasicBlock> block = adoptRef(*new BasicBlock(bytecodeIndex, m_numArguments, m_numLocals, m_numTmps, 1));
    BasicBlock* blockPtr = block.ptr();
    // m_blockLinkingTargets must always be sorted in increasing order of bytecodeBegin
    if (m_inlineStackTop->m_blockLinkingTargets.size())
        ASSERT(m_inlineStackTop->m_blockLinkingTargets.last()->bytecodeBegin.offset() < bytecodeIndex.offset());
    m_inlineStackTop->m_blockLinkingTargets.append(blockPtr);
    m_graph.appendBlock(WTFMove(block));
    return blockPtr;
}

BasicBlock* ByteCodeParser::allocateUntargetableBlock()
{
    Ref<BasicBlock> block = adoptRef(*new BasicBlock(BytecodeIndex(), m_numArguments, m_numLocals, m_numTmps, 1));
    BasicBlock* blockPtr = block.ptr();
    m_graph.appendBlock(WTFMove(block));
    VERBOSE_LOG("Adding new untargetable block: ", blockPtr->index, "\n");
    return blockPtr;
}

void ByteCodeParser::makeBlockTargetable(BasicBlock* block, BytecodeIndex bytecodeIndex)
{
    RELEASE_ASSERT(!block->bytecodeBegin);
    block->bytecodeBegin = bytecodeIndex;
    // m_blockLinkingTargets must always be sorted in increasing order of bytecodeBegin
    if (m_inlineStackTop->m_blockLinkingTargets.size())
        ASSERT(m_inlineStackTop->m_blockLinkingTargets.last()->bytecodeBegin.offset() < bytecodeIndex.offset());
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
ByteCodeParser::Terminality ByteCodeParser::handleCall(const JSInstruction* pc, NodeType op, CallMode callMode, BytecodeIndex osrExitIndex)
{
    auto bytecode = pc->as<CallOp>();
    Node* callTarget = get(calleeFor(bytecode, m_currentIndex.checkpoint()));
    int registerOffset = -static_cast<int>(stackOffsetInRegistersForCall(bytecode, m_currentIndex.checkpoint()));

    CallLinkStatus callLinkStatus = CallLinkStatus::computeFor(
        m_inlineStackTop->m_profiledBlock, currentCodeOrigin(),
        m_inlineStackTop->m_baselineMap, m_icContextStack);

    InlineCallFrame::Kind kind = InlineCallFrame::kindFor(callMode);
    ASSERT(osrExitIndex);

    return handleCall(destinationFor(bytecode, m_currentIndex.checkpoint(), JITType::DFGJIT), op, kind, osrExitIndex, callTarget,
        argumentCountIncludingThisFor(bytecode, m_currentIndex.checkpoint()), registerOffset, callLinkStatus, getPrediction());
}

void ByteCodeParser::refineStatically(CallLinkStatus& callLinkStatus, Node* callTarget)
{
    if (callTarget->isCellConstant())
        callLinkStatus.setProvenConstantCallee(CallVariant(callTarget->asCell()));
}

ByteCodeParser::Terminality ByteCodeParser::handleCall(
    Operand result, NodeType op, InlineCallFrame::Kind kind, BytecodeIndex osrExitIndex,
    Node* callTarget, int argumentCountIncludingThis, int registerOffset,
    CallLinkStatus callLinkStatus, SpeculatedType prediction, ECMAMode ecmaMode)
{
    ASSERT(registerOffset <= 0);

    refineStatically(callLinkStatus, callTarget);
    
    VERBOSE_LOG("    Handling call at ", currentCodeOrigin(), ": ", callLinkStatus, "\n");
    
    // If we have profiling information about this call, and it did not behave too polymorphically,
    // we may be able to inline it, or in the case of recursive tail calls turn it into a jump.
    if (callLinkStatus.canOptimize()) {
        addToGraph(FilterCallLinkStatus, OpInfo(m_graph.m_plan.recordedStatuses().addCallLinkStatus(currentCodeOrigin(), callLinkStatus)), callTarget);

        VirtualRegister thisArgument = virtualRegisterForArgumentIncludingThis(0, registerOffset);
        auto optimizationResult = handleInlining(callTarget, result, callLinkStatus, registerOffset, thisArgument,
            argumentCountIncludingThis, osrExitIndex, op, kind, prediction, ecmaMode);
        if (optimizationResult == CallOptimizationResult::OptimizedToJump)
            return Terminal;
        if (optimizationResult == CallOptimizationResult::Inlined) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedCall();
            return NonTerminal;
        }
    }
    
    if (kind == InlineCallFrame::SetterCall && ecmaMode.isStrict())
        addToGraph(CheckNotJSCast, OpInfo(NullSetterFunction::info()), callTarget);
    Node* callNode = addCall(result, op, OpInfo(), callTarget, argumentCountIncludingThis, registerOffset, prediction);
    ASSERT(callNode->op() != TailCallVarargs && callNode->op() != TailCallForwardVarargs);
    return callNode->op() == TailCall ? Terminal : NonTerminal;
}

template<typename CallOp>
ByteCodeParser::Terminality ByteCodeParser::handleVarargsCall(const JSInstruction* pc, NodeType op, CallMode callMode)
{
    auto bytecode = pc->as<CallOp>();
    int firstFreeReg = bytecode.m_firstFree.offset();
    int firstVarArgOffset = bytecode.m_firstVarArg;
    
    SpeculatedType prediction = getPrediction();
    
    Node* callTarget = get(bytecode.m_callee);
    
    CallLinkStatus callLinkStatus = CallLinkStatus::computeFor(
        m_inlineStackTop->m_profiledBlock, currentCodeOrigin(),
        m_inlineStackTop->m_baselineMap, m_icContextStack);
    refineStatically(callLinkStatus, callTarget);
    
    VERBOSE_LOG("    Varargs call link status at ", currentCodeOrigin(), ": ", callLinkStatus, "\n");
    
    if (callLinkStatus.canOptimize()) {
        addToGraph(FilterCallLinkStatus, OpInfo(m_graph.m_plan.recordedStatuses().addCallLinkStatus(currentCodeOrigin(), callLinkStatus)), callTarget);

        if (handleVarargsInlining(callTarget, bytecode.m_dst,
            callLinkStatus, firstFreeReg, bytecode.m_thisValue, bytecode.m_arguments,
            firstVarArgOffset, op,
            InlineCallFrame::varargsKindFor(callMode))) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedCall();
            return NonTerminal;
        }
    }
    
    CallVarargsData* data = m_graph.m_callVarargsData.add();
    data->firstVarArgOffset = firstVarArgOffset;
    
    Node* thisChild = get(bytecode.m_thisValue);
    Node* argumentsChild = nullptr;
    if (op != TailCallForwardVarargs)
        argumentsChild = get(bytecode.m_arguments);

    if (op == TailCallVarargs || op == TailCallForwardVarargs) {
        if (allInlineFramesAreTailCalls()) {
            addToGraph(op, OpInfo(data), OpInfo(), callTarget, thisChild, argumentsChild);
            return Terminal;
        }
        op = op == TailCallVarargs ? TailCallVarargsInlinedCaller : TailCallForwardVarargsInlinedCaller;
    }

    Node* call = addToGraph(op, OpInfo(data), OpInfo(prediction), callTarget, thisChild, argumentsChild);
    if (bytecode.m_dst.isValid())
        set(bytecode.m_dst, call);
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
    addToGraph(CheckIsConstant, OpInfo(m_graph.freeze(calleeCell)), callTargetForCheck);
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
        addToGraph(Phantom, get(virtualRegisterForArgumentIncludingThis(i, registerOffset)));
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleRecursiveTailCall(Node* callTargetNode, CallVariant callVariant, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& emitFunctionCheckIfNeeded)
{
    if (UNLIKELY(!Options::optimizeRecursiveTailCalls()))
        return false;

    // This optimisation brings more performance if it only runs in FTL, probably because it interferes with tier-up.
    // See https://bugs.webkit.org/show_bug.cgi?id=178389 for details.
    if (!isFTL(m_graph.m_plan.mode()))
        return false;

    auto targetExecutable = callVariant.executable();
    InlineStackEntry* stackEntry = m_inlineStackTop;
    do {
        if (targetExecutable != stackEntry->executable())
            continue;
        VERBOSE_LOG("   We found a recursive tail call, trying to optimize it into a jump.\n");

        if (auto* callFrame = stackEntry->m_inlineCallFrame) {
            // FIXME: We only accept jump to CallFrame which has exact same argumentCountIncludingThis. But we can remove this by fixing up arguments.
            // And we can also allow jumping into CallFrame with Varargs if the passing number of arguments is greater than or equal to mandatoryMinimum of CallFrame.
            // https://bugs.webkit.org/show_bug.cgi?id=202317

            // Some code may statically use the argument count from the InlineCallFrame, so it would be invalid to loop back if it does not match.
            // We "continue" instead of returning false in case another stack entry further on the stack has the right number of arguments.
            if (argumentCountIncludingThis != callFrame->argumentCountIncludingThis)
                continue;
            // If the target InlineCallFrame is Varargs, we do not know how many arguments are actually filled by LoadVarargs. Varargs InlineCallFrame's
            // argumentCountIncludingThis is maximum number of potentially filled arguments by xkLoadVarargs. We "continue" to the upper frame which may be
            // a good target to jump into.
            if (callFrame->isVarargs())
                continue;
            // If an InlineCallFrame is not a closure, it was optimized using a constant callee.
            // Check if this is the same callee that we are dealing with.
            if (!callFrame->isClosureCall && callFrame->calleeConstant() != callVariant.function())
                continue;
        } else {
            // We are in the machine code entry (i.e. the original caller).
            // If we have more arguments than the number of parameters to the function, it is not clear where we could put them on the stack.
            if (static_cast<unsigned>(argumentCountIncludingThis) > m_codeBlock->numParameters())
                return false;
        }

        // We must add some check that the profiling information was correct and the target of this call is what we thought.
        emitFunctionCheckIfNeeded();
        // We flush everything, as if we were in the backedge of a loop (see treatment of op_jmp in parseBlock).
        flushForTerminal();

        // We must set the callee to the right value
        if (!stackEntry->m_inlineCallFrame)
            addToGraph(SetCallee, callTargetNode);
        else if (stackEntry->m_inlineCallFrame->isClosureCall)
            setDirect(remapOperand(stackEntry->m_inlineCallFrame, CallFrameSlot::callee), callTargetNode, NormalSet);


        // We must set the arguments to the right values
        if (!stackEntry->m_inlineCallFrame)
            addToGraph(SetArgumentCountIncludingThis, OpInfo(argumentCountIncludingThis));
        unsigned argIndex = 0;
        for (; argIndex < static_cast<unsigned>(argumentCountIncludingThis); ++argIndex) {
            Node* value = get(virtualRegisterForArgumentIncludingThis(argIndex, registerOffset));
            setDirect(stackEntry->remapOperand(virtualRegisterForArgumentIncludingThis(argIndex)), value, NormalSet);
        }
        Node* undefined = addToGraph(JSConstant, OpInfo(m_constantUndefined));
        for (; argIndex < stackEntry->m_codeBlock->numParameters(); ++argIndex)
            setDirect(stackEntry->remapOperand(virtualRegisterForArgumentIncludingThis(argIndex)), undefined, NormalSet);

        // We must repeat the work of op_enter here as we will jump right after it.
        // We jump right after it and not before it, because of some invariant saying that a CFG root cannot have predecessors in the IR.
        for (unsigned i = 0; i < stackEntry->m_codeBlock->numVars(); ++i)
            setDirect(stackEntry->remapOperand(virtualRegisterForLocal(i)), undefined, NormalSet);

        // We want to emit the SetLocals with an exit origin that points to the place we are jumping to.
        BytecodeIndex oldIndex = m_currentIndex;
        auto oldStackTop = m_inlineStackTop;
        m_inlineStackTop = stackEntry;
        static_assert(OpcodeIDWidthBySize<JSOpcodeTraits, OpcodeSize::Wide32>::opcodeIDSize == 1);
        m_currentIndex = BytecodeIndex(opcodeLengths[op_enter] + 1);
        m_exitOK = true;
        processSetLocalQueue();
        m_currentIndex = oldIndex;
        m_inlineStackTop = oldStackTop;
        m_exitOK = false;

        BasicBlock** entryBlockPtr = tryBinarySearch<BasicBlock*, BytecodeIndex>(stackEntry->m_blockLinkingTargets, stackEntry->m_blockLinkingTargets.size(), BytecodeIndex(opcodeLengths[op_enter] + 1), getBytecodeBeginForBlock);
        RELEASE_ASSERT(entryBlockPtr);
        addJumpTo(*entryBlockPtr);
        return true;
        // It would be unsound to jump over a non-tail call: the "tail" call is not really a tail call in that case.
    } while (stackEntry->m_inlineCallFrame && stackEntry->m_inlineCallFrame->kind == InlineCallFrame::TailCall && (stackEntry = stackEntry->m_caller));

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

    if (m_graph.m_plan.isUnlinked()) {
        VERBOSE_LOG("    Failing because the compilation mode is unlinked DFG.\n");
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
        if (codeBlock->numParameters() > static_cast<unsigned>(argumentCountIncludingThis)) {
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
    VERBOSE_LOG("    Is inlining candidate: ", codeBlock->ownerExecutable()->isInliningCandidate(), "\n");
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
    return codeBlock->bytecodeCost();
}

template<typename ChecksFunctor>
void ByteCodeParser::inlineCall(Node* callTargetNode, Operand result, CallVariant callee, int registerOffset, int argumentCountIncludingThis, InlineCallFrame::Kind kind, BasicBlock* continuationBlock, const ChecksFunctor& insertChecks)
{
    const JSInstruction* savedCurrentInstruction = m_currentInstruction;
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);

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
    
    Operand inlineCallFrameStart = VirtualRegister(m_inlineStackTop->remapOperand(VirtualRegister(registerOffsetAfterFixup)).value() + CallFrame::headerSizeInRegisters);
    
    ensureLocals(
        inlineCallFrameStart.toLocal() + 1 +
        CallFrame::headerSizeInRegisters + codeBlock->numCalleeLocals());
    
    ensureTmps((m_inlineStackTop->m_inlineCallFrame ? m_inlineStackTop->m_inlineCallFrame->tmpOffset : 0) + m_inlineStackTop->m_codeBlock->numTmps() + codeBlock->numTmps());

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

    // We want to claim the exit origin for the arity fixup nodes to be in the caller rather than the callee because
    // otherwise phantom insertion phase will think the virtual registers in the callee's header have been alive from the last
    // time they were set. For example:

    // --> foo: loc10 is a local in foo.
    //    ...
    //    1: MovHint(loc10)
    //    2: SetLocal(loc10)
    // <-- foo: loc10 ten is now out of scope for the InlineCallFrame of the caller.
    // ...
    // --> bar: loc10 is an argument to bar and needs arity fixup.
    //    ... // All of these nodes are ExitInvalid
    //    3: MovHint(loc10, ExitInvalid)
    //    4: SetLocal(loc10, ExitInvalid)
    //    ...

    // In this example phantom insertion phase will think @3 is always alive because it's in the header of bar. So,
    // it will think we are about to kill the old value, as loc10 is in the header of bar and therefore always live, and
    // thus need a Phantom. That Phantom, however, may be inserted  into the caller's NodeOrigin (all the nodes in bar
    // before @3 are ExitInvalid), which doesn't know about loc10. If we move all of the arity fixup nodes into the
    // caller's exit origin, forAllKilledOperands, which is how phantom insertion phase decides where phantoms are needed,
    // will no longer say loc10 is always alive.
    CodeOrigin oldExitOrigin = m_currentExitOrigin;
    m_currentExitOrigin = currentCodeOrigin();

    InlineStackEntry* callerStackTop = m_inlineStackTop;
    InlineStackEntry inlineStackEntry(this, codeBlock, codeBlock, callee.function(), result,
        inlineCallFrameStart.virtualRegister(), argumentCountIncludingThis, kind, continuationBlock);

    // This is where the actual inlining really happens.
    BytecodeIndex oldIndex = m_currentIndex;
    m_currentIndex = BytecodeIndex(0);

    // We don't want to exit here since we could do things like arity fixup which complicates OSR exit availability.
    m_exitOK = false;

    switch (kind) {
    case InlineCallFrame::GetterCall:
    case InlineCallFrame::SetterCall: {
        // When inlining getter and setter calls, we setup a stack frame which does not appear in the bytecode.
        // Because Inlining can switch on executable, we could have a graph like this.
        //
        // BB#0
        //     ...
        //     30: GetSetter
        //     31: MovHint(loc10)
        //     32: SetLocal(loc10)
        //     33: MovHint(loc9)
        //     34: SetLocal(loc9)
        //     ...
        //     37: GetExecutable(@30)
        //     ...
        //     41: Switch(@37)
        //
        // BB#2
        //     42: GetLocal(loc12, bc#7 of caller)
        //     ...
        //     --> callee: loc9 and loc10 are arguments of callee.
        //       ...
        //       <HERE, exit to callee, loc9 and loc10 are required in the bytecode>
        //
        // When we prune OSR availability at the beginning of BB#2 (bc#7 in the caller), we prune loc9 and loc10's liveness because the caller does not actually have loc9 and loc10.
        // However, when we begin executing the callee, we need OSR exit to be aware of where it can recover the arguments to the setter, loc9 and loc10. The MovHints in the inlined
        // callee make it so that if we exit at <HERE>, we can recover loc9 and loc10.
        for (int index = 0; index < argumentCountIncludingThis; ++index) {
            Operand argumentToGet = callerStackTop->remapOperand(virtualRegisterForArgumentIncludingThis(index, registerOffset));
            Node* value = getDirect(argumentToGet);
            addToGraph(MovHint, OpInfo(argumentToGet), value);
            m_setLocalQueue.append(DelayedSetLocal { currentCodeOrigin(), argumentToGet, value, ImmediateNakedSet });
        }
        break;
    }
    default:
        break;
    }

    if (arityFixupCount) {
        // Note: we do arity fixup in two phases:
        // 1. We get all the values we need and MovHint them to the expected locals.
        // 2. We SetLocal them after that. This way, if we exit, the callee's
        //    frame is already set up. If any SetLocal exits, we have a valid exit state.
        //    This is required because if we didn't do this in two phases, we may exit in
        //    the middle of arity fixup from the callee's CodeOrigin. This is unsound because exited
        //    code does not have arity fixup so that remaining necessary fixups are not executed.
        //    For example, consider if we need to pad two args:
        //    [arg3][arg2][arg1][arg0]
        //    [fix ][fix ][arg3][arg2][arg1][arg0]
        //    We memcpy starting from arg0 in the direction of arg3. If we were to exit at a type check
        //    for arg3's SetLocal in the callee's CodeOrigin, we'd exit with a frame like so:
        //    [arg3][arg2][arg1][arg2][arg1][arg0]
        //    Since we do not perform arity fixup in the callee, this is the frame used by the callee.
        //    And the callee would then just end up thinking its argument are:
        //    [fix ][fix ][arg3][arg2][arg1][arg0]
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
                Operand argumentToGet = callerStackTop->remapOperand(virtualRegisterForArgumentIncludingThis(index, registerOffset));
                Node* value = getDirect(argumentToGet);
                Operand argumentToSet = m_inlineStackTop->remapOperand(virtualRegisterForArgumentIncludingThis(index));
                addToGraph(MovHint, OpInfo(argumentToSet), value);
                m_setLocalQueue.append(DelayedSetLocal { currentCodeOrigin(), argumentToSet, value, ImmediateNakedSet });
            }
        }
        for (int index = 0; index < arityFixupCount; ++index) {
            Operand argumentToSet = m_inlineStackTop->remapOperand(virtualRegisterForArgumentIncludingThis(argumentCountIncludingThis + index));
            addToGraph(MovHint, OpInfo(argumentToSet), undefined);
            m_setLocalQueue.append(DelayedSetLocal { currentCodeOrigin(), argumentToSet, undefined, ImmediateNakedSet });
        }

        // At this point, it's OK to OSR exit because we finished setting up
        // our callee's frame. We emit an ExitOK below.
    }

    m_currentExitOrigin = oldExitOrigin;

    // At this point, it's again OK to OSR exit.
    m_exitOK = true;
    addToGraph(ExitOK);

    processSetLocalQueue();

    InlineVariableData inlineVariableData;
    inlineVariableData.inlineCallFrame = m_inlineStackTop->m_inlineCallFrame;
    inlineVariableData.argumentPositionStart = argumentPositionStart;
    inlineVariableData.calleeVariable = nullptr;
    
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

ByteCodeParser::CallOptimizationResult ByteCodeParser::handleCallVariant(Node* callTargetNode, Operand result, CallVariant callee, int registerOffset, VirtualRegister thisArgument, int argumentCountIncludingThis, BytecodeIndex osrExitIndex, InlineCallFrame::Kind kind, SpeculatedType prediction, unsigned& inliningBalance, BasicBlock* continuationBlock, bool needsToCheckCallee)
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
            m_currentIndex = osrExitIndex;
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
        if (handleIntrinsicCall(callTargetNode, result, callee, intrinsic, registerOffset, argumentCountIncludingThis, prediction, insertChecksWithAccounting)) {
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

bool ByteCodeParser::handleVarargsInlining(Node* callTargetNode, Operand result,
    const CallLinkStatus& callLinkStatus, int firstFreeReg, VirtualRegister thisArgument,
    VirtualRegister argumentsArgument, unsigned argumentsOffset,
    NodeType callOp, InlineCallFrame::Kind kind)
{
    VERBOSE_LOG("Handling inlining (Varargs)...\nStack: ", currentCodeOrigin(), "\n");

    StackCheck::Scope stackChecker(m_graph.m_stackChecker);
    if (!stackChecker.isSafeToRecurse()) {
        VERBOSE_LOG("Bailing inlining (compiler thread stack overflow eminent).\nStack: ", currentCodeOrigin(), "\n");
        return false;
    }
    if (callLinkStatus.maxArgumentCountIncludingThis() > Options::maximumVarargsForInlining()) {
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
    unsigned maxArgumentCountIncludingThis = std::max(callLinkStatus.maxArgumentCountIncludingThis(), mandatoryMinimum + 1);

    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    if (inliningCost(callVariant, maxArgumentCountIncludingThis, kind) > getInliningBalance(callLinkStatus, specializationKind)) {
        VERBOSE_LOG("Bailing inlining: inlining cost too high.\n");
        return false;
    }
    
    int registerOffset = firstFreeReg;
    registerOffset -= maxArgumentCountIncludingThis;
    registerOffset -= CallFrame::headerSizeInRegisters;
    registerOffset = -WTF::roundUpToMultipleOf(stackAlignmentRegisters(), -registerOffset);

    auto insertChecks = [&] (CodeBlock* codeBlock) {
        emitFunctionChecks(callVariant, callTargetNode, thisArgument);
        
        int remappedRegisterOffset =
        m_inlineStackTop->remapOperand(VirtualRegister(registerOffset)).virtualRegister().offset();
        
        ensureLocals(VirtualRegister(remappedRegisterOffset).toLocal());
        
        int argumentStart = registerOffset + CallFrame::headerSizeInRegisters;
        int remappedArgumentStart = m_inlineStackTop->remapOperand(VirtualRegister(argumentStart)).virtualRegister().offset();
        
        LoadVarargsData* data = m_graph.m_loadVarargsData.add();
        data->start = VirtualRegister(remappedArgumentStart + 1);
        data->count = VirtualRegister(remappedRegisterOffset + CallFrameSlot::argumentCountIncludingThis);
        data->offset = argumentsOffset;
        data->limit = maxArgumentCountIncludingThis;
        data->mandatoryMinimum = mandatoryMinimum;

        if (callOp == TailCallForwardVarargs) {
            Node* argumentCount;
            if (!inlineCallFrame())
                argumentCount = addToGraph(GetArgumentCountIncludingThis);
            else if (inlineCallFrame()->isVarargs())
                argumentCount = getDirect(remapOperand(inlineCallFrame(), CallFrameSlot::argumentCountIncludingThis));
            else 
                argumentCount = addToGraph(JSConstant, OpInfo(m_graph.freeze(jsNumber(inlineCallFrame()->argumentCountIncludingThis))));
            addToGraph(ForwardVarargs, OpInfo(data), argumentCount);
        } else {
            Node* arguments = get(argumentsArgument);
            auto argCountTmp = m_inlineStackTop->remapOperand(Operand::tmp(OpCallVarargs::argCountIncludingThis));
            setDirect(argCountTmp, addToGraph(VarargsLength, OpInfo(data), arguments));
            progressToNextCheckpoint();

            addToGraph(LoadVarargs, OpInfo(data), getLocalOrTmp(argCountTmp), arguments);
        }
        
        // LoadVarargs may OSR exit. Hence, we need to keep alive callTargetNode, thisArgument
        // and argumentsArgument for the baseline JIT. However, we only need a Phantom for
        // callTargetNode because the other 2 are still in use and alive at this point.
        addToGraph(Phantom, callTargetNode);
        
        // In DFG IR before SSA, we cannot insert control flow between after the
        // LoadVarargs and the last SetArgumentDefinitely. This isn't a problem once we get to DFG
        // SSA. Fortunately, we also have other reasons for not inserting control flow
        // before SSA.
        
        VariableAccessData* countVariable = newVariableAccessData(data->count);
        // This is pretty lame, but it will force the count to be flushed as an int. This doesn't
        // matter very much, since our use of a SetArgumentDefinitely and Flushes for this local slot is
        // mostly just a formality.
        countVariable->predict(SpecInt32Only);
        countVariable->mergeIsProfitableToUnbox(true);
        Node* setArgumentCount = addToGraph(SetArgumentDefinitely, OpInfo(countVariable));
        m_currentBlock->variablesAtTail.setOperand(countVariable->operand(), setArgumentCount);
        
        set(VirtualRegister(argumentStart), get(thisArgument), ImmediateNakedSet);
        unsigned numSetArguments = 0;
        for (unsigned argument = 1; argument < maxArgumentCountIncludingThis; ++argument) {
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
                ConcurrentJSLocker locker(codeBlock->valueProfileLock());
                ValueProfile& profile = codeBlock->valueProfileForArgument(argument);
                variable->predict(profile.computeUpdatedPrediction(locker));
            }
            
            Node* setArgument = addToGraph(numSetArguments >= mandatoryMinimum ? SetArgumentMaybe : SetArgumentDefinitely, OpInfo(variable));
            m_currentBlock->variablesAtTail.setOperand(variable->operand(), setArgument);
            ++numSetArguments;
        }
    };

    // Intrinsics and internal functions can only be inlined if we're not doing varargs. This is because
    // we currently don't have any way of getting profiling information for arguments to non-JS varargs
    // calls. The prediction propagator won't be of any help because LoadVarargs obscures the data flow,
    // and there are no callsite value profiles and native function won't have callee value profiles for
    // those arguments. Even worse, if the intrinsic decides to exit, it won't really have anywhere to
    // exit to: LoadVarargs is effectful and it's part of the op_call_varargs, so we can't exit without
    // calling LoadVarargs twice.
    inlineCall(callTargetNode, result, callVariant, registerOffset, maxArgumentCountIncludingThis, kind, nullptr, insertChecks);


    VERBOSE_LOG("Successful inlining (varargs, monomorphic).\nStack: ", currentCodeOrigin(), "\n");
    return true;
}

unsigned ByteCodeParser::getInliningBalance(const CallLinkStatus& callLinkStatus, CodeSpecializationKind specializationKind)
{
    unsigned inliningBalance = Options::maximumFunctionForCallInlineCandidateBytecodeCost();
    if (specializationKind == CodeForConstruct)
        inliningBalance = std::min(inliningBalance, Options::maximumFunctionForConstructInlineCandidateBytecoodeCost());
    if (callLinkStatus.isClosureCall())
        inliningBalance = std::min(inliningBalance, Options::maximumFunctionForClosureCallInlineCandidateBytecodeCost());
    return inliningBalance;
}

ByteCodeParser::CallOptimizationResult ByteCodeParser::handleInlining(
    Node* callTargetNode, Operand result, const CallLinkStatus& callLinkStatus,
    int registerOffset, VirtualRegister thisArgument,
    int argumentCountIncludingThis,
    BytecodeIndex osrExitIndex, NodeType callOp, InlineCallFrame::Kind kind, SpeculatedType prediction, ECMAMode ecmaMode)
{
    VERBOSE_LOG("Handling inlining...\nStack: ", currentCodeOrigin(), "\n");

    StackCheck::Scope stackChecker(m_graph.m_stackChecker);
    if (!stackChecker.isSafeToRecurse()) {
        VERBOSE_LOG("Bailing inlining (compiler thread stack overflow eminent).\nStack: ", currentCodeOrigin(), "\n");
        return CallOptimizationResult::DidNothing;
    }
    
    CodeSpecializationKind specializationKind = InlineCallFrame::specializationKindFor(kind);
    unsigned inliningBalance = getInliningBalance(callLinkStatus, specializationKind);

    // First check if we can avoid creating control flow. Our inliner does some CFG
    // simplification on the fly and this helps reduce compile times, but we can only leverage
    // this in cases where we don't need control flow diamonds to check the callee.
    if (!callLinkStatus.couldTakeSlowPath() && callLinkStatus.size() == 1) {
        return handleCallVariant(
            callTargetNode, result, callLinkStatus[0], registerOffset, thisArgument,
            argumentCountIncludingThis, osrExitIndex, kind, prediction, inliningBalance, nullptr, true);
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
    calleeReg = m_inlineStackTop->remapOperand(calleeReg).virtualRegister();
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

    BytecodeIndex oldIndex = m_currentIndex;
    for (unsigned i = 0; i < callLinkStatus.size(); ++i) {
        m_currentIndex = oldIndex;
        BasicBlock* calleeEntryBlock = allocateUntargetableBlock();
        m_currentBlock = calleeEntryBlock;
        prepareToParseBlock();

        // At the top of each switch case, we can exit.
        m_exitOK = true;
        
        Node* myCallTargetNode = getDirect(calleeReg);
        
        auto inliningResult = handleCallVariant(
            myCallTargetNode, result, callLinkStatus[i], registerOffset,
            thisArgument, argumentCountIncludingThis, osrExitIndex, kind, prediction,
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
    m_currentIndex = oldIndex;
    m_exitOK = true;
    data.fallThrough = BranchTarget(m_currentBlock);
    prepareToParseBlock();
    Node* myCallTargetNode = getDirect(calleeReg);
    if (couldTakeSlowPath) {
        if (kind == InlineCallFrame::SetterCall && ecmaMode.isStrict())
            addToGraph(CheckNotJSCast, OpInfo(NullSetterFunction::info()), myCallTargetNode);
        addCall(
            result, callOp, OpInfo(), myCallTargetNode, argumentCountIncludingThis,
            registerOffset, prediction);
        VERBOSE_LOG("We added a call in the slow path\n");
    } else {
        addToGraph(CheckBadValue);
        addToGraph(Phantom, myCallTargetNode);
        emitArgumentPhantoms(registerOffset, argumentCountIncludingThis);
        
        if (result.isValid())
            set(result, addToGraph(BottomValue));
        VERBOSE_LOG("couldTakeSlowPath was false\n");
    }

    m_currentIndex = osrExitIndex;
    m_exitOK = true; // Origin changed, so it's fine to exit again.
    processSetLocalQueue();

    if (Node* terminal = m_currentBlock->terminal())
        ASSERT_UNUSED(terminal, terminal->op() == TailCall || terminal->op() == TailCallVarargs || terminal->op() == TailCallForwardVarargs);
    else {
        addJumpTo(continuationBlock);
    }

    prepareToParseBlock();
    
    m_currentIndex = oldIndex;
    m_currentBlock = continuationBlock;
    m_exitOK = true;
    
    VERBOSE_LOG("Done inlining (hard).\nStack: ", currentCodeOrigin(), "\n");
    return CallOptimizationResult::Inlined;
}

template<typename ChecksFunctor>
void ByteCodeParser::handleMinMax(Operand result, NodeType op, int registerOffset, int argumentCountIncludingThis, const ChecksFunctor& insertChecks)
{
    ASSERT(op == ArithMin || op == ArithMax);

    if (argumentCountIncludingThis == 1) {
        insertChecks();
        double limit = op == ArithMax ? -std::numeric_limits<double>::infinity() : +std::numeric_limits<double>::infinity();
        set(result, addToGraph(JSConstant, OpInfo(m_graph.freeze(jsDoubleNumber(limit)))));
        return;
    }
     
    if (argumentCountIncludingThis == 2) {
        insertChecks();
        Node* resultNode = get(VirtualRegister(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
        addToGraph(Phantom, Edge(resultNode, NumberUse));
        set(result, resultNode);
        return;
    }
    
    insertChecks();
    for (int index = 1; index < argumentCountIncludingThis; ++index)
        addVarArgChild(get(virtualRegisterForArgumentIncludingThis(index, registerOffset)));
    set(result, addToGraph(Node::VarArg, op, OpInfo(), OpInfo()));
    return;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleIntrinsicCall(Node* callee, Operand result, CallVariant variant, Intrinsic intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks)
{
    VERBOSE_LOG("       The intrinsic is ", intrinsic, "\n");

    if (!isOpcodeShape<OpCallShape>(m_currentInstruction)) {
        VERBOSE_LOG("    Failing because instruction is not OpCallShape.\n");
        return false;
    }

    // It so happens that the code below doesn't handle the invalid result case. We could fix that, but
    // it would only benefit intrinsics called as setters, like if you do:
    //
    //     o.__defineSetter__("foo", Math.pow)
    //
    // Which is extremely amusing, but probably not worth optimizing.
    if (!result.isValid()) {
        VERBOSE_LOG("    Failing result operand is invalid.\n");
        return false;
    }

    bool didSetResult = false;
    auto setResult = [&] (Node* node) {
        RELEASE_ASSERT(!didSetResult);
        set(result, node);
        didSetResult = true;
    };

    auto inlineIntrinsic = [&] {
        switch (intrinsic) {

        // Intrinsic Functions:

        case AbsIntrinsic: {
            if (argumentCountIncludingThis == 1) { // Math.abs()
                insertChecks();
                setResult(addToGraph(JSConstant, OpInfo(m_constantNaN)));
                return true;
            }

            if (!MacroAssembler::supportsFloatingPointAbs())
                return false;

            insertChecks();
            Node* node = addToGraph(ArithAbs, get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
                node->mergeFlags(NodeMayOverflowInt32InDFG);
            setResult(node);
            return true;
        }

        case MinIntrinsic:
        case MaxIntrinsic:
            handleMinMax(result, intrinsic == MinIntrinsic ? ArithMin : ArithMax, registerOffset, argumentCountIncludingThis, insertChecks);
            didSetResult = true;
            return true;

#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
        case capitalizedName##Intrinsic:
        FOR_EACH_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY
        {
            if (argumentCountIncludingThis == 1) {
                insertChecks();
                setResult(addToGraph(JSConstant, OpInfo(m_constantNaN)));
                return true;
            }
            Arith::UnaryType type = Arith::UnaryType::Sin;
            switch (intrinsic) {
#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
            case capitalizedName##Intrinsic: \
                type = Arith::UnaryType::capitalizedName; \
                break;
            FOR_EACH_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            insertChecks();
            setResult(addToGraph(ArithUnary, OpInfo(static_cast<std::underlying_type<Arith::UnaryType>::type>(type)), get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }

        case FRoundIntrinsic:
        case SqrtIntrinsic: {
            if (argumentCountIncludingThis == 1) {
                insertChecks();
                setResult(addToGraph(JSConstant, OpInfo(m_constantNaN)));
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
            setResult(addToGraph(nodeType, get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }

        case PowIntrinsic: {
            if (argumentCountIncludingThis < 3) {
                // Math.pow() and Math.pow(x) return NaN.
                insertChecks();
                setResult(addToGraph(JSConstant, OpInfo(m_constantNaN)));
                return true;
            }
            insertChecks();
            VirtualRegister xOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            VirtualRegister yOperand = virtualRegisterForArgumentIncludingThis(2, registerOffset);
            setResult(addToGraph(ArithPow, get(xOperand), get(yOperand)));
            return true;
        }

        case TypedArrayEntriesIntrinsic:
        case TypedArrayKeysIntrinsic:
        case TypedArrayValuesIntrinsic: {
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            ArrayMode mode = getArrayMode(Array::Read);
            if (!mode.isSomeTypedArrayView() || mode.mayBeResizableOrGrowableSharedTypedArray())
                return false;

            addToGraph(CheckArray, OpInfo(mode.asWord()), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)));
            addToGraph(CheckDetached, get(virtualRegisterForArgumentIncludingThis(0, registerOffset)));
            FALLTHROUGH;
        }

        case ArrayEntriesIntrinsic:
        case ArrayKeysIntrinsic:
        case ArrayValuesIntrinsic: {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);
            auto* function = variant.function();
            if (!function)
                return false;
            if (function->globalObject() != globalObject)
                return false;

            insertChecks();

            std::optional<IterationKind> kind = interationKindForIntrinsic(intrinsic);
            RELEASE_ASSERT(!!kind);

            // Add the constant before exit becomes invalid because we may want to insert (redundant) checks on it in Fixup.
            Node* kindNode = jsConstant(jsNumber(static_cast<uint32_t>(*kind)));

            // We don't have an existing error string.
            unsigned errorStringIndex = UINT32_MAX;
            Node* object = addToGraph(ToObject, OpInfo(errorStringIndex), OpInfo(SpecNone), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)));

            Node* iterator = addToGraph(NewInternalFieldObject, OpInfo(m_graph.registerStructure(globalObject->arrayIteratorStructure())));

            addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::IteratedObject)), iterator, object);
            addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::Kind)), iterator, kindNode);

            setResult(iterator);
            return true;
        }
            
        case ArrayPushIntrinsic: {
            if (static_cast<unsigned>(argumentCountIncludingThis) >= MIN_SPARSE_ARRAY_INDEX)
                return false;
            
            ArrayMode arrayMode = getArrayMode(Array::Write);
            if (!arrayMode.isJSArray())
                return false;

            insertChecks();

            addVarArgChild(nullptr); // For storage.
            for (int i = 0; i < argumentCountIncludingThis; ++i)
                addVarArgChild(get(virtualRegisterForArgumentIncludingThis(i, registerOffset)));
            Node* arrayPush = addToGraph(Node::VarArg, ArrayPush, OpInfo(arrayMode.asWord()), OpInfo(prediction));
            setResult(arrayPush);
            return true;
        }

        case ArraySliceIntrinsic: {
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
                // FIXME: We could easily relax the Array/Object.prototype transition as long as we OSR exitted if we saw a hole.
                // https://bugs.webkit.org/show_bug.cgi?id=173171
                if (globalObject->arraySpeciesWatchpointSet().state() == IsWatched
                    && globalObject->havingABadTimeWatchpointSet().isStillValid()
                    && globalObject->arrayPrototypeChainIsSaneWatchpointSet().state() == IsWatched) {
                    m_graph.watchpoints().addLazily(globalObject->arraySpeciesWatchpointSet());
                    m_graph.watchpoints().addLazily(globalObject->havingABadTimeWatchpointSet());
                    m_graph.watchpoints().addLazily(globalObject->arrayPrototypeChainIsSaneWatchpointSet());

                    insertChecks();

                    Node* array = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
                    // We do a few things here to prove that we aren't skipping doing side-effects in an observable way:
                    // 1. We ensure that the "constructor" property hasn't been changed (because the observable
                    // effects of slice require that we perform a Get(array, "constructor") and we can skip
                    // that if we're an original array structure. (We can relax this in the future by using
                    // TryGetById and CheckIsConstant).
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
                        addVarArgChild(get(virtualRegisterForArgumentIncludingThis(1, registerOffset))); // Start index.
                    if (argumentCountIncludingThis >= 3)
                        addVarArgChild(get(virtualRegisterForArgumentIncludingThis(2, registerOffset))); // End index.
                    addVarArgChild(addToGraph(GetButterfly, array));

                    Node* arraySlice = addToGraph(Node::VarArg, ArraySlice, OpInfo(), OpInfo());
                    setResult(arraySlice);
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
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache))
                return false;

            // index parameter's BadType is critical. But the other ones can be relaxed, so not giving up optimization.
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType) && argumentCountIncludingThis > 2)
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
                // FIXME: We could easily relax the Array/Object.prototype transition as long as we OSR exitted if we saw a hole.
                // https://bugs.webkit.org/show_bug.cgi?id=173171
                if (globalObject->arrayPrototypeChainIsSaneWatchpointSet().state() == IsWatched) {
                    m_graph.watchpoints().addLazily(globalObject->arrayPrototypeChainIsSaneWatchpointSet());

                    insertChecks();

                    Node* array = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
                    addVarArgChild(array);
                    addVarArgChild(get(virtualRegisterForArgumentIncludingThis(1, registerOffset))); // Search element.
                    if (argumentCountIncludingThis >= 3)
                        addVarArgChild(get(virtualRegisterForArgumentIncludingThis(2, registerOffset))); // Start index.
                    addVarArgChild(nullptr);

                    Node* node = addToGraph(Node::VarArg, ArrayIndexOf, OpInfo(arrayMode.asWord()), OpInfo());
                    setResult(node);
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
            ArrayMode arrayMode = getArrayMode(Array::Write);
            if (!arrayMode.isJSArray())
                return false;
            switch (arrayMode.type()) {
            case Array::Int32:
            case Array::Double:
            case Array::Contiguous:
            case Array::ArrayStorage: {
                insertChecks();
                Node* arrayPop = addToGraph(ArrayPop, OpInfo(arrayMode.asWord()), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)));
                setResult(arrayPop);
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

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantCache)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, OutOfBounds))
                return false;
            
            insertChecks();
            
            for (unsigned i = 0; i < numArgs; ++i)
                addVarArgChild(get(virtualRegisterForArgumentIncludingThis(1 + i, registerOffset)));
            addVarArgChild(nullptr); // For storage edge.
            Node* resultNode = addToGraph(Node::VarArg, op, OpInfo(ArrayMode(Array::SelectUsingPredictions, action).asWord()), OpInfo(prediction));
            
            setResult(resultNode);
            return true;
        }

        case ParseIntIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            VirtualRegister valueOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            Node* parseInt;
            if (argumentCountIncludingThis == 2)
                parseInt = addToGraph(ParseInt, OpInfo(), OpInfo(prediction), get(valueOperand));
            else {
                ASSERT(argumentCountIncludingThis > 2);
                VirtualRegister radixOperand = virtualRegisterForArgumentIncludingThis(2, registerOffset);
                parseInt = addToGraph(ParseInt, OpInfo(), OpInfo(prediction), get(valueOperand), get(radixOperand));
            }
            setResult(parseInt);
            return true;
        }

        case CharCodeAtIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Uncountable) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            VirtualRegister thisOperand = virtualRegisterForArgumentIncludingThis(0, registerOffset);
            VirtualRegister indexOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            Node* charCode = addToGraph(StringCharCodeAt, OpInfo(ArrayMode(Array::String, Array::Read).asWord()), get(thisOperand), get(indexOperand));

            setResult(charCode);
            return true;
        }

        case StringPrototypeCodePointAtIntrinsic: {
            if (!is64Bit())
                return false;

            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Uncountable) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            VirtualRegister thisOperand = virtualRegisterForArgumentIncludingThis(0, registerOffset);
            VirtualRegister indexOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            Node* result = addToGraph(StringCodePointAt, OpInfo(ArrayMode(Array::String, Array::Read).asWord()), get(thisOperand), get(indexOperand));

            setResult(result);
            return true;
        }

        case CharAtIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            // FIXME: String#charAt returns empty string when index is out-of-bounds, and this does not break the AI's claim.
            // Only FTL supports out-of-bounds version now. We should support out-of-bounds version even in DFG.
            // https://bugs.webkit.org/show_bug.cgi?id=201678

            insertChecks();
            VirtualRegister thisOperand = virtualRegisterForArgumentIncludingThis(0, registerOffset);
            VirtualRegister indexOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            Node* charCode = addToGraph(StringCharAt, OpInfo(ArrayMode(Array::String, Array::Read).asWord()), get(thisOperand), get(indexOperand));

            setResult(charCode);
            return true;
        }

        case StringPrototypeLocaleCompareIntrinsic: {
            // Currently, only handling default locale case.
            if (argumentCountIncludingThis != 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            VirtualRegister thisOperand = virtualRegisterForArgumentIncludingThis(0, registerOffset);
            VirtualRegister indexOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            setResult(addToGraph(StringLocaleCompare, get(thisOperand), get(indexOperand)));
            return true;
        }

        case Clz32Intrinsic: {
            insertChecks();
            if (argumentCountIncludingThis == 1)
                setResult(addToGraph(JSConstant, OpInfo(m_graph.freeze(jsNumber(32)))));
            else {
                Node* operand = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
                setResult(addToGraph(ArithClz32, operand));
            }
            return true;
        }
        case FromCharCodeIntrinsic: {
            if (argumentCountIncludingThis != 2)
                return false;

            insertChecks();
            VirtualRegister indexOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            Node* charCode = addToGraph(StringFromCharCode, get(indexOperand));

            setResult(charCode);

            return true;
        }

        case RegExpExecIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;
            
            insertChecks();
            Node* regExpExec = addToGraph(RegExpExec, OpInfo(0), OpInfo(prediction), addToGraph(GetGlobalObject, callee), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
            setResult(regExpExec);
            
            return true;
        }
            
        case RegExpTestIntrinsic:
        case RegExpTestFastIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (intrinsic == RegExpTestIntrinsic) {
                // Don't inline intrinsic if we exited due to one of the primordial RegExp checks failing.
                if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue))
                    return false;

                JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();
                Structure* regExpStructure = globalObject->regExpStructure();
                m_graph.registerStructure(regExpStructure);
                ASSERT(regExpStructure->storedPrototype().isObject());
                ASSERT(regExpStructure->storedPrototype().asCell()->classInfo() == RegExpPrototype::info());

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
                Node* regExpObject = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
                addToGraph(Check, Edge(regExpObject, RegExpObjectUse));

                // Check that regExpObject's exec is actually the primodial RegExp.prototype.exec.
                UniquedStringImpl* execPropertyID = m_vm->propertyNames->exec.impl();
                m_graph.identifiers().ensure(execPropertyID);
                Node* actualProperty = addToGraph(TryGetById, OpInfo(CacheableIdentifier::createFromImmortalIdentifier(execPropertyID)), OpInfo(SpecFunction), Edge(regExpObject, CellUse));
                FrozenValue* regExpPrototypeExec = m_graph.freeze(globalObject->regExpProtoExecFunction());
                addToGraph(CheckIsConstant, OpInfo(regExpPrototypeExec), Edge(actualProperty, CellUse));
            }

            insertChecks();
            Node* regExpObject = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* regExpExec = addToGraph(RegExpTest, OpInfo(0), OpInfo(prediction), addToGraph(GetGlobalObject, callee), regExpObject, get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
            setResult(regExpExec);
            
            return true;
        }

        case RegExpMatchFastIntrinsic: {
            RELEASE_ASSERT(argumentCountIncludingThis == 2);

            insertChecks();
            Node* regExpMatch = addToGraph(RegExpMatchFast, OpInfo(0), OpInfo(prediction), addToGraph(GetGlobalObject, callee), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
            setResult(regExpMatch);
            return true;
        }

        case ObjectCreateIntrinsic: {
            if (argumentCountIncludingThis != 2)
                return false;

            insertChecks();
            setResult(addToGraph(ObjectCreate, get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }

        case ObjectAssignIntrinsic: {
            if (argumentCountIncludingThis != 3)
                return false;

            insertChecks();

            // ToObject is idempotent if it succeeds. Plus, it is non-observable except for the case that an exception is thrown. And when the exception is thrown,
            // we exit from DFG / FTL. Plus, we keep ordering of these two ToObject because clobberizing rule says clobberTop. So,
            // we can say exitOK for each ToObject.
            unsigned errorStringIndex = UINT32_MAX;
            Node* target = addToGraph(ToObject, OpInfo(errorStringIndex), OpInfo(SpecNone), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
            m_exitOK = true;
            addToGraph(ExitOK);
            addToGraph(ObjectAssign, Edge(target, KnownCellUse), Edge(get(virtualRegisterForArgumentIncludingThis(2, registerOffset))));
            setResult(target);
            return true;
        }

        case ObjectGetPrototypeOfIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            setResult(addToGraph(GetPrototypeOf, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }

        case ObjectIsIntrinsic: {
            if (argumentCountIncludingThis < 3)
                return false;

            insertChecks();
            setResult(addToGraph(SameValue, get(virtualRegisterForArgumentIncludingThis(1, registerOffset)), get(virtualRegisterForArgumentIncludingThis(2, registerOffset))));
            return true;
        }

        case ObjectKeysIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            setResult(addToGraph(ObjectKeys, get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }

        case ObjectGetOwnPropertyNamesIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            setResult(addToGraph(ObjectGetOwnPropertyNames, get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }

        case ObjectToStringIntrinsic: {
            insertChecks();
            setResult(addToGraph(ObjectToString, get(virtualRegisterForArgumentIncludingThis(0, registerOffset))));
            return true;
        }

        case ReflectGetPrototypeOfIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            setResult(addToGraph(GetPrototypeOf, OpInfo(0), OpInfo(prediction), Edge(get(virtualRegisterForArgumentIncludingThis(1, registerOffset)), ObjectUse)));
            return true;
        }

        case IsTypedArrayViewIntrinsic: {
            ASSERT(argumentCountIncludingThis == 2);

            insertChecks();
            setResult(addToGraph(IsTypedArrayView, OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }

        case StringPrototypeValueOfIntrinsic: {
            insertChecks();
            Node* value = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            setResult(addToGraph(StringValueOf, value));
            return true;
        }

        case StringPrototypeReplaceIntrinsic: {
            if (argumentCountIncludingThis < 3)
                return false;

            // Don't inline intrinsic if we exited due to "search" not being a RegExp or String object.
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            // Don't inline intrinsic if we exited due to one of the primordial RegExp checks failing.
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue))
                return false;

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();
            if (!globalObject->stringSymbolReplaceWatchpointSet().isStillValid() || !globalObject->regExpPrimordialPropertiesWatchpointSet().isStillValid())
                return false;

            insertChecks();

            Node* resultNode = addToGraph(StringReplace, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)), get(virtualRegisterForArgumentIncludingThis(2, registerOffset)));
            setResult(resultNode);
            return true;
        }
            
        case StringPrototypeReplaceRegExpIntrinsic: {
            if (argumentCountIncludingThis < 3)
                return false;
            
            insertChecks();
            Node* resultNode = addToGraph(StringReplaceRegExp, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)), get(virtualRegisterForArgumentIncludingThis(2, registerOffset)));
            setResult(resultNode);
            return true;
        }

        case StringPrototypeReplaceStringIntrinsic: {
            if (argumentCountIncludingThis < 3)
                return false;

            insertChecks();
            Node* resultNode = addToGraph(StringReplaceString, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)), get(virtualRegisterForArgumentIncludingThis(2, registerOffset)));
            setResult(resultNode);
            return true;
        }
            
        case RoundIntrinsic:
        case FloorIntrinsic:
        case CeilIntrinsic:
        case TruncIntrinsic: {
            if (argumentCountIncludingThis == 1) {
                insertChecks();
                setResult(addToGraph(JSConstant, OpInfo(m_constantNaN)));
                return true;
            }
            insertChecks();
            Node* operand = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
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
            setResult(roundNode);
            return true;
        }
        case IMulIntrinsic: {
            if (argumentCountIncludingThis < 3)
                return false;
            insertChecks();
            VirtualRegister leftOperand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            VirtualRegister rightOperand = virtualRegisterForArgumentIncludingThis(2, registerOffset);
            Node* left = get(leftOperand);
            Node* right = get(rightOperand);
            setResult(addToGraph(ArithIMul, left, right));
            return true;
        }

        case RandomIntrinsic: {
            insertChecks();
            setResult(addToGraph(ArithRandom));
            return true;
        }
            
        case DFGTrueIntrinsic: {
            insertChecks();
            setResult(jsConstant(jsBoolean(true)));
            return true;
        }

        case FTLTrueIntrinsic: {
            insertChecks();
            setResult(jsConstant(jsBoolean(m_graph.m_plan.isFTL())));
            return true;
        }
            
        case OSRExitIntrinsic: {
            insertChecks();
            addToGraph(ForceOSRExit);
            setResult(addToGraph(JSConstant, OpInfo(m_constantUndefined)));
            return true;
        }
            
        case IsFinalTierIntrinsic: {
            insertChecks();
            setResult(jsConstant(jsBoolean(Options::useFTLJIT() ? m_graph.m_plan.isFTL() : true)));
            return true;
        }
            
        case SetInt32HeapPredictionIntrinsic: {
            insertChecks();
            for (int i = 1; i < argumentCountIncludingThis; ++i) {
                Node* node = get(virtualRegisterForArgumentIncludingThis(i, registerOffset));
                if (node->hasHeapPrediction())
                    node->setHeapPrediction(SpecInt32Only);
            }
            setResult(addToGraph(JSConstant, OpInfo(m_constantUndefined)));
            return true;
        }
            
        case CheckInt32Intrinsic: {
            insertChecks();
            for (int i = 1; i < argumentCountIncludingThis; ++i) {
                Node* node = get(virtualRegisterForArgumentIncludingThis(i, registerOffset));
                addToGraph(Phantom, Edge(node, Int32Use));
            }
            setResult(jsConstant(jsBoolean(true)));
            return true;
        }
            
        case FiatInt52Intrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;
            insertChecks();
            VirtualRegister operand = virtualRegisterForArgumentIncludingThis(1, registerOffset);
            if (enableInt52())
                setResult(addToGraph(FiatInt52, get(operand)));
            else
                setResult(get(operand));
            return true;
        }

        case JSMapGetIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            Node* map = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* normalizedKey = addToGraph(NormalizeMapKey, key);
            Node* hash = addToGraph(MapHash, normalizedKey);
            Node* bucket = addToGraph(GetMapBucket, Edge(map, MapObjectUse), Edge(normalizedKey), Edge(hash));
            Node* resultNode = addToGraph(LoadValueFromMapBucket, OpInfo(BucketOwnerType::Map), OpInfo(prediction), bucket);
            setResult(resultNode);
            return true;
        }

        case JSSetHasIntrinsic:
        case JSMapHasIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            Node* mapOrSet = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* normalizedKey = addToGraph(NormalizeMapKey, key);
            Node* hash = addToGraph(MapHash, normalizedKey);
            UseKind useKind = intrinsic == JSSetHasIntrinsic ? SetObjectUse : MapObjectUse;
            Node* bucket = addToGraph(GetMapBucket, OpInfo(0), Edge(mapOrSet, useKind), Edge(normalizedKey), Edge(hash));
            JSCell* sentinel = nullptr;
            if (intrinsic == JSMapHasIntrinsic)
                sentinel = m_vm->sentinelMapBucket();
            else
                sentinel = m_vm->sentinelSetBucket();

            FrozenValue* frozenPointer = m_graph.freeze(sentinel);
            Node* invertedResult = addToGraph(CompareEqPtr, OpInfo(frozenPointer), bucket);
            Node* resultNode = addToGraph(LogicalNot, invertedResult);
            setResult(resultNode);
            return true;
        }

        case JSSetAddIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            Node* base = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* normalizedKey = addToGraph(NormalizeMapKey, key);
            Node* hash = addToGraph(MapHash, normalizedKey);
            addToGraph(SetAdd, base, normalizedKey, hash);
            setResult(base);
            return true;
        }

        case JSMapSetIntrinsic: {
            if (argumentCountIncludingThis < 3)
                return false;

            insertChecks();
            Node* base = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* value = get(virtualRegisterForArgumentIncludingThis(2, registerOffset));

            Node* normalizedKey = addToGraph(NormalizeMapKey, key);
            Node* hash = addToGraph(MapHash, normalizedKey);

            addVarArgChild(base);
            addVarArgChild(normalizedKey);
            addVarArgChild(value);
            addVarArgChild(hash);
            addToGraph(Node::VarArg, MapSet, OpInfo(0), OpInfo(0));
            setResult(base);
            return true;
        }

        case JSMapEntriesIntrinsic:
        case JSMapKeysIntrinsic:
        case JSMapValuesIntrinsic:
        case JSSetEntriesIntrinsic:
        case JSSetValuesIntrinsic: {
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();

            IterationKind kind = IterationKind::Values;
            UseKind useKind = MapObjectUse;
            switch (intrinsic) {
            case JSMapValuesIntrinsic:
                kind = IterationKind::Values;
                useKind = MapObjectUse;
                break;
            case JSMapKeysIntrinsic:
                kind = IterationKind::Keys;
                useKind = MapObjectUse;
                break;
            case JSMapEntriesIntrinsic:
                kind = IterationKind::Entries;
                useKind = MapObjectUse;
                break;
            case JSSetValuesIntrinsic:
                kind = IterationKind::Values;
                useKind = SetObjectUse;
                break;
            case JSSetEntriesIntrinsic:
                kind = IterationKind::Entries;
                useKind = SetObjectUse;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            Node* base = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            addToGraph(Check, Edge(base, useKind));
            Node* bucket = addToGraph(GetMapBucketHead, Edge(base, useKind));

            Node* kindNode = jsConstant(jsNumber(static_cast<uint32_t>(kind)));

            JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);
            Node* iterator = nullptr;
            if (useKind == MapObjectUse) {
                iterator = addToGraph(NewInternalFieldObject, OpInfo(m_graph.registerStructure(globalObject->mapIteratorStructure())));
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSMapIterator::Field::MapBucket)), iterator, bucket);
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSMapIterator::Field::IteratedObject)), iterator, base);
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSMapIterator::Field::Kind)), iterator, kindNode);
            } else {
                iterator = addToGraph(NewInternalFieldObject, OpInfo(m_graph.registerStructure(globalObject->setIteratorStructure())));
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSSetIterator::Field::SetBucket)), iterator, bucket);
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSSetIterator::Field::IteratedObject)), iterator, base);
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSSetIterator::Field::Kind)), iterator, kindNode);
            }

            setResult(iterator);
            return true;
        }

        case JSSetBucketHeadIntrinsic:
        case JSMapBucketHeadIntrinsic: {
            ASSERT(argumentCountIncludingThis == 2);

            insertChecks();
            Node* map = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            UseKind useKind = intrinsic == JSSetBucketHeadIntrinsic ? SetObjectUse : MapObjectUse;
            Node* resultNode = addToGraph(GetMapBucketHead, Edge(map, useKind));
            setResult(resultNode);
            return true;
        }

        case JSSetBucketNextIntrinsic:
        case JSMapBucketNextIntrinsic: {
            ASSERT(argumentCountIncludingThis == 2);

            insertChecks();
            Node* bucket = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            BucketOwnerType type = intrinsic == JSSetBucketNextIntrinsic ? BucketOwnerType::Set : BucketOwnerType::Map;
            Node* resultNode = addToGraph(GetMapBucketNext, OpInfo(type), bucket);
            setResult(resultNode);
            return true;
        }

        case JSSetBucketKeyIntrinsic:
        case JSMapBucketKeyIntrinsic: {
            ASSERT(argumentCountIncludingThis == 2);

            insertChecks();
            Node* bucket = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            BucketOwnerType type = intrinsic == JSSetBucketKeyIntrinsic ? BucketOwnerType::Set : BucketOwnerType::Map;
            Node* resultNode = addToGraph(LoadKeyFromMapBucket, OpInfo(type), OpInfo(prediction), bucket);
            setResult(resultNode);
            return true;
        }

        case JSMapBucketValueIntrinsic: {
            ASSERT(argumentCountIncludingThis == 2);

            insertChecks();
            Node* bucket = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* resultNode = addToGraph(LoadValueFromMapBucket, OpInfo(BucketOwnerType::Map), OpInfo(prediction), bucket);
            setResult(resultNode);
            return true;
        }

        case JSWeakMapGetIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* map = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            addToGraph(Check, Edge(key, CellUse));
            Node* hash = addToGraph(MapHash, key);
            Node* holder = addToGraph(WeakMapGet, Edge(map, WeakMapObjectUse), Edge(key, CellUse), Edge(hash, Int32Use));
            Node* resultNode = addToGraph(ExtractValueFromWeakMapGet, OpInfo(), OpInfo(prediction), holder);

            setResult(resultNode);
            return true;
        }

        case JSWeakMapHasIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* map = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            addToGraph(Check, Edge(key, CellUse));
            Node* hash = addToGraph(MapHash, key);
            Node* holder = addToGraph(WeakMapGet, Edge(map, WeakMapObjectUse), Edge(key, CellUse), Edge(hash, Int32Use));
            Node* invertedResult = addToGraph(IsEmpty, holder);
            Node* resultNode = addToGraph(LogicalNot, invertedResult);

            setResult(resultNode);
            return true;
        }

        case JSWeakSetHasIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* map = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            addToGraph(Check, Edge(key, CellUse));
            Node* hash = addToGraph(MapHash, key);
            Node* holder = addToGraph(WeakMapGet, Edge(map, WeakSetObjectUse), Edge(key, CellUse), Edge(hash, Int32Use));
            Node* invertedResult = addToGraph(IsEmpty, holder);
            Node* resultNode = addToGraph(LogicalNot, invertedResult);

            setResult(resultNode);
            return true;
        }

        case JSWeakSetAddIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* base = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            addToGraph(Check, Edge(key, CellUse));
            Node* hash = addToGraph(MapHash, key);
            addToGraph(WeakSetAdd, Edge(base, WeakSetObjectUse), Edge(key, CellUse), Edge(hash, Int32Use));
            setResult(base);
            return true;
        }

        case JSWeakMapSetIntrinsic: {
            if (argumentCountIncludingThis < 3)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* base = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* value = get(virtualRegisterForArgumentIncludingThis(2, registerOffset));

            addToGraph(Check, Edge(key, CellUse));
            Node* hash = addToGraph(MapHash, key);

            addVarArgChild(Edge(base, WeakMapObjectUse));
            addVarArgChild(Edge(key, CellUse));
            addVarArgChild(Edge(value));
            addVarArgChild(Edge(hash, Int32Use));
            addToGraph(Node::VarArg, WeakMapSet, OpInfo(0), OpInfo(0));
            setResult(base);
            return true;
        }

        case DatePrototypeGetTimeIntrinsic: {
            if (!is64Bit())
                return false;
            insertChecks();
            Node* base = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            setResult(addToGraph(DateGetTime, OpInfo(intrinsic), OpInfo(), base));
            return true;
        }

        case DatePrototypeGetFullYearIntrinsic:
        case DatePrototypeGetUTCFullYearIntrinsic:
        case DatePrototypeGetMonthIntrinsic:
        case DatePrototypeGetUTCMonthIntrinsic:
        case DatePrototypeGetDateIntrinsic:
        case DatePrototypeGetUTCDateIntrinsic:
        case DatePrototypeGetDayIntrinsic:
        case DatePrototypeGetUTCDayIntrinsic:
        case DatePrototypeGetHoursIntrinsic:
        case DatePrototypeGetUTCHoursIntrinsic:
        case DatePrototypeGetMinutesIntrinsic:
        case DatePrototypeGetUTCMinutesIntrinsic:
        case DatePrototypeGetSecondsIntrinsic:
        case DatePrototypeGetUTCSecondsIntrinsic:
        case DatePrototypeGetMillisecondsIntrinsic:
        case DatePrototypeGetUTCMillisecondsIntrinsic:
        case DatePrototypeGetTimezoneOffsetIntrinsic:
        case DatePrototypeGetYearIntrinsic: {
            if (!is64Bit())
                return false;
            insertChecks();
            Node* base = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            setResult(addToGraph(DateGetInt32OrNaN, OpInfo(intrinsic), OpInfo(prediction), base));
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
            bool isResizable = false;
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

            TriState isLittleEndian = TriState::Indeterminate;
            Node* littleEndianChild = nullptr;
            if (byteSize > 1) {
                if (argumentCountIncludingThis < 3)
                    isLittleEndian = TriState::False;
                else {
                    littleEndianChild = get(virtualRegisterForArgumentIncludingThis(2, registerOffset));
                    if (littleEndianChild->hasConstant()) {
                        JSValue constant = littleEndianChild->constant()->value();
                        if (constant) {
                            isLittleEndian = constant.pureToBoolean();
                            if (isLittleEndian != TriState::Indeterminate)
                                littleEndianChild = nullptr;
                        }
                    } else
                        isLittleEndian = TriState::Indeterminate;
                }
            }

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, UnexpectedResizableArrayBufferView))
                isResizable = true;
            else
                isResizable = getArrayMode(Array::Read).mayBeResizableOrGrowableSharedTypedArray();

            DataViewData data { };
            data.isLittleEndian = isLittleEndian;
            data.isSigned = isSigned;
            data.isResizable = isResizable;
            data.byteSize = byteSize;

            setResult(
                addToGraph(op, OpInfo(data.asQuadWord), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(0, registerOffset)), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)), littleEndianChild));
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
            bool isResizable = false;
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

            TriState isLittleEndian = TriState::Indeterminate;
            Node* littleEndianChild = nullptr;
            if (byteSize > 1) {
                if (argumentCountIncludingThis < 4)
                    isLittleEndian = TriState::False;
                else {
                    littleEndianChild = get(virtualRegisterForArgumentIncludingThis(3, registerOffset));
                    if (littleEndianChild->hasConstant()) {
                        JSValue constant = littleEndianChild->constant()->value();
                        if (constant) {
                            isLittleEndian = constant.pureToBoolean();
                            if (isLittleEndian != TriState::Indeterminate)
                                littleEndianChild = nullptr;
                        }
                    } else
                        isLittleEndian = TriState::Indeterminate;
                }
            }

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, UnexpectedResizableArrayBufferView))
                isResizable = true;
            else
                isResizable = getArrayMode(Array::Read).mayBeResizableOrGrowableSharedTypedArray();

            DataViewData data { };
            data.isLittleEndian = isLittleEndian;
            data.isSigned = isSigned;
            data.isResizable = isResizable;
            data.byteSize = byteSize;
            data.isFloatingPoint = isFloatingPoint;

            addVarArgChild(get(virtualRegisterForArgumentIncludingThis(0, registerOffset)));
            addVarArgChild(get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
            addVarArgChild(get(virtualRegisterForArgumentIncludingThis(2, registerOffset)));
            addVarArgChild(littleEndianChild);

            addToGraph(Node::VarArg, DataViewSet, OpInfo(data.asQuadWord), OpInfo());
            setResult(addToGraph(JSConstant, OpInfo(m_constantUndefined)));
            return true;
        }

        case HasOwnPropertyIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            // This can be racy, that's fine. We know that once we observe that this is created,
            // that it will never be destroyed until the VM is destroyed. It's unlikely that
            // we'd ever get to the point where we inline this as an intrinsic without the
            // cache being created, however, it's possible if we always throw exceptions inside
            // hasOwnProperty.
            if (!m_vm->hasOwnPropertyCache())
                return false;

            insertChecks();
            Node* object = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* key = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* resultNode = addToGraph(HasOwnProperty, object, key);
            setResult(resultNode);
            return true;
        }

        case StringPrototypeSubstringIntrinsic:
        case StringPrototypeSliceIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* thisString = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* start = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* end = nullptr;
            if (argumentCountIncludingThis > 2)
                end = get(virtualRegisterForArgumentIncludingThis(2, registerOffset));
            Node* resultNode = addToGraph(intrinsic == StringPrototypeSubstringIntrinsic ? StringSubstring : StringSlice, thisString, start, end);
            setResult(resultNode);
            return true;
        }

        case StringPrototypeToLowerCaseIntrinsic: {
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* thisString = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* resultNode = addToGraph(ToLowerCase, thisString);
            setResult(resultNode);
            return true;
        }

        case NumberPrototypeToStringIntrinsic: {
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* thisNumber = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            if (argumentCountIncludingThis == 1) {
                Node* resultNode = addToGraph(NumberToStringWithValidRadixConstant, OpInfo(10), thisNumber);
                setResult(resultNode);
            } else {
                Node* radix = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
                Node* resultNode = addToGraph(NumberToStringWithRadix, thisNumber, radix);
                setResult(resultNode);
            }
            return true;
        }

        case NumberIsIntegerIntrinsic: {
            if (argumentCountIncludingThis < 2)
                return false;

            insertChecks();
            Node* input = get(virtualRegisterForArgumentIncludingThis(1, registerOffset));
            Node* resultNode = addToGraph(NumberIsInteger, input);
            setResult(resultNode);
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
            setResult(addToGraph(CPUIntrinsic, OpInfo(intrinsic), OpInfo()));
            return true;
#else
            return false;
#endif
        }

        case FunctionToStringIntrinsic: {
            if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType))
                return false;

            insertChecks();
            Node* function = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
            Node* resultNode = addToGraph(FunctionToString, function);
            setResult(resultNode);
            return true;
        }

        default:
            return false;
        }
    };

    if (inlineIntrinsic()) {
        RELEASE_ASSERT(didSetResult);
        return true;
    }

    return false;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleDOMJITCall(Node* callTarget, Operand result, const DOMJIT::Signature* signature, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, const ChecksFunctor& insertChecks)
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
    addCall(result, Call, OpInfo(signature), callTarget, argumentCountIncludingThis, registerOffset, prediction);
    return true;
}


template<typename ChecksFunctor>
bool ByteCodeParser::handleIntrinsicGetter(Operand result, SpeculatedType prediction, const GetByVariant& variant, Node* thisNode, const ChecksFunctor& insertChecks)
{
#if USE(LARGE_TYPED_ARRAYS)
    static_assert(enableInt52());
#endif

    switch (variant.intrinsic()) {
    case TypedArrayByteLengthIntrinsic: {
        bool mayBeLargeTypedArray = !isInt32Speculation(prediction) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow);
#if !USE(LARGE_TYPED_ARRAYS)
        if (mayBeLargeTypedArray)
            return false;
#endif
        TypedArrayType type = typedArrayType((*variant.structureSet().begin())->typeInfo().type());
        Array::Type arrayType = toArrayType(type);
        bool mayBeResizableOrGrowableSharedTypedArray = m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, UnexpectedResizableArrayBufferView);
        size_t logSize = logElementSize(type);

        variant.structureSet().forEach([&] (Structure* structure) {
            TypedArrayType curType = typedArrayType(structure->typeInfo().type());
            ASSERT(logSize == logElementSize(curType));
            arrayType = refineTypedArrayType(arrayType, curType);
            mayBeResizableOrGrowableSharedTypedArray |= isResizableOrGrowableSharedTypedArrayIncludingDataView(structure->classInfoForCells());
            ASSERT(arrayType != Array::Generic);
        });

#if USE(JSVALUE32_64)
        if (mayBeResizableOrGrowableSharedTypedArray)
            return false;
#endif

        insertChecks();
        NodeType op = mayBeLargeTypedArray ? GetTypedArrayLengthAsInt52 : GetArrayLength;
        Node* lengthNode = addToGraph(op, OpInfo(ArrayMode(arrayType, Array::NonArray, Array::InBounds, Array::AsIs, Array::Read, mayBeLargeTypedArray, mayBeResizableOrGrowableSharedTypedArray).asWord()), thisNode);
        // Our ArrayMode shouldn't cause us to exit here so we should be ok to exit without effects.
        m_exitOK = true;
        addToGraph(ExitOK);

        if (!logSize) {
            set(result, lengthNode);
            return true;
        }

        // We cannot use a BitLShift here because typed arrays may have a byteLength that overflows Int32.
        Node* typeSize = jsConstant(jsNumber(1 << logSize));
        set(result, addToGraph(ArithMul, lengthNode, typeSize));

        return true;
    }

    case TypedArrayLengthIntrinsic: {
        bool mayBeLargeTypedArray = !isInt32Speculation(prediction) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow);
#if !USE(LARGE_TYPED_ARRAYS)
        if (mayBeLargeTypedArray)
            return false;
#endif
        TypedArrayType type = typedArrayType((*variant.structureSet().begin())->typeInfo().type());
        Array::Type arrayType = toArrayType(type);
        bool mayBeResizableOrGrowableSharedTypedArray = m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, UnexpectedResizableArrayBufferView);

        variant.structureSet().forEach([&] (Structure* structure) {
            TypedArrayType curType = typedArrayType(structure->typeInfo().type());
            arrayType = refineTypedArrayType(arrayType, curType);
            mayBeResizableOrGrowableSharedTypedArray |= isResizableOrGrowableSharedTypedArrayIncludingDataView(structure->classInfoForCells());
            ASSERT(arrayType != Array::Generic);
        });

#if USE(JSVALUE32_64)
        if (mayBeResizableOrGrowableSharedTypedArray)
            return false;
#endif

        insertChecks();
        NodeType op = mayBeLargeTypedArray ? GetTypedArrayLengthAsInt52 : GetArrayLength;
        set(result, addToGraph(op, OpInfo(ArrayMode(arrayType, Array::NonArray, Array::InBounds, Array::AsIs, Array::Read, mayBeLargeTypedArray, mayBeResizableOrGrowableSharedTypedArray).asWord()), thisNode));

        return true;
    }

    case TypedArrayByteOffsetIntrinsic: {
        bool mayBeLargeTypedArray = !isInt32Speculation(prediction) || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow);
#if !USE(LARGE_TYPED_ARRAYS)
        if (mayBeLargeTypedArray)
            return false;
#endif

        TypedArrayType type = typedArrayType((*variant.structureSet().begin())->typeInfo().type());
        Array::Type arrayType = toArrayType(type);
        bool mayBeResizableOrGrowableSharedTypedArray = m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, UnexpectedResizableArrayBufferView);

        variant.structureSet().forEach([&] (Structure* structure) {
            TypedArrayType curType = typedArrayType(structure->typeInfo().type());
            arrayType = refineTypedArrayType(arrayType, curType);
            mayBeResizableOrGrowableSharedTypedArray |= isResizableOrGrowableSharedTypedArrayIncludingDataView(structure->classInfoForCells());
            ASSERT(arrayType != Array::Generic);
        });


#if USE(JSVALUE32_64)
        if (mayBeResizableOrGrowableSharedTypedArray)
            return false;
#endif

        insertChecks();
        NodeType op = mayBeLargeTypedArray ? GetTypedArrayByteOffsetAsInt52 : GetTypedArrayByteOffset;
        set(result, addToGraph(op, OpInfo(ArrayMode(arrayType, Array::NonArray, Array::InBounds, Array::AsIs, Array::Read, mayBeLargeTypedArray, mayBeResizableOrGrowableSharedTypedArray).asWord()), thisNode));

        return true;
    }

    case UnderscoreProtoIntrinsic: {
        insertChecks();

        bool canFold = !variant.structureSet().isEmpty();
        JSValue prototype;
        variant.structureSet().forEach([&] (Structure* structure) {
            if (structure->typeInfo().overridesGetPrototype()) {
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

bool ByteCodeParser::handleDOMJITGetter(Operand result, const GetByVariant& variant, Node* thisNode, unsigned identifierNumber, SpeculatedType prediction)
{
    if (!variant.domAttribute())
        return false;

    auto* domAttribute = variant.domAttribute();

    // We do not need to actually look up CustomGetterSetter here. Checking Structures or registering watchpoints are enough,
    // since replacement of CustomGetterSetter always incurs Structure transition.
    if (!check(variant.conditionSet()))
        return false;
    addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.structureSet())), thisNode);
    
    // We do not need to emit CheckIsConstant thingy here. When the custom accessor is replaced to different one, Structure transition occurs.
    addToGraph(CheckJSCast, OpInfo(domAttribute->classInfo), thisNode);
    
    bool wasSeenInJIT = true;
    GetByStatus* status = m_graph.m_plan.recordedStatuses().addGetByStatus(currentCodeOrigin(), GetByStatus(GetByStatus::Custom, wasSeenInJIT));
    bool success = status->appendVariant(variant);
    RELEASE_ASSERT(success);
    addToGraph(FilterGetByStatus, OpInfo(status), thisNode);

    CallDOMGetterData* callDOMGetterData = m_graph.m_callDOMGetterData.add();
    callDOMGetterData->customAccessorGetter = variant.customAccessorGetter();
    ASSERT(callDOMGetterData->customAccessorGetter);
    callDOMGetterData->requiredClassInfo = domAttribute->classInfo;

    if (const auto* domJIT = domAttribute->domJIT) {
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

bool ByteCodeParser::handleModuleNamespaceLoad(VirtualRegister result, SpeculatedType prediction, Node* base, GetByStatus getById)
{
    if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue))
        return false;
    addToGraph(CheckIsConstant, OpInfo(m_graph.freeze(getById.moduleNamespaceObject())), Edge(base, CellUse));

    addToGraph(FilterGetByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByStatus(currentCodeOrigin(), getById)), base);

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
    Operand result, InternalFunction* function, int registerOffset,
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

    // Check both structures are already initialized.
    {
        constexpr bool isResizableOrGrowableShared = false;
        if (!function->globalObject()->typedArrayStructureConcurrently(type, isResizableOrGrowableShared))
            return false;
    }
    {
        constexpr bool isResizableOrGrowableShared = true;
        if (!function->globalObject()->typedArrayStructureConcurrently(type, isResizableOrGrowableShared))
            return false;
    }

    insertChecks();
    set(result,
        addToGraph(NewTypedArray, OpInfo(type), get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
    return true;
}

template<typename ChecksFunctor>
bool ByteCodeParser::handleConstantInternalFunction(
    Node* callTargetNode, Operand result, InternalFunction* function, int registerOffset,
    int argumentCountIncludingThis, CodeSpecializationKind kind, SpeculatedType prediction, const ChecksFunctor& insertChecks)
{
    VERBOSE_LOG("    Handling constant internal function ", JSValue(function), "\n");
    
    // It so happens that the code below assumes that the result operand is valid. It's extremely
    // unlikely that the result operand would be invalid - you'd have to call this via a setter call.
    if (!result.isValid())
        return false;

    if (kind == CodeForConstruct) {
        Node* newTargetNode = get(virtualRegisterForArgumentIncludingThis(0, registerOffset));
        // We cannot handle the case where new.target != callee (i.e. a construct from a super call) because we
        // don't know what the prototype of the constructed object will be.
        // FIXME: If we have inlined super calls up to the call site, however, we should be able to figure out the structure. https://bugs.webkit.org/show_bug.cgi?id=152700
        if (newTargetNode != callTargetNode)
            return false;
    }

    if (function->classInfo() == ArrayConstructor::info()) {
        if (function->globalObject() != m_inlineStackTop->m_codeBlock->globalObject())
            return false;
        
        insertChecks();
        if (argumentCountIncludingThis == 2) {
            set(result,
                addToGraph(NewArrayWithSize, OpInfo(ArrayWithUndecided), get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));
            return true;
        }
        
        for (int i = 1; i < argumentCountIncludingThis; ++i)
            addVarArgChild(get(virtualRegisterForArgumentIncludingThis(i, registerOffset)));
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
            set(result, addToGraph(CallNumberConstructor, OpInfo(0), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(1, registerOffset))));

        return true;
    }

    if (function->classInfo() == BooleanConstructor::info()) {
        if (kind == CodeForConstruct)
            return false;

        insertChecks();

        Node* resultNode;
        if (argumentCountIncludingThis <= 1)
            resultNode = jsConstant(jsBoolean(false));
        else
            resultNode = addToGraph(ToBoolean, get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));

        set(result, resultNode);
        return true;
    }
    
    if (function->classInfo() == StringConstructor::info()) {
        insertChecks();
        
        Node* resultNode;
        
        if (argumentCountIncludingThis <= 1)
            resultNode = jsConstant(m_vm->smallStrings.emptyString());
        else
            resultNode = addToGraph(CallStringConstructor, get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
        
        if (kind == CodeForConstruct)
            resultNode = addToGraph(NewStringObject, OpInfo(m_graph.registerStructure(function->globalObject()->stringObjectStructure())), resultNode);
        
        set(result, resultNode);
        return true;
    }

    if (function->classInfo() == SymbolConstructor::info() && kind == CodeForCall) {
        insertChecks();

        Node* resultNode;

        if (argumentCountIncludingThis <= 1)
            resultNode = addToGraph(NewSymbol);
        else
            resultNode = addToGraph(NewSymbol, get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));

        set(result, resultNode);
        return true;
    }

    if (function->classInfo() == ObjectConstructor::info()) {
        insertChecks();

        Node* resultNode;
        if (argumentCountIncludingThis <= 1)
            resultNode = addToGraph(NewObject, OpInfo(m_graph.registerStructure(function->globalObject()->objectStructureForObjectConstructor())));
        else
            resultNode = addToGraph(CallObjectConstructor, OpInfo(m_graph.freeze(function->globalObject())), OpInfo(prediction), get(virtualRegisterForArgumentIncludingThis(1, registerOffset)));
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
    SpeculatedType prediction, Node* base, unsigned identifierNumber, PropertyOffset offset, NodeType op)
{
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

Node* ByteCodeParser::handlePutByOffset(
    Node* base, unsigned identifier, PropertyOffset offset,
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
    
    Node* result = addToGraph(PutByOffset, OpInfo(data), propertyStorage, base, value);
    
    return result;
}

bool ByteCodeParser::check(const ObjectPropertyCondition& condition)
{
    if (!condition)
        return false;
    
    if (m_graph.watchCondition(condition))
        return true;
    
    Structure* structure = condition.object()->structure();
    if (!condition.structureEnsuresValidity(Concurrency::ConcurrentThread, structure))
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
    if (needsVarInjectionChecks(type) && globalObject->varInjectionWatchpointSet().hasBeenInvalidated())
        return true;

    switch (type) {
    case GlobalVar:
    case GlobalVarWithVarInjectionChecks: {
        if (opcode == op_put_to_scope && globalObject->varReadOnlyWatchpointSet().hasBeenInvalidated())
            return true;

        return false;
    }

    case GlobalProperty:    
    case GlobalLexicalVar:
    case ClosureVar:
    case ResolvedClosureVar:
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
    ObjectPropertyCondition equivalenceCondition = condition.attemptToMakeEquivalenceWithoutBarrier();
    if (m_graph.watchCondition(equivalenceCondition))
        return GetByOffsetMethod::constant(m_graph.freeze(equivalenceCondition.requiredValue()));
    
    // At this point, we'll have to materialize the condition's base as a constant in DFG IR. Once
    // we do this, the frozen value will have its own idea of what the structure is. Use that from
    // now on just because it's less confusing.
    FrozenValue* base = m_graph.freeze(condition.object());
    Structure* structure = base->structure();
    
    // Check if the structure that we've registered makes the condition hold. If not, just give
    // up. This is case (5) above.
    if (!condition.structureEnsuresValidity(Concurrency::ConcurrentThread, structure))
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
            prediction, baseNode, identifierNumber, method.offset(), op);
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

ObjectPropertyCondition ByteCodeParser::presenceConditionIfConsistent(JSObject* knownBase, UniquedStringImpl* uid, PropertyOffset offset, const StructureSet& set)
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

void ByteCodeParser::checkReplacement(
    Node* base, UniquedStringImpl* uid, PropertyOffset offset, const StructureSet& set)
{
    if (JSObject* knownBase = base->dynamicCastConstant<JSObject*>()) {
        auto condition = presenceConditionIfConsistent(knownBase, uid, offset, set);
        if (condition) {
            auto replacementCondition = condition.attemptToMakeReplacementWithoutBarrier();
            if (check(replacementCondition))
                return;
        }
    }

#if ASSERT_ENABLED
    for (auto structure : set)
        ASSERT(!structure->propertyReplacementWatchpointSet(offset)->isStillValid());
#endif

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
    
    if (JSObject* knownBase = base->dynamicCastConstant<JSObject*>()) {
        // Try to optimize away the structure check. Note that it's not worth doing anything about this
        // if the base's structure is watched.
        Structure* structure = base->constant()->structure();
        if (!structure->dfgShouldWatch()) {
            if (!variant.conditionSet().isEmpty()) {
                // This means that we're loading from a prototype or we have a property miss. We expect
                // the base not to have the property. We can only use ObjectPropertyCondition if all of
                // the structures in the variant.structureSet() agree on the prototype (it would be
                // hilariously rare if they didn't). Note that we are relying on structureSet() having
                // at least one element. That will always be true here because of how GetByStatus/PutByStatus work.

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
                    presenceConditionIfConsistent(knownBase, uid, variant.offset(), variant.structureSet());
                if (presenceCondition) {
                    ObjectPropertyCondition equivalenceCondition =
                        presenceCondition.attemptToMakeEquivalenceWithoutBarrier();
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

        loadedValue = handleGetByOffset(
            loadPrediction, base, identifierNumber, variant.offset(), loadOp);
    }

    return loadedValue;
}

Node* ByteCodeParser::replace(Node* base, unsigned identifier, const PutByVariant& variant, Node* value)
{
    RELEASE_ASSERT(variant.kind() == PutByVariant::Replace);

    checkReplacement(base, m_graph.identifiers()[identifier], variant.offset(), variant.structure());
    return handlePutByOffset(base, identifier, variant.offset(), value);
}

void ByteCodeParser::simplifyGetByStatus(Node* base, GetByStatus& getByStatus)
{
    // Attempt to reduce the set of things in the GetByStatus.
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
            getByStatus.filter(base->structure().get());
    }
}

void ByteCodeParser::handleGetById(
    VirtualRegister destination, SpeculatedType prediction, Node* base, CacheableIdentifier identifier, unsigned identifierNumber,
    GetByStatus getByStatus, AccessType type, BytecodeIndex osrExitIndex)
{
    simplifyGetByStatus(base, getByStatus);
    
    NodeType getById;
    if (type == AccessType::GetById)
        getById = getByStatus.makesCalls() ? GetByIdFlush : GetById;
    else if (type == AccessType::TryGetById)
        getById = TryGetById;
    else
        getById = getByStatus.makesCalls() ? GetByIdDirectFlush : GetByIdDirect;

    if (getById != TryGetById && getByStatus.isModuleNamespace()) {
        if (handleModuleNamespaceLoad(destination, prediction, base, getByStatus)) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedGetById();
            return;
        }
    }

    // Special path for custom accessors since custom's offset does not have any meanings.
    // So, this is completely different from Simple one. But we have a chance to optimize it when we use DOMJIT.
    if (Options::useDOMJIT() && getByStatus.isCustom()) {
        ASSERT(getByStatus.numVariants() == 1);
        ASSERT(!getByStatus.makesCalls());
        GetByVariant variant = getByStatus[0];
        ASSERT(variant.domAttribute());
        if (handleDOMJITGetter(destination, variant, base, identifierNumber, prediction)) {
            if (UNLIKELY(m_graph.compilation()))
                m_graph.compilation()->noticeInlinedGetById();
            return;
        }
    }

    ASSERT(type == AccessType::GetById || type == AccessType::GetByIdDirect ||  !getByStatus.makesCalls());
    if (!getByStatus.isSimple() || !getByStatus.numVariants() || !Options::useAccessInlining()) {
        set(destination,
            addToGraph(getById, OpInfo(identifier), OpInfo(prediction), base));
        return;
    }
    
    // FIXME: If we use the GetByStatus for anything then we should record it and insert a node
    // after everything else (like the GetByOffset or whatever) that will filter the recorded
    // GetByStatus. That means that the constant folder also needs to do the same!
    
    if (getByStatus.numVariants() > 1) {
        if (getByStatus.makesCalls() || !m_graph.m_plan.isFTL()
            || !Options::usePolymorphicAccessInlining()
            || getByStatus.numVariants() > Options::maxPolymorphicAccessInliningListSize()) {
            set(destination,
                addToGraph(getById, OpInfo(identifier), OpInfo(prediction), base));
            return;
        }

        addToGraph(FilterGetByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByStatus(currentCodeOrigin(), getByStatus)), base);

        Vector<MultiGetByOffsetCase, 2> cases;
        
        // 1) Emit prototype structure checks for all chains. This could sort of maybe not be
        //    optimal, if there is some rarely executed case in the chain that requires a lot
        //    of checks and those checks are not watchpointable.
        for (const GetByVariant& variant : getByStatus.variants()) {
            if (variant.intrinsic() != NoIntrinsic) {
                set(destination,
                    addToGraph(getById, OpInfo(identifier), OpInfo(prediction), base));
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
                    addToGraph(getById, OpInfo(identifier), OpInfo(prediction), base));
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

    addToGraph(FilterGetByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByStatus(currentCodeOrigin(), getByStatus)), base);

    ASSERT(getByStatus.numVariants() == 1);
    GetByVariant variant = getByStatus[0];
    
    Node* loadedValue = load(prediction, base, identifierNumber, variant);
    if (!loadedValue) {
        set(destination,
            addToGraph(getById, OpInfo(identifier), OpInfo(prediction), base));
        return;
    }

    if (UNLIKELY(m_graph.compilation()))
        m_graph.compilation()->noticeInlinedGetById();

    ASSERT(type == AccessType::GetById || type == AccessType::GetByIdDirect || !variant.callLinkStatus());
    if (!variant.callLinkStatus() && variant.intrinsic() == NoIntrinsic) {
        set(destination, loadedValue);
        return;
    }
    
    Node* getter = addToGraph(GetGetter, loadedValue);

    if (handleIntrinsicGetter(destination, prediction, variant, base,
            [&] () {
                addToGraph(CheckIsConstant, OpInfo(m_graph.freeze(variant.intrinsicFunction())), getter);
            })) {
        addToGraph(Phantom, base);
        return;
    }

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
    set(virtualRegisterForArgumentIncludingThis(0, registerOffset), base, ImmediateNakedSet);

    // We've set some locals, but they are not user-visible. It's still OK to exit from here.
    m_exitOK = true;
    addToGraph(ExitOK);
    
    handleCall(
        destination, Call, InlineCallFrame::GetterCall, osrExitIndex,
        getter, numberOfParameters - 1, registerOffset, *variant.callLinkStatus(), prediction);
}

// A variant on handleGetById which is more limited in scope
void ByteCodeParser::handleGetPrivateNameById(
    VirtualRegister destination, SpeculatedType prediction, Node* base, CacheableIdentifier identifier, unsigned identifierNumber, GetByStatus getByStatus)
{
    simplifyGetByStatus(base, getByStatus);

    ASSERT(!getByStatus.isCustom());
    ASSERT(!getByStatus.makesCalls());
    if (!getByStatus.isSimple() || !getByStatus.numVariants() || !Options::useAccessInlining()) {
        set(destination,
            addToGraph(GetPrivateNameById, OpInfo(identifier), OpInfo(prediction), base, nullptr));
        return;
    }

    if (getByStatus.numVariants() > 1) {
        if (!m_graph.m_plan.isFTL()
            || !Options::usePolymorphicAccessInlining()
            || getByStatus.numVariants() > Options::maxPolymorphicAccessInliningListSize()) {
            set(destination,
                addToGraph(GetPrivateNameById, OpInfo(identifier), OpInfo(prediction), base, nullptr));
            return;
        }

        addToGraph(FilterGetByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByStatus(currentCodeOrigin(), getByStatus)), base);

        Vector<MultiGetByOffsetCase, 2> cases;

        for (const GetByVariant& variant : getByStatus.variants()) {
            ASSERT(variant.intrinsic() == NoIntrinsic);
            ASSERT(variant.conditionSet().isEmpty());

            GetByOffsetMethod method = GetByOffsetMethod::load(variant.offset());
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

    // FIXME: If we use the GetByStatus for anything then we should record it and insert a node
    // after everything else (like the GetByOffset or whatever) that will filter the recorded
    // GetByStatus. That means that the constant folder also needs to do the same!
    addToGraph(FilterGetByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addGetByStatus(currentCodeOrigin(), getByStatus)), base);

    ASSERT(getByStatus.numVariants() == 1);
    GetByVariant variant = getByStatus[0];

    Node* loadedValue = load(prediction, base, identifierNumber, variant);
    if (!loadedValue) {
        set(destination,
            addToGraph(GetPrivateNameById, OpInfo(identifier), OpInfo(prediction), base, nullptr));
        return;
    }

    if (UNLIKELY(m_graph.compilation()))
        m_graph.compilation()->noticeInlinedGetById();

    ASSERT(!variant.callLinkStatus());
    if (variant.intrinsic() == NoIntrinsic)
        set(destination, loadedValue);
}

void ByteCodeParser::handleDeleteById(
    VirtualRegister destination, Node* base, CacheableIdentifier identifier,
    unsigned identifierNumber, DeleteByStatus deleteByStatus, ECMAMode ecmaMode)
{
    if (!deleteByStatus.isSimple() || !deleteByStatus.variants().size() || !Options::useAccessInlining()) {
        set(destination,
            addToGraph(DeleteById, OpInfo(identifier), OpInfo(ecmaMode), base));
        return;
    }

    if (deleteByStatus.variants().size() > 1) {
        if (!m_graph.m_plan.isFTL()
            || !Options::usePolymorphicAccessInlining()
            || deleteByStatus.variants().size() > Options::maxPolymorphicAccessInliningListSize()) {
            set(destination,
                addToGraph(DeleteById, OpInfo(identifier), OpInfo(ecmaMode), base));
            return;
        }

        addToGraph(FilterDeleteByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addDeleteByStatus(currentCodeOrigin(), deleteByStatus)), base);

        bool hasHit = false;
        bool hasMiss = false;
        bool hasMissNonconfigurable = false;

        for (const DeleteByVariant& variant : deleteByStatus.variants()) {
            m_graph.registerStructure(variant.oldStructure());
            if (variant.newStructure()) {
                m_graph.registerStructure(variant.newStructure());
                hasHit = true;
            } else if (variant.result())
                hasMiss = true;
            else
                hasMissNonconfigurable = true;
        }

        if (!hasHit) {
            if ((hasMiss && !hasMissNonconfigurable) || (!hasMiss && hasMissNonconfigurable)) {
                StructureSet baseSet;

                for (const DeleteByVariant& variant : deleteByStatus.variants())
                    baseSet.add(variant.oldStructure());

                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(baseSet)), base);
                set(destination, jsConstant(jsBoolean(deleteByStatus.variants()[0].result())));
                return;
            }
        }

        MultiDeleteByOffsetData* data = m_graph.m_multiDeleteByOffsetData.add();
        data->variants = deleteByStatus.variants();
        data->identifierNumber = identifierNumber;
        set(destination,
            addToGraph(MultiDeleteByOffset, OpInfo(data), base));
        return;
    }

    ASSERT(deleteByStatus.variants().size() == 1);
    DeleteByVariant variant = deleteByStatus.variants()[0];

    if (!variant.newStructure()) {
        addToGraph(FilterDeleteByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addDeleteByStatus(currentCodeOrigin(), deleteByStatus)), base);
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.oldStructure())), base);
        set(destination, jsConstant(jsBoolean(variant.result())));
        return;
    }

    addToGraph(FilterDeleteByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addDeleteByStatus(currentCodeOrigin(), deleteByStatus)), base);
    addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.oldStructure())), base);
    ASSERT(variant.oldStructure()->transitionWatchpointSetHasBeenInvalidated());
    ASSERT(variant.newStructure());
    ASSERT(isValidOffset(variant.offset()));

    Node* propertyStorage;
    Transition* transition = m_graph.m_transitions.add(
        m_graph.registerStructure(variant.oldStructure()), m_graph.registerStructure(variant.newStructure()));

    if (isInlineOffset(variant.offset()))
        propertyStorage = base;
    else
        propertyStorage = addToGraph(GetButterfly, base);

    StorageAccessData* data = m_graph.m_storageAccessData.add();
    data->offset = variant.offset();
    data->identifierNumber = identifierNumber;

    addToGraph(
        PutByOffset,
        OpInfo(data),
        propertyStorage,
        base,
        jsConstant(JSValue()));

    addToGraph(PutStructure, OpInfo(transition), base);
    set(destination, jsConstant(jsBoolean(variant.result())));
    return;
}

bool ByteCodeParser::handleInByAsMatchStructure(VirtualRegister destination, Node* base, InByStatus status)
{
    if (!status.isSimple() || !Options::useAccessInlining())
        return false;

    bool allOK = true;
    MatchStructureData* data = m_graph.m_matchStructureData.add();
    for (const InByVariant& variant : status.variants()) {
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
        addToGraph(FilterInByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addInByStatus(currentCodeOrigin(), status)), base);
        set(destination, addToGraph(MatchStructure, OpInfo(data), base));
    }

    return allOK;
}

void ByteCodeParser::handleInById(VirtualRegister destination, Node* base, CacheableIdentifier identifier, InByStatus status)
{
    if (handleInByAsMatchStructure(destination, base, status))
        return;

    set(destination, addToGraph(InById, OpInfo(identifier), base));
}

void ByteCodeParser::emitPutById(
    Node* base, CacheableIdentifier identifier, Node* value, const PutByStatus& putByStatus, bool isDirect, ECMAMode ecmaMode)
{
    if (isDirect)
        addToGraph(PutByIdDirect, OpInfo(identifier), OpInfo(ecmaMode), base, value);
    else
        addToGraph(putByStatus.makesCalls() ? PutByIdFlush : PutById, OpInfo(identifier), OpInfo(ecmaMode), base, value);
}

void ByteCodeParser::handlePutById(
    Node* base, CacheableIdentifier identifier, unsigned identifierNumber, Node* value,
    const PutByStatus& putByStatus, bool isDirect, BytecodeIndex osrExitIndex, ECMAMode ecmaMode)
{
    if (!putByStatus.isSimple() || !putByStatus.numVariants() || !Options::useAccessInlining()) {
        if (!putByStatus.isSet())
            addToGraph(ForceOSRExit);
        emitPutById(base, identifier, value, putByStatus, isDirect, ecmaMode);
        return;
    }
    
    if (putByStatus.numVariants() > 1) {
        if (!m_graph.m_plan.isFTL() || putByStatus.makesCalls()
            || !Options::usePolymorphicAccessInlining()
            || putByStatus.numVariants() > Options::maxPolymorphicAccessInliningListSize()) {
            emitPutById(base, identifier, value, putByStatus, isDirect, ecmaMode);
            return;
        }
        
        if (!isDirect) {
            for (unsigned variantIndex = putByStatus.numVariants(); variantIndex--;) {
                if (putByStatus[variantIndex].kind() != PutByVariant::Transition)
                    continue;
                if (!check(putByStatus[variantIndex].conditionSet())) {
                    emitPutById(base, identifier, value, putByStatus, isDirect, ecmaMode);
                    return;
                }
            }
        }
        
        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedPutById();

        addToGraph(FilterPutByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByStatus(currentCodeOrigin(), putByStatus)), base);

        for (const PutByVariant& variant : putByStatus.variants()) {
            for (Structure* structure : variant.oldStructure())
                m_graph.registerStructure(structure);
            if (variant.kind() == PutByVariant::Transition)
                m_graph.registerStructure(variant.newStructure());
        }
        
        MultiPutByOffsetData* data = m_graph.m_multiPutByOffsetData.add();
        data->variants = putByStatus.variants();
        data->identifierNumber = identifierNumber;
        addToGraph(MultiPutByOffset, OpInfo(data), base, value);
        return;
    }
    
    ASSERT(putByStatus.numVariants() == 1);
    const PutByVariant& variant = putByStatus[0];
    
    switch (variant.kind()) {
    case PutByVariant::Replace: {
        addToGraph(FilterPutByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByStatus(currentCodeOrigin(), putByStatus)), base);

        replace(base, identifierNumber, variant, value);
        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedPutById();
        return;
    }
    
    case PutByVariant::Transition: {
        addToGraph(FilterPutByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByStatus(currentCodeOrigin(), putByStatus)), base);

        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.oldStructure())), base);
        if (!check(variant.conditionSet())) {
            emitPutById(base, identifier, value, putByStatus, isDirect, ecmaMode);
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
        
    case PutByVariant::Setter: {
        addToGraph(FilterPutByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByStatus(currentCodeOrigin(), putByStatus)), base);

        Node* loadedValue = load(SpecCellOther, base, identifierNumber, variant);
        if (!loadedValue) {
            emitPutById(base, identifier, value, putByStatus, isDirect, ecmaMode);
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
    
        set(virtualRegisterForArgumentIncludingThis(0, registerOffset), base, ImmediateNakedSet);
        set(virtualRegisterForArgumentIncludingThis(1, registerOffset), value, ImmediateNakedSet);

        // We've set some locals, but they are not user-visible. It's still OK to exit from here.
        m_exitOK = true;
        addToGraph(ExitOK);
    
        handleCall(
            VirtualRegister(), Call, InlineCallFrame::SetterCall,
            osrExitIndex, setter, numberOfParameters - 1, registerOffset,
            *variant.callLinkStatus(), SpecOther, ecmaMode);
        return;
    }
    
    default: {
        emitPutById(base, identifier, value, putByStatus, isDirect, ecmaMode);
        return;
    } }
}

void ByteCodeParser::handlePutPrivateNameById(
    Node* base, CacheableIdentifier identifier, unsigned identifierNumber, Node* value,
    const PutByStatus& putByStatus, PrivateFieldPutKind privateFieldPutKind)
{
    if (!putByStatus.isSimple() || !putByStatus.numVariants() || !Options::useAccessInlining()) {
        if (!putByStatus.isSet())
            addToGraph(ForceOSRExit);
        addToGraph(PutPrivateNameById, OpInfo(identifier), OpInfo(privateFieldPutKind), base, value);
        return;
    }
    
    if (putByStatus.numVariants() > 1) {
        if (!m_graph.m_plan.isFTL() || putByStatus.makesCalls()
            || !Options::usePolymorphicAccessInlining()
            || putByStatus.numVariants() > Options::maxPolymorphicAccessInliningListSize()) {
            addToGraph(PutPrivateNameById, OpInfo(identifier), OpInfo(privateFieldPutKind), base, value);
            return;
        }
        
        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedPutById();
    
        addToGraph(FilterPutByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByStatus(currentCodeOrigin(), putByStatus)), base);
    
        for (const PutByVariant& variant : putByStatus.variants()) {
            for (Structure* structure : variant.oldStructure())
                m_graph.registerStructure(structure);
            if (variant.kind() == PutByVariant::Transition)
                m_graph.registerStructure(variant.newStructure());
        }
        
        MultiPutByOffsetData* data = m_graph.m_multiPutByOffsetData.add();
        data->variants = putByStatus.variants();
        data->identifierNumber = identifierNumber;
        addToGraph(MultiPutByOffset, OpInfo(data), base, value);
        return;
    }
    
    ASSERT(putByStatus.numVariants() == 1);
    const PutByVariant& variant = putByStatus[0];
    
    switch (variant.kind()) {
    case PutByVariant::Replace: {
        ASSERT(privateFieldPutKind.isSet());
        addToGraph(FilterPutByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByStatus(currentCodeOrigin(), putByStatus)), base);
    
        replace(base, identifierNumber, variant, value);
        if (UNLIKELY(m_graph.compilation()))
            m_graph.compilation()->noticeInlinedPutById();
        return;
    }
    
    case PutByVariant::Transition: {
        ASSERT(privateFieldPutKind.isDefine());
        addToGraph(FilterPutByStatus, OpInfo(m_graph.m_plan.recordedStatuses().addPutByStatus(currentCodeOrigin(), putByStatus)), base);
    
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.oldStructure())), base);
        if (!check(variant.conditionSet())) {
            addToGraph(PutPrivateNameById, OpInfo(identifier), OpInfo(privateFieldPutKind), base, value);
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
    
    default: {
        RELEASE_ASSERT_NOT_REACHED();
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
void ByteCodeParser::parseGetById(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    SpeculatedType prediction = getPrediction();
    
    Node* base = get(bytecode.m_base);
    unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
    UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
    
    AccessType type = AccessType::GetById;
    if (Op::opcodeID == op_try_get_by_id)
        type = AccessType::TryGetById;
    else if (Op::opcodeID == op_get_by_id_direct)
        type = AccessType::GetByIdDirect;
    
    GetByStatus getByStatus = GetByStatus::computeFor(
        m_inlineStackTop->m_profiledBlock,
        m_inlineStackTop->m_baselineMap, m_icContextStack,
        currentCodeOrigin());

    handleGetById(bytecode.m_dst, prediction, base, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid), identifierNumber, getByStatus, type, nextOpcodeIndex());
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
        m_currentIndex = BytecodeIndex(m_currentIndex.offset() + currentInstruction->size()); \
        goto WTF_CONCAT(NEXT_OPCODE_, __LINE__); /* Need a unique label: usable more than once per function. */ \
    } else \
        WTF_CONCAT(NEXT_OPCODE_, __LINE__): \
    continue

#define LAST_OPCODE_LINKED(name) do { \
        m_currentIndex = BytecodeIndex(m_currentIndex.offset() + currentInstruction->size()); \
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
    BytecodeIndex blockBegin = m_currentIndex;

    // If we are the first basic block, introduce markers for arguments. This allows
    // us to track if a use of an argument may use the actual argument passed, as
    // opposed to using a value we set explicitly.
    if (m_currentBlock == m_graph.block(0) && !inlineCallFrame()) {
        auto addResult = m_graph.m_rootToArguments.add(m_currentBlock, ArgumentsVector());
        RELEASE_ASSERT(addResult.isNewEntry);
        ArgumentsVector& entrypointArguments = addResult.iterator->value;
        entrypointArguments.resize(m_numArguments);

        // We will emit SetArgumentDefinitely nodes. They don't exit, but we're at the top of an op_enter so
        // exitOK = true.
        m_exitOK = true;
        for (unsigned argument = 0; argument < m_numArguments; ++argument) {
            VariableAccessData* variable = newVariableAccessData(
                virtualRegisterForArgumentIncludingThis(argument));
            variable->mergeStructureCheckHoistingFailed(
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache));
            variable->mergeCheckArrayHoistingFailed(
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType));
            
            Node* setArgument = addToGraph(SetArgumentDefinitely, OpInfo(variable));
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
        if (m_currentIndex.offset() == limit) {
            // Ordinarily we want to plant a jump. But refuse to do this if the block is
            // empty. This is a special case for inlining, which might otherwise create
            // some empty blocks in some cases. When parseBlock() returns with an empty
            // block, it will get repurposed instead of creating a new one. Note that this
            // logic relies on every bytecode resulting in one or more nodes, which would
            // be true anyway except for op_loop_hint, which emits a Phantom to force this
            // to be true.

            if (!m_currentBlock->isEmpty())
                addJumpTo(m_currentIndex.offset());
            return;
        }

        // Switch on the current bytecode opcode.
        const JSInstruction* currentInstruction = instructions.at(m_currentIndex).ptr();
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
            for (unsigned i = 0; i < m_inlineStackTop->m_codeBlock->numVars(); ++i)
                set(virtualRegisterForLocal(i), undefined, ImmediateNakedSet);

            NEXT_OPCODE(op_enter);
        }
            
        case op_to_this: {
            Node* op1 = getThis();
            auto bytecode = currentInstruction->as<OpToThis>();
            auto& metadata = bytecode.metadata(codeBlock);
            StructureID cachedStructureID = metadata.m_cachedStructureID;
            Structure* cachedStructure = nullptr;
            if (cachedStructureID)
                cachedStructure = cachedStructureID.decode();
            if (metadata.m_toThisStatus != ToThisOK
                || !cachedStructure
                || !cachedStructure->classInfoForCells()->isSubClassOf(JSObject::info())
                || cachedStructure->classInfoForCells()->isSubClassOf(JSScope::info())
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
                || (op1->op() == GetLocal && op1->variableAccessData()->structureCheckHoistingFailed())) {
                setThis(addToGraph(ToThis, OpInfo(bytecode.m_ecmaMode), OpInfo(getPrediction()), op1));
            } else {
                addToGraph(
                    CheckStructure,
                    OpInfo(m_graph.addStructureSet(cachedStructure)),
                    op1);
            }
            NEXT_OPCODE(op_to_this);
        }

        case op_create_this: {
            auto bytecode = currentInstruction->as<OpCreateThis>();
            Node* callee = get(VirtualRegister(bytecode.m_callee));

            JSFunction* function = callee->dynamicCastConstant<JSFunction*>();
            if (!function) {
                JSCell* cachedFunction = bytecode.metadata(codeBlock).m_cachedCallee.unvalidatedGet();
                if (cachedFunction
                    && cachedFunction != JSCell::seenMultipleCalleeObjects()
                    && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {
                    ASSERT(cachedFunction->inherits<JSFunction>());

                    FrozenValue* frozen = m_graph.freeze(cachedFunction);
                    addToGraph(CheckIsConstant, OpInfo(frozen), callee);

                    function = static_cast<JSFunction*>(cachedFunction);
                }
            }

            bool alreadyEmitted = false;
            if (function) {
                if (FunctionRareData* rareData = function->rareData()) {
                    JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);
                    if (rareData->allocationProfileWatchpointSet().isStillValid() && globalObject->structureCacheClearedWatchpointSet().isStillValid()) {
                        Structure* structure = rareData->objectAllocationStructure();
                        JSObject* prototype = rareData->objectAllocationPrototype();
                        if (structure
                            && (structure->hasMonoProto() || prototype)) {

                            m_graph.freeze(rareData);
                            m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                            m_graph.freeze(globalObject);
                            m_graph.watchpoints().addLazily(globalObject->structureCacheClearedWatchpointSet());
                            
                            Node* object = addToGraph(NewObject, OpInfo(m_graph.registerStructure(structure)));
                            if (structure->hasPolyProto()) {
                                StorageAccessData* data = m_graph.m_storageAccessData.add();
                                data->offset = knownPolyProtoOffset;
                                data->identifierNumber = m_graph.identifiers().ensure(m_graph.m_vm.propertyNames->builtinNames().polyProtoName().impl());
                                ASSERT(isInlineOffset(knownPolyProtoOffset));
                                addToGraph(PutByOffset, OpInfo(data), object, object, weakJSConstant(prototype));
                            }
                            set(VirtualRegister(bytecode.m_dst), object);
                            // The callee is still live up to this point.
                            addToGraph(Phantom, callee);
                            alreadyEmitted = true;
                        }
                    }
                }
            }
            if (!alreadyEmitted) {
                set(VirtualRegister(bytecode.m_dst),
                    addToGraph(CreateThis, OpInfo(bytecode.m_inlineCapacity), callee));
            }
            NEXT_OPCODE(op_create_this);
        }

        case op_create_promise: {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);
            auto bytecode = currentInstruction->as<OpCreatePromise>();
            Node* callee = get(VirtualRegister(bytecode.m_callee));

            bool alreadyEmitted = false;

            {
                // Attempt to convert to NewPromise first in easy case.
                JSPromiseConstructor* promiseConstructor = callee->dynamicCastConstant<JSPromiseConstructor*>();
                if (promiseConstructor == (bytecode.m_isInternalPromise ? globalObject->internalPromiseConstructor() : globalObject->promiseConstructor())) {
                    JSCell* cachedFunction = bytecode.metadata(codeBlock).m_cachedCallee.unvalidatedGet();
                    if (cachedFunction
                        && cachedFunction != JSCell::seenMultipleCalleeObjects()
                        && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)
                        && cachedFunction == (bytecode.m_isInternalPromise ? globalObject->internalPromiseConstructor() : globalObject->promiseConstructor())) {
                        FrozenValue* frozen = m_graph.freeze(cachedFunction);
                        addToGraph(CheckIsConstant, OpInfo(frozen), callee);

                        promiseConstructor = jsCast<JSPromiseConstructor*>(cachedFunction);
                    }
                }
                if (promiseConstructor) {
                    addToGraph(Phantom, callee);
                    Node* promise = addToGraph(NewInternalFieldObject, OpInfo(m_graph.registerStructure(bytecode.m_isInternalPromise ? globalObject->internalPromiseStructure() : globalObject->promiseStructure())));
                    set(VirtualRegister(bytecode.m_dst), promise);
                    alreadyEmitted = true;
                }
            }

            // Derived function case.
            if (!alreadyEmitted) {
                JSFunction* function = callee->dynamicCastConstant<JSFunction*>();
                if (!function) {
                    JSCell* cachedFunction = bytecode.metadata(codeBlock).m_cachedCallee.unvalidatedGet();
                    if (cachedFunction
                        && cachedFunction != JSCell::seenMultipleCalleeObjects()
                        && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {
                        ASSERT(cachedFunction->inherits<JSFunction>());

                        FrozenValue* frozen = m_graph.freeze(cachedFunction);
                        addToGraph(CheckIsConstant, OpInfo(frozen), callee);

                        function = static_cast<JSFunction*>(cachedFunction);
                    }
                }

                if (function) {
                    if (FunctionRareData* rareData = function->rareData()) {
                        if (rareData->allocationProfileWatchpointSet().isStillValid() && globalObject->structureCacheClearedWatchpointSet().isStillValid()) {
                            Structure* structure = rareData->internalFunctionAllocationStructure();
                            if (structure
                                && structure->classInfoForCells() == (bytecode.m_isInternalPromise ? JSInternalPromise::info() : JSPromise::info())
                                && structure->globalObject() == globalObject) {
                                m_graph.freeze(rareData);
                                m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                                m_graph.freeze(globalObject);
                                m_graph.watchpoints().addLazily(globalObject->structureCacheClearedWatchpointSet());

                                Node* promise = addToGraph(NewInternalFieldObject, OpInfo(m_graph.registerStructure(structure)));
                                set(VirtualRegister(bytecode.m_dst), promise);
                                // The callee is still live up to this point.
                                addToGraph(Phantom, callee);
                                alreadyEmitted = true;
                            }
                        }
                    }
                }
                if (!alreadyEmitted)
                    set(VirtualRegister(bytecode.m_dst), addToGraph(CreatePromise, OpInfo(), OpInfo(bytecode.m_isInternalPromise), callee));
            }
            NEXT_OPCODE(op_create_promise);
        }

        case op_create_generator: {
            handleCreateInternalFieldObject(JSGenerator::info(), CreateGenerator, NewGenerator, currentInstruction->as<OpCreateGenerator>());
            NEXT_OPCODE(op_create_generator);
        }

        case op_create_async_generator: {
            handleCreateInternalFieldObject(JSAsyncGenerator::info(), CreateAsyncGenerator, NewAsyncGenerator, currentInstruction->as<OpCreateAsyncGenerator>());
            NEXT_OPCODE(op_create_async_generator);
        }

        case op_new_object: {
            auto bytecode = currentInstruction->as<OpNewObject>();
            set(bytecode.m_dst,
                addToGraph(NewObject,
                    OpInfo(m_graph.registerStructure(bytecode.metadata(codeBlock).m_objectAllocationProfile.structure()))));
            NEXT_OPCODE(op_new_object);
        }

        case op_new_promise: {
            auto bytecode = currentInstruction->as<OpNewPromise>();
            JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);
            Node* promise = addToGraph(NewInternalFieldObject, OpInfo(m_graph.registerStructure(bytecode.m_isInternalPromise ? globalObject->internalPromiseStructure() : globalObject->promiseStructure())));
            set(bytecode.m_dst, promise);
            NEXT_OPCODE(op_new_promise);
        }

        case op_new_generator: {
            auto bytecode = currentInstruction->as<OpNewGenerator>();
            JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);
            set(bytecode.m_dst, addToGraph(NewGenerator, OpInfo(m_graph.registerStructure(globalObject->generatorStructure()))));
            NEXT_OPCODE(op_new_generator);
        }
            
        case op_new_array: {
            auto bytecode = currentInstruction->as<OpNewArray>();
            int startOperand = bytecode.m_argv.offset();
            int numOperands = bytecode.m_argc;
            ArrayAllocationProfile& profile = bytecode.metadata(codeBlock).m_arrayAllocationProfile;
            for (int operandIdx = startOperand; operandIdx > startOperand - numOperands; --operandIdx)
                addVarArgChild(get(VirtualRegister(operandIdx)));
            unsigned vectorLengthHint = std::max<unsigned>(profile.vectorLengthHintConcurrently(), numOperands);
            set(bytecode.m_dst, addToGraph(Node::VarArg, NewArray, OpInfo(profile.selectIndexingTypeConcurrently()), OpInfo(vectorLengthHint)));
            NEXT_OPCODE(op_new_array);
        }

        case op_new_array_with_spread: {
            auto bytecode = currentInstruction->as<OpNewArrayWithSpread>();
            int startOperand = bytecode.m_argv.offset();
            int numOperands = bytecode.m_argc;
            const BitVector& bitVector = m_inlineStackTop->m_profiledBlock->unlinkedCodeBlock()->bitVector(bytecode.m_bitVector);
            for (int operandIdx = startOperand; operandIdx > startOperand - numOperands; --operandIdx)
                addVarArgChild(get(VirtualRegister(operandIdx)));

            BitVector* copy = m_graph.m_bitVectors.add(bitVector);
            ASSERT(*copy == bitVector);

            set(bytecode.m_dst,
                addToGraph(Node::VarArg, NewArrayWithSpread, OpInfo(copy)));
            NEXT_OPCODE(op_new_array_with_spread);
        }

        case op_spread: {
            auto bytecode = currentInstruction->as<OpSpread>();
            set(bytecode.m_dst,
                addToGraph(Spread, get(bytecode.m_argument)));
            NEXT_OPCODE(op_spread);
        }
            
        case op_new_array_with_size: {
            auto bytecode = currentInstruction->as<OpNewArrayWithSize>();
            ArrayAllocationProfile& profile = bytecode.metadata(codeBlock).m_arrayAllocationProfile;
            set(bytecode.m_dst, addToGraph(NewArrayWithSize, OpInfo(profile.selectIndexingTypeConcurrently()), get(bytecode.m_length)));
            NEXT_OPCODE(op_new_array_with_size);
        }

        case op_new_array_with_species: {
            auto bytecode = currentInstruction->as<OpNewArrayWithSpecies>();
            SpeculatedType prediction = getPrediction();
            auto& metadata = bytecode.metadata(codeBlock);
            ArrayAllocationProfile& profile = metadata.m_arrayAllocationProfile;
            ArrayMode arrayMode = getArrayMode(metadata.m_arrayProfile, Array::Read);
            NewArrayWithSpeciesData data { };
            data.arrayMode = arrayMode.asWord();
            data.indexingMode = profile.selectIndexingTypeConcurrently();
            set(bytecode.m_dst, addToGraph(NewArrayWithSpecies, OpInfo(data.asQuadWord()), OpInfo(prediction), Edge(get(bytecode.m_length)), Edge(get(bytecode.m_array), KnownCellUse)));
            NEXT_OPCODE(op_new_array_with_species);
        }

        case op_new_array_buffer: {
            auto bytecode = currentInstruction->as<OpNewArrayBuffer>();
            // Unfortunately, we can't allocate a new JSImmutableButterfly if the profile tells us new information because we
            // cannot allocate from compilation threads.
            FrozenValue* frozen = get(VirtualRegister(bytecode.m_immutableButterfly))->constant();
            WTF::dependentLoadLoadFence();
            JSImmutableButterfly* immutableButterfly = frozen->cast<JSImmutableButterfly*>();
            NewArrayBufferData data { };
            data.indexingMode = immutableButterfly->indexingMode();
            data.vectorLengthHint = immutableButterfly->toButterfly()->vectorLength();

            set(VirtualRegister(bytecode.m_dst), addToGraph(NewArrayBuffer, OpInfo(frozen), OpInfo(data.asQuadWord)));
            NEXT_OPCODE(op_new_array_buffer);
        }
            
        case op_new_regexp: {
            auto bytecode = currentInstruction->as<OpNewRegexp>();
            ASSERT(bytecode.m_regexp.isConstant());
            FrozenValue* frozenRegExp = m_graph.freezeStrong(m_inlineStackTop->m_codeBlock->getConstant(bytecode.m_regexp));
            set(bytecode.m_dst, addToGraph(NewRegexp, OpInfo(frozenRegExp), jsConstant(jsNumber(0))));
            NEXT_OPCODE(op_new_regexp);
        }

        case op_get_rest_length: {
            auto bytecode = currentInstruction->as<OpGetRestLength>();
            InlineCallFrame* inlineCallFrame = this->inlineCallFrame();
            Node* length;
            if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
                unsigned argumentsLength = inlineCallFrame->argumentCountIncludingThis - 1;
                JSValue restLength;
                if (argumentsLength <= bytecode.m_numParametersToSkip)
                    restLength = jsNumber(0);
                else
                    restLength = jsNumber(argumentsLength - bytecode.m_numParametersToSkip);

                length = jsConstant(restLength);
            } else
                length = addToGraph(GetRestLength, OpInfo(bytecode.m_numParametersToSkip));
            set(bytecode.m_dst, length);
            NEXT_OPCODE(op_get_rest_length);
        }

        case op_create_rest: {
            auto bytecode = currentInstruction->as<OpCreateRest>();
            noticeArgumentsUse();
            Node* arrayLength = get(bytecode.m_arraySize);
            set(bytecode.m_dst,
                addToGraph(CreateRest, OpInfo(bytecode.m_numParametersToSkip), arrayLength));
            NEXT_OPCODE(op_create_rest);
        }
            
        // === Bitwise operations ===

        case op_bitnot: {
            auto bytecode = currentInstruction->as<OpBitnot>();
            SpeculatedType prediction = getPrediction();
            Node* op1 = get(bytecode.m_operand);
            if (op1->hasNumberOrAnyIntResult())
                set(bytecode.m_dst, addToGraph(ArithBitNot, op1));
            else
                set(bytecode.m_dst, addToGraph(ValueBitNot, OpInfo(), OpInfo(prediction), op1));
            NEXT_OPCODE(op_bitnot);
        }

        case op_bitand: {
            auto bytecode = currentInstruction->as<OpBitand>();
            SpeculatedType prediction = getPrediction();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberOrAnyIntResult() && op2->hasNumberOrAnyIntResult())
                set(bytecode.m_dst, addToGraph(ArithBitAnd, op1, op2));
            else
                set(bytecode.m_dst, addToGraph(ValueBitAnd, OpInfo(), OpInfo(prediction), op1, op2));
            NEXT_OPCODE(op_bitand);
        }

        case op_bitor: {
            auto bytecode = currentInstruction->as<OpBitor>();
            SpeculatedType prediction = getPrediction();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberOrAnyIntResult() && op2->hasNumberOrAnyIntResult())
                set(bytecode.m_dst, addToGraph(ArithBitOr, op1, op2));
            else
                set(bytecode.m_dst, addToGraph(ValueBitOr, OpInfo(), OpInfo(prediction), op1, op2));
            NEXT_OPCODE(op_bitor);
        }

        case op_bitxor: {
            auto bytecode = currentInstruction->as<OpBitxor>();
            SpeculatedType prediction = getPrediction();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberOrAnyIntResult() && op2->hasNumberOrAnyIntResult())
                set(bytecode.m_dst, addToGraph(ArithBitXor, op1, op2));
            else
                set(bytecode.m_dst, addToGraph(ValueBitXor, OpInfo(), OpInfo(prediction), op1, op2));
            NEXT_OPCODE(op_bitxor);
        }

        case op_rshift: {
            auto bytecode = currentInstruction->as<OpRshift>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberOrAnyIntResult() && op2->hasNumberOrAnyIntResult())
                set(bytecode.m_dst, addToGraph(ArithBitRShift, op1, op2));
            else {
                SpeculatedType prediction = getPredictionWithoutOSRExit();
                set(bytecode.m_dst, addToGraph(ValueBitRShift, OpInfo(), OpInfo(prediction), op1, op2));
            }
            NEXT_OPCODE(op_rshift);
        }

        case op_lshift: {
            auto bytecode = currentInstruction->as<OpLshift>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberOrAnyIntResult() && op2->hasNumberOrAnyIntResult())
                set(bytecode.m_dst, addToGraph(ArithBitLShift, op1, op2));
            else {
                SpeculatedType prediction = getPredictionWithoutOSRExit();
                set(bytecode.m_dst, addToGraph(ValueBitLShift, OpInfo(), OpInfo(prediction), op1, op2));
            }
            NEXT_OPCODE(op_lshift);
        }

        case op_urshift: {
            auto bytecode = currentInstruction->as<OpUrshift>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(BitURShift, op1, op2));
            NEXT_OPCODE(op_urshift);
        }
            
        case op_unsigned: {
            auto bytecode = currentInstruction->as<OpUnsigned>();
            set(bytecode.m_dst, makeSafe(addToGraph(UInt32ToNumber, get(bytecode.m_operand))));
            NEXT_OPCODE(op_unsigned);
        }

        // === Increment/Decrement opcodes ===

        case op_inc: {
            auto bytecode = currentInstruction->as<OpInc>();
            Node* op = get(bytecode.m_srcDst);
            // FIXME: we can replace the Inc by either ArithAdd with m_constantOne or ArithAdd with the equivalent BigInt in many cases.
            // For now we only do so in DFGFixupPhase.
            // We could probably do it earlier in some cases, but it is not clearly worth the trouble.
            set(bytecode.m_srcDst, makeSafe(addToGraph(Inc, op)));
            NEXT_OPCODE(op_inc);
        }

        case op_dec: {
            auto bytecode = currentInstruction->as<OpDec>();
            Node* op = get(bytecode.m_srcDst);
            // FIXME: we can replace the Inc by either ArithSub with m_constantOne or ArithSub with the equivalent BigInt in many cases.
            // For now we only do so in DFGFixupPhase.
            // We could probably do it earlier in some cases, but it is not clearly worth the trouble.
            set(bytecode.m_srcDst, makeSafe(addToGraph(Dec, op)));
            NEXT_OPCODE(op_dec);
        }

        // === Arithmetic operations ===

        case op_add: {
            auto bytecode = currentInstruction->as<OpAdd>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(bytecode.m_dst, makeSafe(addToGraph(ArithAdd, op1, op2)));
            else
                set(bytecode.m_dst, makeSafe(addToGraph(ValueAdd, op1, op2)));
            NEXT_OPCODE(op_add);
        }

        case op_sub: {
            auto bytecode = currentInstruction->as<OpSub>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(bytecode.m_dst, makeSafe(addToGraph(ArithSub, op1, op2)));
            else
                set(bytecode.m_dst, makeSafe(addToGraph(ValueSub, op1, op2)));
            NEXT_OPCODE(op_sub);
        }

        case op_negate: {
            auto bytecode = currentInstruction->as<OpNegate>();
            Node* op1 = get(bytecode.m_operand);
            if (op1->hasNumberResult())
                set(bytecode.m_dst, makeSafe(addToGraph(ArithNegate, op1)));
            else
                set(bytecode.m_dst, makeSafe(addToGraph(ValueNegate, op1)));
            NEXT_OPCODE(op_negate);
        }

        case op_mul: {
            // Multiply requires that the inputs are not truncated, unfortunately.
            auto bytecode = currentInstruction->as<OpMul>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(bytecode.m_dst, makeSafe(addToGraph(ArithMul, op1, op2)));
            else
                set(bytecode.m_dst, makeSafe(addToGraph(ValueMul, op1, op2)));
            NEXT_OPCODE(op_mul);
        }

        case op_mod: {
            auto bytecode = currentInstruction->as<OpMod>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(bytecode.m_dst, makeSafe(addToGraph(ArithMod, op1, op2)));
            else
                set(bytecode.m_dst, makeSafe(addToGraph(ValueMod, op1, op2)));
            NEXT_OPCODE(op_mod);
        }

        case op_pow: {
            auto bytecode = currentInstruction->as<OpPow>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberOrAnyIntResult() && op2->hasNumberOrAnyIntResult())
                set(bytecode.m_dst, addToGraph(ArithPow, op1, op2));
            else
                set(bytecode.m_dst, addToGraph(ValuePow, op1, op2));
            NEXT_OPCODE(op_pow);
        }

        case op_div: {
            auto bytecode = currentInstruction->as<OpDiv>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(bytecode.m_dst, makeDivSafe(addToGraph(ArithDiv, op1, op2)));
            else
                set(bytecode.m_dst, makeDivSafe(addToGraph(ValueDiv, op1, op2)));
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
            Node* op = get(bytecode.m_src);
            set(bytecode.m_dst, op);
            NEXT_OPCODE(op_mov);
        }

        case op_check_tdz: {
            auto bytecode = currentInstruction->as<OpCheckTdz>();
            addToGraph(CheckNotEmpty, get(bytecode.m_targetVirtualRegister));
            NEXT_OPCODE(op_check_tdz);
        }

        case op_overrides_has_instance: {
            auto bytecode = currentInstruction->as<OpOverridesHasInstance>();
            JSFunction* defaultHasInstanceSymbolFunction = m_inlineStackTop->m_codeBlock->globalObjectFor(currentCodeOrigin())->functionProtoHasInstanceSymbolFunction();

            Node* constructor = get(VirtualRegister(bytecode.m_constructor));
            Node* hasInstanceValue = get(VirtualRegister(bytecode.m_hasInstanceValue));

            set(VirtualRegister(bytecode.m_dst), addToGraph(OverridesHasInstance, OpInfo(m_graph.freeze(defaultHasInstanceSymbolFunction)), constructor, hasInstanceValue));
            NEXT_OPCODE(op_overrides_has_instance);
        }

        case op_identity_with_profile: {
            auto bytecode = currentInstruction->as<OpIdentityWithProfile>();
            Node* srcDst = get(bytecode.m_srcDst);
            SpeculatedType speculation = static_cast<SpeculatedType>(bytecode.m_topProfile) << 32 | static_cast<SpeculatedType>(bytecode.m_bottomProfile);
            set(bytecode.m_srcDst, addToGraph(IdentityWithProfile, OpInfo(speculation), srcDst));
            NEXT_OPCODE(op_identity_with_profile);
        }

        case op_instanceof: {
            auto bytecode = currentInstruction->as<OpInstanceof>();
            
            InstanceOfStatus status = InstanceOfStatus::computeFor(
                m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap,
                m_currentIndex);
            
            Node* value = get(bytecode.m_value);
            Node* prototype = get(bytecode.m_prototype);

            // Only inline it if it's Simple with a commonPrototype; bottom/top or variable
            // prototypes both get handled by the IC. This makes sense for bottom (unprofiled)
            // instanceof ICs because the profit of this optimization is fairly low. So, in the
            // absence of any information, it's better to avoid making this be the cause of a
            // recompilation.
            JSObject* commonPrototype = status.commonPrototype();
            if (commonPrototype && Options::useAccessInlining()) {
                addToGraph(CheckIsConstant, OpInfo(m_graph.freeze(commonPrototype)), prototype);
                
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
                    set(bytecode.m_dst, match);
                    NEXT_OPCODE(op_instanceof);
                }
            }
            
            set(bytecode.m_dst, addToGraph(InstanceOf, value, prototype));
            NEXT_OPCODE(op_instanceof);
        }

        case op_instanceof_custom: {
            auto bytecode = currentInstruction->as<OpInstanceofCustom>();
            Node* value = get(bytecode.m_value);
            Node* constructor = get(bytecode.m_constructor);
            Node* hasInstanceValue = get(bytecode.m_hasInstanceValue);
            set(bytecode.m_dst, addToGraph(InstanceOfCustom, value, constructor, hasInstanceValue));
            NEXT_OPCODE(op_instanceof_custom);
        }
        case op_is_empty: {
            auto bytecode = currentInstruction->as<OpIsEmpty>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsEmpty, value));
            NEXT_OPCODE(op_is_empty);
        }
        case op_typeof_is_undefined: {
            auto bytecode = currentInstruction->as<OpTypeofIsUndefined>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(TypeOfIsUndefined, value));
            NEXT_OPCODE(op_is_undefined);
        }
        case op_typeof_is_object: {
            auto bytecode = currentInstruction->as<OpTypeofIsObject>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(TypeOfIsObject, value));
            NEXT_OPCODE(op_typeof_is_object);
        }
        case op_typeof_is_function: {
            auto bytecode = currentInstruction->as<OpTypeofIsFunction>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(TypeOfIsFunction, value));
            NEXT_OPCODE(op_typeof_is_function);
        }
        case op_is_undefined_or_null: {
            auto bytecode = currentInstruction->as<OpIsUndefinedOrNull>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsUndefinedOrNull, value));
            NEXT_OPCODE(op_is_undefined_or_null);
        }

        case op_is_boolean: {
            auto bytecode = currentInstruction->as<OpIsBoolean>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsBoolean, value));
            NEXT_OPCODE(op_is_boolean);
        }

        case op_is_number: {
            auto bytecode = currentInstruction->as<OpIsNumber>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsNumber, value));
            NEXT_OPCODE(op_is_number);
        }

        case op_is_big_int: {
#if USE(BIGINT32)
            auto bytecode = currentInstruction->as<OpIsBigInt>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsBigInt, value));
            NEXT_OPCODE(op_is_big_int);
#else
            RELEASE_ASSERT_NOT_REACHED();
#endif
        }

        case op_is_cell_with_type: {
            auto bytecode = currentInstruction->as<OpIsCellWithType>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsCellWithType, OpInfo(bytecode.m_type), value));
            NEXT_OPCODE(op_is_cell_with_type);
        }

        case op_is_object: {
            auto bytecode = currentInstruction->as<OpIsObject>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsObject, value));
            NEXT_OPCODE(op_is_object);
        }

        case op_is_callable: {
            auto bytecode = currentInstruction->as<OpIsCallable>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsCallable, value));
            NEXT_OPCODE(op_is_callable);
        }

        case op_is_constructor: {
            auto bytecode = currentInstruction->as<OpIsConstructor>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(IsConstructor, value));
            NEXT_OPCODE(op_is_constructor);
        }

        case op_not: {
            auto bytecode = currentInstruction->as<OpNot>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(LogicalNot, value));
            NEXT_OPCODE(op_not);
        }
            
        case op_to_primitive: {
            auto bytecode = currentInstruction->as<OpToPrimitive>();
            Node* value = get(bytecode.m_src);
            set(bytecode.m_dst, addToGraph(ToPrimitive, value));
            NEXT_OPCODE(op_to_primitive);
        }

        case op_to_property_key: {
            auto bytecode = currentInstruction->as<OpToPropertyKey>();
            Node* value = get(bytecode.m_src);
            set(bytecode.m_dst, addToGraph(ToPropertyKey, value));
            NEXT_OPCODE(op_to_property_key);
        }

        case op_strcat: {
            auto bytecode = currentInstruction->as<OpStrcat>();
            int startOperand = bytecode.m_src.offset();
            int numOperands = bytecode.m_count;
            const unsigned maxArguments = 3;
            Node* operands[AdjacencyList::Size];
            unsigned indexInOperands = 0;
            for (unsigned i = 0; i < AdjacencyList::Size; ++i)
                operands[i] = nullptr;
            for (int operandIdx = 0; operandIdx < numOperands; ++operandIdx) {
                if (indexInOperands == maxArguments) {
                    operands[0] = addToGraph(StrCat, operands[0], operands[1], operands[2]);
                    for (unsigned i = 1; i < AdjacencyList::Size; ++i)
                        operands[i] = nullptr;
                    indexInOperands = 1;
                }
                
                ASSERT(indexInOperands < AdjacencyList::Size);
                ASSERT(indexInOperands < maxArguments);
                operands[indexInOperands++] = get(VirtualRegister(startOperand - operandIdx));
            }
            set(bytecode.m_dst, addToGraph(StrCat, operands[0], operands[1], operands[2]));
            NEXT_OPCODE(op_strcat);
        }

        case op_less: {
            auto bytecode = currentInstruction->as<OpLess>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareLess, op1, op2));
            NEXT_OPCODE(op_less);
        }

        case op_lesseq: {
            auto bytecode = currentInstruction->as<OpLesseq>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareLessEq, op1, op2));
            NEXT_OPCODE(op_lesseq);
        }

        case op_greater: {
            auto bytecode = currentInstruction->as<OpGreater>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareGreater, op1, op2));
            NEXT_OPCODE(op_greater);
        }

        case op_greatereq: {
            auto bytecode = currentInstruction->as<OpGreatereq>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareGreaterEq, op1, op2));
            NEXT_OPCODE(op_greatereq);
        }

        case op_below: {
            auto bytecode = currentInstruction->as<OpBelow>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareBelow, op1, op2));
            NEXT_OPCODE(op_below);
        }

        case op_beloweq: {
            auto bytecode = currentInstruction->as<OpBeloweq>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareBelowEq, op1, op2));
            NEXT_OPCODE(op_beloweq);
        }

        case op_eq: {
            auto bytecode = currentInstruction->as<OpEq>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareEq, op1, op2));
            NEXT_OPCODE(op_eq);
        }

        case op_eq_null: {
            auto bytecode = currentInstruction->as<OpEqNull>();
            Node* value = get(bytecode.m_operand);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            set(bytecode.m_dst, addToGraph(CompareEq, value, nullConstant));
            NEXT_OPCODE(op_eq_null);
        }

        case op_stricteq: {
            auto bytecode = currentInstruction->as<OpStricteq>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(CompareStrictEq, op1, op2));
            NEXT_OPCODE(op_stricteq);
        }

        case op_neq: {
            auto bytecode = currentInstruction->as<OpNeq>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            set(bytecode.m_dst, addToGraph(LogicalNot, addToGraph(CompareEq, op1, op2)));
            NEXT_OPCODE(op_neq);
        }

        case op_neq_null: {
            auto bytecode = currentInstruction->as<OpNeqNull>();
            Node* value = get(bytecode.m_operand);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            set(bytecode.m_dst, addToGraph(LogicalNot, addToGraph(CompareEq, value, nullConstant)));
            NEXT_OPCODE(op_neq_null);
        }

        case op_nstricteq: {
            auto bytecode = currentInstruction->as<OpNstricteq>();
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* invertedResult;
            invertedResult = addToGraph(CompareStrictEq, op1, op2);
            set(bytecode.m_dst, addToGraph(LogicalNot, invertedResult));
            NEXT_OPCODE(op_nstricteq);
        }

        // === Property access operations ===

        case op_get_by_val: {
            auto bytecode = currentInstruction->as<OpGetByVal>();
            SpeculatedType prediction = getPredictionWithoutOSRExit();

            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            bool shouldCompileAsGetById = false;
            GetByStatus getByStatus = GetByStatus::computeFor(m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap, m_icContextStack, currentCodeOrigin());
            unsigned identifierNumber = 0;

            CacheableIdentifier identifier;
            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                // FIXME: In the future, we should be able to do something like MultiGetByOffset in a multi identifier mode.
                // That way, we could both switch on multiple structures and multiple identifiers (or int 32 properties).
                // https://bugs.webkit.org/show_bug.cgi?id=204216
                identifier = getByStatus.singleIdentifier();
                if (identifier) {
                    UniquedStringImpl* uid = identifier.uid();
                    identifierNumber = m_graph.identifiers().ensure(identifier.uid());
                    if (identifier.isCell()) {
                        FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                        if (identifier.isSymbolCell())
                            addToGraph(CheckIsConstant, OpInfo(frozen), property);
                        else
                            addToGraph(CheckIdent, OpInfo(uid), property);
                    } else
                        addToGraph(CheckIdent, OpInfo(uid), property);
                    shouldCompileAsGetById = true;
                }
            }

            if (shouldCompileAsGetById)
                handleGetById(bytecode.m_dst, prediction, base, identifier, identifierNumber, getByStatus, AccessType::GetById, nextOpcodeIndex());
            else {
                ArrayMode arrayMode = getArrayMode(bytecode.metadata(codeBlock).m_arrayProfile, Array::Read);
                // FIXME: We could consider making this not vararg, since it only uses three child
                // slots.
                // https://bugs.webkit.org/show_bug.cgi?id=184192
                addVarArgChild(base);
                addVarArgChild(property);
                addVarArgChild(nullptr); // Leave room for property storage.
                Node* getByVal = addToGraph(Node::VarArg, GetByVal, OpInfo(arrayMode.asWord()), OpInfo(prediction));
                m_exitOK = false; // GetByVal must be treated as if it clobbers exit state, since FixupPhase may make it generic.
                set(bytecode.m_dst, getByVal);
                if (getByStatus.observedStructureStubInfoSlowPath() || bytecode.metadata(codeBlock).m_seenIdentifiers.count() > Options::getByValICMaxNumberOfIdentifiers())
                    m_graph.m_slowGetByVal.add(getByVal);
            }

            NEXT_OPCODE(op_get_by_val);
        }

        case op_get_by_val_with_this: {
            auto bytecode = currentInstruction->as<OpGetByValWithThis>();
            SpeculatedType prediction = getPrediction();

            Node* base = get(bytecode.m_base);
            Node* thisValue = get(bytecode.m_thisValue);
            Node* property = get(bytecode.m_property);
            Node* getByValWithThis = addToGraph(GetByValWithThis, OpInfo(), OpInfo(prediction), base, thisValue, property);
            set(bytecode.m_dst, getByValWithThis);

            NEXT_OPCODE(op_get_by_val_with_this);
        }

        case op_put_by_val_direct:
            handlePutByVal(currentInstruction->as<OpPutByValDirect>(), nextOpcodeIndex());
            NEXT_OPCODE(op_put_by_val_direct);

        case op_put_by_val: {
            handlePutByVal(currentInstruction->as<OpPutByVal>(), nextOpcodeIndex());
            NEXT_OPCODE(op_put_by_val);
        }

        case op_put_by_val_with_this: {
            auto bytecode = currentInstruction->as<OpPutByValWithThis>();
            Node* base = get(bytecode.m_base);
            Node* thisValue = get(bytecode.m_thisValue);
            Node* property = get(bytecode.m_property);
            Node* value = get(bytecode.m_value);

            addVarArgChild(base);
            addVarArgChild(thisValue);
            addVarArgChild(property);
            addVarArgChild(value);
            addToGraph(Node::VarArg, PutByValWithThis, OpInfo(bytecode.m_ecmaMode), OpInfo(0));

            NEXT_OPCODE(op_put_by_val_with_this);
        }

        case op_check_private_brand: {
            auto bytecode = currentInstruction->as<OpCheckPrivateBrand>();
            Node* base = get(bytecode.m_base);
            Node* brand = get(bytecode.m_brand);
            bool compiledAsCheckStructure = false;

            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                CheckPrivateBrandStatus checkStatus = CheckPrivateBrandStatus::computeFor(
                    m_inlineStackTop->m_profiledBlock,
                    m_inlineStackTop->m_baselineMap, m_icContextStack,
                    currentCodeOrigin());

                if (CacheableIdentifier identifier = checkStatus.singleIdentifier()) {
                    m_graph.identifiers().ensure(identifier.uid());
                    ASSERT(identifier.isSymbol());
                    FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                    addToGraph(CheckIsConstant, OpInfo(frozen), brand);

                    if (checkStatus.isSimple() && checkStatus.variants().size() && Options::useAccessInlining()) {
                        ASSERT(checkStatus.variants().size() == 1); // If we have single identifier, we should have only 1 variant.
                        CheckPrivateBrandVariant variant = checkStatus.variants()[0];

                        addToGraph(FilterCheckPrivateBrandStatus, OpInfo(m_graph.m_plan.recordedStatuses().addCheckPrivateBrandStatus(currentCodeOrigin(), checkStatus)), base);
                        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.structureSet())), base);

                        compiledAsCheckStructure = true;
                    }
                }
            }

            if (!compiledAsCheckStructure)
                addToGraph(CheckPrivateBrand, base, brand);

            NEXT_OPCODE(op_check_private_brand);
        }

        case op_set_private_brand: {
            auto bytecode = currentInstruction->as<OpSetPrivateBrand>();
            Node* base = get(bytecode.m_base);
            Node* brand = get(bytecode.m_brand);
            bool inlinedSetPrivateBrand = false;

            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                SetPrivateBrandStatus setStatus = SetPrivateBrandStatus::computeFor(
                    m_inlineStackTop->m_profiledBlock,
                    m_inlineStackTop->m_baselineMap, m_icContextStack,
                    currentCodeOrigin());

                if (CacheableIdentifier identifier = setStatus.singleIdentifier()) {
                    ASSERT(identifier.isSymbol());
                    FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                    addToGraph(CheckIsConstant, OpInfo(frozen), brand);


                    // FIXME: We should include a MultiSetPrivateBrand to handle polymorphic cases
                    // https://bugs.webkit.org/show_bug.cgi?id=221570
                    if (setStatus.isSimple() && setStatus.variants().size() == 1 && Options::useAccessInlining()) {
                        SetPrivateBrandVariant variant = setStatus.variants()[0];

                        addToGraph(FilterSetPrivateBrandStatus, OpInfo(m_graph.m_plan.recordedStatuses().addSetPrivateBrandStatus(currentCodeOrigin(), setStatus)), base);
                        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(variant.oldStructure())), base);
                        ASSERT(variant.oldStructure()->transitionWatchpointSetHasBeenInvalidated());
                        ASSERT(variant.newStructure());

                        Transition* transition = m_graph.m_transitions.add(
                            m_graph.registerStructure(variant.oldStructure()), m_graph.registerStructure(variant.newStructure()));

                        addToGraph(PutStructure, OpInfo(transition), base);

                        inlinedSetPrivateBrand = true;
                    }
                }
            }

            if (!inlinedSetPrivateBrand)
                addToGraph(SetPrivateBrand, base, brand);

            NEXT_OPCODE(op_set_private_brand);
        }

        case op_put_private_name: {
            auto bytecode = currentInstruction->as<OpPutPrivateName>();
            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            Node* value = get(bytecode.m_value);
            bool compiledAsPutPrivateNameById = false;

            PutByStatus status = PutByStatus::computeFor(m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap, m_icContextStack, currentCodeOrigin());

            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {
                if (CacheableIdentifier identifier = status.singleIdentifier()) {
                    UniquedStringImpl* uid = identifier.uid();
                    unsigned identifierNumber = m_graph.identifiers().ensure(uid);
                    if (identifier.isCell()) {
                        FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                        if (identifier.isSymbolCell())
                            addToGraph(CheckIsConstant, OpInfo(frozen), property);
                        else
                            addToGraph(CheckIdent, OpInfo(uid), property);
                    } else
                        addToGraph(CheckIdent, OpInfo(uid), property);

                    handlePutPrivateNameById(base, identifier, identifierNumber, value, status, bytecode.m_putKind);
                    compiledAsPutPrivateNameById = true;
                } else if (status.takesSlowPath()) {
                    // Even though status is taking a slow path, it is possible that this node still has constant identifier and using PutById is always better in that case.
                    UniquedStringImpl* uid = nullptr;
                    JSCell* propertyCell = nullptr;
                    if (auto* symbol = property->dynamicCastConstant<Symbol*>()) {
                        uid = &symbol->uid();
                        propertyCell = symbol;
                        FrozenValue* frozen = m_graph.freezeStrong(symbol);
                        addToGraph(CheckIsConstant, OpInfo(frozen), property);
                    } else if (auto* string = property->dynamicCastConstant<JSString*>()) {
                        auto* impl = string->tryGetValueImpl();
                        if (impl && impl->isAtom() && !parseIndex(*const_cast<StringImpl*>(impl))) {
                            uid = bitwise_cast<UniquedStringImpl*>(impl);
                            propertyCell = string;
                            m_graph.freezeStrong(string);
                            addToGraph(CheckIdent, OpInfo(uid), property);
                        }
                    }

                    if (uid) {
                        unsigned identifierNumber = m_graph.identifiers().ensure(uid);
                        handlePutPrivateNameById(base, CacheableIdentifier::createFromCell(propertyCell), identifierNumber, value, status, bytecode.m_putKind);
                        compiledAsPutPrivateNameById = true;
                    }
                }
            }

            if (!compiledAsPutPrivateNameById) {
                Node* putPrivateName = addToGraph(PutPrivateName, OpInfo(), OpInfo(bytecode.m_putKind), base, property, value);
                if (status.observedStructureStubInfoSlowPath())
                    m_graph.m_slowPutByVal.add(putPrivateName);
            }

            NEXT_OPCODE(op_put_private_name);
        }

        case op_define_data_property: {
            auto bytecode = currentInstruction->as<OpDefineDataProperty>();
            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            Node* value = get(bytecode.m_value);
            Node* attributes = get(bytecode.m_attributes);

            addVarArgChild(base);
            addVarArgChild(property);
            addVarArgChild(value);
            addVarArgChild(attributes);
            addToGraph(Node::VarArg, DefineDataProperty, OpInfo(0), OpInfo(0));

            NEXT_OPCODE(op_define_data_property);
        }

        case op_define_accessor_property: {
            auto bytecode = currentInstruction->as<OpDefineAccessorProperty>();
            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            Node* getter = get(bytecode.m_getter);
            Node* setter = get(bytecode.m_setter);
            Node* attributes = get(bytecode.m_attributes);

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
            Node* base = get(bytecode.m_base);
            Node* thisValue = get(bytecode.m_thisValue);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];

            set(bytecode.m_dst, addToGraph(GetByIdWithThis, OpInfo(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid)), OpInfo(prediction), base, thisValue));

            NEXT_OPCODE(op_get_by_id_with_this);
        }

        case op_get_prototype_of: {
            auto bytecode = currentInstruction->as<OpGetPrototypeOf>();
            set(bytecode.m_dst, addToGraph(GetPrototypeOf, OpInfo(0), OpInfo(getPrediction()), get(bytecode.m_value)));
            NEXT_OPCODE(op_get_prototype_of);
        }

        case op_put_by_id: {
            auto bytecode = currentInstruction->as<OpPutById>();
            Node* value = get(bytecode.m_value);
            Node* base = get(bytecode.m_base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
            bool direct = bytecode.m_flags.isDirect();

            PutByStatus putByStatus = PutByStatus::computeFor(
                m_inlineStackTop->m_profiledBlock,
                m_inlineStackTop->m_baselineMap, m_icContextStack,
                currentCodeOrigin());
            
            handlePutById(base, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid), identifierNumber, value, putByStatus, direct, nextOpcodeIndex(), bytecode.m_flags.ecmaMode());
            NEXT_OPCODE(op_put_by_id);
        }

        case op_put_by_id_with_this: {
            auto bytecode = currentInstruction->as<OpPutByIdWithThis>();
            Node* base = get(bytecode.m_base);
            Node* thisValue = get(bytecode.m_thisValue);
            Node* value = get(bytecode.m_value);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
            addToGraph(PutByIdWithThis, OpInfo(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid)), OpInfo(bytecode.m_ecmaMode), base, thisValue, value);
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
            Node* base = get(bytecode.m_base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
            Node* getter = get(bytecode.m_getter);
            Node* setter = get(bytecode.m_setter);
            addToGraph(PutGetterSetterById, OpInfo(identifierNumber), OpInfo(bytecode.m_attributes), base, getter, setter);
            NEXT_OPCODE(op_put_getter_setter_by_id);
        }

        case op_put_getter_by_val:
            handlePutAccessorByVal(PutGetterByVal, currentInstruction->as<OpPutGetterByVal>());
            NEXT_OPCODE(op_put_getter_by_val);
        case op_put_setter_by_val: {
            handlePutAccessorByVal(PutSetterByVal, currentInstruction->as<OpPutSetterByVal>());
            NEXT_OPCODE(op_put_setter_by_val);
        }

        case op_get_private_name: {
            auto bytecode = currentInstruction->as<OpGetPrivateName>();
            SpeculatedType prediction = getPredictionWithoutOSRExit();
            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            bool compileSingleIdentifier = false;

            GetByStatus getByStatus = GetByStatus::computeFor(m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap, m_icContextStack, currentCodeOrigin());

            CacheableIdentifier identifier;
            unsigned identifierNumber = 0;
            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                identifier = getByStatus.singleIdentifier();
                if (identifier) {
                    identifierNumber = m_graph.identifiers().ensure(identifier.uid());
                    ASSERT(identifier.isSymbolCell());
                    FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                    addToGraph(CheckIsConstant, OpInfo(frozen), property);
                    compileSingleIdentifier = true;
                }
            }

            if (compileSingleIdentifier)
                handleGetPrivateNameById(bytecode.m_dst, prediction, base, identifier, identifierNumber, getByStatus);
            else {
                Node* node = addToGraph(GetPrivateName, OpInfo(), OpInfo(prediction), base, property);
                m_exitOK = false;
                set(bytecode.m_dst, node);
            }
            NEXT_OPCODE(op_get_private_name);
        }

        case op_del_by_id: {
            auto bytecode = currentInstruction->as<OpDelById>();
            Node* base = get(bytecode.m_base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
            DeleteByStatus deleteByStatus = DeleteByStatus::computeFor(
                m_inlineStackTop->m_profiledBlock,
                m_inlineStackTop->m_baselineMap, m_icContextStack,
                currentCodeOrigin());
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
            auto identifier = CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid);
            handleDeleteById(bytecode.m_dst, base, identifier, identifierNumber, deleteByStatus, bytecode.m_ecmaMode);
            NEXT_OPCODE(op_del_by_id);
        }

        case op_del_by_val: {
            auto bytecode = currentInstruction->as<OpDelByVal>();
            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            bool shouldCompileAsDeleteById = false;

            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                DeleteByStatus deleteByStatus = DeleteByStatus::computeFor(
                    m_inlineStackTop->m_profiledBlock,
                    m_inlineStackTop->m_baselineMap, m_icContextStack,
                    currentCodeOrigin());

                if (CacheableIdentifier identifier = deleteByStatus.singleIdentifier()) {
                    UniquedStringImpl* uid = identifier.uid();
                    unsigned identifierNumber = m_graph.identifiers().ensure(identifier.uid());
                    if (identifier.isCell()) {
                        FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                        if (identifier.isSymbolCell())
                            addToGraph(CheckIsConstant, OpInfo(frozen), property);
                        else
                            addToGraph(CheckIdent, OpInfo(uid), property);
                    } else
                        addToGraph(CheckIdent, OpInfo(uid), property);

                    handleDeleteById(bytecode.m_dst, base, identifier, identifierNumber, deleteByStatus, bytecode.m_ecmaMode);
                    shouldCompileAsDeleteById = true;
                }
            }

            if (!shouldCompileAsDeleteById)
                set(bytecode.m_dst, addToGraph(DeleteByVal, OpInfo(bytecode.m_ecmaMode), base, property));
            NEXT_OPCODE(op_del_by_val);
        }

        case op_profile_type: {
            auto bytecode = currentInstruction->as<OpProfileType>();
            auto& metadata = bytecode.metadata(codeBlock);
            Node* valueToProfile = get(bytecode.m_targetVirtualRegister);
            addToGraph(ProfileType, OpInfo(metadata.m_typeLocation), valueToProfile);
            NEXT_OPCODE(op_profile_type);
        }

        case op_profile_control_flow: {
            auto bytecode = currentInstruction->as<OpProfileControlFlow>();
            BasicBlockLocation* basicBlockLocation = bytecode.metadata(codeBlock).m_basicBlockLocation;
            addToGraph(ProfileControlFlow, OpInfo(basicBlockLocation));
            NEXT_OPCODE(op_profile_control_flow);
        }

        // === Block terminators. ===

        case op_jmp: {
            ASSERT(!m_currentBlock->terminal());
            auto bytecode = currentInstruction->as<OpJmp>();
            int relativeOffset = jumpTarget(bytecode.m_targetLabel);
            addToGraph(Jump, OpInfo(m_currentIndex.offset() + relativeOffset));
            if (relativeOffset <= 0)
                flushForTerminal();
            LAST_OPCODE(op_jmp);
        }

        case op_jtrue: {
            auto bytecode = currentInstruction->as<OpJtrue>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* condition = get(bytecode.m_condition);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jtrue);
        }

        case op_jfalse: {
            auto bytecode = currentInstruction->as<OpJfalse>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* condition = get(bytecode.m_condition);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jfalse);
        }

        case op_jeq_null: {
            auto bytecode = currentInstruction->as<OpJeqNull>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* value = get(bytecode.m_value);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            Node* condition = addToGraph(CompareEq, value, nullConstant);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jeq_null);
        }

        case op_jneq_null: {
            auto bytecode = currentInstruction->as<OpJneqNull>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* value = get(bytecode.m_value);
            Node* nullConstant = addToGraph(JSConstant, OpInfo(m_constantNull));
            Node* condition = addToGraph(CompareEq, value, nullConstant);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jneq_null);
        }

        case op_jundefined_or_null: {
            auto bytecode = currentInstruction->as<OpJundefinedOrNull>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* value = get(bytecode.m_value);
            Node* condition = addToGraph(IsUndefinedOrNull, value);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jundefined_or_null);
        }

        case op_jnundefined_or_null: {
            auto bytecode = currentInstruction->as<OpJnundefinedOrNull>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* value = get(bytecode.m_value);
            Node* condition = addToGraph(IsUndefinedOrNull, value);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jnundefined_or_null);
        }

        case op_jless: {
            auto bytecode = currentInstruction->as<OpJless>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jless);
        }

        case op_jlesseq: {
            auto bytecode = currentInstruction->as<OpJlesseq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jlesseq);
        }

        case op_jgreater: {
            auto bytecode = currentInstruction->as<OpJgreater>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jgreater);
        }

        case op_jgreatereq: {
            auto bytecode = currentInstruction->as<OpJgreatereq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jgreatereq);
        }

        case op_jeq: {
            auto bytecode = currentInstruction->as<OpJeq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jeq);
        }

        case op_jstricteq: {
            auto bytecode = currentInstruction->as<OpJstricteq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareStrictEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jstricteq);
        }

        case op_jnless: {
            auto bytecode = currentInstruction->as<OpJnless>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jnless);
        }

        case op_jnlesseq: {
            auto bytecode = currentInstruction->as<OpJnlesseq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jnlesseq);
        }

        case op_jngreater: {
            auto bytecode = currentInstruction->as<OpJngreater>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jngreater);
        }

        case op_jngreatereq: {
            auto bytecode = currentInstruction->as<OpJngreatereq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jngreatereq);
        }

        case op_jneq: {
            auto bytecode = currentInstruction->as<OpJneq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jneq);
        }

        case op_jnstricteq: {
            auto bytecode = currentInstruction->as<OpJnstricteq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareStrictEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
            LAST_OPCODE(op_jnstricteq);
        }

        case op_jbelow: {
            auto bytecode = currentInstruction->as<OpJbelow>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareBelow, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jbelow);
        }

        case op_jbeloweq: {
            auto bytecode = currentInstruction->as<OpJbeloweq>();
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* op1 = get(bytecode.m_lhs);
            Node* op2 = get(bytecode.m_rhs);
            Node* condition = addToGraph(CompareBelowEq, op1, op2);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jbeloweq);
        }

        case op_switch_imm: {
            auto bytecode = currentInstruction->as<OpSwitchImm>();
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchImm;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[bytecode.m_tableIndex];
            data.fallThrough.setBytecodeIndex(m_currentIndex.offset() + jumpTarget(bytecode.m_defaultOffset));
            const UnlinkedSimpleJumpTable& unlinkedTable = m_graph.unlinkedSwitchJumpTable(data.switchTableIndex);
            for (unsigned i = 0; i < unlinkedTable.m_branchOffsets.size(); ++i) {
                if (!unlinkedTable.m_branchOffsets[i])
                    continue;
                unsigned target = m_currentIndex.offset() + unlinkedTable.m_branchOffsets[i];
                if (target == data.fallThrough.bytecodeIndex())
                    continue;
                data.cases.append(SwitchCase::withBytecodeIndex(m_graph.freeze(jsNumber(static_cast<int32_t>(unlinkedTable.m_min + i))), target));
            }
            addToGraph(Switch, OpInfo(&data), get(bytecode.m_scrutinee));
            flushIfTerminal(data);
            LAST_OPCODE(op_switch_imm);
        }
            
        case op_switch_char: {
            auto bytecode = currentInstruction->as<OpSwitchChar>();
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchChar;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[bytecode.m_tableIndex];
            data.fallThrough.setBytecodeIndex(m_currentIndex.offset() + jumpTarget(bytecode.m_defaultOffset));
            const UnlinkedSimpleJumpTable& unlinkedTable = m_graph.unlinkedSwitchJumpTable(data.switchTableIndex);
            for (unsigned i = 0; i < unlinkedTable.m_branchOffsets.size(); ++i) {
                if (!unlinkedTable.m_branchOffsets[i])
                    continue;
                unsigned target = m_currentIndex.offset() + unlinkedTable.m_branchOffsets[i];
                if (target == data.fallThrough.bytecodeIndex())
                    continue;
                data.cases.append(
                    SwitchCase::withBytecodeIndex(LazyJSValue::singleCharacterString(unlinkedTable.m_min + i), target));
            }
            addToGraph(Switch, OpInfo(&data), get(bytecode.m_scrutinee));
            flushIfTerminal(data);
            LAST_OPCODE(op_switch_char);
        }

        case op_switch_string: {
            auto bytecode = currentInstruction->as<OpSwitchString>();
            SwitchData& data = *m_graph.m_switchData.add();
            data.kind = SwitchString;
            data.switchTableIndex = m_inlineStackTop->m_stringSwitchRemap[bytecode.m_tableIndex];
            data.fallThrough.setBytecodeIndex(m_currentIndex.offset() + jumpTarget(bytecode.m_defaultOffset));
            const UnlinkedStringJumpTable& unlinkedTable = m_graph.unlinkedStringSwitchJumpTable(data.switchTableIndex);
            for (const auto& entry : unlinkedTable.m_offsetTable) {
                unsigned target = m_currentIndex.offset() + entry.value.m_branchOffset;
                if (target == data.fallThrough.bytecodeIndex())
                    continue;
                ASSERT(entry.key.get()->isAtom());
                data.cases.append(
                    SwitchCase::withBytecodeIndex(LazyJSValue::knownStringImpl(static_cast<AtomStringImpl*>(entry.key.get())), target));
            }
            addToGraph(Switch, OpInfo(&data), get(bytecode.m_scrutinee));
            flushIfTerminal(data);
            LAST_OPCODE(op_switch_string);
        }

        case op_ret: {
            auto bytecode = currentInstruction->as<OpRet>();
            ASSERT(!m_currentBlock->terminal());
            // We have to get the return here even if we know the caller won't use it because the GetLocal may
            // be the only thing keeping m_value alive for OSR.
            auto returnValue = get(bytecode.m_value);

            if (!inlineCallFrame()) {
                // Simple case: we are just producing a return
                addToGraph(Return, returnValue);
                flushForReturn();
                LAST_OPCODE(op_ret);
            }

            flushForReturn();
            if (m_inlineStackTop->m_returnValue.isValid())
                setDirect(m_inlineStackTop->m_returnValue, returnValue, ImmediateSetWithFlush);

            if (!m_inlineStackTop->m_continuationBlock && m_currentIndex.offset() + currentInstruction->size() != m_inlineStackTop->m_codeBlock->instructions().size()) {
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
            addToGraph(Return, get(currentInstruction->as<OpEnd>().m_value));
            flushForReturn();
            LAST_OPCODE(op_end);

        case op_throw:
            addToGraph(Throw, get(currentInstruction->as<OpThrow>().m_value));
            flushForTerminal();
            LAST_OPCODE(op_throw);
            
        case op_throw_static_error: {
            auto bytecode = currentInstruction->as<OpThrowStaticError>();
            addToGraph(ThrowStaticError, OpInfo(bytecode.m_errorType), get(bytecode.m_message));
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

            if (m_graph.m_plan.mode() == JITCompilationMode::FTLForOSREntry) {
                NEXT_OPCODE(op_catch);
            }

            RELEASE_ASSERT(!m_currentBlock->size() || (m_graph.compilation() && m_currentBlock->size() == 1 && m_currentBlock->at(0)->op() == CountExecution));

            ValueProfileAndVirtualRegisterBuffer* buffer = bytecode.metadata(codeBlock).m_buffer;

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
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->valueProfileLock());

                buffer->forEach([&](ValueProfileAndVirtualRegister& profile) {
                    VirtualRegister operand(profile.m_operand);
                    SpeculatedType prediction = profile.computeUpdatedPrediction(locker);
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
            localsToSet.reserveInitialCapacity(buffer->size()); // Note: This will reserve more than the number of locals we see below because the buffer includes arguments.

            // We're not allowed to exit here since we would not properly recover values.
            // We first need to bootstrap the catch entrypoint state.
            m_exitOK = false; 

            unsigned numberOfLocals = 0;
            buffer->forEach([&] (ValueProfileAndVirtualRegister& profile) {
                VirtualRegister operand(profile.m_operand);
                if (operand.isArgument())
                    return;
                ASSERT(operand.isLocal());
                Node* value = addToGraph(ExtractCatchLocal, OpInfo(numberOfLocals), OpInfo(localPredictions[numberOfLocals]));
                ++numberOfLocals;
                addToGraph(MovHint, OpInfo(operand), value);
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
            // The SetArgumentDefinitely nodes that follow below may exit because we may hoist type checks
            // to them. The SetLocal nodes that follow below may exit because we may choose
            // a flush format that speculates on the type of the local.
            m_exitOK = true; 
            addToGraph(ExitOK);

            {
                auto addResult = m_graph.m_rootToArguments.add(m_currentBlock, ArgumentsVector());
                RELEASE_ASSERT(addResult.isNewEntry);
                ArgumentsVector& entrypointArguments = addResult.iterator->value;
                entrypointArguments.resize(m_numArguments);

                BytecodeIndex exitBytecodeIndex = BytecodeIndex(m_currentIndex.offset() + currentInstruction->size());

                for (unsigned argument = 0; argument < argumentPredictions.size(); ++argument) {
                    VariableAccessData* variable = newVariableAccessData(virtualRegisterForArgumentIncludingThis(argument));
                    variable->predict(argumentPredictions[argument]);

                    variable->mergeStructureCheckHoistingFailed(
                        m_inlineStackTop->m_exitProfile.hasExitSite(exitBytecodeIndex, BadCache));
                    variable->mergeCheckArrayHoistingFailed(
                        m_inlineStackTop->m_exitProfile.hasExitSite(exitBytecodeIndex, BadIndexingType));

                    Node* setArgument = addToGraph(SetArgumentDefinitely, OpInfo(variable));
                    setArgument->origin.forExit = CodeOrigin(exitBytecodeIndex, setArgument->origin.forExit.inlineCallFrame());
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
            handleCall<OpCall>(currentInstruction, Call, CallMode::Regular, nextOpcodeIndex());
            ASSERT_WITH_MESSAGE(m_currentInstruction == currentInstruction, "handleCall, which may have inlined the callee, trashed m_currentInstruction");
            NEXT_OPCODE(op_call);

        case op_tail_call: {
            flushForReturn();
            Terminality terminality = handleCall<OpTailCall>(currentInstruction, TailCall, CallMode::Tail, nextOpcodeIndex());
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
            handleCall<OpConstruct>(currentInstruction, Construct, CallMode::Construct, nextOpcodeIndex());
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

        case op_call_direct_eval: {
            auto bytecode = currentInstruction->as<OpCallDirectEval>();
            int registerOffset = -bytecode.m_argv;
            addCall(bytecode.m_dst, CallDirectEval, OpInfo(bytecode.m_ecmaMode), get(bytecode.m_callee), bytecode.m_argc, registerOffset, getPrediction(), get(bytecode.m_thisValue), get(bytecode.m_scope));
            NEXT_OPCODE(op_call_direct_eval);
        }

        case op_iterator_open: {
            auto bytecode = currentInstruction->as<OpIteratorOpen>();
            auto& metadata = bytecode.metadata(codeBlock);
            uint32_t seenModes = metadata.m_iterationMetadata.seenModes;

            unsigned numberOfRemainingModes = WTF::bitCount(seenModes);
            ASSERT(numberOfRemainingModes <= numberOfIterationModes);
            bool generatedCase = false;

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObjectFor(currentCodeOrigin());
            BasicBlock* genericBlock = nullptr;
            BasicBlock* continuation = allocateUntargetableBlock();

            BytecodeIndex startIndex = m_currentIndex;

            Node* symbolIterator = get(bytecode.m_symbolIterator);
            auto& arrayIteratorProtocolWatchpointSet = globalObject->arrayIteratorProtocolWatchpointSet();

            if (seenModes & IterationMode::FastArray && arrayIteratorProtocolWatchpointSet.isStillValid()) {
                // First set up the watchpoint conditions we need for correctness.
                m_graph.watchpoints().addLazily(arrayIteratorProtocolWatchpointSet);

                ASSERT_WITH_MESSAGE(globalObject->arrayProtoValuesFunctionConcurrently(), "The only way we could have seen FastArray is if we saw this function in the LLInt/Baseline so the iterator function should be allocated.");
                FrozenValue* frozenSymbolIteratorFunction = m_graph.freeze(globalObject->arrayProtoValuesFunctionConcurrently());
                numberOfRemainingModes--;
                if (!numberOfRemainingModes) {
                    addToGraph(CheckIsConstant, OpInfo(frozenSymbolIteratorFunction), symbolIterator);
                    addToGraph(CheckJSCast, OpInfo(JSArray::info()), get(bytecode.m_iterable));
                } else {
                    BasicBlock* fastArrayBlock = allocateUntargetableBlock();
                    genericBlock = allocateUntargetableBlock();

                    Node* isKnownIterFunction = addToGraph(CompareEqPtr, OpInfo(frozenSymbolIteratorFunction), symbolIterator);
                    Node* isArray = addToGraph(IsCellWithType, OpInfo(ArrayType), get(bytecode.m_iterable));

                    BranchData* branchData = m_graph.m_branchData.add();
                    branchData->taken = BranchTarget(fastArrayBlock);
                    branchData->notTaken = BranchTarget(genericBlock);

                    Node* andResult = addToGraph(ArithBitAnd, isArray, isKnownIterFunction);

                    // We know the ArithBitAnd cannot have effects so it's ok to exit here.
                    m_exitOK = true;
                    addToGraph(ExitOK);

                    addToGraph(Branch, OpInfo(branchData), andResult);
                    flushForTerminal();

                    m_currentBlock = fastArrayBlock;
                    clearCaches();
                }

                Node* kindNode = jsConstant(jsNumber(static_cast<uint32_t>(IterationKind::Values)));
                Node* next = jsConstant(JSValue());
                Node* iterator = addToGraph(NewInternalFieldObject, OpInfo(m_graph.registerStructure(globalObject->arrayIteratorStructure())));
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::IteratedObject)), iterator, get(bytecode.m_iterable));
                addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::Kind)), iterator, kindNode);
                set(bytecode.m_iterator, iterator);

                // Set m_next to JSValue() so if we exit between here and iterator_next instruction it knows we are in the fast case.
                set(bytecode.m_next, next);

                // Do our set locals. We don't want to exit backwards so move our exit to the next bytecode.
                m_currentIndex = nextOpcodeIndex();
                m_exitOK = true;
                processSetLocalQueue();

                addToGraph(Jump, OpInfo(continuation));
                generatedCase = true;
            }

            m_currentIndex = startIndex;

            if (seenModes & IterationMode::Generic) {
                ASSERT(numberOfRemainingModes);
                if (genericBlock) {
                    ASSERT(generatedCase);
                    m_currentBlock = genericBlock;
                    clearCaches();
                } else
                    ASSERT(!generatedCase);

                Terminality terminality = handleCall<OpIteratorOpen>(currentInstruction, Call, CallMode::Regular, nextCheckpoint());
                ASSERT_UNUSED(terminality, terminality == NonTerminal);
                progressToNextCheckpoint();

                Node* iterator = get(bytecode.m_iterator);
                BasicBlock* notObjectBlock = allocateUntargetableBlock();
                BasicBlock* isObjectBlock = allocateUntargetableBlock();
                BranchData* branchData = m_graph.m_branchData.add();
                branchData->taken = BranchTarget(isObjectBlock);
                branchData->notTaken = BranchTarget(notObjectBlock);
                addToGraph(Branch, OpInfo(branchData), addToGraph(IsObject, iterator));

                {
                    m_currentBlock = notObjectBlock;
                    clearCaches();
                    LazyJSValue errorString = LazyJSValue::newString(m_graph, "Iterator result interface is not an object."_s);
                    OpInfo info = OpInfo(m_graph.m_lazyJSValues.add(errorString));
                    Node* errorMessage = addToGraph(LazyJSConstant, info);
                    addToGraph(ThrowStaticError, OpInfo(ErrorType::TypeError), errorMessage);
                    flushForTerminal();
                }

                {
                    m_currentBlock = isObjectBlock;
                    clearCaches();
                    SpeculatedType prediction = getPrediction();

                    Node* base = get(bytecode.m_iterator);
                    auto* nextImpl = m_vm->propertyNames->next.impl();
                    unsigned identifierNumber = m_graph.identifiers().ensure(nextImpl);

                    AccessType type = AccessType::GetById;

                    GetByStatus getByStatus = GetByStatus::computeFor(
                        m_inlineStackTop->m_profiledBlock,
                        m_inlineStackTop->m_baselineMap, m_icContextStack,
                        currentCodeOrigin());


                    handleGetById(bytecode.m_next, prediction, base, CacheableIdentifier::createFromImmortalIdentifier(nextImpl), identifierNumber, getByStatus, type, nextOpcodeIndex());

                    // Do our set locals. We don't want to run our get_by_id again so we move to the next bytecode.
                    m_currentIndex = nextOpcodeIndex();
                    m_exitOK = true;
                    processSetLocalQueue();

                    addToGraph(Jump, OpInfo(continuation));
                }
                generatedCase = true;
            }

            if (!generatedCase) {
                Node* result = jsConstant(JSValue());
                addToGraph(ForceOSRExit);
                set(bytecode.m_iterator, result);
                set(bytecode.m_next, result);

                m_currentIndex = nextOpcodeIndex();
                m_exitOK = true;
                processSetLocalQueue();

                addToGraph(Jump, OpInfo(continuation));
            }

            m_currentIndex = startIndex;
            m_currentBlock = continuation;
            clearCaches();

            NEXT_OPCODE(op_iterator_open);
        }

        case op_iterator_next: {
            auto bytecode = currentInstruction->as<OpIteratorNext>();
            auto& metadata = bytecode.metadata(codeBlock);
            uint32_t seenModes = metadata.m_iterationMetadata.seenModes;

            unsigned numberOfRemainingModes = WTF::bitCount(seenModes);
            ASSERT(numberOfRemainingModes <= numberOfIterationModes);
            bool generatedCase = false;

            BytecodeIndex startIndex = m_currentIndex;
            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObjectFor(currentCodeOrigin());
            auto& arrayIteratorProtocolWatchpointSet = globalObject->arrayIteratorProtocolWatchpointSet();
            BasicBlock* genericBlock = nullptr;
            BasicBlock* continuation = allocateUntargetableBlock();

            if (seenModes & IterationMode::FastArray && arrayIteratorProtocolWatchpointSet.isStillValid()) {
                // First set up the watchpoint conditions we need for correctness.
                m_graph.watchpoints().addLazily(arrayIteratorProtocolWatchpointSet);

                if (numberOfRemainingModes != 1) {
                    Node* hasNext = addToGraph(IsEmpty, get(bytecode.m_next));
                    genericBlock = allocateUntargetableBlock();
                    BasicBlock* fastArrayBlock = allocateUntargetableBlock();

                    BranchData* branchData = m_graph.m_branchData.add();
                    branchData->taken = BranchTarget(fastArrayBlock);
                    branchData->notTaken = BranchTarget(genericBlock);
                    addToGraph(Branch, OpInfo(branchData), hasNext);

                    m_currentBlock = fastArrayBlock;
                    clearCaches();
                } else
                    addToGraph(CheckIsConstant, OpInfo(m_graph.freeze(JSValue())), get(bytecode.m_next));

                Node* iterator = get(bytecode.m_iterator);
                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(globalObject->arrayIteratorStructure())), iterator);

                BasicBlock* isDoneBlock = allocateUntargetableBlock();
                BasicBlock* doLoadBlock = allocateUntargetableBlock();

                ArrayMode arrayMode = getArrayMode(metadata.m_iterableProfile, Array::Read);
                auto prediction = getPredictionWithoutOSRExit(BytecodeIndex(m_currentIndex.offset(), OpIteratorNext::getValue));

                {
                    // FIXME: doneIndex is -1 so it seems like we should be able to do CompareBelow(index, length). See: https://bugs.webkit.org/show_bug.cgi?id=210927
                    Node* doneIndex = jsConstant(jsNumber(JSArrayIterator::doneIndex));
                    Node* index = addToGraph(GetInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::Index)), OpInfo(SpecInt32Only), iterator);
                    Node* isDone = addToGraph(CompareStrictEq, index, doneIndex);

                    Node* iterable = get(bytecode.m_iterable);
                    Node* butterfly = addToGraph(GetButterfly, iterable);
                    Node* length = addToGraph(GetArrayLength, OpInfo(arrayMode.asWord()), iterable, butterfly);
                    // GetArrayLength is pessimized prior to fixup.
                    m_exitOK = true;
                    addToGraph(ExitOK);
                    Node* isOutOfBounds = addToGraph(CompareGreaterEq, Edge(index, Int32Use), Edge(length, Int32Use));

                    isDone = addToGraph(ArithBitOr, isDone, isOutOfBounds);
                    // The above compare doesn't produce effects since we know the values are booleans. We don't set UseKinds because Fixup likes to add edges.
                    m_exitOK = true;
                    addToGraph(ExitOK);

                    BranchData* branchData = m_graph.m_branchData.add();
                    branchData->taken = BranchTarget(isDoneBlock);
                    branchData->notTaken = BranchTarget(doLoadBlock);
                    addToGraph(Branch, OpInfo(branchData), isDone);
                }

                {
                    m_currentBlock = doLoadBlock;
                    clearCaches();
                    Node* index = addToGraph(GetInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::Index)), OpInfo(SpecInt32Only), get(bytecode.m_iterator));
                    Node* one = jsConstant(jsNumber(1));
                    Node* newIndex = makeSafe(addToGraph(ArithAdd, index, one));
                    Node* falseNode = jsConstant(jsBoolean(false));


                    // FIXME: We could consider making this not vararg, since it only uses three child
                    // slots.
                    // https://bugs.webkit.org/show_bug.cgi?id=184192
                    addVarArgChild(get(bytecode.m_iterable));
                    addVarArgChild(index);
                    addVarArgChild(nullptr); // Leave room for property storage.
                    Node* getByVal = addToGraph(Node::VarArg, GetByVal, OpInfo(arrayMode.asWord()), OpInfo(prediction));
                    set(bytecode.m_value, getByVal);
                    set(bytecode.m_done, falseNode);
                    addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::Index)), get(bytecode.m_iterator), newIndex);

                    // Do our set locals. We don't want to run our getByVal again so we move to the next bytecode.
                    m_currentIndex = nextOpcodeIndex();
                    m_exitOK = true;
                    processSetLocalQueue();

                    addToGraph(Jump, OpInfo(continuation));
                }

                // Roll back the checkpoint.
                m_currentIndex = startIndex;

                {
                    m_currentBlock = isDoneBlock;
                    clearCaches();
                    Node* trueNode = jsConstant(jsBoolean(true));
                    Node* doneIndex = jsConstant(jsNumber(-1));
                    Node* bottomNode = jsConstant(m_graph.bottomValueMatchingSpeculation(prediction));

                    set(bytecode.m_value, bottomNode);
                    set(bytecode.m_done, trueNode);
                    addToGraph(PutInternalField, OpInfo(static_cast<uint32_t>(JSArrayIterator::Field::Index)), get(bytecode.m_iterator), doneIndex);

                    // Do our set locals. We don't want to run this again so we have to move the exit origin forward.
                    m_currentIndex = nextOpcodeIndex();
                    m_exitOK = true;
                    processSetLocalQueue();

                    addToGraph(Jump, OpInfo(continuation));
                }

                m_currentIndex = startIndex;
                generatedCase = true;
            }

            if (seenModes & IterationMode::Generic) {
                if (genericBlock) {
                    ASSERT(generatedCase);
                    m_currentBlock = genericBlock;
                    clearCaches();
                } else
                    ASSERT(!generatedCase);

                // Our profiling could have been incorrect when we got here. For instance, if we LoopHint OSR enter the first time we would
                // have seen a fast path, next will be the empty value. When that happens we need to make sure the empty value doesn't flow
                // into the Call node since call can't handle empty values.
                addToGraph(CheckNotEmpty, get(bytecode.m_next));

                Terminality terminality = handleCall<OpIteratorNext>(currentInstruction, Call, CallMode::Regular, nextCheckpoint());
                ASSERT_UNUSED(terminality, terminality == NonTerminal);
                progressToNextCheckpoint();

                BasicBlock* notObjectBlock = allocateUntargetableBlock();
                BasicBlock* isObjectBlock = allocateUntargetableBlock();
                BasicBlock* notDoneBlock = allocateUntargetableBlock();

                Operand nextResult = Operand::tmp(OpIteratorNext::nextResult);
                {
                    Node* iteratorResult = get(nextResult);
                    BranchData* branchData = m_graph.m_branchData.add();
                    branchData->taken = BranchTarget(isObjectBlock);
                    branchData->notTaken = BranchTarget(notObjectBlock);
                    addToGraph(Branch, OpInfo(branchData), addToGraph(IsObject, iteratorResult));
                }

                {
                    m_currentBlock = notObjectBlock;
                    clearCaches();
                    LazyJSValue errorString = LazyJSValue::newString(m_graph, "Iterator result interface is not an object."_s);
                    OpInfo info = OpInfo(m_graph.m_lazyJSValues.add(errorString));
                    Node* errorMessage = addToGraph(LazyJSConstant, info);
                    addToGraph(ThrowStaticError, OpInfo(ErrorType::TypeError), errorMessage);
                    flushForTerminal();
                }

                auto valuePredicition = getPredictionWithoutOSRExit(m_currentIndex.withCheckpoint(OpIteratorNext::getValue));

                {
                    m_exitOK = true;
                    m_currentBlock = isObjectBlock;
                    clearCaches();
                    SpeculatedType prediction = getPrediction();

                    Node* base = get(nextResult);
                    auto* doneImpl = m_vm->propertyNames->done.impl();
                    unsigned identifierNumber = m_graph.identifiers().ensure(doneImpl);

                    AccessType type = AccessType::GetById;

                    GetByStatus getByStatus = GetByStatus::computeFor(
                        m_inlineStackTop->m_profiledBlock,
                        m_inlineStackTop->m_baselineMap, m_icContextStack,
                        currentCodeOrigin());

                    handleGetById(bytecode.m_done, prediction, base, CacheableIdentifier::createFromImmortalIdentifier(doneImpl), identifierNumber, getByStatus, type, nextCheckpoint());
                    // Set a value for m_value so we don't exit on it differing from what we expected.
                    set(bytecode.m_value, jsConstant(m_graph.bottomValueMatchingSpeculation(valuePredicition)));
                    progressToNextCheckpoint();

                    BranchData* branchData = m_graph.m_branchData.add();
                    branchData->taken = BranchTarget(continuation);
                    branchData->notTaken = BranchTarget(notDoneBlock);
                    addToGraph(Branch, OpInfo(branchData), get(bytecode.m_done));
                }

                {
                    m_currentBlock = notDoneBlock;
                    clearCaches();

                    Node* base = get(nextResult);
                    auto* valueImpl = m_vm->propertyNames->value.impl();
                    unsigned identifierNumber = m_graph.identifiers().ensure(valueImpl);

                    AccessType type = AccessType::GetById;

                    GetByStatus getByStatus = GetByStatus::computeFor(
                        m_inlineStackTop->m_profiledBlock,
                        m_inlineStackTop->m_baselineMap, m_icContextStack,
                        currentCodeOrigin());

                    handleGetById(bytecode.m_value, valuePredicition, base, CacheableIdentifier::createFromImmortalIdentifier(valueImpl), identifierNumber, getByStatus, type, nextOpcodeIndex());

                    // We're done, exit forwards.
                    m_currentIndex = nextOpcodeIndex();
                    m_exitOK = true;
                    processSetLocalQueue();

                    addToGraph(Jump, OpInfo(continuation));
                }

                generatedCase = true;
            }

            if (!generatedCase) {
                Node* result = jsConstant(JSValue());
                addToGraph(ForceOSRExit);
                set(bytecode.m_value, result);
                set(bytecode.m_done, result);

                // Do our set locals. We don't want to run our get by id again so we move to the next bytecode.
                m_currentIndex = BytecodeIndex(m_currentIndex.offset() + currentInstruction->size());
                m_exitOK = true;
                processSetLocalQueue();

                addToGraph(Jump, OpInfo(continuation));
            }

            m_currentIndex = startIndex;
            m_currentBlock = continuation;
            clearCaches();

            NEXT_OPCODE(op_iterator_next);
        }

        case op_jeq_ptr: {
            auto bytecode = currentInstruction->as<OpJeqPtr>();
            JSValue constant = m_inlineStackTop->m_codeBlock->getConstant(bytecode.m_specialPointer);
            FrozenValue* frozenPointer = m_graph.freezeStrong(constant);
            ASSERT(frozenPointer->cell() == constant);
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* child = get(bytecode.m_value);
            Node* condition = addToGraph(CompareEqPtr, OpInfo(frozenPointer), child);
            addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + relativeOffset, m_currentIndex.offset() + currentInstruction->size())), condition);
            LAST_OPCODE(op_jeq_ptr);
        }

        case op_jneq_ptr: {
            auto bytecode = currentInstruction->as<OpJneqPtr>();
            FrozenValue* frozenPointer = m_graph.freezeStrong(m_inlineStackTop->m_codeBlock->getConstant(bytecode.m_specialPointer));
            unsigned relativeOffset = jumpTarget(bytecode.m_targetLabel);
            Node* child = get(bytecode.m_value);
            if (bytecode.metadata(codeBlock).m_hasJumped) {
                Node* condition = addToGraph(CompareEqPtr, OpInfo(frozenPointer), child);
                addToGraph(Branch, OpInfo(branchData(m_currentIndex.offset() + currentInstruction->size(), m_currentIndex.offset() + relativeOffset)), condition);
                LAST_OPCODE(op_jneq_ptr);
            }

            // We need to phantom any local that is live on the taken block but not live on the not-taken block. i.e. `set of locals
            // live at head of taken` - `set of locals live at head of not-taken`. Otherwise, there are no "uses" to preserve the
            // those locals for OSR after this point. Since computing this precisely is somewhat non-trivial, we instead Phantom
            // everything live at the head of the taken block.
            auto addFlushDirect = [&] (InlineCallFrame* inlineCallFrame, Operand operand) {
                // We don't need to flush anything here since that should be handled by the terminal of the not-taken block.
                UNUSED_PARAM(inlineCallFrame);
                ASSERT_UNUSED(operand, unmapOperand(inlineCallFrame, operand).isArgument() || operand == m_graph.m_codeBlock->scopeRegister());
            };
            auto addPhantomLocalDirect = [&] (InlineCallFrame*, Operand operand) { phantomLocalDirect(operand); };
            // The addPhantomLocalDirect part of flushForTerminal happens to be exactly what we want so let's just call that.
            flushForTerminalImpl(CodeOrigin(BytecodeIndex(m_currentIndex.offset() + relativeOffset), inlineCallFrame()), addFlushDirect, addPhantomLocalDirect);

            addToGraph(CheckIsConstant, OpInfo(frozenPointer), child);
            NEXT_OPCODE(op_jneq_ptr);
        }

        case op_resolve_scope: {
            auto bytecode = currentInstruction->as<OpResolveScope>();
            auto& metadata = bytecode.metadata(codeBlock);

            ResolveType resolveType;
            unsigned depth;
            JSScope* constantScope = nullptr;
            JSCell* lexicalEnvironment = nullptr;
            SymbolTable* symbolTable = nullptr;
            {
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                resolveType = metadata.m_resolveType;
                depth = metadata.m_localScopeDepth;
                switch (resolveType) {
                case GlobalProperty:
                case GlobalVar:
                case GlobalPropertyWithVarInjectionChecks:
                case GlobalVarWithVarInjectionChecks:
                case GlobalLexicalVar:
                case GlobalLexicalVarWithVarInjectionChecks:
                    constantScope = metadata.m_constantScope.get();
                    break;
                case ModuleVar:
                    lexicalEnvironment = metadata.m_lexicalEnvironment.get();
                    break;
                case ResolvedClosureVar:
                case ClosureVar:
                case ClosureVarWithVarInjectionChecks:
                    symbolTable = metadata.m_symbolTable.get();
                    break;
                default:
                    break;
                }
            }

            if (needsDynamicLookup(resolveType, op_resolve_scope)) {
                unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_var];
                set(bytecode.m_dst, addToGraph(ResolveScope, OpInfo(identifierNumber), get(bytecode.m_scope)));
                NEXT_OPCODE(op_resolve_scope);
            }

            // get_from_scope and put_to_scope depend on this watchpoint forcing OSR exit, so they don't add their own watchpoints.
            if (needsVarInjectionChecks(resolveType))
                m_graph.watchpoints().addLazily(m_inlineStackTop->m_codeBlock->globalObject()->varInjectionWatchpointSet());

            if (resolveType == GlobalProperty || resolveType == GlobalPropertyWithVarInjectionChecks) {
                JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();
                unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_var];
                if (!m_graph.watchGlobalProperty(globalObject, identifierNumber))
                    addToGraph(ForceOSRExit);
            }

            switch (resolveType) {
            case GlobalProperty:
            case GlobalVar:
            case GlobalPropertyWithVarInjectionChecks:
            case GlobalVarWithVarInjectionChecks:
            case GlobalLexicalVar:
            case GlobalLexicalVarWithVarInjectionChecks: {
                RELEASE_ASSERT(constantScope);
                RELEASE_ASSERT(constantScope == JSScope::constantScopeForCodeBlock(resolveType, m_inlineStackTop->m_codeBlock));
                set(bytecode.m_dst, weakJSConstant(constantScope));
                addToGraph(Phantom, get(bytecode.m_scope));
                break;
            }
            case ModuleVar: {
                // Since the value of the "scope" virtual register is not used in LLInt / baseline op_resolve_scope with ModuleVar,
                // we need not to keep it alive by the Phantom node.
                // Module environment is already strongly referenced by the CodeBlock.
                set(bytecode.m_dst, weakJSConstant(lexicalEnvironment));
                break;
            }
            case ResolvedClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* localBase = get(bytecode.m_scope);
                addToGraph(Phantom, localBase); // OSR exit cannot handle resolve_scope on a DCE'd scope.
                
                // We have various forms of constant folding here. This is necessary to avoid
                // spurious recompiles in dead-but-foldable code.

                if (symbolTable) {
                    if (JSScope* scope = symbolTable->singleton().inferredValue()) {
                        m_graph.watchpoints().addLazily(symbolTable);
                        set(bytecode.m_dst, weakJSConstant(scope));
                        break;
                    }
                }
                if (JSScope* scope = localBase->dynamicCastConstant<JSScope*>()) {
                    for (unsigned n = depth; n--;)
                        scope = scope->next();
                    set(bytecode.m_dst, weakJSConstant(scope));
                    break;
                }
                for (unsigned n = depth; n--;)
                    localBase = addToGraph(SkipScope, localBase);
                set(bytecode.m_dst, localBase);
                break;
            }
            case UnresolvedProperty:
            case UnresolvedPropertyWithVarInjectionChecks: {
                addToGraph(Phantom, get(bytecode.m_scope));
                addToGraph(ForceOSRExit);
                set(bytecode.m_dst, addToGraph(JSConstant, OpInfo(m_constantNull)));
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
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
            set(bytecode.m_dst, addToGraph(ResolveScopeForHoistingFuncDeclInEval, OpInfo(identifierNumber), get(bytecode.m_scope)));

            NEXT_OPCODE(op_resolve_scope_for_hoisting_func_decl_in_eval);
        }

        case op_get_from_scope: {
            auto bytecode = currentInstruction->as<OpGetFromScope>();
            auto& metadata = bytecode.metadata(codeBlock);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_var];
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];

            ResolveType resolveType;
            GetPutInfo getPutInfo(0);
            Structure* structure = nullptr;
            WatchpointSet* watchpoints = nullptr;
            uintptr_t operand;
            {
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                getPutInfo = metadata.m_getPutInfo;
                resolveType = getPutInfo.resolveType();
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks || resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                    watchpoints = metadata.m_watchpointSet;
                else if (resolveType != UnresolvedProperty && resolveType != UnresolvedPropertyWithVarInjectionChecks)
                    structure = metadata.m_structure.get();
                operand = metadata.m_operand;
            }

            if (needsDynamicLookup(resolveType, op_get_from_scope)) {
                uint64_t opInfo1 = makeDynamicVarOpInfo(identifierNumber, getPutInfo.operand());
                SpeculatedType prediction = getPrediction();
                set(bytecode.m_dst,
                    addToGraph(GetDynamicVar, OpInfo(opInfo1), OpInfo(prediction), get(bytecode.m_scope)));
                NEXT_OPCODE(op_get_from_scope);
            }

            UNUSED_PARAM(watchpoints); // We will use this in the future. For now we set it as a way of documenting the fact that that's what index 5 is in GlobalVar mode.

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                if (!m_graph.watchGlobalProperty(globalObject, identifierNumber))
                    addToGraph(ForceOSRExit);

                SpeculatedType prediction = getPrediction();

                GetByStatus status = GetByStatus::computeFor(structure, uid);
                if (status.state() != GetByStatus::Simple
                    || status.numVariants() != 1
                    || status[0].structureSet().size() != 1) {
                    set(bytecode.m_dst, addToGraph(GetByIdFlush, OpInfo(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid)), OpInfo(prediction), get(bytecode.m_scope)));
                    break;
                }

                Node* base = weakJSConstant(globalObject);
                Node* result = load(prediction, base, identifierNumber, status[0]);
                addToGraph(Phantom, get(bytecode.m_scope));
                set(bytecode.m_dst, result);
                break;
            }
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks:
            case GlobalLexicalVar:
            case GlobalLexicalVarWithVarInjectionChecks: {
                addToGraph(Phantom, get(bytecode.m_scope));
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
                        m_graph.watchpoints().addLazily(*watchpointSet);
                        set(bytecode.m_dst, weakJSConstant(value));
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
                set(bytecode.m_dst, value);
                break;
            }
            case ResolvedClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(bytecode.m_scope);
                
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
                    set(bytecode.m_dst, weakJSConstant(value));
                    break;
                }
                SpeculatedType prediction = getPrediction();
                set(bytecode.m_dst,
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
            unsigned identifierNumber = bytecode.m_var;
            if (identifierNumber != UINT_MAX)
                identifierNumber = m_inlineStackTop->m_identifierRemap[identifierNumber];
            UniquedStringImpl* uid;
            if (identifierNumber != UINT_MAX)
                uid = m_graph.identifiers()[identifierNumber];
            else
                uid = nullptr;
            
            ResolveType resolveType;
            GetPutInfo getPutInfo(0);
            Structure* structure = nullptr;
            WatchpointSet* watchpoints = nullptr;
            uintptr_t operand;
            {
                ConcurrentJSLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                getPutInfo = metadata.m_getPutInfo;
                resolveType = getPutInfo.resolveType();
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks || resolveType == ResolvedClosureVar || resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                    watchpoints = metadata.m_watchpointSet;
                else if (resolveType != UnresolvedProperty && resolveType != UnresolvedPropertyWithVarInjectionChecks)
                    structure = metadata.m_structure.get();
                operand = metadata.m_operand;
            }

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            if (needsDynamicLookup(resolveType, op_put_to_scope)) {
                ASSERT(identifierNumber != UINT_MAX);
                uint64_t opInfo1 = makeDynamicVarOpInfo(identifierNumber, getPutInfo.operand());
                addToGraph(PutDynamicVar, OpInfo(opInfo1), OpInfo(getPutInfo.ecmaMode()), get(bytecode.m_scope), get(bytecode.m_value));
                NEXT_OPCODE(op_put_to_scope);
            }

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                if (!m_graph.watchGlobalProperty(globalObject, identifierNumber))
                    addToGraph(ForceOSRExit);

                PutByStatus status;
                if (uid)
                    status = PutByStatus::computeFor(globalObject, structure, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid), false, PrivateFieldPutKind::none());
                else
                    status = PutByStatus(PutByStatus::LikelyTakesSlowPath);
                if (status.numVariants() != 1
                    || status[0].kind() != PutByVariant::Replace
                    || status[0].structure().size() != 1) {
                    addToGraph(PutById, OpInfo(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid)), OpInfo(bytecode.m_getPutInfo.ecmaMode()), get(bytecode.m_scope), get(bytecode.m_value));
                    break;
                }
                Node* base = weakJSConstant(globalObject);
                replace(base, identifierNumber, status[0], get(bytecode.m_value));
                // Keep scope alive until after put.
                addToGraph(Phantom, get(bytecode.m_scope));
                break;
            }
            case GlobalLexicalVar:
            case GlobalLexicalVarWithVarInjectionChecks:
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks: {
                if (!isInitialization(getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                    SpeculatedType prediction = SpecEmpty;
                    Node* value = addToGraph(GetGlobalLexicalVariable, OpInfo(operand), OpInfo(prediction));
                    addToGraph(CheckNotEmpty, value);
                }
                if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks)
                    m_graph.watchpoints().addLazily(globalObject->varReadOnlyWatchpointSet());

                JSSegmentedVariableObject* scopeObject = jsCast<JSSegmentedVariableObject*>(JSScope::constantScopeForCodeBlock(resolveType, m_inlineStackTop->m_codeBlock));
                if (watchpoints) {
                    SymbolTableEntry entry = scopeObject->symbolTable()->get(uid);
                    ASSERT_UNUSED(entry, watchpoints == entry.watchpointSet());
                }
                Node* valueNode = get(bytecode.m_value);
                addToGraph(PutGlobalVariable, OpInfo(operand), weakJSConstant(scopeObject), valueNode);
                if (watchpoints && watchpoints->state() != IsInvalidated) {
                    // Must happen after the store. See comment for GetGlobalVar.
                    addToGraph(NotifyWrite, OpInfo(watchpoints));
                }
                // Keep scope alive until after put.
                addToGraph(Phantom, get(bytecode.m_scope));
                break;
            }
            case ResolvedClosureVar:
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(bytecode.m_scope);
                Node* valueNode = get(bytecode.m_value);

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
            addToGraph((Options::usePollingTraps() || m_graph.m_plan.isUnlinked()) ? CheckTraps : InvalidationPoint);
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
            ASSERT(bytecode.m_symbolTable.isConstant() && bytecode.m_initialValue.isConstant());
            FrozenValue* symbolTable = m_graph.freezeStrong(m_inlineStackTop->m_codeBlock->getConstant(bytecode.m_symbolTable));
            FrozenValue* initialValue = m_graph.freezeStrong(m_inlineStackTop->m_codeBlock->getConstant(bytecode.m_initialValue));
            Node* scope = get(bytecode.m_scope);
            Node* lexicalEnvironment = addToGraph(CreateActivation, OpInfo(symbolTable), OpInfo(initialValue), scope);
            set(bytecode.m_dst, lexicalEnvironment);
            NEXT_OPCODE(op_create_lexical_environment);
        }

        case op_push_with_scope: {
            auto bytecode = currentInstruction->as<OpPushWithScope>();
            Node* currentScope = get(bytecode.m_currentScope);
            Node* object = get(bytecode.m_newScope);
            set(bytecode.m_dst, addToGraph(PushWithScope, currentScope, object));
            NEXT_OPCODE(op_push_with_scope);
        }

        case op_get_parent_scope: {
            auto bytecode = currentInstruction->as<OpGetParentScope>();
            Node* currentScope = get(bytecode.m_scope);
            Node* newScope = addToGraph(SkipScope, currentScope);
            set(bytecode.m_dst, newScope);
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
            if (JSFunction* function = callee->dynamicCastConstant<JSFunction*>())
                result = weakJSConstant(function->scope());
            else
                result = addToGraph(GetScope, callee);
            set(bytecode.m_dst, result);
            NEXT_OPCODE(op_get_scope);
        }

        case op_argument_count: {
            auto bytecode = currentInstruction->as<OpArgumentCount>();
            Node* sub = addToGraph(ArithSub, OpInfo(Arith::Unchecked), OpInfo(SpecInt32Only), getArgumentCount(), addToGraph(JSConstant, OpInfo(m_constantOne)));
            set(bytecode.m_dst, sub);
            NEXT_OPCODE(op_argument_count);
        }

        case op_create_direct_arguments: {
            auto bytecode = currentInstruction->as<OpCreateDirectArguments>();
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateDirectArguments);
            set(bytecode.m_dst, createArguments);
            NEXT_OPCODE(op_create_direct_arguments);
        }
            
        case op_create_scoped_arguments: {
            auto bytecode = currentInstruction->as<OpCreateScopedArguments>();
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateScopedArguments, get(bytecode.m_scope));
            set(bytecode.m_dst, createArguments);
            NEXT_OPCODE(op_create_scoped_arguments);
        }

        case op_create_cloned_arguments: {
            auto bytecode = currentInstruction->as<OpCreateClonedArguments>();
            noticeArgumentsUse();
            Node* createArguments = addToGraph(CreateClonedArguments);
            set(bytecode.m_dst, createArguments);
            NEXT_OPCODE(op_create_cloned_arguments);
        }

        case op_create_arguments_butterfly: {
            auto bytecode = currentInstruction->as<OpCreateArgumentsButterfly>();
            noticeArgumentsUse();
            set(bytecode.m_dst, addToGraph(CreateArgumentsButterfly));
            NEXT_OPCODE(op_create_arguments_butterfly);
        }
            
        case op_get_from_arguments: {
            auto bytecode = currentInstruction->as<OpGetFromArguments>();
            set(bytecode.m_dst,
                addToGraph(
                    GetFromArguments,
                    OpInfo(bytecode.m_index),
                    OpInfo(getPrediction()),
                    get(bytecode.m_arguments)));
            NEXT_OPCODE(op_get_from_arguments);
        }
            
        case op_put_to_arguments: {
            auto bytecode = currentInstruction->as<OpPutToArguments>();
            addToGraph(
                PutToArguments,
                OpInfo(bytecode.m_index),
                get(bytecode.m_arguments),
                get(bytecode.m_value));
            NEXT_OPCODE(op_put_to_arguments);
        }

        case op_get_argument: {
            auto bytecode = currentInstruction->as<OpGetArgument>();
            InlineCallFrame* inlineCallFrame = this->inlineCallFrame();
            Node* argument;
            int32_t argumentIndexIncludingThis = bytecode.m_index;
            if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
                int32_t argumentCountIncludingThisWithFixup = inlineCallFrame->m_argumentsWithFixup.size();
                if (argumentIndexIncludingThis < argumentCountIncludingThisWithFixup)
                    argument = get(virtualRegisterForArgumentIncludingThis(argumentIndexIncludingThis));
                else
                    argument = addToGraph(JSConstant, OpInfo(m_constantUndefined));
            } else
                argument = addToGraph(GetArgument, OpInfo(argumentIndexIncludingThis), OpInfo(getPrediction()));
            set(bytecode.m_dst, argument);
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
            Node* func = get(bytecode.m_function);
            Node* name = get(bytecode.m_name);
            addToGraph(SetFunctionName, func, name);
            NEXT_OPCODE(op_set_function_name);
        }

        case op_typeof: {
            auto bytecode = currentInstruction->as<OpTypeof>();
            set(bytecode.m_dst, addToGraph(TypeOf, get(bytecode.m_value)));
            NEXT_OPCODE(op_typeof);
        }

        case op_to_number: {
            auto bytecode = currentInstruction->as<OpToNumber>();
            SpeculatedType prediction = getPrediction();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(ToNumber, OpInfo(0), OpInfo(prediction), value));
            NEXT_OPCODE(op_to_number);
        }

        case op_to_numeric: {
            auto bytecode = currentInstruction->as<OpToNumeric>();
            SpeculatedType prediction = getPrediction();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(ToNumeric, OpInfo(0), OpInfo(prediction), value));
            NEXT_OPCODE(op_to_numeric);
        }

        case op_to_string: {
            auto bytecode = currentInstruction->as<OpToString>();
            Node* value = get(bytecode.m_operand);
            set(bytecode.m_dst, addToGraph(ToString, value));
            NEXT_OPCODE(op_to_string);
        }

        case op_to_object: {
            auto bytecode = currentInstruction->as<OpToObject>();
            SpeculatedType prediction = getPrediction();
            Node* value = get(bytecode.m_operand);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_message];
            set(bytecode.m_dst, addToGraph(ToObject, OpInfo(identifierNumber), OpInfo(prediction), value));
            NEXT_OPCODE(op_to_object);
        }

        case op_in_by_val: {
            auto bytecode = currentInstruction->as<OpInByVal>();
            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            bool compiledAsInById = false;

            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                InByStatus status = InByStatus::computeFor(
                    m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap,
                    m_icContextStack, currentCodeOrigin());

                if (CacheableIdentifier identifier = status.singleIdentifier()) {
                    UniquedStringImpl* uid = identifier.uid();
                    m_graph.identifiers().ensure(uid);
                    if (identifier.isCell()) {
                        FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                        if (identifier.isSymbolCell())
                            addToGraph(CheckIsConstant, OpInfo(frozen), property);
                        else
                            addToGraph(CheckIdent, OpInfo(uid), property);
                    } else
                        addToGraph(CheckIdent, OpInfo(uid), property);

                    handleInById(bytecode.m_dst, base, identifier, status);
                    compiledAsInById = true;
                }
            }

            if (!compiledAsInById) {
                ArrayMode arrayMode = getArrayMode(bytecode.metadata(codeBlock).m_arrayProfile, Array::Read);
                set(bytecode.m_dst, addToGraph(InByVal, OpInfo(arrayMode.asWord()), base, property));
            }
            NEXT_OPCODE(op_in_by_val);
        }

        case op_in_by_id: {
            auto bytecode = currentInstruction->as<OpInById>();
            Node* base = get(bytecode.m_base);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
            UniquedStringImpl* uid = m_graph.identifiers()[identifierNumber];
            InByStatus status = InByStatus::computeFor(
                m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap,
                m_icContextStack, currentCodeOrigin());
            handleInById(bytecode.m_dst, base, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_inlineStackTop->m_profiledBlock, uid), status);
            NEXT_OPCODE(op_in_by_id);
        }
        
        case op_has_private_name: {
            auto bytecode = currentInstruction->as<OpHasPrivateName>();
            Node* base = get(bytecode.m_base);
            Node* property = get(bytecode.m_property);
            bool compiledAsInById = false;

            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                InByStatus status = InByStatus::computeFor(
                    m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap,
                    m_icContextStack, currentCodeOrigin());

                if (CacheableIdentifier identifier = status.singleIdentifier()) {
                    m_graph.identifiers().ensure(identifier.uid());
                    ASSERT(identifier.isSymbolCell());
                    FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                    addToGraph(CheckIsConstant, OpInfo(frozen), property);
                    handleInById(bytecode.m_dst, base, identifier, status);
                    compiledAsInById = true;
                }
            }

            if (!compiledAsInById)
                set(bytecode.m_dst, addToGraph(HasPrivateName, base, property));
            NEXT_OPCODE(op_has_private_name);
        }

        case op_has_private_brand: {
            auto bytecode = currentInstruction->as<OpHasPrivateBrand>();
            Node* base = get(bytecode.m_base);
            Node* brand = get(bytecode.m_brand);
            bool compiledAsMatchStructure = false;

            if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {

                InByStatus status = InByStatus::computeFor(
                    m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap,
                    m_icContextStack, currentCodeOrigin());

                if (CacheableIdentifier identifier = status.singleIdentifier()) {
                    m_graph.identifiers().ensure(identifier.uid());
                    ASSERT(identifier.isSymbolCell());
                    FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                    addToGraph(CheckIsConstant, OpInfo(frozen), brand);
                    compiledAsMatchStructure = handleInByAsMatchStructure(bytecode.m_dst, base, status);
                }
            }

            if (!compiledAsMatchStructure)
                set(bytecode.m_dst, addToGraph(HasPrivateBrand, base, brand));
            NEXT_OPCODE(op_has_private_brand);
        }

        case op_get_property_enumerator: {
            auto bytecode = currentInstruction->as<OpGetPropertyEnumerator>();
            set(bytecode.m_dst, addToGraph(GetPropertyEnumerator, get(bytecode.m_base)));
            NEXT_OPCODE(op_get_property_enumerator);
        }

        case op_enumerator_next: {
            auto bytecode = currentInstruction->as<OpEnumeratorNext>();
            auto& metadata = bytecode.metadata(codeBlock);
            ArrayMode arrayMode = getArrayMode(metadata.m_arrayProfile, Array::Read);
            Node* base = get(bytecode.m_base);
            Node* index = get(bytecode.m_index);
            Node* enumerator = get(bytecode.m_enumerator);
            Node* mode = get(bytecode.m_mode);

            auto seenModes = OptionSet<JSPropertyNameEnumerator::Flag>::fromRaw(metadata.m_enumeratorMetadata);

            if (!seenModes)
                addToGraph(ForceOSRExit);

            addVarArgChild(base);
            addVarArgChild(index);
            addVarArgChild(mode);
            addVarArgChild(enumerator);
            addVarArgChild(nullptr); // storage for IndexedMode only.
            Node* updatedIndexAndMode = addToGraph(Node::VarArg, EnumeratorNextUpdateIndexAndMode, OpInfo(arrayMode.asWord()), OpInfo(seenModes));

            Node* updatedMode = addToGraph(EnumeratorNextExtractMode, updatedIndexAndMode);
            set(bytecode.m_mode, updatedMode);

            Node* updatedIndex = addToGraph(EnumeratorNextExtractIndex, updatedIndexAndMode);
            set(bytecode.m_index, updatedIndex);

            set(bytecode.m_propertyName, addToGraph(EnumeratorNextUpdatePropertyName, OpInfo(), OpInfo(seenModes), updatedIndex, updatedMode, enumerator));

            NEXT_OPCODE(op_enumerator_next);
        }

        case op_enumerator_get_by_val: {
            auto bytecode = currentInstruction->as<OpEnumeratorGetByVal>();
            auto& metadata = bytecode.metadata(codeBlock);
            ArrayMode arrayMode = getArrayMode(metadata.m_arrayProfile, Array::Read);
            SpeculatedType speculation = getPredictionWithoutOSRExit();

            Node* base = get(bytecode.m_base);
            Node* propertyName = get(bytecode.m_propertyName);
            Node* index = get(bytecode.m_index);
            Node* mode = get(bytecode.m_mode);
            Node* enumerator = get(bytecode.m_enumerator);

            auto seenModes = OptionSet<JSPropertyNameEnumerator::Flag>::fromRaw(metadata.m_enumeratorMetadata);
            if (!seenModes)
                addToGraph(ForceOSRExit);

            if (seenModes == JSPropertyNameEnumerator::IndexedMode) {
                Node* badMode = addToGraph(ArithBitAnd, mode, jsConstant(jsNumber(JSPropertyNameEnumerator::GenericMode | JSPropertyNameEnumerator::OwnStructureMode)));

                // We know the ArithBitAnd cannot have effects so it's ok to exit here.
                m_exitOK = true;
                addToGraph(ExitOK);

                addToGraph(CheckIsConstant, OpInfo(m_graph.freezeStrong(jsNumber(0))), badMode);

                addVarArgChild(base);
                addVarArgChild(index); // Use index so we'll use the normal indexed optimizations.
                addVarArgChild(nullptr); // For property storage to match GetByVal.
                set(bytecode.m_dst, addToGraph(Node::VarArg, GetByVal, OpInfo(arrayMode.asWord()), OpInfo(speculation)));

                addToGraph(Phantom, propertyName);
                addToGraph(Phantom, enumerator);
                NEXT_OPCODE(op_enumerator_get_by_val);
            }

            // FIXME: Checking for a BadConstantValue causes us to always use the Generic variant if we switched from IndexedMode -> IndexedMode + OwnStructureMode even though that might be fine.
            if (!seenModes.containsAny({ JSPropertyNameEnumerator::GenericMode, JSPropertyNameEnumerator::HasSeenOwnStructureModeStructureMismatch })
                && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {
                Node* modeTest = addToGraph(SameValue, mode, jsConstant(jsNumber(static_cast<uint8_t>(JSPropertyNameEnumerator::GenericMode))));
                addToGraph(CheckIsConstant, OpInfo(m_graph.freezeStrong(jsBoolean(false))), modeTest);

                addVarArgChild(base);
                addVarArgChild(index); // Use index so we'll use the normal indexed optimizations.
                addVarArgChild(nullptr); // For property storage to match GetByVal.
                addVarArgChild(index);
                addVarArgChild(mode);
                addVarArgChild(enumerator);
                set(bytecode.m_dst, addToGraph(Node::VarArg, EnumeratorGetByVal, OpInfo(arrayMode.asWord()), OpInfo(speculation)));

                addToGraph(Phantom, propertyName);
                NEXT_OPCODE(op_enumerator_get_by_val);
            }

            addVarArgChild(base);
            addVarArgChild(propertyName);
            addVarArgChild(nullptr); // For property storage to match GetByVal.
            addVarArgChild(index);
            addVarArgChild(mode);
            addVarArgChild(enumerator);
            set(bytecode.m_dst, addToGraph(Node::VarArg, EnumeratorGetByVal, OpInfo(arrayMode.asWord()), OpInfo(speculation)));

            NEXT_OPCODE(op_enumerator_get_by_val);
        }

        case op_enumerator_in_by_val: {
            auto bytecode = currentInstruction->as<OpEnumeratorInByVal>();
            auto& metadata = bytecode.metadata(codeBlock);
            ArrayMode arrayMode = getArrayMode(metadata.m_arrayProfile, Array::Read);

            addVarArgChild(get(bytecode.m_base));
            addVarArgChild(get(bytecode.m_propertyName));
            addVarArgChild(get(bytecode.m_index));
            addVarArgChild(get(bytecode.m_mode));
            addVarArgChild(get(bytecode.m_enumerator));
            set(bytecode.m_dst, addToGraph(Node::VarArg, EnumeratorInByVal, OpInfo(arrayMode.asWord()), OpInfo(metadata.m_enumeratorMetadata)));

            NEXT_OPCODE(op_enumerator_in_by_val);
        }

        case op_enumerator_has_own_property: {
            auto bytecode = currentInstruction->as<OpEnumeratorHasOwnProperty>();
            auto& metadata = bytecode.metadata(codeBlock);
            ArrayMode arrayMode = getArrayMode(metadata.m_arrayProfile, Array::Read);

            addVarArgChild(get(bytecode.m_base));
            addVarArgChild(get(bytecode.m_propertyName));
            addVarArgChild(get(bytecode.m_index));
            addVarArgChild(get(bytecode.m_mode));
            addVarArgChild(get(bytecode.m_enumerator));
            set(bytecode.m_dst, addToGraph(Node::VarArg, EnumeratorHasOwnProperty, OpInfo(arrayMode.asWord()), OpInfo(metadata.m_enumeratorMetadata)));

            NEXT_OPCODE(op_enumerator_has_own_property);
        }

        case op_get_internal_field: {
            auto bytecode = currentInstruction->as<OpGetInternalField>();
            set(bytecode.m_dst, addToGraph(GetInternalField, OpInfo(bytecode.m_index), OpInfo(getPrediction()), get(bytecode.m_base)));
            NEXT_OPCODE(op_get_internal_field);
        }

        case op_put_internal_field: {
            auto bytecode = currentInstruction->as<OpPutInternalField>();
            addToGraph(PutInternalField, OpInfo(bytecode.m_index), get(bytecode.m_base), get(bytecode.m_value));
            NEXT_OPCODE(op_put_internal_field);
        }
            
        case op_log_shadow_chicken_prologue: {
            auto bytecode = currentInstruction->as<OpLogShadowChickenPrologue>();
            if (!m_inlineStackTop->m_inlineCallFrame)
                addToGraph(LogShadowChickenPrologue, get(bytecode.m_scope));
            NEXT_OPCODE(op_log_shadow_chicken_prologue);
        }

        case op_log_shadow_chicken_tail: {
            auto bytecode = currentInstruction->as<OpLogShadowChickenTail>();
            if (!m_inlineStackTop->m_inlineCallFrame) {
                // FIXME: The right solution for inlining is to elide these whenever the tail call
                // ends up being inlined.
                // https://bugs.webkit.org/show_bug.cgi?id=155686
                addToGraph(LogShadowChickenTail, get(bytecode.m_thisValue), get(bytecode.m_scope));
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
        node->targetBlock() = blockForBytecodeIndex(possibleTargets, BytecodeIndex(node->targetBytecodeOffsetDuringParsing()));
        break;
        
    case Branch: {
        BranchData* data = node->branchData();
        data->taken.block = blockForBytecodeIndex(possibleTargets, BytecodeIndex(data->takenBytecodeIndex()));
        data->notTaken.block = blockForBytecodeIndex(possibleTargets, BytecodeIndex(data->notTakenBytecodeIndex()));
        break;
    }
        
    case Switch: {
        SwitchData* data = node->switchData();
        for (unsigned i = node->switchData()->cases.size(); i--;)
            data->cases[i].target.block = blockForBytecodeIndex(possibleTargets, BytecodeIndex(data->cases[i].target.bytecodeIndex()));
        data->fallThrough.block = blockForBytecodeIndex(possibleTargets, BytecodeIndex(data->fallThrough.bytecodeIndex()));
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
    Operand returnValue,
    VirtualRegister inlineCallFrameStart,
    int argumentCountIncludingThis,
    InlineCallFrame::Kind kind,
    BasicBlock* continuationBlock)
    : m_byteCodeParser(byteCodeParser)
    , m_codeBlock(codeBlock)
    , m_profiledBlock(profiledBlock)
    , m_continuationBlock(continuationBlock)
    , m_returnValue(returnValue)
    , m_caller(byteCodeParser->m_inlineStackTop)
{
    {
        m_exitProfile.initialize(m_profiledBlock->unlinkedCodeBlock());

        ConcurrentJSLocker locker(m_profiledBlock->m_lock);
        m_lazyOperands.initialize(locker, m_profiledBlock->lazyOperandValueProfiles(locker));
        
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
        m_inlineCallFrame->setTmpOffset((m_caller->m_inlineCallFrame ? m_caller->m_inlineCallFrame->tmpOffset : 0) + m_caller->m_codeBlock->numTmps());
        m_inlineCallFrame->setStackOffset(inlineCallFrameStart.offset() - CallFrame::headerSizeInRegisters);
        m_inlineCallFrame->argumentCountIncludingThis = argumentCountIncludingThis;
        RELEASE_ASSERT(m_inlineCallFrame->argumentCountIncludingThis == argumentCountIncludingThis);
        if (callee) {
            m_inlineCallFrame->calleeRecovery = ValueRecovery::constant(callee);
            m_inlineCallFrame->isClosureCall = false;
        } else
            m_inlineCallFrame->isClosureCall = true;
        m_inlineCallFrame->directCaller = byteCodeParser->currentCodeOrigin();
        m_inlineCallFrame->m_argumentsWithFixup = FixedVector<ValueRecovery>(argumentCountIncludingThisWithFixup); // Set the number of arguments including this, but don't configure the value recoveries, yet.
        m_inlineCallFrame->kind = kind;
        
        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i) {
            UniquedStringImpl* rep = codeBlock->identifier(i).impl();
            unsigned index = byteCodeParser->m_graph.identifiers().ensure(rep);
            m_identifierRemap[i] = index;
        }
    } else {
        // Machine code block case.
        ASSERT(codeBlock == byteCodeParser->m_codeBlock);
        ASSERT(!callee);
        ASSERT(!returnValue.isValid());
        ASSERT(!inlineCallFrameStart.isValid());

        m_inlineCallFrame = nullptr;

        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i)
            m_identifierRemap[i] = i;
    }

    m_switchRemap.resize(codeBlock->numberOfUnlinkedSwitchJumpTables());
    byteCodeParser->m_graph.m_switchJumpTables.resize(byteCodeParser->m_graph.m_switchJumpTables.size() + codeBlock->numberOfUnlinkedSwitchJumpTables());
    for (unsigned i = 0; i < codeBlock->numberOfUnlinkedSwitchJumpTables(); ++i) {
        m_switchRemap[i] = byteCodeParser->m_graph.m_unlinkedSwitchJumpTables.size();
        byteCodeParser->m_graph.m_unlinkedSwitchJumpTables.append(&codeBlock->unlinkedSwitchJumpTable(i));
    }

    m_stringSwitchRemap.resize(codeBlock->numberOfUnlinkedStringSwitchJumpTables());
    byteCodeParser->m_graph.m_stringSwitchJumpTables.resize(byteCodeParser->m_graph.m_stringSwitchJumpTables.size() + codeBlock->numberOfUnlinkedStringSwitchJumpTables());
    for (unsigned i = 0; i < codeBlock->numberOfUnlinkedStringSwitchJumpTables(); ++i) {
        m_stringSwitchRemap[i] = byteCodeParser->m_graph.m_unlinkedStringSwitchJumpTables.size();
        byteCodeParser->m_graph.m_unlinkedStringSwitchJumpTables.append(&codeBlock->unlinkedStringSwitchJumpTable(i));
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
            DeferredSourceDump dump(codeBlock->baselineVersion(), m_codeBlock, JITType::DFGJIT, inlineCallFrame()->directCaller.bytecodeIndex());
            deferredSourceDump.append(dump);
        } else
            deferredSourceDump.append(DeferredSourceDump(codeBlock->baselineVersion()));
    }

    if (UNLIKELY(Options::dumpBytecodeAtDFGTime())) {
        dataLog("Parsing ", *codeBlock);
        if (inlineCallFrame()) {
            dataLog(
                " for inlining at ", CodeBlockWithJITType(m_codeBlock, JITType::DFGJIT),
                " ", inlineCallFrame()->directCaller);
        }
        dataLogLn();
        codeBlock->baselineVersion()->dumpBytecode();
    }

    Vector<JSInstructionStream::Offset, 32> jumpTargets;
    computePreciseJumpTargets(codeBlock, jumpTargets);
    if (UNLIKELY(Options::dumpBytecodeAtDFGTime())) {
        dataLog("Jump targets: ");
        CommaPrinter comma;
        for (unsigned i = 0; i < jumpTargets.size(); ++i)
            dataLog(comma, jumpTargets[i]);
        dataLog("\n");
    }
    
    for (unsigned jumpTargetIndex = 0; jumpTargetIndex <= jumpTargets.size(); ++jumpTargetIndex) {
        // The maximum bytecode offset to go into the current basicblock is either the next jump target, or the end of the instructions.
        unsigned limit = jumpTargetIndex < jumpTargets.size() ? jumpTargets[jumpTargetIndex] : codeBlock->instructions().size();
        ASSERT(m_currentIndex.offset() < limit);

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
            ASSERT(m_currentIndex.offset() <= limit);

            if (m_currentBlock->isEmpty()) {
                // This case only happens if the last instruction was an inlined call with early returns
                // or polymorphic (creating an empty continuation block),
                // and then we hit the limit before putting anything in the continuation block.
                ASSERT(m_currentIndex.offset() == limit);
                makeBlockTargetable(m_currentBlock, m_currentIndex);
            } else {
                ASSERT(m_currentBlock->terminal() || (m_currentIndex.offset() == codeBlock->instructions().size() && inlineCallFrame()));
                m_currentBlock = nullptr;
            }
        } while (m_currentIndex.offset() < limit);
    }

    // Should have reached the end of the instructions.
    ASSERT(m_currentIndex.offset() == codeBlock->instructions().size());
    
    VERBOSE_LOG("Done parsing ", *codeBlock, " (fell off end)\n");
}

template <typename Bytecode>
void ByteCodeParser::handlePutByVal(Bytecode bytecode, BytecodeIndex osrExitIndex)
{
    CodeBlock* codeBlock = m_inlineStackTop->m_codeBlock;
    Node* base = get(bytecode.m_base);
    Node* property = get(bytecode.m_property);
    Node* value = get(bytecode.m_value);
    bool isDirect = Bytecode::opcodeID == op_put_by_val_direct;
    bool compiledAsPutById = false;

    PutByStatus status = PutByStatus::computeFor(m_inlineStackTop->m_profiledBlock, m_inlineStackTop->m_baselineMap, m_icContextStack, currentCodeOrigin());

    if (!m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIdent)
        && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadType)
        && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {
        if (CacheableIdentifier identifier = status.singleIdentifier()) {
            UniquedStringImpl* uid = identifier.uid();
            unsigned identifierNumber = m_graph.identifiers().ensure(uid);
            if (identifier.isCell()) {
                FrozenValue* frozen = m_graph.freezeStrong(identifier.cell());
                if (identifier.isSymbolCell())
                    addToGraph(CheckIsConstant, OpInfo(frozen), property);
                else
                    addToGraph(CheckIdent, OpInfo(uid), property);
            } else
                addToGraph(CheckIdent, OpInfo(uid), property);

            handlePutById(base, identifier, identifierNumber, value, status, isDirect, osrExitIndex, bytecode.m_ecmaMode);
            compiledAsPutById = true;
        } else if (status.takesSlowPath()) {
            // Even though status is taking a slow path, it is possible that this node still has constant identifier and using PutById is always better in that case.
            UniquedStringImpl* uid = nullptr;
            JSCell* propertyCell = nullptr;
            if (auto* symbol = property->dynamicCastConstant<Symbol*>()) {
                uid = &symbol->uid();
                propertyCell = symbol;
                FrozenValue* frozen = m_graph.freezeStrong(symbol);
                addToGraph(CheckIsConstant, OpInfo(frozen), property);
            } else if (auto* string = property->dynamicCastConstant<JSString*>()) {
                auto* impl = string->tryGetValueImpl();
                if (impl && impl->isAtom() && !parseIndex(*const_cast<StringImpl*>(impl))) {
                    uid = bitwise_cast<UniquedStringImpl*>(impl);
                    propertyCell = string;
                    m_graph.freezeStrong(string);
                    addToGraph(CheckIdent, OpInfo(uid), property);
                }
            }

            if (uid) {
                unsigned identifierNumber = m_graph.identifiers().ensure(uid);
                handlePutById(base, CacheableIdentifier::createFromCell(propertyCell), identifierNumber, value, status, isDirect, osrExitIndex, bytecode.m_ecmaMode);
                compiledAsPutById = true;
            }
        }
    }

    if (!compiledAsPutById) {
        ArrayMode arrayMode = getArrayMode(bytecode.metadata(codeBlock).m_arrayProfile, Array::Write);

        addVarArgChild(base);
        addVarArgChild(property);
        addVarArgChild(value);
        addVarArgChild(nullptr); // Leave room for property storage.
        addVarArgChild(nullptr); // Leave room for length.
        Node* putByVal = addToGraph(Node::VarArg, isDirect ? PutByValDirect : PutByVal, OpInfo(arrayMode.asWord()), OpInfo(bytecode.m_ecmaMode));
        m_exitOK = false; // PutByVal and PutByValDirect must be treated as if they clobber exit state, since FixupPhase may make them generic.
        if (status.observedStructureStubInfoSlowPath())
            m_graph.m_slowPutByVal.add(putByVal);
    }
}

template <typename Bytecode>
void ByteCodeParser::handlePutAccessorById(NodeType op, Bytecode bytecode)
{
    Node* base = get(bytecode.m_base);
    unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[bytecode.m_property];
    Node* accessor = get(bytecode.m_accessor);
    addToGraph(op, OpInfo(identifierNumber), OpInfo(bytecode.m_attributes), base, accessor);
}

template <typename Bytecode>
void ByteCodeParser::handlePutAccessorByVal(NodeType op, Bytecode bytecode)
{
    Node* base = get(bytecode.m_base);
    Node* subscript = get(bytecode.m_property);
    Node* accessor = get(bytecode.m_accessor);
    addToGraph(op, OpInfo(bytecode.m_attributes), base, subscript, accessor);
}

template <typename Bytecode>
void ByteCodeParser::handleNewFunc(NodeType op, Bytecode bytecode)
{
    FunctionExecutable* decl = m_inlineStackTop->m_profiledBlock->functionDecl(bytecode.m_functionDecl);
    FrozenValue* frozen = m_graph.freezeStrong(decl);
    Node* scope = get(bytecode.m_scope);
    set(bytecode.m_dst, addToGraph(op, OpInfo(frozen), scope));
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
    FunctionExecutable* expr = m_inlineStackTop->m_profiledBlock->functionExpr(bytecode.m_functionDecl);
    FrozenValue* frozen = m_graph.freezeStrong(expr);
    Node* scope = get(bytecode.m_scope);
    set(bytecode.m_dst, addToGraph(op, OpInfo(frozen), scope));
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
void ByteCodeParser::handleCreateInternalFieldObject(const ClassInfo* classInfo, NodeType createOp, NodeType newOp, Bytecode bytecode)
{
    CodeBlock* codeBlock = m_inlineStackTop->m_codeBlock;
    JSGlobalObject* globalObject = m_graph.globalObjectFor(currentNodeOrigin().semantic);
    Node* callee = get(VirtualRegister(bytecode.m_callee));

    JSFunction* function = callee->dynamicCastConstant<JSFunction*>();
    if (!function) {
        JSCell* cachedFunction = bytecode.metadata(codeBlock).m_cachedCallee.unvalidatedGet();
        if (cachedFunction
            && cachedFunction != JSCell::seenMultipleCalleeObjects()
            && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadConstantValue)) {
            ASSERT(cachedFunction->inherits<JSFunction>());

            FrozenValue* frozen = m_graph.freeze(cachedFunction);
            addToGraph(CheckIsConstant, OpInfo(frozen), callee);

            function = static_cast<JSFunction*>(cachedFunction);
        }
    }

    if (function) {
        if (FunctionRareData* rareData = function->rareData()) {
            if (rareData->allocationProfileWatchpointSet().isStillValid() && globalObject->structureCacheClearedWatchpointSet().isStillValid()) {
                Structure* structure = rareData->internalFunctionAllocationStructure();
                if (structure
                    && structure->classInfoForCells() == classInfo
                    && structure->globalObject() == globalObject) {
                    m_graph.freeze(rareData);
                    m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                    m_graph.freeze(globalObject);
                    m_graph.watchpoints().addLazily(globalObject->structureCacheClearedWatchpointSet());

                    set(VirtualRegister(bytecode.m_dst), addToGraph(newOp, OpInfo(m_graph.registerStructure(structure))));
                    // The callee is still live up to this point.
                    addToGraph(Phantom, callee);
                    return;
                }
            }
        }
    }

    set(VirtualRegister(bytecode.m_dst), addToGraph(createOp, callee));
}

void ByteCodeParser::parse()
{
    // Set during construction.
    ASSERT(!m_currentIndex.offset());
    
    VERBOSE_LOG("Parsing ", *m_codeBlock, "\n");
    
    InlineStackEntry inlineStackEntry(
        this, m_codeBlock, m_profiledBlock, nullptr, VirtualRegister(), VirtualRegister(),
        m_codeBlock->numParameters(), InlineCallFrame::Call, nullptr);
    
    parseCodeBlock();
    linkBlocks(inlineStackEntry.m_unlinkedBlocks, inlineStackEntry.m_blockLinkingTargets);

    // We run backwards propagation now because the soundness of that phase
    // relies on seeing the graph as if it were an IR over bytecode, since
    // the spec-correctness of that phase relies on seeing all bytecode uses.
    // Therefore, we run this pass before we do any pruning of the graph
    // after ForceOSRExit sites.
    performBackwardsPropagation(m_graph);

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
                {
                    Node* node = block->at(nodeIndex);

                    if (node->hasVariableAccessData(m_graph))
                        mapping.operand(node->operand()) = node->variableAccessData();

                    if (node->op() != ForceOSRExit)
                        continue;
                }

                NodeOrigin origin = block->at(nodeIndex)->origin;
                RELEASE_ASSERT(origin.exitOK);

                ++nodeIndex;

                if (validationEnabled()) {
                    // This verifies that we don't need to change any of the successors's predecessor
                    // list after planting the Unreachable below. At this point in the bytecode
                    // parser, we haven't linked up the predecessor lists yet.
                    for (BasicBlock* successor : block->successors())
                        RELEASE_ASSERT(successor->predecessors.isEmpty());
                }

                block->resize(nodeIndex);

                {
                    auto insertLivenessPreservingOp = [&] (InlineCallFrame* inlineCallFrame, NodeType op, Operand operand) {
                        VariableAccessData* variable = mapping.operand(operand);
                        if (!variable) {
                            variable = newVariableAccessData(operand);
                            mapping.operand(operand) = variable;
                        }

                        Operand argument = unmapOperand(inlineCallFrame, operand);
                        if (argument.isArgument() && !argument.isHeader()) {
                            const Vector<ArgumentPosition*>& arguments = m_inlineCallFrameToArgumentPositions.get(inlineCallFrame);
                            arguments[argument.toArgument()]->addVariable(variable);
                        }
                        insertionSet.insertNode(nodeIndex, SpecNone, op, origin, OpInfo(variable));
                    };
                    auto addFlushDirect = [&] (InlineCallFrame* inlineCallFrame, Operand operand) {
                        insertLivenessPreservingOp(inlineCallFrame, Flush, operand);
                    };
                    auto addPhantomLocalDirect = [&] (InlineCallFrame* inlineCallFrame, Operand operand) {
                        insertLivenessPreservingOp(inlineCallFrame, PhantomLocal, operand);
                    };
                    flushForTerminalImpl(origin.semantic, addFlushDirect, addPhantomLocalDirect);
                }

                insertionSet.insertNode(nodeIndex, SpecNone, Unreachable, origin);
                insertionSet.execute(block);

                break;
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

    m_graph.m_tmps = m_numTmps;
    m_graph.m_localVars = m_numLocals;
    m_graph.m_parameterSlots = m_parameterSlots;
}

void parse(Graph& graph)
{
    ByteCodeParser(graph).parse();
}

} } // namespace JSC::DFG

#endif
