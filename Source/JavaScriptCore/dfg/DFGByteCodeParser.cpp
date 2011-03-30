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

#include "DFGAliasTracker.h"
#include "DFGScoreBoard.h"
#include "CodeBlock.h"

namespace JSC { namespace DFG {

// === ByteCodeParser ===
//
// This class is used to compile the dataflow graph from a CodeBlock.
class ByteCodeParser {
public:
    ByteCodeParser(JSGlobalData* globalData, CodeBlock* codeBlock, Graph& graph)
        : m_globalData(globalData)
        , m_codeBlock(codeBlock)
        , m_graph(graph)
        , m_currentIndex(0)
        , m_noArithmetic(true)
        , m_constantUndefined(UINT_MAX)
        , m_constant1(UINT_MAX)
    {
        unsigned numberOfConstants = codeBlock->numberOfConstantRegisters();
        m_constantRecords.grow(numberOfConstants);

        unsigned numberOfParameters = codeBlock->m_numParameters;
        m_arguments.grow(numberOfParameters);
        for (unsigned i = 0; i < numberOfParameters; ++i)
            m_arguments[i] = NoNode;

        unsigned numberOfRegisters = codeBlock->m_numCalleeRegisters;
        m_calleeRegisters.grow(numberOfRegisters);
        for (unsigned i = 0; i < numberOfRegisters; ++i)
            m_calleeRegisters[i] = NoNode;
    }

    bool parse();

private:
    // Get/Set the operands/result of a bytecode instruction.
    NodeIndex get(int operand)
    {
        // Is this a constant?
        if (operand >= FirstConstantRegisterIndex) {
            unsigned constant = operand - FirstConstantRegisterIndex;
            ASSERT(constant < m_constantRecords.size());
            return getJSConstant(constant);
        }

        // Is this an argument?
        if (operand < 0) {
            unsigned argument = operand + m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize;
            ASSERT(argument < m_arguments.size());
            return getArgument(argument);
        }

        // Must be a local or temporary.
        ASSERT((unsigned)operand < m_calleeRegisters.size());
        return getRegister((unsigned)operand);
    }
    void set(int operand, NodeIndex value)
    {
        // Is this an argument?
        if (operand < 0) {
            unsigned argument = operand + m_codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize;
            ASSERT(argument < m_arguments.size());
            return setArgument(argument, value);
        }

        // Must be a local or temporary.
        ASSERT((unsigned)operand < m_calleeRegisters.size());
        return setRegister((unsigned)operand, value);
    }

    // Used in implementing get/set, above, where the operand is a local or temporary.
    NodeIndex getRegister(unsigned operand)
    {
        NodeIndex index = m_calleeRegisters[operand];
        if (index != NoNode)
            return index;
        // We have not yet seen a definition for this value in this block.
        // For now, since we are only generating single block functions,
        // this value must be undefined.
        // For example:
        //     function f() { var x; return x; }
        return constantUndefined();
    }
    void setRegister(int operand, NodeIndex value)
    {
        m_calleeRegisters[operand] = value;
    }

    // Used in implementing get/set, above, where the operand is an argument.
    NodeIndex getArgument(unsigned argument)
    {
        NodeIndex index = m_arguments[argument];
        if (index != NoNode)
            return index;
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(Argument, m_currentIndex, OpInfo(argument)));
        return m_arguments[argument] = resultIndex;
    }
    void setArgument(int operand, NodeIndex value)
    {
        m_arguments[operand] = value;
    }

    // Get an operand, and perform a ToInt32/ToNumber conversion on it.
    NodeIndex getToInt32(int operand)
    {
        // Avoid wastefully adding a JSConstant node to the graph, only to
        // replace it with a Int32Constant (which is what would happen if
        // we called 'toInt32(get(operand))' in this case).
        if (operand >= FirstConstantRegisterIndex) {
            JSValue v = m_codeBlock->getConstant(operand);
            if (v.isInt32())
                return getInt32Constant(v.asInt32(), operand - FirstConstantRegisterIndex);
        }
        return toInt32(get(operand));
    }
    NodeIndex getToNumber(int operand)
    {
        // Avoid wastefully adding a JSConstant node to the graph, only to
        // replace it with a DoubleConstant (which is what would happen if
        // we called 'toNumber(get(operand))' in this case).
        if (operand >= FirstConstantRegisterIndex) {
            JSValue v = m_codeBlock->getConstant(operand);
            if (v.isNumber())
                return getDoubleConstant(v.uncheckedGetNumber(), operand - FirstConstantRegisterIndex);
        }
        return toNumber(get(operand));
    }

