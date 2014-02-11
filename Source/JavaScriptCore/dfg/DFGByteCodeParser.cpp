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

#if ENABLE(DFG_JIT)

#include "DFGByteCodeParser.h"

#include "ArrayConstructor.h"
#include "CallLinkStatus.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGArrayMode.h"
#include "DFGCapabilities.h"
#include "DFGJITCode.h"
#include "GetByIdStatus.h"
#include "JSActivation.h"
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
        , m_constantUndefined(UINT_MAX)
        , m_constantNull(UINT_MAX)
        , m_constantNaN(UINT_MAX)
        , m_constant1(UINT_MAX)
        , m_constants(m_codeBlock->numberOfConstantRegisters())
        , m_numArguments(m_codeBlock->numParameters())
        , m_numLocals(m_codeBlock->m_numCalleeRegisters)
        , m_parameterSlots(0)
        , m_numPassedVarArgs(0)
        , m_inlineStackTop(0)
        , m_haveBuiltOperandMaps(false)
        , m_emptyJSValueIndex(UINT_MAX)
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

    // Helper for min and max.
    bool handleMinMax(int resultOperand, NodeType op, int registerOffset, int argumentCountIncludingThis);
    
    // Handle calls. This resolves issues surrounding inlining and intrinsics.
    void handleCall(int result, NodeType op, CodeSpecializationKind, unsigned instructionSize, int callee, int argCount, int registerOffset);
    void handleCall(Instruction* pc, NodeType op, CodeSpecializationKind);
    void emitFunctionChecks(const CallLinkStatus&, Node* callTarget, int registerOffset, CodeSpecializationKind);
    void emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind);
    // Handle inlining. Return true if it succeeded, false if we need to plant a call.
    bool handleInlining(Node* callTargetNode, int resultOperand, const CallLinkStatus&, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, CodeSpecializationKind);
    // Handle intrinsic functions. Return true if it succeeded, false if we need to plant a call.
    bool handleIntrinsic(int resultOperand, Intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction);
    bool handleTypedArrayConstructor(int resultOperand, InternalFunction*, int registerOffset, int argumentCountIncludingThis, TypedArrayType);
    bool handleConstantInternalFunction(int resultOperand, InternalFunction*, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction, CodeSpecializationKind);
    Node* handlePutByOffset(Node* base, unsigned identifier, PropertyOffset, Node* value);
    Node* handleGetByOffset(SpeculatedType, Node* base, unsigned identifierNumber, PropertyOffset);
    void handleGetByOffset(
        int destinationOperand, SpeculatedType, Node* base, unsigned identifierNumber,
        PropertyOffset);
    void handleGetById(
        int destinationOperand, SpeculatedType, Node* base, unsigned identifierNumber,
        const GetByIdStatus&);

    Node* getScope(bool skipTop, unsigned skipCount);
    
    // Prepare to parse a block.
    void prepareToParseBlock();
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
        // Is this a constant?
        if (operand.isConstant()) {
            unsigned constant = operand.toConstantIndex();
            ASSERT(constant < m_constants.size());
            return getJSConstant(constant);
        }

        // Is this an argument?
        if (operand.isArgument())
            return getArgument(operand);

        // Must be a local.
        return getLocal(operand);
    }

    Node* get(VirtualRegister operand)
    {
        if (inlineCallFrame()) {
            if (!inlineCallFrame()->isClosureCall) {
                JSFunction* callee = inlineCallFrame()->calleeConstant();
                if (operand.offset() == JSStack::Callee)
                    return cellConstant(callee);
                if (operand.offset() == JSStack::ScopeChain)
                    return cellConstant(callee->scope());
            }
        } else if (operand.offset() == JSStack::Callee)
            return addToGraph(GetCallee);
        else if (operand.offset() == JSStack::ScopeChain)
            return addToGraph(GetMyScope);
        
        return getDirect(m_inlineStackTop->remapOperand(operand));
    }
    
    enum SetMode { NormalSet, ImmediateSet };
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

    Node* set(VirtualRegister operand, Node* value, SetMode setMode = NormalSet)
    {
        return setDirect(m_inlineStackTop->remapOperand(operand), value, setMode);
    }
    
    Node* injectLazyOperandSpeculation(Node* node)
    {
        ASSERT(node->op() == GetLocal);
        ASSERT(node->codeOrigin.bytecodeIndex == m_currentIndex);
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
                    // Note: this is very special from an OSR exit standpoint. We wouldn't be
                    // able to do this for most locals, but it works here because we're dealing
                    // with a flushed local. For most locals we would need to issue a GetLocal
                    // here and ensure that we have uses in DFG IR wherever there would have
                    // been uses in bytecode. Clearly this optimization does not do this. But
                    // that's fine, because we don't need to track liveness for captured
                    // locals, and this optimization only kicks in for captured locals.
                    return inferredConstant(value);
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
        
        if (setMode == NormalSet) {
            ArgumentPosition* argumentPosition = findArgumentPositionForLocal(operand);
            if (isCaptured || argumentPosition)
                flushDirect(operand, argumentPosition);
        }

        VariableAccessData* variableAccessData = newVariableAccessData(operand, isCaptured);
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
            || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCacheWatchpoint));
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
            if (setMode == NormalSet)
                flushDirect(operand);
        } else if (m_codeBlock->specializationKind() == CodeForConstruct)
            variableAccessData->mergeShouldNeverUnbox(true);
        
        variableAccessData->mergeStructureCheckHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
            || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCacheWatchpoint));
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

    void addConstant(JSValue value)
    {
        unsigned constantIndex = m_codeBlock->addConstantLazily();
        initializeLazyWriteBarrierForConstant(
            m_graph.m_plan.writeBarriers,
            m_codeBlock->constants()[constantIndex],
            m_codeBlock,
            constantIndex,
            m_codeBlock->ownerExecutable(), 
            value);
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

    void flushAllArgumentsAndCapturedVariablesInInlineStack()
    {
        for (InlineStackEntry* inlineStackEntry = m_inlineStackTop; inlineStackEntry; inlineStackEntry = inlineStackEntry->m_caller)
            flush(inlineStackEntry);
    }

    void flushArgumentsAndCapturedVariables()
    {
        flush(m_inlineStackTop);
    }

    // NOTE: Only use this to construct constants that arise from non-speculative
    // constant folding. I.e. creating constants using this if we had constant
    // field inference would be a bad idea, since the bytecode parser's folding
    // doesn't handle liveness preservation.
    Node* getJSConstantForValue(JSValue constantValue, NodeFlags flags = NodeIsStaticConstant)
    {
        unsigned constantIndex;
        if (!m_codeBlock->findConstant(constantValue, constantIndex)) {
            addConstant(constantValue);
            m_constants.append(ConstantRecord());
        }
        
        ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        
        return getJSConstant(constantIndex, flags);
    }

    Node* getJSConstant(unsigned constant, NodeFlags flags = NodeIsStaticConstant)
    {
        Node* node = m_constants[constant].asJSValue;
        if (node)
            return node;

        Node* result = addToGraph(JSConstant, OpInfo(constant));
        result->mergeFlags(flags);
        m_constants[constant].asJSValue = result;
        return result;
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

    // Convenience methods for checking nodes for constants.
    bool isJSConstant(Node* node)
    {
        return node->op() == JSConstant;
    }
    bool isInt32Constant(Node* node)
    {
        return isJSConstant(node) && valueOfJSConstant(node).isInt32();
    }
    // Convenience methods for getting constant values.
    JSValue valueOfJSConstant(Node* node)
    {
        ASSERT(isJSConstant(node));
        return m_codeBlock->getConstant(FirstConstantRegisterIndex + node->constantNumber());
    }
    int32_t valueOfInt32Constant(Node* node)
    {
        ASSERT(isInt32Constant(node));
        return valueOfJSConstant(node).asInt32();
    }
    
    // This method returns a JSConstant with the value 'undefined'.
    Node* constantUndefined()
    {
        // Has m_constantUndefined been set up yet?
        if (m_constantUndefined == UINT_MAX) {
            // Search the constant pool for undefined, if we find it, we can just reuse this!
            unsigned numberOfConstants = m_codeBlock->numberOfConstantRegisters();
            for (m_constantUndefined = 0; m_constantUndefined < numberOfConstants; ++m_constantUndefined) {
                JSValue testMe = m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantUndefined);
                if (testMe.isUndefined())
                    return getJSConstant(m_constantUndefined);
            }

            // Add undefined to the CodeBlock's constants, and add a corresponding slot in m_constants.
            ASSERT(m_constants.size() == numberOfConstants);
            addConstant(jsUndefined());
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constantUndefined must refer to an entry in the CodeBlock's constant pool that has the value 'undefined'.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantUndefined).isUndefined());
        return getJSConstant(m_constantUndefined);
    }

    // This method returns a JSConstant with the value 'null'.
    Node* constantNull()
    {
        // Has m_constantNull been set up yet?
        if (m_constantNull == UINT_MAX) {
            // Search the constant pool for null, if we find it, we can just reuse this!
            unsigned numberOfConstants = m_codeBlock->numberOfConstantRegisters();
            for (m_constantNull = 0; m_constantNull < numberOfConstants; ++m_constantNull) {
                JSValue testMe = m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNull);
                if (testMe.isNull())
                    return getJSConstant(m_constantNull);
            }

            // Add null to the CodeBlock's constants, and add a corresponding slot in m_constants.
            ASSERT(m_constants.size() == numberOfConstants);
            addConstant(jsNull());
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constantNull must refer to an entry in the CodeBlock's constant pool that has the value 'null'.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNull).isNull());
        return getJSConstant(m_constantNull);
    }

    // This method returns a DoubleConstant with the value 1.
    Node* one()
    {
        // Has m_constant1 been set up yet?
        if (m_constant1 == UINT_MAX) {
            // Search the constant pool for the value 1, if we find it, we can just reuse this!
            unsigned numberOfConstants = m_codeBlock->numberOfConstantRegisters();
            for (m_constant1 = 0; m_constant1 < numberOfConstants; ++m_constant1) {
                JSValue testMe = m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constant1);
                if (testMe.isInt32() && testMe.asInt32() == 1)
                    return getJSConstant(m_constant1);
            }

            // Add the value 1 to the CodeBlock's constants, and add a corresponding slot in m_constants.
            ASSERT(m_constants.size() == numberOfConstants);
            addConstant(jsNumber(1));
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constant1 must refer to an entry in the CodeBlock's constant pool that has the integer value 1.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constant1).isInt32());
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constant1).asInt32() == 1);
        return getJSConstant(m_constant1);
    }
    
    // This method returns a DoubleConstant with the value NaN.
    Node* constantNaN()
    {
        JSValue nan = jsNaN();
        
        // Has m_constantNaN been set up yet?
        if (m_constantNaN == UINT_MAX) {
            // Search the constant pool for the value NaN, if we find it, we can just reuse this!
            unsigned numberOfConstants = m_codeBlock->numberOfConstantRegisters();
            for (m_constantNaN = 0; m_constantNaN < numberOfConstants; ++m_constantNaN) {
                JSValue testMe = m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNaN);
                if (JSValue::encode(testMe) == JSValue::encode(nan))
                    return getJSConstant(m_constantNaN);
            }

            // Add the value nan to the CodeBlock's constants, and add a corresponding slot in m_constants.
            ASSERT(m_constants.size() == numberOfConstants);
            addConstant(nan);
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constantNaN must refer to an entry in the CodeBlock's constant pool that has the value nan.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNaN).isDouble());
        ASSERT(std::isnan(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNaN).asDouble()));
        return getJSConstant(m_constantNaN);
    }
    
    Node* cellConstant(JSCell* cell)
    {
        HashMap<JSCell*, Node*>::AddResult result = m_cellConstantNodes.add(cell, nullptr);
        if (result.isNewEntry)
            result.iterator->value = addToGraph(WeakJSConstant, OpInfo(cell));
        
        return result.iterator->value;
    }
    
    Node* inferredConstant(JSValue value)
    {
        if (value.isCell())
            return cellConstant(value.asCell());
        return getJSConstantForValue(value, 0);
    }
    
    InlineCallFrame* inlineCallFrame()
    {
        return m_inlineStackTop->m_inlineCallFrame;
    }

    CodeOrigin currentCodeOrigin()
    {
        return CodeOrigin(m_currentIndex, inlineCallFrame());
    }
    
    bool canFold(Node* node)
    {
        if (Options::validateFTLOSRExitLiveness()) {
            // The static folding that the bytecode parser does results in the DFG
            // being able to do some DCE that the bytecode liveness analysis would
            // miss. Hence, we disable the static folding if we're validating FTL OSR
            // exit liveness. This may be brutish, but this validator is powerful
            // enough that it's worth it.
            return false;
        }
        
        return node->isStronglyProvedConstantIn(inlineCallFrame());
    }

    // Our codegen for constant strict equality performs a bitwise comparison,
    // so we can only select values that have a consistent bitwise identity.
    bool isConstantForCompareStrictEq(Node* node)
    {
        if (!node->isConstant())
            return false;
        JSValue value = valueOfJSConstant(node);
        return value.isBoolean() || value.isUndefinedOrNull();
    }
    
    Node* addToGraph(NodeType op, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentCodeOrigin(), Edge(child1), Edge(child2), Edge(child3));
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    Node* addToGraph(NodeType op, Edge child1, Edge child2 = Edge(), Edge child3 = Edge())
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentCodeOrigin(), child1, child2, child3);
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    Node* addToGraph(NodeType op, OpInfo info, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentCodeOrigin(), info, Edge(child1), Edge(child2), Edge(child3));
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    Node* addToGraph(NodeType op, OpInfo info1, OpInfo info2, Node* child1 = 0, Node* child2 = 0, Node* child3 = 0)
    {
        Node* result = m_graph.addNode(
            SpecNone, op, currentCodeOrigin(), info1, info2,
            Edge(child1), Edge(child2), Edge(child3));
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        return result;
    }
    
    Node* addToGraph(Node::VarArgTag, NodeType op, OpInfo info1, OpInfo info2)
    {
        Node* result = m_graph.addNode(
            SpecNone, Node::VarArg, op, currentCodeOrigin(), info1, info2,
            m_graph.m_varArgChildren.size() - m_numPassedVarArgs, m_numPassedVarArgs);
        ASSERT(op != Phi);
        m_currentBlock->append(result);
        
        m_numPassedVarArgs = 0;
        
        return result;
    }

    void addVarArgChild(Node* child)
    {
        m_graph.m_varArgChildren.append(Edge(child));
        m_numPassedVarArgs++;
    }
    
    Node* addCall(int result, NodeType op, int callee, int argCount, int registerOffset)
    {
        SpeculatedType prediction = getPrediction();
        
        addVarArgChild(get(VirtualRegister(callee)));
        size_t parameterSlots = JSStack::CallFrameHeaderSize - JSStack::CallerFrameAndPCSize + argCount;
        if (parameterSlots > m_parameterSlots)
            m_parameterSlots = parameterSlots;

        int dummyThisArgument = op == Call ? 0 : 1;
        for (int i = 0 + dummyThisArgument; i < argCount; ++i)
            addVarArgChild(get(virtualRegisterForArgument(i, registerOffset)));

        Node* call = addToGraph(Node::VarArg, op, OpInfo(0), OpInfo(prediction));
        set(VirtualRegister(result), call);
        return call;
    }
    
    Node* cellConstantWithStructureCheck(JSCell* object, Structure* structure)
    {
        Node* objectNode = cellConstant(object);
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(structure)), objectNode);
        return objectNode;
    }
    
    Node* cellConstantWithStructureCheck(JSCell* object)
    {
        return cellConstantWithStructureCheck(object, object->structure());
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
        bool likelyToTakeSlowCase;
        if (!isX86() && node->op() == ArithMod)
            likelyToTakeSlowCase = false;
        else
            likelyToTakeSlowCase = m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex);
        
        if (!likelyToTakeSlowCase
            && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow)
            && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
            return node;
        
        switch (node->op()) {
        case UInt32ToNumber:
        case ArithAdd:
        case ArithSub:
        case ValueAdd:
        case ArithMod: // for ArithMod "MayOverflow" means we tried to divide by zero, or we saw double.
            node->mergeFlags(NodeMayOverflow);
            break;
            
        case ArithNegate:
            // Currently we can't tell the difference between a negation overflowing
            // (i.e. -(1 << 31)) or generating negative zero (i.e. -0). If it took slow
            // path then we assume that it did both of those things.
            node->mergeFlags(NodeMayOverflow);
            node->mergeFlags(NodeMayNegZero);
            break;

        case ArithMul:
            if (m_inlineStackTop->m_profiledBlock->likelyToTakeDeepestSlowCase(m_currentIndex)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
                node->mergeFlags(NodeMayOverflow | NodeMayNegZero);
            else if (m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
                node->mergeFlags(NodeMayNegZero);
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
        
        // The main slow case counter for op_div in the old JIT counts only when
        // the operands are not numbers. We don't care about that since we already
        // have speculations in place that take care of that separately. We only
        // care about when the outcome of the division is not an integer, which
        // is what the special fast case counter tells us.
        
        if (!m_inlineStackTop->m_profiledBlock->couldTakeSpecialFastCase(m_currentIndex)
            && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow)
            && !m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero))
            return node;
        
        // FIXME: It might be possible to make this more granular. The DFG certainly can
        // distinguish between negative zero and overflow in its exit profiles.
        node->mergeFlags(NodeMayOverflow | NodeMayNegZero);
        
        return node;
    }
    
    bool structureChainIsStillValid(bool direct, Structure* previousStructure, StructureChain* chain)
    {
        if (direct)
            return true;
        
        if (!previousStructure->storedPrototype().isNull() && previousStructure->storedPrototype().asCell()->structure() != chain->head()->get())
            return false;
        
        for (WriteBarrier<Structure>* it = chain->head(); *it; ++it) {
            if (!(*it)->storedPrototype().isNull() && (*it)->storedPrototype().asCell()->structure() != it[1].get())
                return false;
        }
        
        return true;
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

    // We use these values during code generation, and to avoid the need for
    // special handling we make sure they are available as constants in the
    // CodeBlock's constant pool. These variables are initialized to
    // UINT_MAX, and lazily updated to hold an index into the CodeBlock's
    // constant pool, as necessary.
    unsigned m_constantUndefined;
    unsigned m_constantNull;
    unsigned m_constantNaN;
    unsigned m_constant1;
    HashMap<JSCell*, unsigned> m_cellConstants;
    HashMap<JSCell*, Node*> m_cellConstantNodes;

    // A constant in the constant pool may be represented by more than one
    // node in the graph, depending on the context in which it is being used.
    struct ConstantRecord {
        ConstantRecord()
            : asInt32(0)
            , asNumeric(0)
            , asJSValue(0)
        {
        }

        Node* asInt32;
        Node* asNumeric;
        Node* asJSValue;
    };

    // Track the index of the node whose result is the current value for every
    // register value in the bytecode - argument, local, and temporary.
    Vector<ConstantRecord, 16> m_constants;

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
        Vector<unsigned> m_constantRemap;
        Vector<unsigned> m_constantBufferRemap;
        Vector<unsigned> m_switchRemap;
        
        // Blocks introduced by this code block, which need successor linking.
        // May include up to one basic block that includes the continuation after
        // the callsite in the caller. These must be appended in the order that they
        // are created, but their bytecodeBegin values need not be in order as they
        // are ignored.
        Vector<UnlinkedBlock> m_unlinkedBlocks;
        
        // Potential block linking targets. Must be sorted by bytecodeBegin, and
        // cannot have two blocks that have the same bytecodeBegin. For this very
        // reason, this is not equivalent to 
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
            CodeSpecializationKind);
        
        ~InlineStackEntry()
        {
            m_byteCodeParser->m_inlineStackTop = m_caller;
        }
        
        VirtualRegister remapOperand(VirtualRegister operand) const
        {
            if (!m_inlineCallFrame)
                return operand;
            
            if (operand.isConstant()) {
                VirtualRegister result = VirtualRegister(m_constantRemap[operand.toConstantIndex()]);
                ASSERT(result.isConstant());
                return result;
            }

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
    // Mapping between values and constant numbers.
    JSValueMap m_jsValueMap;
    // Index of the empty value, or UINT_MAX if there is no mapping. This is a horrible
    // work-around for the fact that JSValueMap can't handle "empty" values.
    unsigned m_emptyJSValueIndex;
    
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
    ASSERT(registerOffset <= 0);
    
    Node* callTarget = get(VirtualRegister(callee));
    
    CallLinkStatus callLinkStatus;

    if (m_graph.isConstant(callTarget)) {
        callLinkStatus = CallLinkStatus(
            m_graph.valueOfJSConstant(callTarget)).setIsProved(true);
    } else {
        callLinkStatus = CallLinkStatus::computeFor(
            m_inlineStackTop->m_profiledBlock, currentCodeOrigin(), m_callContextMap);
    }
    
    if (!callLinkStatus.canOptimize()) {
        // Oddly, this conflates calls that haven't executed with calls that behaved sufficiently polymorphically
        // that we cannot optimize them.
        
        addCall(result, op, callee, argumentCountIncludingThis, registerOffset);
        return;
    }
    
    unsigned nextOffset = m_currentIndex + instructionSize;
    SpeculatedType prediction = getPrediction();

    if (InternalFunction* function = callLinkStatus.internalFunction()) {
        if (handleConstantInternalFunction(result, function, registerOffset, argumentCountIncludingThis, prediction, kind)) {
            // This phantoming has to be *after* the code for the intrinsic, to signify that
            // the inputs must be kept alive whatever exits the intrinsic may do.
            addToGraph(Phantom, callTarget);
            emitArgumentPhantoms(registerOffset, argumentCountIncludingThis, kind);
            return;
        }
        
        // Can only handle this using the generic call handler.
        addCall(result, op, callee, argumentCountIncludingThis, registerOffset);
        return;
    }
        
    Intrinsic intrinsic = callLinkStatus.intrinsicFor(kind);
    if (intrinsic != NoIntrinsic) {
        emitFunctionChecks(callLinkStatus, callTarget, registerOffset, kind);
            
        if (handleIntrinsic(result, intrinsic, registerOffset, argumentCountIncludingThis, prediction)) {
            // This phantoming has to be *after* the code for the intrinsic, to signify that
            // the inputs must be kept alive whatever exits the intrinsic may do.
            addToGraph(Phantom, callTarget);
            emitArgumentPhantoms(registerOffset, argumentCountIncludingThis, kind);
            if (m_graph.compilation())
                m_graph.compilation()->noticeInlinedCall();
            return;
        }
    } else if (handleInlining(callTarget, result, callLinkStatus, registerOffset, argumentCountIncludingThis, nextOffset, kind)) {
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedCall();
        return;
    }
    
    addCall(result, op, callee, argumentCountIncludingThis, registerOffset);
}

