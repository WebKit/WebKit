/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "FTLCapabilities.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

using namespace DFG;

static bool verboseCapabilities()
{
    return verboseCompilationEnabled() || Options::verboseFTLFailure();
}

inline CapabilityLevel canCompile(Node* node)
{
    // NOTE: If we ever have phantom arguments, we can compile them but we cannot
    // OSR enter.
    
    switch (node->op()) {
    case JSConstant:
    case LazyJSConstant:
    case GetLocal:
    case SetLocal:
    case PutStack:
    case KillStack:
    case GetStack:
    case MovHint:
    case ZombieHint:
    case ExitOK:
    case Phantom:
    case Flush:
    case PhantomLocal:
    case SetArgumentDefinitely:
    case SetArgumentMaybe:
    case Return:
    case ArithBitNot:
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitXor:
    case ArithBitRShift:
    case ArithBitLShift:
    case BitURShift:
    case CheckStructure:
    case CheckStructureOrEmpty:
    case DoubleAsInt32:
    case Arrayify:
    case ArrayifyToStructure:
    case PutStructure:
    case GetButterfly:
    case NewObject:
    case NewGenerator:
    case NewAsyncGenerator:
    case NewStringObject:
    case NewSymbol:
    case NewArray:
    case NewArrayWithSpread:
    case NewInternalFieldObject:
    case Spread:
    case NewArrayBuffer:
    case NewTypedArray:
    case GetByOffset:
    case GetGetterSetterByOffset:
    case GetGetter:
    case GetSetter:
    case PutByOffset:
    case GetGlobalVar:
    case GetGlobalLexicalVariable:
    case PutGlobalVariable:
    case ValueBitAnd:
    case ValueBitXor:
    case ValueBitOr:
    case ValueBitNot:
    case ValueBitLShift:
    case ValueBitRShift:
    case ValueNegate:
    case ValueAdd:
    case ValueSub:
    case ValueMul:
    case ValueDiv:
    case ValueMod:
    case ValuePow:
    case Inc:
    case Dec:
    case StrCat:
    case ArithAdd:
    case ArithClz32:
    case ArithSub:
    case ArithMul:
    case ArithDiv:
    case ArithMod:
    case ArithMin:
    case ArithMax:
    case ArithAbs:
    case ArithPow:
    case ArithRandom:
    case ArithRound:
    case ArithFloor:
    case ArithCeil:
    case ArithTrunc:
    case ArithSqrt:
    case ArithFRound:
    case ArithNegate:
    case ArithUnary:
    case UInt32ToNumber:
    case Jump:
    case ForceOSRExit:
    case Phi:
    case Upsilon:
    case ExtractOSREntryLocal:
    case ExtractCatchLocal:
    case ClearCatchLocals:
    case LoopHint:
    case SkipScope:
    case GetGlobalObject:
    case GetGlobalThis:
    case CreateActivation:
    case PushWithScope:
    case NewFunction:
    case NewGeneratorFunction:
    case NewAsyncFunction:
    case NewAsyncGeneratorFunction:
    case GetClosureVar:
    case PutClosureVar:
    case GetInternalField:
    case PutInternalField:
    case CreateDirectArguments:
    case CreateScopedArguments:
    case CreateClonedArguments:
    case CreateArgumentsButterfly:
    case GetFromArguments:
    case PutToArguments:
    case GetArgument:
    case InvalidationPoint:
    case StringCharAt:
    case CheckIsConstant:
    case CheckBadValue:
    case CheckNotEmpty:
    case AssertNotEmpty:
    case CheckIdent:
    case CheckTraps:
    case StringCharCodeAt:
    case StringCodePointAt:
    case StringFromCharCode:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
    case NukeStructureAndSetButterfly:
    case GetTypedArrayByteOffset:
    case GetPrototypeOf:
    case NotifyWrite:
    case StoreBarrier:
    case FencedStoreBarrier:
    case Call:
    case DirectCall:
    case TailCall:
    case DirectTailCall:
    case TailCallInlinedCaller:
    case DirectTailCallInlinedCaller:
    case Construct:
    case DirectConstruct:
    case CallVarargs:
    case CallEval:
    case TailCallVarargs:
    case TailCallVarargsInlinedCaller:
    case ConstructVarargs:
    case CallForwardVarargs:
    case TailCallForwardVarargs:
    case TailCallForwardVarargsInlinedCaller:
    case ConstructForwardVarargs:
    case VarargsLength:
    case LoadVarargs:
    case ValueToInt32:
    case Branch:
    case LogicalNot:
    case CheckInBounds:
    case ConstantStoragePointer:
    case Check:
    case CheckVarargs:
    case CheckArray:
    case CheckArrayOrEmpty:
    case CheckNeutered:
    case CountExecution:
    case SuperSamplerBegin:
    case SuperSamplerEnd:
    case GetExecutable:
    case GetScope:
    case GetCallee:
    case SetCallee:
    case GetArgumentCountIncludingThis:
    case SetArgumentCountIncludingThis:
    case ToNumber:
    case ToNumeric:
    case ToString:
    case ToObject:
    case CallObjectConstructor:
    case CallStringConstructor:
    case CallNumberConstructor:
    case ObjectCreate:
    case ObjectKeys:
    case ObjectGetOwnPropertyNames:
    case MakeRope:
    case NewArrayWithSize:
    case TryGetById:
    case GetById:
    case GetByIdFlush:
    case GetByIdWithThis:
    case GetByIdDirect:
    case GetByIdDirectFlush:
    case ToThis:
    case MultiGetByOffset:
    case MultiPutByOffset:
    case MultiDeleteByOffset:
    case ToPrimitive:
    case ToPropertyKey:
    case Throw:
    case ThrowStaticError:
    case Unreachable:
    case InByVal:
    case InById:
    case HasOwnProperty:
    case IsCellWithType:
    case MapHash:
    case NormalizeMapKey:
    case GetMapBucket:
    case GetMapBucketHead:
    case GetMapBucketNext:
    case LoadKeyFromMapBucket:
    case LoadValueFromMapBucket:
    case ExtractValueFromWeakMapGet:
    case SetAdd:
    case MapSet:
    case WeakMapGet:
    case WeakSetAdd:
    case WeakMapSet:
    case IsEmpty:
    case TypeOfIsUndefined:
    case TypeOfIsObject:
    case TypeOfIsFunction:
    case IsUndefinedOrNull:
    case IsBoolean:
    case IsNumber:
    case IsBigInt:
    case NumberIsInteger:
    case IsObject:
    case IsCallable:
    case IsConstructor:
    case IsTypedArrayView:
    case CheckTypeInfoFlags:
    case OverridesHasInstance:
    case InstanceOf:
    case InstanceOfCustom:
    case DoubleRep:
    case ValueRep:
    case Int52Rep:
    case DoubleConstant:
    case Int52Constant:
    case BooleanToNumber:
    case HasGenericProperty:
    case HasStructureProperty:
    case HasOwnStructureProperty:
    case InStructureProperty:
    case HasIndexedProperty:
    case GetDirectPname:
    case GetEnumerableLength:
    case GetIndexedPropertyStorage:
    case GetPropertyEnumerator:
    case GetEnumeratorStructurePname:
    case GetEnumeratorGenericPname:
    case ToIndexString:
    case BottomValue:
    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewInternalFieldObject:
    case PhantomCreateActivation:
    case PhantomNewRegexp:
    case PutHint:
    case CheckStructureImmediate:
    case MaterializeNewObject:
    case MaterializeCreateActivation:
    case MaterializeNewInternalFieldObject:
    case PhantomDirectArguments:
    case PhantomCreateRest:
    case PhantomSpread:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
    case PhantomClonedArguments:
    case GetMyArgumentByVal:
    case GetMyArgumentByValOutOfBounds:
    case ForwardVarargs:
    case EntrySwitch:
    case Switch:
    case TypeOf:
    case PutById:
    case PutByIdDirect:
    case PutByIdFlush:
    case PutByIdWithThis:
    case PutGetterById:
    case PutSetterById:
    case PutGetterSetterById:
    case PutGetterByVal:
    case PutSetterByVal:
    case DeleteById:
    case DeleteByVal:
    case CreateRest:
    case GetRestLength:
    case RegExpExec:
    case RegExpExecNonGlobalOrSticky:
    case RegExpTest:
    case RegExpMatchFast:
    case RegExpMatchFastGlobal:
    case NewRegexp:
    case StringReplace:
    case StringReplaceRegExp: 
    case GetRegExpObjectLastIndex:
    case SetRegExpObjectLastIndex:
    case RecordRegExpCachedResult:
    case SetFunctionName:
    case LogShadowChickenPrologue:
    case LogShadowChickenTail:
    case ResolveScope:
    case ResolveScopeForHoistingFuncDeclInEval:
    case GetDynamicVar:
    case PutDynamicVar:
    case CompareEq:
    case CompareEqPtr:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareBelow:
    case CompareBelowEq:
    case CompareStrictEq:
    case SameValue:
    case DefineDataProperty:
    case DefineAccessorProperty:
    case StringValueOf:
    case StringSlice:
    case ToLowerCase:
    case NumberToStringWithRadix:
    case NumberToStringWithValidRadixConstant:
    case CheckJSCast:
    case CheckNotJSCast:
    case CallDOM:
    case CallDOMGetter:
    case ArraySlice:
    case ArrayIndexOf:
    case ArrayPop:
    case ArrayPush:
    case ParseInt:
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
    case InitializeEntrypointArguments:
    case CPUIntrinsic:
    case GetArrayLength:
    case GetVectorLength:
    case GetByVal:
    case GetByValWithThis:
    case PutByVal:
    case PutByValAlias:
    case PutByValDirect:
    case PutByValWithThis:
    case MatchStructure:
    case FilterCallLinkStatus:
    case FilterGetByStatus:
    case FilterPutByIdStatus:
    case FilterInByIdStatus:
    case FilterDeleteByStatus:
    case CreateThis:
    case CreatePromise:
    case CreateGenerator:
    case CreateAsyncGenerator:
    case DataViewGetInt:
    case DataViewGetFloat:
    case DataViewSet:
    case DateGetInt32OrNaN:
    case DateGetTime:
        // These are OK.
        break;

    case Identity:
        // No backend handles this because it will be optimized out. But we may check
        // for capabilities before optimization. It would be a deep error to remove this
        // case because it would prevent us from catching bugs where the FTL backend
        // pipeline failed to optimize out an Identity.
        break;

    case IdentityWithProfile:
    case CheckTierUpInLoop:
    case CheckTierUpAndOSREnter:
    case CheckTierUpAtReturn:
    case FiatInt52:
    case ArithIMul:
    case ProfileType:
    case ProfileControlFlow:
    case LastNodeType:
        return CannotCompile;
    }
    return CanCompileAndOSREnter;
}

