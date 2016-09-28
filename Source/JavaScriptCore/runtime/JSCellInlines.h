/*
 * Copyright (C) 2012, 2013, 2015 Apple Inc. All rights reserved.
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

#ifndef JSCellInlines_h
#define JSCellInlines_h

#include "CallFrame.h"
#include "DeferGC.h"
#include "Handle.h"
#include "JSCell.h"
#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "MarkedBlock.h"
#include "Structure.h"
#include "Symbol.h"
#include <wtf/CompilationThread.h>

namespace JSC {

inline JSCell::JSCell(CreatingEarlyCellTag)
    : m_cellState(CellState::NewWhite)
{
    ASSERT(!isCompilationThread());
}

inline JSCell::JSCell(VM&, Structure* structure)
    : m_structureID(structure->id())
    , m_indexingType(structure->indexingType())
    , m_type(structure->typeInfo().type())
    , m_flags(structure->typeInfo().inlineTypeFlags())
    , m_cellState(CellState::NewWhite)
{
    ASSERT(!isCompilationThread());
}

inline void JSCell::finishCreation(VM& vm)
{
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
        m_indexingType = structure->indexingType();
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

inline IndexingType JSCell::indexingType() const
{
    return m_indexingType;
}

inline Structure* JSCell::structure() const
{
    return Heap::heap(this)->structureIDTable().get(m_structureID);
}

inline Structure* JSCell::structure(VM& vm) const
{
    return vm.heap.structureIDTable().get(m_structureID);
}

inline void JSCell::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Structure* structure = cell->structure(visitor.vm());
    visitor.appendUnbarrieredPointer(&structure);
}

ALWAYS_INLINE VM& ExecState::vm() const
{
    ASSERT(callee());
    ASSERT(callee()->vm());
    ASSERT(!callee()->isLargeAllocation());
    // This is an important optimization since we access this so often.
    return *calleeAsValue().asCell()->markedBlock().vm();
}

template<typename T>
void* allocateCell(Heap& heap, size_t size)
{
    ASSERT(!DisallowGC::isGCDisallowedOnCurrentThread());
    ASSERT(size >= sizeof(T));
    JSCell* result = static_cast<JSCell*>(heap.allocateObjectOfType<T>(size));
#if ENABLE(GC_VALIDATION)
    ASSERT(!heap.vm()->isInitializingObject());
    heap.vm()->setInitializingObjectClass(T::info());
#endif
    result->clearStructure();
    return result;
}
    
template<typename T>
void* allocateCell(Heap& heap)
{
    return allocateCell<T>(heap, sizeof(T));
}
    
inline bool JSCell::isObject() const
{
    return TypeInfo::isObject(m_type);
}

inline bool JSCell::isString() const
{
    return m_type == StringType;
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
    return m_type == ImpureProxyType || m_type == PureForwardingProxyType;
}

inline bool JSCell::isAPIValueWrapper() const
{
    return m_type == APIValueWrapperType;
}

inline void JSCell::setStructure(VM& vm, Structure* structure)
{
    ASSERT(structure->classInfo() == this->structure()->classInfo());
    ASSERT(!this->structure()
        || this->structure()->transitionWatchpointSetHasBeenInvalidated()
        || Heap::heap(this)->structureIDTable().get(structure->id()) == structure);
    vm.heap.writeBarrier(this, structure);
    m_structureID = structure->id();
    m_flags = structure->typeInfo().inlineTypeFlags();
    m_type = structure->typeInfo().type();
    m_indexingType = structure->indexingType();
}

inline const MethodTable* JSCell::methodTable() const
{
    VM& vm = *Heap::heap(this)->vm();
    Structure* structure = this->structure(vm);
    if (Structure* rootStructure = structure->structure(vm))
        RELEASE_ASSERT(rootStructure == rootStructure->structure(vm));

    return &structure->classInfo()->methodTable;
}

inline const MethodTable* JSCell::methodTable(VM& vm) const
{
    Structure* structure = this->structure(vm);
    if (Structure* rootStructure = structure->structure(vm))
        RELEASE_ASSERT(rootStructure == rootStructure->structure(vm));

    return &structure->classInfo()->methodTable;
}

inline bool JSCell::inherits(const ClassInfo* info) const
{
    return classInfo()->isSubClassOf(info);
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

ALWAYS_INLINE const ClassInfo* JSCell::classInfo() const
{
    if (isLargeAllocation()) {
        LargeAllocation& allocation = largeAllocation();
        if (allocation.attributes().destruction == NeedsDestruction
            && !(inlineTypeFlags() & StructureIsImmortal))
            return static_cast<const JSDestructibleObject*>(this)->classInfo();
        return structure(*allocation.vm())->classInfo();
    }
    MarkedBlock& block = markedBlock();
    if (block.needsDestruction() && !(inlineTypeFlags() & StructureIsImmortal))
        return static_cast<const JSDestructibleObject*>(this)->classInfo();
    return structure(*block.vm())->classInfo();
}

inline bool JSCell::toBoolean(ExecState* exec) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toBoolean();
    return !structure()->masqueradesAsUndefined(exec->lexicalGlobalObject());
}

inline TriState JSCell::pureToBoolean() const
{
    if (isString())
        return static_cast<const JSString*>(this)->toBoolean() ? TrueTriState : FalseTriState;
    if (isSymbol())
        return TrueTriState;
    return MixedTriState;
}

inline void JSCell::callDestructor(VM& vm)
{
    if (isZapped())
        return;
    ASSERT(structureID());
    if (inlineTypeFlags() & StructureIsImmortal)
        structure(vm)->classInfo()->methodTable.destroy(this);
    else
        jsCast<JSDestructibleObject*>(this)->classInfo()->methodTable.destroy(this);
    zap();
}

} // namespace JSC

#endif // JSCellInlines_h