void ByteCodeParser::emitFunctionChecks(const CallLinkStatus& callLinkStatus, Node* callTarget, int registerOffset, CodeSpecializationKind kind)
{
    Node* thisArgument;
    if (kind == CodeForCall)
        thisArgument = get(virtualRegisterForArgument(0, registerOffset));
    else
        thisArgument = 0;

    if (callLinkStatus.isProved()) {
        addToGraph(Phantom, callTarget, thisArgument);
        return;
    }
    
    ASSERT(callLinkStatus.canOptimize());
    
    if (JSFunction* function = callLinkStatus.function())
        addToGraph(CheckFunction, OpInfo(function), callTarget, thisArgument);
    else {
        ASSERT(callLinkStatus.structure());
        ASSERT(callLinkStatus.executable());
        
        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(callLinkStatus.structure())), callTarget);
        addToGraph(CheckExecutable, OpInfo(callLinkStatus.executable()), callTarget, thisArgument);
    }
}

void ByteCodeParser::emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind kind)
{
    for (int i = kind == CodeForCall ? 0 : 1; i < argumentCountIncludingThis; ++i)
        addToGraph(Phantom, get(virtualRegisterForArgument(i, registerOffset)));
}

bool ByteCodeParser::handleInlining(Node* callTargetNode, int resultOperand, const CallLinkStatus& callLinkStatus, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, CodeSpecializationKind kind)
{
    static const bool verbose = false;
    
    if (verbose)
        dataLog("Considering inlining ", callLinkStatus, " into ", currentCodeOrigin(), "\n");
    
    // First, the really simple checks: do we have an actual JS function?
    if (!callLinkStatus.executable()) {
        if (verbose)
            dataLog("    Failing because there is no executable.\n");
        return false;
    }
    if (callLinkStatus.executable()->isHostFunction()) {
        if (verbose)
            dataLog("    Failing because it's a host function.\n");
        return false;
    }
    
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(callLinkStatus.executable());
    
    // Does the number of arguments we're passing match the arity of the target? We currently
    // inline only if the number of arguments passed is greater than or equal to the number
    // arguments expected.
    if (static_cast<int>(executable->parameterCount()) + 1 > argumentCountIncludingThis) {
        if (verbose)
            dataLog("    Failing because of arity mismatch.\n");
        return false;
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
        return false;
    }
    CapabilityLevel capabilityLevel = inlineFunctionForCapabilityLevel(
        codeBlock, kind, callLinkStatus.isClosureCall());
    if (!canInline(capabilityLevel)) {
        if (verbose)
            dataLog("    Failing because the function is not inlineable.\n");
        return false;
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
            return false;
        }
        
        if (entry->executable() == executable) {
            ++recursion;
            if (recursion >= Options::maximumInliningRecursion()) {
                if (verbose)
                    dataLog("    Failing because recursion detected.\n");
                return false;
            }
        }
    }
    
    if (verbose)
        dataLog("    Committing to inlining.\n");
    
    // Now we know without a doubt that we are committed to inlining. So begin the process
    // by checking the callee (if necessary) and making sure that arguments and the callee
    // are flushed.
    emitFunctionChecks(callLinkStatus, callTargetNode, registerOffset, kind);
    
    // FIXME: Don't flush constants!
    
    int inlineCallFrameStart = m_inlineStackTop->remapOperand(VirtualRegister(registerOffset)).offset() + JSStack::CallFrameHeaderSize;
    
    // Make sure that we have enough locals.
    unsigned newNumLocals = VirtualRegister(inlineCallFrameStart).toLocal() + 1 + JSStack::CallFrameHeaderSize + codeBlock->m_numCalleeRegisters;
    if (newNumLocals > m_numLocals) {
        m_numLocals = newNumLocals;
        for (size_t i = 0; i < m_graph.numBlocks(); ++i)
            m_graph.block(i)->ensureLocals(newNumLocals);
    }
    
    size_t argumentPositionStart = m_graph.m_argumentPositions.size();

    InlineStackEntry inlineStackEntry(
        this, codeBlock, codeBlock, m_graph.lastBlock(), callLinkStatus.function(),
        m_inlineStackTop->remapOperand(VirtualRegister(resultOperand)),
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
        == callLinkStatus.isClosureCall());
    if (callLinkStatus.isClosureCall()) {
        VariableAccessData* calleeVariable =
            set(VirtualRegister(JSStack::Callee), callTargetNode, ImmediateSet)->variableAccessData();
        VariableAccessData* scopeVariable =
            set(VirtualRegister(JSStack::ScopeChain), addToGraph(GetScope, callTargetNode), ImmediateSet)->variableAccessData();
        
        calleeVariable->mergeShouldNeverUnbox(true);
        scopeVariable->mergeShouldNeverUnbox(true);
        
        inlineVariableData.calleeVariable = calleeVariable;
    }
    
    m_graph.m_inlineVariableData.append(inlineVariableData);
    
    parseCodeBlock();
    
    m_currentIndex = oldIndex;
    
    // If the inlined code created some new basic blocks, then we have linking to do.
    if (inlineStackEntry.m_callsiteBlockHead != m_graph.lastBlock()) {
        
        ASSERT(!inlineStackEntry.m_unlinkedBlocks.isEmpty());
        if (inlineStackEntry.m_callsiteBlockHeadNeedsLinking)
            linkBlock(inlineStackEntry.m_callsiteBlockHead, inlineStackEntry.m_blockLinkingTargets);
        else
            ASSERT(inlineStackEntry.m_callsiteBlockHead->isLinked);
        
        // It's possible that the callsite block head is not owned by the caller.
        if (!inlineStackEntry.m_caller->m_unlinkedBlocks.isEmpty()) {
            // It's definitely owned by the caller, because the caller created new blocks.
            // Assert that this all adds up.
            ASSERT(inlineStackEntry.m_caller->m_unlinkedBlocks.last().m_block == inlineStackEntry.m_callsiteBlockHead);
            ASSERT(inlineStackEntry.m_caller->m_unlinkedBlocks.last().m_needsNormalLinking);
            inlineStackEntry.m_caller->m_unlinkedBlocks.last().m_needsNormalLinking = false;
        } else {
            // It's definitely not owned by the caller. Tell the caller that he does not
            // need to link his callsite block head, because we did it for him.
            ASSERT(inlineStackEntry.m_caller->m_callsiteBlockHeadNeedsLinking);
            ASSERT(inlineStackEntry.m_caller->m_callsiteBlockHead == inlineStackEntry.m_callsiteBlockHead);
            inlineStackEntry.m_caller->m_callsiteBlockHeadNeedsLinking = false;
        }
        
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
            m_inlineStackTop->m_caller->m_unlinkedBlocks.append(UnlinkedBlock(m_graph.lastBlock()));
        }
        
        m_currentBlock = m_graph.lastBlock();
        return true;
    }
    
    // If we get to this point then all blocks must end in some sort of terminals.
    ASSERT(lastBlock->last()->isTerminal());
    

    // Need to create a new basic block for the continuation at the caller.
    RefPtr<BasicBlock> block = adoptRef(new BasicBlock(nextOffset, m_numArguments, m_numLocals));

    // Link the early returns to the basic block we're about to create.
    for (size_t i = 0; i < inlineStackEntry.m_unlinkedBlocks.size(); ++i) {
        if (!inlineStackEntry.m_unlinkedBlocks[i].m_needsEarlyReturnLinking)
            continue;
        BasicBlock* blockToLink = inlineStackEntry.m_unlinkedBlocks[i].m_block;
        ASSERT(!blockToLink->isLinked);
        Node* node = blockToLink->last();
        ASSERT(node->op() == Jump);
        ASSERT(node->takenBlock() == 0);
        node->setTakenBlock(block.get());
        inlineStackEntry.m_unlinkedBlocks[i].m_needsEarlyReturnLinking = false;
#if !ASSERT_DISABLED
        blockToLink->isLinked = true;
#endif
    }
    
    m_currentBlock = block.get();
    ASSERT(m_inlineStackTop->m_caller->m_blockLinkingTargets.isEmpty() || m_inlineStackTop->m_caller->m_blockLinkingTargets.last()->bytecodeBegin < nextOffset);
    m_inlineStackTop->m_caller->m_unlinkedBlocks.append(UnlinkedBlock(block.get()));
    m_inlineStackTop->m_caller->m_blockLinkingTargets.append(block.get());
    m_graph.appendBlock(block);
    prepareToParseBlock();
    
    // At this point we return and continue to generate code for the caller, but
    // in the new basic block.
    return true;
}

