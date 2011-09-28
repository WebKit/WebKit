/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "DFGCapabilities.h"
#include "CodeBlock.h"
#include <wtf/MathExtras.h>

namespace JSC { namespace DFG {

// === ByteCodeParser ===
//
// This class is used to compile the dataflow graph from a CodeBlock.
class ByteCodeParser {
public:
    ByteCodeParser(JSGlobalData* globalData, CodeBlock* codeBlock, CodeBlock* profiledBlock, Graph& graph)
        : m_globalData(globalData)
        , m_codeBlock(codeBlock)
        , m_profiledBlock(profiledBlock)
        , m_graph(graph)
        , m_currentIndex(0)
        , m_parseFailed(false)
        , m_constantUndefined(UINT_MAX)
        , m_constantNull(UINT_MAX)
        , m_constantNaN(UINT_MAX)
        , m_constant1(UINT_MAX)
        , m_constants(codeBlock->numberOfConstantRegisters())
        , m_numArguments(codeBlock->m_numParameters)
        , m_numLocals(codeBlock->m_numCalleeRegisters)
        , m_preservedVars(codeBlock->m_numVars)
        , m_parameterSlots(0)
        , m_numPassedVarArgs(0)
        , m_globalResolveNumber(0)
    {
        ASSERT(m_profiledBlock);
    }

    // Parse a full CodeBlock of bytecode.
    bool parse();

private:
    // Helper for min and max.
    bool handleMinMax(bool usesResult, int resultOperand, NodeType op, int firstArg, int lastArg);
    
    // Handle intrinsic functions.
    bool handleIntrinsic(bool usesResult, int resultOperand, Intrinsic, int firstArg, int lastArg);
    // Parse a single basic block of bytecode instructions.
    bool parseBlock(unsigned limit);
    // Setup predecessor links in the graph's BasicBlocks.
    void setupPredecessors();
    // Link GetLocal & SetLocal nodes, to ensure live values are generated.
    enum PhiStackType {
        LocalPhiStack,
        ArgumentPhiStack
    };
    template<PhiStackType stackType>
    void processPhiStack();
    // Add spill locations to nodes.
    void allocateVirtualRegisters();

    // Get/Set the operands/result of a bytecode instruction.
    NodeIndex get(int operand)
    {
        // Is this a constant?
        if (operand >= FirstConstantRegisterIndex) {
            unsigned constant = operand - FirstConstantRegisterIndex;
            ASSERT(constant < m_constants.size());
            return getJSConstant(constant);
        }

        // Is this an argument?
        if (operandIsArgument(operand))
            return getArgument(operand);

        // Must be a local.
        return getLocal((unsigned)operand);
    }
    void set(int operand, NodeIndex value)
    {
        // Is this an argument?
        if (operandIsArgument(operand)) {
            setArgument(operand, value);
            return;
        }

        // Must be a local.
        setLocal((unsigned)operand, value);
    }

    // Used in implementing get/set, above, where the operand is a local variable.
    NodeIndex getLocal(unsigned operand)
    {
        NodeIndex nodeIndex = m_currentBlock->m_locals[operand].value;

        if (nodeIndex != NoNode) {
            Node& node = m_graph[nodeIndex];
            if (node.op == GetLocal)
                return nodeIndex;
            ASSERT(node.op == SetLocal);
            return node.child1();
        }

        // Check for reads of temporaries from prior blocks,
        // expand m_preservedVars to cover these.
        m_preservedVars = std::max(m_preservedVars, operand + 1);

        NodeIndex phi = addToGraph(Phi);
        m_localPhiStack.append(PhiStackEntry(m_currentBlock, phi, operand));
        nodeIndex = addToGraph(GetLocal, OpInfo(operand), phi);
        m_currentBlock->m_locals[operand].value = nodeIndex;
        return nodeIndex;
    }
    void setLocal(unsigned operand, NodeIndex value)
    {
        m_currentBlock->m_locals[operand].value = addToGraph(SetLocal, OpInfo(operand), value);
    }

    // Used in implementing get/set, above, where the operand is an argument.
    NodeIndex getArgument(unsigned operand)
    {
        unsigned argument = operand + m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize;
        ASSERT(argument < m_numArguments);

        NodeIndex nodeIndex = m_currentBlock->m_arguments[argument].value;

        if (nodeIndex != NoNode) {
            Node& node = m_graph[nodeIndex];
            if (node.op == GetLocal)
                return nodeIndex;
            ASSERT(node.op == SetLocal);
            return node.child1();
        }

        NodeIndex phi = addToGraph(Phi);
        m_argumentPhiStack.append(PhiStackEntry(m_currentBlock, phi, argument));
        nodeIndex = addToGraph(GetLocal, OpInfo(operand), phi);
        m_currentBlock->m_arguments[argument].value = nodeIndex;
        return nodeIndex;
    }
    void setArgument(int operand, NodeIndex value)
    {
        unsigned argument = operand + m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize;
        ASSERT(argument < m_numArguments);

        m_currentBlock->m_arguments[argument].value = addToGraph(SetLocal, OpInfo(operand), value);
    }

    // Get an operand, and perform a ToInt32/ToNumber conversion on it.
    NodeIndex getToInt32(int operand)
    {
        return toInt32(get(operand));
    }
    NodeIndex getToNumber(int operand)
    {
        return toNumber(get(operand));
    }

    // Perform an ES5 ToInt32 operation - returns a node of type NodeResultInt32.
    NodeIndex toInt32(NodeIndex index)
    {
        Node& node = m_graph[index];

        if (node.hasInt32Result())
            return index;

        if (node.op == UInt32ToNumber)
            return node.child1();

        // Check for numeric constants boxed as JSValues.
        if (node.op == JSConstant) {
            JSValue v = valueOfJSConstant(index);
            if (v.isInt32())
                return getJSConstant(node.constantNumber());
            // FIXME: We could convert the double ToInteger at this point.
        }

        return addToGraph(ValueToInt32, index);
    }

    // Perform an ES5 ToNumber operation - returns a node of type NodeResultDouble.
    NodeIndex toNumber(NodeIndex index)
    {
        Node& node = m_graph[index];

        if (node.hasNumberResult())
            return index;

        if (node.op == JSConstant) {
            JSValue v = valueOfJSConstant(index);
            if (v.isNumber())
                return getJSConstant(node.constantNumber());
        }

        return addToGraph(ValueToNumber, OpInfo(NodeUseBottom), index);
    }

