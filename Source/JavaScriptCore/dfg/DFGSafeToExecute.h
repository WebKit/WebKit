/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
        case DoubleRepUse:
        case DoubleRepRealUse:
        case Int52RepUse:
        case NumberUse:
        case RealNumberUse:
        case BooleanUse:
        case CellUse:
        case ObjectUse:
        case FunctionUse:
        case FinalObjectUse:
        case ObjectOrOtherUse:
        case StringIdentUse:
        case StringUse:
        case StringObjectUse:
        case StringOrStringObjectUse:
        case NotStringVarUse:
        case NotCellUse:
        case OtherUse:
        case MiscUse:
        case MachineIntUse:
        case DoubleRepMachineIntUse:
            return;
            
        case KnownInt32Use:
            if (m_state.forNode(edge).m_type & ~SpecInt32)
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
    case DoubleConstant:
    case Int52Constant:
    case Identity:
    case ToThis:
    case CreateThis:
    case GetCallee:
    case GetArgumentCount:
    case GetLocal:
    case SetLocal:
    case PutStack:
    case KillStack:
    case GetStack:
    case MovHint:
    case ZombieHint:
    case Phantom:
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
    case ArithFRound:
    case ArithRound:
    case ArithSin:
    case ArithCos:
    case ArithLog:
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
    case Arrayify:
    case ArrayifyToStructure:
    case GetScope:
    case SkipScope:
    case GetClosureVar:
    case PutClosureVar:
    case GetGlobalVar:
    case PutGlobalVar:
    case VarInjectionWatchpoint:
    case CheckCell:
    case CheckBadCell:
    case CheckNotEmpty:
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
    case CallVarargs:
    case ConstructVarargs:
    case LoadVarargs:
    case CallForwardVarargs:
    case ConstructForwardVarargs:
    case NewObject:
    case NewArray:
    case NewArrayWithSize:
    case NewArrayBuffer:
    case NewRegexp:
    case Breakpoint:
    case ProfileWillCall:
    case ProfileDidCall:
    case ProfileType:
    case ProfileControlFlow:
    case CheckHasInstance:
    case InstanceOf:
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case IsObjectOrNull:
    case IsFunction:
    case TypeOf:
    case LogicalNot:
    case ToPrimitive:
    case ToString:
    case CallStringConstructor:
    case NewStringObject:
    case MakeRope:
    case In:
    case CreateActivation:
    case CreateDirectArguments:
    case CreateScopedArguments:
    case CreateClonedArguments:
    case GetFromArguments:
    case PutToArguments:
    case NewFunction:
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
    case CheckTierUpWithNestedTriggerAndOSREnter:
    case LoopHint:
    case StoreBarrier:
    case InvalidationPoint:
    case NotifyWrite:
    case CheckInBounds:
    case ConstantStoragePointer:
    case Check:
    case MultiGetByOffset:
    case MultiPutByOffset:
    case ValueRep:
    case DoubleRep:
    case Int52Rep:
    case BooleanToNumber:
    case FiatInt52:
    case GetGetter:
    case GetSetter:
    case GetEnumerableLength:
    case HasGenericProperty:
    case HasStructureProperty:
    case HasIndexedProperty:
    case GetDirectPname:
    case GetPropertyEnumerator:
    case GetEnumeratorStructurePname:
    case GetEnumeratorGenericPname:
    case ToIndexString:
    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomCreateActivation:
    case PutHint:
    case CheckStructureImmediate:
    case MaterializeNewObject:
    case MaterializeCreateActivation:
    case PhantomDirectArguments:
    case PhantomClonedArguments:
    case GetMyArgumentByVal:
    case ForwardVarargs:
        return true;

    case NativeCall:
    case NativeConstruct:
        return false; // TODO: add a check for already checked.  https://bugs.webkit.org/show_bug.cgi?id=133769

    case BottomValue:
        // If in doubt, assume that this isn't safe to execute, just because we have no way of
        // compiling this node.
        return false;

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

    case PutStructure:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
        return state.forNode(node->child1()).m_structure.isSubsetOf(
            StructureSet(node->transition()->previous));
        
    case GetByOffset:
    case GetGetterSetterByOffset:
    case PutByOffset: {
        StructureAbstractValue& value = state.forNode(node->child1()).m_structure;
        if (value.isTop())
            return false;
        PropertyOffset offset = node->storageAccessData().offset;
        for (unsigned i = value.size(); i--;) {
            if (!value[i]->isValidOffset(offset))
                return false;
        }
        return true;
    }
        
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