bool ByteCodeParser::handleMinMax(int resultOperand, NodeType op, int registerOffset, int argumentCountIncludingThis)
{
    if (argumentCountIncludingThis == 1) { // Math.min()
        set(VirtualRegister(resultOperand), constantNaN());
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
            set(VirtualRegister(resultOperand), constantNaN());
            return true;
        }

        if (!MacroAssembler::supportsFloatingPointAbs())
            return false;

        Node* node = addToGraph(ArithAbs, get(virtualRegisterForArgument(1, registerOffset)));
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflow);
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
            set(VirtualRegister(resultOperand), constantNaN());
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
        
        ArrayMode arrayMode = getArrayMode(m_currentInstruction[6].u.arrayProfile);
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
        
        ArrayMode arrayMode = getArrayMode(m_currentInstruction[6].u.arrayProfile);
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
    int argumentCountIncludingThis, SpeculatedType prediction, CodeSpecializationKind kind)
{
    // If we ever find that we have a lot of internal functions that we specialize for,
    // then we should probably have some sort of hashtable dispatch, or maybe even
    // dispatch straight through the MethodTable of the InternalFunction. But for now,
    // it seems that this case is hit infrequently enough, and the number of functions
    // we know about is small enough, that having just a linear cascade of if statements
    // is good enough.
    
    UNUSED_PARAM(prediction); // Remove this once we do more things.
    
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
            result = cellConstant(m_vm->smallStrings.emptyString());
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

Node* ByteCodeParser::handleGetByOffset(SpeculatedType prediction, Node* base, unsigned identifierNumber, PropertyOffset offset)
{
    Node* propertyStorage;
    if (isInlineOffset(offset))
        propertyStorage = base;
    else
        propertyStorage = addToGraph(GetButterfly, base);
    Node* getByOffset = addToGraph(GetByOffset, OpInfo(m_graph.m_storageAccessData.size()), OpInfo(prediction), propertyStorage, base);

    StorageAccessData storageAccessData;
    storageAccessData.offset = offset;
    storageAccessData.identifierNumber = identifierNumber;
    m_graph.m_storageAccessData.append(storageAccessData);

    return getByOffset;
}

void ByteCodeParser::handleGetByOffset(
    int destinationOperand, SpeculatedType prediction, Node* base, unsigned identifierNumber,
    PropertyOffset offset)
{
    set(VirtualRegister(destinationOperand), handleGetByOffset(prediction, base, identifierNumber, offset));
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

void ByteCodeParser::handleGetById(
    int destinationOperand, SpeculatedType prediction, Node* base, unsigned identifierNumber,
    const GetByIdStatus& getByIdStatus)
{
    if (!getByIdStatus.isSimple()) {
        set(VirtualRegister(destinationOperand),
            addToGraph(
                getByIdStatus.makesCalls() ? GetByIdFlush : GetById,
                OpInfo(identifierNumber), OpInfo(prediction), base));
        return;
    }
    
    ASSERT(getByIdStatus.structureSet().size());
                
    if (m_graph.compilation())
        m_graph.compilation()->noticeInlinedGetById();
    
    Node* originalBaseForBaselineJIT = base;
                
    addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(getByIdStatus.structureSet())), base);
    
    if (getByIdStatus.chain()) {
        m_graph.chains().addLazily(getByIdStatus.chain());
        Structure* currentStructure = getByIdStatus.structureSet().singletonStructure();
        JSObject* currentObject = 0;
        for (unsigned i = 0; i < getByIdStatus.chain()->size(); ++i) {
            currentObject = asObject(currentStructure->prototypeForLookup(m_inlineStackTop->m_codeBlock));
            currentStructure = getByIdStatus.chain()->at(i);
            base = cellConstantWithStructureCheck(currentObject, currentStructure);
        }
    }
    
    // Unless we want bugs like https://bugs.webkit.org/show_bug.cgi?id=88783, we need to
    // ensure that the base of the original get_by_id is kept alive until we're done with
    // all of the speculations. We only insert the Phantom if there had been a CheckStructure
    // on something other than the base following the CheckStructure on base, or if the
    // access was compiled to a WeakJSConstant specific value, in which case we might not
    // have any explicit use of the base at all.
    if (getByIdStatus.specificValue() || originalBaseForBaselineJIT != base)
        addToGraph(Phantom, originalBaseForBaselineJIT);
    
    if (getByIdStatus.specificValue()) {
        ASSERT(getByIdStatus.specificValue().isCell());
        
        set(VirtualRegister(destinationOperand), cellConstant(getByIdStatus.specificValue().asCell()));
        return;
    }
    
    handleGetByOffset(
        destinationOperand, prediction, base, identifierNumber, getByIdStatus.offset());
}

