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

#ifndef DFGClobberize_h
#define DFGClobberize_h

#if ENABLE(DFG_JIT)

#include "DFGAbstractHeap.h"
#include "DFGEdgeUsesStructure.h"
#include "DFGGraph.h"

namespace JSC { namespace DFG {

template<typename ReadFunctor, typename WriteFunctor>
void clobberizeForAllocation(ReadFunctor& read, WriteFunctor& write)
{
    read(GCState);
    read(BarrierState);
    write(GCState);
    write(BarrierState);
}

template<typename ReadFunctor, typename WriteFunctor>
void clobberize(Graph& graph, Node* node, ReadFunctor& read, WriteFunctor& write)
{
    // Some notes:
    //
    // - We cannot hoist, or sink, anything that has effects. This means that the
    //   easiest way of indicating that something cannot be hoisted is to claim
    //   that it side-effects some miscellaneous thing.
    //
    // - We cannot hoist forward-exiting nodes without some additional effort. I
    //   believe that what it comes down to is that forward-exiting generally have
    //   their NodeExitsForward cleared upon hoist, except for forward-exiting
    //   nodes that take bogus state as their input. Those are substantially
    //   harder. We disable it for now. In the future we could enable it by having
    //   versions of those nodes that backward-exit instead, but I'm not convinced
    //   of the soundness.
    //
    // - Some nodes lie, and claim that they do not read the JSCell_structure.
    //   These are nodes that use the structure in a way that does not depend on
    //   things that change under structure transitions.
    //
    // - It's implicitly understood that OSR exits read the world. This is why we
    //   generally don't move or eliminate stores. Every node can exit, so the
    //   read set does not reflect things that would be read if we exited.
    //   Instead, the read set reflects what the node will have to read if it
    //   *doesn't* exit.
    //
    // - Broadly, we don't say that we're reading something if that something is
    //   immutable.
    //
    // - We try to make this work even prior to type inference, just so that we
    //   can use it for IR dumps. No promises on whether the answers are sound
    //   prior to type inference - though they probably could be if we did some
    //   small hacking.
    
    if (edgesUseStructure(graph, node))
        read(JSCell_structure);
    
    switch (node->op()) {
    case JSConstant:
    case WeakJSConstant:
    case Identity:
    case Phantom:
    case HardPhantom:
    case Breakpoint:
    case ProfileWillCall:
    case ProfileDidCall:
    case BitAnd:
    case BitOr:
    case BitXor:
    case BitLShift:
    case BitRShift:
    case BitURShift:
    case ValueToInt32:
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
    case GetScope:
    case SkipScope:
    case CheckFunction:
    case StringCharCodeAt:
    case StringFromCharCode:
    case CompareEqConstant:
    case CompareStrictEqConstant:
    case CompareStrictEq:
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case LogicalNot:
    case Int32ToDouble:
    case ExtractOSREntryLocal:
    case Int52ToDouble:
    case Int52ToValue:
    case CheckInBounds:
    case ConstantStoragePointer:
    case UInt32ToNumber:
    case DoubleAsInt32:
    case Check:
        return;
        
    case MovHint:
    case ZombieHint:
    case Upsilon:
    case Phi:
    case Flush:
    case PhantomLocal:
    case SetArgument:
    case PhantomArguments:
    case Jump:
    case Branch:
    case Switch:
    case Throw:
    case ForceOSRExit:
    case Return:
    case Unreachable:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
    case CheckTierUpAndOSREnter:
    case LoopHint:
    case InvalidationPoint:
        write(SideState);
        return;
        
    case VariableWatchpoint:
    case TypedArrayWatchpoint:
        read(Watchpoint_fire);
        write(SideState);
        return;
        
    case NotifyWrite:
        write(Watchpoint_fire);
        write(SideState);
        return;

    case CreateActivation:
    case CreateArguments:
        clobberizeForAllocation(read, write);
        write(SideState);
        write(Watchpoint_fire);
        return;
        
    case FunctionReentryWatchpoint:
        read(Watchpoint_fire);
        return;

    case ToThis:
    case CreateThis:
        read(MiscFields);
        clobberizeForAllocation(read, write);
        return;

    case VarInjectionWatchpoint:
    case AllocationProfileWatchpoint:
    case IsObject:
    case IsFunction:
    case TypeOf:
        read(MiscFields);
        return;
        
    case GetById:
    case GetByIdFlush:
    case PutById:
    case PutByIdDirect:
    case ArrayPush:
    case ArrayPop:
    case Call:
    case Construct:
    case ToPrimitive:
    case In:
    case GetMyArgumentsLengthSafe:
    case GetMyArgumentByValSafe:
    case ValueAdd:
        read(World);
        write(World);
        return;
        
    case GetCallee:
        read(AbstractHeap(Variables, JSStack::Callee));
        return;
        
    case GetLocal:
    case GetArgument:
        read(AbstractHeap(Variables, node->local()));
        return;
        
    case SetLocal:
        write(AbstractHeap(Variables, node->local()));
        return;
        
    case GetLocalUnlinked:
        read(AbstractHeap(Variables, node->unlinkedLocal()));
        return;
        
    case GetByVal: {
        ArrayMode mode = node->arrayMode();
        switch (mode.type()) {
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
        case Array::Undecided:
            // Assume the worst since we don't have profiling yet.
            read(World);
            write(World);
            return;
            
        case Array::ForceExit:
            write(SideState);
            return;
            
        case Array::Generic:
            read(World);
            write(World);
            return;
            
        case Array::String:
            if (mode.isOutOfBounds()) {
                read(World);
                write(World);
                return;
            }
            // This appears to read nothing because it's only reading immutable data.
            return;
            
        case Array::Arguments:
            read(Arguments_registers);
            read(Variables);
            return;
            
        case Array::Int32:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(Butterfly_vectorLength);
                read(IndexedInt32Properties);
                return;
            }
            read(World);
            write(World);
            return;
            
        case Array::Double:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(Butterfly_vectorLength);
                read(IndexedDoubleProperties);
                return;
            }
            read(World);
            write(World);
            return;
            
        case Array::Contiguous:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(Butterfly_vectorLength);
                read(IndexedContiguousProperties);
                return;
            }
            read(World);
            write(World);
            return;
            
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            // Give up on life for now.
            read(World);
            write(World);
            return;
            
        case Array::Int8Array:
        case Array::Int16Array:
        case Array::Int32Array:
        case Array::Uint8Array:
        case Array::Uint8ClampedArray:
        case Array::Uint16Array:
        case Array::Uint32Array:
        case Array::Float32Array:
        case Array::Float64Array:
            read(TypedArrayProperties);
            read(JSArrayBufferView_vector);
            read(JSArrayBufferView_length);
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }

