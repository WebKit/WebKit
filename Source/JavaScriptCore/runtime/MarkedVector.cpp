/*
 *  Copyright (C) 2003-2023, 2026 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "MarkedVector.h"

#include "APICast.h"
#include "JSCJSValueInlines.h"

namespace JSC {

EncodedJSValue MarkedVectorBase::m_storageForOutOfBoundsAccess;

void MarkedVectorBase::addMarkSet(JSValue v)
{
    if (m_markSet)
        return;

    Heap* heap = Heap::heap(v);
    if (!heap)
        return;

    m_markSet = &heap->markListSet();
    m_markSet->add(this);
}

#if CPU(ADDRESS32)
JSCell* MarkedVectorBase::toCell(const void* pointer, MarkedVectorBase::StorageType type)
{
    switch (type) {
    case StorageType::JSValue:
        RELEASE_ASSERT_NOT_REACHED();
    case StorageType::JSCell:
        return std::bit_cast<JSCell*>(pointer);
    case StorageType::JSContextRef:
        return toJS(std::bit_cast<JSContextRef>(pointer));
    case StorageType::JSGlobalContextRef:
        return toJS(std::bit_cast<JSGlobalContextRef>(pointer));
    case StorageType::JSValueRef: {
        JSValue value = toJS(std::bit_cast<JSValueRef>(pointer));
        if (value.isCell())
            return value.asCell();
        return nullptr;
    }
    case StorageType::JSObjectRef:
        return toJS(std::bit_cast<JSObjectRef>(pointer));
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

JSCell* MarkedVectorBase::toCellForGC(const void* pointer, MarkedVectorBase::StorageType type)
{
    switch (type) {
    case StorageType::JSValue:
        RELEASE_ASSERT_NOT_REACHED();
    case StorageType::JSCell:
        return std::bit_cast<JSCell*>(pointer);
    case StorageType::JSContextRef:
        return toJS(std::bit_cast<JSContextRef>(pointer));
    case StorageType::JSGlobalContextRef:
        return toJS(std::bit_cast<JSGlobalContextRef>(pointer));
    case StorageType::JSValueRef: {
        JSValue value = toJSForGC(std::bit_cast<JSValueRef>(pointer));
        if (value.isCell())
            return value.asCell();
        return nullptr;
    }
    case StorageType::JSObjectRef:
        return toJS(std::bit_cast<JSObjectRef>(pointer));
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void MarkedVectorBase::addMarkSet(const void* pointer)
{
    if (m_markSet)
        return;

    Heap* heap = Heap::heap(toCell(pointer, m_storageType));
    if (!heap)
        return;

    m_markSet = &heap->markListSet();
    m_markSet->add(this);
}
#endif

template<typename Visitor>
void MarkedVectorBase::markLists(Visitor& visitor, ListSet& markSet)
{
    ListSet::iterator end = markSet.end();
    for (ListSet::iterator it = markSet.begin(); it != end; ++it) {
        MarkedVectorBase* list = *it;
#if CPU(ADDRESS32)
        if (list->m_storageType != StorageType::JSValue) {
            for (unsigned i = 0; i < list->m_size; ++i)
                visitor.appendUnbarriered(JSValue(toCellForGC(list->pointerSlotFor(i), list->m_storageType)));
            continue;
        }
#endif
        for (unsigned i = 0; i < list->m_size; ++i)
            visitor.appendUnbarriered(JSValue::decode(list->jsValueSlotFor(i)));
    }
}

template void MarkedVectorBase::markLists(AbstractSlotVisitor&, ListSet&);
template void MarkedVectorBase::markLists(SlotVisitor&, ListSet&);

auto MarkedVectorBase::slowEnsureCapacity(size_t requestedCapacity) -> Status
{
    setNeedsOverflowCheck();
    auto checkedNewCapacity = CheckedInt32(requestedCapacity);
    if (checkedNewCapacity.hasOverflowed()) [[unlikely]]
        return Status::Overflowed;
    return expandCapacity(checkedNewCapacity);
}

void MarkedVectorBase::slowEnsureCapacityAndCrashOnOverflow(size_t requestedCapacity)
{
    auto status = slowEnsureCapacity(requestedCapacity);
    RELEASE_ASSERT(status != Status::Overflowed);
}

auto MarkedVectorBase::expandCapacity() -> Status
{
    setNeedsOverflowCheck();
    auto checkedNewCapacity = CheckedInt32(m_capacity) * 2;
    if (checkedNewCapacity.hasOverflowed()) [[unlikely]]
        return Status::Overflowed;
    return expandCapacity(checkedNewCapacity);
}

auto MarkedVectorBase::expandCapacity(unsigned newCapacity) -> Status
{
    setNeedsOverflowCheck();
    ASSERT(m_capacity < newCapacity);
    unsigned elementSize = sizeof(EncodedJSValue);
#if CPU(ADDRESS32)
    if (m_storageType != StorageType::JSValue)
        elementSize = sizeof(uintptr_t);
#endif
    auto checkedSize = CheckedSize(newCapacity) * elementSize;
    if (checkedSize.hasOverflowed()) [[unlikely]]
        return Status::Overflowed;
    void* newBuffer = FastMalloc::tryMalloc(checkedSize);
    if (!newBuffer)
        return Status::Overflowed;
#if CPU(ADDRESS32)
    if (m_storageType != StorageType::JSValue) {
        auto* newPointerBuffer = std::bit_cast<void**>(newBuffer);
        const auto* oldPointerBuffer = std::bit_cast<void**>(m_buffer);
        for (unsigned i = 0; i < m_size; ++i) {
            newPointerBuffer[i] = oldPointerBuffer[i];
            addMarkSet(oldPointerBuffer[i]);
        }
    } else
#endif
    {
        auto* newJSValueBuffer = std::bit_cast<EncodedJSValue*>(newBuffer);
        const auto* oldJSValueBuffer = std::bit_cast<EncodedJSValue*>(m_buffer);
        for (unsigned i = 0; i < m_size; ++i) {
            newJSValueBuffer[i] = oldJSValueBuffer[i];
            addMarkSet(JSValue::decode(oldJSValueBuffer[i]));
        }
    }

    if (EncodedJSValue* base = mallocBase())
        FastMalloc::free(base);

    m_buffer = newBuffer;
    m_capacity = newCapacity;
    return Status::Success;
}

auto MarkedVectorBase::slowAppend(JSValue v) -> Status
{
    ASSERT(m_size <= m_capacity);
    if (m_size == m_capacity) {
        auto status = expandCapacity();
        if (status == Status::Overflowed) {
            ASSERT(m_needsOverflowCheck);
            return status;
        }
    }
    jsValueSlotFor(m_size) = JSValue::encode(v);
    ++m_size;
    addMarkSet(v);
    return Status::Success;
}

#if CPU(ADDRESS32)
auto MarkedVectorBase::slowAppend(const void* pointer) -> Status
{
    ASSERT(m_size <= m_capacity);
    if (m_size == m_capacity) {
        auto status = expandCapacity();
        if (status == Status::Overflowed) {
            ASSERT(m_needsOverflowCheck);
            return status;
        }
    }
    pointerSlotFor(m_size) = const_cast<void*>(pointer);
    ++m_size;
    addMarkSet(const_cast<void*>(pointer));
    return Status::Success;
}
#endif

} // namespace JSC
