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

#ifndef DFGNode_h
#define DFGNode_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "CodeOrigin.h"
#include "DFGCommon.h"
#include "DFGOperands.h"
#include "DFGVariableAccessData.h"
#include "JSValue.h"
#include "PredictedType.h"
#include "ValueProfile.h"
#include <wtf/BoundsCheckedPointer.h>
#include <wtf/Vector.h>

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

typedef unsigned ArithNodeFlags;
#define NodeUseBottom      0x00
#define NodeUsedAsNumber   0x01
#define NodeNeedsNegZero   0x02
#define NodeUsedAsMask     0x03
#define NodeMayOverflow    0x04
#define NodeMayNegZero     0x08
#define NodeBehaviorMask   0x0c

static inline bool nodeUsedAsNumber(ArithNodeFlags flags)
{
    return !!(flags & NodeUsedAsNumber);
}

static inline bool nodeCanTruncateInteger(ArithNodeFlags flags)
{
    return !nodeUsedAsNumber(flags);
}

static inline bool nodeCanIgnoreNegativeZero(ArithNodeFlags flags)
{
    return !(flags & NodeNeedsNegZero);
}

static inline bool nodeCanSpeculateInteger(ArithNodeFlags flags)
{
    if (flags & NodeMayOverflow)
        return !nodeUsedAsNumber(flags);
    
    if (flags & NodeMayNegZero)
        return nodeCanIgnoreNegativeZero(flags);
    
    return true;
}

#ifndef NDEBUG
static inline const char* arithNodeFlagsAsString(ArithNodeFlags flags)
{
    if (!flags)
        return "<empty>";

    static const int size = 64;
    static char description[size];
    BoundsCheckedPointer<char> ptr(description, size);
    
    bool hasPrinted = false;
    
    if (flags & NodeUsedAsNumber) {
        ptr.strcat("UsedAsNum");
        hasPrinted = true;
    }
    
    if (flags & NodeNeedsNegZero) {
        if (hasPrinted)
            ptr.strcat("|");
        ptr.strcat("NeedsNegZero");
        hasPrinted = true;
    }
    
    if (flags & NodeMayOverflow) {
        if (hasPrinted)
            ptr.strcat("|");
        ptr.strcat("MayOverflow");
        hasPrinted = true;
    }
    
    if (flags & NodeMayNegZero) {
        if (hasPrinted)
            ptr.strcat("|");
        ptr.strcat("MayNegZero");
        hasPrinted = true;
    }
    
    *ptr++ = 0;
    
    return description;
}
#endif

// Entries in the NodeType enum (below) are composed of an id, a result type (possibly none)
// and some additional informative flags (must generate, is constant, etc).
#define NodeIdMask           0xFFF
#define NodeResultMask      0xF000
#define NodeMustGenerate   0x10000 // set on nodes that have side effects, and may not trivially be removed by DCE.
#define NodeIsConstant     0x20000
#define NodeIsJump         0x40000
#define NodeIsBranch       0x80000
#define NodeIsTerminal    0x100000
#define NodeHasVarArgs    0x200000
#define NodeClobbersWorld 0x400000
#define NodeMightClobber  0x800000

// These values record the result type of the node (as checked by NodeResultMask, above), 0 for no result.
#define NodeResultJS        0x1000
#define NodeResultNumber    0x2000
#define NodeResultInt32     0x3000
#define NodeResultBoolean   0x4000
#define NodeResultStorage   0x5000