    case PutByValDirect:
    case PutByVal:
    case PutByValAlias: {
        ArrayMode mode = node->arrayMode();
        switch (mode.modeForPut().type()) {
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
        case Array::Undecided:
        case Array::String:
            // Assume the worst since we don't have profiling yet.
            read(World);
            write(World);
            return;
            
        case Array::ForceExit:
            write(SideState);
            return;
            
        case Array::Generic:
            read(World);
            write(World);
            return;
            
        case Array::Arguments:
            read(Arguments_registers);
            read(Arguments_numArguments);
            read(Arguments_slowArguments);
            write(Variables);
            return;
            
        case Array::Int32:
            if (node->arrayMode().isOutOfBounds()) {
                read(World);
                write(World);
                return;
            }
            read(Butterfly_publicLength);
            read(Butterfly_vectorLength);
            read(IndexedInt32Properties);
            write(IndexedInt32Properties);
            return;
            
        case Array::Double:
            if (node->arrayMode().isOutOfBounds()) {
                read(World);
                write(World);
                return;
            }
            read(Butterfly_publicLength);
            read(Butterfly_vectorLength);
            read(IndexedDoubleProperties);
            write(IndexedDoubleProperties);
            return;
            
        case Array::Contiguous:
            if (node->arrayMode().isOutOfBounds()) {
                read(World);
                write(World);
                return;
            }
            read(Butterfly_publicLength);
            read(Butterfly_vectorLength);
            read(IndexedContiguousProperties);
            write(IndexedContiguousProperties);
            return;
            
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            // Give up on life for now.
            read(World);
            write(World);
            return;

        case Array::Int8Array:
        case Array::Int16Array:
        case Array::Int32Array:
        case Array::Uint8Array:
        case Array::Uint8ClampedArray:
        case Array::Uint16Array:
        case Array::Uint32Array:
        case Array::Float32Array:
        case Array::Float64Array:
            read(JSArrayBufferView_vector);
            read(JSArrayBufferView_length);
            write(TypedArrayProperties);
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
        
    case CheckStructure:
    case StructureTransitionWatchpoint:
    case CheckArray:
    case CheckHasInstance:
    case InstanceOf:
        read(JSCell_structure);
        return;
        
    case CheckExecutable:
        read(JSFunction_executable);
        return;
        
    case PutStructure:
    case PhantomPutStructure:
        write(JSCell_structure);
        return;
        
    case AllocatePropertyStorage:
        write(JSObject_butterfly);
        clobberizeForAllocation(read, write);
        return;
        
    case ReallocatePropertyStorage:
        read(JSObject_butterfly);
        write(JSObject_butterfly);
        clobberizeForAllocation(read, write);
        return;
        
    case GetButterfly:
        read(JSObject_butterfly);
        return;
        
    case Arrayify:
    case ArrayifyToStructure:
        read(JSCell_structure);
        read(JSObject_butterfly);
        write(JSCell_structure);
        write(JSObject_butterfly);
        clobberizeForAllocation(read, write);
        return;
        
    case GetIndexedPropertyStorage:
        if (node->arrayMode().type() == Array::String)
            return;
        read(JSArrayBufferView_vector);
        return;
        
    case GetTypedArrayByteOffset:
        read(JSArrayBufferView_vector);
        read(JSArrayBufferView_mode);
        read(Butterfly_arrayBuffer);
        read(ArrayBuffer_data);
        return;
        
    case GetByOffset:
        read(AbstractHeap(NamedProperties, graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber));
        return;
        
    case MultiGetByOffset:
        read(JSCell_structure);
        read(JSObject_butterfly);
        read(AbstractHeap(NamedProperties, node->multiGetByOffsetData().identifierNumber));
        return;
        
    case MultiPutByOffset:
        read(JSCell_structure);
        read(JSObject_butterfly);
        write(AbstractHeap(NamedProperties, node->multiPutByOffsetData().identifierNumber));
        if (node->multiPutByOffsetData().writesStructures())
            write(JSCell_structure);
        if (node->multiPutByOffsetData().reallocatesStorage()) {
            write(JSObject_butterfly);
            clobberizeForAllocation(read, write);
        }
        return;
        
    case PutByOffset:
        write(AbstractHeap(NamedProperties, graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber));
        return;
        
    case GetArrayLength: {
        ArrayMode mode = node->arrayMode();
        switch (mode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            read(Butterfly_publicLength);
            return;
            
        case Array::String:
            return;
            
        case Array::Arguments:
            read(Arguments_overrideLength);
            read(Arguments_numArguments);
            return;
            
        default:
            read(JSArrayBufferView_length);
            return;
        }
    }
        
    case GetMyScope:
        read(AbstractHeap(Variables, JSStack::ScopeChain));
        return;
        
    case SkipTopScope:
        read(AbstractHeap(Variables, graph.activationRegister()));
        return;
        
    case GetClosureRegisters:
        read(JSVariableObject_registers);
        return;
        
    case GetClosureVar:
        read(AbstractHeap(Variables, node->varNumber()));
        return;
        
    case PutClosureVar:
        write(AbstractHeap(Variables, node->varNumber()));
        return;
        
    case GetGlobalVar:
        read(AbstractHeap(Absolute, node->registerPointer()));
        return;
        
    case PutGlobalVar:
        write(AbstractHeap(Absolute, node->registerPointer()));
        return;

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
        clobberizeForAllocation(read, write);
        return;
        
    case NewTypedArray:
        clobberizeForAllocation(read, write);
        switch (node->child1().useKind()) {
        case Int32Use:
            return;
        case UntypedUse:
            read(World);
            write(World);
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
        
    case RegExpExec:
    case RegExpTest:
        read(RegExpState);
        write(RegExpState);
        return;

    case StringCharAt:
        if (node->arrayMode().isOutOfBounds()) {
            read(World);
            write(World);
            return;
        }
        return;
        
    case CompareEq:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
        if (!node->isBinaryUseKind(UntypedUse))
            return;
        read(World);
        write(World);
        return;
        
    case ToString:
        switch (node->child1().useKind()) {
        case StringObjectUse:
        case StringOrStringObjectUse:
            return;
            
        case CellUse:
        case UntypedUse:
            read(World);
            write(World);
            return;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }

    case TearOffActivation:
        write(JSVariableObject_registers);
        return;
        
    case TearOffArguments:
        write(Arguments_registers);
        return;
        
    case GetMyArgumentsLength:
        read(AbstractHeap(Variables, graph.argumentsRegisterFor(node->origin.semantic)));
        read(AbstractHeap(Variables, JSStack::ArgumentCount));
        return;
        
    case GetMyArgumentByVal:
        read(Variables);
        return;
        
    case CheckArgumentsNotCreated:
        read(AbstractHeap(Variables, graph.argumentsRegisterFor(node->origin.semantic)));
        return;

    case ThrowReferenceError:
        write(SideState);
        clobberizeForAllocation(read, write);
        return;
        
    case CountExecution:
    case CheckWatchdogTimer:
        read(InternalState);
        write(InternalState);
        return;

    case StoreBarrier:
    case ConditionalStoreBarrier:
    case StoreBarrierWithNullCheck:
        read(BarrierState);
        write(BarrierState);
        return;
        
    case LastNodeType:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

class NoOpClobberize {
public:
    NoOpClobberize() { }
    void operator()(AbstractHeap) { }
};

class CheckClobberize {
public:
    CheckClobberize()
        : m_result(false)
    {
    }
    
    void operator()(AbstractHeap) { m_result = true; }
    
    bool result() const { return m_result; }
    
private:
    bool m_result;
};

bool doesWrites(Graph&, Node*);

class AbstractHeapOverlaps {
public:
    AbstractHeapOverlaps(AbstractHeap heap)
        : m_heap(heap)
        , m_result(false)
    {
    }
    
    void operator()(AbstractHeap otherHeap)
    {
        if (m_result)
            return;
        m_result = m_heap.overlaps(otherHeap);
    }
    
    bool result() const { return m_result; }

private:
    AbstractHeap m_heap;
    bool m_result;
};

bool writesOverlap(Graph&, Node*, AbstractHeap);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGClobberize_h