void ByteCodeParser::prepareToParseBlock()
{
    for (unsigned i = 0; i < m_constants.size(); ++i)
        m_constants[i] = ConstantRecord();
    m_cellConstantNodes.clear();
}

Node* ByteCodeParser::getScope(bool skipTop, unsigned skipCount)
{
    Node* localBase = get(VirtualRegister(JSStack::ScopeChain));
    if (skipTop) {
        ASSERT(!inlineCallFrame());
        localBase = addToGraph(SkipTopScope, localBase);
    }
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
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCacheWatchpoint));
            variable->mergeCheckArrayHoistingFailed(
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType));
            
            Node* setArgument = addToGraph(SetArgument, OpInfo(variable));
            m_graph.m_arguments[argument] = setArgument;
            m_currentBlock->variablesAtTail.setArgumentFirstTime(argument, setArgument);
        }
    }

    while (true) {
        for (unsigned i = 0; i < m_setLocalQueue.size(); ++i)
            m_setLocalQueue[i].execute(this);
        m_setLocalQueue.resize(0);
        
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
        
        if (m_graph.compilation()) {
            addToGraph(CountExecution, OpInfo(m_graph.compilation()->executionCounterFor(
                Profiler::OriginStack(*m_vm->m_perBytecodeProfiler, m_codeBlock, currentCodeOrigin()))));
        }
        
        switch (opcodeID) {

        // === Function entry opcodes ===

        case op_enter:
            // Initialize all locals to undefined.
            for (int i = 0; i < m_inlineStackTop->m_codeBlock->m_numVars; ++i)
                set(virtualRegisterForLocal(i), constantUndefined(), ImmediateSet);
            if (m_inlineStackTop->m_codeBlock->specializationKind() == CodeForConstruct)
                set(virtualRegisterForArgument(0), constantUndefined(), ImmediateSet);
            NEXT_OPCODE(op_enter);
            
        case op_touch_entry:
            if (m_inlineStackTop->m_codeBlock->symbolTable()->m_functionEnteredOnce.isStillValid())
                addToGraph(ForceOSRExit);
            NEXT_OPCODE(op_touch_entry);
            
        case op_to_this: {
            Node* op1 = getThis();
            if (op1->op() != ToThis) {
                Structure* cachedStructure = currentInstruction[2].u.structure.get();
                if (!cachedStructure
                    || cachedStructure->classInfo()->methodTable.toThis != JSObject::info()->methodTable.toThis
                    || m_inlineStackTop->m_profiledBlock->couldTakeSlowCase(m_currentIndex)
                    || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
                    || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCacheWatchpoint)
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
            if (callee->op() == WeakJSConstant) {
                JSCell* cell = callee->weakConstant();
                ASSERT(cell->inherits(JSFunction::info()));
                
                JSFunction* function = jsCast<JSFunction*>(cell);
                if (Structure* structure = function->allocationStructure()) {
                    addToGraph(AllocationProfileWatchpoint, OpInfo(function));
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
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadFunction)) {
                set(VirtualRegister(currentInstruction[1].u.operand), get(VirtualRegister(JSStack::Callee)));
            } else {
                ASSERT(cachedFunction->inherits(JSFunction::info()));
                Node* actualCallee = get(VirtualRegister(JSStack::Callee));
                addToGraph(CheckFunction, OpInfo(cachedFunction), actualCallee);
                set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(WeakJSConstant, OpInfo(cachedFunction)));
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
            set(srcDstVirtualRegister, makeSafe(addToGraph(ArithAdd, op, one())));
            NEXT_OPCODE(op_inc);
        }

        case op_dec: {
            int srcDst = currentInstruction[1].u.operand;
            VirtualRegister srcDstVirtualRegister = VirtualRegister(srcDst);
            Node* op = get(srcDstVirtualRegister);
            set(srcDstVirtualRegister, makeSafe(addToGraph(ArithSub, op, one())));
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
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(VirtualRegister(currentInstruction[1].u.operand),
                        getJSConstantForValue(jsBoolean(a.asNumber() < b.asNumber())));
                    NEXT_OPCODE(op_less);
                }
            }
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareLess, op1, op2));
            NEXT_OPCODE(op_less);
        }

        case op_lesseq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(VirtualRegister(currentInstruction[1].u.operand),
                        getJSConstantForValue(jsBoolean(a.asNumber() <= b.asNumber())));
                    NEXT_OPCODE(op_lesseq);
                }
            }
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareLessEq, op1, op2));
            NEXT_OPCODE(op_lesseq);
        }

        case op_greater: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(VirtualRegister(currentInstruction[1].u.operand),
                        getJSConstantForValue(jsBoolean(a.asNumber() > b.asNumber())));
                    NEXT_OPCODE(op_greater);
                }
            }
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareGreater, op1, op2));
            NEXT_OPCODE(op_greater);
        }

        case op_greatereq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(VirtualRegister(currentInstruction[1].u.operand),
                        getJSConstantForValue(jsBoolean(a.asNumber() >= b.asNumber())));
                    NEXT_OPCODE(op_greatereq);
                }
            }
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareGreaterEq, op1, op2));
            NEXT_OPCODE(op_greatereq);
        }

        case op_eq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(VirtualRegister(currentInstruction[1].u.operand),
                    getJSConstantForValue(jsBoolean(JSValue::equal(m_codeBlock->globalObject()->globalExec(), a, b))));
                NEXT_OPCODE(op_eq);
            }
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareEq, op1, op2));
            NEXT_OPCODE(op_eq);
        }

        case op_eq_null: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareEqConstant, value, constantNull()));
            NEXT_OPCODE(op_eq_null);
        }

        case op_stricteq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(VirtualRegister(currentInstruction[1].u.operand),
                    getJSConstantForValue(jsBoolean(JSValue::strictEqual(m_codeBlock->globalObject()->globalExec(), a, b))));
                NEXT_OPCODE(op_stricteq);
            }
            if (isConstantForCompareStrictEq(op1))
                set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareStrictEqConstant, op2, op1));
            else if (isConstantForCompareStrictEq(op2))
                set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareStrictEqConstant, op1, op2));
            else
                set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CompareStrictEq, op1, op2));
            NEXT_OPCODE(op_stricteq);
        }

        case op_neq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(VirtualRegister(currentInstruction[1].u.operand),
                    getJSConstantForValue(jsBoolean(!JSValue::equal(m_codeBlock->globalObject()->globalExec(), a, b))));
                NEXT_OPCODE(op_neq);
            }
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(LogicalNot, addToGraph(CompareEq, op1, op2)));
            NEXT_OPCODE(op_neq);
        }

        case op_neq_null: {
            Node* value = get(VirtualRegister(currentInstruction[2].u.operand));
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(LogicalNot, addToGraph(CompareEqConstant, value, constantNull())));
            NEXT_OPCODE(op_neq_null);
        }

        case op_nstricteq: {
            Node* op1 = get(VirtualRegister(currentInstruction[2].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[3].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(VirtualRegister(currentInstruction[1].u.operand),
                    getJSConstantForValue(jsBoolean(!JSValue::strictEqual(m_codeBlock->globalObject()->globalExec(), a, b))));
                NEXT_OPCODE(op_nstricteq);
            }
            Node* invertedResult;
            if (isConstantForCompareStrictEq(op1))
                invertedResult = addToGraph(CompareStrictEqConstant, op2, op1);
            else if (isConstantForCompareStrictEq(op2))
                invertedResult = addToGraph(CompareStrictEqConstant, op1, op2);
            else
                invertedResult = addToGraph(CompareStrictEq, op1, op2);
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(LogicalNot, invertedResult));
            NEXT_OPCODE(op_nstricteq);
        }

        // === Property access operations ===

        case op_get_by_val: {
            SpeculatedType prediction = getPrediction();
            
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
            bool canCountAsInlined = true;
            if (!putByIdStatus.isSet()) {
                addToGraph(ForceOSRExit);
                canCountAsInlined = false;
            }
            
            if (putByIdStatus.isSimpleReplace()) {
                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(putByIdStatus.oldStructure())), base);
                handlePutByOffset(base, identifierNumber, putByIdStatus.offset(), value);
            } else if (
                putByIdStatus.isSimpleTransition()
                && (!putByIdStatus.structureChain()
                    || putByIdStatus.structureChain()->isStillValid())) {
                
                m_graph.chains().addLazily(putByIdStatus.structureChain());
                
                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(putByIdStatus.oldStructure())), base);
                if (!direct) {
                    if (!putByIdStatus.oldStructure()->storedPrototype().isNull()) {
                        cellConstantWithStructureCheck(
                            putByIdStatus.oldStructure()->storedPrototype().asCell());
                    }
                    
                    for (unsigned i = 0; i < putByIdStatus.structureChain()->size(); ++i) {
                        JSValue prototype = putByIdStatus.structureChain()->at(i)->storedPrototype();
                        if (prototype.isNull())
                            continue;
                        cellConstantWithStructureCheck(prototype.asCell());
                    }
                }
                ASSERT(putByIdStatus.oldStructure()->transitionWatchpointSetHasBeenInvalidated());
                
                Node* propertyStorage;
                StructureTransitionData* transitionData =
                    m_graph.addStructureTransitionData(
                        StructureTransitionData(
                            putByIdStatus.oldStructure(),
                            putByIdStatus.newStructure()));

                if (putByIdStatus.oldStructure()->outOfLineCapacity()
                    != putByIdStatus.newStructure()->outOfLineCapacity()) {
                    
                    // If we're growing the property storage then it must be because we're
                    // storing into the out-of-line storage.
                    ASSERT(!isInlineOffset(putByIdStatus.offset()));
                    
                    if (!putByIdStatus.oldStructure()->outOfLineCapacity()) {
                        propertyStorage = addToGraph(
                            AllocatePropertyStorage, OpInfo(transitionData), base);
                    } else {
                        propertyStorage = addToGraph(
                            ReallocatePropertyStorage, OpInfo(transitionData),
                            base, addToGraph(GetButterfly, base));
                    }
                } else {
                    if (isInlineOffset(putByIdStatus.offset()))
                        propertyStorage = base;
                    else
                        propertyStorage = addToGraph(GetButterfly, base);
                }
                
                addToGraph(PutStructure, OpInfo(transitionData), base);
                
                addToGraph(
                    PutByOffset,
                    OpInfo(m_graph.m_storageAccessData.size()),
                    propertyStorage,
                    base,
                    value);
                
                StorageAccessData storageAccessData;
                storageAccessData.offset = putByIdStatus.offset();
                storageAccessData.identifierNumber = identifierNumber;
                m_graph.m_storageAccessData.append(storageAccessData);
            } else {
                if (direct)
                    addToGraph(PutByIdDirect, OpInfo(identifierNumber), base, value);
                else
                    addToGraph(PutById, OpInfo(identifierNumber), base, value);
                canCountAsInlined = false;
            }
            
            if (canCountAsInlined && m_graph.compilation())
                m_graph.compilation()->noticeInlinedPutById();

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
            unsigned relativeOffset = currentInstruction[1].u.operand;
            addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
            LAST_OPCODE(op_jmp);
        }

        case op_jtrue: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* condition = get(VirtualRegister(currentInstruction[1].u.operand));
            if (canFold(condition)) {
                TriState state = valueOfJSConstant(condition).pureToBoolean();
                if (state == TrueTriState) {
                    addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                    LAST_OPCODE(op_jtrue);
                } else if (state == FalseTriState) {
                    // Emit a placeholder for this bytecode operation but otherwise
                    // just fall through.
                    addToGraph(Phantom);
                    NEXT_OPCODE(op_jtrue);
                }
            }
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jtrue)), condition);
            LAST_OPCODE(op_jtrue);
        }

        case op_jfalse: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* condition = get(VirtualRegister(currentInstruction[1].u.operand));
            if (canFold(condition)) {
                TriState state = valueOfJSConstant(condition).pureToBoolean();
                if (state == FalseTriState) {
                    addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                    LAST_OPCODE(op_jfalse);
                } else if (state == TrueTriState) {
                    // Emit a placeholder for this bytecode operation but otherwise
                    // just fall through.
                    addToGraph(Phantom);
                    NEXT_OPCODE(op_jfalse);
                }
            }
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jfalse)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jfalse);
        }

        case op_jeq_null: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* value = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* condition = addToGraph(CompareEqConstant, value, constantNull());
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jeq_null)), condition);
            LAST_OPCODE(op_jeq_null);
        }

        case op_jneq_null: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* value = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* condition = addToGraph(CompareEqConstant, value, constantNull());
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jneq_null)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jneq_null);
        }

        case op_jless: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a < b) {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jless);
                    } else {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jless);
                    }
                }
            }
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jless)), condition);
            LAST_OPCODE(op_jless);
        }

        case op_jlesseq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a <= b) {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jlesseq);
                    } else {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jlesseq);
                    }
                }
            }
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jlesseq)), condition);
            LAST_OPCODE(op_jlesseq);
        }

        case op_jgreater: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a > b) {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jgreater);
                    } else {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jgreater);
                    }
                }
            }
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jgreater)), condition);
            LAST_OPCODE(op_jgreater);
        }

        case op_jgreatereq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a >= b) {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jgreatereq);
                    } else {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jgreatereq);
                    }
                }
            }
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jgreatereq)), condition);
            LAST_OPCODE(op_jgreatereq);
        }

        case op_jnless: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a < b) {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jnless);
                    } else {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jnless);
                    }
                }
            }
            Node* condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jnless)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jnless);
        }

        case op_jnlesseq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a <= b) {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jnlesseq);
                    } else {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jnlesseq);
                    }
                }
            }
            Node* condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jnlesseq)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jnlesseq);
        }

        case op_jngreater: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a > b) {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jngreater);
                    } else {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jngreater);
                    }
                }
            }
            Node* condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jngreater)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jngreater);
        }

        case op_jngreatereq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(VirtualRegister(currentInstruction[1].u.operand));
            Node* op2 = get(VirtualRegister(currentInstruction[2].u.operand));
            if (canFold(op1) && canFold(op2)) {
                JSValue aValue = valueOfJSConstant(op1);
                JSValue bValue = valueOfJSConstant(op2);
                if (aValue.isNumber() && bValue.isNumber()) {
                    double a = aValue.asNumber();
                    double b = bValue.asNumber();
                    if (a >= b) {
                        // Emit a placeholder for this bytecode operation but otherwise
                        // just fall through.
                        addToGraph(Phantom);
                        NEXT_OPCODE(op_jngreatereq);
                    } else {
                        addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
                        LAST_OPCODE(op_jngreatereq);
                    }
                }
            }
            Node* condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jngreatereq)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jngreatereq);
        }
            
        case op_switch_imm: {
            SwitchData data;
            data.kind = SwitchImm;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[currentInstruction[1].u.operand];
            data.setFallThroughBytecodeIndex(m_currentIndex + currentInstruction[2].u.operand);
            SimpleJumpTable& table = m_codeBlock->switchJumpTable(data.switchTableIndex);
            for (unsigned i = 0; i < table.branchOffsets.size(); ++i) {
                if (!table.branchOffsets[i])
                    continue;
                unsigned target = m_currentIndex + table.branchOffsets[i];
                if (target == data.fallThroughBytecodeIndex())
                    continue;
                data.cases.append(SwitchCase::withBytecodeIndex(jsNumber(static_cast<int32_t>(table.min + i)), target));
            }
            m_graph.m_switchData.append(data);
            addToGraph(Switch, OpInfo(&m_graph.m_switchData.last()), get(VirtualRegister(currentInstruction[3].u.operand)));
            LAST_OPCODE(op_switch_imm);
        }
            
        case op_switch_char: {
            SwitchData data;
            data.kind = SwitchChar;
            data.switchTableIndex = m_inlineStackTop->m_switchRemap[currentInstruction[1].u.operand];
            data.setFallThroughBytecodeIndex(m_currentIndex + currentInstruction[2].u.operand);
            SimpleJumpTable& table = m_codeBlock->switchJumpTable(data.switchTableIndex);
            for (unsigned i = 0; i < table.branchOffsets.size(); ++i) {
                if (!table.branchOffsets[i])
                    continue;
                unsigned target = m_currentIndex + table.branchOffsets[i];
                if (target == data.fallThroughBytecodeIndex())
                    continue;
                data.cases.append(
                    SwitchCase::withBytecodeIndex(LazyJSValue::singleCharacterString(table.min + i), target));
            }
            m_graph.m_switchData.append(data);
            addToGraph(Switch, OpInfo(&m_graph.m_switchData.last()), get(VirtualRegister(currentInstruction[3].u.operand)));
            LAST_OPCODE(op_switch_char);
        }

        case op_switch_string: {
            SwitchData data;
            data.kind = SwitchString;
            data.switchTableIndex = currentInstruction[1].u.operand;
            data.setFallThroughBytecodeIndex(m_currentIndex + currentInstruction[2].u.operand);
            StringJumpTable& table = m_codeBlock->stringSwitchJumpTable(data.switchTableIndex);
            StringJumpTable::StringOffsetTable::iterator iter;
            StringJumpTable::StringOffsetTable::iterator end = table.offsetTable.end();
            for (iter = table.offsetTable.begin(); iter != end; ++iter) {
                unsigned target = m_currentIndex + iter->value.branchOffset;
                if (target == data.fallThroughBytecodeIndex())
                    continue;
                data.cases.append(
                    SwitchCase::withBytecodeIndex(LazyJSValue::knownStringImpl(iter->key.get()), target));
            }
            m_graph.m_switchData.append(data);
            addToGraph(Switch, OpInfo(&m_graph.m_switchData.last()), get(VirtualRegister(currentInstruction[3].u.operand)));
            LAST_OPCODE(op_switch_string);
        }

        case op_ret:
            flushArgumentsAndCapturedVariables();
            if (inlineCallFrame()) {
                ASSERT(m_inlineStackTop->m_returnValue.isValid());
                setDirect(m_inlineStackTop->m_returnValue, get(VirtualRegister(currentInstruction[1].u.operand)), ImmediateSet);
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
            flushArgumentsAndCapturedVariables();
            ASSERT(!inlineCallFrame());
            addToGraph(Return, get(VirtualRegister(currentInstruction[1].u.operand)));
            LAST_OPCODE(op_end);

        case op_throw:
            addToGraph(Throw, get(VirtualRegister(currentInstruction[1].u.operand)));
            flushAllArgumentsAndCapturedVariablesInInlineStack();
            addToGraph(Unreachable);
            LAST_OPCODE(op_throw);
            
        case op_throw_static_error:
            addToGraph(ThrowReferenceError);
            flushAllArgumentsAndCapturedVariablesInInlineStack();
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
            
            // The bytecode wouldn't have set up the arguments. But we'll do it and make it
            // look like the bytecode had done it.
            int nextRegister = registerOffset + JSStack::CallFrameHeaderSize;
            set(VirtualRegister(nextRegister++), get(VirtualRegister(thisReg)), ImmediateSet);
            for (unsigned argument = 1; argument < argCount; ++argument)
                set(VirtualRegister(nextRegister++), get(virtualRegisterForArgument(argument)), ImmediateSet);
            
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
                CheckFunction,
                OpInfo(actualPointerFor(m_inlineStackTop->m_codeBlock, currentInstruction[2].u.specialPointer)),
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
                set(VirtualRegister(dst), cellConstant(m_inlineStackTop->m_codeBlock->globalObject()));
                break;
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                JSActivation* activation = currentInstruction[5].u.activation.get();
                if (activation
                    && activation->symbolTable()->m_functionEnteredOnce.isStillValid()) {
                    addToGraph(FunctionReentryWatchpoint, OpInfo(activation->symbolTable()));
                    set(VirtualRegister(dst), cellConstant(activation));
                    break;
                }
                set(VirtualRegister(dst),
                    getScope(m_inlineStackTop->m_codeBlock->needsActivation(), depth));
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

            SpeculatedType prediction = getPrediction();
            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                GetByIdStatus status = GetByIdStatus::computeFor(*m_vm, structure, uid);
                if (status.takesSlowPath()) {
                    set(VirtualRegister(dst), addToGraph(GetByIdFlush, OpInfo(identifierNumber), OpInfo(prediction), get(VirtualRegister(scope))));
                    break;
                }
                Node* base = cellConstantWithStructureCheck(globalObject, status.structureSet().singletonStructure());
                addToGraph(Phantom, get(VirtualRegister(scope)));
                if (JSValue specificValue = status.specificValue())
                    set(VirtualRegister(dst), cellConstant(specificValue.asCell()));
                else
                    set(VirtualRegister(dst), handleGetByOffset(prediction, base, identifierNumber, operand));
                break;
            }
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks: {
                addToGraph(Phantom, get(VirtualRegister(scope)));
                SymbolTableEntry entry = globalObject->symbolTable()->get(uid);
                VariableWatchpointSet* watchpointSet = entry.watchpointSet();
                JSValue specificValue =
                    watchpointSet ? watchpointSet->inferredValue() : JSValue();
                if (!specificValue) {
                    set(VirtualRegister(dst), addToGraph(GetGlobalVar, OpInfo(operand), OpInfo(prediction)));
                    break;
                }
                
                addToGraph(VariableWatchpoint, OpInfo(watchpointSet));
                set(VirtualRegister(dst), inferredConstant(specificValue));
                break;
            }
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(VirtualRegister(scope));
                if (JSActivation* activation = m_graph.tryGetActivation(scopeNode)) {
                    SymbolTable* symbolTable = activation->symbolTable();
                    ConcurrentJITLocker locker(symbolTable->m_lock);
                    SymbolTable::Map::iterator iter = symbolTable->find(locker, uid);
                    ASSERT(iter != symbolTable->end(locker));
                    VariableWatchpointSet* watchpointSet = iter->value.watchpointSet();
                    if (watchpointSet) {
                        if (JSValue value = watchpointSet->inferredValue()) {
                            addToGraph(Phantom, scopeNode);
                            addToGraph(VariableWatchpoint, OpInfo(watchpointSet));
                            set(VirtualRegister(dst), inferredConstant(value));
                            break;
                        }
                    }
                }
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
                if (!status.isSimpleReplace()) {
                    addToGraph(PutById, OpInfo(identifierNumber), get(VirtualRegister(scope)), get(VirtualRegister(value)));
                    break;
                }
                Node* base = cellConstantWithStructureCheck(globalObject, status.oldStructure());
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
            
            if (m_vm->watchdog.isEnabled())
                addToGraph(CheckWatchdogTimer);
            
            NEXT_OPCODE(op_loop_hint);
        }
            
        case op_init_lazy_reg: {
            set(VirtualRegister(currentInstruction[1].u.operand), getJSConstantForValue(JSValue()));
            ASSERT(operandIsLocal(currentInstruction[1].u.operand));
            m_graph.m_lazyVars.set(VirtualRegister(currentInstruction[1].u.operand).toLocal());
            NEXT_OPCODE(op_init_lazy_reg);
        }
            
        case op_create_activation: {
            set(VirtualRegister(currentInstruction[1].u.operand), addToGraph(CreateActivation, get(VirtualRegister(currentInstruction[1].u.operand))));
            NEXT_OPCODE(op_create_activation);
        }
            
        case op_create_arguments: {
            m_graph.m_hasArguments = true;
            Node* createArguments = addToGraph(CreateArguments, get(VirtualRegister(currentInstruction[1].u.operand)));
            set(VirtualRegister(currentInstruction[1].u.operand), createArguments);
            set(unmodifiedArgumentsRegister(VirtualRegister(currentInstruction[1].u.operand)), createArguments);
            NEXT_OPCODE(op_create_arguments);
        }
            
        case op_tear_off_activation: {
            addToGraph(TearOffActivation, get(VirtualRegister(currentInstruction[1].u.operand)));
            NEXT_OPCODE(op_tear_off_activation);
        }

        case op_tear_off_arguments: {
            m_graph.m_hasArguments = true;
            addToGraph(TearOffArguments, get(unmodifiedArgumentsRegister(VirtualRegister(currentInstruction[1].u.operand))), get(VirtualRegister(currentInstruction[2].u.operand)));
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
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(Identity, Edge(get(VirtualRegister(currentInstruction[2].u.operand)), NumberUse)));
            NEXT_OPCODE(op_to_number);
        }
            
        case op_in: {
            set(VirtualRegister(currentInstruction[1].u.operand),
                addToGraph(In, get(VirtualRegister(currentInstruction[2].u.operand)), get(VirtualRegister(currentInstruction[3].u.operand))));
            NEXT_OPCODE(op_in);
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
        node->setTakenBlock(blockForBytecodeOffset(possibleTargets, node->takenBytecodeOffsetDuringParsing()));
        break;
        
    case Branch:
        node->setTakenBlock(blockForBytecodeOffset(possibleTargets, node->takenBytecodeOffsetDuringParsing()));
        node->setNotTakenBlock(blockForBytecodeOffset(possibleTargets, node->notTakenBytecodeOffsetDuringParsing()));
        break;
        
    case Switch:
        for (unsigned i = node->switchData()->cases.size(); i--;)
            node->switchData()->cases[i].target = blockForBytecodeOffset(possibleTargets, node->switchData()->cases[i].targetBytecodeIndex());
        node->switchData()->fallThrough = blockForBytecodeOffset(possibleTargets, node->switchData()->fallThroughBytecodeIndex());
        break;
        
    default:
        break;
    }
    
