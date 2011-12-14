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

// Emit various logging information for debugging, including dumping the dataflow graphs.
#define DFG_DEBUG_VERBOSE 0
// Enable generation of dynamic checks into the instruction stream.
#define DFG_JIT_ASSERT 0
// Consistency check contents compiler data structures.
#define DFG_CONSISTENCY_CHECK 0
// Emit a breakpoint into the head of every generated function, to aid debugging in GDB.
#define DFG_JIT_BREAK_ON_EVERY_FUNCTION 0
// Emit a breakpoint into the head of every generated node, to aid debugging in GDB.
#define DFG_JIT_BREAK_ON_EVERY_BLOCK 0
// Emit a breakpoint into the head of every generated node, to aid debugging in GDB.
#define DFG_JIT_BREAK_ON_EVERY_NODE 0
// Emit a breakpoint into the speculation failure code.
#define DFG_JIT_BREAK_ON_SPECULATION_FAILURE 0
// Disable the DFG JIT without having to touch Platform.h!
#define DFG_DEBUG_LOCAL_DISBALE 0
// Disable the SpeculativeJIT without having to touch Platform.h!
#define DFG_DEBUG_LOCAL_DISBALE_SPECULATIVE 0
// Generate stats on how successful we were in making use of the DFG jit, and remaining on the hot path.
#define DFG_SUCCESS_STATS 0


#if ENABLE(DFG_JIT)

#include <wtf/Vector.h>

