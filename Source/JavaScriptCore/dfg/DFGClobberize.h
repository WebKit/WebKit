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

#ifndef DFGClobberize_h
#define DFGClobberize_h

#if ENABLE(DFG_JIT)

#include "DFGAbstractHeap.h"
#include "DFGEdgeUsesStructure.h"
#include "DFGGraph.h"
#include "DFGHeapLocation.h"
#include "DFGLazyNode.h"
#include "DFGPureValue.h"

namespace JSC { namespace DFG {

template<typename ReadFunctor, typename WriteFunctor, typename DefFunctor>
void clobberize(Graph& graph, Node* node, const ReadFunctor& read, const WriteFunctor& write, const DefFunctor& def)
{
    // Some notes:
    //
    // - The canonical way of clobbering the world is to read world and write
    //   heap. This is because World subsumes Heap and Stack, and Stack can be
    //   read by anyone but only written to by explicit stack writing operations.
    //   Of course, claiming to also write World is not wrong; it'll just
    //   pessimise some important optimizations.
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
    // - Some nodes lie, and claim that they do not read the JSCell_structureID,
    //   JSCell_typeInfoFlags, etc. These are nodes that use the structure in a way
    //   that does not depend on things that change under structure transitions.
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
    //
    // - If you do read(Stack) or read(World), then make sure that readTop() in
    //   PreciseLocalClobberize is correct.
    
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
    
    // We allow the runtime to perform a stack scan at any time. We don't model which nodes get implemented
    // by calls into the runtime. For debugging we might replace the implementation of any node with a call
    // to the runtime, and that call may walk stack. Therefore, each node must read() anything that a stack
    // scan would read. That's what this does.
    for (InlineCallFrame* inlineCallFrame = node->origin.semantic.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->directCaller.inlineCallFrame) {
        if (inlineCallFrame->isClosureCall)
            read(AbstractHeap(Stack, inlineCallFrame->stackOffset + JSStack::Callee));
        if (inlineCallFrame->isVarargs())
            read(AbstractHeap(Stack, inlineCallFrame->stackOffset + JSStack::ArgumentCount));
    }
    
    switch (node->op()) {
    case JSConstant:
    case DoubleConstant:
    case Int52Constant:
        def(PureValue(node, node->constant()));
        return;

    case Identity:
    case Phantom:
    case Check:
    case ExtractOSREntryLocal:
    case CheckStructureImmediate:
        return;
        
    case LazyJSConstant:
        // We should enable CSE of LazyJSConstant. It's a little annoying since LazyJSValue has
        // more bits than we currently have in PureValue.
        return;
        
    case ArithIMul:
    case ArithAbs:
    case ArithClz32:
    case ArithMin:
    case ArithMax:
    case ArithPow:
    case ArithSqrt:
    case ArithFRound:
    case ArithSin:
    case ArithCos:
    case ArithLog:
    case GetScope:
    case SkipScope:
    case GetGlobalObject:
    case StringCharCodeAt:
    case CompareStrictEq:
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case LogicalNot:
    case CheckInBounds:
    case DoubleRep:
    case ValueRep:
    case Int52Rep:
    case BooleanToNumber:
    case FiatInt52:
    case MakeRope:
    case StrCat:
    case ValueToInt32:
    case GetExecutable:
    case BottomValue:
    case TypeOf:
        def(PureValue(node));
        return;

    case BitAnd:
    case BitOr:
    case BitXor:
    case BitLShift:
    case BitRShift:
    case BitURShift:
        if (node->child1().useKind() == UntypedUse || node->child2().useKind() == UntypedUse) {
            read(World);
            write(Heap);
            return;
        }
        def(PureValue(node));
        return;

    case ArithRandom:
        read(MathDotRandomState);
        write(MathDotRandomState);
        return;
        
    case HasGenericProperty:
    case HasStructureProperty:
    case GetEnumerableLength:
    case GetPropertyEnumerator: {
        read(Heap);
        write(SideState);
        return;
    }

    case GetDirectPname: {
        // This reads and writes heap because it can end up calling a generic getByVal 
        // if the Structure changed, which could in turn end up calling a getter.
        read(World);
        write(Heap);
        return;
    }

    case ToIndexString:
    case GetEnumeratorStructurePname:
    case GetEnumeratorGenericPname: {
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
                def(HeapLocation(HasIndexedPropertyLoc, IndexedInt32Properties, node->child1(), node->child2()), LazyNode(node));
                return;
            }
            read(Heap);
            return;
        }
            
        case Array::Double: {
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedDoubleProperties);
                def(HeapLocation(HasIndexedPropertyLoc, IndexedDoubleProperties, node->child1(), node->child2()), LazyNode(node));
                return;
            }
            read(Heap);
            return;
        }
            