CapabilityLevel canCompile(Graph& graph)
{
    if (graph.m_codeBlock->bytecodeCost() > Options::maximumFTLCandidateBytecodeCost()) {
        if (verboseCapabilities())
            dataLog("FTL rejecting ", *graph.m_codeBlock, " because it's too big.\n");
        return CannotCompile;
    }
    
    if (UNLIKELY(graph.m_codeBlock->ownerExecutable()->neverFTLOptimize())) {
        if (verboseCapabilities())
            dataLog("FTL rejecting ", *graph.m_codeBlock, " because it is marked as never FTL compile.\n");
        return CannotCompile;
    }
    
    CapabilityLevel result = CanCompileAndOSREnter;
    
    for (BlockIndex blockIndex = graph.numBlocks(); blockIndex--;) {
        BasicBlock* block = graph.block(blockIndex);
        if (!block)
            continue;
        
        // We don't care if we can compile blocks that the CFA hasn't visited.
        if (!block->cfaHasVisited)
            continue;
        
        for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
            Node* node = block->at(nodeIndex);
            
            for (unsigned childIndex = graph.numChildren(node); childIndex--;) {
                Edge edge = graph.child(node, childIndex);
                if (!edge)
                    continue;
                switch (edge.useKind()) {
                case UntypedUse:
                case Int32Use:
                case KnownInt32Use:
                case Int52RepUse:
                case NumberUse:
                case RealNumberUse:
                case DoubleRepUse:
                case DoubleRepRealUse:
                case BooleanUse:
                case KnownBooleanUse:
                case CellUse:
                case KnownCellUse:
                case CellOrOtherUse:
                case ObjectUse:
                case ArrayUse:
                case FunctionUse:
                case ObjectOrOtherUse:
                case StringUse:
                case StringOrOtherUse:
                case KnownStringUse:
                case KnownPrimitiveUse:
                case StringObjectUse:
                case StringOrStringObjectUse:
                case SymbolUse:
                case AnyBigIntUse:
                case BigInt32Use:
                case HeapBigIntUse:
                case DateObjectUse:
                case MapObjectUse:
                case SetObjectUse:
                case WeakMapObjectUse:
                case WeakSetObjectUse:
                case DataViewObjectUse:
                case FinalObjectUse:
                case PromiseObjectUse:
                case RegExpObjectUse:
                case ProxyObjectUse:
                case DerivedArrayUse:
                case NotCellUse:
                case NotCellNorBigIntUse:
                case OtherUse:
                case KnownOtherUse:
                case MiscUse:
                case StringIdentUse:
                case NotStringVarUse:
                case NotSymbolUse:
                case AnyIntUse:
                case DoubleRepAnyIntUse:
                    // These are OK.
                    break;
                default:
                    // Don't know how to handle anything else.
                    if (verboseCapabilities()) {
                        dataLog("FTL rejecting node in ", *graph.m_codeBlock, " because of bad use kind: ", edge.useKind(), " in node:\n");
                        graph.dump(WTF::dataFile(), "    ", node);
                    }
                    return CannotCompile;
                }
            }
            
            switch (canCompile(node)) {
            case CannotCompile: 
                if (verboseCapabilities()) {
                    dataLog("FTL rejecting node in ", *graph.m_codeBlock, ":\n");
                    graph.dump(WTF::dataFile(), "    ", node);
                }
                return CannotCompile;
                
            case CanCompile:
                if (result == CanCompileAndOSREnter && verboseCompilationEnabled()) {
                    dataLog("FTL disabling OSR entry because of node:\n");
                    graph.dump(WTF::dataFile(), "    ", node);
                }
                result = CanCompile;
                break;
                
            case CanCompileAndOSREnter:
                break;
            }
            
            if (node->op() == ForceOSRExit)
                break;
        }
    }
    
    return result;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

