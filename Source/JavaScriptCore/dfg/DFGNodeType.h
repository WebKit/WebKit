/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(DFG_JIT)

#include "DFGNodeFlags.h"

namespace JSC { namespace DFG {

// This macro defines a set of information about all known node types, used to populate NodeId, NodeType below.
#define FOR_EACH_DFG_OP(macro) \
    /* A constant in the CodeBlock's constant pool. */\
    macro(JSConstant, NodeResultJS) \
    \
    /* Constants with specific representations. */\
    macro(DoubleConstant, NodeResultDouble) \
    macro(Int52Constant, NodeResultInt52) \
    \
    /* Lazy JSValue constant. We don't know the JSValue bits of it yet. */\
    macro(LazyJSConstant, NodeResultJS) \
    \
    /* Marker to indicate that an operation was optimized entirely and all that is left */\
    /* is to make one node alias another. CSE will later usually eliminate this node, */\
    /* though it may choose not to if it would corrupt predictions (very rare). */\
    macro(Identity, NodeResultJS) \
    /* Used for debugging to force a profile to appear as anything we want. */ \
    macro(IdentityWithProfile, NodeResultJS | NodeMustGenerate) \
    \
    /* Nodes for handling functions (both as call and as construct). */\
    macro(ToThis, NodeResultJS) \
    macro(CreateThis, NodeResultJS) /* Note this is not MustGenerate since we're returning it anyway. */ \
    macro(CreatePromise, NodeResultJS | NodeMustGenerate) \
    macro(CreateGenerator, NodeResultJS | NodeMustGenerate) \
    macro(CreateAsyncGenerator, NodeResultJS | NodeMustGenerate) \
    macro(GetCallee, NodeResultJS) \
    macro(SetCallee, NodeMustGenerate) \
    macro(GetArgumentCountIncludingThis, NodeResultInt32) \
    macro(SetArgumentCountIncludingThis, NodeMustGenerate) \
    \
    /* Nodes for local variable access. These nodes are linked together using Phi nodes. */\
    /* Any two nodes that are part of the same Phi graph will share the same */\
    /* VariableAccessData, and thus will share predictions. FIXME: We should come up with */\
    /* better names for a lot of these. https://bugs.webkit.org/show_bug.cgi?id=137307. */\
    /* Note that GetLocal is MustGenerate because it's our only way of knowing that some other */\
    /* basic block might have read a local variable in bytecode. We only remove GetLocals if it */\
    /* is redundant because of an earlier GetLocal or SetLocal in the same block. We could make */\
    /* these not MustGenerate and use a more sophisticated analysis to insert PhantomLocals in */\
    /* the same way that we insert Phantoms. That's hard and probably not profitable. See */\
    /* https://bugs.webkit.org/show_bug.cgi?id=144086 */\
    macro(GetLocal, NodeResultJS | NodeMustGenerate) \
    macro(SetLocal, 0) \
    \
    /* These are used in SSA form to represent to track */\
    macro(PutStack, NodeMustGenerate) \
    macro(KillStack, NodeMustGenerate) \
    macro(GetStack, NodeResultJS) \
    \
    macro(MovHint, NodeMustGenerate) \
    macro(ZombieHint, NodeMustGenerate) \
    macro(ExitOK, NodeMustGenerate) /* Indicates that exit state is intact and it is safe to exit back to the beginning of the exit origin. */ \
    macro(Phantom, NodeMustGenerate) \
    macro(Check, NodeMustGenerate) /* Used if we want just a type check but not liveness. Non-checking uses will be removed. */\
    macro(CheckVarargs, NodeMustGenerate | NodeHasVarArgs) /* Used if we want just a type check but not liveness. Non-checking uses will be removed. */\
    macro(Upsilon, 0) \
    macro(Phi, 0) \
    macro(Flush, NodeMustGenerate) \
    macro(PhantomLocal, NodeMustGenerate) \
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
    macro(ExtractCatchLocal, NodeResultJS) \
    macro(ClearCatchLocals, NodeMustGenerate) \
    \
    /* Tier-up checks from the DFG to the FTL. */\
    macro(CheckTierUpInLoop, NodeMustGenerate) \
    macro(CheckTierUpAndOSREnter, NodeMustGenerate) \
    macro(CheckTierUpAtReturn, NodeMustGenerate) \
    \
    /* Marker for an argument being set at the prologue of a function. The argument is guaranteed to be set after this node. */\
    macro(SetArgumentDefinitely, 0) \
    /* A marker like the above that we use to track variable liveness and OSR exit state. However, it's not guaranteed to be set. To verify it was set, you'd need to check the actual argument length. We use this for varargs when we're unsure how many argument may actually end up on the stack. */\
    macro(SetArgumentMaybe, 0) \
    \
    /* Marker of location in the IR where we may possibly perform jump replacement to */\
    /* invalidate this code block. */\
    macro(InvalidationPoint, NodeMustGenerate) \
    \
    /* Nodes for bitwise operations. */\
    macro(ValueBitNot, NodeResultJS | NodeMustGenerate) \
    macro(ArithBitNot, NodeResultInt32) \
    macro(ValueBitAnd, NodeResultJS | NodeMustGenerate) \
    macro(ArithBitAnd, NodeResultInt32) \
    macro(ValueBitOr, NodeResultJS | NodeMustGenerate) \
    macro(ArithBitOr, NodeResultInt32) \
    macro(ValueBitXor, NodeResultJS | NodeMustGenerate) \
    macro(ArithBitXor, NodeResultInt32) \
    macro(ArithBitLShift, NodeResultInt32) \
    macro(ValueBitLShift, NodeResultJS | NodeMustGenerate) \
    macro(ArithBitRShift, NodeResultInt32) \
    macro(ValueBitRShift, NodeResultJS | NodeMustGenerate) \
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
    /* Nodes for arithmetic operations. Note that if they do checks other than just type checks, */\
    /* then they are MustGenerate. This is probably stricter than it needs to be - for example */\
    /* they won't do checks if they are speculated double. Also, we could kill these if we do it */\
    /* before AI starts eliminating downstream operations based on proofs, for example in the */\
    /* case of "var tmp = a + b; return (tmp | 0) == tmp;". If a, b are speculated integer then */\
    /* this is only true if we do the overflow check - hence the need to keep it alive. More */\
    /* generally, we need to keep alive any operation whose checks cause filtration in AI. */\
    macro(ArithAdd, NodeResultNumber | NodeMustGenerate) \
    macro(ArithClz32, NodeResultInt32 | NodeMustGenerate) \
    macro(ArithSub, NodeResultNumber | NodeMustGenerate) \
    macro(ArithNegate, NodeResultNumber | NodeMustGenerate) \
    macro(ArithMul, NodeResultNumber | NodeMustGenerate) \
    macro(ArithIMul, NodeResultInt32) \
    macro(ArithDiv, NodeResultNumber | NodeMustGenerate) \
    macro(ArithMod, NodeResultNumber | NodeMustGenerate) \
    macro(ArithAbs, NodeResultNumber | NodeMustGenerate) \
    macro(ArithMin, NodeResultNumber) \
    macro(ArithMax, NodeResultNumber) \
    macro(ArithFRound, NodeResultDouble | NodeMustGenerate) \
    macro(ArithPow, NodeResultDouble) \
    macro(ArithRandom, NodeResultDouble | NodeMustGenerate) \
    macro(ArithRound, NodeResultNumber | NodeMustGenerate) \
    macro(ArithFloor, NodeResultNumber | NodeMustGenerate) \
    macro(ArithCeil, NodeResultNumber | NodeMustGenerate) \
    macro(ArithTrunc, NodeResultNumber | NodeMustGenerate) \
    macro(ArithSqrt, NodeResultDouble | NodeMustGenerate) \
    macro(ArithUnary, NodeResultDouble | NodeMustGenerate) \
    \
    /* BigInt is a valid operand for these three operations */\
    /* Inc and Dec don't update their operand in-place, they are typically combined with some form of SetLocal */\
    macro(Inc, NodeResultJS | NodeMustGenerate) \
    macro(Dec, NodeResultJS | NodeMustGenerate) \
    macro(ValueNegate, NodeResultJS | NodeMustGenerate) \
    \
    /* Add of values may either be arithmetic, or result in string concatenation. */\
    macro(ValueAdd, NodeResultJS | NodeMustGenerate) \
    \
    macro(ValueSub, NodeResultJS | NodeMustGenerate) \
    macro(ValueMul, NodeResultJS | NodeMustGenerate) \
    macro(ValueDiv, NodeResultJS | NodeMustGenerate) \
    macro(ValuePow, NodeResultJS | NodeMustGenerate) \
    macro(ValueMod, NodeResultJS | NodeMustGenerate) \
    \
    /* Add of values that always convers its inputs to strings. May have two or three kids. */\
    macro(StrCat, NodeResultJS | NodeMustGenerate) \
    \
    /* Property access. */\
    /* PutByValAlias indicates a 'put' aliases a prior write to the same property. */\
    /* Since a put to 'length' may invalidate optimizations here, */\
    /* this must be the directly subsequent property put. Note that PutByVal */\
    /* opcodes use VarArgs beause they may have up to 4 children. */\
    macro(GetByVal, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(GetByValWithThis, NodeResultJS | NodeMustGenerate) \
    macro(GetMyArgumentByVal, NodeResultJS | NodeMustGenerate) \
    macro(GetMyArgumentByValOutOfBounds, NodeResultJS | NodeMustGenerate) \
    macro(VarargsLength, NodeMustGenerate | NodeResultInt32) \
    macro(LoadVarargs, NodeMustGenerate) \
    macro(ForwardVarargs, NodeMustGenerate) \
    macro(PutByValDirect, NodeMustGenerate | NodeHasVarArgs) \
    macro(PutByVal, NodeMustGenerate | NodeHasVarArgs) \
    macro(PutByValAlias, NodeMustGenerate | NodeHasVarArgs) \
    macro(TryGetById, NodeResultJS) \
    macro(GetById, NodeResultJS | NodeMustGenerate) \
    macro(GetByIdFlush, NodeResultJS | NodeMustGenerate) \
    macro(GetByIdWithThis, NodeResultJS | NodeMustGenerate) \
    macro(GetByIdDirect, NodeResultJS | NodeMustGenerate) \
    macro(GetByIdDirectFlush, NodeResultJS | NodeMustGenerate) \
    macro(PutById, NodeMustGenerate) \
    macro(PutByIdFlush, NodeMustGenerate) \
    macro(PutByIdDirect, NodeMustGenerate) \
    macro(PutByIdWithThis, NodeMustGenerate) \
    macro(PutByValWithThis, NodeMustGenerate | NodeHasVarArgs) \
    macro(PutGetterById, NodeMustGenerate) \
    macro(PutSetterById, NodeMustGenerate) \
    macro(PutGetterSetterById, NodeMustGenerate) \
    macro(PutGetterByVal, NodeMustGenerate) \
    macro(PutSetterByVal, NodeMustGenerate) \
    macro(DefineDataProperty, NodeMustGenerate | NodeHasVarArgs) \
    macro(DefineAccessorProperty, NodeMustGenerate | NodeHasVarArgs) \
    macro(DeleteById, NodeResultBoolean | NodeMustGenerate) \
    macro(DeleteByVal, NodeResultBoolean | NodeMustGenerate) \
    macro(CheckStructure, NodeMustGenerate) \
    macro(CheckStructureOrEmpty, NodeMustGenerate) \
    macro(GetExecutable, NodeResultJS) \
    macro(PutStructure, NodeMustGenerate) \
    macro(AllocatePropertyStorage, NodeMustGenerate | NodeResultStorage) \
    macro(ReallocatePropertyStorage, NodeMustGenerate | NodeResultStorage) \
    macro(GetButterfly, NodeResultStorage) \
    macro(NukeStructureAndSetButterfly, NodeMustGenerate) \
    macro(CheckArray, NodeMustGenerate) \
    macro(CheckArrayOrEmpty, NodeMustGenerate) \
    /* This checks if the edge is a typed array and if it is neutered. */ \
    macro(CheckNeutered, NodeMustGenerate) \
    macro(Arrayify, NodeMustGenerate) \
    macro(ArrayifyToStructure, NodeMustGenerate) \
    macro(GetIndexedPropertyStorage, NodeResultStorage) \
    macro(ConstantStoragePointer, NodeResultStorage) \
    macro(GetGetter, NodeResultJS) \
    macro(GetSetter, NodeResultJS) \
    macro(GetByOffset, NodeResultJS) \
    macro(GetGetterSetterByOffset, NodeResultJS) \
    macro(MultiGetByOffset, NodeResultJS | NodeMustGenerate) \
    macro(PutByOffset, NodeMustGenerate) \
    macro(MultiPutByOffset, NodeMustGenerate) \
    macro(MultiDeleteByOffset, NodeMustGenerate | NodeResultJS) \
    macro(GetArrayLength, NodeResultInt32) \
    macro(GetVectorLength, NodeResultInt32) \
    macro(GetTypedArrayByteOffset, NodeResultInt32) \
    macro(GetScope, NodeResultJS) \
    macro(SkipScope, NodeResultJS) \
    macro(ResolveScope, NodeResultJS | NodeMustGenerate) \
    macro(ResolveScopeForHoistingFuncDeclInEval, NodeResultJS | NodeMustGenerate) \
    macro(GetGlobalObject, NodeResultJS) \
    macro(GetGlobalThis, NodeResultJS) \
    macro(GetClosureVar, NodeResultJS) \
    macro(PutClosureVar, NodeMustGenerate) \
    macro(GetGlobalVar, NodeResultJS) \
    macro(GetGlobalLexicalVariable, NodeResultJS) \
    macro(PutGlobalVariable, NodeMustGenerate) \
    macro(GetDynamicVar, NodeResultJS | NodeMustGenerate) \
    macro(PutDynamicVar, NodeMustGenerate) \
    macro(NotifyWrite, NodeMustGenerate) \
    macro(GetRegExpObjectLastIndex, NodeResultJS) \
    macro(SetRegExpObjectLastIndex, NodeMustGenerate) \
    macro(RecordRegExpCachedResult, NodeMustGenerate | NodeHasVarArgs) \
    macro(CheckIsConstant, NodeMustGenerate) \
    macro(CheckNotEmpty, NodeMustGenerate) \
    macro(AssertNotEmpty, NodeMustGenerate) \
    macro(CheckBadCell, NodeMustGenerate) \
    macro(CheckInBounds, NodeMustGenerate | NodeResultJS) \
    macro(CheckIdent, NodeMustGenerate) \
    macro(CheckTypeInfoFlags, NodeMustGenerate) /* Takes an OpInfo with the flags you want to test are set */\
    macro(CheckSubClass, NodeMustGenerate) \
    macro(ParseInt, NodeMustGenerate | NodeResultJS) \
    macro(GetPrototypeOf, NodeMustGenerate | NodeResultJS) \
    macro(ObjectCreate, NodeMustGenerate | NodeResultJS) \
    macro(ObjectKeys, NodeMustGenerate | NodeResultJS) \
    \
    /* Atomics object functions. */\
    macro(AtomicsAdd, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(AtomicsAnd, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(AtomicsCompareExchange, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(AtomicsExchange, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(AtomicsIsLockFree, NodeResultBoolean) \
    macro(AtomicsLoad, NodeResultJS | NodeMustGenerate) \
    macro(AtomicsOr, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(AtomicsStore, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(AtomicsSub, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(AtomicsXor, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    \
    /* Optimizations for array mutation. */\
    macro(ArrayPush, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(ArrayPop, NodeResultJS | NodeMustGenerate) \
    macro(ArraySlice, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(ArrayIndexOf, NodeResultInt32 | NodeHasVarArgs) \
    \
    /* Optimizations for regular expression matching. */\
    macro(RegExpExec, NodeResultJS | NodeMustGenerate) \
    macro(RegExpExecNonGlobalOrSticky, NodeResultJS) \
    macro(RegExpTest, NodeResultJS | NodeMustGenerate) \
    macro(RegExpMatchFast, NodeResultJS | NodeMustGenerate) \
    macro(RegExpMatchFastGlobal, NodeResultJS) \
    macro(StringReplace, NodeResultJS | NodeMustGenerate) \
    macro(StringReplaceRegExp, NodeResultJS | NodeMustGenerate) \
    \
    /* Optimizations for string access */ \
    macro(StringCharCodeAt, NodeResultInt32) \
    macro(StringCodePointAt, NodeResultInt32) \
    macro(StringCharAt, NodeResultJS) \
    macro(StringFromCharCode, NodeResultJS | NodeMustGenerate) \
    \
    /* Nodes for comparison operations. */\
    macro(CompareLess, NodeResultBoolean | NodeMustGenerate) \
    macro(CompareLessEq, NodeResultBoolean | NodeMustGenerate) \
    macro(CompareGreater, NodeResultBoolean | NodeMustGenerate) \
    macro(CompareGreaterEq, NodeResultBoolean | NodeMustGenerate) \
    macro(CompareBelow, NodeResultBoolean) \
    macro(CompareBelowEq, NodeResultBoolean) \
    macro(CompareEq, NodeResultBoolean | NodeMustGenerate) \
    macro(CompareStrictEq, NodeResultBoolean) \
    macro(CompareEqPtr, NodeResultBoolean) \
    macro(SameValue, NodeResultBoolean) \
    \
    /* Calls. */\
    macro(Call, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(DirectCall, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(Construct, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(DirectConstruct, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(CallVarargs, NodeResultJS | NodeMustGenerate) \
    macro(CallForwardVarargs, NodeResultJS | NodeMustGenerate) \
    macro(ConstructVarargs, NodeResultJS | NodeMustGenerate) \
    macro(ConstructForwardVarargs, NodeResultJS | NodeMustGenerate) \
    macro(TailCallInlinedCaller, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(DirectTailCallInlinedCaller, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(TailCallVarargsInlinedCaller, NodeResultJS | NodeMustGenerate) \
    macro(TailCallForwardVarargsInlinedCaller, NodeResultJS | NodeMustGenerate) \
    macro(CallEval, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    \
    /* Shadow Chicken */\
    macro(LogShadowChickenPrologue, NodeMustGenerate) \
    macro(LogShadowChickenTail, NodeMustGenerate) \
    \
    /* Allocations. */\
    macro(NewObject, NodeResultJS) \
    macro(NewGenerator, NodeResultJS) \
    macro(NewAsyncGenerator, NodeResultJS) \
    macro(NewArray, NodeResultJS | NodeHasVarArgs) \
    macro(NewArrayWithSpread, NodeResultJS | NodeHasVarArgs) \
    macro(NewArrayWithSize, NodeResultJS | NodeMustGenerate) \
    macro(NewArrayBuffer, NodeResultJS) \
    macro(NewInternalFieldObject, NodeResultJS) \
    macro(NewTypedArray, NodeResultJS | NodeMustGenerate) \
    macro(NewRegexp, NodeResultJS) \
    macro(NewSymbol, NodeResultJS) \
    macro(NewStringObject, NodeResultJS) \
    /* Rest Parameter */\
    macro(GetRestLength, NodeResultInt32) \
    macro(CreateRest, NodeResultJS | NodeMustGenerate) \
    \
    macro(Spread, NodeResultJS | NodeMustGenerate) \
    /* Support for allocation sinking. */\
    macro(PhantomNewObject, NodeResultJS | NodeMustGenerate) \
    macro(PutHint, NodeMustGenerate) \
    macro(CheckStructureImmediate, NodeMustGenerate) \
    macro(MaterializeNewObject, NodeResultJS | NodeHasVarArgs) \
    macro(PhantomNewFunction, NodeResultJS | NodeMustGenerate) \
    macro(PhantomNewGeneratorFunction, NodeResultJS | NodeMustGenerate) \
    macro(PhantomNewAsyncFunction, NodeResultJS | NodeMustGenerate) \
    macro(PhantomNewAsyncGeneratorFunction, NodeResultJS | NodeMustGenerate) \
    macro(PhantomNewInternalFieldObject, NodeResultJS | NodeMustGenerate) \
    macro(MaterializeNewInternalFieldObject, NodeResultJS | NodeHasVarArgs) \
    macro(PhantomCreateActivation, NodeResultJS | NodeMustGenerate) \
    macro(MaterializeCreateActivation, NodeResultJS | NodeHasVarArgs) \
    macro(PhantomNewRegexp, NodeResultJS | NodeMustGenerate) \
    \
    /* Nodes for misc operations. */\
    macro(OverridesHasInstance, NodeMustGenerate | NodeResultBoolean) \
    macro(InstanceOf, NodeMustGenerate | NodeResultBoolean) \
    macro(InstanceOfCustom, NodeMustGenerate | NodeResultBoolean) \
    macro(MatchStructure, NodeMustGenerate | NodeResultBoolean) \
    \
    macro(IsCellWithType, NodeResultBoolean) \
    macro(IsEmpty, NodeResultBoolean) \
    macro(IsUndefined, NodeResultBoolean) \
    macro(IsUndefinedOrNull, NodeResultBoolean) \
    macro(IsBoolean, NodeResultBoolean) \
    macro(IsNumber, NodeResultBoolean) \
    /* IsBigInt is only used when USE_BIGINT32. Otherwise we emit IsCellWithType */\
    macro(IsBigInt, NodeResultBoolean) \
    macro(NumberIsInteger, NodeResultBoolean) \
    macro(IsObject, NodeResultBoolean) \
    macro(IsObjectOrNull, NodeResultBoolean) \
    macro(IsFunction, NodeResultBoolean) \
    macro(IsConstructor, NodeResultBoolean) \
    macro(IsTypedArrayView, NodeResultBoolean) \
    macro(TypeOf, NodeResultJS) \
    macro(LogicalNot, NodeResultBoolean) \
    macro(ToPrimitive, NodeResultJS | NodeMustGenerate) \
    macro(ToPropertyKey, NodeResultJS | NodeMustGenerate) \
    macro(ToString, NodeResultJS | NodeMustGenerate) \
    macro(ToNumber, NodeResultJS | NodeMustGenerate) \
    macro(ToNumeric, NodeResultJS | NodeMustGenerate) \
    macro(ToObject, NodeResultJS | NodeMustGenerate) \
    macro(CallObjectConstructor, NodeResultJS) \
    macro(CallStringConstructor, NodeResultJS | NodeMustGenerate) \
    macro(CallNumberConstructor, NodeResultJS | NodeMustGenerate) \
    macro(NumberToStringWithRadix, NodeResultJS | NodeMustGenerate) \
    macro(NumberToStringWithValidRadixConstant, NodeResultJS) \
    macro(MakeRope, NodeResultJS) \
    macro(InByVal, NodeResultBoolean | NodeMustGenerate) \
    macro(InById, NodeResultBoolean | NodeMustGenerate) \
    macro(ProfileType, NodeMustGenerate) \
    macro(ProfileControlFlow, NodeMustGenerate) \
    macro(SetFunctionName, NodeMustGenerate) \
    macro(HasOwnProperty, NodeResultBoolean) \
    \
    macro(GetInternalField, NodeResultJS) \
    macro(PutInternalField, NodeMustGenerate) \
    \
    macro(CreateActivation, NodeResultJS) \
    macro(PushWithScope, NodeResultJS | NodeMustGenerate) \
    \
    macro(CreateDirectArguments, NodeResultJS) \
    macro(PhantomDirectArguments, NodeResultJS | NodeMustGenerate) \
    macro(PhantomCreateRest, NodeResultJS | NodeMustGenerate) \
    macro(PhantomSpread, NodeResultJS | NodeMustGenerate) \
    macro(PhantomNewArrayWithSpread, NodeResultJS | NodeMustGenerate | NodeHasVarArgs) \
    macro(PhantomNewArrayBuffer, NodeResultJS | NodeMustGenerate) \
    macro(CreateScopedArguments, NodeResultJS) \
    macro(CreateClonedArguments, NodeResultJS) \
    macro(PhantomClonedArguments, NodeResultJS | NodeMustGenerate) \
    macro(CreateArgumentsButterfly, NodeResultJS) \
    macro(GetFromArguments, NodeResultJS) \
    macro(PutToArguments, NodeMustGenerate) \
    macro(GetArgument, NodeResultJS) \
    \
    macro(NewFunction, NodeResultJS) \
    macro(NewGeneratorFunction, NodeResultJS) \
    macro(NewAsyncGeneratorFunction, NodeResultJS) \
    macro(NewAsyncFunction, NodeResultJS) \
    \
    /* Block terminals. */\
    macro(Jump, NodeMustGenerate) \
    macro(Branch, NodeMustGenerate) \
    macro(Switch, NodeMustGenerate) \
    macro(EntrySwitch, NodeMustGenerate) \
    macro(Return, NodeMustGenerate) \
    macro(TailCall, NodeMustGenerate | NodeHasVarArgs) \
    macro(DirectTailCall, NodeMustGenerate | NodeHasVarArgs) \
    macro(TailCallVarargs, NodeMustGenerate) \
    macro(TailCallForwardVarargs, NodeMustGenerate) \
    macro(Unreachable, NodeMustGenerate) \
    macro(Throw, NodeMustGenerate) \
    macro(ThrowStaticError, NodeMustGenerate) \
    \
    /* Count execution. */\
    macro(CountExecution, NodeMustGenerate) \
    /* Super sampler. */\
    macro(SuperSamplerBegin, NodeMustGenerate) \
    macro(SuperSamplerEnd, NodeMustGenerate) \
    \
    /* This is a pseudo-terminal. It means that execution should fall out of DFG at */\
    /* this point, but execution does continue in the basic block - just in a */\
    /* different compiler. */\
    macro(ForceOSRExit, NodeMustGenerate) \
    \
    /* Vends a bottom JS value. It is invalid to ever execute this. Useful for cases */\
    /* where we know that we would have exited but we'd like to still track the control */\
    /* flow. */\
    macro(BottomValue, NodeResultJS) \
    \
    /* Checks for VM traps. If there is a trap, we'll jettison or call operation operationHandleTraps. */ \
    macro(CheckTraps, NodeMustGenerate) \
    /* Write barriers */\
    macro(StoreBarrier, NodeMustGenerate) \
    macro(FencedStoreBarrier, NodeMustGenerate) \
    \
    /* For-in enumeration opcodes */\
    macro(GetEnumerableLength, NodeMustGenerate | NodeResultJS) \
    /* Must generate because of Proxies on the prototype chain */ \
    macro(HasIndexedProperty, NodeMustGenerate | NodeResultBoolean | NodeHasVarArgs) \
    macro(HasStructureProperty, NodeResultBoolean) \
    macro(HasOwnStructureProperty, NodeResultBoolean | NodeMustGenerate) \
    macro(InStructureProperty, NodeMustGenerate | NodeResultBoolean) \
    macro(HasGenericProperty, NodeResultBoolean) \
    macro(GetDirectPname, NodeMustGenerate | NodeHasVarArgs | NodeResultJS) \
    macro(GetPropertyEnumerator, NodeMustGenerate | NodeResultJS) \
    macro(GetEnumeratorStructurePname, NodeMustGenerate | NodeResultJS) \
    macro(GetEnumeratorGenericPname, NodeMustGenerate | NodeResultJS) \
    macro(ToIndexString, NodeResultJS) \
    /* Nodes for JSMap and JSSet */ \
    macro(MapHash, NodeResultInt32) \
    macro(NormalizeMapKey, NodeResultJS) \
    macro(GetMapBucket, NodeResultJS) \
    macro(GetMapBucketHead, NodeResultJS) \
    macro(GetMapBucketNext, NodeResultJS) \
    macro(LoadKeyFromMapBucket, NodeResultJS) \
    macro(LoadValueFromMapBucket, NodeResultJS) \
    macro(SetAdd, NodeMustGenerate | NodeResultJS) \
    macro(MapSet, NodeMustGenerate | NodeHasVarArgs | NodeResultJS) \
    /* Nodes for JSWeakMap and JSWeakSet */ \
    macro(WeakMapGet, NodeResultJS) \
    macro(WeakSetAdd, NodeMustGenerate) \
    macro(WeakMapSet, NodeMustGenerate | NodeHasVarArgs) \
    macro(ExtractValueFromWeakMapGet, NodeResultJS) \
    \
    macro(StringValueOf, NodeMustGenerate | NodeResultJS) \
    macro(StringSlice, NodeResultJS) \
    macro(ToLowerCase, NodeResultJS) \
    /* Nodes for DOM JIT */\
    macro(CallDOMGetter, NodeResultJS | NodeMustGenerate) \
    macro(CallDOM, NodeResultJS | NodeMustGenerate) \
    /* Metadata node that initializes the state for flushed argument types at an entrypoint in the program. */ \
    /* Currently, we only use this for the blocks an EntrySwitch branches to at the root of the program. */ \
    /* This is only used in SSA. */ \
    macro(InitializeEntrypointArguments, NodeMustGenerate) \
    \
    /* Used for $vm performance debugging */ \
    macro(CPUIntrinsic, NodeResultJS | NodeMustGenerate) \
    \
    /* Used to provide feedback to the IC profiler. */ \
    macro(FilterCallLinkStatus, NodeMustGenerate) \
    macro(FilterGetByStatus, NodeMustGenerate) \
    macro(FilterInByIdStatus, NodeMustGenerate) \
    macro(FilterPutByIdStatus, NodeMustGenerate) \
    macro(FilterDeleteByStatus, NodeMustGenerate) \
    /* Data view access */ \
    macro(DataViewGetInt, NodeMustGenerate | NodeResultJS) /* The gets are must generate for now because they do bounds checks */ \
    macro(DataViewGetFloat, NodeMustGenerate | NodeResultDouble) \
    macro(DataViewSet, NodeMustGenerate | NodeMustGenerate | NodeHasVarArgs) \
    /* Date access */ \
    macro(DateGetInt32OrNaN, NodeResultJS) \
    macro(DateGetTime, NodeResultDouble) \


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

inline bool isAtomicsIntrinsic(NodeType op)
{
    switch (op) {
    case AtomicsAdd:
    case AtomicsAnd:
    case AtomicsCompareExchange:
    case AtomicsExchange:
    case AtomicsLoad:
    case AtomicsOr:
    case AtomicsStore:
    case AtomicsSub:
    case AtomicsXor:
    case AtomicsIsLockFree:
        return true;
    default:
        return false;
    }
}

static constexpr unsigned maxNumExtraAtomicsArgs = 2;

inline unsigned numExtraAtomicsArgs(NodeType op)
{
    switch (op) {
    case AtomicsLoad:
        return 0;
    case AtomicsAdd:
    case AtomicsAnd:
    case AtomicsExchange:
    case AtomicsOr:
    case AtomicsStore:
    case AtomicsSub:
    case AtomicsXor:
        return 1;
    case AtomicsCompareExchange:
        return 2;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