#if !ASSERT_DISABLED
    block->isLinked = true;
#endif
}

void ByteCodeParser::linkBlocks(Vector<UnlinkedBlock>& unlinkedBlocks, Vector<BasicBlock*>& possibleTargets)
{
    for (size_t i = 0; i < unlinkedBlocks.size(); ++i) {
        if (unlinkedBlocks[i].m_needsNormalLinking) {
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
    for (size_t i = 0; i < m_codeBlock->numberOfConstantRegisters(); ++i) {
        JSValue value = m_codeBlock->getConstant(i + FirstConstantRegisterIndex);
        if (!value)
            m_emptyJSValueIndex = i + FirstConstantRegisterIndex;
        else
            m_jsValueMap.add(JSValue::encode(value), i + FirstConstantRegisterIndex);
    }
    
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
    CodeSpecializationKind kind)
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
        if (m_profiledBlock->hasBaselineJITProfiling())
            m_profiledBlock->getStubInfoMap(locker, m_stubInfos);
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
        
        m_inlineCallFrame = byteCodeParser->m_graph.m_inlineCallFrames->add();
        initializeLazyWriteBarrierForInlineCallFrameExecutable(
            byteCodeParser->m_graph.m_plan.writeBarriers,
            m_inlineCallFrame->executable,
            byteCodeParser->m_codeBlock,
            m_inlineCallFrame,
            byteCodeParser->m_codeBlock->ownerExecutable(),
            codeBlock->ownerExecutable());
        m_inlineCallFrame->stackOffset = inlineCallFrameStart.offset() - JSStack::CallFrameHeaderSize;
        if (callee) {
            m_inlineCallFrame->calleeRecovery = ValueRecovery::constant(callee);
            m_inlineCallFrame->isClosureCall = false;
        } else
            m_inlineCallFrame->isClosureCall = true;
        m_inlineCallFrame->caller = byteCodeParser->currentCodeOrigin();
        m_inlineCallFrame->arguments.resize(argumentCountIncludingThis); // Set the number of arguments including this, but don't configure the value recoveries, yet.
        m_inlineCallFrame->isCall = isCall(kind);
        
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
        m_constantRemap.resize(codeBlock->numberOfConstantRegisters());
        m_constantBufferRemap.resize(codeBlock->numberOfConstantBuffers());
        m_switchRemap.resize(codeBlock->numberOfSwitchJumpTables());

        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i) {
            StringImpl* rep = codeBlock->identifier(i).impl();
            BorrowedIdentifierMap::AddResult result = byteCodeParser->m_identifierMap.add(rep, byteCodeParser->m_graph.identifiers().numberOfIdentifiers());
            if (result.isNewEntry)
                byteCodeParser->m_graph.identifiers().addLazily(rep);
            m_identifierRemap[i] = result.iterator->value;
        }
        for (size_t i = 0; i < codeBlock->numberOfConstantRegisters(); ++i) {
            JSValue value = codeBlock->getConstant(i + FirstConstantRegisterIndex);
            if (!value) {
                if (byteCodeParser->m_emptyJSValueIndex == UINT_MAX) {
                    byteCodeParser->m_emptyJSValueIndex = byteCodeParser->m_codeBlock->numberOfConstantRegisters() + FirstConstantRegisterIndex;
                    byteCodeParser->addConstant(JSValue());
                    byteCodeParser->m_constants.append(ConstantRecord());
                }
                m_constantRemap[i] = byteCodeParser->m_emptyJSValueIndex;
                continue;
            }
            JSValueMap::AddResult result = byteCodeParser->m_jsValueMap.add(JSValue::encode(value), byteCodeParser->m_codeBlock->numberOfConstantRegisters() + FirstConstantRegisterIndex);
            if (result.isNewEntry) {
                byteCodeParser->addConstant(value);
                byteCodeParser->m_constants.append(ConstantRecord());
            }
            m_constantRemap[i] = result.iterator->value;
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
        m_constantRemap.resize(codeBlock->numberOfConstantRegisters());
        m_constantBufferRemap.resize(codeBlock->numberOfConstantBuffers());
        m_switchRemap.resize(codeBlock->numberOfSwitchJumpTables());
        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i)
            m_identifierRemap[i] = i;
        for (size_t i = 0; i < codeBlock->numberOfConstantRegisters(); ++i)
            m_constantRemap[i] = i + FirstConstantRegisterIndex;
        for (size_t i = 0; i < codeBlock->numberOfConstantBuffers(); ++i)
            m_constantBufferRemap[i] = i;
        for (size_t i = 0; i < codeBlock->numberOfSwitchJumpTables(); ++i)
            m_switchRemap[i] = i;
        m_callsiteBlockHeadNeedsLinking = false;
    }
    
    for (size_t i = 0; i < m_constantRemap.size(); ++i)
        ASSERT(m_constantRemap[i] >= static_cast<unsigned>(FirstConstantRegisterIndex));
    
    byteCodeParser->m_inlineStackTop = this;
}

void ByteCodeParser::parseCodeBlock()
{
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
                    RefPtr<BasicBlock> block = adoptRef(new BasicBlock(m_currentIndex, m_numArguments, m_numLocals));
                    m_currentBlock = block.get();
                    // This assertion checks two things:
                    // 1) If the bytecodeBegin is greater than currentIndex, then something has gone
                    //    horribly wrong. So, we're probably generating incorrect code.
                    // 2) If the bytecodeBegin is equal to the currentIndex, then we failed to do
                    //    a peephole coalescing of this block in the if statement above. So, we're
                    //    generating suboptimal code and leaving more work for the CFG simplifier.
                    ASSERT(m_inlineStackTop->m_unlinkedBlocks.isEmpty() || m_inlineStackTop->m_unlinkedBlocks.last().m_block->bytecodeBegin < m_currentIndex);
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
    
    m_dfgCodeBlock = m_graph.m_plan.profiledDFGCodeBlock.get();
    if (isFTL(m_graph.m_plan.mode) && m_dfgCodeBlock) {
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
        m_codeBlock->numParameters(), CodeForCall);
    
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
