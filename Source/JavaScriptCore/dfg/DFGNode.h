/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef DFGNode_h
#define DFGNode_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "CodeOrigin.h"
#include "DFGAdjacencyList.h"
#include "DFGCommon.h"
#include "DFGNodeFlags.h"
#include "DFGNodeType.h"
#include "DFGVariableAccessData.h"
#include "JSValue.h"
#include "Operands.h"
#include "PredictedType.h"
#include "ValueProfile.h"

namespace JSC { namespace DFG {

struct StructureTransitionData {
    Structure* previousStructure;
    Structure* newStructure;
    
    StructureTransitionData() { }
    
    StructureTransitionData(Structure* previousStructure, Structure* newStructure)
        : previousStructure(previousStructure)
        , newStructure(newStructure)
    {
    }
};

// This type used in passing an immediate argument to Node constructor;
// distinguishes an immediate value (typically an index into a CodeBlock data structure - 
// a constant index, argument, or identifier) from a NodeIndex.
struct OpInfo {
    explicit OpInfo(int32_t value) : m_value(static_cast<uintptr_t>(value)) { }
    explicit OpInfo(uint32_t value) : m_value(static_cast<uintptr_t>(value)) { }
#if OS(DARWIN) || USE(JSVALUE64)
    explicit OpInfo(size_t value) : m_value(static_cast<uintptr_t>(value)) { }
#endif
    explicit OpInfo(void* value) : m_value(reinterpret_cast<uintptr_t>(value)) { }
    uintptr_t m_value;
};

// === Node ===
//
// Node represents a single operation in the data flow graph.
struct Node {
    enum VarArgTag { VarArg };