namespace JSC { namespace DFG {

// Type for a virtual register number (spill location).
// Using an enum to make this type-checked at compile time, to avert programmer errors.
enum VirtualRegister { InvalidVirtualRegister = -1 };
COMPILE_ASSERT(sizeof(VirtualRegister) == sizeof(int), VirtualRegister_is_32bit);

// Type for a reference to another node in the graph.
typedef uint32_t NodeIndex;
static const NodeIndex NoNode = UINT_MAX;

// Information used to map back from an exception to any handler/source information.
// (Presently implemented as a bytecode index).
typedef uint32_t ExceptionInfo;

// Entries in the NodeType enum (below) are composed of an id, a result type (possibly none)
// and some additional informative flags (must generate, is constant, etc).
#define NodeIdMask          0xFFF
#define NodeResultMask     0xF000
#define NodeMustGenerate  0x10000 // set on nodes that have side effects, and may not trivially be removed by DCE.
#define NodeIsConstant    0x20000
#define NodeIsJump        0x40000
#define NodeIsBranch      0x80000
#define NodeIsTerminal   0x100000
#define NodeHasVarArgs   0x200000

// These values record the result type of the node (as checked by NodeResultMask, above), 0 for no result.
#define NodeResultJS      0x1000
#define NodeResultDouble  0x2000
#define NodeResultInt32   0x3000

// This macro defines a set of information about all known node types, used to populate NodeId, NodeType below.
#define FOR_EACH_DFG_OP(macro) \
    /* Nodes for constants. */\
    macro(JSConstant, NodeResultJS) \
    macro(ConvertThis, NodeResultJS) \
    \
    /* Nodes for local variable access. */\
    macro(GetLocal, NodeResultJS) \
    macro(SetLocal, 0) \
    macro(Phi, 0) \
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
    macro(UInt32ToNumber, NodeResultDouble) \
    \
    /* Nodes for arithmetic operations. */\
    macro(ArithAdd, NodeResultDouble) \
    macro(ArithSub, NodeResultDouble) \
    macro(ArithMul, NodeResultDouble) \
    macro(ArithDiv, NodeResultDouble) \
    macro(ArithMod, NodeResultDouble) \
    /* Arithmetic operators call ToNumber on their operands. */\
    macro(ValueToNumber, NodeResultDouble | NodeMustGenerate) \
    \
    /* Add of values may either be arithmetic, or result in string concatenation. */\
    macro(ValueAdd, NodeResultJS | NodeMustGenerate) \
    \
    /* Property access. */\
    /* PutByValAlias indicates a 'put' aliases a prior write to the same property. */\
    /* Since a put to 'length' may invalidate optimizations here, */\
    /* this must be the directly subsequent property put. */\
    macro(GetByVal, NodeResultJS | NodeMustGenerate) \
    macro(PutByVal, NodeMustGenerate) \
    macro(PutByValAlias, NodeMustGenerate) \
    macro(GetById, NodeResultJS | NodeMustGenerate) \
    macro(PutById, NodeMustGenerate) \
    macro(PutByIdDirect, NodeMustGenerate) \
    macro(GetMethod, NodeResultJS | NodeMustGenerate) \
    macro(GetGlobalVar, NodeResultJS | NodeMustGenerate) \
    macro(PutGlobalVar, NodeMustGenerate) \
    \
    /* Nodes for comparison operations. */\
    macro(CompareLess, NodeResultJS | NodeMustGenerate) \
    macro(CompareLessEq, NodeResultJS | NodeMustGenerate) \
    macro(CompareGreater, NodeResultJS | NodeMustGenerate) \
    macro(CompareGreaterEq, NodeResultJS | NodeMustGenerate) \
    macro(CompareEq, NodeResultJS | NodeMustGenerate) \
    macro(CompareStrictEq, NodeResultJS) \
    \
    /* Calls. */\
    macro(Call, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(Construct, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    \
    /* Resolve nodes. */\
    macro(Resolve, NodeResultJS | NodeMustGenerate) \
    macro(ResolveBase, NodeResultJS | NodeMustGenerate) \
    macro(ResolveBaseStrictPut, NodeResultJS | NodeMustGenerate) \
    \
    /* Nodes for misc operations. */\
    macro(Breakpoint, NodeMustGenerate) \
    macro(CheckHasInstance, NodeMustGenerate) \
    macro(InstanceOf, NodeResultJS) \
    macro(LogicalNot, NodeResultJS) \
    \
    /* Block terminals. */\
    macro(Jump, NodeMustGenerate | NodeIsTerminal | NodeIsJump) \
    macro(Branch, NodeMustGenerate | NodeIsTerminal | NodeIsBranch) \
    macro(Return, NodeMustGenerate | NodeIsTerminal)

// This enum generates a monotonically increasing id for all Node types,
// and is used by the subsequent enum to fill out the id (as accessed via the NodeIdMask).
enum NodeId {
#define DFG_OP_ENUM(opcode, flags) opcode##_id,
    FOR_EACH_DFG_OP(DFG_OP_ENUM)
#undef DFG_OP_ENUM
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
    explicit OpInfo(unsigned value) : m_value(value) {}
    unsigned m_value;
};

typedef uint8_t PredictedType;
static const PredictedType PredictNone   = 0;
static const PredictedType PredictCell   = 0x01;
static const PredictedType PredictArray  = 0x03;
static const PredictedType PredictInt32  = 0x04;
static const PredictedType PredictDouble = 0x08;
static const PredictedType PredictNumber = 0x0c;

inline bool isCellPrediction(PredictedType value)
{
    return (value & PredictCell) == PredictCell && !(value & ~PredictArray);
}

inline bool isArrayPrediction(PredictedType value)
{
    return value == PredictArray;
}

inline bool isInt32Prediction(PredictedType value)
{
    return value == PredictInt32;
}

inline bool isDoublePrediction(PredictedType value)
{
    return value == PredictDouble;
}

inline bool isNumberPrediction(PredictedType value)
{
    return !!(value & PredictNumber) && !(value & ~PredictNumber);
}

#ifndef NDEBUG
inline const char* predictionToString(PredictedType value)
{
    switch (value) {
    case PredictNone:
        return "p-bottom";
    case PredictCell:
        return "p-cell";
    case PredictArray:
        return "p-array";
    case PredictInt32:
        return "p-int32";
    case PredictNumber:
        return "p-number";
    default:
        return "p-top";
    }
}
#endif

// === Node ===
//
// Node represents a single operation in the data flow graph.
struct Node {
    enum VarArgTag { VarArg };

    // Construct a node with up to 3 children, no immediate value.
    Node(NodeType op, ExceptionInfo exceptionInfo, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : op(op)
        , exceptionInfo(exceptionInfo)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
    {
        ASSERT(!(op & NodeHasVarArgs));
        children.fixed.child1 = child1;
        children.fixed.child2 = child2;
        children.fixed.child3 = child3;
    }

    // Construct a node with up to 3 children and an immediate value.
    Node(NodeType op, ExceptionInfo exceptionInfo, OpInfo imm, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : op(op)
        , exceptionInfo(exceptionInfo)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm.m_value)
    {
        ASSERT(!(op & NodeHasVarArgs));
        children.fixed.child1 = child1;
        children.fixed.child2 = child2;
        children.fixed.child3 = child3;
    }

    // Construct a node with up to 3 children and two immediate values.
    Node(NodeType op, ExceptionInfo exceptionInfo, OpInfo imm1, OpInfo imm2, NodeIndex child1 = NoNode, NodeIndex child2 = NoNode, NodeIndex child3 = NoNode)
        : op(op)
        , exceptionInfo(exceptionInfo)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(imm2.m_value)
    {
        ASSERT(!(op & NodeHasVarArgs));
        children.fixed.child1 = child1;
        children.fixed.child2 = child2;
        children.fixed.child3 = child3;
    }
    
    // Construct a node with a variable number of children and two immediate values.
    Node(VarArgTag, NodeType op, ExceptionInfo exceptionInfo, OpInfo imm1, OpInfo imm2, unsigned firstChild, unsigned numChildren)
        : op(op)
        , exceptionInfo(exceptionInfo)
        , m_virtualRegister(InvalidVirtualRegister)
        , m_refCount(0)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(imm2.m_value)
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

    unsigned constantNumber()
    {
        ASSERT(isConstant());
        return m_opInfo;
    }

    bool hasLocal()
    {
        return op == GetLocal || op == SetLocal;
    }

    VirtualRegister local()
    {
        ASSERT(hasLocal());
        return (VirtualRegister)m_opInfo;
    }

#if !ASSERT_DISABLED
    // If we want to use this in production code, should make it faster -
    // e.g. make hasIdentifier a flag in the bitfield.
    bool hasIdentifier()
    {
        return op == GetById || op == PutById || op == PutByIdDirect || op == GetMethod
            || op == Resolve || op == ResolveBase || op == ResolveBaseStrictPut;
    }
#endif

    unsigned identifierNumber()
    {
        ASSERT(hasIdentifier());
        return m_opInfo;
    }

    bool hasVarNumber()
    {
        return op == GetGlobalVar || op == PutGlobalVar;
    }

    unsigned varNumber()
    {
        ASSERT(hasVarNumber());
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

    bool hasDoubleResult()
    {
        return (op & NodeResultMask) == NodeResultDouble;
    }

    bool hasJSResult()
    {
        return (op & NodeResultMask) == NodeResultJS;
    }

    // Check for integers or doubles.
    bool hasNumericResult()
    {
        // This check will need updating if more result types are added.
        ASSERT((hasInt32Result() || hasDoubleResult()) == !hasJSResult());
        return !hasJSResult();
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

    unsigned takenBytecodeOffset()
    {
        ASSERT(isBranch() || isJump());
        return m_opInfo;
    }

    unsigned notTakenBytecodeOffset()
    {
        ASSERT(isBranch());
        return m_opInfo2;
    }
    
    bool hasPrediction()
    {
        switch (op) {
        case GetById:
        case GetMethod:
        case GetByVal:
        case Call:
        case Construct:
            return true;
        default:
            return false;
        }
    }
    
    PredictedType getPrediction()
    {
        ASSERT(hasPrediction());
        return static_cast<PredictedType>(m_opInfo2);
    }
    
    void predict(PredictedType prediction)
    {
        ASSERT(hasPrediction());
        m_opInfo2 |= prediction;
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
        return m_refCount && op != Phi;
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
    
    NodeIndex child1()
    {
        ASSERT(!(op & NodeHasVarArgs));
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
    
    // This enum value describes the type of the node.
    NodeType op;
    // Used to look up exception handling information (currently implemented as a bytecode index).
    ExceptionInfo exceptionInfo;
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
    // Immediate values, accesses type-checked via accessors above.
    unsigned m_opInfo, m_opInfo2;
};

} } // namespace JSC::DFG

#endif
#endif