    // Perform an ES5 ToInt32 operation - returns a node of type NodeResultInt32.
    NodeIndex toInt32(NodeIndex index)
    {
        Node& node = m_graph[index];

        if (node.hasInt32Result())
            return index;

        if (node.hasDoubleResult()) {
            if (node.op == DoubleConstant)
                return getInt32Constant(JSC::toInt32(valueOfDoubleConstant(index)), node.constantNumber());
            // 'NumberToInt32(Int32ToNumber(X))' == X, and 'NumberToInt32(UInt32ToNumber(X)) == X'
            if (node.op == Int32ToNumber || node.op == UInt32ToNumber)
                return node.child1;

            // We unique NumberToInt32 nodes in a map to prevent duplicate conversions.
            pair<UnaryOpMap::iterator, bool> result = m_numberToInt32Nodes.add(index, NoNode);
            // Either we added a new value, or the existing value in the map is non-zero.
            ASSERT(result.second == (result.first->second == NoNode));
            if (result.second)
                result.first->second = addToGraph(NumberToInt32, index);
            return result.first->second;
        }

        // Check for numeric constants boxed as JSValues.
        if (node.op == JSConstant) {
            JSValue v = valueOfJSConstant(index);
            if (v.isInt32())
                return getInt32Constant(v.asInt32(), node.constantNumber());
            if (v.isNumber())
                return getInt32Constant(JSC::toInt32(v.uncheckedGetNumber()), node.constantNumber());
        }

        return addToGraph(ValueToInt32, index);
    }

    // Perform an ES5 ToNumber operation - returns a node of type NodeResultDouble.
    NodeIndex toNumber(NodeIndex index)
    {
        Node& node = m_graph[index];

        if (node.hasDoubleResult())
            return index;

        if (node.hasInt32Result()) {
            if (node.op == Int32Constant)
                return getDoubleConstant(valueOfInt32Constant(index), node.constantNumber());

            // We unique Int32ToNumber nodes in a map to prevent duplicate conversions.
            pair<UnaryOpMap::iterator, bool> result = m_int32ToNumberNodes.add(index, NoNode);
            // Either we added a new value, or the existing value in the map is non-zero.
            ASSERT(result.second == (result.first->second == NoNode));
            if (result.second)
                result.first->second = addToGraph(Int32ToNumber, index);
            return result.first->second;
        }

        if (node.op == JSConstant) {
            JSValue v = valueOfJSConstant(index);
            if (v.isNumber())
                return getDoubleConstant(v.uncheckedGetNumber(), node.constantNumber());
        }

        return addToGraph(ValueToNumber, index);
    }