    NodeIndex getJSConstant(unsigned constant)
    {
        NodeIndex index = m_constants[constant].asJSValue;
        if (index != NoNode)
            return index;

        NodeIndex resultIndex = addToGraph(JSConstant, OpInfo(constant));
        m_constants[constant].asJSValue = resultIndex;
        return resultIndex;
    }

    // Helper functions to get/set the this value.
    NodeIndex getThis()
    {
        return getArgument(m_codeBlock->thisRegister());
    }
    void setThis(NodeIndex value)
    {
        setArgument(m_codeBlock->thisRegister(), value);
    }

    // Convenience methods for checking nodes for constants.
    bool isJSConstant(NodeIndex index)
    {
        return m_graph[index].op == JSConstant;
    }
    bool isInt32Constant(NodeIndex nodeIndex)
    {
        return isJSConstant(nodeIndex) && valueOfJSConstant(nodeIndex).isInt32();
    }
    bool isSmallInt32Constant(NodeIndex nodeIndex)
    {
        if (!isJSConstant(nodeIndex))
            return false;
        JSValue value = valueOfJSConstant(nodeIndex);
        if (!value.isInt32())
            return false;
        int32_t intValue = value.asInt32();
        return intValue >= -5 && intValue <= 5;
    }
    // Convenience methods for getting constant values.
    JSValue valueOfJSConstant(NodeIndex index)
    {
        ASSERT(isJSConstant(index));
        return m_codeBlock->getConstant(FirstConstantRegisterIndex + m_graph[index].constantNumber());
    }
    int32_t valueOfInt32Constant(NodeIndex nodeIndex)
    {
        ASSERT(isInt32Constant(nodeIndex));
        return valueOfJSConstant(nodeIndex).asInt32();
    }

    // This method returns a JSConstant with the value 'undefined'.
    NodeIndex constantUndefined()
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
            m_codeBlock->addConstant(jsUndefined());
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constantUndefined must refer to an entry in the CodeBlock's constant pool that has the value 'undefined'.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantUndefined).isUndefined());
        return getJSConstant(m_constantUndefined);
    }

    // This method returns a JSConstant with the value 'null'.
    NodeIndex constantNull()
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
            m_codeBlock->addConstant(jsNull());
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constantNull must refer to an entry in the CodeBlock's constant pool that has the value 'null'.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNull).isNull());
        return getJSConstant(m_constantNull);
    }

