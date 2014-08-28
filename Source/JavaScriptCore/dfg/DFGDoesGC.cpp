/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "DFGGraph.h"
#include "DFGNode.h"
#include "Operations.h"

namespace JSC { namespace DFG {

bool doesGC(Graph& graph, Node* node)
{
    if (graph.clobbersWorld(node))
        return true;
    
    // Now consider nodes that don't clobber the world but that still may GC. This includes all
    // nodes. By convention we put world-clobbering nodes in the block of "false" cases but we can
    // put them anywhere.
    switch (node->op()) {
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
    case Identity:
    case GetCallee:
    case GetLocal:
    case SetLocal:
    case MovHint:
    case ZombieHint:
    case GetArgument:
    case Phantom:
    case HardPhantom:
    case Upsilon:
    case Phi:
    case Flush:
    case PhantomLocal:
    case GetLocalUnlinked:
    case SetArgument:
    case BitAnd:
    case BitOr:
    case BitXor:
    case BitLShift:
    case BitRShift:
    case BitURShift:
    case ValueToInt32:
    case UInt32ToNumber:
    case DoubleAsInt32:
    case ArithAdd:
    case ArithSub:
    case ArithNegate:
    case ArithMul:
    case ArithIMul:
    case ArithDiv:
    case ArithMod:
    case ArithAbs:
    case ArithMin:
    case ArithMax:
    case ArithSqrt:
    case ArithFRound:
    case ArithSin:
    case ArithCos:
    case ValueAdd:
    case GetById:
    case GetByIdFlush:
    case PutById:
    case PutByIdFlush:
    case PutByIdDirect:
    case CheckStructure:
    case GetExecutable:
    case GetButterfly:
    case CheckArray:
    case GetScope:
    case GetMyScope:
    case SkipScope:
    case GetClosureRegisters:
    case GetClosureVar:
    case PutClosureVar:
    case GetGlobalVar:
    case PutGlobalVar:
    case VariableWatchpoint:
    case VarInjectionWatchpoint:
    case CheckCell:
    case AllocationProfileWatchpoint:
    case RegExpExec:
    case RegExpTest:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq:
    case CompareEqConstant:
    case CompareStrictEq:
    case Call:
    case Construct:
    case NativeCall:
    case NativeConstruct:
    case ProfiledCall:
    case ProfiledConstruct:
    case Breakpoint:
    case ProfileWillCall:
    case ProfileDidCall:
    case CheckHasInstance:
    case InstanceOf:
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case IsFunction:
    case TypeOf:
    case LogicalNot:
    case ToPrimitive:
    case ToString:
    case In:
    case TearOffActivation:
    case PhantomArguments:
    case TearOffArguments:
    case GetMyArgumentsLength:
    case GetMyArgumentByVal:
    case GetMyArgumentsLengthSafe:
    case GetMyArgumentByValSafe:
    case CheckArgumentsNotCreated:
    case Jump:
    case Branch:
    case Switch:
    case Return:
    case Throw:
    case CountExecution:
    case ForceOSRExit:
    case CheckWatchdogTimer:
    case StringFromCharCode:
    case Unreachable:
    case ExtractOSREntryLocal:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
    case CheckTierUpAndOSREnter:
    case LoopHint:
    case StoreBarrier:
    case StoreBarrierWithNullCheck:
    case InvalidationPoint:
    case NotifyWrite:
    case FunctionReentryWatchpoint:
    case TypedArrayWatchpoint:
    case CheckInBounds:
    case ConstantStoragePointer:
    case Check:
    case MultiGetByOffset:
    case ValueRep:
    case DoubleRep:
    case Int52Rep:
    case GetGetter:
    case GetSetter:
    case GetByVal:
    case GetIndexedPropertyStorage:
    case GetArrayLength:
    case ArrayPush:
    case ArrayPop:
    case StringCharAt:
    case StringCharCodeAt:
    case GetTypedArrayByteOffset:
    case PutByValDirect:
    case PutByVal:
    case PutByValAlias:
    case PutStructure:
    case GetByOffset:
    case GetGetterSetterByOffset:
    case PutByOffset:
    case GetEnumerableLength:
    case HasGenericProperty:
    case HasStructureProperty:
    case HasIndexedProperty:
    case GetDirectPname:
    case FiatInt52:
    case BooleanToNumber:
    case CheckBadCell:
    case BottomValue:
        return false;

    case CreateActivation:
    case CreateArguments:
    case ToThis:
    case CreateThis:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
    case Arrayify:
    case ArrayifyToStructure:
    case NewObject:
    case NewArray:
    case NewArrayWithSize:
    case NewArrayBuffer:
    case NewRegexp:
    case NewStringObject:
    case MakeRope:
    case NewFunctionNoCheck:
    case NewFunction:
    case NewFunctionExpression:
    case NewTypedArray:
    case ThrowReferenceError:
    case GetStructurePropertyEnumerator:
    case GetGenericPropertyEnumerator:
    case GetEnumeratorPname:
    case ToIndexString:
        return true;
        
    case MultiPutByOffset:
        return node->multiPutByOffsetData().reallocatesStorage();

    case LastNodeType:
        RELEASE_ASSERT_NOT_REACHED();
        return true;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return true;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
