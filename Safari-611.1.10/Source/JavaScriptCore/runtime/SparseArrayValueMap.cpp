/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "SparseArrayValueMap.h"

#include "GetterSetter.h"
#include "JSCJSValueInlines.h"
#include "JSObjectInlines.h"
#include "PropertySlot.h"
#include "StructureInlines.h"
#include "TypeError.h"

namespace JSC {

const ClassInfo SparseArrayValueMap::s_info = { "SparseArrayValueMap", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(SparseArrayValueMap) };

SparseArrayValueMap::SparseArrayValueMap(VM& vm)
    : Base(vm, vm.sparseArrayValueMapStructure.get())
{
}

void SparseArrayValueMap::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
}

SparseArrayValueMap* SparseArrayValueMap::create(VM& vm)
{
    SparseArrayValueMap* result = new (NotNull, allocateCell<SparseArrayValueMap>(vm.heap)) SparseArrayValueMap(vm);
    result->finishCreation(vm);
    return result;
}

void SparseArrayValueMap::destroy(JSCell* cell)
{
    static_cast<SparseArrayValueMap*>(cell)->SparseArrayValueMap::~SparseArrayValueMap();
}

Structure* SparseArrayValueMap::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

SparseArrayValueMap::AddResult SparseArrayValueMap::add(JSObject* array, unsigned i)
{
    AddResult result;
    size_t increasedCapacity = 0;
    {
        auto locker = holdLock(cellLock());
        result = m_map.add(i, SparseArrayEntry());
        size_t capacity = m_map.capacity();
        if (capacity > m_reportedCapacity) {
            increasedCapacity = capacity - m_reportedCapacity;
            m_reportedCapacity = capacity;
        }
    }
    if (increasedCapacity)
        Heap::heap(array)->reportExtraMemoryAllocated(increasedCapacity * sizeof(Map::KeyValuePairType));
    return result;
}

void SparseArrayValueMap::remove(iterator it)
{
    auto locker = holdLock(cellLock());
    m_map.remove(it);
}

void SparseArrayValueMap::remove(unsigned i)
{
    auto locker = holdLock(cellLock());
    m_map.remove(i);
}

bool SparseArrayValueMap::putEntry(JSGlobalObject* globalObject, JSObject* array, unsigned i, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(value);
    
    AddResult result = add(array, i);
    SparseArrayEntry& entry = result.iterator->value;

    // To save a separate find & add, we first always add to the sparse map.
    // In the uncommon case that this is a new property, and the array is not
    // extensible, this is not the right thing to have done - so remove again.
    if (result.isNewEntry && !array->isStructureExtensible(vm)) {
        remove(result.iterator);
        return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);
    }
    
    RELEASE_AND_RETURN(scope, entry.put(globalObject, array, this, value, shouldThrow));
}

bool SparseArrayValueMap::putDirect(JSGlobalObject* globalObject, JSObject* array, unsigned i, JSValue value, unsigned attributes, PutDirectIndexMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(value);
    
    bool shouldThrow = (mode == PutDirectIndexShouldThrow);

    AddResult result = add(array, i);
    SparseArrayEntry& entry = result.iterator->value;

    // To save a separate find & add, we first always add to the sparse map.
    // In the uncommon case that this is a new property, and the array is not
    // extensible, this is not the right thing to have done - so remove again.
    if (mode != PutDirectIndexLikePutDirect && result.isNewEntry && !array->isStructureExtensible(vm)) {
        remove(result.iterator);
        return typeError(globalObject, scope, shouldThrow, NonExtensibleObjectPropertyDefineError);
    }

    if (entry.attributes() & PropertyAttribute::ReadOnly)
        return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

    entry.forceSet(vm, this, value, attributes);
    return true;
}

JSValue SparseArrayValueMap::getConcurrently(unsigned i)
{
    auto locker = holdLock(cellLock());
    auto iterator = m_map.find(i);
    if (iterator == m_map.end())
        return JSValue();
    return iterator->value.getConcurrently();
}

void SparseArrayEntry::get(JSObject* thisObject, PropertySlot& slot) const
{
    JSValue value = Base::get();
    ASSERT(value);

    if (LIKELY(!value.isGetterSetter())) {
        slot.setValue(thisObject, m_attributes, value);
        return;
    }

    slot.setGetterSlot(thisObject, m_attributes, jsCast<GetterSetter*>(value));
}

void SparseArrayEntry::get(PropertyDescriptor& descriptor) const
{
    descriptor.setDescriptor(Base::get(), m_attributes);
}

JSValue SparseArrayEntry::getConcurrently() const
{
    // These attributes and value can be updated while executing getConcurrently.
    // But this is OK since attributes should be never weaken once it gets DontDelete and ReadOnly.
    // By emitting store-store-fence and load-load-fence between value setting and attributes setting,
    // we can ensure that the value is what we want once the attributes get ReadOnly & DontDelete:
    // once attributes get this state, the value should not be changed.
    unsigned attributes = m_attributes;
    Dependency attributesDependency = Dependency::fence(attributes);
    if (attributes & PropertyAttribute::Accessor)
        return JSValue();

    if (!(attributes & PropertyAttribute::ReadOnly))
        return JSValue();

    if (!(attributes & PropertyAttribute::DontDelete))
        return JSValue();

    return attributesDependency.consume(this)->Base::get();
}

bool SparseArrayEntry::put(JSGlobalObject* globalObject, JSValue thisValue, SparseArrayValueMap* map, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!(m_attributes & PropertyAttribute::Accessor)) {
        if (m_attributes & PropertyAttribute::ReadOnly)
            return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

        set(vm, map, value);
        return true;
    }

    RELEASE_AND_RETURN(scope, callSetter(globalObject, thisValue, Base::get(), value, shouldThrow ? ECMAMode::strict() : ECMAMode::sloppy()));
}

JSValue SparseArrayEntry::getNonSparseMode() const
{
    ASSERT(!m_attributes);
    return Base::get();
}

void SparseArrayValueMap::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    SparseArrayValueMap* thisObject = jsCast<SparseArrayValueMap*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    {
        auto locker = holdLock(thisObject->cellLock());
        for (auto& entry : thisObject->m_map)
            visitor.append(entry.value.asValue());
    }
    visitor.reportExtraMemoryVisited(thisObject->m_reportedCapacity * sizeof(Map::KeyValuePairType));
}

} // namespace JSC

