/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGNodeFlags.h"

namespace JSC { namespace DFG {

// This macro defines a set of information about all known node types, used to populate NodeId, NodeType below.
#define FOR_EACH_DFG_OP(macro) \
    /* A constant in the CodeBlock's constant pool. */\
    macro(JSConstant, NodeResultJS | NodeDoesNotExit) \
    \
    /* Constants with specific representations. */\
    macro(DoubleConstant, NodeResultDouble | NodeDoesNotExit) \
    macro(Int52Constant, NodeResultInt52 | NodeDoesNotExit) \
    \
    /* A constant not in the CodeBlock's constant pool. Uses get patched to jumps that exit the */\
    /* code block. */\
    macro(WeakJSConstant, NodeResultJS | NodeDoesNotExit) \
    \
    /* Marker to indicate that an operation was optimized entirely and all that is left */\
    /* is to make one node alias another. CSE will later usually eliminate this node, */\
    /* though it may choose not to if it would corrupt predictions (very rare). */\
    macro(Identity, NodeResultJS) \
    \
    /* Nodes for handling functions (both as call and as construct). */\
    macro(ToThis, NodeResultJS) \
    macro(CreateThis, NodeResultJS) /* Note this is not MustGenerate since we're returning it anyway. */ \
    macro(GetCallee, NodeResultJS) \
    \
    /* Nodes for local variable access. These nodes are linked together using Phi nodes. */\
    /* Any two nodes that are part of the same Phi graph will share the same */\
    /* VariableAccessData, and thus will share predictions. */\
    macro(GetLocal, NodeResultJS) \
    macro(SetLocal, 0) \
    macro(MovHint, NodeDoesNotExit) \
    macro(ZombieHint, NodeDoesNotExit) \
    macro(GetArgument, NodeResultJS | NodeMustGenerate) \
    macro(Phantom, NodeMustGenerate) \
    macro(HardPhantom, NodeMustGenerate) /* Like Phantom, but we never remove any of its children. */ \
    macro(Check, 0) /* Used if we want just a type check but not liveness. DCE eithers kills this or converts it to Phantom. */\
    macro(Upsilon, NodeDoesNotExit | NodeRelevantToOSR) \
    macro(Phi, NodeDoesNotExit | NodeRelevantToOSR) \
    macro(Flush, NodeMustGenerate | NodeDoesNotExit) \
    macro(PhantomLocal, NodeMustGenerate | NodeDoesNotExit) \
    \
    /* Hint that this is where bytecode thinks is a good place to OSR. Note that this */\
    /* will exist even in inlined loops. This has no execution semantics but it must */\
    /* survive all DCE. We treat this as being a can-exit because tier-up to FTL may */\
    /* want all state. */\
    macro(LoopHint, NodeMustGenerate) \
    \
    /* Special node for OSR entry into the FTL. Indicates that we're loading a local */\
    /* variable from the scratch buffer. */\
    macro(ExtractOSREntryLocal, NodeResultJS) \
    \
    /* Tier-up checks from the DFG to the FTL. */\
    macro(CheckTierUpInLoop, NodeMustGenerate) \
    macro(CheckTierUpAndOSREnter, NodeMustGenerate) \
    macro(CheckTierUpAtReturn, NodeMustGenerate) \
    \
    /* Get the value of a local variable, without linking into the VariableAccessData */\
    /* network. This is only valid for variable accesses whose predictions originated */\
    /* as something other than a local access, and thus had their own profiling. */\
    macro(GetLocalUnlinked, NodeResultJS) \
    \
    /* Marker for an argument being set at the prologue of a function. */\
    macro(SetArgument, NodeDoesNotExit) \
    \
    /* Marker of location in the IR where we may possibly perform jump replacement to */\
    /* invalidate this code block. */\
    macro(InvalidationPoint, NodeMustGenerate) \
    \
    /* Nodes for bitwise operations. */\
    macro(BitAnd, NodeResultInt32) \
    macro(BitOr, NodeResultInt32) \
    macro(BitXor, NodeResultInt32) \
    macro(BitLShift, NodeResultInt32) \
    macro(BitRShift, NodeResultInt32) \
    macro(BitURShift, NodeResultInt32) \
    /* Bitwise operators call ToInt32 on their operands. */\
    macro(ValueToInt32, NodeResultInt32) \
    /* Used to box the result of URShift nodes (result has range 0..2^32-1). */\
    macro(UInt32ToNumber, NodeResultNumber) \
    /* Converts booleans to numbers but passes everything else through. */\
    macro(BooleanToNumber, NodeResultJS) \
    \
    /* Attempt to truncate a double to int32; this will exit if it can't do it. */\
    macro(DoubleAsInt32, NodeResultInt32) \
    \
    /* Change the representation of a value. */\
    macro(DoubleRep, NodeResultDouble) \
    macro(Int52Rep, NodeResultInt52) \
    macro(ValueRep, NodeResultJS) \
    \
    /* Bogus type asserting node. Useful for testing, disappears during Fixup. */\
    macro(FiatInt52, NodeResultJS) \
    \
    /* Nodes for arithmetic operations. */\
    macro(ArithAdd, NodeResultNumber) \
    macro(ArithSub, NodeResultNumber) \
    macro(ArithNegate, NodeResultNumber) \
    macro(ArithMul, NodeResultNumber) \
    macro(ArithIMul, NodeResultInt32) \
    macro(ArithDiv, NodeResultNumber) \
    macro(ArithMod, NodeResultNumber) \
    macro(ArithAbs, NodeResultNumber) \
    macro(ArithMin, NodeResultNumber) \
    macro(ArithMax, NodeResultNumber) \
    macro(ArithFRound, NodeResultNumber) \
    macro(ArithSqrt, NodeResultNumber) \
    macro(ArithSin, NodeResultNumber) \
    macro(ArithCos, NodeResultNumber) \
    \
    /* Add of values may either be arithmetic, or result in string concatenation. */\
    macro(ValueAdd, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    \
    /* Property access. */\
    /* PutByValAlias indicates a 'put' aliases a prior write to the same property. */\
    /* Since a put to 'length' may invalidate optimizations here, */\
    /* this must be the directly subsequent property put. Note that PutByVal */\
    /* opcodes use VarArgs beause they may have up to 4 children. */\
    macro(GetByVal, NodeResultJS | NodeMustGenerate | NodeMightClobber) \
    macro(PutByValDirect, NodeMustGenerate | NodeHasVarArgs | NodeMightClobber) \
    macro(PutByVal, NodeMustGenerate | NodeHasVarArgs | NodeMightClobber) \
    macro(PutByValAlias, NodeMustGenerate | NodeHasVarArgs | NodeMightClobber) \
    macro(GetById, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(GetByIdFlush, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(PutById, NodeMustGenerate | NodeClobbersWorld) \
    macro(PutByIdFlush, NodeMustGenerate | NodeMustGenerate | NodeClobbersWorld) \
    macro(PutByIdDirect, NodeMustGenerate | NodeClobbersWorld) \
    macro(CheckStructure, NodeMustGenerate) \
    macro(CheckExecutable, NodeMustGenerate) \
    /* Transition watchpoints are a contract between the party setting the watchpoint */\
    /* and the runtime system, where the party promises that the child object once had */\
    /* the structure being watched, and the runtime system in turn promises that the */\
    /* watchpoint will be turned into an OSR exit if any object with that structure */\
    /* ever transitions to a different structure. Hence, the child object must have */\
    /* previously had a CheckStructure executed on it or we're dealing with an object */\
    /* constant (WeakJSConstant) and the object was known to have that structure at */\
    /* compile-time. In the latter case this means that no structure checks have to be */\
    /* performed for this object by JITted code. In the former case this means that*/\
    /* the object's structure does not need to be rechecked due to side-effecting */\
    /* (clobbering) operations. */\
    macro(StructureTransitionWatchpoint, NodeMustGenerate) \
    macro(PutStructure, NodeMustGenerate) \
    macro(PhantomPutStructure, NodeMustGenerate | NodeDoesNotExit) \
    macro(AllocatePropertyStorage, NodeMustGenerate | NodeDoesNotExit | NodeResultStorage) \
    macro(ReallocatePropertyStorage, NodeMustGenerate | NodeDoesNotExit | NodeResultStorage) \
    macro(GetButterfly, NodeResultStorage) \
    macro(CheckArray, NodeMustGenerate) \
    macro(Arrayify, NodeMustGenerate) \
    macro(ArrayifyToStructure, NodeMustGenerate) \
    macro(GetIndexedPropertyStorage, NodeResultStorage) \
    macro(ConstantStoragePointer, NodeResultStorage) \
    macro(TypedArrayWatchpoint, NodeMustGenerate) \
    macro(GetByOffset, NodeResultJS) \
    macro(MultiGetByOffset, NodeResultJS) \
    macro(PutByOffset, NodeMustGenerate) \
    macro(MultiPutByOffset, NodeMustGenerate) \
    macro(GetArrayLength, NodeResultInt32) \
    macro(GetTypedArrayByteOffset, NodeResultInt32) \
    macro(GetScope, NodeResultJS) \
    macro(GetMyScope, NodeResultJS) \
    macro(SkipTopScope, NodeResultJS) \
    macro(SkipScope, NodeResultJS) \
    macro(GetClosureRegisters, NodeResultStorage) \
    macro(GetClosureVar, NodeResultJS) \
    macro(PutClosureVar, NodeMustGenerate) \
    macro(GetGlobalVar, NodeResultJS) \
    macro(PutGlobalVar, NodeMustGenerate) \
    macro(NotifyWrite, NodeMustGenerate) \
    macro(VariableWatchpoint, NodeMustGenerate) \
    macro(VarInjectionWatchpoint, NodeMustGenerate) \
    macro(FunctionReentryWatchpoint, NodeMustGenerate) \
    macro(CheckFunction, NodeMustGenerate) \
    macro(AllocationProfileWatchpoint, NodeMustGenerate) \
    macro(CheckInBounds, NodeMustGenerate) \
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
    macro(StringFromCharCode, NodeResultJS) \
    \
    /* Nodes for comparison operations. */\
    macro(CompareLess, NodeResultBoolean | NodeMustGenerate | NodeClobbersWorld) \
    macro(CompareLessEq, NodeResultBoolean | NodeMustGenerate | NodeClobbersWorld) \
    macro(CompareGreater, NodeResultBoolean | NodeMustGenerate | NodeClobbersWorld) \
    macro(CompareGreaterEq, NodeResultBoolean | NodeMustGenerate | NodeClobbersWorld) \
    macro(CompareEq, NodeResultBoolean | NodeMustGenerate | NodeClobbersWorld) \
    macro(CompareEqConstant, NodeResultBoolean) \
    macro(CompareStrictEq, NodeResultBoolean) \
    \
    /* Calls. */\
    macro(Call, NodeResultJS | NodeMustGenerate | NodeHasVarArgs | NodeClobbersWorld) \
    macro(Construct, NodeResultJS | NodeMustGenerate | NodeHasVarArgs | NodeClobbersWorld) \
    \
    /* Allocations. */\
    macro(NewObject, NodeResultJS) \
    macro(NewArray, NodeResultJS | NodeHasVarArgs) \
    macro(NewArrayWithSize, NodeResultJS | NodeMustGenerate) \
    macro(NewArrayBuffer, NodeResultJS) \
    macro(NewTypedArray, NodeResultJS | NodeClobbersWorld | NodeMustGenerate) \
    macro(NewRegexp, NodeResultJS) \
    \
    /* Nodes for misc operations. */\
    macro(Breakpoint, NodeMustGenerate) \
    macro(ProfileWillCall, NodeMustGenerate) \
    macro(ProfileDidCall, NodeMustGenerate) \
    macro(CheckHasInstance, NodeMustGenerate) \
    macro(InstanceOf, NodeResultBoolean) \
    macro(IsUndefined, NodeResultBoolean) \
    macro(IsBoolean, NodeResultBoolean) \
    macro(IsNumber, NodeResultBoolean) \
    macro(IsString, NodeResultBoolean) \
    macro(IsObject, NodeResultBoolean) \
    macro(IsFunction, NodeResultBoolean) \
    macro(TypeOf, NodeResultJS) \
    macro(LogicalNot, NodeResultBoolean) \
    macro(ToPrimitive, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(ToString, NodeResultJS | NodeMustGenerate | NodeMightClobber) \
    macro(NewStringObject, NodeResultJS) \
    macro(MakeRope, NodeResultJS) \
    macro(In, NodeResultBoolean | NodeMustGenerate | NodeClobbersWorld) \
    \
    /* Nodes used for activations. Activation support works by having it anchored at */\
    /* epilgoues via TearOffActivation, and all CreateActivation nodes kept alive by */\
    /* being threaded with each other. */\
    macro(CreateActivation, NodeResultJS) \
    macro(TearOffActivation, NodeMustGenerate) \
    \
    /* Nodes used for arguments. Similar to activation support, only it makes even less */\
    /* sense. */\
    macro(CreateArguments, NodeResultJS) \
    macro(PhantomArguments, NodeResultJS | NodeDoesNotExit) \
    macro(TearOffArguments, NodeMustGenerate) \
    macro(GetMyArgumentsLength, NodeResultJS | NodeMustGenerate) \
    macro(GetMyArgumentByVal, NodeResultJS | NodeMustGenerate) \
    macro(GetMyArgumentsLengthSafe, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(GetMyArgumentByValSafe, NodeResultJS | NodeMustGenerate | NodeClobbersWorld) \
    macro(CheckArgumentsNotCreated, NodeMustGenerate) \
    \
    /* Nodes for creating functions. */\
    macro(NewFunctionNoCheck, NodeResultJS) \
    macro(NewFunction, NodeResultJS) \
    macro(NewFunctionExpression, NodeResultJS) \
    \
    /* These aren't terminals but always exit */ \
    macro(Throw, NodeMustGenerate) \
    macro(ThrowReferenceError, NodeMustGenerate) \
    \
    /* Block terminals. */\
    macro(Jump, NodeMustGenerate) \
    macro(Branch, NodeMustGenerate) \
    macro(Switch, NodeMustGenerate) \
    macro(Return, NodeMustGenerate) \
    macro(Unreachable, NodeMustGenerate) \
    \
    /* Count execution. */\
    macro(CountExecution, NodeMustGenerate) \
    \
    /* This is a pseudo-terminal. It means that execution should fall out of DFG at */\
    /* this point, but execution does continue in the basic block - just in a */\
    /* different compiler. */\
    macro(ForceOSRExit, NodeMustGenerate) \
    \
    /* Checks the watchdog timer. If the timer has fired, we OSR exit to the */ \
    /* baseline JIT to redo the watchdog timer check, and service the timer. */ \
    macro(CheckWatchdogTimer, NodeMustGenerate) \
    /* Write barriers ! */\
    macro(StoreBarrier, NodeMustGenerate) \
    macro(StoreBarrierWithNullCheck, NodeMustGenerate) \

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
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGNodeType_h