    // This method returns a DoubleConstant with the value 1.
    NodeIndex one()
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
            m_codeBlock->addConstant(jsNumber(1));
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constant1 must refer to an entry in the CodeBlock's constant pool that has the integer value 1.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constant1).isInt32());
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constant1).asInt32() == 1);
        return getJSConstant(m_constant1);
    }
    
    // This method returns a DoubleConstant with the value NaN.
    NodeIndex constantNaN()
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
            m_codeBlock->addConstant(nan);
            m_constants.append(ConstantRecord());
            ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constantNaN must refer to an entry in the CodeBlock's constant pool that has the value nan.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNaN).isDouble());
        ASSERT(isnan(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantNaN).asDouble()));
        return getJSConstant(m_constantNaN);
    }
    
    CodeOrigin currentCodeOrigin()
    {
        return CodeOrigin(m_currentIndex);
    }

    // These methods create a node and add it to the graph. If nodes of this type are
    // 'mustGenerate' then the node  will implicitly be ref'ed to ensure generation.
    NodeIndex addToGraph(NodeType op, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
    {
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(op, currentCodeOrigin(), child1, child2, child3));

        if (op & NodeMustGenerate)
            m_graph.ref(resultIndex);
        return resultIndex;
    }
    NodeIndex addToGraph(NodeType op, OpInfo info, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
    {
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(op, currentCodeOrigin(), info, child1, child2, child3));

        if (op & NodeMustGenerate)
            m_graph.ref(resultIndex);
        return resultIndex;
    }
    NodeIndex addToGraph(NodeType op, OpInfo info1, OpInfo info2, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
    {
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(op, currentCodeOrigin(), info1, info2, child1, child2, child3));

        if (op & NodeMustGenerate)
            m_graph.ref(resultIndex);
        return resultIndex;
    }
    
    NodeIndex addToGraph(Node::VarArgTag, NodeType op, OpInfo info1, OpInfo info2)
    {
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(Node::VarArg, op, currentCodeOrigin(), info1, info2, m_graph.m_varArgChildren.size() - m_numPassedVarArgs, m_numPassedVarArgs));
        
        m_numPassedVarArgs = 0;
        
        if (op & NodeMustGenerate)
            m_graph.ref(resultIndex);
        return resultIndex;
    }
    void addVarArgChild(NodeIndex child)
    {
        m_graph.m_varArgChildren.append(child);
        m_numPassedVarArgs++;
    }
    
    NodeIndex addCall(Interpreter* interpreter, Instruction* currentInstruction, NodeType op)
    {
        Instruction* putInstruction = currentInstruction + OPCODE_LENGTH(op_call);

        PredictedType prediction = PredictNone;
        if (interpreter->getOpcodeID(putInstruction->u.opcode) == op_call_put_result)
            prediction = getPrediction(m_graph.size(), m_currentIndex + OPCODE_LENGTH(op_call));
        
        addVarArgChild(get(currentInstruction[1].u.operand));
        int argCount = currentInstruction[2].u.operand;
        int registerOffset = currentInstruction[3].u.operand;
        int firstArg = registerOffset - argCount - RegisterFile::CallFrameHeaderSize;
        for (int argIdx = firstArg + (op == Construct ? 1 : 0); argIdx < firstArg + argCount; argIdx++)
            addVarArgChild(get(argIdx));
        NodeIndex call = addToGraph(Node::VarArg, op, OpInfo(0), OpInfo(prediction));
        if (interpreter->getOpcodeID(putInstruction->u.opcode) == op_call_put_result)
            set(putInstruction[1].u.operand, call);
        if (RegisterFile::CallFrameHeaderSize + (unsigned)argCount > m_parameterSlots)
            m_parameterSlots = RegisterFile::CallFrameHeaderSize + argCount;
        return call;
    }

    PredictedType getPrediction(NodeIndex nodeIndex, unsigned bytecodeIndex)
    {
        UNUSED_PARAM(nodeIndex);
        
        ValueProfile* profile = m_profiledBlock->valueProfileForBytecodeOffset(bytecodeIndex);
        ASSERT(profile);
        PredictedType prediction = profile->computeUpdatedPrediction();
#if ENABLE(DFG_DEBUG_VERBOSE)
        printf("Dynamic [@%u, bc#%u] prediction: %s\n", nodeIndex, bytecodeIndex, predictionToString(prediction));
#endif
        
        if (prediction == PredictNone) {
            // We have no information about what values this node generates. Give up
            // on executing this code, since we're likely to do more damage than good.
            addToGraph(ForceOSRExit);
        }
        
        return prediction;
    }
    
    PredictedType getPrediction()
    {
        return getPrediction(m_graph.size(), m_currentIndex);
    }

    NodeIndex makeSafe(NodeIndex nodeIndex)
    {
        if (!m_profiledBlock->likelyToTakeSlowCase(m_currentIndex))
            return nodeIndex;
        
#if ENABLE(DFG_DEBUG_VERBOSE)
        printf("Making %s @%u safe at bc#%u because slow-case counter is at %u\n", Graph::opName(m_graph[nodeIndex].op), nodeIndex, m_currentIndex, m_profiledBlock->rareCaseProfileForBytecodeOffset(m_currentIndex)->m_counter);
#endif
        
        switch (m_graph[nodeIndex].op) {
        case UInt32ToNumber:
        case ArithAdd:
        case ArithSub:
        case ValueAdd:
        case ArithMod: // for ArithMode "MayOverflow" means we tried to divide by zero, or we saw double.
            m_graph[nodeIndex].mergeArithNodeFlags(NodeMayOverflow);
            break;
            
        case ArithMul:
            if (m_profiledBlock->likelyToTakeDeepestSlowCase(m_currentIndex))
                m_graph[nodeIndex].mergeArithNodeFlags(NodeMayOverflow | NodeMayNegZero);
            else
                m_graph[nodeIndex].mergeArithNodeFlags(NodeMayNegZero);
            break;
            
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        
        return nodeIndex;
    }
    
    NodeIndex makeDivSafe(NodeIndex nodeIndex)
    {
        ASSERT(m_graph[nodeIndex].op == ArithDiv);
        
        // The main slow case counter for op_div in the old JIT counts only when
        // the operands are not numbers. We don't care about that since we already
        // have speculations in place that take care of that separately. We only
        // care about when the outcome of the division is not an integer, which
        // is what the special fast case counter tells us.
        
        if (!m_profiledBlock->likelyToTakeSpecialFastCase(m_currentIndex))
            return nodeIndex;
        
        m_graph[nodeIndex].mergeArithNodeFlags(NodeMayOverflow | NodeMayNegZero);
        
        return nodeIndex;
    }
    
    JSGlobalData* m_globalData;
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledBlock;
    Graph& m_graph;

    // The current block being generated.
    BasicBlock* m_currentBlock;
    // The bytecode index of the current instruction being generated.
    unsigned m_currentIndex;

    // Record failures due to unimplemented functionality or regressions.
    bool m_parseFailed;

    // We use these values during code generation, and to avoid the need for
    // special handling we make sure they are available as constants in the
    // CodeBlock's constant pool. These variables are initialized to
    // UINT_MAX, and lazily updated to hold an index into the CodeBlock's
    // constant pool, as necessary.
    unsigned m_constantUndefined;
    unsigned m_constantNull;
    unsigned m_constantNaN;
    unsigned m_constant1;

    // A constant in the constant pool may be represented by more than one
    // node in the graph, depending on the context in which it is being used.
    struct ConstantRecord {
        ConstantRecord()
            : asInt32(NoNode)
            , asNumeric(NoNode)
            , asJSValue(NoNode)
        {
        }

        NodeIndex asInt32;
        NodeIndex asNumeric;
        NodeIndex asJSValue;
    };

    // Track the index of the node whose result is the current value for every
    // register value in the bytecode - argument, local, and temporary.
    Vector<ConstantRecord, 16> m_constants;

    // The number of arguments passed to the function.
    unsigned m_numArguments;
    // The number of locals (vars + temporaries) used in the function.
    unsigned m_numLocals;
    // The number of registers we need to preserve across BasicBlock boundaries;
    // typically equal to the number vars, but we expand this to cover all
    // temporaries that persist across blocks (dues to ?:, &&, ||, etc).
    unsigned m_preservedVars;
    // The number of slots (in units of sizeof(Register)) that we need to
    // preallocate for calls emanating from this frame. This includes the
    // size of the CallFrame, only if this is not a leaf function.  (I.e.
    // this is 0 if and only if this function is a leaf.)
    unsigned m_parameterSlots;
    // The number of var args passed to the next var arg node.
    unsigned m_numPassedVarArgs;
    // The index in the global resolve info.
    unsigned m_globalResolveNumber;

    struct PhiStackEntry {
        PhiStackEntry(BasicBlock* block, NodeIndex phi, unsigned varNo)
            : m_block(block)
            , m_phi(phi)
            , m_varNo(varNo)
        {
        }

        BasicBlock* m_block;
        NodeIndex m_phi;
        unsigned m_varNo;
    };
    Vector<PhiStackEntry, 16> m_argumentPhiStack;
    Vector<PhiStackEntry, 16> m_localPhiStack;
    
    Vector<Node*, 16> m_reusableNodeStack;
};

#define NEXT_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    continue

#define LAST_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    return !m_parseFailed

bool ByteCodeParser::handleMinMax(bool usesResult, int resultOperand, NodeType op, int firstArg, int lastArg)
{
    if (!usesResult)
        return true;

    if (lastArg == firstArg) {
        set(resultOperand, constantNaN());
        return true;
    }
     
    if (lastArg == firstArg + 1) {
        set(resultOperand, getToNumber(firstArg + 1));
        return true;
    }
    
    if (lastArg == firstArg + 2) {
        set(resultOperand, addToGraph(op, OpInfo(NodeUseBottom), getToNumber(firstArg + 1), getToNumber(firstArg + 2)));
        return true;
    }
    
    // Don't handle >=3 arguments for now.
    return false;
}

