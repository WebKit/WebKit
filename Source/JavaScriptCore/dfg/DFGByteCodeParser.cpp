/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "GetByIdStatus.h"
#include "Operations.h"
#include "PreciseJumpTargets.h"
#include "PutByIdStatus.h"
#include "StringConstructor.h"
#include <wtf/CommaPrinter.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>

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
        , m_preservedVars(m_codeBlock->m_numVars)
        , m_parameterSlots(0)
        , m_numPassedVarArgs(0)
        , m_inlineStackTop(0)
        , m_haveBuiltOperandMaps(false)
        , m_emptyJSValueIndex(UINT_MAX)
        , m_currentInstruction(0)
    {
        ASSERT(m_profiledBlock);
        
        for (int i = 0; i < m_codeBlock->m_numVars; ++i)
            m_preservedVars.set(i);
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
    void handleCall(Instruction* currentInstruction, NodeType op, CodeSpecializationKind);
    void emitFunctionChecks(const CallLinkStatus&, Node* callTarget, int registerOffset, CodeSpecializationKind);
    void emitArgumentPhantoms(int registerOffset, int argumentCountIncludingThis, CodeSpecializationKind);
    // Handle inlining. Return true if it succeeded, false if we need to plant a call.
    bool handleInlining(Node* callTargetNode, int resultOperand, const CallLinkStatus&, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, CodeSpecializationKind);
    // Handle intrinsic functions. Return true if it succeeded, false if we need to plant a call.
    bool handleIntrinsic(int resultOperand, Intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction);
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
    
    VariableAccessData* newVariableAccessData(int operand, bool isCaptured)
    {
        ASSERT(operand < FirstConstantRegisterIndex);
        
        m_graph.m_variableAccessData.append(VariableAccessData(static_cast<VirtualRegister>(operand), isCaptured));
        return &m_graph.m_variableAccessData.last();
    }
    
    // Get/Set the operands/result of a bytecode instruction.
    Node* getDirect(int operand)
    {
        // Is this a constant?
        if (operand >= FirstConstantRegisterIndex) {
            unsigned constant = operand - FirstConstantRegisterIndex;
            ASSERT(constant < m_constants.size());
            return getJSConstant(constant);
        }

        ASSERT(operand != JSStack::Callee);
        
        // Is this an argument?
        if (operandIsArgument(operand))
            return getArgument(operand);

        // Must be a local.
        return getLocal((unsigned)operand);
    }
    Node* get(int operand)
    {
        if (operand == JSStack::Callee) {
            if (inlineCallFrame() && inlineCallFrame()->callee)
                return cellConstant(inlineCallFrame()->callee.get());
            
            return getCallee();
        }
        
        return getDirect(m_inlineStackTop->remapOperand(operand));
    }
    enum SetMode { NormalSet, SetOnEntry };
    void setDirect(int operand, Node* value, SetMode setMode = NormalSet)
    {
        // Is this an argument?
        if (operandIsArgument(operand)) {
            setArgument(operand, value, setMode);
            return;
        }

        // Must be a local.
        setLocal((unsigned)operand, value, setMode);
    }
    void set(int operand, Node* value, SetMode setMode = NormalSet)
    {
        setDirect(m_inlineStackTop->remapOperand(operand), value, setMode);
    }
    
    Node* injectLazyOperandSpeculation(Node* node)
    {
        ASSERT(node->op() == GetLocal);
        ASSERT(node->codeOrigin.bytecodeIndex == m_currentIndex);
        ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
        LazyOperandValueProfileKey key(m_currentIndex, node->local());
        SpeculatedType prediction = m_inlineStackTop->m_lazyOperands.prediction(locker, key);
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLog("Lazy operand [@", node->index(), ", bc#", m_currentIndex, ", r", node->local(), "] prediction: ", SpeculationDump(prediction), "\n");
#endif
        node->variableAccessData()->predict(prediction);
        return node;
    }

    // Used in implementing get/set, above, where the operand is a local variable.
    Node* getLocal(unsigned operand)
    {
        Node* node = m_currentBlock->variablesAtTail.local(operand);
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
        } else {
            m_preservedVars.set(operand);
            variable = newVariableAccessData(operand, isCaptured);
        }
        
        node = injectLazyOperandSpeculation(addToGraph(GetLocal, OpInfo(variable)));
        m_currentBlock->variablesAtTail.local(operand) = node;
        return node;
    }
    void setLocal(unsigned operand, Node* value, SetMode setMode = NormalSet)
    {
        bool isCaptured = m_codeBlock->isCaptured(operand, inlineCallFrame());
        
        if (setMode == NormalSet) {
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
        m_currentBlock->variablesAtTail.local(operand) = node;
    }

    // Used in implementing get/set, above, where the operand is an argument.
    Node* getArgument(unsigned operand)
    {
        unsigned argument = operandToArgument(operand);
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
    void setArgument(int operand, Node* value, SetMode setMode = NormalSet)
    {
        unsigned argument = operandToArgument(operand);
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
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache));
        variableAccessData->mergeCheckArrayHoistingFailed(
            m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadIndexingType));
        Node* node = addToGraph(SetLocal, OpInfo(variableAccessData), value);
        m_currentBlock->variablesAtTail.argument(argument) = node;
    }
    
    ArgumentPosition* findArgumentPositionForArgument(int argument)
    {
        InlineStackEntry* stack = m_inlineStackTop;
        while (stack->m_inlineCallFrame)
            stack = stack->m_caller;
        return stack->m_argumentPositions[argument];
    }
    
    ArgumentPosition* findArgumentPositionForLocal(int operand)
    {
        for (InlineStackEntry* stack = m_inlineStackTop; ; stack = stack->m_caller) {
            InlineCallFrame* inlineCallFrame = stack->m_inlineCallFrame;
            if (!inlineCallFrame)
                break;
            if (operand >= static_cast<int>(inlineCallFrame->stackOffset - JSStack::CallFrameHeaderSize))
                continue;
            if (operand == inlineCallFrame->stackOffset + CallFrame::thisArgumentOffset())
                continue;
            if (operand < static_cast<int>(inlineCallFrame->stackOffset - JSStack::CallFrameHeaderSize - inlineCallFrame->arguments.size()))
                continue;
            int argument = operandToArgument(operand - inlineCallFrame->stackOffset);
            return stack->m_argumentPositions[argument];
        }
        return 0;
    }
    
    ArgumentPosition* findArgumentPosition(int operand)
    {
        if (operandIsArgument(operand))
            return findArgumentPositionForArgument(operandToArgument(operand));
        return findArgumentPositionForLocal(operand);
    }

    void addConstant(JSValue value)
    {
        initializeLazyWriteBarrierForConstant(
            m_codeBlock,
            m_graph.m_plan.writeBarriers, 
            m_codeBlock->ownerExecutable(), 
            value);
    }
    
    void flush(int operand)
    {
        flushDirect(m_inlineStackTop->remapOperand(operand));
    }
    
    void flushDirect(int operand)
    {
        flushDirect(operand, findArgumentPosition(operand));
    }
    
    void flushDirect(int operand, ArgumentPosition* argumentPosition)
    {
        bool isCaptured = m_codeBlock->isCaptured(operand, inlineCallFrame());
        
        ASSERT(operand < FirstConstantRegisterIndex);
        
        if (!operandIsArgument(operand))
            m_preservedVars.set(operand);
        
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
        if (InlineCallFrame* inlineCallFrame = inlineStackEntry->m_inlineCallFrame)
            numArguments = inlineCallFrame->arguments.size();
        else
            numArguments = inlineStackEntry->m_codeBlock->numParameters();
        for (unsigned argument = numArguments; argument-- > 1;)
            flushDirect(inlineStackEntry->remapOperand(argumentToOperand(argument)));
        for (int local = 0; local < inlineStackEntry->m_codeBlock->m_numVars; ++local) {
            if (!inlineStackEntry->m_codeBlock->isCaptured(local))
                continue;
            flushDirect(inlineStackEntry->remapOperand(local));
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

    // Get an operand, and perform a ToInt32/ToNumber conversion on it.
    Node* getToInt32(int operand)
    {
        return toInt32(get(operand));
    }

    // Perform an ES5 ToInt32 operation - returns a node of type NodeResultInt32.
    Node* toInt32(Node* node)
    {
        if (node->hasInt32Result())
            return node;

        if (node->op() == UInt32ToNumber)
            return node->child1().node();

        // Check for numeric constants boxed as JSValues.
        if (canFold(node)) {
            JSValue v = valueOfJSConstant(node);
            if (v.isInt32())
                return getJSConstant(node->constantNumber());
            if (v.isNumber())
                return getJSConstantForValue(JSValue(JSC::toInt32(v.asNumber())));
        }

        return addToGraph(ValueToInt32, node);
    }

    // NOTE: Only use this to construct constants that arise from non-speculative
    // constant folding. I.e. creating constants using this if we had constant
    // field inference would be a bad idea, since the bytecode parser's folding
    // doesn't handle liveness preservation.
    Node* getJSConstantForValue(JSValue constantValue)
    {
        unsigned constantIndex;
        if (!m_codeBlock->findConstant(constantValue, constantIndex)) {
            addConstant(constantValue);
            m_constants.append(ConstantRecord());
        }
        
        ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        
        return getJSConstant(constantIndex);
    }

    Node* getJSConstant(unsigned constant)
    {
        Node* node = m_constants[constant].asJSValue;
        if (node)
            return node;

        Node* result = addToGraph(JSConstant, OpInfo(constant));
        m_constants[constant].asJSValue = result;
        return result;
    }

    Node* getCallee()
    {
        return addToGraph(GetCallee);
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
        HashMap<JSCell*, Node*>::AddResult result = m_cellConstantNodes.add(cell, 0);
        if (result.isNewEntry)
            result.iterator->value = addToGraph(WeakJSConstant, OpInfo(cell));
        
        return result.iterator->value;
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
    
    Node* addCall(Instruction* currentInstruction, NodeType op)
    {
        SpeculatedType prediction = getPrediction();
        
        addVarArgChild(get(currentInstruction[2].u.operand));
        int argCount = currentInstruction[3].u.operand;
        if (JSStack::CallFrameHeaderSize + (unsigned)argCount > m_parameterSlots)
            m_parameterSlots = JSStack::CallFrameHeaderSize + argCount;

        int registerOffset = currentInstruction[4].u.operand;
        int dummyThisArgument = op == Call ? 0 : 1;
        for (int i = 0 + dummyThisArgument; i < argCount; ++i)
            addVarArgChild(get(registerOffset + argumentToOperand(i)));

        Node* call = addToGraph(Node::VarArg, op, OpInfo(0), OpInfo(prediction));
        set(currentInstruction[1].u.operand, call);
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
        
#if DFG_ENABLE(DEBUG_PROPAGATION_VERBOSE)
        if (m_inlineStackTop->m_profiledBlock->numberOfRareCaseProfiles())
            dataLogF("Slow case profile for bc#%u: %u\n", m_currentIndex, m_inlineStackTop->m_profiledBlock->rareCaseProfileForBytecodeOffset(m_currentIndex)->m_counter);
        dataLogF("Array profile for bc#%u: %u %s%s\n", m_currentIndex, profile->observedArrayModes(locker), profile->structureIsPolymorphic(locker) ? " (polymorphic)" : "", profile->mayInterceptIndexedAccesses(locker) ? " (may intercept)" : "");
#endif
        
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
        case ArithNegate:
        case ValueAdd:
        case ArithMod: // for ArithMod "MayOverflow" means we tried to divide by zero, or we saw double.
            node->mergeFlags(NodeMayOverflow);
            break;
            
        case ArithMul:
            if (m_inlineStackTop->m_profiledBlock->likelyToTakeDeepestSlowCase(m_currentIndex)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow)) {
#if DFG_ENABLE(DEBUG_VERBOSE)
                dataLogF("Making ArithMul @%u take deepest slow case.\n", node->index());
#endif
                node->mergeFlags(NodeMayOverflow | NodeMayNegZero);
            } else if (m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex)
                       || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero)) {
#if DFG_ENABLE(DEBUG_VERBOSE)
                dataLogF("Making ArithMul @%u take faster slow case.\n", node->index());
#endif
                node->mergeFlags(NodeMayNegZero);
            }
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
        
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLogF("Making %s @%u safe at bc#%u because special fast-case counter is at %u and exit profiles say %d, %d\n", Graph::opName(node->op()), node->index(), m_currentIndex, m_inlineStackTop->m_profiledBlock->specialFastCaseProfileForBytecodeOffset(m_currentIndex)->m_counter, m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow), m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, NegativeZero));
#endif
        
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
    // The set of registers we need to preserve across BasicBlock boundaries;
    // typically equal to the set of vars, but we expand this to cover all
    // temporaries that persist across blocks (dues to ?:, &&, ||, etc).
    BitVector m_preservedVars;
    // The number of slots (in units of sizeof(Register)) that we need to
    // preallocate for calls emanating from this frame. This includes the
    // size of the CallFrame, only if this is not a leaf function.  (I.e.
    // this is 0 if and only if this function is a leaf.)
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
        
        int remapOperand(int operand) const
        {
            if (!m_inlineCallFrame)
                return operand;
            
            if (operand >= FirstConstantRegisterIndex) {
                int result = m_constantRemap[operand - FirstConstantRegisterIndex];
                ASSERT(result >= FirstConstantRegisterIndex);
                return result;
            }

            ASSERT(operand != JSStack::Callee);

            return operand + m_inlineCallFrame->stackOffset;
        }
    };
    
    InlineStackEntry* m_inlineStackTop;

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
    
    Instruction* m_currentInstruction;
};

