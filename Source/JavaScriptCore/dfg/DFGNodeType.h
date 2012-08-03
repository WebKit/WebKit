/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef DFGNodeType_h
#define DFGNodeType_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGNodeFlags.h"

namespace JSC { namespace DFG {

// This macro defines a set of information about all known node types, used to populate NodeId, NodeType below.
#define FOR_EACH_DFG_OP(macro) \
    /* A constant in the CodeBlock's constant pool. */\
    macro(JSConstant, NodeResultJS) \
    \
    /* A constant not in the CodeBlock's constant pool. Uses get patched to jumps that exit the */\
    /* code block. */\
    macro(WeakJSConstant, NodeResultJS) \
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
    macro(Nop, 0) \
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
    /* Used to cast known integers to doubles, so as to separate the double form */\
    /* of the value from the integer form. */\
    macro(Int32ToDouble, NodeResultNumber) \
    /* Used to speculate that a double value is actually an integer. */\
    macro(DoubleAsInt32, NodeResultInt32) \
    /* Used to record places where we must check if a value is a number. */\
    macro(CheckNumber, NodeMustGenerate) \
    \
    /* Nodes for arithmetic operations. */\
    macro(ArithAdd, NodeResultNumber | NodeMustGenerate) \
    macro(ArithSub, NodeResultNumber | NodeMustGenerate) \
    macro(ArithNegate, NodeResultNumber | NodeMustGenerate) \
    macro(ArithMul, NodeResultNumber | NodeMustGenerate) \
    macro(ArithDiv, NodeResultNumber | NodeMustGenerate) \
    macro(ArithMod, NodeResultNumber | NodeMustGenerate) \
    macro(ArithAbs, NodeResultNumber | NodeMustGenerate) \
    macro(ArithMin, NodeResultNumber | NodeMustGenerate) \
    macro(ArithMax, NodeResultNumber | NodeMustGenerate) \
    macro(ArithSqrt, NodeResultNumber | NodeMustGenerate) \
    \
    /* Add of values may either be arithmetic, or result in string concatenation. */\
    macro(ValueAdd, NodeResultJS | NodeMustGenerate | NodeMightClobber) \
    \
    /* Property access. */\
    /* PutByValAlias indicates a 'put' aliases a prior write to the same property. */\
    /* Since a put to 'length' may invalidate optimizations here, */\
    /* this must be the directly subsequent property put. */\
    macro(GetByVal, NodeResultJS | NodeMustGenerate | NodeMightClobber) \
    macro(PutByVal, NodeMustGenerate | NodeMightClobber) \
    macro(PutByValAlias, NodeMustGenerate | NodeMightClobber) \
    macro(GetById, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(GetByIdFlush, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(PutById, NodeMustGenerate | NodeClobbersWorld) \
    macro(PutByIdDirect, NodeMustGenerate | NodeClobbersWorld) \
    macro(CheckStructure, NodeMustGenerate) \
    macro(PutStructure, NodeMustGenerate | NodeClobbersWorld) \
    macro(GetPropertyStorage, NodeResultStorage) \
    macro(GetIndexedPropertyStorage, NodeMustGenerate | NodeResultStorage) \
    macro(GetByOffset, NodeResultJS) \
    macro(PutByOffset, NodeMustGenerate | NodeClobbersWorld) \
    macro(GetArrayLength, NodeResultInt32) \
    macro(GetStringLength, NodeResultInt32) \
    macro(GetInt8ArrayLength, NodeResultInt32) \
    macro(GetInt16ArrayLength, NodeResultInt32) \
    macro(GetInt32ArrayLength, NodeResultInt32) \
    macro(GetUint8ArrayLength, NodeResultInt32) \
    macro(GetUint8ClampedArrayLength, NodeResultInt32) \
    macro(GetUint16ArrayLength, NodeResultInt32) \
    macro(GetUint32ArrayLength, NodeResultInt32) \
    macro(GetFloat32ArrayLength, NodeResultInt32) \
    macro(GetFloat64ArrayLength, NodeResultInt32) \
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
    /* Optimizations for regular expression matching. */\
    macro(RegExpExec, NodeResultJS | NodeMustGenerate) \
    macro(RegExpTest, NodeResultJS | NodeMustGenerate) \
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
    macro(IsUndefined, NodeResultBoolean) \
    macro(IsBoolean, NodeResultBoolean) \
    macro(IsNumber, NodeResultBoolean) \
    macro(IsString, NodeResultBoolean) \
    macro(IsObject, NodeResultBoolean) \
    macro(IsFunction, NodeResultBoolean) \
    macro(LogicalNot, NodeResultBoolean | NodeMightClobber) \
    macro(ToPrimitive, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(StrCat, NodeResultJS | NodeMustGenerate | NodeHasVarArgs | NodeClobbersWorld) \
    \
    /* Nodes used for activations. Activation support works by having it anchored at */\
    /* epilgoues via TearOffActivation, and all CreateActivation nodes kept alive by */\
    /* being threaded with each other. */\
    macro(CreateActivation, NodeResultJS) \
    macro(TearOffActivation, NodeMustGenerate) \
    \
    /* Nodes for creating functions. */\
    macro(NewFunctionNoCheck, NodeResultJS) \
    macro(NewFunction, NodeResultJS) \
    macro(NewFunctionExpression, NodeResultJS) \
    \
    /* Block terminals. */\
    macro(Jump, NodeMustGenerate) \
    macro(Branch, NodeMustGenerate) \
    macro(Return, NodeMustGenerate) \
    macro(Throw, NodeMustGenerate) \
    macro(ThrowReferenceError, NodeMustGenerate) \
    \
    /* This is a pseudo-terminal. It means that execution should fall out of DFG at */\
    /* this point, but execution does continue in the basic block - just in a */\
    /* different compiler. */\
    macro(ForceOSRExit, NodeMustGenerate)

// This enum generates a monotonically increasing id for all Node types,
// and is used by the subsequent enum to fill out the id (as accessed via the NodeIdMask).
enum NodeType {
#define DFG_OP_ENUM(opcode, flags) opcode,
    FOR_EACH_DFG_OP(DFG_OP_ENUM)
#undef DFG_OP_ENUM
    LastNodeType
};

// Specifies the default flags for each node.
inline NodeFlags defaultFlags(NodeType op)
{
    switch (op) {
#define DFG_OP_ENUM(opcode, flags) case opcode: return flags;
    FOR_EACH_DFG_OP(DFG_OP_ENUM)
#undef DFG_OP_ENUM
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGNodeType_h

