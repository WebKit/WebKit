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

#include "DFGByteCodeCache.h"
#include "DFGCapabilities.h"
#include "CodeBlock.h"
#include <wtf/HashMap.h>
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
        , m_currentBlock(0)
        , m_currentIndex(0)
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
        , m_inlineStackTop(0)
        , m_haveBuiltOperandMaps(false)
    {
        ASSERT(m_profiledBlock);
        
        for (int i = 0; i < codeBlock->m_numVars; ++i)
            m_preservedVars.set(i);
    }

    // Parse a full CodeBlock of bytecode.
    bool parse();
    
private:
    // Just parse from m_currentIndex to the end of the current CodeBlock.
    void parseCodeBlock();

    // Helper for min and max.
    bool handleMinMax(bool usesResult, int resultOperand, NodeType op, int firstArg, int lastArg);
    
    // Handle calls. This resolves issues surrounding inlining and intrinsics.
    void handleCall(Interpreter*, Instruction* currentInstruction, NodeType op, CodeSpecializationKind);
    // Handle inlining. Return true if it succeeded, false if we need to plant a call.
    bool handleInlining(bool usesResult, int callTarget, NodeIndex callTargetNodeIndex, int resultOperand, bool certainAboutExpectedFunction, JSFunction*, int firstArg, int lastArg, unsigned nextOffset, CodeSpecializationKind);
    // Handle intrinsic functions. Return true if it succeeded, false if we need to plant a call.
    bool handleIntrinsic(bool usesResult, int resultOperand, Intrinsic, int firstArg, int lastArg, PredictedType prediction);
    // Prepare to parse a block.
    void prepareToParseBlock();
    // Parse a single basic block of bytecode instructions.
    bool parseBlock(unsigned limit);
    // Find reachable code and setup predecessor links in the graph's BasicBlocks.
    void determineReachability();
    // Enqueue a block onto the worklist, if necessary.
    void handleSuccessor(Vector<BlockIndex, 16>& worklist, BlockIndex, BlockIndex successor);
    // Link block successors.
    void linkBlock(BasicBlock*, Vector<BlockIndex>& possibleTargets);
    void linkBlocks(Vector<UnlinkedBlock>& unlinkedBlocks, Vector<BlockIndex>& possibleTargets);
    // Link GetLocal & SetLocal nodes, to ensure live values are generated.
    enum PhiStackType {
        LocalPhiStack,
        ArgumentPhiStack
    };
    template<PhiStackType stackType>
    void processPhiStack();
    // Add spill locations to nodes.
    void allocateVirtualRegisters();
    
    VariableAccessData* newVariableAccessData(int operand)
    {
        ASSERT(operand < FirstConstantRegisterIndex);
        
        m_graph.m_variableAccessData.append(VariableAccessData(static_cast<VirtualRegister>(operand)));
        return &m_graph.m_variableAccessData.last();
    }
    
    // Get/Set the operands/result of a bytecode instruction.
    NodeIndex getDirect(int operand)
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
    NodeIndex get(int operand)
    {
        return getDirect(m_inlineStackTop->remapOperand(operand));
    }
    void setDirect(int operand, NodeIndex value)
    {
        // Is this an argument?
        if (operandIsArgument(operand)) {
            setArgument(operand, value);
            return;
        }

        // Must be a local.
        setLocal((unsigned)operand, value);
    }
    void set(int operand, NodeIndex value)
    {
        setDirect(m_inlineStackTop->remapOperand(operand), value);
    }

    // Used in implementing get/set, above, where the operand is a local variable.
    NodeIndex getLocal(unsigned operand)
    {
        NodeIndex nodeIndex = m_currentBlock->variablesAtTail.local(operand);
        
        if (nodeIndex != NoNode) {
            Node* nodePtr = &m_graph[nodeIndex];
            if (nodePtr->op == Flush) {
                // Two possibilities: either the block wants the local to be live
                // but has not loaded its value, or it has loaded its value, in
                // which case we're done.
                Node& flushChild = m_graph[nodePtr->child1()];
                if (flushChild.op == Phi) {
                    VariableAccessData* variableAccessData = flushChild.variableAccessData();
                    nodeIndex = addToGraph(GetLocal, OpInfo(variableAccessData), nodePtr->child1());
                    m_currentBlock->variablesAtTail.local(operand) = nodeIndex;
                    return nodeIndex;
                }
                nodePtr = &flushChild;
            }
            if (nodePtr->op == GetLocal)
                return nodeIndex;
            ASSERT(nodePtr->op == SetLocal);
            return nodePtr->child1();
        }

        // Check for reads of temporaries from prior blocks,
        // expand m_preservedVars to cover these.
        m_preservedVars.set(operand);
        
        VariableAccessData* variableAccessData = newVariableAccessData(operand);
        
        NodeIndex phi = addToGraph(Phi, OpInfo(variableAccessData));
        m_localPhiStack.append(PhiStackEntry(m_currentBlock, phi, operand));
        nodeIndex = addToGraph(GetLocal, OpInfo(variableAccessData), phi);
        m_currentBlock->variablesAtTail.local(operand) = nodeIndex;
        
        m_currentBlock->variablesAtHead.setLocalFirstTime(operand, nodeIndex);
        
        return nodeIndex;
    }
    void setLocal(unsigned operand, NodeIndex value)
    {
        m_currentBlock->variablesAtTail.local(operand) = addToGraph(SetLocal, OpInfo(newVariableAccessData(operand)), value);
    }

    // Used in implementing get/set, above, where the operand is an argument.
    NodeIndex getArgument(unsigned operand)
    {
        unsigned argument = operand + m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize;
        ASSERT(argument < m_numArguments);

        NodeIndex nodeIndex = m_currentBlock->variablesAtTail.argument(argument);

        if (nodeIndex != NoNode) {
            Node* nodePtr = &m_graph[nodeIndex];
            if (nodePtr->op == Flush) {
                // Two possibilities: either the block wants the local to be live
                // but has not loaded its value, or it has loaded its value, in
                // which case we're done.
                Node& flushChild = m_graph[nodePtr->child1()];
                if (flushChild.op == Phi) {
                    VariableAccessData* variableAccessData = flushChild.variableAccessData();
                    nodeIndex = addToGraph(GetLocal, OpInfo(variableAccessData), nodePtr->child1());
                    m_currentBlock->variablesAtTail.local(operand) = nodeIndex;
                    return nodeIndex;
                }
                nodePtr = &flushChild;
            }
            if (nodePtr->op == SetArgument) {
                // We're getting an argument in the first basic block; link
                // the GetLocal to the SetArgument.
                ASSERT(nodePtr->local() == static_cast<VirtualRegister>(operand));
                nodeIndex = addToGraph(GetLocal, OpInfo(nodePtr->variableAccessData()), nodeIndex);
                m_currentBlock->variablesAtTail.argument(argument) = nodeIndex;
                return nodeIndex;
            }
            
            if (nodePtr->op == GetLocal)
                return nodeIndex;
            
            ASSERT(nodePtr->op == SetLocal);
            return nodePtr->child1();
        }
        
        VariableAccessData* variableAccessData = newVariableAccessData(operand);

        NodeIndex phi = addToGraph(Phi, OpInfo(variableAccessData));
        m_argumentPhiStack.append(PhiStackEntry(m_currentBlock, phi, argument));
        nodeIndex = addToGraph(GetLocal, OpInfo(variableAccessData), phi);
        m_currentBlock->variablesAtTail.argument(argument) = nodeIndex;
        
        m_currentBlock->variablesAtHead.setArgumentFirstTime(argument, nodeIndex);
        
        return nodeIndex;
    }
    void setArgument(int operand, NodeIndex value)
    {
        unsigned argument = operand + m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize;
        ASSERT(argument < m_numArguments);

        m_currentBlock->variablesAtTail.argument(argument) = addToGraph(SetLocal, OpInfo(newVariableAccessData(operand)), value);
    }
    
    void flush(int operand)
    {
        // FIXME: This should check if the same operand had already been flushed to
        // some other local variable.
        
        operand = m_inlineStackTop->remapOperand(operand);
        
        ASSERT(operand < FirstConstantRegisterIndex);
        
        NodeIndex nodeIndex;
        int index;
        if (operandIsArgument(operand)) {
            index = operand + m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize;
            nodeIndex = m_currentBlock->variablesAtTail.argument(index);
        } else {
            index = operand;
            nodeIndex = m_currentBlock->variablesAtTail.local(index);
            m_preservedVars.set(operand);
        }
        
        if (nodeIndex != NoNode) {
            Node& node = m_graph[nodeIndex];
            if (node.op == Flush || node.op == SetArgument) {
                // If a local has already been flushed, or if it's an argument in the
                // first basic block, then there is really no need to flush it. In fact
                // emitting a Flush instruction could just confuse things, since the
                // getArgument() code assumes that we never see a Flush of a SetArgument.
                return;
            }
        
            addToGraph(Flush, OpInfo(node.variableAccessData()), nodeIndex);
            return;
        }
        
        VariableAccessData* variableAccessData = newVariableAccessData(operand);
        NodeIndex phi = addToGraph(Phi, OpInfo(variableAccessData));
        nodeIndex = addToGraph(Flush, OpInfo(variableAccessData), phi);
        if (operandIsArgument(operand)) {
            m_argumentPhiStack.append(PhiStackEntry(m_currentBlock, phi, index));
            m_currentBlock->variablesAtTail.argument(index) = nodeIndex;
            m_currentBlock->variablesAtHead.setArgumentFirstTime(index, nodeIndex);
        } else {
            m_localPhiStack.append(PhiStackEntry(m_currentBlock, phi, index));
            m_currentBlock->variablesAtTail.local(index) = nodeIndex;
            m_currentBlock->variablesAtHead.setLocalFirstTime(index, nodeIndex);
        }
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
        return get(m_inlineStackTop->m_codeBlock->thisRegister());
    }
    void setThis(NodeIndex value)
    {
        set(m_inlineStackTop->m_codeBlock->thisRegister(), value);
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
    
    unsigned getCellConstantIndex(JSCell* cell)
    {
        HashMap<JSCell*, unsigned>::iterator iter = m_cellConstants.find(cell);
        if (iter != m_cellConstants.end())
            return iter->second;
        
        m_codeBlock->addConstant(cell);
        m_constants.append(ConstantRecord());
        ASSERT(m_constants.size() == m_codeBlock->numberOfConstantRegisters());
        
        m_cellConstants.add(cell, m_codeBlock->numberOfConstantRegisters() - 1);
        
        return m_codeBlock->numberOfConstantRegisters() - 1;
    }
    
    void pinCell(JSCell* cell)
    {
        if (!cell)
            return;
        getCellConstantIndex(cell);
    }
    
    NodeIndex cellConstant(JSCell* cell)
    {
        pair<HashMap<JSCell*, NodeIndex>::iterator, bool> iter = m_cellConstantNodes.add(cell, NoNode);
        if (iter.second)
            iter.first->second = addToGraph(WeakJSConstant, OpInfo(cell));
        
        return iter.first->second;
    }
    
    CodeOrigin currentCodeOrigin()
    {
        return CodeOrigin(m_currentIndex, m_inlineStackTop->m_inlineCallFrame);
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
    
    PredictedType getPredictionWithoutOSRExit(NodeIndex nodeIndex, unsigned bytecodeIndex)
    {
        UNUSED_PARAM(nodeIndex);
        
        ValueProfile* profile = m_inlineStackTop->m_profiledBlock->valueProfileForBytecodeOffset(bytecodeIndex);
        ASSERT(profile);
        PredictedType prediction = profile->computeUpdatedPrediction();
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Dynamic [@%u, bc#%u] prediction: %s\n", nodeIndex, bytecodeIndex, predictionToString(prediction));
#endif
        
        return prediction;
    }

    PredictedType getPrediction(NodeIndex nodeIndex, unsigned bytecodeIndex)
    {
        PredictedType prediction = getPredictionWithoutOSRExit(nodeIndex, bytecodeIndex);
        
        if (prediction == PredictNone) {
            // We have no information about what values this node generates. Give up
            // on executing this code, since we're likely to do more damage than good.
            addToGraph(ForceOSRExit);
        }
        
        return prediction;
    }
    
    PredictedType getPredictionWithoutOSRExit()
    {
        return getPredictionWithoutOSRExit(m_graph.size(), m_currentIndex);
    }
    
    PredictedType getPrediction()
    {
        return getPrediction(m_graph.size(), m_currentIndex);
    }

    NodeIndex makeSafe(NodeIndex nodeIndex)
    {
        if (!m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex))
            return nodeIndex;
        
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Making %s @%u safe at bc#%u because slow-case counter is at %u\n", Graph::opName(m_graph[nodeIndex].op), nodeIndex, m_currentIndex, m_inlineStackTop->m_profiledBlock->rareCaseProfileForBytecodeOffset(m_currentIndex)->m_counter);
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
            if (m_inlineStackTop->m_profiledBlock->likelyToTakeDeepestSlowCase(m_currentIndex))
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
        
        if (!m_inlineStackTop->m_profiledBlock->likelyToTakeSpecialFastCase(m_currentIndex))
            return nodeIndex;
        
        m_graph[nodeIndex].mergeArithNodeFlags(NodeMayOverflow | NodeMayNegZero);
        
        return nodeIndex;
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
    
    JSGlobalData* m_globalData;
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
    HashMap<JSCell*, NodeIndex> m_cellConstantNodes;

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
    
    struct InlineStackEntry {
        ByteCodeParser* m_byteCodeParser;
        
        CodeBlock* m_codeBlock;
        CodeBlock* m_profiledBlock;
        InlineCallFrame* m_inlineCallFrame;
        VirtualRegister m_calleeVR; // absolute virtual register, not relative to call frame
        
        ScriptExecutable* executable() { return m_codeBlock->ownerExecutable(); }
        
        // Remapping of identifier and constant numbers from the code block being
        // inlined (inline callee) to the code block that we're inlining into
        // (the machine code block, which is the transitive, though not necessarily
        // direct, caller).
        Vector<unsigned> m_identifierRemap;
        Vector<unsigned> m_constantRemap;
        
        // Blocks introduced by this code block, which need successor linking.
        // May include up to one basic block that includes the continuation after
        // the callsite in the caller. These must be appended in the order that they
        // are created, but their bytecodeBegin values need not be in order as they
        // are ignored.
        Vector<UnlinkedBlock> m_unlinkedBlocks;
        
        // Potential block linking targets. Must be sorted by bytecodeBegin, and
        // cannot have two blocks that have the same bytecodeBegin. For this very
        // reason, this is not equivalent to 
        Vector<BlockIndex> m_blockLinkingTargets;
        
        // If the callsite's basic block was split into two, then this will be
        // the head of the callsite block. It needs its successors linked to the
        // m_unlinkedBlocks, but not the other way around: there's no way for
        // any blocks in m_unlinkedBlocks to jump back into this block.
        BlockIndex m_callsiteBlockHead;
        
        // Does the callsite block head need linking? This is typically true
        // but will be false for the machine code block's inline stack entry
        // (since that one is not inlined) and for cases where an inline callee
        // did the linking for us.
        bool m_callsiteBlockHeadNeedsLinking;
        
        VirtualRegister m_returnValue;
        
        // Did we see any returns? We need to handle the (uncommon but necessary)
        // case where a procedure that does not return was inlined.
        bool m_didReturn;
        
        // Did we have any early returns?
        bool m_didEarlyReturn;
        
        InlineStackEntry* m_caller;
        
        InlineStackEntry(ByteCodeParser*, CodeBlock*, CodeBlock* profiledBlock, BlockIndex callsiteBlockHead, VirtualRegister calleeVR, JSFunction* callee, VirtualRegister returnValueVR, VirtualRegister inlineCallFrameStart, CodeSpecializationKind);
        
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
            
            return operand + m_inlineCallFrame->stackOffset;
        }
    };
    
    InlineStackEntry* m_inlineStackTop;

    // Have we built operand maps? We initialize them lazily, and only when doing
    // inlining.
    bool m_haveBuiltOperandMaps;
    // Mapping between identifier names and numbers.
    IdentifierMap m_identifierMap;
    // Mapping between values and constant numbers.
    JSValueMap m_jsValueMap;
    
    // Cache of code blocks that we've generated bytecode for.
    ByteCodeCache<canInlineFunctionFor> m_codeBlockCache;
};

#define NEXT_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    continue

#define LAST_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    return shouldContinueParsing

void ByteCodeParser::handleCall(Interpreter* interpreter, Instruction* currentInstruction, NodeType op, CodeSpecializationKind kind)
{
    ASSERT(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_construct));
    
    NodeIndex callTarget = get(currentInstruction[1].u.operand);
    enum { ConstantFunction, LinkedFunction, UnknownFunction } callType;
            
#if DFG_ENABLE(DEBUG_VERBOSE)
    printf("Slow case count for call at @%lu bc#%u: %u.\n", m_graph.size(), m_currentIndex, m_inlineStackTop->m_profiledBlock->rareCaseProfileForBytecodeOffset(m_currentIndex)->m_counter);
#endif
            
    if (m_graph.isFunctionConstant(m_codeBlock, callTarget))
        callType = ConstantFunction;
    else if (!!m_inlineStackTop->m_profiledBlock->getCallLinkInfo(m_currentIndex).lastSeenCallee && !m_inlineStackTop->m_profiledBlock->couldTakeSlowCase(m_currentIndex))
        callType = LinkedFunction;
    else
        callType = UnknownFunction;
    if (callType != UnknownFunction) {
        int argCount = currentInstruction[2].u.operand;
        int registerOffset = currentInstruction[3].u.operand;
        int firstArg = registerOffset - argCount - RegisterFile::CallFrameHeaderSize;
        int lastArg = firstArg + argCount - 1;
                
        // Do we have a result?
        bool usesResult = false;
        int resultOperand = 0; // make compiler happy
        unsigned nextOffset = m_currentIndex + OPCODE_LENGTH(op_call);
        Instruction* putInstruction = currentInstruction + OPCODE_LENGTH(op_call);
        PredictedType prediction = PredictNone;
        if (interpreter->getOpcodeID(putInstruction->u.opcode) == op_call_put_result) {
            resultOperand = putInstruction[1].u.operand;
            usesResult = true;
            prediction = getPrediction(m_graph.size(), nextOffset);
            nextOffset += OPCODE_LENGTH(op_call_put_result);
        }
        JSFunction* expectedFunction;
        DFG::Intrinsic intrinsic;
        bool certainAboutExpectedFunction;
        if (callType == ConstantFunction) {
            expectedFunction = m_graph.valueOfFunctionConstant(m_codeBlock, callTarget);
            intrinsic = expectedFunction->executable()->intrinsicFor(kind);
            certainAboutExpectedFunction = true;
        } else {
            ASSERT(callType == LinkedFunction);
            expectedFunction = m_inlineStackTop->m_profiledBlock->getCallLinkInfo(m_currentIndex).lastSeenCallee.get();
            intrinsic = expectedFunction->executable()->intrinsicFor(kind);
            certainAboutExpectedFunction = false;
        }
                
        if (intrinsic != NoIntrinsic) {
            if (!certainAboutExpectedFunction) {
                pinCell(expectedFunction);
                addToGraph(CheckFunction, OpInfo(expectedFunction), callTarget);
            }
            
            if (handleIntrinsic(usesResult, resultOperand, intrinsic, firstArg, lastArg, prediction)) {
                if (!certainAboutExpectedFunction) {
                    // Need to keep the call target alive for OSR. We could easily optimize this out if we wanted
                    // to, since at this point we know that the call target is a constant. It's just that OSR isn't
                    // smart enough to figure that out, since it doesn't understand CheckFunction.
                    addToGraph(Phantom, callTarget);
                }
                
                return;
            }
        } else if (handleInlining(usesResult, currentInstruction[1].u.operand, callTarget, resultOperand, certainAboutExpectedFunction, expectedFunction, firstArg, lastArg, nextOffset, kind))
            return;
    }
            
    addCall(interpreter, currentInstruction, op);
}