#define NEXT_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    continue

#define LAST_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    return shouldContinueParsing


void ByteCodeParser::handleCall(Instruction* currentInstruction, NodeType op, CodeSpecializationKind kind)
{
    ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_construct));
    
    Node* callTarget = get(currentInstruction[2].u.operand);
    
    CallLinkStatus callLinkStatus;

    if (m_graph.isConstant(callTarget))
        callLinkStatus = CallLinkStatus(m_graph.valueOfJSConstant(callTarget)).setIsProved(true);
    else {
        callLinkStatus = CallLinkStatus::computeFor(m_inlineStackTop->m_profiledBlock, m_currentIndex);
        callLinkStatus.setHasBadFunctionExitSite(m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadFunction));
        callLinkStatus.setHasBadCacheExitSite(m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache));
        callLinkStatus.setHasBadExecutableExitSite(m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadExecutable));
    }
    
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("For call at bc#", m_currentIndex, ": ", callLinkStatus, "\n");
#endif
    
    if (!callLinkStatus.canOptimize()) {
        // Oddly, this conflates calls that haven't executed with calls that behaved sufficiently polymorphically
        // that we cannot optimize them.
        
        addCall(currentInstruction, op);
        return;
    }
    
    int argumentCountIncludingThis = currentInstruction[3].u.operand;
    int registerOffset = currentInstruction[4].u.operand;

    int resultOperand = currentInstruction[1].u.operand;
    unsigned nextOffset = m_currentIndex + OPCODE_LENGTH(op_call);
    SpeculatedType prediction = getPrediction();

    if (InternalFunction* function = callLinkStatus.internalFunction()) {
        if (handleConstantInternalFunction(resultOperand, function, registerOffset, argumentCountIncludingThis, prediction, kind)) {
            // This phantoming has to be *after* the code for the intrinsic, to signify that
            // the inputs must be kept alive whatever exits the intrinsic may do.
            addToGraph(Phantom, callTarget);
            emitArgumentPhantoms(registerOffset, argumentCountIncludingThis, kind);
            return;
        }
        
        // Can only handle this using the generic call handler.
        addCall(currentInstruction, op);
        return;
    }
        
    Intrinsic intrinsic = callLinkStatus.intrinsicFor(kind);
    if (intrinsic != NoIntrinsic) {
        emitFunctionChecks(callLinkStatus, callTarget, registerOffset, kind);
            
        if (handleIntrinsic(resultOperand, intrinsic, registerOffset, argumentCountIncludingThis, prediction)) {
            // This phantoming has to be *after* the code for the intrinsic, to signify that
            // the inputs must be kept alive whatever exits the intrinsic may do.
            addToGraph(Phantom, callTarget);
            emitArgumentPhantoms(registerOffset, argumentCountIncludingThis, kind);
            if (m_graph.compilation())
                m_graph.compilation()->noticeInlinedCall();
            return;
        }
    } else if (handleInlining(callTarget, resultOperand, callLinkStatus, registerOffset, argumentCountIncludingThis, nextOffset, kind)) {
        if (m_graph.compilation())
            m_graph.compilation()->noticeInlinedCall();
        return;
    }
    
    addCall(currentInstruction, op);
}