    // Used in implementing get, above, where the operand is a constant.
    NodeIndex getInt32Constant(int32_t value, unsigned constant)
    {
        NodeIndex index = m_constantRecords[constant].asInt32;
        if (index != NoNode)
            return index;
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(Int32Constant, m_currentIndex, OpInfo(constant)));
        m_graph[resultIndex].setInt32Constant(value);
        m_constantRecords[constant].asInt32 = resultIndex;
        return resultIndex;
    }
    NodeIndex getDoubleConstant(double value, unsigned constant)
    {
        NodeIndex index = m_constantRecords[constant].asNumeric;
        if (index != NoNode)
            return index;
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(DoubleConstant, m_currentIndex, OpInfo(constant)));
        m_graph[resultIndex].setDoubleConstant(value);
        m_constantRecords[constant].asNumeric = resultIndex;
        return resultIndex;
    }
    NodeIndex getJSConstant(unsigned constant)
    {
        NodeIndex index = m_constantRecords[constant].asJSValue;
        if (index != NoNode)
            return index;

        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(JSConstant, m_currentIndex, OpInfo(constant)));
        m_constantRecords[constant].asJSValue = resultIndex;
        return resultIndex;
    }

    // Helper functions to get/set the this value.
    NodeIndex getThis()
    {
        return getArgument(0);
    }
    void setThis(NodeIndex value)
    {
        setArgument(0, value);
    }

    // Convenience methods for checking nodes for constants.
    bool isInt32Constant(NodeIndex index)
    {
        return m_graph[index].op == Int32Constant;
    }
    bool isDoubleConstant(NodeIndex index)
    {
        return m_graph[index].op == DoubleConstant;
    }
    bool isJSConstant(NodeIndex index)
    {
        return m_graph[index].op == JSConstant;
    }

    // Convenience methods for getting constant values.
    int32_t valueOfInt32Constant(NodeIndex index)
    {
        ASSERT(isInt32Constant(index));
        return m_graph[index].int32Constant();
    }
    double valueOfDoubleConstant(NodeIndex index)
    {
        ASSERT(isDoubleConstant(index));
        return m_graph[index].numericConstant();
    }
    JSValue valueOfJSConstant(NodeIndex index)
    {
        ASSERT(isJSConstant(index));
        return m_codeBlock->getConstant(FirstConstantRegisterIndex + m_graph[index].constantNumber());
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

            // Add undefined to the CodeBlock's constants, and add a corresponding slot in m_constantRecords.
            ASSERT(m_constantRecords.size() == numberOfConstants);
            m_codeBlock->addConstant(jsUndefined());
            m_constantRecords.append(ConstantRecord());
            ASSERT(m_constantRecords.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constantUndefined must refer to an entry in the CodeBlock's constant pool that has the value 'undefined'.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constantUndefined).isUndefined());
        return getJSConstant(m_constantUndefined);
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
                    return getDoubleConstant(1, m_constant1);
            }

            // Add the value 1 to the CodeBlock's constants, and add a corresponding slot in m_constantRecords.
            ASSERT(m_constantRecords.size() == numberOfConstants);
            m_codeBlock->addConstant(jsNumber(1));
            m_constantRecords.append(ConstantRecord());
            ASSERT(m_constantRecords.size() == m_codeBlock->numberOfConstantRegisters());
        }

        // m_constant1 must refer to an entry in the CodeBlock's constant pool that has the integer value 1.
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constant1).isInt32());
        ASSERT(m_codeBlock->getConstant(FirstConstantRegisterIndex + m_constant1).asInt32() == 1);
        return getDoubleConstant(1, m_constant1);
    }


    // These methods create a node and add it to the graph. If nodes of this type are
    // 'mustGenerate' then the node  will implicitly be ref'ed to ensure generation.
    NodeIndex addToGraph(NodeType op, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
    {
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(op, m_currentIndex, child1, child2, child3));

        if (op & NodeMustGenerate)
            m_graph.ref(resultIndex);
        return resultIndex;
    }
    NodeIndex addToGraph(NodeType op, OpInfo info, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
    {
        NodeIndex resultIndex = (NodeIndex)m_graph.size();
        m_graph.append(Node(op, m_currentIndex, info, child1, child2, child3));

        if (op & NodeMustGenerate)
            m_graph.ref(resultIndex);
        return resultIndex;
    }

    JSGlobalData* m_globalData;
    CodeBlock* m_codeBlock;
    Graph& m_graph;

    // The bytecode index of the current instruction being generated.
    unsigned m_currentIndex;

    // FIXME: used to temporarily disable arithmetic, until we fix associated performance regressions.
    bool m_noArithmetic;

    // We use these values during code generation, and to avoid the need for
    // special handling we make sure they are available as constants in the
    // CodeBlock's constant pool. These variables are initialized to
    // UINT_MAX, and lazily updated to hold an index into the CodeBlock's
    // constant pool, as necessary.
    unsigned m_constantUndefined;
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
    Vector <ConstantRecord, 32> m_constantRecords;

    // Track the index of the node whose result is the current value for every
    // register value in the bytecode - argument, local, and temporary.
    Vector <NodeIndex, 32> m_arguments;
    Vector <NodeIndex, 32> m_calleeRegisters;

    // These maps are used to unique ToNumber and ToInt32 operations.
    typedef HashMap<NodeIndex, NodeIndex> UnaryOpMap;
    UnaryOpMap m_int32ToNumberNodes;
    UnaryOpMap m_numberToInt32Nodes;
};

#define NEXT_OPCODE(name) \
    m_currentIndex += OPCODE_LENGTH(name); \
    continue;