bool ByteCodeParser::handleIntrinsic(bool usesResult, int resultOperand, Intrinsic intrinsic, int firstArg, int lastArg)
{
    switch (intrinsic) {
    case AbsIntrinsic: {
        if (!usesResult) {
            // There is no such thing as executing abs for effect, so this
            // is dead code.
            return true;
        }
        
        // We don't care about the this argument. If we don't have a first
        // argument then make this JSConstant(NaN).
        int absArg = firstArg + 1;
        if (absArg > lastArg)
            set(resultOperand, constantNaN());
        else
            set(resultOperand, addToGraph(ArithAbs, OpInfo(NodeUseBottom), getToNumber(absArg)));
        return true;
    }
        
    case MinIntrinsic:
        return handleMinMax(usesResult, resultOperand, ArithMin, firstArg, lastArg);
        
    case MaxIntrinsic:
        return handleMinMax(usesResult, resultOperand, ArithMax, firstArg, lastArg);
        
    case SqrtIntrinsic: {
        if (!usesResult)
            return true;
        
        if (firstArg == lastArg) {
            set(resultOperand, constantNaN());
            return true;
        }
        
        set(resultOperand, addToGraph(ArithSqrt, getToNumber(firstArg + 1)));
        return true;
    }
        
    default:
        ASSERT(intrinsic == NoIntrinsic);
        return false;
    }
}

bool ByteCodeParser::parseBlock(unsigned limit)
{
    // No need to reset state initially, since it has been set by the constructor.
    if (m_currentIndex) {
        for (unsigned i = 0; i < m_constants.size(); ++i)
            m_constants[i] = ConstantRecord();
    }

    Interpreter* interpreter = m_globalData->interpreter;
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    unsigned blockBegin = m_currentIndex;
    while (true) {
        // Don't extend over jump destinations.
        if (m_currentIndex == limit) {
            addToGraph(Jump, OpInfo(m_currentIndex));
            return !m_parseFailed;
        }
        
        // Switch on the current bytecode opcode.
        Instruction* currentInstruction = instructionsBegin + m_currentIndex;
        OpcodeID opcodeID = interpreter->getOpcodeID(currentInstruction->u.opcode);
        switch (opcodeID) {

        // === Function entry opcodes ===

        case op_enter:
            // Initialize all locals to undefined.
            for (int i = 0; i < m_codeBlock->m_numVars; ++i)
                set(i, constantUndefined());
            NEXT_OPCODE(op_enter);

        case op_convert_this: {
            NodeIndex op1 = getThis();
            setThis(addToGraph(ConvertThis, op1));
            NEXT_OPCODE(op_convert_this);
        }

        case op_create_this: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CreateThis, op1));
            NEXT_OPCODE(op_create_this);
        }
            
        case op_new_object: {
            set(currentInstruction[1].u.operand, addToGraph(NewObject));
            NEXT_OPCODE(op_new_object);
        }
            
        case op_new_array: {
            int startOperand = currentInstruction[2].u.operand;
            int numOperands = currentInstruction[3].u.operand;
            for (int operandIdx = startOperand; operandIdx < startOperand + numOperands; ++operandIdx)
                addVarArgChild(get(operandIdx));
            set(currentInstruction[1].u.operand, addToGraph(Node::VarArg, NewArray, OpInfo(0), OpInfo(0)));
            NEXT_OPCODE(op_new_array);
        }
            
        case op_new_array_buffer: {
            int startConstant = currentInstruction[2].u.operand;
            int numConstants = currentInstruction[3].u.operand;
            set(currentInstruction[1].u.operand, addToGraph(NewArrayBuffer, OpInfo(startConstant), OpInfo(numConstants)));
            NEXT_OPCODE(op_new_array_buffer);
        }
            
        case op_new_regexp: {
            set(currentInstruction[1].u.operand, addToGraph(NewRegexp, OpInfo(currentInstruction[2].u.operand)));
            NEXT_OPCODE(op_new_regexp);
        }
            
        case op_get_callee: {
            set(currentInstruction[1].u.operand, addToGraph(GetCallee));
            NEXT_OPCODE(op_get_callee);
        }

        // === Bitwise operations ===

        case op_bitand: {
            NodeIndex op1 = getToInt32(currentInstruction[2].u.operand);
            NodeIndex op2 = getToInt32(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(BitAnd, op1, op2));
            NEXT_OPCODE(op_bitand);
        }

        case op_bitor: {
            NodeIndex op1 = getToInt32(currentInstruction[2].u.operand);
            NodeIndex op2 = getToInt32(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(BitOr, op1, op2));
            NEXT_OPCODE(op_bitor);
        }

        case op_bitxor: {
            NodeIndex op1 = getToInt32(currentInstruction[2].u.operand);
            NodeIndex op2 = getToInt32(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(BitXor, op1, op2));
            NEXT_OPCODE(op_bitxor);
        }

        case op_rshift: {
            NodeIndex op1 = getToInt32(currentInstruction[2].u.operand);
            NodeIndex op2 = getToInt32(currentInstruction[3].u.operand);
            NodeIndex result;
            // Optimize out shifts by zero.
            if (isInt32Constant(op2) && !(valueOfInt32Constant(op2) & 0x1f))
                result = op1;
            else
                result = addToGraph(BitRShift, op1, op2);
            set(currentInstruction[1].u.operand, result);
            NEXT_OPCODE(op_rshift);
        }

        case op_lshift: {
            NodeIndex op1 = getToInt32(currentInstruction[2].u.operand);
            NodeIndex op2 = getToInt32(currentInstruction[3].u.operand);
            NodeIndex result;
            // Optimize out shifts by zero.
            if (isInt32Constant(op2) && !(valueOfInt32Constant(op2) & 0x1f))
                result = op1;
            else
                result = addToGraph(BitLShift, op1, op2);
            set(currentInstruction[1].u.operand, result);
            NEXT_OPCODE(op_lshift);
        }

        case op_urshift: {
            NodeIndex op1 = getToInt32(currentInstruction[2].u.operand);
            NodeIndex op2 = getToInt32(currentInstruction[3].u.operand);
            NodeIndex result;
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
                    result = makeSafe(addToGraph(UInt32ToNumber, OpInfo(NodeUseBottom), op1));
            }  else {
                // Cannot optimize at this stage; shift & potentially rebox as a double.
                result = addToGraph(BitURShift, op1, op2);
                result = makeSafe(addToGraph(UInt32ToNumber, OpInfo(NodeUseBottom), result));
            }
            set(currentInstruction[1].u.operand, result);
            NEXT_OPCODE(op_urshift);
        }

        // === Increment/Decrement opcodes ===

        case op_pre_inc: {
            unsigned srcDst = currentInstruction[1].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(srcDst, makeSafe(addToGraph(ArithAdd, OpInfo(NodeUseBottom), op, one())));
            NEXT_OPCODE(op_pre_inc);
        }

        case op_post_inc: {
            unsigned result = currentInstruction[1].u.operand;
            unsigned srcDst = currentInstruction[2].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(result, op);
            set(srcDst, makeSafe(addToGraph(ArithAdd, OpInfo(NodeUseBottom), op, one())));
            NEXT_OPCODE(op_post_inc);
        }

        case op_pre_dec: {
            unsigned srcDst = currentInstruction[1].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(srcDst, makeSafe(addToGraph(ArithSub, OpInfo(NodeUseBottom), op, one())));
            NEXT_OPCODE(op_pre_dec);
        }

        case op_post_dec: {
            unsigned result = currentInstruction[1].u.operand;
            unsigned srcDst = currentInstruction[2].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(result, op);
            set(srcDst, makeSafe(addToGraph(ArithSub, OpInfo(NodeUseBottom), op, one())));
            NEXT_OPCODE(op_post_dec);
        }

        // === Arithmetic operations ===

        case op_add: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            if (m_graph[op1].hasNumberResult() && m_graph[op2].hasNumberResult())
                set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithAdd, OpInfo(NodeUseBottom), toNumber(op1), toNumber(op2))));
            else
                set(currentInstruction[1].u.operand, makeSafe(addToGraph(ValueAdd, OpInfo(NodeUseBottom), op1, op2)));
            NEXT_OPCODE(op_add);
        }

        case op_sub: {
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithSub, OpInfo(NodeUseBottom), op1, op2)));
            NEXT_OPCODE(op_sub);
        }

        case op_mul: {
            // Multiply requires that the inputs are not truncated, unfortunately.
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithMul, OpInfo(NodeUseBottom), op1, op2)));
            NEXT_OPCODE(op_mul);
        }

        case op_mod: {
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeSafe(addToGraph(ArithMod, OpInfo(NodeUseBottom), op1, op2)));
            NEXT_OPCODE(op_mod);
        }

        case op_div: {
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, makeDivSafe(addToGraph(ArithDiv, OpInfo(NodeUseBottom), op1, op2)));
            NEXT_OPCODE(op_div);
        }

        // === Misc operations ===