void ByteCodeParser::emitFunctionChecks(const CallLinkStatus& callLinkStatus, Node* callTarget, int registerOffset, CodeSpecializationKind kind)
{
    Node* thisArgument;
    if (kind == CodeForCall)
        thisArgument = get(registerOffset + argumentToOperand(0));
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
        addToGraph(Phantom, get(registerOffset + argumentToOperand(i)));
}

bool ByteCodeParser::handleInlining(Node* callTargetNode, int resultOperand, const CallLinkStatus& callLinkStatus, int registerOffset, int argumentCountIncludingThis, unsigned nextOffset, CodeSpecializationKind kind)
{
    // First, the really simple checks: do we have an actual JS function?
    if (!callLinkStatus.executable())
        return false;
    if (callLinkStatus.executable()->isHostFunction())
        return false;
    
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(callLinkStatus.executable());
    
    // Does the number of arguments we're passing match the arity of the target? We currently
    // inline only if the number of arguments passed is greater than or equal to the number
    // arguments expected.
    if (static_cast<int>(executable->parameterCount()) + 1 > argumentCountIncludingThis)
        return false;
    
    // Have we exceeded inline stack depth, or are we trying to inline a recursive call?
    // If either of these are detected, then don't inline.
    unsigned depth = 0;
    for (InlineStackEntry* entry = m_inlineStackTop; entry; entry = entry->m_caller) {
        ++depth;
        if (depth >= Options::maximumInliningDepth())
            return false; // Depth exceeded.
        
        if (entry->executable() == executable)
            return false; // Recursion detected.
    }
    
    // Do we have a code block, and does the code block's size match the heuristics/requirements for
    // being an inline candidate? We might not have a code block if code was thrown away or if we
    // simply hadn't actually made this call yet. We could still theoretically attempt to inline it
    // if we had a static proof of what was being called; this might happen for example if you call a
    // global function, where watchpointing gives us static information. Overall, it's a rare case
    // because we expect that any hot callees would have already been compiled.
    CodeBlock* codeBlock = executable->baselineCodeBlockFor(kind);
    if (!codeBlock)
        return false;
    if (!canInlineFunctionFor(codeBlock, kind, callLinkStatus.isClosureCall()))
        return false;
    
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("Inlining executable %p.\n", executable);
#endif
    
    // Now we know without a doubt that we are committed to inlining. So begin the process
    // by checking the callee (if necessary) and making sure that arguments and the callee
    // are flushed.
    emitFunctionChecks(callLinkStatus, callTargetNode, registerOffset, kind);
    
    // FIXME: Don't flush constants!
    
    int inlineCallFrameStart = m_inlineStackTop->remapOperand(registerOffset) - JSStack::CallFrameHeaderSize;
    
    // Make sure that the area used by the call frame is reserved.
    for (int arg = inlineCallFrameStart + JSStack::CallFrameHeaderSize + codeBlock->m_numVars; arg-- > inlineCallFrameStart;)
        m_preservedVars.set(arg);
    
    // Make sure that we have enough locals.
    unsigned newNumLocals = inlineCallFrameStart + JSStack::CallFrameHeaderSize + codeBlock->m_numCalleeRegisters;
    if (newNumLocals > m_numLocals) {
        m_numLocals = newNumLocals;
        for (size_t i = 0; i < m_graph.numBlocks(); ++i)
            m_graph.block(i)->ensureLocals(newNumLocals);
    }
    
    size_t argumentPositionStart = m_graph.m_argumentPositions.size();

    InlineStackEntry inlineStackEntry(
        this, codeBlock, codeBlock, m_graph.lastBlock(), callLinkStatus.function(),
        (VirtualRegister)m_inlineStackTop->remapOperand(resultOperand),
        (VirtualRegister)inlineCallFrameStart, argumentCountIncludingThis, kind);
    
    // This is where the actual inlining really happens.
    unsigned oldIndex = m_currentIndex;
    m_currentIndex = 0;

    addToGraph(InlineStart, OpInfo(argumentPositionStart));
    if (callLinkStatus.isClosureCall()) {
        addToGraph(SetCallee, callTargetNode);
        addToGraph(SetMyScope, addToGraph(GetScope, callTargetNode));
    }
    
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
#if DFG_ENABLE(DEBUG_VERBOSE)
            dataLogF("Reascribing bytecode index of block %p from bc#%u to bc#%u (inline return case).\n", lastBlock, lastBlock->bytecodeBegin, m_currentIndex);
#endif
            // For debugging purposes, set the bytecodeBegin. Note that this doesn't matter
            // for release builds because this block will never serve as a potential target
            // in the linker's binary search.
            lastBlock->bytecodeBegin = m_currentIndex;
            m_inlineStackTop->m_caller->m_unlinkedBlocks.append(UnlinkedBlock(m_graph.lastBlock()));
        }
        
        m_currentBlock = m_graph.lastBlock();
        
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLogF("Done inlining executable %p, continuing code generation at epilogue.\n", executable);
#endif
        return true;
    }
    
    // If we get to this point then all blocks must end in some sort of terminals.
    ASSERT(lastBlock->last()->isTerminal());
    

    // Need to create a new basic block for the continuation at the caller.
    RefPtr<BasicBlock> block = adoptRef(new BasicBlock(nextOffset, m_numArguments, m_numLocals));

#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("Creating inline epilogue basic block %p, #%zu for %p bc#%u at inline depth %u.\n", block.get(), m_graph.numBlocks(), m_inlineStackTop->executable(), m_currentIndex, CodeOrigin::inlineDepthForCallFrame(inlineCallFrame()));
#endif

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
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("Done inlining executable %p, continuing code generation in new block.\n", executable);
#endif
    return true;
}

bool ByteCodeParser::handleMinMax(int resultOperand, NodeType op, int registerOffset, int argumentCountIncludingThis)
{
    if (argumentCountIncludingThis == 1) { // Math.min()
        set(resultOperand, constantNaN());
        return true;
    }
     
    if (argumentCountIncludingThis == 2) { // Math.min(x)
        Node* result = get(registerOffset + argumentToOperand(1));
        addToGraph(Phantom, Edge(result, NumberUse));
        set(resultOperand, result);
        return true;
    }
    
    if (argumentCountIncludingThis == 3) { // Math.min(x, y)
        set(resultOperand, addToGraph(op, get(registerOffset + argumentToOperand(1)), get(registerOffset + argumentToOperand(2))));
        return true;
    }
    
    // Don't handle >=3 arguments for now.
    return false;
}