    // Construct a node with up to 3 children, no immediate value.
    Node(NodeType op, CodeOrigin codeOrigin, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : codeOrigin(codeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_prediction(PredictNone)
    {
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }

    // Construct a node with up to 3 children and an immediate value.
    Node(NodeType op, CodeOrigin codeOrigin, OpInfo imm, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : codeOrigin(codeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm.m_value)
        , m_prediction(PredictNone)
    {
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }

    // Construct a node with up to 3 children and two immediate values.
    Node(NodeType op, CodeOrigin codeOrigin, OpInfo imm1, OpInfo imm2, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : codeOrigin(codeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(safeCast<unsigned>(imm2.m_value))
        , m_prediction(PredictNone)
    {
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }
    
    // Construct a node with a variable number of children and two immediate values.
    Node(VarArgTag, NodeType op, CodeOrigin codeOrigin, OpInfo imm1, OpInfo imm2, unsigned firstChild, unsigned numChildren)
        : codeOrigin(codeOrigin)
        , children(AdjacencyList::Variable, firstChild, numChildren)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(safeCast<unsigned>(imm2.m_value))
        , m_prediction(PredictNone)
    {
        setOpAndDefaultFlags(op);
        ASSERT(m_flags & NodeHasVarArgs);
    }
    
    NodeType op() const { return static_cast<NodeType>(m_op); }
    NodeFlags flags() const { return m_flags; }
    
    void setOp(NodeType op)
    {
        m_op = op;
    }
    
    void setFlags(NodeFlags flags)
    {
        m_flags = flags;
    }
    
    bool mergeFlags(NodeFlags flags)
    {
        NodeFlags newFlags = m_flags | flags;
        if (newFlags == m_flags)
            return false;
        m_flags = newFlags;
        return true;
    }
    
    bool filterFlags(NodeFlags flags)
    {
        NodeFlags newFlags = m_flags & flags;
        if (newFlags == m_flags)
            return false;
        m_flags = newFlags;
        return true;
    }
    
    bool clearFlags(NodeFlags flags)
    {
        return filterFlags(~flags);
    }
    
    void setOpAndDefaultFlags(NodeType op)
    {
        m_op = op;
        m_flags = defaultFlags(op);
    }

    bool mustGenerate()
    {
        return m_flags & NodeMustGenerate;
    }

    bool isConstant()
    {
        return op() == JSConstant;
    }
    
    bool isWeakConstant()
    {
        return op() == WeakJSConstant;
    }
    
    bool hasConstant()
    {
        return isConstant() || isWeakConstant();
    }

    unsigned constantNumber()
    {
        ASSERT(isConstant());
        return m_opInfo;
    }
    
    JSCell* weakConstant()
    {
        return bitwise_cast<JSCell*>(m_opInfo);
    }
    
    JSValue valueOfJSConstant(CodeBlock* codeBlock)
    {
        if (op() == WeakJSConstant)
            return JSValue(weakConstant());
        return codeBlock->constantRegister(FirstConstantRegisterIndex + constantNumber()).get();
    }

    bool isInt32Constant(CodeBlock* codeBlock)
    {
        return isConstant() && valueOfJSConstant(codeBlock).isInt32();
    }
    
    bool isDoubleConstant(CodeBlock* codeBlock)
    {
        bool result = isConstant() && valueOfJSConstant(codeBlock).isDouble();
        if (result)
            ASSERT(!isInt32Constant(codeBlock));
        return result;
    }
    
    bool isNumberConstant(CodeBlock* codeBlock)
    {
        bool result = isConstant() && valueOfJSConstant(codeBlock).isNumber();
        ASSERT(result == (isInt32Constant(codeBlock) || isDoubleConstant(codeBlock)));
        return result;
    }
    
    bool isBooleanConstant(CodeBlock* codeBlock)
    {
        return isConstant() && valueOfJSConstant(codeBlock).isBoolean();
    }
    
    bool hasVariableAccessData()
    {
        switch (op()) {
        case GetLocal:
        case SetLocal:
        case Phi:
        case SetArgument:
        case Flush:
            return true;
        default:
            return false;
        }
    }
    
    bool hasLocal()
    {
        return hasVariableAccessData();
    }
    
    VariableAccessData* variableAccessData()
    {
        ASSERT(hasVariableAccessData());
        return reinterpret_cast<VariableAccessData*>(m_opInfo)->find();
    }
    
    VirtualRegister local()
    {
        return variableAccessData()->local();
    }
    
    bool hasIdentifier()
    {
        switch (op()) {
        case GetById:
        case GetByIdFlush:
        case PutById:
        case PutByIdDirect:
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
            return true;
        default:
            return false;
        }
    }

    unsigned identifierNumber()
    {
        ASSERT(hasIdentifier());
        return m_opInfo;
    }
    
    unsigned resolveGlobalDataIndex()
    {
        ASSERT(op() == ResolveGlobal);
        return m_opInfo;
    }

    bool hasArithNodeFlags()
    {
        switch (op()) {
        case UInt32ToNumber:
        case ArithAdd:
        case ArithSub:
        case ArithNegate:
        case ArithMul:
        case ArithAbs:
        case ArithMin:
        case ArithMax:
        case ArithMod:
        case ArithDiv:
        case ValueAdd:
            return true;
        default:
            return false;
        }
    }
    
    // This corrects the arithmetic node flags, so that irrelevant bits are
    // ignored. In particular, anything other than ArithMul does not need
    // to know if it can speculate on negative zero.
    NodeFlags arithNodeFlags()
    {
        NodeFlags result = m_flags;
        if (op() == ArithMul || op() == ArithDiv || op() == ArithMod)
            return result;
        return result & ~NodeNeedsNegZero;
    }
    
    bool hasConstantBuffer()
    {
        return op() == NewArrayBuffer;
    }
    
    unsigned startConstant()
    {
        ASSERT(hasConstantBuffer());
        return m_opInfo;
    }
    
    unsigned numConstants()
    {
        ASSERT(hasConstantBuffer());
        return m_opInfo2;
    }
    
    bool hasRegexpIndex()
    {
        return op() == NewRegexp;
    }
    
    unsigned regexpIndex()
    {
        ASSERT(hasRegexpIndex());
        return m_opInfo;
    }
    
    bool hasVarNumber()
    {
        return op() == GetGlobalVar || op() == PutGlobalVar || op() == GetScopedVar || op() == PutScopedVar;
    }

    unsigned varNumber()
    {
        ASSERT(hasVarNumber());
        return m_opInfo;
    }

    bool hasScopeChainDepth()
    {
        return op() == GetScopeChain;
    }
    
    unsigned scopeChainDepth()
    {
        ASSERT(hasScopeChainDepth());
        return m_opInfo;
    }

    bool hasResult()
    {
        return m_flags & NodeResultMask;
    }

    bool hasInt32Result()
    {
        return (m_flags & NodeResultMask) == NodeResultInt32;
    }
    
    bool hasNumberResult()
    {
        return (m_flags & NodeResultMask) == NodeResultNumber;
    }
    
    bool hasJSResult()
    {
        return (m_flags & NodeResultMask) == NodeResultJS;
    }
    
    bool hasBooleanResult()
    {
        return (m_flags & NodeResultMask) == NodeResultBoolean;
    }

    bool isJump()
    {
        return op() == Jump;
    }

    bool isBranch()
    {
        return op() == Branch;
    }

    bool isTerminal()
    {
        switch (op()) {
        case Jump:
        case Branch:
        case Return:
        case Throw:
        case ThrowReferenceError:
            return true;
        default:
            return false;
        }
    }

    unsigned takenBytecodeOffsetDuringParsing()
    {
        ASSERT(isBranch() || isJump());
        return m_opInfo;
    }

    unsigned notTakenBytecodeOffsetDuringParsing()
    {
        ASSERT(isBranch());
        return m_opInfo2;
    }
    
    void setTakenBlockIndex(BlockIndex blockIndex)
    {
        ASSERT(isBranch() || isJump());
        m_opInfo = blockIndex;
    }
    
    void setNotTakenBlockIndex(BlockIndex blockIndex)
    {
        ASSERT(isBranch());
        m_opInfo2 = blockIndex;
    }
    
    BlockIndex takenBlockIndex()
    {
        ASSERT(isBranch() || isJump());
        return m_opInfo;
    }
    
    BlockIndex notTakenBlockIndex()
    {
        ASSERT(isBranch());
        return m_opInfo2;
    }
    
    bool hasHeapPrediction()
    {
        switch (op()) {
        case GetById:
        case GetByIdFlush:
        case GetByVal:
        case Call:
        case Construct:
        case GetByOffset:
        case GetScopedVar:
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
        case ResolveGlobal:
        case ArrayPop:
        case ArrayPush:
        case RegExpExec:
        case RegExpTest:
        case GetGlobalVar:
            return true;
        default:
            return false;
        }
    }
    
    PredictedType getHeapPrediction()
    {
        ASSERT(hasHeapPrediction());
        return static_cast<PredictedType>(m_opInfo2);
    }
    
    bool predictHeap(PredictedType prediction)
    {
        ASSERT(hasHeapPrediction());
        
        return mergePrediction(m_opInfo2, prediction);
    }
    
    bool hasFunctionCheckData()
    {
        return op() == CheckFunction;
    }

    JSFunction* function()
    {
        ASSERT(hasFunctionCheckData());
        return reinterpret_cast<JSFunction*>(m_opInfo);
    }

    bool hasStructureTransitionData()
    {
        return op() == PutStructure;
    }
    
    StructureTransitionData& structureTransitionData()
    {
        ASSERT(hasStructureTransitionData());
        return *reinterpret_cast<StructureTransitionData*>(m_opInfo);
    }
    
    bool hasStructureSet()
    {
        return op() == CheckStructure;
    }
    
    StructureSet& structureSet()
    {
        ASSERT(hasStructureSet());
        return *reinterpret_cast<StructureSet*>(m_opInfo);
    }
    
    bool hasStorageAccessData()
    {
        return op() == GetByOffset || op() == PutByOffset;
    }
    
    unsigned storageAccessDataIndex()
    {
        ASSERT(hasStorageAccessData());
        return m_opInfo;
    }
    
    bool hasFunctionDeclIndex()
    {
        return op() == NewFunction
            || op() == NewFunctionNoCheck;
    }
    
    unsigned functionDeclIndex()
    {
        ASSERT(hasFunctionDeclIndex());
        return m_opInfo;
    }
    
    bool hasFunctionExprIndex()
    {
        return op() == NewFunctionExpression;
    }
    
    unsigned functionExprIndex()
    {
        ASSERT(hasFunctionExprIndex());
        return m_opInfo;
    }
    
    bool hasVirtualRegister()
    {
        return m_virtualRegister != InvalidVirtualRegister;
    }
    
    VirtualRegister virtualRegister()
    {
        ASSERT(hasResult());
        ASSERT(m_virtualRegister != InvalidVirtualRegister);
        return m_virtualRegister;
    }

    void setVirtualRegister(VirtualRegister virtualRegister)
    {
        ASSERT(hasResult());
        ASSERT(m_virtualRegister == InvalidVirtualRegister);
        m_virtualRegister = virtualRegister;
    }

    bool shouldGenerate()
    {
        return m_refCount;
    }

    unsigned refCount()
    {
        return m_refCount;
    }

    // returns true when ref count passes from 0 to 1.
    bool ref()
    {
        return !m_refCount++;
    }

    unsigned adjustedRefCount()
    {
        return mustGenerate() ? m_refCount - 1 : m_refCount;
    }
    
    void setRefCount(unsigned refCount)
    {
        m_refCount = refCount;
    }
    
    // Derefs the node and returns true if the ref count reached zero.
    // In general you don't want to use this directly; use Graph::deref
    // instead.
    bool deref()
    {
        ASSERT(m_refCount);
        return !--m_refCount;
    }
    
    Edge child1()
    {
        ASSERT(!(m_flags & NodeHasVarArgs));
        return children.child1();
    }
    
    // This is useful if you want to do a fast check on the first child
    // before also doing a check on the opcode. Use this with care and
    // avoid it if possible.
    Edge child1Unchecked()
    {
        return children.child1Unchecked();
    }

    Edge child2()
    {
        ASSERT(!(m_flags & NodeHasVarArgs));
        return children.child2();
    }

    Edge child3()
    {
        ASSERT(!(m_flags & NodeHasVarArgs));
        return children.child3();
    }
    
    unsigned firstChild()
    {
        ASSERT(m_flags & NodeHasVarArgs);
        return children.firstChild();
    }
    
    unsigned numChildren()
    {
        ASSERT(m_flags & NodeHasVarArgs);
        return children.numChildren();
    }
    
    PredictedType prediction()
    {
        return m_prediction;
    }
    
    bool predict(PredictedType prediction)
    {
        return mergePrediction(m_prediction, prediction);
    }
    
    bool shouldSpeculateInteger()
    {
        return isInt32Prediction(prediction());
    }
    
    bool shouldSpeculateDouble()
    {
        return isDoublePrediction(prediction());
    }
    
    bool shouldSpeculateNumber()
    {
        return isNumberPrediction(prediction());
    }
    
    bool shouldSpeculateBoolean()
    {
        return isBooleanPrediction(prediction());
    }
    
    bool shouldSpeculateFinalObject()
    {
        return isFinalObjectPrediction(prediction());
    }
    
    bool shouldSpeculateFinalObjectOrOther()
    {
        return isFinalObjectOrOtherPrediction(prediction());
    }
    
    bool shouldSpeculateArray()
    {
        return isArrayPrediction(prediction());
    }
    
    bool shouldSpeculateInt8Array()
    {
        return isInt8ArrayPrediction(prediction());
    }
    
    bool shouldSpeculateInt16Array()
    {
        return isInt16ArrayPrediction(prediction());
    }
    
    bool shouldSpeculateInt32Array()
    {
        return isInt32ArrayPrediction(prediction());
    }
    
    bool shouldSpeculateUint8Array()
    {
        return isUint8ArrayPrediction(prediction());
    }

    bool shouldSpeculateUint8ClampedArray()
    {
        return isUint8ClampedArrayPrediction(prediction());
    }
    
    bool shouldSpeculateUint16Array()
    {
        return isUint16ArrayPrediction(prediction());
    }
    
    bool shouldSpeculateUint32Array()
    {
        return isUint32ArrayPrediction(prediction());
    }
    
    bool shouldSpeculateFloat32Array()
    {
        return isFloat32ArrayPrediction(prediction());
    }
    
    bool shouldSpeculateFloat64Array()
    {
        return isFloat64ArrayPrediction(prediction());
    }
    
    bool shouldSpeculateArrayOrOther()
    {
        return isArrayOrOtherPrediction(prediction());
    }
    
    bool shouldSpeculateObject()
    {
        return isObjectPrediction(prediction());
    }
    
    bool shouldSpeculateCell()
    {
        return isCellPrediction(prediction());
    }
    
    static bool shouldSpeculateInteger(Node& op1, Node& op2)
    {
        return op1.shouldSpeculateInteger() && op2.shouldSpeculateInteger();
    }
    
    static bool shouldSpeculateNumber(Node& op1, Node& op2)
    {
        return op1.shouldSpeculateNumber() && op2.shouldSpeculateNumber();
    }
    
    static bool shouldSpeculateFinalObject(Node& op1, Node& op2)
    {
        return op1.shouldSpeculateFinalObject() && op2.shouldSpeculateFinalObject();
    }

    static bool shouldSpeculateArray(Node& op1, Node& op2)
    {
        return op1.shouldSpeculateArray() && op2.shouldSpeculateArray();
    }
    
    bool canSpeculateInteger()
    {
        return nodeCanSpeculateInteger(arithNodeFlags());
    }
    
    void dumpChildren(FILE* out)
    {
        if (!child1())
            return;
        fprintf(out, "@%u", child1().index());
        if (!child2())
            return;
        fprintf(out, ", @%u", child2().index());
        if (!child3())
            return;
        fprintf(out, ", @%u", child3().index());
    }
    
    // Used to look up exception handling information (currently implemented as a bytecode index).
    CodeOrigin codeOrigin;
    // References to up to 3 children, or links to a variable length set of children.
    AdjacencyList children;

private:
    uint16_t m_op; // real type is NodeType
    NodeFlags m_flags;
    // The virtual register number (spill location) associated with this .
    VirtualRegister m_virtualRegister;
    // The number of uses of the result of this operation (+1 for 'must generate' nodes, which have side-effects).
    unsigned m_refCount;
    // Immediate values, accesses type-checked via accessors above. The first one is
    // big enough to store a pointer.
    uintptr_t m_opInfo;
    unsigned m_opInfo2;
    // The prediction ascribed to this node after propagation.
    PredictedType m_prediction;
};

} } // namespace JSC::DFG

#endif
#endif