bool ByteCodeParser::handleInlining(bool usesResult, int callTarget, NodeIndex callTargetNodeIndex, int resultOperand, bool certainAboutExpectedFunction, JSFunction* expectedFunction, int firstArg, int lastArg, unsigned nextOffset, CodeSpecializationKind kind)
{
    // First, the really simple checks: do we have an actual JS function?
    if (!expectedFunction)
        return false;
    if (expectedFunction->isHostFunction())
        return false;
    
    FunctionExecutable* executable = expectedFunction->jsExecutable();
    
    // Does the number of arguments we're passing match the arity of the target? We could
    // inline arity check failures, but for simplicity we currently don't.
    if (static_cast<int>(executable->parameterCount()) + 1 != lastArg - firstArg + 1)
        return false;
    
    // Have we exceeded inline stack depth, or are we trying to inline a recursive call?
    // If either of these are detected, then don't inline.
    unsigned depth = 0;
    for (InlineStackEntry* entry = m_inlineStackTop; entry; entry = entry->m_caller) {
        ++depth;
        if (depth >= Heuristics::maximumInliningDepth)
            return false; // Depth exceeded.
        
        if (entry->executable() == executable)
            return false; // Recursion detected.
    }
    
    // Does the code block's size match the heuristics/requirements for being
    // an inline candidate?
    CodeBlock* profiledBlock = executable->profiledCodeBlockFor(kind);
    if (!mightInlineFunctionFor(profiledBlock, kind))
        return false;
    
    // If we get here then it looks like we should definitely inline this code. Proceed
    // with parsing the code to get bytecode, so that we can then parse the bytecode.
    CodeBlock* codeBlock = m_codeBlockCache.get(CodeBlockKey(executable, kind), expectedFunction->scope());
    if (!codeBlock)
        return false;
    
    ASSERT(canInlineFunctionFor(codeBlock, kind));

#if DFG_ENABLE(DEBUG_VERBOSE)
    printf("Inlining executable %p.\n", executable);
#endif
    
    // Now we know without a doubt that we are committed to inlining. So begin the process
    // by checking the callee (if necessary) and making sure that arguments and the callee
    // are flushed.
    if (!certainAboutExpectedFunction) {
        pinCell(expectedFunction);
        addToGraph(CheckFunction, OpInfo(expectedFunction), callTargetNodeIndex);
    }
    
    // FIXME: Don't flush constants!
    
    for (int arg = firstArg + 1; arg <= lastArg; ++arg)
        flush(arg);
    
    int inlineCallFrameStart = m_inlineStackTop->remapOperand(lastArg) + 1;
    
    // Make sure that the area used by the call frame is reserved.
    for (int arg = inlineCallFrameStart + RegisterFile::CallFrameHeaderSize + codeBlock->m_numVars; arg-- > inlineCallFrameStart + 1;)
        m_preservedVars.set(m_inlineStackTop->remapOperand(arg));
    
    // Make sure that we have enough locals.
    unsigned newNumLocals = inlineCallFrameStart + RegisterFile::CallFrameHeaderSize + codeBlock->m_numCalleeRegisters;
    if (newNumLocals > m_numLocals) {
        m_numLocals = newNumLocals;
        for (size_t i = 0; i < m_graph.m_blocks.size(); ++i)
            m_graph.m_blocks[i]->ensureLocals(newNumLocals);
    }

    InlineStackEntry inlineStackEntry(this, codeBlock, profiledBlock, m_graph.m_blocks.size() - 1, (VirtualRegister)m_inlineStackTop->remapOperand(callTarget), expectedFunction, (VirtualRegister)m_inlineStackTop->remapOperand(usesResult ? resultOperand : InvalidVirtualRegister), (VirtualRegister)inlineCallFrameStart, kind);
    
    // This is where the actual inlining really happens.
    unsigned oldIndex = m_currentIndex;
    m_currentIndex = 0;

    addToGraph(InlineStart);
    
    parseCodeBlock();
    
    m_currentIndex = oldIndex;
    
    // If the inlined code created some new basic blocks, then we have linking to do.
    if (inlineStackEntry.m_callsiteBlockHead != m_graph.m_blocks.size() - 1) {
        
        ASSERT(!inlineStackEntry.m_unlinkedBlocks.isEmpty());
        if (inlineStackEntry.m_callsiteBlockHeadNeedsLinking)
            linkBlock(m_graph.m_blocks[inlineStackEntry.m_callsiteBlockHead].get(), inlineStackEntry.m_blockLinkingTargets);
        else
            ASSERT(m_graph.m_blocks[inlineStackEntry.m_callsiteBlockHead]->isLinked);
        
        // It's possible that the callsite block head is not owned by the caller.
        if (!inlineStackEntry.m_caller->m_unlinkedBlocks.isEmpty()) {
            // It's definitely owned by the caller, because the caller created new blocks.
            // Assert that this all adds up.
            ASSERT(inlineStackEntry.m_caller->m_unlinkedBlocks.last().m_blockIndex == inlineStackEntry.m_callsiteBlockHead);
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
    
    // If there was a return, but no early returns, then we're done. We allow parsing of
    // the caller to continue in whatever basic block we're in right now.
    if (!inlineStackEntry.m_didEarlyReturn && inlineStackEntry.m_didReturn) {
        BasicBlock* lastBlock = m_graph.m_blocks.last().get();
        ASSERT(lastBlock->begin == lastBlock->end || !m_graph.last().isTerminal());
        
        // If we created new blocks then the last block needs linking, but in the
        // caller. It doesn't need to be linked to, but it needs outgoing links.
        if (!inlineStackEntry.m_unlinkedBlocks.isEmpty()) {
#if DFG_ENABLE(DEBUG_VERBOSE)
            printf("Reascribing bytecode index of block %p from bc#%u to bc#%u (inline return case).\n", lastBlock, lastBlock->bytecodeBegin, m_currentIndex);
#endif
            // For debugging purposes, set the bytecodeBegin. Note that this doesn't matter
            // for release builds because this block will never serve as a potential target
            // in the linker's binary search.
            lastBlock->bytecodeBegin = m_currentIndex;
            m_inlineStackTop->m_caller->m_unlinkedBlocks.append(UnlinkedBlock(m_graph.m_blocks.size() - 1));
        }
        
        m_currentBlock = m_graph.m_blocks.last().get();

#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Done inlining executable %p, continuing code generation at epilogue.\n", executable);
#endif
        return true;
    }
    
    // If we get to this point then all blocks must end in some sort of terminals.
    ASSERT(m_graph.last().isTerminal());
    
    // Link the early returns to the basic block we're about to create.
    for (size_t i = 0; i < inlineStackEntry.m_unlinkedBlocks.size(); ++i) {
        if (!inlineStackEntry.m_unlinkedBlocks[i].m_needsEarlyReturnLinking)
            continue;
        BasicBlock* block = m_graph.m_blocks[inlineStackEntry.m_unlinkedBlocks[i].m_blockIndex].get();
        ASSERT(!block->isLinked);
        Node& node = m_graph[block->end - 1];
        ASSERT(node.op == Jump);
        ASSERT(node.takenBlockIndex() == NoBlock);
        node.setTakenBlockIndex(m_graph.m_blocks.size());
        inlineStackEntry.m_unlinkedBlocks[i].m_needsEarlyReturnLinking = false;
#if !ASSERT_DISABLED
        block->isLinked = true;
#endif
    }
    
    // Need to create a new basic block for the continuation at the caller.
    OwnPtr<BasicBlock> block = adoptPtr(new BasicBlock(nextOffset, m_graph.size(), m_numArguments, m_numLocals));
#if DFG_ENABLE(DEBUG_VERBOSE)
    printf("Creating inline epilogue basic block %p, #%lu for %p bc#%u at inline depth %u.\n", block.get(), m_graph.m_blocks.size(), m_inlineStackTop->executable(), m_currentIndex, CodeOrigin::inlineDepthForCallFrame(m_inlineStackTop->m_inlineCallFrame));
#endif
    m_currentBlock = block.get();
    ASSERT(m_inlineStackTop->m_caller->m_blockLinkingTargets.isEmpty() || m_graph.m_blocks[m_inlineStackTop->m_caller->m_blockLinkingTargets.last()]->bytecodeBegin < nextOffset);
    m_inlineStackTop->m_caller->m_unlinkedBlocks.append(UnlinkedBlock(m_graph.m_blocks.size()));
    m_inlineStackTop->m_caller->m_blockLinkingTargets.append(m_graph.m_blocks.size());
    m_graph.m_blocks.append(block.release());
    prepareToParseBlock();
    
    // At this point we return and continue to generate code for the caller, but
    // in the new basic block.
#if DFG_ENABLE(DEBUG_VERBOSE)
    printf("Done inlining executable %p, continuing code generation in new block.\n", executable);
#endif
    return true;
}

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

bool ByteCodeParser::handleIntrinsic(bool usesResult, int resultOperand, Intrinsic intrinsic, int firstArg, int lastArg, PredictedType prediction)
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
        if (absArg > lastArg) {
            set(resultOperand, constantNaN());
            return true;
        }

        if (!MacroAssembler::supportsFloatingPointAbs())
            return false;

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
        
        if (!MacroAssembler::supportsFloatingPointSqrt())
            return false;
        
        set(resultOperand, addToGraph(ArithSqrt, getToNumber(firstArg + 1)));
        return true;
    }
        
    case ArrayPushIntrinsic: {
        if (firstArg + 1 != lastArg)
            return false;
        
        NodeIndex arrayPush = addToGraph(ArrayPush, OpInfo(0), OpInfo(prediction), get(firstArg), get(firstArg + 1));
        if (usesResult)
            set(resultOperand, arrayPush);
        
        return true;
    }
        
    case ArrayPopIntrinsic: {
        if (firstArg != lastArg)
            return false;
        
        NodeIndex arrayPop = addToGraph(ArrayPop, OpInfo(0), OpInfo(prediction), get(firstArg));
        if (usesResult)
            set(resultOperand, arrayPop);
        return true;
    }

    case CharCodeAtIntrinsic: {
        if (firstArg + 1 != lastArg)
            return false;

        NodeIndex charCode = addToGraph(StringCharCodeAt, get(firstArg), getToInt32(firstArg + 1));
        if (usesResult)
            set(resultOperand, charCode);
        return true;
    }

    case CharAtIntrinsic: {
        if (firstArg + 1 != lastArg)
            return false;
        
        NodeIndex charCode = addToGraph(StringCharAt, get(firstArg), getToInt32(firstArg + 1));
        if (usesResult)
            set(resultOperand, charCode);
        return true;
    }

    default:
        ASSERT(intrinsic == NoIntrinsic);
        return false;
    }
}

