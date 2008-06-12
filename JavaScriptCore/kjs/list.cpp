/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "list.h"

using std::min;

namespace KJS {

void List::getSlice(int startIndex, List& result) const
{
    ASSERT(!result.m_isReadOnly);

    const_iterator start = min(begin() + startIndex, end());
    result.m_vector.appendRange(start, end());
    result.m_size = result.m_vector.size();
    result.m_bufferSlot = result.m_vector.dataSlot();
}

void List::markLists(ListSet& markSet)
{
    ListSet::iterator end = markSet.end();
    for (ListSet::iterator it = markSet.begin(); it != end; ++it) {
        List* list = *it;

        iterator end2 = list->end();
        for (iterator it2 = list->begin(); it2 != end2; ++it2) {
            JSValue* v = *it2;
            if (!v->marked())
                v->mark();
        }
    }
}

void List::slowAppend(JSValue* v)
{
    // As long as our size stays within our Vector's inline 
    // capacity, all our values are allocated on the stack, and 
    // therefore don't need explicit marking. Once our size exceeds
    // our Vector's inline capacity, though, our values move to the 
    // heap, where they do need explicit marking.
    if (!m_markSet) {
        // We can only register for explicit marking once we know which heap
        // is the current one, i.e., when a non-immediate value is appended.
        if (!JSImmediate::isImmediate(v)) { // Will be: if (Heap* heap = Heap::heap(v))
            ListSet& markSet = Collector::markListSet();
            markSet.add(this);
            m_markSet = &markSet;
        }
    }

    if (m_vector.size() < m_vector.capacity()) {
        m_vector.uncheckedAppend(v);
        return;
    }

    // 4x growth would be excessive for a normal vector, but it's OK for Lists 
    // because they're short-lived.
    m_vector.reserveCapacity(m_vector.capacity() * 4);
    
    m_vector.uncheckedAppend(v);
    m_bufferSlot = m_vector.dataSlot();
}

} // namespace KJS
