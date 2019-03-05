/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "AllocatorForMode.h"
#include "AllocatorInlines.h"
#include "CompleteSubspaceInlines.h"
#include "CPU.h"
#include "CallFrame.h"
#include "DeferGC.h"
#include "FreeListInlines.h"
#include "Handle.h"
#include "IsoSubspaceInlines.h"
#include "JSBigInt.h"
#include "JSCast.h"
#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "LocalAllocatorInlines.h"
#include "MarkedBlock.h"
#include "Structure.h"
#include "Symbol.h"
#include <wtf/CompilationThread.h>

namespace JSC {

inline JSCell::JSCell(CreatingEarlyCellTag)
    : m_cellState(CellState::DefinitelyWhite)
{
    ASSERT(!isCompilationThread());
}

inline JSCell::JSCell(VM&, Structure* structure)
    : m_structureID(structure->id())
    , m_indexingTypeAndMisc(structure->indexingModeIncludingHistory())
    , m_type(structure->typeInfo().type())
    , m_flags(structure->typeInfo().inlineTypeFlags())
    , m_cellState(CellState::DefinitelyWhite)
{
    ASSERT(!isCompilationThread());
}

inline void JSCell::finishCreation(VM& vm)
{
    // This object is ready to be escaped so the concurrent GC may see it at any time. We have
    // to make sure that none of our stores sink below here.
    vm.heap.mutatorFence();
#if ENABLE(GC_VALIDATION)
    ASSERT(vm.isInitializingObject());
    vm.setInitializingObjectClass(0);
#else
    UNUSED_PARAM(vm);
#endif
    ASSERT(m_structureID);
}

inline void JSCell::finishCreation(VM& vm, Structure* structure, CreatingEarlyCellTag)
{
#if ENABLE(GC_VALIDATION)
    ASSERT(vm.isInitializingObject());
    vm.setInitializingObjectClass(0);
    if (structure) {
#endif
        m_structureID = structure->id();
        m_indexingTypeAndMisc = structure->indexingModeIncludingHistory();
        m_type = structure->typeInfo().type();
        m_flags = structure->typeInfo().inlineTypeFlags();
#if ENABLE(GC_VALIDATION)
    }
#else
    UNUSED_PARAM(vm);
#endif
    // Very first set of allocations won't have a real structure.
    ASSERT(m_structureID || !vm.structureStructure);
}

inline JSType JSCell::type() const
{
    return m_type;
}

inline IndexingType JSCell::indexingTypeAndMisc() const
{
    return m_indexingTypeAndMisc;
}

inline IndexingType JSCell::indexingType() const
{
    return indexingTypeAndMisc() & AllWritableArrayTypes;
}

inline IndexingType JSCell::indexingMode() const
{
    return indexingTypeAndMisc() & AllArrayTypes;
}

ALWAYS_INLINE Structure* JSCell::structure() const
{
    return structure(*vm());
}

ALWAYS_INLINE Structure* JSCell::structure(VM& vm) const
{
    return vm.getStructure(m_structureID);
}

inline void JSCell::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    visitor.appendUnbarriered(cell->structure(visitor.vm()));
}

inline void JSCell::visitOutputConstraints(JSCell*, SlotVisitor&)
{
}

ALWAYS_INLINE VM& ExecState::vm() const
{
    JSCell* callee = this->callee().asCell();
    ASSERT(callee);
    ASSERT(callee->vm());
    ASSERT(!callee->isLargeAllocation());
    // This is an important optimization since we access this so often.
    return *callee->markedBlock().vm();
}

template<typename CellType, SubspaceAccess>
CompleteSubspace* JSCell::subspaceFor(VM& vm)
{
    if (CellType::needsDestruction)
        return &vm.destructibleCellSpace;
    return &vm.cellSpace;
}

template<typename Type>
inline Allocator allocatorForNonVirtualConcurrently(VM& vm, size_t allocationSize, AllocatorForMode mode)
{
    if (auto* subspace = subspaceForConcurrently<Type>(vm))
        return subspace->allocatorForNonVirtual(allocationSize, mode);
    return { };
}

template<typename T>
ALWAYS_INLINE void* tryAllocateCellHelper(Heap& heap, size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    VM& vm = *heap.vm();
    if (validateDFGDoesGC)
        RELEASE_ASSERT(heap.expectDoesGC());

    ASSERT(deferralContext || !DisallowGC::isInEffectOnCurrentThread());
    ASSERT(size >= sizeof(T));
    JSCell* result = static_cast<JSCell*>(subspaceFor<T>(vm)->allocateNonVirtual(vm, size, deferralContext, failureMode));
    if (failureMode == AllocationFailureMode::ReturnNull && !result)
        return nullptr;
#if ENABLE(GC_VALIDATION)
    ASSERT(!vm.isInitializingObject());
    vm.setInitializingObjectClass(T::info());
#endif
    result->clearStructure();
    return result;
}