void ByteCodeParser::prepareToParseBlock()
{
    for (unsigned i = 0; i < m_constants.size(); ++i)
        m_constants[i] = ConstantRecord();
    m_cellConstantNodes.clear();
}

bool ByteCodeParser::parseBlock(unsigned limit)
{
    bool shouldContinueParsing = true;
    
    // If we are the first basic block, introduce markers for arguments. This allows
    // us to track if a use of an argument may use the actual argument passed, as
    // opposed to using a value we set explicitly.
    if (m_currentBlock == m_graph.m_blocks[0].get() && !m_inlineStackTop->m_inlineCallFrame) {
        m_graph.m_arguments.resize(m_numArguments);
        for (unsigned argument = 0; argument < m_numArguments; ++argument) {
            NodeIndex setArgument = addToGraph(SetArgument, OpInfo(newVariableAccessData(argument - m_codeBlock->m_numParameters - RegisterFile::CallFrameHeaderSize)));
            m_graph.m_arguments[argument] = setArgument;
            m_currentBlock->variablesAtHead.setArgumentFirstTime(argument, setArgument);
            m_currentBlock->variablesAtTail.setArgumentFirstTime(argument, setArgument);
        }
    }

    Interpreter* interpreter = m_globalData->interpreter;
    Instruction* instructionsBegin = m_inlineStackTop->m_codeBlock->instructions().begin();
    unsigned blockBegin = m_currentIndex;
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
            if (m_currentBlock->begin != m_graph.size())
                addToGraph(Jump, OpInfo(m_currentIndex));
            else {
#if DFG_ENABLE(DEBUG_VERBOSE)
                printf("Refusing to plant jump at limit %u because block %p is empty.\n", limit, m_currentBlock);
#endif
            }
            return shouldContinueParsing;
        }
        
        // Switch on the current bytecode opcode.
        Instruction* currentInstruction = instructionsBegin + m_currentIndex;
        OpcodeID opcodeID = interpreter->getOpcodeID(currentInstruction->u.opcode);
        switch (opcodeID) {

        // === Function entry opcodes ===

        case op_enter:
            // Initialize all locals to undefined.
            for (int i = 0; i < m_inlineStackTop->m_codeBlock->m_numVars; ++i)
                set(i, constantUndefined());
            NEXT_OPCODE(op_enter);

        case op_convert_this: {
            NodeIndex op1 = getThis();
            if (m_graph[op1].op == ConvertThis)
                setThis(op1);
            else
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
            if (m_inlineStackTop->m_inlineCallFrame)
                set(currentInstruction[1].u.operand, getDirect(m_inlineStackTop->m_calleeVR));
            else
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
            ASSERT(result != srcDst); // Required for assumptions we make during OSR.
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
            unsigned identifier = m_inlineStackTop->m_identifierRemap[getInstruction[3].u.operand];
                
            // Check if the method_check was monomorphic. If so, emit a CheckXYZMethod
            // node, which is a lot more efficient.
            StructureStubInfo& stubInfo = m_inlineStackTop->m_profiledBlock->getStubInfo(m_currentIndex);
            MethodCallLinkInfo& methodCall = m_inlineStackTop->m_profiledBlock->getMethodCallLinkInfo(m_currentIndex);
            
            if (methodCall.seen && !!methodCall.cachedStructure && !stubInfo.seen) {
                // It's monomorphic as far as we can tell, since the method_check was linked
                // but the slow path (i.e. the normal get_by_id) never fired.

                pinCell(methodCall.cachedStructure.get()); // first check
                pinCell(methodCall.cachedPrototype.get()); // second check
                pinCell(methodCall.cachedPrototypeStructure.get()); // second check
                pinCell(methodCall.cachedFunction.get()); // result
                
                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(methodCall.cachedStructure.get())), base);
                if (methodCall.cachedPrototype.get() != m_inlineStackTop->m_profiledBlock->globalObject()->methodCallDummy())
                    addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(methodCall.cachedPrototypeStructure.get())), cellConstant(methodCall.cachedPrototype.get()));
                
                set(getInstruction[1].u.operand, cellConstant(methodCall.cachedFunction.get()));
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
            PredictedType prediction = getPredictionWithoutOSRExit();
            
            NodeIndex base = get(currentInstruction[2].u.operand);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[3].u.operand];
            
            Identifier identifier = m_codeBlock->identifier(identifierNumber);
            StructureStubInfo& stubInfo = m_inlineStackTop->m_profiledBlock->getStubInfo(m_currentIndex);
            
            size_t offset = notFound;
            StructureSet structureSet;
            if (stubInfo.seen && !m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex)) {
                switch (stubInfo.accessType) {
                case access_get_by_id_self: {
                    Structure* structure = stubInfo.u.getByIdSelf.baseObjectStructure.get();
                    offset = structure->get(*m_globalData, identifier);
                    
                    if (offset != notFound)
                        structureSet.add(structure);

                    if (offset != notFound)
                        ASSERT(structureSet.size());
                    break;
                }
                    
                case access_get_by_id_self_list: {
                    PolymorphicAccessStructureList* list = stubInfo.u.getByIdProtoList.structureList;
                    unsigned size = stubInfo.u.getByIdProtoList.listSize;
                    for (unsigned i = 0; i < size; ++i) {
                        if (!list->list[i].isDirect) {
                            offset = notFound;
                            break;
                        }
                        
                        Structure* structure = list->list[i].base.get();
                        if (structureSet.contains(structure))
                            continue;
                        
                        size_t myOffset = structure->get(*m_globalData, identifier);
                    
                        if (myOffset == notFound) {
                            offset = notFound;
                            break;
                        }
                    
                        if (!i)
                            offset = myOffset;
                        else if (offset != myOffset) {
                            offset = notFound;
                            break;
                        }
                    
                        structureSet.add(structure);
                    }
                    
                    if (offset != notFound)
                        ASSERT(structureSet.size());
                    break;
                }
                    
                default:
                    ASSERT(offset == notFound);
                    break;
                }
            }
                        
            if (offset != notFound) {
                ASSERT(structureSet.size());
                
                // The implementation of GetByOffset does not know to terminate speculative
                // execution if it doesn't have a prediction, so we do it manually.
                if (prediction == PredictNone)
                    addToGraph(ForceOSRExit);
                
                for (unsigned i = 0; i < structureSet.size(); ++i)
                    pinCell(structureSet[i]);
                
                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(structureSet)), base);
                set(currentInstruction[1].u.operand, addToGraph(GetByOffset, OpInfo(m_graph.m_storageAccessData.size()), OpInfo(prediction), addToGraph(GetPropertyStorage, base)));
                
                StorageAccessData storageAccessData;
                storageAccessData.offset = offset;
                storageAccessData.identifierNumber = identifierNumber;
                m_graph.m_storageAccessData.append(storageAccessData);
            } else
                set(currentInstruction[1].u.operand, addToGraph(GetById, OpInfo(identifierNumber), OpInfo(prediction), base));

            NEXT_OPCODE(op_get_by_id);
        }

        case op_put_by_id: {
            NodeIndex value = get(currentInstruction[3].u.operand);
            NodeIndex base = get(currentInstruction[1].u.operand);
            unsigned identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[2].u.operand];
            bool direct = currentInstruction[8].u.operand;

            StructureStubInfo& stubInfo = m_inlineStackTop->m_profiledBlock->getStubInfo(m_currentIndex);
            if (!stubInfo.seen)
                addToGraph(ForceOSRExit);
            
            bool alreadyGenerated = false;
            
            if (stubInfo.seen && !m_inlineStackTop->m_profiledBlock->likelyToTakeSlowCase(m_currentIndex)) {
                switch (stubInfo.accessType) {
                case access_put_by_id_replace: {
                    Structure* structure = stubInfo.u.putByIdReplace.baseObjectStructure.get();
                    Identifier identifier = m_codeBlock->identifier(identifierNumber);
                    size_t offset = structure->get(*m_globalData, identifier);
                    
                    if (offset != notFound) {
                        pinCell(structure);
                        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(structure)), base);
                        addToGraph(PutByOffset, OpInfo(m_graph.m_storageAccessData.size()), base, addToGraph(GetPropertyStorage, base), value);
                        
                        StorageAccessData storageAccessData;
                        storageAccessData.offset = offset;
                        storageAccessData.identifierNumber = identifierNumber;
                        m_graph.m_storageAccessData.append(storageAccessData);
                        
                        alreadyGenerated = true;
                    }
                    break;
                }
                    
                case access_put_by_id_transition: {
                    Structure* previousStructure = stubInfo.u.putByIdTransition.previousStructure.get();
                    Structure* newStructure = stubInfo.u.putByIdTransition.structure.get();
                    
                    if (previousStructure->propertyStorageCapacity() != newStructure->propertyStorageCapacity())
                        break;
                    
                    StructureChain* structureChain = stubInfo.u.putByIdTransition.chain.get();
                    
                    Identifier identifier = m_codeBlock->identifier(identifierNumber);
                    size_t offset = newStructure->get(*m_globalData, identifier);
                    
                    if (offset != notFound && structureChainIsStillValid(direct, previousStructure, structureChain)) {
                        pinCell(previousStructure);
                        pinCell(newStructure);
                        addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(previousStructure)), base);
                        if (!direct) {
                            if (!previousStructure->storedPrototype().isNull()) {
                                pinCell(previousStructure->storedPrototype().asCell()->structure());
                                pinCell(previousStructure->storedPrototype().asCell());
                                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(previousStructure->storedPrototype().asCell()->structure())), cellConstant(previousStructure->storedPrototype().asCell()));
                            }
                            
                            for (WriteBarrier<Structure>* it = structureChain->head(); *it; ++it) {
                                JSValue prototype = (*it)->storedPrototype();
                                if (prototype.isNull())
                                    continue;
                                ASSERT(prototype.isCell());
                                pinCell(prototype.asCell());
                                pinCell(prototype.asCell()->structure());
                                addToGraph(CheckStructure, OpInfo(m_graph.addStructureSet(prototype.asCell()->structure())), cellConstant(prototype.asCell()));
                            }
                        }
                        addToGraph(PutStructure, OpInfo(m_graph.addStructureTransitionData(StructureTransitionData(previousStructure, newStructure))), base);
                        
                        addToGraph(PutByOffset, OpInfo(m_graph.m_storageAccessData.size()), base, addToGraph(GetPropertyStorage, base), value);
                        
                        StorageAccessData storageAccessData;
                        storageAccessData.offset = offset;
                        storageAccessData.identifierNumber = identifierNumber;
                        m_graph.m_storageAccessData.append(storageAccessData);
                        
                        alreadyGenerated = true;
                    }
                    break;
                }
                    
                default:
                    break;
                }
            }
            
            if (!alreadyGenerated) {
                if (direct)
                    addToGraph(PutByIdDirect, OpInfo(identifierNumber), base, value);
                else
                    addToGraph(PutById, OpInfo(identifierNumber), base, value);
            }

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
            if (m_inlineStackTop->m_inlineCallFrame) {
                if (m_inlineStackTop->m_returnValue != InvalidVirtualRegister)
                    setDirect(m_inlineStackTop->m_returnValue, get(currentInstruction[1].u.operand));
                m_inlineStackTop->m_didReturn = true;
                if (m_inlineStackTop->m_unlinkedBlocks.isEmpty()) {
                    // If we're returning from the first block, then we're done parsing.
                    ASSERT(m_inlineStackTop->m_callsiteBlockHead == m_graph.m_blocks.size() - 1);
                    shouldContinueParsing = false;
                    LAST_OPCODE(op_ret);
                } else {
                    // If inlining created blocks, and we're doing a return, then we need some
                    // special linking.
                    ASSERT(m_inlineStackTop->m_unlinkedBlocks.last().m_blockIndex == m_graph.m_blocks.size() - 1);
                    m_inlineStackTop->m_unlinkedBlocks.last().m_needsNormalLinking = false;
                }
                if (m_currentIndex + OPCODE_LENGTH(op_ret) != m_inlineStackTop->m_codeBlock->instructions().size() || m_inlineStackTop->m_didEarlyReturn) {
                    ASSERT(m_currentIndex + OPCODE_LENGTH(op_ret) <= m_inlineStackTop->m_codeBlock->instructions().size());
                    addToGraph(Jump, OpInfo(NoBlock));
                    m_inlineStackTop->m_unlinkedBlocks.last().m_needsEarlyReturnLinking = true;
                    m_inlineStackTop->m_didEarlyReturn = true;
                }
                LAST_OPCODE(op_ret);
            }
            addToGraph(Return, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_ret);
            
        case op_end:
            ASSERT(!m_inlineStackTop->m_inlineCallFrame);
            addToGraph(Return, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_end);

        case op_throw:
            addToGraph(Throw, get(currentInstruction[1].u.operand));
            LAST_OPCODE(op_throw);
            
        case op_throw_reference_error:
            addToGraph(ThrowReferenceError);
            LAST_OPCODE(op_throw_reference_error);
            
        case op_call:
            handleCall(interpreter, currentInstruction, Call, CodeForCall);
            NEXT_OPCODE(op_call);
            
        case op_construct:
            handleCall(interpreter, currentInstruction, Construct, CodeForConstruct);
            NEXT_OPCODE(op_construct);
            
        case op_call_put_result:
            NEXT_OPCODE(op_call_put_result);

        case op_resolve: {
            PredictedType prediction = getPrediction();
            
            unsigned identifier = m_inlineStackTop->m_identifierRemap[currentInstruction[2].u.operand];

            NodeIndex resolve = addToGraph(Resolve, OpInfo(identifier), OpInfo(prediction));
            set(currentInstruction[1].u.operand, resolve);

            NEXT_OPCODE(op_resolve);
        }

        case op_resolve_base: {
            PredictedType prediction = getPrediction();
            
            unsigned identifier = m_inlineStackTop->m_identifierRemap[currentInstruction[2].u.operand];

            NodeIndex resolve = addToGraph(currentInstruction[3].u.operand ? ResolveBaseStrictPut : ResolveBase, OpInfo(identifier), OpInfo(prediction));
            set(currentInstruction[1].u.operand, resolve);

            NEXT_OPCODE(op_resolve_base);
        }
            
        case op_resolve_global: {
            PredictedType prediction = getPrediction();
            
            NodeIndex resolve = addToGraph(ResolveGlobal, OpInfo(m_graph.m_resolveGlobalData.size()), OpInfo(prediction));
            m_graph.m_resolveGlobalData.append(ResolveGlobalData());
            ResolveGlobalData& data = m_graph.m_resolveGlobalData.last();
            data.identifierNumber = m_inlineStackTop->m_identifierRemap[currentInstruction[2].u.operand];
            data.resolveInfoIndex = m_globalResolveNumber++;
            set(currentInstruction[1].u.operand, resolve);

            NEXT_OPCODE(op_resolve_global);
        }

        case op_loop_hint: {
            // Baseline->DFG OSR jumps between loop hints. The DFG assumes that Baseline->DFG
            // OSR can only happen at basic block boundaries. Assert that these two statements
            // are compatible.
            ASSERT_UNUSED(blockBegin, m_currentIndex == blockBegin);
            
            // We never do OSR into an inlined code block. That could not happen, since OSR
            // looks up the code block that is the replacement for the baseline JIT code
            // block. Hence, machine code block = true code block = not inline code block.
            if (!m_inlineStackTop->m_caller)
                m_currentBlock->isOSRTarget = true;
            
            // Emit a phantom node to ensure that there is a placeholder node for this bytecode
            // op.
            addToGraph(Phantom);
            
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
        VariableAccessData* dataForPhi = m_graph[entry.m_phi].variableAccessData();

        for (size_t i = 0; i < predecessors.size(); ++i) {
            BasicBlock* predecessorBlock = m_graph.m_blocks[predecessors[i]].get();

            NodeIndex& var = (stackType == ArgumentPhiStack) ? predecessorBlock->variablesAtTail.argument(varNo) : predecessorBlock->variablesAtTail.local(varNo);

            NodeIndex valueInPredecessor = var;
            if (valueInPredecessor == NoNode) {
                valueInPredecessor = addToGraph(Phi, OpInfo(newVariableAccessData(stackType == ArgumentPhiStack ? varNo - m_codeBlock->m_numParameters - RegisterFile::CallFrameHeaderSize : varNo)));
                var = valueInPredecessor;
                if (stackType == ArgumentPhiStack)
                    predecessorBlock->variablesAtHead.setArgumentFirstTime(varNo, valueInPredecessor);
                else
                    predecessorBlock->variablesAtHead.setLocalFirstTime(varNo, valueInPredecessor);
                phiStack.append(PhiStackEntry(predecessorBlock, valueInPredecessor, varNo));
            } else if (m_graph[valueInPredecessor].op == GetLocal) {
                // We want to ensure that the VariableAccessDatas are identical between the
                // GetLocal and its block-local Phi. Strictly speaking we only need the two
                // to be unified. But for efficiency, we want the code that creates GetLocals
                // and Phis to try to reuse VariableAccessDatas as much as possible.
                ASSERT(m_graph[valueInPredecessor].variableAccessData() == m_graph[m_graph[valueInPredecessor].child1()].variableAccessData());
                
                valueInPredecessor = m_graph[valueInPredecessor].child1();
            }
            ASSERT(m_graph[valueInPredecessor].op == SetLocal || m_graph[valueInPredecessor].op == Phi || m_graph[valueInPredecessor].op == Flush || (m_graph[valueInPredecessor].op == SetArgument && stackType == ArgumentPhiStack));
            
            VariableAccessData* dataForPredecessor = m_graph[valueInPredecessor].variableAccessData();
            
            dataForPredecessor->unify(dataForPhi);

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
            
            NodeIndex newPhi = addToGraph(Phi, OpInfo(dataForPhi));
            
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

void ByteCodeParser::linkBlock(BasicBlock* block, Vector<BlockIndex>& possibleTargets)
{
    ASSERT(block->end != NoNode);
    ASSERT(!block->isLinked);
    ASSERT(block->end > block->begin);
    Node& node = m_graph[block->end - 1];
    ASSERT(node.isTerminal());
    
    switch (node.op) {
    case Jump:
        node.setTakenBlockIndex(m_graph.blockIndexForBytecodeOffset(possibleTargets, node.takenBytecodeOffsetDuringParsing()));
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Linked basic block %p to %p, #%u.\n", block, m_graph.m_blocks[node.takenBlockIndex()].get(), node.takenBlockIndex());
#endif
        break;
        
    case Branch:
        node.setTakenBlockIndex(m_graph.blockIndexForBytecodeOffset(possibleTargets, node.takenBytecodeOffsetDuringParsing()));
        node.setNotTakenBlockIndex(m_graph.blockIndexForBytecodeOffset(possibleTargets, node.notTakenBytecodeOffsetDuringParsing()));
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Linked basic block %p to %p, #%u and %p, #%u.\n", block, m_graph.m_blocks[node.takenBlockIndex()].get(), node.takenBlockIndex(), m_graph.m_blocks[node.notTakenBlockIndex()].get(), node.notTakenBlockIndex());
#endif
        break;
        
    default:
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Marking basic block %p as linked.\n", block);
#endif
        break;
    }
    
#if !ASSERT_DISABLED
    block->isLinked = true;
#endif
}

void ByteCodeParser::linkBlocks(Vector<UnlinkedBlock>& unlinkedBlocks, Vector<BlockIndex>& possibleTargets)
{
    for (size_t i = 0; i < unlinkedBlocks.size(); ++i) {
        if (unlinkedBlocks[i].m_needsNormalLinking) {
            linkBlock(m_graph.m_blocks[unlinkedBlocks[i].m_blockIndex].get(), possibleTargets);
            unlinkedBlocks[i].m_needsNormalLinking = false;
        }
    }
}

void ByteCodeParser::handleSuccessor(Vector<BlockIndex, 16>& worklist, BlockIndex blockIndex, BlockIndex successorIndex)
{
    BasicBlock* successor = m_graph.m_blocks[successorIndex].get();
    if (!successor->isReachable) {
        successor->isReachable = true;
        worklist.append(successorIndex);
    }
    
    successor->m_predecessors.append(blockIndex);
}

void ByteCodeParser::determineReachability()
{
    Vector<BlockIndex, 16> worklist;
    worklist.append(0);
    m_graph.m_blocks[0]->isReachable = true;
    while (!worklist.isEmpty()) {
        BlockIndex index = worklist.last();
        worklist.removeLast();
        
        BasicBlock* block = m_graph.m_blocks[index].get();
        ASSERT(block->isLinked);
        
        Node& node = m_graph[block->end - 1];
        ASSERT(node.isTerminal());
        
        if (node.isJump())
            handleSuccessor(worklist, index, node.takenBlockIndex());
        else if (node.isBranch()) {
            handleSuccessor(worklist, index, node.takenBlockIndex());
            handleSuccessor(worklist, index, node.notTakenBlockIndex());
        }
    }
}

void ByteCodeParser::buildOperandMapsIfNecessary()
{
    if (m_haveBuiltOperandMaps)
        return;
    
    for (size_t i = 0; i < m_codeBlock->numberOfIdentifiers(); ++i)
        m_identifierMap.add(m_codeBlock->identifier(i).impl(), i);
    for (size_t i = 0; i < m_codeBlock->numberOfConstantRegisters(); ++i)
        m_jsValueMap.add(JSValue::encode(m_codeBlock->getConstant(i + FirstConstantRegisterIndex)), i + FirstConstantRegisterIndex);
    
    m_haveBuiltOperandMaps = true;
}

ByteCodeParser::InlineStackEntry::InlineStackEntry(ByteCodeParser* byteCodeParser, CodeBlock* codeBlock, CodeBlock* profiledBlock, BlockIndex callsiteBlockHead, VirtualRegister calleeVR, JSFunction* callee, VirtualRegister returnValueVR, VirtualRegister inlineCallFrameStart, CodeSpecializationKind kind)
    : m_byteCodeParser(byteCodeParser)
    , m_codeBlock(codeBlock)
    , m_profiledBlock(profiledBlock)
    , m_calleeVR(calleeVR)
    , m_callsiteBlockHead(callsiteBlockHead)
    , m_returnValue(returnValueVR)
    , m_didReturn(false)
    , m_didEarlyReturn(false)
    , m_caller(byteCodeParser->m_inlineStackTop)
{
    if (m_caller) {
        // Inline case.
        ASSERT(codeBlock != byteCodeParser->m_codeBlock);
        ASSERT(callee);
        ASSERT(calleeVR != InvalidVirtualRegister);
        ASSERT(inlineCallFrameStart != InvalidVirtualRegister);
        ASSERT(callsiteBlockHead != NoBlock);
        
        InlineCallFrame inlineCallFrame;
        inlineCallFrame.executable.set(*byteCodeParser->m_globalData, byteCodeParser->m_codeBlock->ownerExecutable(), codeBlock->ownerExecutable());
        inlineCallFrame.stackOffset = inlineCallFrameStart + RegisterFile::CallFrameHeaderSize;
        inlineCallFrame.callee.set(*byteCodeParser->m_globalData, byteCodeParser->m_codeBlock->ownerExecutable(), callee);
        inlineCallFrame.caller = byteCodeParser->currentCodeOrigin();
        inlineCallFrame.arguments.resize(codeBlock->m_numParameters); // Set the number of arguments including this, but don't configure the value recoveries, yet.
        inlineCallFrame.isCall = isCall(kind);
        byteCodeParser->m_codeBlock->inlineCallFrames().append(inlineCallFrame);
        m_inlineCallFrame = &byteCodeParser->m_codeBlock->inlineCallFrames().last();
        
        byteCodeParser->buildOperandMapsIfNecessary();
        
        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        m_constantRemap.resize(codeBlock->numberOfConstantRegisters());

        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i) {
            StringImpl* rep = codeBlock->identifier(i).impl();
            pair<IdentifierMap::iterator, bool> result = byteCodeParser->m_identifierMap.add(rep, byteCodeParser->m_codeBlock->numberOfIdentifiers());
            if (result.second)
                byteCodeParser->m_codeBlock->addIdentifier(Identifier(byteCodeParser->m_globalData, rep));
            m_identifierRemap[i] = result.first->second;
        }
        for (size_t i = 0; i < codeBlock->numberOfConstantRegisters(); ++i) {
            JSValue value = codeBlock->getConstant(i + FirstConstantRegisterIndex);
            pair<JSValueMap::iterator, bool> result = byteCodeParser->m_jsValueMap.add(JSValue::encode(value), byteCodeParser->m_codeBlock->numberOfConstantRegisters() + FirstConstantRegisterIndex);
            if (result.second) {
                byteCodeParser->m_codeBlock->addConstant(value);
                byteCodeParser->m_constants.append(ConstantRecord());
            }
            m_constantRemap[i] = result.first->second;
        }
        
        m_callsiteBlockHeadNeedsLinking = true;
    } else {
        // Machine code block case.
        ASSERT(codeBlock == byteCodeParser->m_codeBlock);
        ASSERT(!callee);
        ASSERT(calleeVR == InvalidVirtualRegister);
        ASSERT(returnValueVR == InvalidVirtualRegister);
        ASSERT(inlineCallFrameStart == InvalidVirtualRegister);
        ASSERT(callsiteBlockHead == NoBlock);

        m_inlineCallFrame = 0;

        m_identifierRemap.resize(codeBlock->numberOfIdentifiers());
        m_constantRemap.resize(codeBlock->numberOfConstantRegisters());

        for (size_t i = 0; i < codeBlock->numberOfIdentifiers(); ++i)
            m_identifierRemap[i] = i;
        for (size_t i = 0; i < codeBlock->numberOfConstantRegisters(); ++i)
            m_constantRemap[i] = i + FirstConstantRegisterIndex;

        m_callsiteBlockHeadNeedsLinking = false;
    }
    
    for (size_t i = 0; i < m_constantRemap.size(); ++i)
        ASSERT(m_constantRemap[i] >= static_cast<unsigned>(FirstConstantRegisterIndex));
    
    byteCodeParser->m_inlineStackTop = this;
}

void ByteCodeParser::parseCodeBlock()
{
    CodeBlock* codeBlock = m_inlineStackTop->m_codeBlock;
    
    for (unsigned jumpTargetIndex = 0; jumpTargetIndex <= codeBlock->numberOfJumpTargets(); ++jumpTargetIndex) {
        // The maximum bytecode offset to go into the current basicblock is either the next jump target, or the end of the instructions.
        unsigned limit = jumpTargetIndex < codeBlock->numberOfJumpTargets() ? codeBlock->jumpTarget(jumpTargetIndex) : codeBlock->instructions().size();
#if DFG_ENABLE(DEBUG_VERBOSE)
        printf("Parsing bytecode with limit %p bc#%u at inline depth %u.\n", m_inlineStackTop->executable(), limit, CodeOrigin::inlineDepthForCallFrame(m_inlineStackTop->m_inlineCallFrame));
#endif
        ASSERT(m_currentIndex < limit);

        // Loop until we reach the current limit (i.e. next jump target).
        do {
            if (!m_currentBlock) {
                // Check if we can use the last block.
                if (!m_graph.m_blocks.isEmpty() && m_graph.m_blocks.last()->begin == m_graph.m_blocks.last()->end) {
                    // This must be a block belonging to us.
                    ASSERT(m_inlineStackTop->m_unlinkedBlocks.last().m_blockIndex == m_graph.m_blocks.size() - 1);
                    // Either the block is linkable or it isn't. If it's linkable then it's the last
                    // block in the blockLinkingTargets list. If it's not then the last block will
                    // have a lower bytecode index that the one we're about to give to this block.
                    if (m_inlineStackTop->m_blockLinkingTargets.isEmpty() || m_inlineStackTop->m_blockLinkingTargets.last() != m_currentIndex) {
                        // Make the block linkable.
                        ASSERT(m_inlineStackTop->m_blockLinkingTargets.isEmpty() || m_inlineStackTop->m_blockLinkingTargets.last() < m_currentIndex);
                        m_inlineStackTop->m_blockLinkingTargets.append(m_graph.m_blocks.size() - 1);
                    }
                    // Change its bytecode begin and continue.
                    m_currentBlock = m_graph.m_blocks.last().get();
#if DFG_ENABLE(DEBUG_VERBOSE)
                    printf("Reascribing bytecode index of block %p from bc#%u to bc#%u (peephole case).\n", m_currentBlock, m_currentBlock->bytecodeBegin, m_currentIndex);
#endif
                    m_currentBlock->bytecodeBegin = m_currentIndex;
                } else {
                    OwnPtr<BasicBlock> block = adoptPtr(new BasicBlock(m_currentIndex, m_graph.size(), m_numArguments, m_numLocals));
#if DFG_ENABLE(DEBUG_VERBOSE)
                    printf("Creating basic block %p, #%lu for %p bc#%u at inline depth %u.\n", block.get(), m_graph.m_blocks.size(), m_inlineStackTop->executable(), m_currentIndex, CodeOrigin::inlineDepthForCallFrame(m_inlineStackTop->m_inlineCallFrame));
#endif
                    m_currentBlock = block.get();
                    ASSERT(m_inlineStackTop->m_unlinkedBlocks.isEmpty() || m_graph.m_blocks[m_inlineStackTop->m_unlinkedBlocks.last().m_blockIndex]->bytecodeBegin < m_currentIndex);
                    m_inlineStackTop->m_unlinkedBlocks.append(UnlinkedBlock(m_graph.m_blocks.size()));
                    m_inlineStackTop->m_blockLinkingTargets.append(m_graph.m_blocks.size());
                    m_graph.m_blocks.append(block.release());
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
            ASSERT(m_currentBlock->begin == m_graph.size() || m_graph.last().isTerminal() || (m_currentIndex == codeBlock->instructions().size() && m_inlineStackTop->m_inlineCallFrame) || !shouldContinueParsing);

            m_currentBlock->end = m_graph.size();
            
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
    
    InlineStackEntry inlineStackEntry(this, m_codeBlock, m_profiledBlock, NoBlock, InvalidVirtualRegister, 0, InvalidVirtualRegister, InvalidVirtualRegister, CodeForCall);
    
    parseCodeBlock();

    linkBlocks(inlineStackEntry.m_unlinkedBlocks, inlineStackEntry.m_blockLinkingTargets);
    determineReachability();
    processPhiStack<LocalPhiStack>();
    processPhiStack<ArgumentPhiStack>();
    
    m_graph.m_preservedVars = m_preservedVars;
    m_graph.m_localVars = m_numLocals;
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