#if ENABLE(DEBUG_WITH_BREAKPOINT)
        case op_debug:
            addToGraph(Breakpoint);
            NEXT_OPCODE(op_debug);
#endif
        case op_mov: {
            NodeIndex op = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, op);
            NEXT_OPCODE(op_mov);
        }

        case op_check_has_instance:
            addToGraph(CheckHasInstance, get(currentInstruction[1].u.operand));
            NEXT_OPCODE(op_check_has_instance);

        case op_instanceof: {
            NodeIndex value = get(currentInstruction[2].u.operand);
            NodeIndex baseValue = get(currentInstruction[3].u.operand);
            NodeIndex prototype = get(currentInstruction[4].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(InstanceOf, value, baseValue, prototype));
            NEXT_OPCODE(op_instanceof);
        }

        case op_not: {
            NodeIndex value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, value));
            NEXT_OPCODE(op_not);
        }
            
        case op_to_primitive: {
            NodeIndex value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(ToPrimitive, value));
            NEXT_OPCODE(op_to_primitive);
        }
            
        case op_strcat: {
            int startOperand = currentInstruction[2].u.operand;
            int numOperands = currentInstruction[3].u.operand;
            for (int operandIdx = startOperand; operandIdx < startOperand + numOperands; ++operandIdx)
                addVarArgChild(get(operandIdx));
            set(currentInstruction[1].u.operand, addToGraph(Node::VarArg, StrCat, OpInfo(0), OpInfo(0)));
            NEXT_OPCODE(op_strcat);
        }

        case op_less: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareLess, op1, op2));
            NEXT_OPCODE(op_less);
        }

        case op_lesseq: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareLessEq, op1, op2));
            NEXT_OPCODE(op_lesseq);
        }

        case op_greater: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareGreater, op1, op2));
            NEXT_OPCODE(op_greater);
        }

        case op_greatereq: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareGreaterEq, op1, op2));
            NEXT_OPCODE(op_greatereq);
        }

        case op_eq: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareEq, op1, op2));
            NEXT_OPCODE(op_eq);
        }

        case op_eq_null: {
            NodeIndex value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareEq, value, constantNull()));
            NEXT_OPCODE(op_eq_null);
        }

        case op_stricteq: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(CompareStrictEq, op1, op2));
            NEXT_OPCODE(op_stricteq);
        }

        case op_neq: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, addToGraph(CompareEq, op1, op2)));
            NEXT_OPCODE(op_neq);
        }

        case op_neq_null: {
            NodeIndex value = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, addToGraph(CompareEq, value, constantNull())));
            NEXT_OPCODE(op_neq_null);
        }

        case op_nstricteq: {
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(LogicalNot, addToGraph(CompareStrictEq, op1, op2)));
            NEXT_OPCODE(op_nstricteq);
        }

        // === Property access operations ===

        case op_get_by_val: {
            PredictedType prediction = getPrediction();
            
            NodeIndex base = get(currentInstruction[2].u.operand);
            NodeIndex property = get(currentInstruction[3].u.operand);

            NodeIndex getByVal = addToGraph(GetByVal, OpInfo(0), OpInfo(prediction), base, property);
            set(currentInstruction[1].u.operand, getByVal);

            NEXT_OPCODE(op_get_by_val);
        }

        case op_put_by_val: {
            NodeIndex base = get(currentInstruction[1].u.operand);
            NodeIndex property = get(currentInstruction[2].u.operand);
            NodeIndex value = get(currentInstruction[3].u.operand);

            addToGraph(PutByVal, base, property, value);

            NEXT_OPCODE(op_put_by_val);
        }
            
        case op_method_check: {
            Instruction* getInstruction = currentInstruction + OPCODE_LENGTH(op_method_check);
            
            PredictedType prediction = getPrediction();
            
            ASSERT(interpreter->getOpcodeID(getInstruction->u.opcode) == op_get_by_id);
            
            NodeIndex base = get(getInstruction[2].u.operand);
            unsigned identifier = getInstruction[3].u.operand;
                
            // Check if the method_check was monomorphic. If so, emit a CheckXYZMethod
            // node, which is a lot more efficient.
            StructureStubInfo& stubInfo = m_profiledBlock->getStubInfo(m_currentIndex);
            MethodCallLinkInfo& methodCall = m_profiledBlock->getMethodCallLinkInfo(m_currentIndex);
            
            if (methodCall.seen && !!methodCall.cachedStructure && !stubInfo.seen) {
                // It's monomorphic as far as we can tell, since the method_check was linked
                // but the slow path (i.e. the normal get_by_id) never fired.
            
                NodeIndex checkMethod = addToGraph(CheckMethod, OpInfo(identifier), OpInfo(m_graph.m_methodCheckData.size()), base);
                set(getInstruction[1].u.operand, checkMethod);
                
                MethodCheckData methodCheckData;
                methodCheckData.structure = methodCall.cachedStructure.get();
                methodCheckData.prototypeStructure = methodCall.cachedPrototypeStructure.get();
                methodCheckData.function = methodCall.cachedFunction.get();
                methodCheckData.prototype = methodCall.cachedPrototype.get();
                m_graph.m_methodCheckData.append(methodCheckData);
            } else {
                NodeIndex getMethod = addToGraph(GetMethod, OpInfo(identifier), OpInfo(prediction), base);
                set(getInstruction[1].u.operand, getMethod);
            }
            
            m_currentIndex += OPCODE_LENGTH(op_method_check) + OPCODE_LENGTH(op_get_by_id);
            continue;
        }
        case op_get_scoped_var: {
            PredictedType prediction = getPrediction();
            int dst = currentInstruction[1].u.operand;
            int slot = currentInstruction[2].u.operand;
            int depth = currentInstruction[3].u.operand;
            NodeIndex getScopeChain = addToGraph(GetScopeChain, OpInfo(depth));
            NodeIndex getScopedVar = addToGraph(GetScopedVar, OpInfo(slot), OpInfo(prediction), getScopeChain);
            set(dst, getScopedVar);
            NEXT_OPCODE(op_get_scoped_var);
        }
        case op_put_scoped_var: {
            int slot = currentInstruction[1].u.operand;
            int depth = currentInstruction[2].u.operand;
            int source = currentInstruction[3].u.operand;
            NodeIndex getScopeChain = addToGraph(GetScopeChain, OpInfo(depth));
            addToGraph(PutScopedVar, OpInfo(slot), getScopeChain, get(source));
            NEXT_OPCODE(op_put_scoped_var);
        }
        case op_get_by_id: {
            PredictedType prediction = getPrediction();
            
            NodeIndex base = get(currentInstruction[2].u.operand);
            unsigned identifierNumber = currentInstruction[3].u.operand;
            
            StructureStubInfo& stubInfo = m_profiledBlock->getStubInfo(m_currentIndex);
            
            NodeIndex getById = NoNode;
            if (stubInfo.seen && stubInfo.accessType == access_get_by_id_self) {
                Structure* structure = stubInfo.u.getByIdSelf.baseObjectStructure.get();
                Identifier identifier = m_codeBlock->identifier(identifierNumber);
                size_t offset = structure->get(*m_globalData, identifier);
                
                if (offset != notFound) {
                    getById = addToGraph(GetByOffset, OpInfo(m_graph.m_storageAccessData.size()), OpInfo(prediction), addToGraph(CheckStructure, OpInfo(structure), base));
                    
                    StorageAccessData storageAccessData;
                    storageAccessData.offset = offset;
                    storageAccessData.identifierNumber = identifierNumber;
                    m_graph.m_storageAccessData.append(storageAccessData);
                }
            }
            
            if (getById == NoNode)
                getById = addToGraph(GetById, OpInfo(identifierNumber), OpInfo(prediction), base);
            
            set(currentInstruction[1].u.operand, getById);

            NEXT_OPCODE(op_get_by_id);
        }

        case op_put_by_id: {
            NodeIndex value = get(currentInstruction[3].u.operand);
            NodeIndex base = get(currentInstruction[1].u.operand);
            unsigned identifier = currentInstruction[2].u.operand;
            bool direct = currentInstruction[8].u.operand;

            if (direct)
                addToGraph(PutByIdDirect, OpInfo(identifier), base, value);
            else
                addToGraph(PutById, OpInfo(identifier), base, value);

            NEXT_OPCODE(op_put_by_id);
        }

        case op_get_global_var: {
            PredictedType prediction = getPrediction();
            
            NodeIndex getGlobalVar = addToGraph(GetGlobalVar, OpInfo(currentInstruction[2].u.operand));
            set(currentInstruction[1].u.operand, getGlobalVar);
            m_graph.predictGlobalVar(currentInstruction[2].u.operand, prediction);
            NEXT_OPCODE(op_get_global_var);
        }

        case op_put_global_var: {
            NodeIndex value = get(currentInstruction[2].u.operand);
            addToGraph(PutGlobalVar, OpInfo(currentInstruction[1].u.operand), value);
            NEXT_OPCODE(op_put_global_var);
        }

        // === Block terminators. ===

        case op_jmp: {
            unsigned relativeOffset = currentInstruction[1].u.operand;
            addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
            LAST_OPCODE(op_jmp);
        }

        case op_loop: {
            unsigned relativeOffset = currentInstruction[1].u.operand;
            addToGraph(Jump, OpInfo(m_currentIndex + relativeOffset));
            LAST_OPCODE(op_loop);
        }

        case op_jtrue: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            NodeIndex condition = get(currentInstruction[1].u.operand);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jtrue)), condition);
            LAST_OPCODE(op_jtrue);
        }

        case op_jfalse: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            NodeIndex condition = get(currentInstruction[1].u.operand);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jfalse)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jfalse);
        }

        case op_loop_if_true: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            NodeIndex condition = get(currentInstruction[1].u.operand);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_loop_if_true)), condition);
            LAST_OPCODE(op_loop_if_true);
        }

        case op_loop_if_false: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            NodeIndex condition = get(currentInstruction[1].u.operand);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_loop_if_false)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_loop_if_false);
        }

        case op_jeq_null: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            NodeIndex value = get(currentInstruction[1].u.operand);
            NodeIndex condition = addToGraph(CompareEq, value, constantNull());
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jeq_null)), condition);
            LAST_OPCODE(op_jeq_null);
        }

        case op_jneq_null: {
            unsigned relativeOffset = currentInstruction[2].u.operand;
            NodeIndex value = get(currentInstruction[1].u.operand);
            NodeIndex condition = addToGraph(CompareEq, value, constantNull());
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jneq_null)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jneq_null);
        }

        case op_jless: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jless)), condition);
            LAST_OPCODE(op_jless);
        }

        case op_jlesseq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jlesseq)), condition);
            LAST_OPCODE(op_jlesseq);
        }

        case op_jgreater: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jgreater)), condition);
            LAST_OPCODE(op_jgreater);
        }

        case op_jgreatereq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_jgreatereq)), condition);
            LAST_OPCODE(op_jgreatereq);
        }

        case op_jnless: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jnless)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jnless);
        }

        case op_jnlesseq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jnlesseq)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jnlesseq);
        }

        case op_jngreater: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jngreater)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jngreater);
        }

        case op_jngreatereq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + OPCODE_LENGTH(op_jngreatereq)), OpInfo(m_currentIndex + relativeOffset), condition);
            LAST_OPCODE(op_jngreatereq);
        }

        case op_loop_if_less: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareLess, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_loop_if_less)), condition);
            LAST_OPCODE(op_loop_if_less);
        }

        case op_loop_if_lesseq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareLessEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_loop_if_lesseq)), condition);
            LAST_OPCODE(op_loop_if_lesseq);
        }

        case op_loop_if_greater: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareGreater, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_loop_if_greater)), condition);
            LAST_OPCODE(op_loop_if_greater);
        }

        case op_loop_if_greatereq: {
            unsigned relativeOffset = currentInstruction[3].u.operand;
            NodeIndex op1 = get(currentInstruction[1].u.operand);
            NodeIndex op2 = get(currentInstruction[2].u.operand);
            NodeIndex condition = addToGraph(CompareGreaterEq, op1, op2);
            addToGraph(Branch, OpInfo(m_currentIndex + relativeOffset), OpInfo(m_currentIndex + OPCODE_LENGTH(op_loop_if_greatereq)), condition);
            LAST_OPCODE(op_loop_if_greatereq);
        }

        case op_ret:
            addToGraph(Return, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_ret);
            
        case op_end:
            addToGraph(Return, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_end);

        case op_throw:
            addToGraph(Throw, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_throw);
            
        case op_throw_reference_error:
            addToGraph(ThrowReferenceError);
            LAST_OPCODE(op_throw_reference_error);
            
