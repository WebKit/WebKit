/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
    case SetArgument:
    case Return:
    case BitAnd:
    case BitOr:
    case BitXor:
    case BitRShift:
    case BitLShift:
    case BitURShift:
    case CheckStructure:
    case DoubleAsInt32:
    case ArrayifyToStructure:
    case PutStructure:
    case GetButterfly:
    case NewObject:
    case NewArray:
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
    case ValueAdd:
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
    case ArithSin:
    case ArithCos:
    case ArithPow:
    case ArithRandom:
    case ArithRound:
    case ArithFloor:
    case ArithCeil:
    case ArithTrunc:
    case ArithSqrt:
    case ArithLog:
    case ArithFRound:
    case ArithNegate:
    case UInt32ToNumber:
    case Jump:
    case ForceOSRExit:
    case Phi:
    case Upsilon:
    case ExtractOSREntryLocal:
    case LoopHint:
    case SkipScope:
    case GetGlobalObject:
    case CreateActivation:
    case NewArrowFunction:
    case NewFunction:
    case NewGeneratorFunction:
    case GetClosureVar:
    case PutClosureVar:
    case CreateDirectArguments:
    case CreateScopedArguments:
    case CreateClonedArguments:
    case GetFromArguments:
    case PutToArguments:
    case InvalidationPoint:
    case StringCharAt:
    case CheckCell:
    case CheckBadCell:
    case CheckNotEmpty:
    case CheckIdent:
    case CheckWatchdogTimer:
    case StringCharCodeAt:
    case StringFromCharCode:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
    case GetTypedArrayByteOffset:
    case NotifyWrite:
    case StoreBarrier:
    case Call:
    case TailCall:
    case TailCallInlinedCaller:
    case Construct:
    case CallVarargs:
    case TailCallVarargs:
    case TailCallVarargsInlinedCaller:
    case ConstructVarargs:
    case CallForwardVarargs:
    case TailCallForwardVarargs:
    case TailCallForwardVarargsInlinedCaller:
    case ConstructForwardVarargs:
    case LoadVarargs:
    case ValueToInt32:
    case Branch:
    case LogicalNot:
    case CheckInBounds:
    case ConstantStoragePointer:
    case Check:
    case CountExecution:
    case GetExecutable:
    case GetScope:
    case GetCallee:
    case GetArgumentCount:
    case ToString:
    case CallStringConstructor:
    case MakeRope:
    case NewArrayWithSize:
    case GetById:
    case GetByIdFlush:
    case ToThis:
    case MultiGetByOffset:
    case MultiPutByOffset:
    case ToPrimitive:
    case Throw:
    case ThrowReferenceError:
    case Unreachable:
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case IsObjectOrNull:
    case IsFunction:
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
    case GetDirectPname:
    case GetEnumerableLength:
    case GetPropertyEnumerator:
    case GetEnumeratorStructurePname:
    case GetEnumeratorGenericPname:
    case ToIndexString:
    case BottomValue:
    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomCreateActivation:
    case PutHint:
    case CheckStructureImmediate:
    case MaterializeNewObject:
    case MaterializeCreateActivation:
    case PhantomDirectArguments:
    case PhantomClonedArguments:
    case GetMyArgumentByVal:
    case ForwardVarargs:
    case Switch:
    case TypeOf:
    case PutGetterById:
    case PutSetterById:
    case PutGetterSetterById:
    case PutGetterByVal:
    case PutSetterByVal:
    case CopyRest:
    case GetRestLength:
    case RegExpExec:
    case RegExpTest:
    case NewRegexp:
    case StringReplace:
    case GetRegExpObjectLastIndex:
    case SetRegExpObjectLastIndex:
    case RecordRegExpCachedResult:
    case SetFunctionName:
        // These are OK.
        break;

    case Identity:
        // No backend handles this because it will be optimized out. But we may check
        // for capabilities before optimization. It would be a deep error to remove this
        // case because it would prevent us from catching bugs where the FTL backend
        // pipeline failed to optimize out an Identity.
        break;
    case In:
        if (node->child2().useKind() == CellUse)
            break;
        return CannotCompile;
    case PutByIdDirect:
    case PutById:
    case PutByIdFlush:
        if (node->child1().useKind() == CellUse)
            break;
        return CannotCompile;
    case GetIndexedPropertyStorage:
        if (node->arrayMode().type() == Array::String)
            break;
        if (isTypedView(node->arrayMode().typedArrayType()))
            break;
        return CannotCompile;
    case CheckArray:
        switch (node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::DirectArguments:
        case Array::ScopedArguments:
            break;
        default:
            if (isTypedView(node->arrayMode().typedArrayType()))
                break;
            return CannotCompile;
        }
        break;
    case GetArrayLength:
        switch (node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::String:
        case Array::DirectArguments:
        case Array::ScopedArguments:
            break;
        default:
            if (node->arrayMode().isSomeTypedArrayView())
                break;
            return CannotCompile;
        }
        break;
    case HasIndexedProperty:
        switch (node->arrayMode().type()) {
        case Array::ForceExit:
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
            break;
        default:
            return CannotCompile;
        }
        break;
    case GetByVal:
        switch (node->arrayMode().type()) {
        case Array::ForceExit:
        case Array::Generic:
        case Array::String:
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::Undecided:
        case Array::DirectArguments:
        case Array::ScopedArguments:
            break;
        default:
            if (isTypedView(node->arrayMode().typedArrayType()))
                return CanCompileAndOSREnter;
            return CannotCompile;
        }
        break;
    case PutByVal:
    case PutByValAlias:
    case PutByValDirect:
        switch (node->arrayMode().type()) {
        case Array::ForceExit:
        case Array::Generic:
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
            break;
        default:
            if (isTypedView(node->arrayMode().typedArrayType()))
                return CanCompileAndOSREnter;
            return CannotCompile;
        }
        break;
    case ArrayPush:
    case ArrayPop:
        switch (node->arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous:
        case Array::Double:
            break;
        default:
            return CannotCompile;
        }
        break;
    case CompareEq:
        if (node->isBinaryUseKind(Int32Use))
            break;
        if (node->isBinaryUseKind(Int52RepUse))
            break;
        if (node->isBinaryUseKind(DoubleRepUse))
            break;
        if (node->isBinaryUseKind(StringIdentUse))
            break;
        if (node->isBinaryUseKind(StringUse))
            break;
        if (node->isBinaryUseKind(SymbolUse))
            break;
        if (node->isBinaryUseKind(ObjectUse))
            break;
        if (node->isBinaryUseKind(UntypedUse))
            break;
        if (node->isBinaryUseKind(BooleanUse))
            break;
        if (node->isBinaryUseKind(ObjectUse, ObjectOrOtherUse))
            break;
        if (node->isBinaryUseKind(ObjectOrOtherUse, ObjectUse))
            break;
        if (node->child1().useKind() == OtherUse || node->child2().useKind() == OtherUse)
            break;
        return CannotCompile;
    case CompareStrictEq:
        if (node->isBinaryUseKind(Int32Use))
            break;
        if (node->isBinaryUseKind(Int52RepUse))
            break;
        if (node->isBinaryUseKind(DoubleRepUse))
            break;
        if (node->isBinaryUseKind(StringIdentUse))
            break;
        if (node->isBinaryUseKind(StringUse))
            break;
        if (node->isBinaryUseKind(ObjectUse, UntypedUse))
            break;
        if (node->isBinaryUseKind(UntypedUse, ObjectUse))
            break;
        if (node->isBinaryUseKind(ObjectUse))
            break;
        if (node->isBinaryUseKind(BooleanUse))
            break;
        if (node->isBinaryUseKind(SymbolUse))
            break;
        if (node->isBinaryUseKind(MiscUse, UntypedUse))
            break;
        if (node->isBinaryUseKind(UntypedUse, MiscUse))
            break;
        if (node->isBinaryUseKind(StringIdentUse, NotStringVarUse))
            break;
        if (node->isBinaryUseKind(NotStringVarUse, StringIdentUse))
            break;
        return CannotCompile;
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
        if (node->isBinaryUseKind(Int32Use))
            break;
        if (node->isBinaryUseKind(Int52RepUse))
            break;
        if (node->isBinaryUseKind(DoubleRepUse))
            break;
        if (node->isBinaryUseKind(UntypedUse))
            break;
        return CannotCompile;
    default:
        // Don't know how to handle anything else.
        return CannotCompile;
    }
    return CanCompileAndOSREnter;
}

CapabilityLevel canCompile(Graph& graph)
{
    if (graph.m_codeBlock->instructionCount() > Options::maximumFTLCandidateInstructionCount()) {
        if (verboseCapabilities())
            dataLog("FTL rejecting ", *graph.m_codeBlock, " because it's too big.\n");
        return CannotCompile;
    }
    
    if (graph.m_codeBlock->codeType() != FunctionCode) {
        if (verboseCapabilities())
            dataLog("FTL rejecting ", *graph.m_codeBlock, " because it doesn't belong to a function.\n");
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
                case FunctionUse:
                case ObjectOrOtherUse:
                case StringUse:
                case StringOrOtherUse:
                case KnownStringUse:
                case KnownPrimitiveUse:
                case StringObjectUse:
                case StringOrStringObjectUse:
                case SymbolUse:
                case FinalObjectUse:
                case RegExpObjectUse:
                case NotCellUse:
                case OtherUse:
                case MiscUse:
                case StringIdentUse:
                case NotStringVarUse:
                case MachineIntUse:
                case DoubleRepMachineIntUse:
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

