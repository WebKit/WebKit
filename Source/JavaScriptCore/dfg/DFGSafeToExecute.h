/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGSafeToExecute_h
#define DFGSafeToExecute_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"

namespace JSC { namespace DFG {

template<typename AbstractStateType>
class SafeToExecuteEdge {
public:
    SafeToExecuteEdge(AbstractStateType& state)
        : m_state(state)
        , m_result(true)
    {
    }
    
    void operator()(Node*, Edge edge)
    {
        switch (edge.useKind()) {
        case UntypedUse:
        case Int32Use:
        case RealNumberUse:
        case NumberUse:
        case BooleanUse:
        case CellUse:
        case ObjectUse:
        case FinalObjectUse:
        case ObjectOrOtherUse:
        case StringIdentUse:
        case StringUse:
        case StringObjectUse:
        case StringOrStringObjectUse:
        case NotCellUse:
        case OtherUse:
        case MachineIntUse:
            return;
            
        case KnownInt32Use:
            if (m_state.forNode(edge).m_type & ~SpecInt32)
                m_result = false;
            return;
            
        case KnownNumberUse:
            if (m_state.forNode(edge).m_type & ~SpecFullNumber)
                m_result = false;
            return;
            
        case KnownCellUse:
            if (m_state.forNode(edge).m_type & ~SpecCell)
                m_result = false;
            return;
            
        case KnownStringUse:
            if (m_state.forNode(edge).m_type & ~SpecString)
                m_result = false;
            return;
            
        case LastUseKind:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    bool result() const { return m_result; }
private:
    AbstractStateType& m_state;
    bool m_result;
};

// Determines if it's safe to execute a node within the given abstract state. This may
// return false conservatively. If it returns true, then you can hoist the given node
// up to the given point and expect that it will not crash. This doesn't guarantee that
// the node will produce the result you wanted other than not crashing.
template<typename AbstractStateType>
bool safeToExecute(AbstractStateType& state, Graph& graph, Node* node)
{
    SafeToExecuteEdge<AbstractStateType> safeToExecuteEdge(state);
    DFG_NODE_DO_TO_CHILDREN(graph, node, safeToExecuteEdge);
    if (!safeToExecuteEdge.result())
        return false;

    switch (node->op()) {
    case JSConstant:
    case WeakJSConstant:
    case Identity:
    case ToThis:
    case CreateThis:
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
    case Int32ToDouble:
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
    case ArithSin:
    case ArithCos:
    case ValueAdd:
    case GetById:
    case GetByIdFlush:
    case PutById:
    case PutByIdDirect:
    case CheckStructure:
    case CheckExecutable:
    case GetButterfly:
    case CheckArray:
    case Arrayify:
    case ArrayifyToStructure:
    case GetScope:
    case GetMyScope:
    case SkipTopScope:
    case SkipScope:
    case GetClosureRegisters:
    case GetClosureVar:
    case PutClosureVar:
    case GetGlobalVar:
    case PutGlobalVar:
    case VariableWatchpoint:
    case VarInjectionWatchpoint:
    case CheckFunction:
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
    case CompareStrictEqConstant:
    case Call:
    case Construct:
    case NewObject:
    case NewArray:
    case NewArrayWithSize:
    case NewArrayBuffer:
    case NewRegexp:
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
    case NewStringObject:
    case MakeRope:
    case In:
    case CreateActivation:
    case TearOffActivation:
    case CreateArguments:
    case PhantomArguments:
    case TearOffArguments:
    case GetMyArgumentsLength:
    case GetMyArgumentByVal:
    case GetMyArgumentsLengthSafe:
    case GetMyArgumentByValSafe:
    case CheckArgumentsNotCreated:
    case NewFunctionNoCheck:
    case NewFunction:
    case NewFunctionExpression:
    case Jump:
    case Branch:
    case Switch:
    case Return:
    case Throw:
    case ThrowReferenceError:
    case CountExecution:
    case ForceOSRExit:
    case CheckWatchdogTimer:
    case StringFromCharCode:
    case NewTypedArray:
    case Unreachable:
    case ExtractOSREntryLocal:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
    case CheckTierUpAndOSREnter:
    case LoopHint:
    case Int52ToDouble:
    case Int52ToValue:
    case StoreBarrier:
    case ConditionalStoreBarrier:
    case StoreBarrierWithNullCheck:
    case InvalidationPoint:
    case NotifyWrite:
    case FunctionReentryWatchpoint:
    case TypedArrayWatchpoint:
    case CheckInBounds:
    case ConstantStoragePointer:
    case Check:
    case MultiGetByOffset:
        return true;
        
    case GetByVal:
    case GetIndexedPropertyStorage:
    case GetArrayLength:
    case ArrayPush:
    case ArrayPop:
    case StringCharAt:
    case StringCharCodeAt:
        return node->arrayMode().alreadyChecked(graph, node, state.forNode(node->child1()));
        
    case GetTypedArrayByteOffset:
        return !(state.forNode(node->child1()).m_type & ~(SpecTypedArrayView));
            
    case PutByValDirect:
    case PutByVal:
    case PutByValAlias:
        return node->arrayMode().modeForPut().alreadyChecked(
            graph, node, state.forNode(graph.varArgChild(node, 0)));

    case StructureTransitionWatchpoint:
        return state.forNode(node->child1()).m_futurePossibleStructure.isSubsetOf(
            StructureSet(node->structure()));
        
    case PutStructure:
    case PhantomPutStructure:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
        return state.forNode(node->child1()).m_currentKnownStructure.isSubsetOf(
            StructureSet(node->structureTransitionData().previousStructure));
        
    case GetByOffset:
    case PutByOffset:
        return state.forNode(node->child1()).m_currentKnownStructure.isValidOffset(
            graph.m_storageAccessData[node->storageAccessDataIndex()].offset);
        
    case LastNodeType:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGSafeToExecute_h