        case Array::Contiguous: {
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedContiguousProperties);
                def(HeapLocation(HasIndexedPropertyLoc, IndexedContiguousProperties, node->child1(), node->child2()), LazyNode(node));
                return;
            }
            read(Heap);
            return;
        }

        case Array::ArrayStorage: {
            if (mode.isInBounds()) {
                read(Butterfly_vectorLength);
                read(IndexedArrayStorageProperties);
                return;
            }
            read(Heap);
            return;
        }

        default: {
            read(World);
            write(Heap);
            return;
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }

    case StringFromCharCode:
        switch (node->child1().useKind()) {
        case Int32Use:
            def(PureValue(node));
            return;
        case UntypedUse:
            read(World);
            write(Heap);
            return;
        default:
            DFG_CRASH(graph, node, "Bad use kind");
        }
        return;

    case ArithAdd:
    case ArithNegate:
    case ArithMod:
    case DoubleAsInt32:
    case UInt32ToNumber:
        def(PureValue(node, node->arithMode()));
        return;

    case ArithDiv:
    case ArithMul:
    case ArithSub:
        switch (node->binaryUseKind()) {
        case Int32Use:
        case Int52RepUse:
        case DoubleRepUse:
            def(PureValue(node, node->arithMode()));
            return;
        case UntypedUse:
            read(World);
            write(Heap);
            return;
        default:
            DFG_CRASH(graph, node, "Bad use kind");
        }

    case ArithRound:
    case ArithFloor:
    case ArithCeil:
    case ArithTrunc:
        def(PureValue(node, static_cast<uintptr_t>(node->arithRoundingMode())));
        return;

    case CheckCell:
        def(PureValue(CheckCell, AdjacencyList(AdjacencyList::Fixed, node->child1()), node->cellOperand()));
        return;

    case CheckNotEmpty:
        def(PureValue(CheckNotEmpty, AdjacencyList(AdjacencyList::Fixed, node->child1())));
        return;

    case CheckIdent:
        def(PureValue(CheckIdent, AdjacencyList(AdjacencyList::Fixed, node->child1()), node->uidOperand()));
        return;

    case ConstantStoragePointer:
        def(PureValue(node, node->storagePointer()));
        return;
         
    case MovHint:
    case ZombieHint:
    case ExitOK:
    case KillStack:
    case Upsilon:
    case Phi:
    case PhantomLocal:
    case SetArgument:
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
    case ProfileWillCall:
    case ProfileDidCall:
    case ProfileType:
    case ProfileControlFlow:
    case StoreBarrier:
    case PutHint:
        write(SideState);
        return;
        
    case InvalidationPoint:
        write(SideState);
        def(HeapLocation(InvalidationPointLoc, Watchpoint_fire), LazyNode(node));
        return;

    case Flush:
        read(AbstractHeap(Stack, node->local()));
        write(SideState);
        return;

    case NotifyWrite:
        write(Watchpoint_fire);
        write(SideState);
        return;

    case CreateActivation: {
        SymbolTable* table = node->castOperand<SymbolTable*>();
        if (table->singletonScope()->isStillValid())
            write(Watchpoint_fire);
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;
    }
        
    case CreateDirectArguments:
    case CreateScopedArguments:
    case CreateClonedArguments:
        read(Stack);
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

    case PhantomDirectArguments:
    case PhantomClonedArguments:
        // DFG backend requires that the locals that this reads are flushed. FTL backend can handle those
        // locals being promoted.
        if (!isFTL(graph.m_plan.mode))
            read(Stack);
        
        // Even though it's phantom, it still has the property that one can't be replaced with another.
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

    case ToThis:
    case CreateThis:
        read(MiscFields);
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

    case VarInjectionWatchpoint:
        read(MiscFields);
        def(HeapLocation(VarInjectionWatchpointLoc, MiscFields), LazyNode(node));
        return;

    case IsObjectOrNull:
        read(MiscFields);
        def(HeapLocation(IsObjectOrNullLoc, MiscFields, node->child1()), LazyNode(node));
        return;
        
    case IsFunction:
        read(MiscFields);
        def(HeapLocation(IsFunctionLoc, MiscFields, node->child1()), LazyNode(node));
        return;
        
    case GetById:
    case GetByIdFlush:
    case PutById:
    case PutByIdFlush:
    case PutByIdDirect:
    case PutGetterById:
    case PutSetterById:
    case PutGetterSetterById:
    case PutGetterByVal:
    case PutSetterByVal:
    case ArrayPush:
    case ArrayPop:
    case Call:
    case TailCallInlinedCaller:
    case Construct:
    case CallVarargs:
    case CallForwardVarargs:
    case TailCallVarargsInlinedCaller:
    case TailCallForwardVarargsInlinedCaller:
    case ConstructVarargs:
    case ConstructForwardVarargs:
    case ToPrimitive:
    case In:
    case ValueAdd:
    case SetFunctionName:
        read(World);
        write(Heap);
        return;

    case TailCall:
    case TailCallVarargs:
    case TailCallForwardVarargs:
        read(World);
        write(SideState);
        return;
        
    case GetGetter:
        read(GetterSetter_getter);
        def(HeapLocation(GetterLoc, GetterSetter_getter, node->child1()), LazyNode(node));
        return;
        
    case GetSetter:
        read(GetterSetter_setter);
        def(HeapLocation(SetterLoc, GetterSetter_setter, node->child1()), LazyNode(node));
        return;
        
    case GetCallee:
        read(AbstractHeap(Stack, JSStack::Callee));
        def(HeapLocation(StackLoc, AbstractHeap(Stack, JSStack::Callee)), LazyNode(node));
        return;
        
    case GetArgumentCount:
        read(AbstractHeap(Stack, JSStack::ArgumentCount));
        def(HeapLocation(StackPayloadLoc, AbstractHeap(Stack, JSStack::ArgumentCount)), LazyNode(node));
        return;

    case GetRestLength:
        read(Stack);
        return;
        
    case GetLocal:
        read(AbstractHeap(Stack, node->local()));
        def(HeapLocation(StackLoc, AbstractHeap(Stack, node->local())), LazyNode(node));
        return;
        
    case SetLocal:
        write(AbstractHeap(Stack, node->local()));
        def(HeapLocation(StackLoc, AbstractHeap(Stack, node->local())), LazyNode(node->child1().node()));
        return;
        
    case GetStack: {
        AbstractHeap heap(Stack, node->stackAccessData()->local);
        read(heap);
        def(HeapLocation(StackLoc, heap), LazyNode(node));
        return;
    }
        
    case PutStack: {
        AbstractHeap heap(Stack, node->stackAccessData()->local);
        write(heap);
        def(HeapLocation(StackLoc, heap), LazyNode(node->child1().node()));
        return;
    }
        
    case LoadVarargs: {
        read(World);
        write(Heap);
        LoadVarargsData* data = node->loadVarargsData();
        write(AbstractHeap(Stack, data->count.offset()));
        for (unsigned i = data->limit; i--;)
            write(AbstractHeap(Stack, data->start.offset() + static_cast<int>(i)));
        return;
    }
        
    case ForwardVarargs: {
        // We could be way more precise here.
        read(Stack);
        
        LoadVarargsData* data = node->loadVarargsData();
        write(AbstractHeap(Stack, data->count.offset()));
        for (unsigned i = data->limit; i--;)
            write(AbstractHeap(Stack, data->start.offset() + static_cast<int>(i)));
        return;
    }
        
    case GetLocalUnlinked:
        read(AbstractHeap(Stack, node->unlinkedLocal()));
        def(HeapLocation(StackLoc, AbstractHeap(Stack, node->unlinkedLocal())), LazyNode(node));
        return;
        
    case GetByVal: {
        ArrayMode mode = node->arrayMode();
        switch (mode.type()) {
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
        case Array::SelectUsingArguments:
            // Assume the worst since we don't have profiling yet.
            read(World);
            write(Heap);
            return;
            
        case Array::ForceExit:
            write(SideState);
            return;
            
        case Array::Generic:
            read(World);
            write(Heap);
            return;
            
        case Array::String:
            if (mode.isOutOfBounds()) {
                read(World);
                write(Heap);
                return;
            }
            // This appears to read nothing because it's only reading immutable data.
            def(PureValue(node, mode.asWord()));
            return;
            
        case Array::DirectArguments:
            read(DirectArgumentsProperties);
            def(HeapLocation(IndexedPropertyLoc, DirectArgumentsProperties, node->child1(), node->child2()), LazyNode(node));
            return;
            
        case Array::ScopedArguments:
            read(ScopeProperties);
            def(HeapLocation(IndexedPropertyLoc, ScopeProperties, node->child1(), node->child2()), LazyNode(node));
            return;
            
        case Array::Int32:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedInt32Properties);
                def(HeapLocation(IndexedPropertyLoc, IndexedInt32Properties, node->child1(), node->child2()), LazyNode(node));
                return;
            }
            read(World);
            write(Heap);
            return;
            
        case Array::Double:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedDoubleProperties);
                def(HeapLocation(IndexedPropertyLoc, IndexedDoubleProperties, node->child1(), node->child2()), LazyNode(node));
                return;
            }
            read(World);
            write(Heap);
            return;
            
        case Array::Contiguous:
            if (mode.isInBounds()) {
                read(Butterfly_publicLength);
                read(IndexedContiguousProperties);
                def(HeapLocation(IndexedPropertyLoc, IndexedContiguousProperties, node->child1(), node->child2()), LazyNode(node));
                return;
            }
            read(World);
            write(Heap);
            return;

        case Array::Undecided:
            def(PureValue(node));
            return;
            
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            if (mode.isInBounds()) {
                read(Butterfly_vectorLength);
                read(IndexedArrayStorageProperties);
                return;
            }
            read(World);
            write(Heap);
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
            def(HeapLocation(IndexedPropertyLoc, TypedArrayProperties, node->child1(), node->child2()), LazyNode(node));
            return;
        // We should not get an AnyTypedArray in a GetByVal as AnyTypedArray is only created from intrinsics, which
        // are only added from Inline Caching a GetById.
        case Array::AnyTypedArray:
            DFG_CRASH(graph, node, "impossible array mode for get");
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
        
    case GetMyArgumentByVal: {
        read(Stack);
        // FIXME: It would be trivial to have a def here.
        // https://bugs.webkit.org/show_bug.cgi?id=143077
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
        case Array::SelectUsingArguments:
        case Array::Unprofiled:
        case Array::Undecided:
            // Assume the worst since we don't have profiling yet.
            read(World);
            write(Heap);
            return;
            
        case Array::ForceExit:
            write(SideState);
            return;
            
        case Array::Generic:
            read(World);
            write(Heap);
            return;
            
        case Array::Int32:
            if (node->arrayMode().isOutOfBounds()) {
                read(World);
                write(Heap);
                return;
            }
            read(Butterfly_publicLength);
            read(Butterfly_vectorLength);
            read(IndexedInt32Properties);
            write(IndexedInt32Properties);
            if (node->arrayMode().mayStoreToHole())
                write(Butterfly_publicLength);
            def(HeapLocation(IndexedPropertyLoc, IndexedInt32Properties, base, index), LazyNode(value));
            return;
            
        case Array::Double:
            if (node->arrayMode().isOutOfBounds()) {
                read(World);
                write(Heap);
                return;
            }
            read(Butterfly_publicLength);
            read(Butterfly_vectorLength);
            read(IndexedDoubleProperties);
            write(IndexedDoubleProperties);
            if (node->arrayMode().mayStoreToHole())
                write(Butterfly_publicLength);
            def(HeapLocation(IndexedPropertyLoc, IndexedDoubleProperties, base, index), LazyNode(value));
            return;
            
        case Array::Contiguous:
            if (node->arrayMode().isOutOfBounds()) {
                read(World);
                write(Heap);
                return;
            }
            read(Butterfly_publicLength);
            read(Butterfly_vectorLength);
            read(IndexedContiguousProperties);
            write(IndexedContiguousProperties);
            if (node->arrayMode().mayStoreToHole())
                write(Butterfly_publicLength);
            def(HeapLocation(IndexedPropertyLoc, IndexedContiguousProperties, base, index), LazyNode(value));
            return;
            
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            // Give up on life for now.
            read(World);
            write(Heap);
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
        case Array::AnyTypedArray:
        case Array::String:
        case Array::DirectArguments:
        case Array::ScopedArguments:
            DFG_CRASH(graph, node, "impossible array mode for put");
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

    case CheckTypeInfoFlags:
        read(JSCell_typeInfoFlags);
        def(HeapLocation(CheckTypeInfoFlagsLoc, JSCell_typeInfoFlags, node->child1()), LazyNode(node));
        return;

    case OverridesHasInstance:
        read(JSCell_typeInfoFlags);
        def(HeapLocation(OverridesHasInstanceLoc, JSCell_typeInfoFlags, node->child1()), LazyNode(node));
        return;

    case InstanceOf:
        read(JSCell_structureID);
        def(HeapLocation(InstanceOfLoc, JSCell_structureID, node->child1(), node->child2()), LazyNode(node));
        return;

    case InstanceOfCustom:
        read(World);
        write(Heap);
        return;

    case PutStructure:
        write(JSCell_structureID);
        write(JSCell_typeInfoType);
        write(JSCell_typeInfoFlags);
        write(JSCell_indexingType);
        return;
        
    case AllocatePropertyStorage:
        write(JSObject_butterfly);
        def(HeapLocation(ButterflyLoc, JSObject_butterfly, node->child1()), LazyNode(node));
        return;
        
    case ReallocatePropertyStorage:
        read(JSObject_butterfly);
        write(JSObject_butterfly);
        def(HeapLocation(ButterflyLoc, JSObject_butterfly, node->child1()), LazyNode(node));
        return;
        
    case GetButterfly:
        read(JSObject_butterfly);
        def(HeapLocation(ButterflyLoc, JSObject_butterfly, node->child1()), LazyNode(node));
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
        def(HeapLocation(IndexedPropertyStorageLoc, MiscFields, node->child1()), LazyNode(node));
        return;
        
    case GetTypedArrayByteOffset:
        read(MiscFields);
        def(HeapLocation(TypedArrayByteOffsetLoc, MiscFields, node->child1()), LazyNode(node));
        return;
        
    case GetByOffset:
    case GetGetterSetterByOffset: {
        unsigned identifierNumber = node->storageAccessData().identifierNumber;
        AbstractHeap heap(NamedProperties, identifierNumber);
        read(heap);
        def(HeapLocation(NamedPropertyLoc, heap, node->child2()), LazyNode(node));
        return;
    }

    case TryGetById: {
        read(Heap);
        return;
    }

    case MultiGetByOffset: {
        read(JSCell_structureID);
        read(JSObject_butterfly);
        AbstractHeap heap(NamedProperties, node->multiGetByOffsetData().identifierNumber);
        read(heap);
        def(HeapLocation(NamedPropertyLoc, heap, node->child1()), LazyNode(node));
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
        def(HeapLocation(NamedPropertyLoc, heap, node->child1()), LazyNode(node->child2().node()));
        return;
    }
        
    case PutByOffset: {
        unsigned identifierNumber = node->storageAccessData().identifierNumber;
        AbstractHeap heap(NamedProperties, identifierNumber);
        write(heap);
        def(HeapLocation(NamedPropertyLoc, heap, node->child2()), LazyNode(node->child3().node()));
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
            def(HeapLocation(ArrayLengthLoc, Butterfly_publicLength, node->child1()), LazyNode(node));
            return;
            
        case Array::String:
            def(PureValue(node, mode.asWord()));
            return;

        case Array::DirectArguments:
        case Array::ScopedArguments:
            read(MiscFields);
            def(HeapLocation(ArrayLengthLoc, MiscFields, node->child1()), LazyNode(node));
            return;

        default:
            ASSERT(mode.isSomeTypedArrayView());
            read(MiscFields);
            def(HeapLocation(ArrayLengthLoc, MiscFields, node->child1()), LazyNode(node));
            return;
        }
    }
        
    case GetClosureVar:
        read(AbstractHeap(ScopeProperties, node->scopeOffset().offset()));
        def(HeapLocation(ClosureVariableLoc, AbstractHeap(ScopeProperties, node->scopeOffset().offset()), node->child1()), LazyNode(node));
        return;
        
    case PutClosureVar:
        write(AbstractHeap(ScopeProperties, node->scopeOffset().offset()));
        def(HeapLocation(ClosureVariableLoc, AbstractHeap(ScopeProperties, node->scopeOffset().offset()), node->child1()), LazyNode(node->child2().node()));
        return;

    case GetRegExpObjectLastIndex:
        read(RegExpObject_lastIndex);
        def(HeapLocation(RegExpObjectLastIndexLoc, RegExpObject_lastIndex, node->child1()), LazyNode(node));
        return;

    case SetRegExpObjectLastIndex:
        write(RegExpObject_lastIndex);
        def(HeapLocation(RegExpObjectLastIndexLoc, RegExpObject_lastIndex, node->child1()), LazyNode(node->child2().node()));
        return;

    case RecordRegExpCachedResult:
        write(RegExpState);
        return;
        
    case GetFromArguments: {
        AbstractHeap heap(DirectArgumentsProperties, node->capturedArgumentsOffset().offset());
        read(heap);
        def(HeapLocation(DirectArgumentsLoc, heap, node->child1()), LazyNode(node));
        return;
    }
        
    case PutToArguments: {
        AbstractHeap heap(DirectArgumentsProperties, node->capturedArgumentsOffset().offset());
        write(heap);
        def(HeapLocation(DirectArgumentsLoc, heap, node->child1()), LazyNode(node->child2().node()));
        return;
    }
        
    case GetGlobalVar:
    case GetGlobalLexicalVariable:
        read(AbstractHeap(Absolute, node->variablePointer()));
        def(HeapLocation(GlobalVariableLoc, AbstractHeap(Absolute, node->variablePointer())), LazyNode(node));
        return;
        
    case PutGlobalVariable:
        write(AbstractHeap(Absolute, node->variablePointer()));
        def(HeapLocation(GlobalVariableLoc, AbstractHeap(Absolute, node->variablePointer())), LazyNode(node->child2().node()));
        return;

    case NewArrayWithSize:
    case NewTypedArray:
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

    case NewArray: {
        read(HeapObjectCount);
        write(HeapObjectCount);

        unsigned numElements = node->numChildren();

        def(HeapLocation(ArrayLengthLoc, Butterfly_publicLength, node),
            LazyNode(graph.freeze(jsNumber(numElements))));

        if (!numElements)
            return;

        AbstractHeap heap;
        switch (node->indexingType()) {
        case ALL_DOUBLE_INDEXING_TYPES:
            heap = IndexedDoubleProperties;
            break;

        case ALL_INT32_INDEXING_TYPES:
            heap = IndexedInt32Properties;
            break;

        case ALL_CONTIGUOUS_INDEXING_TYPES:
            heap = IndexedContiguousProperties;
            break;

        default:
            return;
        }

        if (numElements < graph.m_uint32ValuesInUse.size()) {
            for (unsigned operandIdx = 0; operandIdx < numElements; ++operandIdx) {
                Edge use = graph.m_varArgChildren[node->firstChild() + operandIdx];
                def(HeapLocation(IndexedPropertyLoc, heap, node, LazyNode(graph.freeze(jsNumber(operandIdx)))),
                    LazyNode(use.node()));
            }
        } else {
            for (uint32_t operandIdx : graph.m_uint32ValuesInUse) {
                if (operandIdx >= numElements)
                    continue;
                Edge use = graph.m_varArgChildren[node->firstChild() + operandIdx];
                // operandIdx comes from graph.m_uint32ValuesInUse and thus is guaranteed to be already frozen
                def(HeapLocation(IndexedPropertyLoc, heap, node, LazyNode(graph.freeze(jsNumber(operandIdx)))),
                    LazyNode(use.node()));
            }
        }
        return;
    }

    case NewArrayBuffer: {
        read(HeapObjectCount);
        write(HeapObjectCount);

        unsigned numElements = node->numConstants();
        def(HeapLocation(ArrayLengthLoc, Butterfly_publicLength, node),
            LazyNode(graph.freeze(jsNumber(numElements))));

        AbstractHeap heap;
        NodeType op = JSConstant;
        switch (node->indexingType()) {
        case ALL_DOUBLE_INDEXING_TYPES:
            heap = IndexedDoubleProperties;
            op = DoubleConstant;
            break;

        case ALL_INT32_INDEXING_TYPES:
            heap = IndexedInt32Properties;
            break;

        case ALL_CONTIGUOUS_INDEXING_TYPES:
            heap = IndexedContiguousProperties;
            break;

        default:
            return;
        }

        JSValue* data = graph.m_codeBlock->constantBuffer(node->startConstant());
        if (numElements < graph.m_uint32ValuesInUse.size()) {
            for (unsigned index = 0; index < numElements; ++index) {
                def(HeapLocation(IndexedPropertyLoc, heap, node, LazyNode(graph.freeze(jsNumber(index)))),
                    LazyNode(graph.freeze(data[index]), op));
            }
        } else {
            Vector<uint32_t> possibleIndices;
            for (uint32_t index : graph.m_uint32ValuesInUse) {
                if (index >= numElements)
                    continue;
                possibleIndices.append(index);
            }
            for (uint32_t index : possibleIndices) {
                def(HeapLocation(IndexedPropertyLoc, heap, node, LazyNode(graph.freeze(jsNumber(index)))),
                    LazyNode(graph.freeze(data[index]), op));
            }
        }
        return;
    }

    case CopyRest: {
        read(Stack);
        write(Heap);
        return;
    }

    case NewObject:
    case NewRegexp:
    case NewStringObject:
    case PhantomNewObject:
    case MaterializeNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomCreateActivation:
    case MaterializeCreateActivation:
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;
    
    case NewArrowFunction:
    case NewFunction:
    case NewGeneratorFunction:
        if (node->castOperand<FunctionExecutable*>()->singletonFunction()->isStillValid())
            write(Watchpoint_fire);
        read(HeapObjectCount);
        write(HeapObjectCount);
        return;

    case RegExpExec:
    case RegExpTest:
        if (node->child2().useKind() == RegExpObjectUse
            && node->child3().useKind() == StringUse) {
            read(RegExpState);
            read(RegExpObject_lastIndex);
            write(RegExpState);
            write(RegExpObject_lastIndex);
            return;
        }
        read(World);
        write(Heap);
        return;

    case StringReplace:
        if (node->child1().useKind() == StringUse
            && node->child2().useKind() == RegExpObjectUse
            && node->child3().useKind() == StringUse) {
            read(RegExpState);
            read(RegExpObject_lastIndex);
            write(RegExpState);
            write(RegExpObject_lastIndex);
            return;
        }
        read(World);
        write(Heap);
        return;

    case StringCharAt:
        if (node->arrayMode().isOutOfBounds()) {
            read(World);
            write(Heap);
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
        write(Heap);
        return;
        
    case ToString:
    case CallStringConstructor:
        switch (node->child1().useKind()) {
        case StringObjectUse:
        case StringOrStringObjectUse:
            // These don't def a pure value, unfortunately. I'll avoid load-eliminating these for
            // now.
            return;
            
        case CellUse:
        case UntypedUse:
            read(World);
            write(Heap);
            return;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
        
    case ThrowReferenceError:
        write(SideState);
        return;
        
    case CountExecution:
    case CheckWatchdogTimer:
        read(InternalState);
        write(InternalState);
        return;
        
    case LogShadowChickenPrologue:
    case LogShadowChickenTail:
        write(SideState);
        return;
        
    case LastNodeType:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
    
    DFG_CRASH(graph, node, toCString("Unrecognized node type: ", Graph::opName(node->op())).data());
}

class NoOpClobberize {
public:
    NoOpClobberize() { }
    template<typename... T>
    void operator()(T...) const { }
};

class CheckClobberize {
public:
    CheckClobberize()
        : m_result(false)
    {
    }
    
    template<typename... T>
    void operator()(T...) const { m_result = true; }
    
    bool result() const { return m_result; }
    
private:
    mutable bool m_result;
};

bool doesWrites(Graph&, Node*);

class AbstractHeapOverlaps {
public:
    AbstractHeapOverlaps(AbstractHeap heap)
        : m_heap(heap)
        , m_result(false)
    {
    }
    
    void operator()(AbstractHeap otherHeap) const
    {
        if (m_result)
            return;
        m_result = m_heap.overlaps(otherHeap);
    }
    
    bool result() const { return m_result; }

private:
    AbstractHeap m_heap;
    mutable bool m_result;
};

bool accessesOverlap(Graph&, Node*, AbstractHeap);
bool writesOverlap(Graph&, Node*, AbstractHeap);

bool clobbersHeap(Graph&, Node*);

// We would have used bind() for these, but because of the overlaoding that we are doing,
// it's quite a bit of clearer to just write this out the traditional way.

template<typename T>
class ReadMethodClobberize {
public:
    ReadMethodClobberize(T& value)
        : m_value(value)
    {
    }
    
    void operator()(AbstractHeap heap) const
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
    
    void operator()(AbstractHeap heap) const
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
    
    void operator()(PureValue value) const
    {
        m_value.def(value);
    }
    
    void operator()(HeapLocation location, LazyNode node) const
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