template<typename T>
void* allocateCell(Heap& heap, size_t size)
{
    return tryAllocateCellHelper<T>(heap, size, nullptr, AllocationFailureMode::Assert);
}

template<typename T>
void* tryAllocateCell(Heap& heap, size_t size)
{
    return tryAllocateCellHelper<T>(heap, size, nullptr, AllocationFailureMode::ReturnNull);
}

template<typename T>
void* allocateCell(Heap& heap, GCDeferralContext* deferralContext, size_t size)
{
    return tryAllocateCellHelper<T>(heap, size, deferralContext, AllocationFailureMode::Assert);
}

template<typename T>
void* tryAllocateCell(Heap& heap, GCDeferralContext* deferralContext, size_t size)
{
    return tryAllocateCellHelper<T>(heap, size, deferralContext, AllocationFailureMode::ReturnNull);
}

inline bool JSCell::isObject() const
{
    return TypeInfo::isObject(m_type);
}

inline bool JSCell::isString() const
{
    return m_type == StringType;
}

inline bool JSCell::isBigInt() const
{
    return m_type == BigIntType;
}

inline bool JSCell::isSymbol() const
{
    return m_type == SymbolType;
}

inline bool JSCell::isGetterSetter() const
{
    return m_type == GetterSetterType;
}

inline bool JSCell::isCustomGetterSetter() const
{
    return m_type == CustomGetterSetterType;
}

inline bool JSCell::isProxy() const
{
    return m_type == ImpureProxyType || m_type == PureForwardingProxyType || m_type == ProxyObjectType;
}

ALWAYS_INLINE bool JSCell::isFunction(VM& vm)
{
    if (type() == JSFunctionType)
        return true;
    if (inlineTypeFlags() & OverridesGetCallData) {
        CallData ignoredCallData;
        return methodTable(vm)->getCallData(this, ignoredCallData) != CallType::None;
    }
    return false;
}

inline bool JSCell::isCallable(VM& vm, CallType& callType, CallData& callData)
{
    if (type() != JSFunctionType && !(inlineTypeFlags() & OverridesGetCallData))
        return false;
    callType = methodTable(vm)->getCallData(this, callData);
    return callType != CallType::None;
}

inline bool JSCell::isConstructor(VM& vm)
{
    ConstructType constructType;
    ConstructData constructData;
    return isConstructor(vm, constructType, constructData);
}

inline bool JSCell::isConstructor(VM& vm, ConstructType& constructType, ConstructData& constructData)
{
    constructType = methodTable(vm)->getConstructData(this, constructData);
    return constructType != ConstructType::None;
}

inline bool JSCell::isAPIValueWrapper() const
{
    return m_type == APIValueWrapperType;
}

ALWAYS_INLINE void JSCell::setStructure(VM& vm, Structure* structure)
{
    ASSERT(structure->classInfo() == this->structure(vm)->classInfo());
    ASSERT(!this->structure(vm)
        || this->structure(vm)->transitionWatchpointSetHasBeenInvalidated()
        || Heap::heap(this)->structureIDTable().get(structure->id()) == structure);
    m_structureID = structure->id();
    m_flags = TypeInfo::mergeInlineTypeFlags(structure->typeInfo().inlineTypeFlags(), m_flags);
    m_type = structure->typeInfo().type();
    IndexingType newIndexingType = structure->indexingModeIncludingHistory();
    if (m_indexingTypeAndMisc != newIndexingType) {
        ASSERT(!(newIndexingType & ~AllArrayTypesAndHistory));
        for (;;) {
            IndexingType oldValue = m_indexingTypeAndMisc;
            IndexingType newValue = (oldValue & ~AllArrayTypesAndHistory) | structure->indexingModeIncludingHistory();
            if (WTF::atomicCompareExchangeWeakRelaxed(&m_indexingTypeAndMisc, oldValue, newValue))
                break;
        }
    }
    vm.heap.writeBarrier(this, structure);
}

inline const MethodTable* JSCell::methodTable(VM& vm) const
{
    Structure* structure = this->structure(vm);
#if !ASSERT_DISABLED
    if (Structure* rootStructure = structure->structure(vm))
        ASSERT(rootStructure == rootStructure->structure(vm));
#endif
    return &structure->classInfo()->methodTable;
}