#if USE(JSVALUE64)
        case op_call: {
            NodeIndex callTarget = get(currentInstruction[1].u.operand);
            if (m_graph.isFunctionConstant(m_codeBlock, callTarget)) {
                int argCount = currentInstruction[2].u.operand;
                int registerOffset = currentInstruction[3].u.operand;
                int firstArg = registerOffset - argCount - RegisterFile::CallFrameHeaderSize;
                int lastArg = firstArg + argCount - 1;
                
                // Do we have a result?
                bool usesResult = false;
                int resultOperand = 0; // make compiler happy
                Instruction* putInstruction = currentInstruction + OPCODE_LENGTH(op_call);
                if (interpreter->getOpcodeID(putInstruction->u.opcode) == op_call_put_result) {
                    resultOperand = putInstruction[1].u.operand;
                    usesResult = true;
                }
                
                DFG::Intrinsic intrinsic = m_graph.valueOfFunctionConstant(m_codeBlock, callTarget)->executable()->intrinsic();
                
                if (handleIntrinsic(usesResult, resultOperand, intrinsic, firstArg, lastArg)) {
                    // NEXT_OPCODE() has to be inside braces.
                    NEXT_OPCODE(op_call);
                }
            }
            
            addCall(interpreter, currentInstruction, Call);
            NEXT_OPCODE(op_call);
        }
            
        case op_construct: {
            addCall(interpreter, currentInstruction, Construct);
            NEXT_OPCODE(op_construct);
        }