// FIXME: We dead-code-eliminate unused Math intrinsics, but that's invalid because
// they need to perform the ToNumber conversion, which can have side-effects.
bool ByteCodeParser::handleIntrinsic(int resultOperand, Intrinsic intrinsic, int registerOffset, int argumentCountIncludingThis, SpeculatedType prediction)
{
    switch (intrinsic) {
    case AbsIntrinsic: {
        if (argumentCountIncludingThis == 1) { // Math.abs()
            set(resultOperand, constantNaN());
            return true;
        }

        if (!MacroAssembler::supportsFloatingPointAbs())
            return false;

        Node* node = addToGraph(ArithAbs, get(registerOffset + argumentToOperand(1)));
        if (m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, Overflow))
            node->mergeFlags(NodeMayOverflow);
        set(resultOperand, node);
        return true;
    }

    case MinIntrinsic:
        return handleMinMax(resultOperand, ArithMin, registerOffset, argumentCountIncludingThis);
        
    case MaxIntrinsic:
        return handleMinMax(resultOperand, ArithMax, registerOffset, argumentCountIncludingThis);
        
    case SqrtIntrinsic: {
        if (argumentCountIncludingThis == 1) { // Math.sqrt()
            set(resultOperand, constantNaN());
            return true;
        }
        
        if (!MacroAssembler::supportsFloatingPointSqrt())
            return false;

        set(resultOperand, addToGraph(ArithSqrt, get(registerOffset + argumentToOperand(1))));
        return true;
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
            Node* arrayPush = addToGraph(ArrayPush, OpInfo(arrayMode.asWord()), OpInfo(prediction), get(registerOffset + argumentToOperand(0)), get(registerOffset + argumentToOperand(1)));
            set(resultOperand, arrayPush);
            
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
            Node* arrayPop = addToGraph(ArrayPop, OpInfo(arrayMode.asWord()), OpInfo(prediction), get(registerOffset + argumentToOperand(0)));
            set(resultOperand, arrayPop);
            return true;
        }
            
        default:
            return false;
        }
    }

    case CharCodeAtIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        int thisOperand = registerOffset + argumentToOperand(0);
        int indexOperand = registerOffset + argumentToOperand(1);
        Node* charCode = addToGraph(StringCharCodeAt, OpInfo(ArrayMode(Array::String).asWord()), get(thisOperand), getToInt32(indexOperand));

        set(resultOperand, charCode);
        return true;
    }

    case CharAtIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        int thisOperand = registerOffset + argumentToOperand(0);
        int indexOperand = registerOffset + argumentToOperand(1);
        Node* charCode = addToGraph(StringCharAt, OpInfo(ArrayMode(Array::String).asWord()), get(thisOperand), getToInt32(indexOperand));

        set(resultOperand, charCode);
        return true;
    }
    case FromCharCodeIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;

        int indexOperand = registerOffset + argumentToOperand(1);
        Node* charCode = addToGraph(StringFromCharCode, getToInt32(indexOperand));

        set(resultOperand, charCode);

        return true;
    }

    case RegExpExecIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        Node* regExpExec = addToGraph(RegExpExec, OpInfo(0), OpInfo(prediction), get(registerOffset + argumentToOperand(0)), get(registerOffset + argumentToOperand(1)));
        set(resultOperand, regExpExec);
        
        return true;
    }
        
    case RegExpTestIntrinsic: {
        if (argumentCountIncludingThis != 2)
            return false;
        
        Node* regExpExec = addToGraph(RegExpTest, OpInfo(0), OpInfo(prediction), get(registerOffset + argumentToOperand(0)), get(registerOffset + argumentToOperand(1)));
        set(resultOperand, regExpExec);
        
        return true;
    }

    case IMulIntrinsic: {
        if (argumentCountIncludingThis != 3)
            return false;
        int leftOperand = registerOffset + argumentToOperand(1);
        int rightOperand = registerOffset + argumentToOperand(2);
        Node* left = getToInt32(leftOperand);
        Node* right = getToInt32(rightOperand);
        set(resultOperand, addToGraph(ArithIMul, left, right));
        return true;
    }
        
    default:
        return false;
    }
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
            set(resultOperand,
                addToGraph(NewArrayWithSize, OpInfo(ArrayWithUndecided), get(registerOffset + argumentToOperand(1))));
            return true;
        }
        
        for (int i = 1; i < argumentCountIncludingThis; ++i)
            addVarArgChild(get(registerOffset + argumentToOperand(i)));
        set(resultOperand,
            addToGraph(Node::VarArg, NewArray, OpInfo(ArrayWithUndecided), OpInfo(0)));
        return true;
    } else if (function->classInfo() == StringConstructor::info()) {
        Node* result;
        
        if (argumentCountIncludingThis <= 1)
            result = cellConstant(m_vm->smallStrings.emptyString());
        else
            result = addToGraph(ToString, get(registerOffset + argumentToOperand(1)));
        
        if (kind == CodeForConstruct)
            result = addToGraph(NewStringObject, OpInfo(function->globalObject()->stringObjectStructure()), result);
        
        set(resultOperand, result);
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
    set(destinationOperand, handleGetByOffset(prediction, base, identifierNumber, offset));
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
    if (!getByIdStatus.isSimple()
        || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
        || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadWeakConstantCache)) {
        set(destinationOperand,
            addToGraph(
                getByIdStatus.makesCalls() ? GetByIdFlush : GetById,
                OpInfo(identifierNumber), OpInfo(prediction), base));
        return;
    }
    
    ASSERT(getByIdStatus.structureSet().size());
                
    // The implementation of GetByOffset does not know to terminate speculative
    // execution if it doesn't have a prediction, so we do it manually.
    if (prediction == SpecNone)
        addToGraph(ForceOSRExit);
    else if (m_graph.compilation())
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
        
        set(destinationOperand, cellConstant(getByIdStatus.specificValue().asCell()));
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
    Node* localBase;
    if (inlineCallFrame() && !inlineCallFrame()->isClosureCall()) {
        ASSERT(inlineCallFrame()->callee);
        localBase = cellConstant(inlineCallFrame()->callee->scope());
    } else
        localBase = addToGraph(GetMyScope);
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
                argumentToOperand(argument), m_codeBlock->isCaptured(argumentToOperand(argument)));
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
            else {
#if DFG_ENABLE(DEBUG_VERBOSE)
                dataLogF("Refusing to plant jump at limit %u because block %p is empty.\n", limit, m_currentBlock);
#endif
            }
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
                set(i, constantUndefined(), SetOnEntry);
            NEXT_OPCODE(op_enter);

        case op_to_this: {
            Node* op1 = getThis();
            if (op1->op() != ToThis) {
                ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                ValueProfile* profile =
                    m_inlineStackTop->m_profiledBlock->valueProfileForBytecodeOffset(m_currentIndex);
                profile->computeUpdatedPrediction(locker);
#if DFG_ENABLE(DEBUG_VERBOSE)
                dataLogF("[bc#%u]: profile %p: ", m_currentIndex, profile);
                profile->dump(WTF::dataFile());
                dataLogF("\n");
#endif
                if (profile->m_singletonValueIsTop
                    || !profile->m_singletonValue
                    || !profile->m_singletonValue.isCell()
                    || profile->m_singletonValue.asCell()->classInfo() != Structure::info())
                    setThis(addToGraph(ToThis, op1));
                else {
                    addToGraph(
                        CheckStructure,
                        OpInfo(m_graph.addStructureSet(jsCast<Structure*>(profile->m_singletonValue.asCell()))),
                        op1);
                }
            }
            NEXT_OPCODE(op_to_this);
        }

        case op_create_this: {
            int calleeOperand = currentInstruction[2].u.operand;
            Node* callee = get(calleeOperand);
            bool alreadyEmitted = false;
            if (callee->op() == WeakJSConstant) {
                JSCell* cell = callee->weakConstant();
                ASSERT(cell->inherits(JSFunction::info()));
                
                JSFunction* function = jsCast<JSFunction*>(cell);
                ObjectAllocationProfile* allocationProfile = function->tryGetAllocationProfile();
                if (allocationProfile) {
                    addToGraph(AllocationProfileWatchpoint, OpInfo(function));
                    // The callee is still live up to this point.
                    addToGraph(Phantom, callee);
                    set(currentInstruction[1].u.operand,
                        addToGraph(NewObject, OpInfo(allocationProfile->structure())));
                    alreadyEmitted = true;
                }
            }
            if (!alreadyEmitted)
                set(currentInstruction[1].u.operand,
                    addToGraph(CreateThis, OpInfo(currentInstruction[3].u.operand), callee));
            NEXT_OPCODE(op_create_this);
        }

        case op_new_object: {
            set(currentInstruction[1].u.operand,
                addToGraph(NewObject,
                    OpInfo(currentInstruction[3].u.objectAllocationProfile->structure())));
            NEXT_OPCODE(op_new_object);
        }
            
        case op_new_array: {
            int startOperand = currentInstruction[2].u.operand;
            int numOperands = currentInstruction[3].u.operand;
            ArrayAllocationProfile* profile = currentInstruction[4].u.arrayAllocationProfile;
            for (int operandIdx = startOperand; operandIdx < startOperand + numOperands; ++operandIdx)
                addVarArgChild(get(operandIdx));
            set(currentInstruction[1].u.operand, addToGraph(Node::VarArg, NewArray, OpInfo(profile->selectIndexingType()), OpInfo(0)));
            NEXT_OPCODE(op_new_array);
        }
            
        case op_new_array_with_size: {
            int lengthOperand = currentInstruction[2].u.operand;
            ArrayAllocationProfile* profile = currentInstruction[3].u.arrayAllocationProfile;
            set(currentInstruction[1].u.operand, addToGraph(NewArrayWithSize, OpInfo(profile->selectIndexingType()), get(lengthOperand)));
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
            set(currentInstruction[1].u.operand, addToGraph(NewArrayBuffer, OpInfo(&m_graph.m_newArrayBufferData.last())));
            NEXT_OPCODE(op_new_array_buffer);
        }
            
        case op_new_regexp: {
            set(currentInstruction[1].u.operand, addToGraph(NewRegexp, OpInfo(currentInstruction[2].u.operand)));
            NEXT_OPCODE(op_new_regexp);
        }
            
        case op_get_callee: {
            ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
            ValueProfile* profile = currentInstruction[2].u.profile;
            profile->computeUpdatedPrediction(locker);
            if (profile->m_singletonValueIsTop
                || !profile->m_singletonValue
                || !profile->m_singletonValue.isCell())
                set(currentInstruction[1].u.operand, get(JSStack::Callee));
            else {
                ASSERT(profile->m_singletonValue.asCell()->inherits(JSFunction::info()));
                Node* actualCallee = get(JSStack::Callee);
                addToGraph(CheckFunction, OpInfo(profile->m_singletonValue.asCell()), actualCallee);
                set(currentInstruction[1].u.operand, addToGraph(WeakJSConstant, OpInfo(profile->m_singletonValue.asCell())));
            }
            NEXT_OPCODE(op_get_callee);
        }

        // === Bitwise operations ===

        case op_bitand: {
            Node* op1 = getToInt32(currentInstruction[2].u.operand);
            Node* op2 = getToInt32(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(BitAnd, op1, op2));
            NEXT_OPCODE(op_bitand);
        }

        case op_bitor: {
            Node* op1 = getToInt32(currentInstruction[2].u.operand);
            Node* op2 = getToInt32(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(BitOr, op1, op2));
            NEXT_OPCODE(op_bitor);
        }

        case op_bitxor: {
            Node* op1 = getToInt32(currentInstruction[2].u.operand);
            Node* op2 = getToInt32(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(BitXor, op1, op2));
            NEXT_OPCODE(op_bitxor);
        }

        case op_rshift: {
            Node* op1 = getToInt32(currentInstruction[2].u.operand);
            Node* op2 = getToInt32(currentInstruction[3].u.operand);
            Node* result;
            // Optimize out shifts by zero.
            if (isInt32Constant(op2) && !(valueOfInt32Constant(op2) & 0x1f))
                result = op1;
            else
                result = addToGraph(BitRShift, op1, op2);
            set(currentInstruction[1].u.operand, result);
            NEXT_OPCODE(op_rshift);
        }

        case op_lshift: {
            Node* op1 = getToInt32(currentInstruction[2].u.operand);
            Node* op2 = getToInt32(currentInstruction[3].u.operand);
            Node* result;
            // Optimize out shifts by zero.
            if (isInt32Constant(op2) && !(valueOfInt32Constant(op2) & 0x1f))
                result = op1;
            else
                result = addToGraph(BitLShift, op1, op2);
            set(currentInstruction[1].u.operand, result);
            NEXT_OPCODE(op_lshift);
        }

        case op_urshift: {
            Node* op1 = getToInt32(currentInstruction[2].u.operand);
            Node* op2 = getToInt32(currentInstruction[3].u.operand);
            Node* result;
            // The result of a zero-extending right shift is treated as an unsigned value.
            // This means that if the top bit is set, the result is not in the int32 range,
            // and as such must be stored as a double. If the shift amount is a constant,
            // we may be able to optimize.
            if (isInt32Constant(op2)) {
                // If we know we are shifting by a non-zero amount, then since the operation
                // zero fills we know the top bit of the result must be zero, and as such the
                // result must be within the int32 range. Conversely, if this is a shift by
                // zero, then the result may be changed by the conversion to unsigned, but it
                // is not necessary to perform the shift!
                if (valueOfInt32Constant(op2) & 0x1f)
                    result = addToGraph(BitURShift, op1, op2);
                else
                    result = makeSafe(addToGraph(UInt32ToNumber, op1));
            }  else {
                // Cannot optimize at this stage; shift & potentially rebox as a double.
                result = addToGraph(BitURShift, op1, op2);
                result = makeSafe(addToGraph(UInt32ToNumber, result));
            }
            set(currentInstruction[1].u.operand, result);
            NEXT_OPCODE(op_urshift);
        }

        // === Increment/Decrement opcodes ===

        case op_inc: {
            unsigned srcDst = currentInstruction[1].u.operand;
            Node* op = get(srcDst);
            set(srcDst, makeSafe(addToGraph(ArithAdd, op, one())));
            NEXT_OPCODE(op_inc);
        }

        case op_dec: {
            unsigned srcDst = currentInstruction[1].u.operand;
            Node* op = get(srcDst);
            set(srcDst, makeSafe(addToGraph(ArithSub, op, one())));
            NEXT_OPCODE(op_dec);
        }

        // === Arithmetic operations ===

        case op_add: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (op1->hasNumberResult() && op2->hasNumberResult())
                set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithAdd, op1, op2)));
            else
                set(currentInstruction[1].u.operand, makeSafe(addToGraph(ValueAdd, op1, op2)));
            NEXT_OPCODE(op_add);
        }

        case op_sub: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithSub, op1, op2)));
            NEXT_OPCODE(op_sub);
        }

        case op_negate: {
            Node* op1 = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithNegate, op1)));
            NEXT_OPCODE(op_negate);
        }

        case op_mul: {
            // Multiply requires that the inputs are not truncated, unfortunately.
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithMul, op1, op2)));
            NEXT_OPCODE(op_mul);
        }

        case op_mod: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithMod, op1, op2)));
            NEXT_OPCODE(op_mod);
        }

        case op_div: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeDivSafe(addToGraph(ArithDiv, op1, op2)));
            NEXT_OPCODE(op_div);
        }

        // === Misc operations ===

