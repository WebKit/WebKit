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
#include "Operations.h"

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
    case SetArgument:
    case ArithBitNot:
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitXor:
    case BitLShift:
    case BitRShift:
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
    case TryGetById:
    case CheckStructure:
    case CheckStructureOrEmpty:
    case CheckStructureImmediate:
    case GetExecutable:
    case GetButterfly:
    case CheckSubClass:
    case CheckArray:
    case GetScope:
    case SkipScope:
    case GetGlobalObject:
    case GetGlobalThis:
    case GetClosureVar:
    case PutClosureVar:
    case GetRegExpObjectLastIndex:
    case SetRegExpObjectLastIndex:
    case RecordRegExpCachedResult:
    case GetGlobalVar:
    case GetGlobalLexicalVariable:
    case PutGlobalVariable:
    case CheckCell:
    case CheckNotEmpty:
    case AssertNotEmpty:
    case CheckStringIdent:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareBelow:
    case CompareBelowEq:
    case CompareEq:
    case CompareStrictEq:
    case CompareEqPtr:
    case ProfileType:
    case ProfileControlFlow:
    case OverridesHasInstance:
    case IsEmpty:
    case IsUndefined:
    case IsUndefinedOrNull:
    case IsBoolean:
    case IsNumber:
    case NumberIsInteger:
    case IsObject:
    case IsObjectOrNull:
    case IsFunction:
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
    case ValueRep:
    case DoubleRep:
    case Int52Rep:
    case GetGetter:
    case GetSetter:
    case GetArrayLength:
    case GetVectorLength:
    case StringCharCodeAt:
    case GetTypedArrayByteOffset:
    case GetPrototypeOf:
    case PutStructure:
    case GetByOffset:
    case GetGetterSetterByOffset:
    case GetEnumerableLength:
    case HasIndexedProperty:
    case FiatInt52:
    case BooleanToNumber:
    case CheckBadCell:
    case BottomValue:
    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewAsyncGeneratorFunction:
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
    case FilterGetByIdStatus:
    case FilterPutByIdStatus:
    case FilterInByIdStatus:
    case DataViewGetInt:
    case DataViewGetFloat:
    case DataViewSet:
        return false;

#if !ASSERT_DISABLED
    case ArrayPush:
    case ArrayPop:
    case PushWithScope:
    case CreateActivation:
    case CreateDirectArguments:
    case CreateScopedArguments:
    case CreateClonedArguments:
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
    case HasOwnProperty:
    case HasStructureProperty:
    case InById:
    case InByVal:
    case InstanceOf:
    case InstanceOfCustom:
    case LoadVarargs:
    case NumberToStringWithRadix:
    case NumberToStringWithValidRadixConstant:
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
    case ToObject:
    case ToPrimitive:
    case ToThis:
    case CreateThis:
    case ObjectCreate:
    case ObjectKeys:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
    case Arrayify:
    case ArrayifyToStructure:
    case NewObject:
    case NewArray:
    case NewArrayWithSpread:
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
    case ValueAdd:
    case ValueSub:
    case ValueMul:
    case ValueDiv:
    case ValueNegate:
#else
    // See comment at the top for why be default for all nodes should be to
    // return true.
    default:
#endif
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

    case LastNodeType:
        RELEASE_ASSERT_NOT_REACHED();
        return true;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return true;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