#endif
            
        case op_call_put_result:
            NEXT_OPCODE(op_call_put_result);

        case op_resolve: {
            PredictedType prediction = getPrediction();
            
            unsigned identifier = currentInstruction[2].u.operand;

            NodeIndex resolve = addToGraph(Resolve, OpInfo(identifier), OpInfo(prediction));
            set(currentInstruction[1].u.operand, resolve);

            NEXT_OPCODE(op_resolve);
        }

        case op_resolve_base: {
            PredictedType prediction = getPrediction();
            
            unsigned identifier = currentInstruction[2].u.operand;

            NodeIndex resolve = addToGraph(currentInstruction[3].u.operand ? ResolveBaseStrictPut : ResolveBase, OpInfo(identifier), OpInfo(prediction));
            set(currentInstruction[1].u.operand, resolve);

            NEXT_OPCODE(op_resolve_base);
        }
            
        case op_resolve_global: {
            PredictedType prediction = getPrediction();
            
            NodeIndex resolve = addToGraph(ResolveGlobal, OpInfo(m_graph.m_resolveGlobalData.size()), OpInfo(prediction));
            m_graph.m_resolveGlobalData.append(ResolveGlobalData());
            ResolveGlobalData& data = m_graph.m_resolveGlobalData.last();
            data.identifierNumber = currentInstruction[2].u.operand;
            data.resolveInfoIndex = m_globalResolveNumber++;
            set(currentInstruction[1].u.operand, resolve);

            NEXT_OPCODE(op_resolve_global);
        }

        case op_loop_hint: {
            // Baseline->DFG OSR jumps between loop hints. The DFG assumes that Baseline->DFG
            // OSR can only happen at basic block boundaries. Assert that these two statements
            // are compatible.
            ASSERT_UNUSED(blockBegin, m_currentIndex == blockBegin);
            
            m_currentBlock->isOSRTarget = true;
            
            NEXT_OPCODE(op_loop_hint);
        }

        default:
            // Parse failed! This should not happen because the capabilities checker
            // should have caught it.
            ASSERT_NOT_REACHED();
            return false;
        }
        
        ASSERT(canCompileOpcode(opcodeID));
    }
}

