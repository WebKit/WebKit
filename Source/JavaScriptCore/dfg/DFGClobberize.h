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
#include "DFGHeapLocation.h"
#include "DFGPureValue.h"

namespace JSC { namespace DFG {

template<typename ReadFunctor, typename WriteFunctor, typename DefFunctor>
void clobberize(Graph& graph, Node* node, ReadFunctor& read, WriteFunctor& write, DefFunctor& def)
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
    // - Some nodes lie, and claim that they do not read the JSCell_structureID, JSCell_typeInfoFlags, etc.
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
    
    // While read() and write() are fairly self-explanatory - they track what sorts of things the
    // node may read or write - the def() functor is more tricky. It tells you the heap locations
    // (not just abstract heaps) that are defined by a node. A heap location comprises an abstract
    // heap, some nodes, and a LocationKind. Briefly, a location defined by a node is a location
    // whose value can be deduced from looking at the node itself. The locations returned must obey
    // the following properties:
    //
    // - If someone wants to CSE a load from the heap, then a HeapLocation object should be
    //   sufficient to find a single matching node.
    //
    // - The abstract heap is the only abstract heap that could be clobbered to invalidate any such
    //   CSE attempt. I.e. if clobberize() reports that on every path between some node and a node
    //   that defines a HeapLocation that it wanted, there were no writes to any abstract heap that
    //   overlap the location's heap, then we have a sound match. Effectively, the semantics of
    //   write() and def() are intertwined such that for them to be sound they must agree on what
    //   is CSEable.
    //
    // read(), write(), and def() for heap locations is enough to do GCSE on effectful things. To
    // keep things simple, this code will also def() pure things. def() must be overloaded to also
    // accept PureValue. This way, a client of clobberize() can implement GCSE entirely using the
    // information that clobberize() passes to write() and def(). Other clients of clobberize() can
    // just ignore def() by using a NoOpClobberize functor.

    if (edgesUseStructure(graph, node))
        read(JSCell_structureID);
    
    switch (node->op()) {
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
        def(PureValue(node, node->constant()));
        return;
        
    case Identity:
    case Phantom:
    case HardPhantom:
    case Check:
    case ExtractOSREntryLocal:
        return;
        
    case BitAnd:
    case BitOr:
    case BitXor:
    case BitLShift:
    case BitRShift:
    case BitURShift:
    case ArithIMul:
    case ArithAbs:
    case ArithMin:
    case ArithMax:
    case ArithSqrt:
    case ArithFRound:
    case ArithSin:
    case ArithCos:
    case GetScope:
    case SkipScope:
    case StringCharCodeAt:
    case StringFromCharCode:
    case CompareEqConstant:
    case CompareStrictEq:
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case LogicalNot:
    case CheckInBounds:
    case DoubleRep:
    case ValueRep:
    case Int52Rep:
    case BooleanToNumber:
    case FiatInt52:
    case MakeRope:
    case ValueToInt32:
    case GetExecutable:
    case BottomValue:
        def(PureValue(node));
        return;
        
    case HasGenericProperty:
    case HasStructureProperty:
    case GetEnumerableLength:
    case GetStructurePropertyEnumerator:
    case GetGenericPropertyEnumerator: {
        read(World);
        write(SideState);
        return;
    }

    case GetDirectPname: {
        // This reads and writes world because it can end up calling a generic getByVal 
        // if the Structure changed, which could in turn end up calling a getter.
        read(World);
        write(World);
        return;
    }

    case ToIndexString:
    case GetEnumeratorPname: {
        def(PureValue(node));
        return;
    }

    case HasIndexedProperty: {
        read(JSObject_butterfly);
        ArrayMode mode = node->arrayMode();
        switch (mode.type()) {
        case Array::Int32: {
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedInt32Properties);
                def(HeapLocation(HasIndexedPropertyLoc, IndexedInt32Properties, node->child1(), node->child2()), node);
                return;
            }
            read(World);
            return;
        }
            
        case Array::Double: {
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedDoubleProperties);
                def(HeapLocation(HasIndexedPropertyLoc, IndexedDoubleProperties, node->child1(), node->child2()), node);
                return;
            }
            read(World);
            return;
        }
            
