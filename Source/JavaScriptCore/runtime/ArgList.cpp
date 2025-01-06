/*
 *  Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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
#include "ArgList.h"

#include "JSCJSValueInlines.h"
#include <wtf/TZoneMallocInlines.h>

using std::min;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ArgList);

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

void ArgList::getSlice(int startIndex, ArgList& result) const
{
    if (startIndex <= 0 || static_cast<unsigned>(startIndex) >= m_argCount) {
        result = ArgList();
        return;
    }

    result.m_args = m_args + startIndex;
    result.m_argCount =  m_argCount - startIndex;
}

template<typename Visitor>
void MarkedVectorBase::markLists(Visitor& visitor, ListSet& markSet)
{
    ListSet::iterator end = markSet.end();
    for (ListSet::iterator it = markSet.begin(); it != end; ++it) {
        MarkedVectorBase* list = *it;
        for (unsigned i = 0; i < list->m_size; ++i)
            visitor.appendUnbarriered(JSValue::decode(list->slotFor(i)));
    }
}

template void MarkedVectorBase::markLists(AbstractSlotVisitor&, ListSet&);
template void MarkedVectorBase::markLists(SlotVisitor&, ListSet&);

auto MarkedVectorBase::slowEnsureCapacity(size_t requestedCapacity) -> Status
{
    setNeedsOverflowCheck();
    auto checkedNewCapacity = CheckedInt32(requestedCapacity);
    if (UNLIKELY(checkedNewCapacity.hasOverflowed()))
        return Status::Overflowed;
    return expandCapacity(checkedNewCapacity);
}

auto MarkedVectorBase::expandCapacity() -> Status
{
    setNeedsOverflowCheck();
    auto checkedNewCapacity = CheckedInt32(m_capacity) * 2;
    if (UNLIKELY(checkedNewCapacity.hasOverflowed()))
        return Status::Overflowed;
    return expandCapacity(checkedNewCapacity);
}

auto MarkedVectorBase::expandCapacity(unsigned newCapacity) -> Status
{
    setNeedsOverflowCheck();
    ASSERT(m_capacity < newCapacity);
    auto checkedSize = CheckedSize(newCapacity) * sizeof(EncodedJSValue);
    if (UNLIKELY(checkedSize.hasOverflowed()))
        return Status::Overflowed;
    EncodedJSValue* newBuffer = static_cast<EncodedJSValue*>(FastMalloc::tryMalloc(checkedSize));
    if (!newBuffer)
        return Status::Overflowed;
    for (unsigned i = 0; i < m_size; ++i) {
        newBuffer[i] = m_buffer[i];
        addMarkSet(JSValue::decode(m_buffer[i]));
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
    slotFor(m_size) = JSValue::encode(v);
    ++m_size;
    addMarkSet(v);
    return Status::Success;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