template<ByteCodeParser::PhiStackType stackType>
void ByteCodeParser::processPhiStack()
{
    Vector<PhiStackEntry, 16>& phiStack = (stackType == ArgumentPhiStack) ? m_argumentPhiStack : m_localPhiStack;

    while (!phiStack.isEmpty()) {
        PhiStackEntry entry = phiStack.last();
        phiStack.removeLast();
        
        PredecessorList& predecessors = entry.m_block->m_predecessors;
        unsigned varNo = entry.m_varNo;

        for (size_t i = 0; i < predecessors.size(); ++i) {
            BasicBlock* predecessorBlock = m_graph.m_blocks[predecessors[i]].get();

            VariableRecord& var = (stackType == ArgumentPhiStack) ? predecessorBlock->m_arguments[varNo] : predecessorBlock->m_locals[varNo];

            NodeIndex valueInPredecessor = var.value;
            if (valueInPredecessor == NoNode) {
                valueInPredecessor = addToGraph(Phi);
                var.value = valueInPredecessor;
                phiStack.append(PhiStackEntry(predecessorBlock, valueInPredecessor, varNo));
            } else if (m_graph[valueInPredecessor].op == GetLocal)
                valueInPredecessor = m_graph[valueInPredecessor].child1();
            ASSERT(m_graph[valueInPredecessor].op == SetLocal || m_graph[valueInPredecessor].op == Phi);

            Node* phiNode = &m_graph[entry.m_phi];
            if (phiNode->refCount())
                m_graph.ref(valueInPredecessor);

            if (phiNode->child1() == NoNode) {
                phiNode->children.fixed.child1 = valueInPredecessor;
                continue;
            }
            if (phiNode->child2() == NoNode) {
                phiNode->children.fixed.child2 = valueInPredecessor;
                continue;
            }
            if (phiNode->child3() == NoNode) {
                phiNode->children.fixed.child3 = valueInPredecessor;
                continue;
            }

            NodeIndex newPhi = addToGraph(Phi);
            
            phiNode = &m_graph[entry.m_phi]; // reload after vector resize
            Node& newPhiNode = m_graph[newPhi];
            if (phiNode->refCount())
                m_graph.ref(newPhi);

            newPhiNode.children.fixed.child1 = phiNode->child1();
            newPhiNode.children.fixed.child2 = phiNode->child2();
            newPhiNode.children.fixed.child3 = phiNode->child3();

            phiNode->children.fixed.child1 = newPhi;
            phiNode->children.fixed.child1 = valueInPredecessor;
            phiNode->children.fixed.child3 = NoNode;
        }
    }
}

void ByteCodeParser::setupPredecessors()
{
    for (BlockIndex index = 0; index < m_graph.m_blocks.size(); ++index) {
        BasicBlock* block = m_graph.m_blocks[index].get();
        ASSERT(block->end != NoNode);
        Node& node = m_graph[block->end - 1];
        ASSERT(node.isTerminal());

        if (node.isJump())
            m_graph.blockForBytecodeOffset(node.takenBytecodeOffset()).m_predecessors.append(index);
        else if (node.isBranch()) {
            m_graph.blockForBytecodeOffset(node.takenBytecodeOffset()).m_predecessors.append(index);
            m_graph.blockForBytecodeOffset(node.notTakenBytecodeOffset()).m_predecessors.append(index);
        }
    }
}

bool ByteCodeParser::parse()
{
    // Set during construction.
    ASSERT(!m_currentIndex);

    for (unsigned jumpTargetIndex = 0; jumpTargetIndex <= m_codeBlock->numberOfJumpTargets(); ++jumpTargetIndex) {
        // The maximum bytecode offset to go into the current basicblock is either the next jump target, or the end of the instructions.
        unsigned limit = jumpTargetIndex < m_codeBlock->numberOfJumpTargets() ? m_codeBlock->jumpTarget(jumpTargetIndex) : m_codeBlock->instructions().size();
        ASSERT(m_currentIndex < limit);

        // Loop until we reach the current limit (i.e. next jump target).
        do {
            OwnPtr<BasicBlock> block = adoptPtr(new BasicBlock(m_currentIndex, m_graph.size(), m_numArguments, m_numLocals));
            m_currentBlock = block.get();
            m_graph.m_blocks.append(block.release());

            if (!parseBlock(limit))
                return false;
            // We should not have gone beyond the limit.
            ASSERT(m_currentIndex <= limit);

            m_currentBlock->end = m_graph.size();
        } while (m_currentIndex < limit);
    }

    // Should have reached the end of the instructions.
    ASSERT(m_currentIndex == m_codeBlock->instructions().size());

    setupPredecessors();
    processPhiStack<LocalPhiStack>();
    processPhiStack<ArgumentPhiStack>();
    
    m_graph.m_preservedVars = m_preservedVars;
    m_graph.m_parameterSlots = m_parameterSlots;

    return true;
}

bool parse(Graph& graph, JSGlobalData* globalData, CodeBlock* codeBlock)
{
#if DFG_DEBUG_LOCAL_DISBALE
    UNUSED_PARAM(graph);
    UNUSED_PARAM(globalData);
    UNUSED_PARAM(codeBlock);
    return false;
#else
    return ByteCodeParser(globalData, codeBlock, codeBlock->alternative(), graph).parse();
#endif
}

} } // namespace JSC::DFG

#endif