        case Array::Contiguous: {
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedContiguousProperties);
                def(HeapLocation(HasIndexedPropertyLoc, IndexedContiguousProperties, node->child1(), node->child2()), node);
                return;
            }
            read(World);
            return;
        }

        case Array::ArrayStorage: {
            if (mode.isInBounds()) {
                read(Butterfly_vectorLength);
                read(IndexedArrayStorageProperties);
                return;
            }
            read(World);
            return;
        }

        default: {
            read(World);
            write(World);
            return;
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }

    case ArithAdd:
    case ArithSub:
    case ArithNegate:
    case ArithMul:
    case ArithDiv:
    case ArithMod:
    case DoubleAsInt32:
    case UInt32ToNumber:
        def(PureValue(node, node->arithMode()));
        return;
        
    case CheckCell:
        def(PureValue(CheckCell, AdjacencyList(AdjacencyList::Fixed, node->child1()), node->cellOperand()));
        return;
        
    case ConstantStoragePointer:
        def(PureValue(node, node->storagePointer()));
        return;
         
    case MovHint:
    case ZombieHint:
    case Upsilon:
    case Phi:
    case PhantomLocal:
    case SetArgument:
    case PhantomArguments:
    case Jump:
    case Branch:
    case Switch:
    case Throw:
    case ForceOSRExit:
    case CheckBadCell:
    case Return:
    case Unreachable:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
    case CheckTierUpAndOSREnter:
    case LoopHint:
    case Breakpoint:
    case ProfileWillCall:
    case ProfileDidCall:
    case StoreBarrier:
    case StoreBarrierWithNullCheck:
        write(SideState);
        return;
        
    case InvalidationPoint:
        write(SideState);
        def(HeapLocation(InvalidationPointLoc, Watchpoint_fire), node);
        return;

    case Flush:
        read(AbstractHeap(Variables, node->local()));
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
        read(HeapObjectCount);
        write(HeapObjectCount);
        write(SideState);
        write(Watchpoint_fire);
        return;
        
    case CreateArguments:
        read(Variables);
        read(HeapObjectCount);
        write(HeapObjectCount);
        write(SideState);
        write(Watchpoint_fire);
        return;

    case FunctionReentryWatchpoint:
        read(Watchpoint_fire);
        return;

    case ToThis:
    case CreateThis:
        read(MiscFields);
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

    case VarInjectionWatchpoint:
        read(MiscFields);
        def(HeapLocation(VarInjectionWatchpointLoc, MiscFields), node);
        return;

    case AllocationProfileWatchpoint:
        read(MiscFields);
        def(HeapLocation(AllocationProfileWatchpointLoc, MiscFields), node);
        return;
        
    case IsObject:
        read(MiscFields);
        def(HeapLocation(IsObjectLoc, MiscFields, node->child1()), node);
        return;
        
    case IsFunction:
        read(MiscFields);
        def(HeapLocation(IsFunctionLoc, MiscFields, node->child1()), node);
        return;
        
    case TypeOf:
        read(MiscFields);
        def(HeapLocation(TypeOfLoc, MiscFields, node->child1()), node);
        return;

    case GetById:
    case GetByIdFlush:
    case PutById:
    case PutByIdFlush:
    case PutByIdDirect:
    case ArrayPush:
    case ArrayPop:
    case Call:
    case Construct:
    case ProfiledCall:
    case ProfiledConstruct:
    case NativeCall:
    case NativeConstruct:
    case ToPrimitive:
    case In:
    case GetMyArgumentsLengthSafe:
    case GetMyArgumentByValSafe:
    case ValueAdd:
        read(World);
        write(World);
        return;
        
    case GetGetter:
        read(GetterSetter_getter);
        def(HeapLocation(GetterLoc, GetterSetter_getter, node->child1()), node);
        return;
        
    case GetSetter:
        read(GetterSetter_setter);
        def(HeapLocation(SetterLoc, GetterSetter_setter, node->child1()), node);
        return;
        
    case GetCallee:
        read(AbstractHeap(Variables, JSStack::Callee));
        def(HeapLocation(VariableLoc, AbstractHeap(Variables, JSStack::Callee)), node);
        return;
        
    case GetLocal:
    case GetArgument:
        read(AbstractHeap(Variables, node->local()));
        def(HeapLocation(VariableLoc, AbstractHeap(Variables, node->local())), node);
        return;
        
    case SetLocal:
        write(AbstractHeap(Variables, node->local()));
        def(HeapLocation(VariableLoc, AbstractHeap(Variables, node->local())), node->child1().node());
        return;
        
    case GetLocalUnlinked:
        read(AbstractHeap(Variables, node->unlinkedLocal()));
        def(HeapLocation(VariableLoc, AbstractHeap(Variables, node->unlinkedLocal())), node);
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
            def(PureValue(node, mode.asWord()));
            return;
            
        case Array::Arguments:
            read(Arguments_registers);
            read(Variables);
            def(HeapLocation(IndexedPropertyLoc, Variables, node->child1(), node->child2()), node);
            return;
            
        case Array::Int32:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedInt32Properties);
                def(HeapLocation(IndexedPropertyLoc, IndexedInt32Properties, node->child1(), node->child2()), node);
                return;
            }
            read(World);
            write(World);
            return;
            
        case Array::Double:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedDoubleProperties);
                def(HeapLocation(IndexedPropertyLoc, IndexedDoubleProperties, node->child1(), node->child2()), node);
                return;
            }
            read(World);
            write(World);
            return;
            
        case Array::Contiguous:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedContiguousProperties);
                def(HeapLocation(IndexedPropertyLoc, IndexedContiguousProperties, node->child1(), node->child2()), node);
                return;
            }
            read(World);
            write(World);
            return;
            
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            if (mode.isInBounds()) {
                read(Butterfly_vectorLength);
                read(IndexedArrayStorageProperties);
                return;
            }
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
            read(MiscFields);
            def(HeapLocation(IndexedPropertyLoc, TypedArrayProperties, node->child1(), node->child2()), node);
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }

    case PutByValDirect:
    case PutByVal:
    case PutByValAlias: {
        ArrayMode mode = node->arrayMode();
        Node* base = graph.varArgChild(node, 0).node();
        Node* index = graph.varArgChild(node, 1).node();
        Node* value = graph.varArgChild(node, 2).node();
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
            read(MiscFields);
            write(Variables);
            def(HeapLocation(IndexedPropertyLoc, Variables, base, index), value);
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
            if (node->arrayMode().mayStoreToHole())
                write(Butterfly_publicLength);
            def(HeapLocation(IndexedPropertyLoc, IndexedInt32Properties, base, index), value);
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
            if (node->arrayMode().mayStoreToHole())
                write(Butterfly_publicLength);
            def(HeapLocation(IndexedPropertyLoc, IndexedDoubleProperties, base, index), value);
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
            if (node->arrayMode().mayStoreToHole())
                write(Butterfly_publicLength);
            def(HeapLocation(IndexedPropertyLoc, IndexedContiguousProperties, base, index), value);
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
            read(MiscFields);
            write(TypedArrayProperties);
            // FIXME: We can't def() anything here because these operations truncate their inputs.
            // https://bugs.webkit.org/show_bug.cgi?id=134737
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
        
    case CheckStructure:
        read(JSCell_structureID);
        return;

    case CheckArray:
        read(JSCell_indexingType);
        read(JSCell_typeInfoType);
        read(JSCell_structureID);
        return;

    case CheckHasInstance:
        read(JSCell_typeInfoFlags);
        def(HeapLocation(CheckHasInstanceLoc, JSCell_typeInfoFlags, node->child1()), node);
        return;

    case InstanceOf:
        read(JSCell_structureID);
        def(HeapLocation(InstanceOfLoc, JSCell_structureID, node->child1(), node->child2()), node);
        return;

    case PutStructure:
        write(JSCell_structureID);
        write(JSCell_typeInfoType);
        write(JSCell_typeInfoFlags);
        write(JSCell_indexingType);
        return;
        
    case AllocatePropertyStorage:
        write(JSObject_butterfly);
        def(HeapLocation(ButterflyLoc, JSObject_butterfly, node->child1()), node);
        return;
        
    case ReallocatePropertyStorage:
        read(JSObject_butterfly);
        write(JSObject_butterfly);
        def(HeapLocation(ButterflyLoc, JSObject_butterfly, node->child1()), node);
        return;
        
    case GetButterfly:
        read(JSObject_butterfly);
        def(HeapLocation(ButterflyLoc, JSObject_butterfly, node->child1()), node);
        return;
        
    case Arrayify:
    case ArrayifyToStructure:
        read(JSCell_structureID);
        read(JSCell_indexingType);
        read(JSObject_butterfly);
        write(JSCell_structureID);
        write(JSCell_indexingType);
        write(JSObject_butterfly);
        write(Watchpoint_fire);
        return;
        
    case GetIndexedPropertyStorage:
        if (node->arrayMode().type() == Array::String) {
            def(PureValue(node, node->arrayMode().asWord()));
            return;
        }
        read(MiscFields);
        def(HeapLocation(IndexedPropertyStorageLoc, MiscFields, node->child1()), node);
        return;
        
    case GetTypedArrayByteOffset:
        read(MiscFields);
        def(HeapLocation(TypedArrayByteOffsetLoc, MiscFields, node->child1()), node);
        return;
        
    case GetByOffset:
    case GetGetterSetterByOffset: {
        unsigned identifierNumber =
            graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber;
        AbstractHeap heap(NamedProperties, identifierNumber);
        read(heap);
        def(HeapLocation(NamedPropertyLoc, heap, node->child2()), node);
        return;
    }
        
    case MultiGetByOffset: {
        read(JSCell_structureID);
        read(JSObject_butterfly);
        AbstractHeap heap(NamedProperties, node->multiGetByOffsetData().identifierNumber);
        read(heap);
        def(HeapLocation(NamedPropertyLoc, heap, node->child1()), node);
        return;
    }
        
    case MultiPutByOffset: {
        read(JSCell_structureID);
        read(JSObject_butterfly);
        AbstractHeap heap(NamedProperties, node->multiPutByOffsetData().identifierNumber);
        write(heap);
        if (node->multiPutByOffsetData().writesStructures())
            write(JSCell_structureID);
        if (node->multiPutByOffsetData().reallocatesStorage())
            write(JSObject_butterfly);
        def(HeapLocation(NamedPropertyLoc, heap, node->child1()), node->child2().node());
        return;
    }
        
    case PutByOffset: {
        unsigned identifierNumber =
            graph.m_storageAccessData[node->storageAccessDataIndex()].identifierNumber;
        AbstractHeap heap(NamedProperties, identifierNumber);
        write(heap);
        def(HeapLocation(NamedPropertyLoc, heap, node->child2()), node->child3().node());
        return;
    }
        
    case GetArrayLength: {
        ArrayMode mode = node->arrayMode();
        switch (mode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            read(Butterfly_publicLength);
            def(HeapLocation(ArrayLengthLoc, Butterfly_publicLength, node->child1()), node);
            return;
            
        case Array::String:
            def(PureValue(node, mode.asWord()));
            return;
            
        case Array::Arguments:
            read(MiscFields);
            def(HeapLocation(ArrayLengthLoc, MiscFields, node->child1()), node);
            return;
            
        default:
            ASSERT(mode.typedArrayType() != NotTypedArray);
            read(MiscFields);
            def(HeapLocation(ArrayLengthLoc, MiscFields, node->child1()), node);
            return;
        }
    }
        
    case GetMyScope:
        if (graph.m_codeBlock->needsActivation()) {
            read(AbstractHeap(Variables, JSStack::ScopeChain));
            def(HeapLocation(VariableLoc, AbstractHeap(Variables, JSStack::ScopeChain)), node);
        } else
            def(PureValue(node));
        return;
        
    case GetClosureRegisters:
        read(JSEnvironmentRecord_registers);
        def(HeapLocation(ClosureRegistersLoc, JSEnvironmentRecord_registers, node->child1()), node);
        return;

    case GetClosureVar:
        read(AbstractHeap(Variables, node->varNumber()));
        def(HeapLocation(ClosureVariableLoc, AbstractHeap(Variables, node->varNumber()), node->child1()), node);
        return;
        
    case PutClosureVar:
        write(AbstractHeap(Variables, node->varNumber()));
        def(HeapLocation(ClosureVariableLoc, AbstractHeap(Variables, node->varNumber()), node->child2()), node->child3().node());
        return;
        
    case GetGlobalVar:
        read(AbstractHeap(Absolute, node->registerPointer()));
        def(HeapLocation(GlobalVariableLoc, AbstractHeap(Absolute, node->registerPointer())), node);
        return;
        
    case PutGlobalVar:
        write(AbstractHeap(Absolute, node->registerPointer()));
        def(HeapLocation(GlobalVariableLoc, AbstractHeap(Absolute, node->registerPointer())), node->child1().node());
        return;

    case NewArray:
    case NewArrayWithSize:
    case NewArrayBuffer:
    case NewTypedArray:
        // FIXME: Enable CSE for these nodes. We can't do this right now because there is no way
        // for us to claim an index node and a value node. We could make this work if we lowered
        // these nodes or if we had a more flexible way of def()'ing.
        // https://bugs.webkit.org/show_bug.cgi?id=134737
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

    case NewObject:
    case NewRegexp:
    case NewStringObject:
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;
        
    case NewFunctionNoCheck:
    case NewFunction:
    case NewFunctionExpression:
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

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
        def(PureValue(node));
        return;
        
    case CompareEq:
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
        if (!node->isBinaryUseKind(UntypedUse)) {
            def(PureValue(node));
            return;
        }
        read(World);
        write(World);
        return;
        
    case ToString:
        switch (node->child1().useKind()) {
        case StringObjectUse:
        case StringOrStringObjectUse:
            // These don't def a pure value, unfortunately. I'll avoid load-eliminating these for
            // now.
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
        read(Variables);
        write(JSEnvironmentRecord_registers);
        return;
        
    case TearOffArguments:
        read(Variables);
        write(Arguments_registers);
        return;
        
    case GetMyArgumentsLength:
        read(AbstractHeap(Variables, graph.argumentsRegisterFor(node->origin.semantic)));
        read(AbstractHeap(Variables, JSStack::ArgumentCount));
        // FIXME: We could def() this by specifying the code origin as a kind of m_info, like we
        // have for PureValue.
        // https://bugs.webkit.org/show_bug.cgi?id=134797
        return;
        
    case GetMyArgumentByVal:
        read(Variables);
        // FIXME: We could def() this by specifying the code origin as a kind of m_info, like we
        // have for PureValue.
        // https://bugs.webkit.org/show_bug.cgi?id=134797
        return;
        
    case CheckArgumentsNotCreated:
        read(AbstractHeap(Variables, graph.argumentsRegisterFor(node->origin.semantic)));
        return;

    case ThrowReferenceError:
        write(SideState);
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;
        
    case CountExecution:
    case CheckWatchdogTimer:
        read(InternalState);
        write(InternalState);
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
    template<typename... T>
    void operator()(T...) { }
};

class CheckClobberize {
public:
    CheckClobberize()
        : m_result(false)
    {
    }
    
    template<typename... T>
    void operator()(T...) { m_result = true; }
    
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

bool accessesOverlap(Graph&, Node*, AbstractHeap);
bool writesOverlap(Graph&, Node*, AbstractHeap);

// We would have used bind() for these, but because of the overlaoding that we are doing,
// it's quite a bit of clearer to just write this out the traditional way.

template<typename T>
class ReadMethodClobberize {
public:
    ReadMethodClobberize(T& value)
        : m_value(value)
    {
    }
    
    void operator()(AbstractHeap heap)
    {
        m_value.read(heap);
    }
private:
    T& m_value;
};

template<typename T>
class WriteMethodClobberize {
public:
    WriteMethodClobberize(T& value)
        : m_value(value)
    {
    }
    
    void operator()(AbstractHeap heap)
    {
        m_value.write(heap);
    }
private:
    T& m_value;
};

template<typename T>
class DefMethodClobberize {
public:
    DefMethodClobberize(T& value)
        : m_value(value)
    {
    }
    
    void operator()(PureValue value)
    {
        m_value.def(value);
    }
    
    void operator()(HeapLocation location, Node* node)
    {
        m_value.def(location, node);
    }

private:
    T& m_value;
};

template<typename Adaptor>
void clobberize(Graph& graph, Node* node, Adaptor& adaptor)
{
    ReadMethodClobberize<Adaptor> read(adaptor);
    WriteMethodClobberize<Adaptor> write(adaptor);
    DefMethodClobberize<Adaptor> def(adaptor);
    clobberize(graph, node, read, write, def);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGClobberize_h