// This macro defines a set of information about all known node types, used to populate NodeId, NodeType below.
#define FOR_EACH_DFG_OP(macro) \
    /* Nodes for constants. */\
    macro(JSConstant, NodeResultJS) \
    \
    /* Nodes for handling functions (both as call and as construct). */\
    macro(ConvertThis, NodeResultJS) \
    macro(CreateThis, NodeResultJS) /* Note this is not MustGenerate since we're returning it anyway. */ \
    macro(GetCallee, NodeResultJS) \
    \
    /* Nodes for local variable access. */\
    macro(GetLocal, NodeResultJS) \
    macro(SetLocal, 0) \
    macro(Phantom, NodeMustGenerate) \
    macro(Phi, 0) \
    macro(Flush, NodeMustGenerate) \
    \
    /* Marker for arguments being set. */\
    macro(SetArgument, 0) \
    \
    /* Hint that inlining begins here. No code is generated for this node. It's only */\
    /* used for copying OSR data into inline frame data, to support reification of */\
    /* call frames of inlined functions. */\
    macro(InlineStart, 0) \
    \
    /* Nodes for bitwise operations. */\
    macro(BitAnd, NodeResultInt32) \
    macro(BitOr, NodeResultInt32) \
    macro(BitXor, NodeResultInt32) \
    macro(BitLShift, NodeResultInt32) \
    macro(BitRShift, NodeResultInt32) \
    macro(BitURShift, NodeResultInt32) \
    /* Bitwise operators call ToInt32 on their operands. */\
    macro(ValueToInt32, NodeResultInt32 | NodeMustGenerate) \
    /* Used to box the result of URShift nodes (result has range 0..2^32-1). */\
    macro(UInt32ToNumber, NodeResultNumber) \
    \
    /* Nodes for arithmetic operations. */\
    macro(ArithAdd, NodeResultNumber) \
    macro(ArithSub, NodeResultNumber) \
    macro(ArithMul, NodeResultNumber) \
    macro(ArithDiv, NodeResultNumber) \
    macro(ArithMod, NodeResultNumber) \
    macro(ArithAbs, NodeResultNumber) \
    macro(ArithMin, NodeResultNumber) \
    macro(ArithMax, NodeResultNumber) \
    macro(ArithSqrt, NodeResultNumber) \
    /* Arithmetic operators call ToNumber on their operands. */\
    macro(ValueToNumber, NodeResultNumber | NodeMustGenerate) \
    \
    /* A variant of ValueToNumber, which a hint that the parents will always use this as a double. */\
    macro(ValueToDouble, NodeResultNumber | NodeMustGenerate) \
    \
    /* Add of values may either be arithmetic, or result in string concatenation. */\
    macro(ValueAdd, NodeResultJS | NodeMustGenerate | NodeMightClobber) \
    \
    /* Property access. */\
    /* PutByValAlias indicates a 'put' aliases a prior write to the same property. */\
    /* Since a put to 'length' may invalidate optimizations here, */\
    /* this must be the directly subsequent property put. */\
    macro(GetByVal, NodeResultJS | NodeMustGenerate | NodeMightClobber) \
    macro(PutByVal, NodeMustGenerate | NodeClobbersWorld) \
    macro(PutByValAlias, NodeMustGenerate | NodeClobbersWorld) \
    macro(GetById, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(PutById, NodeMustGenerate | NodeClobbersWorld) \
    macro(PutByIdDirect, NodeMustGenerate | NodeClobbersWorld) \
    macro(CheckStructure, NodeMustGenerate) \
    macro(PutStructure, NodeMustGenerate | NodeClobbersWorld) \
    macro(GetPropertyStorage, NodeResultStorage) \
    macro(GetByOffset, NodeResultJS) \
    macro(PutByOffset, NodeMustGenerate | NodeClobbersWorld) \
    macro(GetArrayLength, NodeResultInt32) \
    macro(GetStringLength, NodeResultInt32) \
    macro(GetByteArrayLength, NodeResultInt32) \
    macro(GetMethod, NodeResultJS | NodeMustGenerate) \
    macro(CheckMethod, NodeResultJS | NodeMustGenerate) \
    macro(GetScopeChain, NodeResultJS) \
    macro(GetScopedVar, NodeResultJS | NodeMustGenerate) \
    macro(PutScopedVar, NodeMustGenerate | NodeClobbersWorld) \
    macro(GetGlobalVar, NodeResultJS | NodeMustGenerate) \
    macro(PutGlobalVar, NodeMustGenerate | NodeClobbersWorld) \
    macro(CheckFunction, NodeMustGenerate) \
    \
    /* Optimizations for array mutation. */\
    macro(ArrayPush, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(ArrayPop, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    \
    /* Optimizations for string access */ \
    macro(StringCharCodeAt, NodeResultInt32) \
    macro(StringCharAt, NodeResultJS) \
    \
    /* Nodes for comparison operations. */\
    macro(CompareLess, NodeResultBoolean | NodeMustGenerate | NodeMightClobber) \
    macro(CompareLessEq, NodeResultBoolean | NodeMustGenerate | NodeMightClobber) \
    macro(CompareGreater, NodeResultBoolean | NodeMustGenerate | NodeMightClobber) \
    macro(CompareGreaterEq, NodeResultBoolean | NodeMustGenerate | NodeMightClobber) \
    macro(CompareEq, NodeResultBoolean | NodeMustGenerate | NodeMightClobber) \
    macro(CompareStrictEq, NodeResultBoolean) \
    \
    /* Calls. */\
    macro(Call, NodeResultJS | NodeMustGenerate | NodeHasVarArgs | NodeClobbersWorld) \
    macro(Construct, NodeResultJS | NodeMustGenerate | NodeHasVarArgs | NodeClobbersWorld) \
    \
    /* Allocations. */\
    macro(NewObject, NodeResultJS) \
    macro(NewArray, NodeResultJS | NodeHasVarArgs) \
    macro(NewArrayBuffer, NodeResultJS) \
    macro(NewRegexp, NodeResultJS) \
    \
    /* Resolve nodes. */\
    macro(Resolve, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(ResolveBase, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(ResolveBaseStrictPut, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(ResolveGlobal, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    \
    /* Nodes for misc operations. */\
    macro(Breakpoint, NodeMustGenerate | NodeClobbersWorld) \
    macro(CheckHasInstance, NodeMustGenerate) \
    macro(InstanceOf, NodeResultBoolean) \
    macro(LogicalNot, NodeResultBoolean | NodeMightClobber) \
    macro(ToPrimitive, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(StrCat, NodeResultJS | NodeMustGenerate | NodeHasVarArgs | NodeClobbersWorld) \
    \
    /* Block terminals. */\
    macro(Jump, NodeMustGenerate | NodeIsTerminal | NodeIsJump) \
    macro(Branch, NodeMustGenerate | NodeIsTerminal | NodeIsBranch) \
    macro(Return, NodeMustGenerate | NodeIsTerminal) \
    macro(Throw, NodeMustGenerate | NodeIsTerminal) \
    macro(ThrowReferenceError, NodeMustGenerate | NodeIsTerminal) \
    \
    /* This is a pseudo-terminal. It means that execution should fall out of DFG at */\
    /* this point, but execution does continue in the basic block - just in a */\
    /* different compiler. */\
    macro(ForceOSRExit, NodeMustGenerate)

// This enum generates a monotonically increasing id for all Node types,
// and is used by the subsequent enum to fill out the id (as accessed via the NodeIdMask).
enum NodeId {
#define DFG_OP_ENUM(opcode, flags) opcode##_id,
    FOR_EACH_DFG_OP(DFG_OP_ENUM)
#undef DFG_OP_ENUM
    LastNodeId
};

// Entries in this enum describe all Node types.
// The enum value contains a monotonically increasing id, a result type, and additional flags.
enum NodeType {
#define DFG_OP_ENUM(opcode, flags) opcode = opcode##_id | (flags),
    FOR_EACH_DFG_OP(DFG_OP_ENUM)
#undef DFG_OP_ENUM
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
        : op(op)
        , codeOrigin(codeOrigin)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_prediction(PredictNone)
    {
        ASSERT(!(op & NodeHasVarArgs));
        ASSERT(!hasArithNodeFlags());
        children.fixed.child1 = child1;
        children.fixed.child2 = child2;
        children.fixed.child3 = child3;
    }

    // Construct a node with up to 3 children and an immediate value.
    Node(NodeType op, CodeOrigin codeOrigin, OpInfo imm, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : op(op)
        , codeOrigin(codeOrigin)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm.m_value)
        , m_prediction(PredictNone)
    {
        ASSERT(!(op & NodeHasVarArgs));
        children.fixed.child1 = child1;
        children.fixed.child2 = child2;
        children.fixed.child3 = child3;
    }

    // Construct a node with up to 3 children and two immediate values.
    Node(NodeType op, CodeOrigin codeOrigin, OpInfo imm1, OpInfo imm2, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : op(op)
        , codeOrigin(codeOrigin)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(safeCast<unsigned>(imm2.m_value))
        , m_prediction(PredictNone)
    {
        ASSERT(!(op & NodeHasVarArgs));
        children.fixed.child1 = child1;
        children.fixed.child2 = child2;
        children.fixed.child3 = child3;
    }
    
    // Construct a node with a variable number of children and two immediate values.
    Node(VarArgTag, NodeType op, CodeOrigin codeOrigin, OpInfo imm1, OpInfo imm2, unsigned firstChild, unsigned numChildren)
        : op(op)
        , codeOrigin(codeOrigin)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(safeCast<unsigned>(imm2.m_value))
        , m_prediction(PredictNone)
    {
        ASSERT(op & NodeHasVarArgs);
        children.variable.firstChild = firstChild;
        children.variable.numChildren = numChildren;
    }

    bool mustGenerate()
    {
        return op & NodeMustGenerate;
    }

    bool isConstant()
    {
        return op == JSConstant;
    }
    
    bool hasConstant()
    {
        return isConstant() || hasMethodCheckData();
    }

    unsigned constantNumber()
    {
        ASSERT(isConstant());
        return m_opInfo;
    }
    
    // NOTE: this only works for JSConstant nodes.
    JSValue valueOfJSConstantNode(CodeBlock* codeBlock)
    {
        return codeBlock->constantRegister(FirstConstantRegisterIndex + constantNumber()).get();
    }

    bool isInt32Constant(CodeBlock* codeBlock)
    {
        return isConstant() && valueOfJSConstantNode(codeBlock).isInt32();
    }
    
    bool isDoubleConstant(CodeBlock* codeBlock)
    {
        bool result = isConstant() && valueOfJSConstantNode(codeBlock).isDouble();
        if (result)
            ASSERT(!isInt32Constant(codeBlock));
        return result;
    }
    
    bool isNumberConstant(CodeBlock* codeBlock)
    {
        bool result = isConstant() && valueOfJSConstantNode(codeBlock).isNumber();
        ASSERT(result == (isInt32Constant(codeBlock) || isDoubleConstant(codeBlock)));
        return result;
    }
    
    bool isBooleanConstant(CodeBlock* codeBlock)
    {
        return isConstant() && valueOfJSConstantNode(codeBlock).isBoolean();
    }
    
    bool hasVariableAccessData()
    {
        switch (op) {
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

#ifndef NDEBUG
    bool hasIdentifier()
    {
        switch (op) {
        case GetById:
        case PutById:
        case PutByIdDirect:
        case GetMethod:
        case CheckMethod:
        case Resolve:
        case ResolveBase:
        case ResolveBaseStrictPut:
            return true;
        default:
            return false;
        }
    }
#endif

    unsigned identifierNumber()
    {
        ASSERT(hasIdentifier());
        return m_opInfo;
    }
    
    unsigned resolveGlobalDataIndex()
    {
        ASSERT(op == ResolveGlobal);
        return m_opInfo;
    }

    bool hasArithNodeFlags()
    {
        switch (op) {
        case ValueToNumber:
        case ValueToDouble:
        case UInt32ToNumber:
        case ArithAdd:
        case ArithSub:
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
    
    ArithNodeFlags rawArithNodeFlags()
    {
        ASSERT(hasArithNodeFlags());
        return m_opInfo;
    }
    
    // This corrects the arithmetic node flags, so that irrelevant bits are
    // ignored. In particular, anything other than ArithMul does not need
    // to know if it can speculate on negative zero.
    ArithNodeFlags arithNodeFlags()
    {
        ArithNodeFlags result = rawArithNodeFlags();
        if (op == ArithMul)
            return result;
        return result & ~NodeNeedsNegZero;
    }
    
    ArithNodeFlags arithNodeFlagsForCompare()
    {
        if (hasArithNodeFlags())
            return arithNodeFlags();
        return 0;
    }
    
    void setArithNodeFlag(ArithNodeFlags flags)
    {
        ASSERT(hasArithNodeFlags());
        m_opInfo = flags;
    }
    
    bool mergeArithNodeFlags(ArithNodeFlags flags)
    {
        if (!hasArithNodeFlags())
            return false;
        ArithNodeFlags newFlags = m_opInfo | flags;
        if (newFlags == m_opInfo)
            return false;
        m_opInfo = newFlags;
        return true;
    }
    
    bool hasConstantBuffer()
    {
        return op == NewArrayBuffer;
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
        return op == NewRegexp;
    }
    
    unsigned regexpIndex()
    {
        ASSERT(hasRegexpIndex());
        return m_opInfo;
    }
    
    bool hasVarNumber()
    {
        return op == GetGlobalVar || op == PutGlobalVar || op == GetScopedVar || op == PutScopedVar;
    }

    unsigned varNumber()
    {
        ASSERT(hasVarNumber());
        return m_opInfo;
    }

    bool hasScopeChainDepth()
    {
        return op == GetScopeChain;
    }
    
    unsigned scopeChainDepth()
    {
        ASSERT(hasScopeChainDepth());
        return m_opInfo;
    }

    bool hasResult()
    {
        return op & NodeResultMask;
    }

    bool hasInt32Result()
    {
        return (op & NodeResultMask) == NodeResultInt32;
    }
    
    bool hasNumberResult()
    {
        return (op & NodeResultMask) == NodeResultNumber;
    }
    
    bool hasJSResult()
    {
        return (op & NodeResultMask) == NodeResultJS;
    }
    
    bool hasBooleanResult()
    {
        return (op & NodeResultMask) == NodeResultBoolean;
    }

    bool isJump()
    {
        return op & NodeIsJump;
    }

    bool isBranch()
    {
        return op & NodeIsBranch;
    }

    bool isTerminal()
    {
        return op & NodeIsTerminal;
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
        switch (op) {
        case GetById:
        case GetMethod:
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
    
    bool hasMethodCheckData()
    {
        return op == CheckMethod;
    }
    
    unsigned methodCheckDataIndex()
    {
        ASSERT(hasMethodCheckData());
        return m_opInfo2;
    }

    bool hasFunctionCheckData()
    {
        return op == CheckFunction;
    }

    JSFunction* function()
    {
        ASSERT(hasFunctionCheckData());
        return reinterpret_cast<JSFunction*>(m_opInfo);
    }

    bool hasStructureTransitionData()
    {
        return op == PutStructure;
    }
    
    StructureTransitionData& structureTransitionData()
    {
        ASSERT(hasStructureTransitionData());
        return *reinterpret_cast<StructureTransitionData*>(m_opInfo);
    }
    
    bool hasStructureSet()
    {
        return op == CheckStructure;
    }
    
    StructureSet& structureSet()
    {
        ASSERT(hasStructureSet());
        return *reinterpret_cast<StructureSet*>(m_opInfo);
    }
    
    bool hasStorageAccessData()
    {
        return op == GetByOffset || op == PutByOffset;
    }
    
    unsigned storageAccessDataIndex()
    {
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
        return m_refCount && op != Phi && op != Flush;
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
    
    NodeIndex child1()
    {
        ASSERT(!(op & NodeHasVarArgs));
        return children.fixed.child1;
    }
    
    // This is useful if you want to do a fast check on the first child
    // before also doing a check on the opcode. Use this with care and
    // avoid it if possible.
    NodeIndex child1Unchecked()
    {
        return children.fixed.child1;
    }

    NodeIndex child2()
    {
        ASSERT(!(op & NodeHasVarArgs));
        return children.fixed.child2;
    }

    NodeIndex child3()
    {
        ASSERT(!(op & NodeHasVarArgs));
        return children.fixed.child3;
    }
    
    unsigned firstChild()
    {
        ASSERT(op & NodeHasVarArgs);
        return children.variable.firstChild;
    }
    
    unsigned numChildren()
    {
        ASSERT(op & NodeHasVarArgs);
        return children.variable.numChildren;
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
        return isNumberPrediction(prediction()) || prediction() == PredictNone;
    }
    
    bool shouldNotSpeculateInteger()
    {
        return !!(prediction() & PredictDouble);
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
    
    bool shouldSpeculateByteArray()
    {
        return !!(prediction() & PredictByteArray);
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
        return (op1.shouldSpeculateFinalObject() && op2.shouldSpeculateObject())
            || (op1.shouldSpeculateObject() && op2.shouldSpeculateFinalObject());
    }

    static bool shouldSpeculateArray(Node& op1, Node& op2)
    {
        return (op1.shouldSpeculateArray() && op2.shouldSpeculateObject())
            || (op1.shouldSpeculateObject() && op2.shouldSpeculateArray());
    }
    
    bool canSpeculateInteger()
    {
        return nodeCanSpeculateInteger(arithNodeFlags());
    }
    
    // This enum value describes the type of the node.
    NodeType op;
    // Used to look up exception handling information (currently implemented as a bytecode index).
    CodeOrigin codeOrigin;
    // References to up to 3 children (0 for no child).
    union {
        struct {
            NodeIndex child1, child2, child3;
        } fixed;
        struct {
            unsigned firstChild;
            unsigned numChildren;
        } variable;
    } children;

private:
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
