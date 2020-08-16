/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#include "DFGDoesGC.h"

#if ENABLE(DFG_JIT)

#include "DFGClobberize.h"
#include "DFGGraph.h"
#include "DFGNode.h"

namespace JSC { namespace DFG {

bool doesGC(Graph& graph, Node* node)
{
    if (clobbersHeap(graph, node))
        return true;
    
    // Now consider nodes that don't clobber the world but that still may GC. This includes all
    // nodes. By default, we should assume every node can GC and return true. This includes the
    // world-clobbering nodes. We should only return false if we have proven that the node cannot
    // GC. Typical examples of how a node can GC is if the code emitted for the node does any of the
    // following:
    //     1. Allocates any objects.
    //     2. Resolves a rope string, which allocates a string.
    //     3. Produces a string (which allocates the string) except when we can prove that
    //        the string will always be one of the pre-allcoated SmallStrings.
    //     4. Triggers a structure transition (which can allocate a new structure)
    //        unless it is a known transition between previously allocated structures
    //        such as between Array types.
    //     5. Calls to a JS function, which can execute arbitrary code including allocating objects.
    //     6. Calls operations that uses DeferGC, because it may GC in its destructor.

    switch (node->op()) {
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
    case LazyJSConstant:
    case Identity:
    case IdentityWithProfile:
    case GetCallee:
    case SetCallee:
    case GetArgumentCountIncludingThis:
    case SetArgumentCountIncludingThis:
    case GetRestLength:
    case GetLocal:
    case SetLocal:
    case MovHint:
    case InitializeEntrypointArguments:
    case ZombieHint:
    case ExitOK:
    case Phantom:
    case Upsilon:
    case Phi:
    case Flush:
    case PhantomLocal:
    case SetArgumentDefinitely:
    case SetArgumentMaybe:
    case ArithBitNot:
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitXor:
    case ArithBitLShift:
    case ArithBitRShift:
    case BitURShift:
    case ValueToInt32:
    case UInt32ToNumber:
    case DoubleAsInt32:
    case ArithAdd:
    case ArithClz32:
    case ArithSub:
    case ArithNegate:
    case ArithMul:
    case ArithIMul:
    case ArithDiv:
    case ArithMod:
    case ArithAbs:
    case ArithMin:
    case ArithMax:
    case ArithPow:
    case ArithSqrt:
    case ArithRandom:
    case ArithRound:
    case ArithFloor:
    case ArithCeil:
    case ArithTrunc:
    case ArithFRound:
    case ArithUnary:
    case CheckStructure:
    case CheckStructureOrEmpty:
    case CheckStructureImmediate:
    case GetExecutable:
    case GetButterfly:
    case CheckJSCast:
    case CheckNotJSCast:
    case CheckArray:
    case CheckArrayOrEmpty:
    case CheckNeutered:
    case GetScope:
    case SkipScope:
    case GetGlobalObject:
    case GetGlobalThis:
    case GetClosureVar:
    case PutClosureVar:
    case GetInternalField:
    case PutInternalField:
    case GetRegExpObjectLastIndex:
    case SetRegExpObjectLastIndex:
    case RecordRegExpCachedResult:
    case GetGlobalVar:
    case GetGlobalLexicalVariable:
    case PutGlobalVariable:
    case CheckIsConstant:
    case CheckNotEmpty:
    case AssertNotEmpty:
    case CheckIdent:
    case CompareBelow:
    case CompareBelowEq:
    case CompareEqPtr:
    case ProfileControlFlow:
    case OverridesHasInstance:
    case IsEmpty:
    case TypeOfIsUndefined:
    case TypeOfIsObject:
    case IsUndefinedOrNull:
    case IsBoolean:
    case IsNumber:
    case IsBigInt:
    case NumberIsInteger:
    case IsObject:
    case IsFunction:
    case IsConstructor:
    case IsCellWithType:
    case IsTypedArrayView:
    case TypeOf:
    case LogicalNot:
    case Jump:
    case Branch:
    case EntrySwitch:
    case CountExecution:
    case SuperSamplerBegin:
    case SuperSamplerEnd:
    case CPUIntrinsic:
    case NormalizeMapKey:
    case GetMapBucketHead:
    case GetMapBucketNext:
    case LoadKeyFromMapBucket:
    case LoadValueFromMapBucket:
    case ExtractValueFromWeakMapGet:
    case WeakMapGet:
    case WeakSetAdd:
    case WeakMapSet:
    case Unreachable:
    case ExtractOSREntryLocal:
    case ExtractCatchLocal:
    case ClearCatchLocals:
    case LoopHint:
    case StoreBarrier:
    case FencedStoreBarrier:
    case InvalidationPoint:
    case NotifyWrite:
    case CheckInBounds:
    case ConstantStoragePointer:
    case Check:
    case CheckVarargs:
    case CheckTypeInfoFlags:
    case MultiGetByOffset:
    case MultiDeleteByOffset:
    case ValueRep:
    case DoubleRep:
    case Int52Rep:
    case GetGetter:
    case GetSetter:
    case GetArrayLength:
    case GetVectorLength:
    case StringCharCodeAt:
    case StringCodePointAt:
    case GetTypedArrayByteOffset:
    case GetPrototypeOf:
    case PutStructure:
    case GetByOffset:
    case GetGetterSetterByOffset:
    case GetEnumerableLength:
    case FiatInt52:
    case BooleanToNumber:
    case CheckBadValue:
    case BottomValue:
    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomNewInternalFieldObject:
    case PhantomCreateActivation:
    case PhantomDirectArguments:
    case PhantomCreateRest:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
    case PhantomSpread:
    case PhantomClonedArguments:
    case PhantomNewRegexp:
    case GetMyArgumentByVal:
    case GetMyArgumentByValOutOfBounds:
    case ForwardVarargs:
    case PutHint:
    case KillStack:
    case GetStack:
    case GetFromArguments:
    case GetArgument:
    case LogShadowChickenPrologue:
    case LogShadowChickenTail:
    case NukeStructureAndSetButterfly:
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
    case MatchStructure:
    case FilterCallLinkStatus:
    case FilterGetByStatus:
    case FilterPutByIdStatus:
    case FilterInByIdStatus:
    case FilterDeleteByStatus:
    case DateGetInt32OrNaN:
    case DateGetTime:
    case DataViewGetInt:
    case DataViewGetFloat:
    case DataViewSet:
        return false;

#if ASSERT_ENABLED
    case ArrayPush:
    case ArrayPop:
    case PushWithScope:
    case CreateActivation:
    case CreateDirectArguments:
    case CreateScopedArguments:
    case CreateClonedArguments:
    case CreateArgumentsButterfly:
    case Call:
    case CallEval:
    case CallForwardVarargs:
    case CallObjectConstructor:
    case CallVarargs:
    case CheckTierUpAndOSREnter:
    case CheckTierUpAtReturn:
    case CheckTierUpInLoop:
    case Construct:
    case ConstructForwardVarargs:
    case ConstructVarargs:
    case DefineDataProperty:
    case DefineAccessorProperty:
    case DeleteById:
    case DeleteByVal:
    case DirectCall:
    case DirectConstruct:
    case DirectTailCall:
    case DirectTailCallInlinedCaller:
    case ForceOSRExit:
    case GetById:
    case GetByIdDirect:
    case GetByIdDirectFlush:
    case GetByIdFlush:
    case GetByIdWithThis:
    case GetByValWithThis:
    case GetDirectPname:
    case GetDynamicVar:
    case GetMapBucket:
    case HasGenericProperty:
    case HasIndexedProperty:
    case HasOwnProperty:
    case HasStructureProperty:
    case HasOwnStructureProperty:
    case InStructureProperty:
    case InById:
    case InByVal:
    case InstanceOf:
    case InstanceOfCustom:
    case VarargsLength:
    case LoadVarargs:
    case NumberToStringWithRadix:
    case NumberToStringWithValidRadixConstant:
    case ProfileType:
    case PutById:
    case PutByIdDirect:
    case PutByIdFlush:
    case PutByIdWithThis:
    case PutByOffset:
    case PutByValWithThis:
    case PutDynamicVar:
    case PutGetterById:
    case PutGetterByVal:
    case PutGetterSetterById:
    case PutSetterById:
    case PutSetterByVal:
    case PutStack:
    case PutToArguments:
    case RegExpExec:
    case RegExpExecNonGlobalOrSticky:
    case RegExpMatchFast:
    case RegExpMatchFastGlobal:
    case RegExpTest:
    case ResolveScope:
    case ResolveScopeForHoistingFuncDeclInEval:
    case Return:
    case StringCharAt:
    case TailCall:
    case TailCallForwardVarargs:
    case TailCallForwardVarargsInlinedCaller:
    case TailCallInlinedCaller:
    case TailCallVarargs:
    case TailCallVarargsInlinedCaller:
    case Throw:
    case ToNumber:
    case ToNumeric:
    case ToObject:
    case ToPrimitive:
    case ToPropertyKey:
    case ToThis:
    case TryGetById:
    case CreateThis:
    case CreatePromise:
    case CreateGenerator:
    case CreateAsyncGenerator:
    case ObjectCreate:
    case ObjectKeys:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
    case Arrayify:
    case ArrayifyToStructure:
    case NewObject:
    case NewGenerator:
    case NewAsyncGenerator:
    case NewArray:
    case NewArrayWithSpread:
    case NewInternalFieldObject:
    case Spread:
    case NewArrayWithSize:
    case NewArrayBuffer:
    case NewRegexp:
    case NewStringObject:
    case NewSymbol:
    case MakeRope:
    case NewFunction:
    case NewGeneratorFunction:
    case NewAsyncGeneratorFunction:
    case NewAsyncFunction:
    case NewTypedArray:
    case ThrowStaticError:
    case GetPropertyEnumerator:
    case GetEnumeratorStructurePname:
    case GetEnumeratorGenericPname:
    case ToIndexString:
    case MaterializeNewObject:
    case MaterializeNewInternalFieldObject:
    case MaterializeCreateActivation:
    case SetFunctionName:
    case StrCat:
    case StringReplace:
    case StringReplaceRegExp:
    case StringSlice:
    case StringValueOf:
    case CreateRest:
    case ToLowerCase:
    case CallDOMGetter:
    case CallDOM:
    case ArraySlice:
    case ArrayIndexOf:
    case ParseInt: // We might resolve a rope even though we don't clobber anything.
    case SetAdd:
    case MapSet:
    case ValueBitAnd:
    case ValueBitOr:
    case ValueBitXor:
    case ValueBitLShift:
    case ValueBitRShift:
    case ValueAdd:
    case ValueSub:
    case ValueMul:
    case ValueDiv:
    case ValueMod:
    case ValuePow:
    case ValueBitNot:
    case ValueNegate:
#else // not ASSERT_ENABLED
    // See comment at the top for why the default for all nodes should be to
    // return true.
    default:
#endif // not ASSERT_ENABLED
        return true;

    case CallNumberConstructor:
        switch (node->child1().useKind()) {
        case BigInt32Use:
            return false;
        default:
            break;
        }
        return true;

    case CallStringConstructor:
    case ToString:
        switch (node->child1().useKind()) {
        case StringObjectUse:
        case StringOrStringObjectUse:
            return false;
        default:
            break;
        }
        return true;

    case CheckTraps:
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=194323
        ASSERT(Options::usePollingTraps());
        return true;

    case CompareEq:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
        // FIXME: Add AnyBigIntUse and HeapBigIntUse specific optimizations in DFG / FTL code generation and ensure it does not perform GC.
        // https://bugs.webkit.org/show_bug.cgi?id=210923
        if (node->isBinaryUseKind(Int32Use)
#if USE(JSVALUE64)
            || node->isBinaryUseKind(Int52RepUse)
#endif
            || node->isBinaryUseKind(DoubleRepUse)
            || node->isBinaryUseKind(BigInt32Use)
            || node->isBinaryUseKind(StringIdentUse)
            )
            return false;
        if (node->op() == CompareEq) {
            if (node->isBinaryUseKind(BooleanUse)
                || node->isBinaryUseKind(SymbolUse)
                || node->isBinaryUseKind(ObjectUse)
                || node->isBinaryUseKind(ObjectUse, ObjectOrOtherUse) || node->isBinaryUseKind(ObjectOrOtherUse, ObjectUse))
                return false;
        }
        return true;

    case CompareStrictEq:
        if (node->isBinaryUseKind(BooleanUse)
            || node->isBinaryUseKind(Int32Use)
#if USE(JSVALUE64)
            || node->isBinaryUseKind(Int52RepUse)
#endif
            || node->isBinaryUseKind(DoubleRepUse)
            || node->isBinaryUseKind(SymbolUse)
            || node->isBinaryUseKind(SymbolUse, UntypedUse)
            || node->isBinaryUseKind(UntypedUse, SymbolUse)
            || node->isBinaryUseKind(StringIdentUse)
            || node->isBinaryUseKind(ObjectUse, UntypedUse) || node->isBinaryUseKind(UntypedUse, ObjectUse)
            || node->isBinaryUseKind(ObjectUse)
            || node->isBinaryUseKind(MiscUse, UntypedUse) || node->isBinaryUseKind(UntypedUse, MiscUse)
            || node->isBinaryUseKind(StringIdentUse, NotStringVarUse) || node->isBinaryUseKind(NotStringVarUse, StringIdentUse))
            return false;
        return true;

    case GetIndexedPropertyStorage:
    case GetByVal:
        if (node->arrayMode().type() == Array::String)
            return true;
        return false;

    case PutByValDirect:
    case PutByVal:
    case PutByValAlias:
        if (!graph.m_plan.isFTL()) {
            switch (node->arrayMode().modeForPut().type()) {
            case Array::Int8Array:
            case Array::Int16Array:
            case Array::Int32Array:
            case Array::Uint8Array:
            case Array::Uint8ClampedArray:
            case Array::Uint16Array:
            case Array::Uint32Array:
                return true;
            default:
                break;
            }
        }
        return false;

    case MapHash:
        switch (node->child1().useKind()) {
        case BooleanUse:
        case Int32Use:
        case SymbolUse:
        case ObjectUse:
            return false;
        default:
            // We might resolve a rope.
            return true;
        }
        
    case MultiPutByOffset:
        return node->multiPutByOffsetData().reallocatesStorage();

    case SameValue:
        if (node->isBinaryUseKind(DoubleRepUse))
            return false;
        return true;

    case StringFromCharCode:
        // FIXME: Should we constant fold this case?
        // https://bugs.webkit.org/show_bug.cgi?id=194308
        if (node->child1()->isInt32Constant() && (node->child1()->asUInt32() <= maxSingleCharacterString))
            return false;
        return true;

    case Switch:
        switch (node->switchData()->kind) {
        case SwitchCell:
            ASSERT(graph.m_plan.isFTL());
            FALLTHROUGH;
        case SwitchImm:
            return false;
        case SwitchChar:
            return true;
        case SwitchString:
            if (node->child1().useKind() == StringIdentUse)
                return false;
            ASSERT(node->child1().useKind() == StringUse || node->child1().useKind() == UntypedUse);
            return true;
        }
        RELEASE_ASSERT_NOT_REACHED();

    case Inc:
    case Dec:
        switch (node->child1().useKind()) {
        case Int32Use:
        case Int52RepUse:
        case DoubleRepUse:
            return false;
        default:
            return true;
        }

    case LastNodeType:
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