bool ByteCodeParser::parse()
{
    AliasTracker aliases(m_graph);

    Interpreter* interpreter = m_globalData->interpreter;
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    while (true) {
        // Switch on the current bytecode opcode.
        Instruction* currentInstruction = instructionsBegin + m_currentIndex;
        switch (interpreter->getOpcodeID(currentInstruction->u.opcode)) {

        // === Function entry opcodes ===

        case op_enter:
            // This is a no-op for now - may need to initialize locals, if
            // DCE analysis cannot determine that the values are never read.
            NEXT_OPCODE(op_enter);

        case op_convert_this: {
            NodeIndex op1 = getThis();
            setThis(addToGraph(ConvertThis, op1));
            NEXT_OPCODE(op_convert_this);
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
                    result = addToGraph(UInt32ToNumber, op1);
            }  else {
                // Cannot optimize at this stage; shift & potentially rebox as a double.
                result = addToGraph(BitURShift, op1, op2);
                result = addToGraph(UInt32ToNumber, result);
            }
            set(currentInstruction[1].u.operand, result);
            NEXT_OPCODE(op_urshift);
        }

        // === Increment/Decrement opcodes ===

        case op_pre_inc: {
            unsigned srcDst = currentInstruction[1].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(srcDst, addToGraph(ArithAdd, op, one()));
            NEXT_OPCODE(op_pre_inc);
        }

        case op_post_inc: {
            unsigned result = currentInstruction[1].u.operand;
            unsigned srcDst = currentInstruction[2].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(result, op);
            set(srcDst, addToGraph(ArithAdd, op, one()));
            NEXT_OPCODE(op_post_inc);
        }

        case op_pre_dec: {
            unsigned srcDst = currentInstruction[1].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(srcDst, addToGraph(ArithSub, op, one()));
            NEXT_OPCODE(op_pre_dec);
        }

        case op_post_dec: {
            unsigned result = currentInstruction[1].u.operand;
            unsigned srcDst = currentInstruction[2].u.operand;
            NodeIndex op = getToNumber(srcDst);
            set(result, op);
            set(srcDst, addToGraph(ArithSub, op, one()));
            NEXT_OPCODE(op_post_dec);
        }

        // === Arithmetic operations ===

        case op_add: {
            m_noArithmetic = false;
            NodeIndex op1 = get(currentInstruction[2].u.operand);
            NodeIndex op2 = get(currentInstruction[3].u.operand);
            // If both operands can statically be determined to the numbers, then this is an arithmetic add.
            // Otherwise, we must assume this may be performing a concatenation to a string.
            if (m_graph[op1].hasNumericResult() && m_graph[op2].hasNumericResult())
                set(currentInstruction[1].u.operand, addToGraph(ArithAdd, toNumber(op1), toNumber(op2)));
            else
                set(currentInstruction[1].u.operand, addToGraph(ValueAdd, op1, op2));
            NEXT_OPCODE(op_add);
        }

        case op_sub: {
            m_noArithmetic = false;
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(ArithSub, op1, op2));
            NEXT_OPCODE(op_sub);
        }

        case op_mul: {
            m_noArithmetic = false;
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(ArithMul, op1, op2));
            NEXT_OPCODE(op_mul);
        }

        case op_mod: {
            m_noArithmetic = false;
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(ArithMod, op1, op2));
            NEXT_OPCODE(op_mod);
        }

        case op_div: {
            m_noArithmetic = false;
            NodeIndex op1 = getToNumber(currentInstruction[2].u.operand);
            NodeIndex op2 = getToNumber(currentInstruction[3].u.operand);
            set(currentInstruction[1].u.operand, addToGraph(ArithDiv, op1, op2));
            NEXT_OPCODE(op_div);
        }

        // === Misc operations ===

        case op_mov: {
            NodeIndex op = get(currentInstruction[2].u.operand);
            set(currentInstruction[1].u.operand, op);
            NEXT_OPCODE(op_mov);
        }

        // === Property access operations ===

        case op_get_by_val: {
            NodeIndex base = get(currentInstruction[2].u.operand);
            NodeIndex property = get(currentInstruction[3].u.operand);

            NodeIndex getByVal = addToGraph(GetByVal, base, property, aliases.lookupGetByVal(base, property));
            set(currentInstruction[1].u.operand, getByVal);
            aliases.recordGetByVal(getByVal);

            NEXT_OPCODE(op_get_by_val);
        };

        case op_put_by_val: {
            NodeIndex base = get(currentInstruction[1].u.operand);
            NodeIndex property = get(currentInstruction[2].u.operand);
            NodeIndex value = get(currentInstruction[3].u.operand);

            NodeIndex aliasedGet = aliases.lookupGetByVal(base, property);
            NodeIndex putByVal = addToGraph(aliasedGet != NoNode ? PutByValAlias : PutByVal, base, property, value);
            aliases.recordPutByVal(putByVal);

            NEXT_OPCODE(op_put_by_val);
        };

        case op_get_by_id: {
            NodeIndex base = get(currentInstruction[2].u.operand);
            unsigned identifier = currentInstruction[3].u.operand;

            NodeIndex getById = addToGraph(GetById, OpInfo(identifier), base);
            set(currentInstruction[1].u.operand, getById);
            aliases.recordGetById(getById);

            NEXT_OPCODE(op_get_by_id);
        }

        case op_put_by_id: {
            NodeIndex value = get(currentInstruction[3].u.operand);
            NodeIndex base = get(currentInstruction[1].u.operand);
            unsigned identifier = currentInstruction[2].u.operand;
            bool direct = currentInstruction[8].u.operand;

            if (direct) {
                NodeIndex putByIdDirect = addToGraph(PutByIdDirect, OpInfo(identifier), base, value);
                aliases.recordPutByIdDirect(putByIdDirect);
            } else {
                NodeIndex putById = addToGraph(PutById, OpInfo(identifier), base, value);
                aliases.recordPutById(putById);
            }

            NEXT_OPCODE(op_put_by_id);
        }

        case op_get_global_var: {
            NodeIndex getGlobalVar = addToGraph(GetGlobalVar, OpInfo(currentInstruction[2].u.operand));
            set(currentInstruction[1].u.operand, getGlobalVar);
            NEXT_OPCODE(op_get_global_var);
        }

        case op_put_global_var: {
            NodeIndex value = get(currentInstruction[2].u.operand);
            addToGraph(PutGlobalVar, OpInfo(currentInstruction[1].u.operand), value);
            NEXT_OPCODE(op_put_global_var);
        }

        // === Block terminators. ===

        case op_ret: {
            addToGraph(Return, get(currentInstruction[1].u.operand));
            m_currentIndex += OPCODE_LENGTH(op_ret);
#if ENABLE(DFG_JIT_RESTRICTIONS)
            // FIXME: temporarily disabling the DFG JIT for functions containing arithmetic.
            return m_noArithmetic;
#else
            return true;
#endif
        }

        default:
            // parse failed!
            return false;
        }
    }
}