inline bool JSCell::inherits(VM& vm, const ClassInfo* info) const
{
    return classInfo(vm)->isSubClassOf(info);
}

template<typename Target>
inline bool JSCell::inherits(VM& vm) const
{
    return JSCastingHelpers::inherits<Target>(vm, this);
}

ALWAYS_INLINE JSValue JSCell::fastGetOwnProperty(VM& vm, Structure& structure, PropertyName name)
{
    ASSERT(canUseFastGetOwnProperty(structure));
    PropertyOffset offset = structure.get(vm, name);
    if (offset != invalidOffset)
        return asObject(this)->locationForOffset(offset)->get();
    return JSValue();
}

inline bool JSCell::canUseFastGetOwnProperty(const Structure& structure)
{
    return !structure.hasGetterSetterProperties() 
        && !structure.hasCustomGetterSetterProperties()
        && !structure.typeInfo().overridesGetOwnPropertySlot();
}

ALWAYS_INLINE const ClassInfo* JSCell::classInfo(VM& vm) const
{
    // What we really want to assert here is that we're not currently destructing this object (which makes its classInfo
    // invalid). If mutatorState() == MutatorState::Running, then we're not currently sweeping, and therefore cannot be
    // destructing the object. The GC thread or JIT threads, unlike the mutator thread, are able to access classInfo
    // independent of whether the mutator thread is sweeping or not. Hence, we also check for !currentThreadIsHoldingAPILock()
    // to allow the GC thread or JIT threads to pass this assertion.
    ASSERT(vm.heap.mutatorState() != MutatorState::Sweeping || !vm.currentThreadIsHoldingAPILock());
    return structure(vm)->classInfo();
}

inline bool JSCell::toBoolean(ExecState* exec) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toBoolean();
    if (isBigInt())
        return static_cast<const JSBigInt*>(this)->toBoolean();
    return !structure(exec->vm())->masqueradesAsUndefined(exec->lexicalGlobalObject());
}

inline TriState JSCell::pureToBoolean() const
{
    if (isString())
        return static_cast<const JSString*>(this)->toBoolean() ? TrueTriState : FalseTriState;
    if (isBigInt())
        return static_cast<const JSBigInt*>(this)->toBoolean() ? TrueTriState : FalseTriState;
    if (isSymbol())
        return TrueTriState;
    return MixedTriState;
}

inline void JSCellLock::lock()
{
    Atomic<IndexingType>* lock = bitwise_cast<Atomic<IndexingType>*>(&m_indexingTypeAndMisc);
    if (UNLIKELY(!IndexingTypeLockAlgorithm::lockFast(*lock)))
        lockSlow();
}

inline bool JSCellLock::tryLock()
{
    Atomic<IndexingType>* lock = bitwise_cast<Atomic<IndexingType>*>(&m_indexingTypeAndMisc);
    return IndexingTypeLockAlgorithm::tryLock(*lock);
}

inline void JSCellLock::unlock()
{
    Atomic<IndexingType>* lock = bitwise_cast<Atomic<IndexingType>*>(&m_indexingTypeAndMisc);
    if (UNLIKELY(!IndexingTypeLockAlgorithm::unlockFast(*lock)))
        unlockSlow();
}

inline bool JSCellLock::isLocked() const
{
    Atomic<IndexingType>* lock = bitwise_cast<Atomic<IndexingType>*>(&m_indexingTypeAndMisc);
    return IndexingTypeLockAlgorithm::isLocked(*lock);
}

inline bool JSCell::perCellBit() const
{
    return TypeInfo::perCellBit(inlineTypeFlags());
}

inline void JSCell::setPerCellBit(bool value)
{
    if (value == perCellBit())
        return;

    if (value)
        m_flags |= static_cast<TypeInfo::InlineTypeFlags>(TypeInfoPerCellBit);
    else
        m_flags &= ~static_cast<TypeInfo::InlineTypeFlags>(TypeInfoPerCellBit);
}

inline JSObject* JSCell::toObject(ExecState* exec, JSGlobalObject* globalObject) const
{
    if (isObject())
        return jsCast<JSObject*>(const_cast<JSCell*>(this));
    return toObjectSlow(exec, globalObject);
}

ALWAYS_INLINE bool JSCell::putInline(ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    auto putMethod = methodTable(exec->vm())->put;
    if (LIKELY(putMethod == JSObject::put))
        return JSObject::putInlineForJSObject(asObject(this), exec, propertyName, value, slot);
    return putMethod(this, exec, propertyName, value, slot);
}

inline bool isWebAssemblyToJSCallee(const JSCell* cell)
{
    return cell->type() == WebAssemblyToJSCalleeType;
}

} // namespace JSC