#if ENABLE(DEBUG_WITH_BREAKPOINT)
        case op_debug:
            addToGraph(Breakpoint);
            NEXT_OPCODE(op_debug);
#endif
        case op_mov: {
            Node* op = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, op);
            NEXT_OPCODE(op_mov);
        }

        case op_check_has_instance:
            addToGraph(CheckHasInstance, get(currentInstruction[3].u.operand));
            NEXT_OPCODE(op_check_has_instance);

        case op_instanceof: {
            Node* value = get(currentInstruction[2].u.operand);
            Node* prototype = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(InstanceOf, value, prototype));
            NEXT_OPCODE(op_instanceof);
        }
            
        case op_is_undefined: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(IsUndefined, value));
            NEXT_OPCODE(op_is_undefined);
        }

        case op_is_boolean: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(IsBoolean, value));
            NEXT_OPCODE(op_is_boolean);
        }

        case op_is_number: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(IsNumber, value));
            NEXT_OPCODE(op_is_number);
        }

        case op_is_string: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(IsString, value));
            NEXT_OPCODE(op_is_string);
        }

        case op_is_object: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(IsObject, value));
            NEXT_OPCODE(op_is_object);
        }

        case op_is_function: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(IsFunction, value));
            NEXT_OPCODE(op_is_function);
        }

        case op_not: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, value));
            NEXT_OPCODE(op_not);
        }
            
        case op_to_primitive: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(ToPrimitive, value));
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
            OwnArrayPtr<Node*> toStringNodes = adoptArrayPtr(new Node*[numOperands]);
            for (int i = 0; i < numOperands; i++)
                toStringNodes[i] = addToGraph(ToString, get(startOperand + i));

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
            set(currentInstruction[1].u.operand,
                addToGraph(MakeRope, operands[0], operands[1], operands[2]));
            NEXT_OPCODE(op_strcat);
        }

        case op_less: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(currentInstruction[1].u.operand,
                        getJSConstantForValue(jsBoolean(a.asNumber() < b.asNumber())));
                    NEXT_OPCODE(op_less);
                }
            }
            set(currentInstruction[1].u.operand, addToGraph(CompareLess, op1, op2));
            NEXT_OPCODE(op_less);
        }

        case op_lesseq: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(currentInstruction[1].u.operand,
                        getJSConstantForValue(jsBoolean(a.asNumber() <= b.asNumber())));
                    NEXT_OPCODE(op_lesseq);
                }
            }
            set(currentInstruction[1].u.operand, addToGraph(CompareLessEq, op1, op2));
            NEXT_OPCODE(op_lesseq);
        }

        case op_greater: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(currentInstruction[1].u.operand,
                        getJSConstantForValue(jsBoolean(a.asNumber() > b.asNumber())));
                    NEXT_OPCODE(op_greater);
                }
            }
            set(currentInstruction[1].u.operand, addToGraph(CompareGreater, op1, op2));
            NEXT_OPCODE(op_greater);
        }

        case op_greatereq: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                if (a.isNumber() && b.isNumber()) {
                    set(currentInstruction[1].u.operand,
                        getJSConstantForValue(jsBoolean(a.asNumber() >= b.asNumber())));
                    NEXT_OPCODE(op_greatereq);
                }
            }
            set(currentInstruction[1].u.operand, addToGraph(CompareGreaterEq, op1, op2));
            NEXT_OPCODE(op_greatereq);
        }

        case op_eq: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(currentInstruction[1].u.operand,
                    getJSConstantForValue(jsBoolean(JSValue::equal(m_codeBlock->globalObject()->globalExec(), a, b))));
                NEXT_OPCODE(op_eq);
            }
            set(currentInstruction[1].u.operand, addToGraph(CompareEq, op1, op2));
            NEXT_OPCODE(op_eq);
        }

        case op_eq_null: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareEqConstant, value, constantNull()));
            NEXT_OPCODE(op_eq_null);
        }

        case op_stricteq: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(currentInstruction[1].u.operand,
                    getJSConstantForValue(jsBoolean(JSValue::strictEqual(m_codeBlock->globalObject()->globalExec(), a, b))));
                NEXT_OPCODE(op_stricteq);
            }
            if (isConstantForCompareStrictEq(op1))
                set(currentInstruction[1].u.operand, addToGraph(CompareStrictEqConstant, op2, op1));
            else if (isConstantForCompareStrictEq(op2))
                set(currentInstruction[1].u.operand, addToGraph(CompareStrictEqConstant, op1, op2));
            else
                set(currentInstruction[1].u.operand, addToGraph(CompareStrictEq, op1, op2));
            NEXT_OPCODE(op_stricteq);
        }

        case op_neq: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(currentInstruction[1].u.operand,
                    getJSConstantForValue(jsBoolean(!JSValue::equal(m_codeBlock->globalObject()->globalExec(), a, b))));
                NEXT_OPCODE(op_neq);
            }
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, addToGraph(CompareEq, op1, op2)));
            NEXT_OPCODE(op_neq);
        }

        case op_neq_null: {
            Node* value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, addToGraph(CompareEqConstant, value, constantNull())));
            NEXT_OPCODE(op_neq_null);
        }

        case op_nstricteq: {
            Node* op1 = get(currentInstruction[2].u.operand);
            Node* op2 = get(currentInstruction[3].u.operand);
            if (canFold(op1) && canFold(op2)) {
                JSValue a = valueOfJSConstant(op1);
                JSValue b = valueOfJSConstant(op2);
                set(currentInstruction[1].u.operand,
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
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, invertedResult));
            NEXT_OPCODE(op_nstricteq);
        }

        // === Property access operations ===

        case op_get_by_val: {
            SpeculatedType prediction = getPrediction();
            
            Node* base = get(currentInstruction[2].u.operand);
            ArrayMode arrayMode = getArrayModeConsideringSlowPath(currentInstruction[4].u.arrayProfile, Array::Read);
            Node* property = get(currentInstruction[3].u.operand);
            Node* getByVal = addToGraph(GetByVal, OpInfo(arrayMode.asWord()), OpInfo(prediction), base, property);
            set(currentInstruction[1].u.operand, getByVal);

            NEXT_OPCODE(op_get_by_val);
        }

        case op_put_by_val: {
            Node* base = get(currentInstruction[1].u.operand);

            ArrayMode arrayMode = getArrayModeConsideringSlowPath(currentInstruction[4].u.arrayProfile, Array::Write);
            
            Node* property = get(currentInstruction[2].u.operand);
            Node* value = get(currentInstruction[3].u.operand);
            
            addVarArgChild(base);
            addVarArgChild(property);
            addVarArgChild(value);
            addVarArgChild(0); // Leave room for property storage.
            addToGraph(Node::VarArg, PutByVal, OpInfo(arrayMode.asWord()), OpInfo(0));

            NEXT_OPCODE(op_put_by_val);
        }
            
        case op_get_by_id:
        case op_get_by_id_out_of_line:
        case op_get_array_length: {
            SpeculatedType prediction = getPrediction();
            
            Node* base = get(currentInstruction[2].u.operand);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[3].u.operand];
            
            StringImpl* uid = m_graph.identifiers()[identifierNumber];
            GetByIdStatus getByIdStatus = GetByIdStatus::computeFor(
                m_inlineStackTop->m_profiledBlock, m_currentIndex, uid);
            
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
            Node* value = get(currentInstruction[3].u.operand);
            Node* base = get(currentInstruction[1].u.operand);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[2].u.operand];
            bool direct = currentInstruction[8].u.operand;

            PutByIdStatus putByIdStatus = PutByIdStatus::computeFor(
                m_inlineStackTop->m_profiledBlock,
                m_currentIndex,
                m_graph.identifiers()[identifierNumber]);
            bool canCountAsInlined = true;
            if (!putByIdStatus.isSet()) {
                addToGraph(ForceOSRExit);
                canCountAsInlined = false;
            }
            
            bool hasExitSite =
                m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadCache)
                || m_inlineStackTop->m_exitProfile.hasExitSite(m_currentIndex, BadWeakConstantCache);
            
            if (!hasExitSite && putByIdStatus.isSimpleReplace()) {
                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(putByIdStatus.oldStructure())), base);
                handlePutByOffset(base, identifierNumber, putByIdStatus.offset(), value);
            } else if (
                !hasExitSite
                && putByIdStatus.isSimpleTransition()
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
            Node* value = get(currentInstruction[2].u.operand);
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
            Node* condition = get(currentInstruction[1].u.operand);
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
            Node* condition = get(currentInstruction[1].u.operand);
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
            Node* value = get(currentInstruction[1].u.operand);
            Node* condition = addToGraph(CompareEqConstant, value, constantNull());
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jeq_null)), condition);
            LAST_OPCODE(op_jeq_null);
        }

        case op_jneq_null: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            Node* value = get(currentInstruction[1].u.operand);
            Node* condition = addToGraph(CompareEqConstant, value, constantNull());
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jneq_null)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jneq_null);
        }

        case op_jless: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
            Node* op1 = get(currentInstruction[1].u.operand);
            Node* op2 = get(currentInstruction[2].u.operand);
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
                data.cases.append(SwitchCase::withBytecodeIndex(jsNumber(table.min + i), target));
            }
            m_graph.m_switchData.append(data);
            addToGraph(Switch, OpInfo(&m_graph.m_switchData.last()), get(currentInstruction[3].u.operand));
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
            addToGraph(Switch, OpInfo(&m_graph.m_switchData.last()), get(currentInstruction[3].u.operand));
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
            addToGraph(Switch, OpInfo(&m_graph.m_switchData.last()), get(currentInstruction[3].u.operand));
            LAST_OPCODE(op_switch_string);
        }

        case op_ret:
            flushArgumentsAndCapturedVariables();
            if (inlineCallFrame()) {
                ASSERT(m_inlineStackTop->m_returnValue != InvalidVirtualRegister);
                setDirect(m_inlineStackTop->m_returnValue, get(currentInstruction[1].u.operand));
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
            addToGraph(Return, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_ret);
            
        case op_end:
            flushArgumentsAndCapturedVariables();
            ASSERT(!inlineCallFrame());
            addToGraph(Return, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_end);

        case op_throw:
            addToGraph(Throw, get(currentInstruction[1].u.operand));
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
            ASSERT(inlineCallFrame());
            ASSERT(currentInstruction[4].u.operand == m_inlineStackTop->m_codeBlock->argumentsRegister());
            ASSERT(!m_inlineStackTop->m_codeBlock->symbolTable()->slowArguments());
            // It would be cool to funnel this into handleCall() so that it can handle
            // inlining. But currently that won't be profitable anyway, since none of the
            // uses of call_varargs will be inlineable. So we set this up manually and
            // without inline/intrinsic detection.
            
            SpeculatedType prediction = getPrediction();
            
            addToGraph(CheckArgumentsNotCreated);
            
            unsigned argCount = inlineCallFrame()->arguments.size();
            if (JSStack::CallFrameHeaderSize + argCount > m_parameterSlots)
                m_parameterSlots = JSStack::CallFrameHeaderSize + argCount;
            
            addVarArgChild(get(currentInstruction[2].u.operand)); // callee
            addVarArgChild(get(currentInstruction[3].u.operand)); // this
            for (unsigned argument = 1; argument < argCount; ++argument)
                addVarArgChild(get(argumentToOperand(argument)));
            
            set(currentInstruction[1].u.operand,
                addToGraph(Node::VarArg, Call, OpInfo(0), OpInfo(prediction)));
            
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
                get(currentInstruction[1].u.operand));
            addToGraph(Jump, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jneq_ptr)));
            LAST_OPCODE(op_jneq_ptr);

        case op_resolve_scope: {
            unsigned dst = currentInstruction[1].u.operand;
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
                set(dst, cellConstant(m_inlineStackTop->m_codeBlock->globalObject()));
                break;
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks:
                set(dst, getScope(m_inlineStackTop->m_codeBlock->needsActivation(), depth));
                break;
            case Dynamic:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            NEXT_OPCODE(op_resolve_scope);
        }

        case op_get_from_scope: {
            unsigned dst = currentInstruction[1].u.operand;
            unsigned scope = currentInstruction[2].u.operand;
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[3].u.operand];
            StringImpl* uid = m_graph.identifiers()[identifierNumber];
            ResolveType resolveType = ResolveModeAndType(currentInstruction[4].u.operand).type();

            Structure* structure;
            uintptr_t operand;
            {
                ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                structure = currentInstruction[5].u.structure.get();
                operand = reinterpret_cast<uintptr_t>(currentInstruction[6].u.pointer);
            }

            SpeculatedType prediction = getPrediction();
            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                GetByIdStatus status = GetByIdStatus::computeFor(*m_vm, structure, uid);
                if (status.takesSlowPath()) {
                    set(dst, addToGraph(GetByIdFlush, OpInfo(identifierNumber), OpInfo(prediction), get(scope)));
                    break;
                }
                Node* base = cellConstantWithStructureCheck(globalObject, status.structureSet().singletonStructure());
                if (JSValue specificValue = status.specificValue())
                    set(dst, cellConstant(specificValue.asCell()));
                else
                    set(dst, handleGetByOffset(prediction, base, identifierNumber, operand));
                break;
            }
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks: {
                SymbolTableEntry entry = globalObject->symbolTable()->get(uid);
                if (!entry.couldBeWatched() || !m_graph.watchpoints().isStillValid(entry.watchpointSet())) {
                    set(dst, addToGraph(GetGlobalVar, OpInfo(operand), OpInfo(prediction)));
                    break;
                }

                addToGraph(GlobalVarWatchpoint, OpInfo(operand), OpInfo(identifierNumber));
                JSValue specificValue = globalObject->registerAt(entry.getIndex()).get();
                set(dst, cellConstant(specificValue.asCell()));
                break;
            }
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks:
                set(dst, 
                    addToGraph(GetClosureVar, OpInfo(operand), OpInfo(prediction), 
                        addToGraph(GetClosureRegisters, get(scope))));
                break;
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

            Structure* structure;
            uintptr_t operand;
            {
                ConcurrentJITLocker locker(m_inlineStackTop->m_profiledBlock->m_lock);
                structure = currentInstruction[5].u.structure.get();
                operand = reinterpret_cast<uintptr_t>(currentInstruction[6].u.pointer);
            }

            JSGlobalObject* globalObject = m_inlineStackTop->m_codeBlock->globalObject();

            switch (resolveType) {
            case GlobalProperty:
            case GlobalPropertyWithVarInjectionChecks: {
                PutByIdStatus status = PutByIdStatus::computeFor(*m_vm, globalObject, structure, uid, false);
                if (!status.isSimpleReplace()) {
                    addToGraph(PutById, OpInfo(identifierNumber), get(scope), get(value));
                    break;
                }
                Node* base = cellConstantWithStructureCheck(globalObject, status.oldStructure());
                handlePutByOffset(base, identifierNumber, static_cast<PropertyOffset>(operand), get(value));
                break;
            }
            case GlobalVar:
            case GlobalVarWithVarInjectionChecks: {
                SymbolTableEntry entry = globalObject->symbolTable()->get(uid);
                ASSERT(!entry.couldBeWatched() || !m_graph.watchpoints().isStillValid(entry.watchpointSet()));
                addToGraph(PutGlobalVar, OpInfo(operand), get(value));
                break;
            }
            case ClosureVar:
            case ClosureVarWithVarInjectionChecks: {
                Node* scopeNode = get(scope);
                Node* scopeRegisters = addToGraph(GetClosureRegisters, scopeNode);
                addToGraph(PutClosureVar, OpInfo(operand), scopeNode, scopeRegisters, get(value));
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

            if (m_vm->watchdog.isEnabled())
                addToGraph(CheckWatchdogTimer);
            else {
                // Emit a phantom node to ensure that there is a placeholder
                // node for this bytecode op.
                addToGraph(Phantom);
            }
            
            NEXT_OPCODE(op_loop_hint);
        }
            
        case op_init_lazy_reg: {
            set(currentInstruction[1].u.operand, getJSConstantForValue(JSValue()));
            NEXT_OPCODE(op_init_lazy_reg);
        }
            
        case op_create_activation: {
            set(currentInstruction[1].u.operand, addToGraph(CreateActivation, get(currentInstruction[1].u.operand)));
            NEXT_OPCODE(op_create_activation);
        }
            
        case op_create_arguments: {
            m_graph.m_hasArguments = true;
            Node* createArguments = addToGraph(CreateArguments, get(currentInstruction[1].u.operand));
            set(currentInstruction[1].u.operand, createArguments);
            set(unmodifiedArgumentsRegister(currentInstruction[1].u.operand), createArguments);
            NEXT_OPCODE(op_create_arguments);
        }
            
        case op_tear_off_activation: {
            addToGraph(TearOffActivation, get(currentInstruction[1].u.operand));
            NEXT_OPCODE(op_tear_off_activation);
        }

        case op_tear_off_arguments: {
            m_graph.m_hasArguments = true;
            addToGraph(TearOffArguments, get(unmodifiedArgumentsRegister(currentInstruction[1].u.operand)), get(currentInstruction[2].u.operand));
            NEXT_OPCODE(op_tear_off_arguments);
        }
            
        case op_get_arguments_length: {
            m_graph.m_hasArguments = true;
            set(currentInstruction[1].u.operand, addToGraph(GetMyArgumentsLengthSafe));
            NEXT_OPCODE(op_get_arguments_length);
        }
            
        case op_get_argument_by_val: {
            m_graph.m_hasArguments = true;
            set(currentInstruction[1].u.operand,
                addToGraph(
                    GetMyArgumentByValSafe, OpInfo(0), OpInfo(getPrediction()),
                    get(currentInstruction[3].u.operand)));
            NEXT_OPCODE(op_get_argument_by_val);
        }
            
        case op_new_func: {
            if (!currentInstruction[3].u.operand) {
                set(currentInstruction[1].u.operand,
                    addToGraph(NewFunctionNoCheck, OpInfo(currentInstruction[2].u.operand)));
            } else {
                set(currentInstruction[1].u.operand,
                    addToGraph(
                        NewFunction,
                        OpInfo(currentInstruction[2].u.operand),
                        get(currentInstruction[1].u.operand)));
            }
            NEXT_OPCODE(op_new_func);
        }
            
        case op_new_func_exp: {
            set(currentInstruction[1].u.operand,
                addToGraph(NewFunctionExpression, OpInfo(currentInstruction[2].u.operand)));
            NEXT_OPCODE(op_new_func_exp);
        }

        case op_typeof: {
            set(currentInstruction[1].u.operand,
                addToGraph(TypeOf, get(currentInstruction[2].u.operand)));
            NEXT_OPCODE(op_typeof);
        }

        case op_to_number: {
            set(currentInstruction[1].u.operand,
                addToGraph(Identity, Edge(get(currentInstruction[2].u.operand), NumberUse)));
            NEXT_OPCODE(op_to_number);
        }
            
        case op_in: {
            set(currentInstruction[1].u.operand,
                addToGraph(In, get(currentInstruction[2].u.operand), get(currentInstruction[3].u.operand)));
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
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLogF("Linked basic block %p to %p, #%u.\n", block, node->takenBlock(), node->takenBlock()->index);
#endif
        break;
        
    case Branch:
        node->setTakenBlock(blockForBytecodeOffset(possibleTargets, node->takenBytecodeOffsetDuringParsing()));
        node->setNotTakenBlock(blockForBytecodeOffset(possibleTargets, node->notTakenBytecodeOffsetDuringParsing()));
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLogF("Linked basic block %p to %p, #%u and %p, #%u.\n", block, node->takenBlock(), node->takenBlock()->index, node->notTakenBlock(), node->notTakenBlock()->index);
#endif
        break;
        
    case Switch:
        for (unsigned i = node->switchData()->cases.size(); i--;)
            node->switchData()->cases[i].target = blockForBytecodeOffset(possibleTargets, node->switchData()->cases[i].targetBytecodeIndex());
        node->switchData()->fallThrough = blockForBytecodeOffset(possibleTargets, node->switchData()->fallThroughBytecodeIndex());
        break;
        
    default:
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLogF("Marking basic block %p as linked.\n", block);
#endif
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
        ASSERT(inlineCallFrameStart != InvalidVirtualRegister);
        ASSERT(callsiteBlockHead);
        
        InlineCallFrame inlineCallFrame;
        initializeLazyWriteBarrier(
            inlineCallFrame.executable,
            byteCodeParser->m_graph.m_plan.writeBarriers,
            byteCodeParser->m_codeBlock->ownerExecutable(), 
            codeBlock->ownerExecutable());
        inlineCallFrame.stackOffset = inlineCallFrameStart + JSStack::CallFrameHeaderSize;
        if (callee) {
            initializeLazyWriteBarrier(
                inlineCallFrame.callee,
                byteCodeParser->m_graph.m_plan.writeBarriers,
                byteCodeParser->m_codeBlock->ownerExecutable(), 
                callee);
        }
        inlineCallFrame.caller = byteCodeParser->currentCodeOrigin();
        inlineCallFrame.arguments.resize(argumentCountIncludingThis); // Set the number of arguments including this, but don't configure the value recoveries, yet.
        inlineCallFrame.isCall = isCall(kind);
        
        if (inlineCallFrame.caller.inlineCallFrame)
            inlineCallFrame.capturedVars = inlineCallFrame.caller.inlineCallFrame->capturedVars;
        else {
            for (int i = byteCodeParser->m_codeBlock->m_numVars; i--;) {
                if (byteCodeParser->m_codeBlock->isCaptured(i))
                    inlineCallFrame.capturedVars.set(i);
            }
        }

        for (int i = argumentCountIncludingThis; i--;) {
            if (codeBlock->isCaptured(argumentToOperand(i)))
                inlineCallFrame.capturedVars.set(argumentToOperand(i) + inlineCallFrame.stackOffset);
        }
        for (size_t i = codeBlock->m_numVars; i--;) {
            if (codeBlock->isCaptured(i))
                inlineCallFrame.capturedVars.set(i + inlineCallFrame.stackOffset);
        }

#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLogF("Current captured variables: ");
        inlineCallFrame.capturedVars.dump(WTF::dataFile());
        dataLogF("\n");
#endif
        
        byteCodeParser->m_codeBlock->inlineCallFrames().append(inlineCallFrame);
        m_inlineCallFrame = &byteCodeParser->m_codeBlock->inlineCallFrames().last();
        
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
        ASSERT(returnValueVR == InvalidVirtualRegister);
        ASSERT(inlineCallFrameStart == InvalidVirtualRegister);
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
#if DFG_ENABLE(DEBUG_VERBOSE)
    shouldDumpBytecode |= true;
#endif
    if (shouldDumpBytecode) {
        dataLog("Parsing ", *codeBlock);
        if (inlineCallFrame()) {
            dataLog(
                " for inlining at ", CodeBlockWithJITType(m_codeBlock, JITCode::DFGJIT),
                " ", inlineCallFrame()->caller);
        }
        dataLog(
            ": captureCount = ", codeBlock->symbolTable() ? codeBlock->symbolTable()->captureCount() : 0,
            ", needsFullScopeChain = ", codeBlock->needsFullScopeChain(),
            ", needsActivation = ", codeBlock->ownerExecutable()->needsActivation(),
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
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLog(
            "Parsing bytecode with limit ", pointerDump(inlineCallFrame()),
            " bc#", limit, " at inline depth ",
            CodeOrigin::inlineDepthForCallFrame(inlineCallFrame()), ".\n");
#endif
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
#if DFG_ENABLE(DEBUG_VERBOSE)
                    dataLogF("Reascribing bytecode index of block %p from bc#%u to bc#%u (peephole case).\n", m_currentBlock, m_currentBlock->bytecodeBegin, m_currentIndex);
#endif
                    m_currentBlock->bytecodeBegin = m_currentIndex;
                } else {
                    RefPtr<BasicBlock> block = adoptRef(new BasicBlock(m_currentIndex, m_numArguments, m_numLocals));
#if DFG_ENABLE(DEBUG_VERBOSE)
                    dataLogF("Creating basic block %p, #%zu for %p bc#%u at inline depth %u.\n", block.get(), m_graph.numBlocks(), m_inlineStackTop->executable(), m_currentIndex, CodeOrigin::inlineDepthForCallFrame(inlineCallFrame()));
#endif
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
    
#if DFG_ENABLE(ALL_VARIABLES_CAPTURED)
    // We should be pretending that the code has an activation.
    ASSERT(m_graph.needsActivation());
#endif
    
    InlineStackEntry inlineStackEntry(
        this, m_codeBlock, m_profiledBlock, 0, 0, InvalidVirtualRegister, InvalidVirtualRegister,
        m_codeBlock->numParameters(), CodeForCall);
    
    parseCodeBlock();

    linkBlocks(inlineStackEntry.m_unlinkedBlocks, inlineStackEntry.m_blockLinkingTargets);
    m_graph.determineReachability();
    
    ASSERT(m_preservedVars.size());
    size_t numberOfLocals = 0;
    for (size_t i = m_preservedVars.size(); i--;) {
        if (m_preservedVars.quickGet(i)) {
            numberOfLocals = i + 1;
            break;
        }
    }
    
    for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
        BasicBlock* block = m_graph.block(blockIndex);
        ASSERT(block);
        if (!block->isReachable) {
            m_graph.killBlock(block);
            continue;
        }
        
        block->variablesAtHead.ensureLocals(numberOfLocals);
        block->variablesAtTail.ensureLocals(numberOfLocals);
    }
    
    m_graph.m_preservedVars = m_preservedVars;
    m_graph.m_localVars = m_numLocals;
    m_graph.m_parameterSlots = m_parameterSlots;

    return true;
}

bool parse(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Parsing");
#if DFG_DEBUG_LOCAL_DISBALE
    UNUSED_PARAM(graph);
    return false;
#else
    return ByteCodeParser(graph).parse();
#endif
}

} } // namespace JSC::DFG

#endif