bool parse(Graph& graph, JSGlobalData* globalData, CodeBlock* codeBlock)
{
    // Call ByteCodeParser::parse to build the dataflow for the basic block at 'startIndex'.
    ByteCodeParser state(globalData, codeBlock, graph);
    if (!state.parse())
        return false;

    // Assign VirtualRegisters.
    ScoreBoard scoreBoard(graph);
    Node* nodes = graph.begin();
    size_t size = graph.size();
    for (size_t i = 0; i < size; ++i) {
        Node& node = nodes[i];
        if (node.refCount) {
            // First, call use on all of the current node's children, then
            // allocate a VirtualRegister for this node. We do so in this
            // order so that if a child is on its last use, and a
            // VirtualRegister is freed, then it may be reused for node.
            scoreBoard.use(node.child1);
            scoreBoard.use(node.child2);
            scoreBoard.use(node.child3);
            node.virtualRegister = scoreBoard.allocate();
            // 'mustGenerate' nodes have their useCount artificially elevated,
            // call use now to account for this.
            if (node.mustGenerate())
                scoreBoard.use(i);
        }
    }

    // 'm_numCalleeRegisters' is the number of locals and temporaries allocated
    // for the function (and checked for on entry). Since we perform a new and
    // different allocation of temporaries, more registers may now be required.
    if ((unsigned)codeBlock->m_numCalleeRegisters < scoreBoard.allocatedCount())
        codeBlock->m_numCalleeRegisters = scoreBoard.allocatedCount();

#if DFG_DEBUG_VERBOSE
    graph.dump(codeBlock);
#endif
    return true;
}

} } // namespace JSC::DFG

#endif
