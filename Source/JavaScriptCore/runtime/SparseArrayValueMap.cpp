/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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
#include "StructureCreateInlines.h"
#include "TypeError.h"

namespace JSC {

const ClassInfo SparseArrayValueMap::s_info = { "SparseArrayValueMap"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(SparseArrayValueMap) };

SparseArrayValueMap::SparseArrayValueMap(VM& vm)
    : Base(vm, vm.sparseArrayValueMapStructure.get())
{
}

SparseArrayValueMap* SparseArrayValueMap::create(VM& vm)
{
    SparseArrayValueMap* result = new (NotNull, allocateCell<SparseArrayValueMap>(vm)) SparseArrayValueMap(vm);
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
    AddResult addResult;
    size_t increasedCapacity = 0;
    {
        Locker locker { cellLock() };
        addResult = m_set.ensure<SparseArrayEntryTranslator>(i, [&] {
            return SparseArrayEntry(i);
        });
        size_t capacity = m_set.capacity();
        if (capacity > m_reportedCapacity) {
            increasedCapacity = capacity - m_reportedCapacity;
            m_reportedCapacity = capacity;
        }
    }
    if (increasedCapacity)
        Heap::heap(array)->reportExtraMemoryAllocated(array, increasedCapacity * sizeof(SparseArrayEntry));
    return addResult;
}

void SparseArrayValueMap::remove(iterator it)
{
    Locker locker { cellLock() };
    m_set.remove(it);
}

void SparseArrayValueMap::remove(unsigned i)
{
    Locker locker { cellLock() };
    auto it = m_set.find<SparseArrayEntryTranslator>(i);
    if (it != m_set.end())
        m_set.remove(it);
}

bool SparseArrayValueMap::putEntry(JSGlobalObject* globalObject, JSObject* array, unsigned i, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(value);

    AddResult result = add(array, i);
    SparseArrayEntry& entry = *result.iterator;

    // To save a separate find & add, we first always add to the sparse map.
    // In the uncommon case that this is a new property, and the array is not
    // extensible, this is not the right thing to have done - so remove again.
    if (result.isNewEntry && !array->isStructureExtensible()) {
        remove(static_cast<const_iterator>(result.iterator));
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
    SparseArrayEntry& entry = *result.iterator;

    // To save a separate find & add, we first always add to the sparse map.
    // In the uncommon case that this is a new property, and the array is not
    // extensible, this is not the right thing to have done - so remove again.
    if (mode != PutDirectIndexLikePutDirect && result.isNewEntry && !array->isStructureExtensible()) {
        remove(static_cast<const_iterator>(result.iterator));
        return typeError(globalObject, scope, shouldThrow, NonExtensibleObjectPropertyDefineError);
    }

    if (entry.attributes() & PropertyAttribute::ReadOnly)
        return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

    entry.forceSet(vm, this, value, attributes);
    return true;
}

JSValue SparseArrayValueMap::getConcurrently(unsigned i)
{
    Locker locker { cellLock() };
    auto iterator = m_set.find<SparseArrayEntryTranslator>(i);
    if (iterator == m_set.end())
        return JSValue();
    return iterator->getConcurrently();
}

void SparseArrayEntry::forceSet(SparseArrayValueMap* map, unsigned attributes)
{
    // FIXME: We can expand this for non x86 environments. Currently, loading ReadOnly | DontDelete property
    // from compiler thread is only supported in X86 architecture because of its TSO nature.
    // https://bugs.webkit.org/show_bug.cgi?id=134641
    if (isX86())
        WTF::storeStoreFence();

    if (attributes & PropertyAttribute::Accessor)
        map->setHasAnyKindOfGetterSetterProperties();
    m_attributes = attributes;
}

void SparseArrayEntry::forceSet(VM& vm, SparseArrayValueMap* map, JSValue value, unsigned attributes)
{
    m_value.set(vm, map, value);
    forceSet(map, attributes);
}

void SparseArrayEntry::get(JSObject* thisObject, PropertySlot& slot) const
{
    JSValue value = m_value.get();
    ASSERT(value);

    if (!value.isGetterSetter()) [[likely]] {
        slot.setValue(thisObject, m_attributes, value);
        return;
    }

    slot.setGetterSlot(thisObject, m_attributes, uncheckedDowncast<GetterSetter>(value));
}

void SparseArrayEntry::get(PropertyDescriptor& descriptor) const
{
    descriptor.setDescriptor(m_value.get(), m_attributes);
}

JSValue SparseArrayEntry::getConcurrently() const
{
    // These attributes and value can be updated while executing getConcurrently.
    // But this is OK since attributes should be never weaken once it gets DontDelete and ReadOnly.
    // By emitting store-store-fence and load-load-fence between value setting and attributes setting,
    // we can ensure that the value is what we want once the attributes get ReadOnly & DontDelete:
    // once attributes get this state, the value should not be changed.
    unsigned attributes;
    Dependency attributesDependency = Dependency::loadAndFence(&m_attributes, attributes);
    if (attributes & PropertyAttribute::Accessor)
        return JSValue();

    if (!(attributes & PropertyAttribute::ReadOnly))
        return JSValue();

    if (!(attributes & PropertyAttribute::DontDelete))
        return JSValue();

    return attributesDependency.consume(this)->m_value.get();
}

bool SparseArrayEntry::put(JSGlobalObject* globalObject, JSValue thisValue, SparseArrayValueMap* map, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!(m_attributes & PropertyAttribute::Accessor)) {
        if (m_attributes & PropertyAttribute::ReadOnly)
            return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

        m_value.set(vm, map, value);
        return true;
    }

    RELEASE_AND_RETURN(scope, uncheckedDowncast<GetterSetter>(m_value.get())->callSetter(globalObject, thisValue, value, shouldThrow));
}

JSValue SparseArrayEntry::getNonSparseMode() const
{
    ASSERT(!m_attributes);
    return m_value.get();
}

JSValue SparseArrayEntry::get() const
{
    return m_value.get();
}

template<typename Visitor>
void SparseArrayValueMap::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    SparseArrayValueMap* thisObject = uncheckedDowncast<SparseArrayValueMap>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    {
        Locker locker { thisObject->cellLock() };
        for (auto& entry : thisObject->m_set)
            visitor.append(entry.asValue());
    }
    visitor.reportExtraMemoryVisited(thisObject->m_reportedCapacity * sizeof(SparseArrayEntry));
}

DEFINE_VISIT_CHILDREN(SparseArrayValueMap);

} // namespace JSC

