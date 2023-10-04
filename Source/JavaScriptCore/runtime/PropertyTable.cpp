/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "PropertyTable.h"

#include "JSCJSValueInlines.h"

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PropertyTable);

const ClassInfo PropertyTable::s_info = { "PropertyTable"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(PropertyTable) };

PropertyTable* PropertyTable::create(VM& vm, unsigned initialCapacity)
{
    PropertyTable* table = new (NotNull, allocateCell<PropertyTable>(vm)) PropertyTable(vm, initialCapacity);
    table->finishCreation(vm);
    return table;
}

PropertyTable* PropertyTable::clone(VM& vm, const PropertyTable& other)
{
    PropertyTable* table = new (NotNull, allocateCell<PropertyTable>(vm)) PropertyTable(vm, other);
    table->finishCreation(vm);
    return table;
}

PropertyTable* PropertyTable::clone(VM& vm, unsigned initialCapacity, const PropertyTable& other)
{
    PropertyTable* table = new (NotNull, allocateCell<PropertyTable>(vm)) PropertyTable(vm, initialCapacity, other);
    table->finishCreation(vm);
    return table;
}

PropertyTable::PropertyTable(VM& vm, unsigned initialCapacity)
    : JSCell(vm, vm.propertyTableStructure.get())
    , m_indexSize(sizeForCapacity(initialCapacity))
    , m_indexMask(m_indexSize - 1)
    , m_indexVector()
    , m_keyCount(0)
    , m_deletedCount(0)
{
    ASSERT(isPowerOf2(m_indexSize));
    bool isCompact = tableCapacity() < UINT8_MAX;
    m_indexVector = allocateZeroedIndexVector(isCompact, m_indexSize);
    ASSERT(isCompact == this->isCompact());
}

PropertyTable::PropertyTable(VM& vm, const PropertyTable& other)
    : JSCell(vm, vm.propertyTableStructure.get())
    , m_indexSize(other.m_indexSize)
    , m_indexMask(other.m_indexMask)
    , m_indexVector(allocateIndexVector(other.isCompact(), other.m_indexSize))
    , m_keyCount(other.m_keyCount)
    , m_deletedCount(other.m_deletedCount)
{
    ASSERT(isPowerOf2(m_indexSize));
    ASSERT(isCompact() == other.isCompact());
    memcpy(bitwise_cast<void*>(m_indexVector & indexVectorMask), bitwise_cast<void*>(other.m_indexVector & indexVectorMask), dataSize(isCompact()));

    forEachProperty([&](auto& entry) {
        entry.key()->ref();
        return IterationStatus::Continue;
    });

    // Copy the m_deletedOffsets vector.
    Vector<PropertyOffset>* otherDeletedOffsets = other.m_deletedOffsets.get();
    if (otherDeletedOffsets)
        m_deletedOffsets = makeUnique<Vector<PropertyOffset>>(*otherDeletedOffsets);
}

PropertyTable::PropertyTable(VM& vm, unsigned initialCapacity, const PropertyTable& other)
    : JSCell(vm, vm.propertyTableStructure.get())
    , m_indexSize(sizeForCapacity(initialCapacity))
    , m_indexMask(m_indexSize - 1)
    , m_indexVector()
    , m_keyCount(0)
    , m_deletedCount(0)
{
    ASSERT(isPowerOf2(m_indexSize));
    ASSERT(initialCapacity >= other.m_keyCount);
    bool isCompact = other.isCompact() && tableCapacity() < UINT8_MAX;
    m_indexVector = allocateZeroedIndexVector(isCompact, m_indexSize);
    ASSERT(this->isCompact() == isCompact);

    withIndexVector([&](auto* vector) {
        auto* table = tableFromIndexVector(vector);
        other.forEachProperty([&](auto& entry) {
            ASSERT(canInsert(entry));
            reinsert(vector, table, entry);
            entry.key()->ref();
            return IterationStatus::Continue;
        });
    });

    // Copy the m_deletedOffsets vector.
    Vector<PropertyOffset>* otherDeletedOffsets = other.m_deletedOffsets.get();
    if (otherDeletedOffsets)
        m_deletedOffsets = makeUnique<Vector<PropertyOffset>>(*otherDeletedOffsets);
}

void PropertyTable::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    vm.heap.reportExtraMemoryAllocated(this, dataSize(isCompact()));
}

template<typename Visitor>
void PropertyTable::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<PropertyTable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    visitor.reportExtraMemoryVisited(thisObject->dataSize(thisObject->isCompact()));
}

DEFINE_VISIT_CHILDREN(PropertyTable);

void PropertyTable::destroy(JSCell* cell)
{
    static_cast<PropertyTable*>(cell)->PropertyTable::~PropertyTable();
}

PropertyTable::~PropertyTable()
{
    forEachProperty([&](auto& entry) {
        entry.key()->deref();
        return IterationStatus::Continue;
    });
    destroyIndexVector(m_indexVector);
}

void PropertyTable::seal()
{
    forEachPropertyMutable([&](auto& entry) {
        entry.setAttributes(entry.attributes() | static_cast<unsigned>(PropertyAttribute::DontDelete));
        return IterationStatus::Continue;
    });
}

void PropertyTable::freeze()
{
    forEachPropertyMutable([&](auto& entry) {
        if (!(entry.attributes() & PropertyAttribute::Accessor))
            entry.setAttributes(entry.attributes() | static_cast<unsigned>(PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly));
        else
            entry.setAttributes(entry.attributes() | static_cast<unsigned>(PropertyAttribute::DontDelete));
        return IterationStatus::Continue;
    });
}

bool PropertyTable::isSealed() const
{
    bool result = true;
    forEachProperty([&](const auto& entry) {
        if ((entry.attributes() & PropertyAttribute::DontDelete) != static_cast<unsigned>(PropertyAttribute::DontDelete)) {
            result = false;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return result;
}

bool PropertyTable::isFrozen() const
{
    bool result = true;
    forEachProperty([&](const auto& entry) {
        if (!(entry.attributes() & PropertyAttribute::DontDelete)) {
            result = false;
            return IterationStatus::Done;
        }
        if (!(entry.attributes() & (PropertyAttribute::ReadOnly | PropertyAttribute::Accessor))) {
            result = false;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return result;
}

PropertyOffset PropertyTable::renumberPropertyOffsets(JSObject* object, unsigned inlineCapacity, Vector<JSValue>& values)
{
    ASSERT(values.size() == size());
    unsigned i = 0;
    PropertyOffset offset = invalidOffset;
    forEachPropertyMutable([&](auto& entry) {
        values[i] = object->getDirect(entry.offset());
        offset = offsetForPropertyNumber(i, inlineCapacity);
        entry.setOffset(offset);
        ++i;
        return IterationStatus::Continue;
    });
    clearDeletedOffsets();
    return offset;
}

template<typename Functor>
inline void PropertyTable::forEachPropertyMutable(const Functor& functor)
{
    withIndexVector([&](auto* vector) {
        auto* cursor = tableFromIndexVector(vector);
        auto* end = tableEndFromIndexVector(vector);
        for (; cursor != end; ++cursor) {
            if (cursor->key() == PROPERTY_MAP_DELETED_ENTRY_KEY)
                continue;
            if (functor(*cursor) == IterationStatus::Done)
                return;
        }
    });
}

} // namespace JSC

