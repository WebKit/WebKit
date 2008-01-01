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
    const_iterator start = min(begin() + startIndex, end());
    result.m_vector.appendRange(start, end());
}

List::ListSet& List::markSet()
{
    static ListSet staticMarkSet;
    return staticMarkSet;
}

void List::markProtectedListsSlowCase()
{
    ListSet::iterator end = markSet().end();
    for (ListSet::iterator it = markSet().begin(); it != end; ++it) {
        List* list = *it;

        iterator end2 = list->end();
        for (iterator it2 = list->begin(); it2 != end2; ++it2) {
            JSValue* v = *it2;
            if (!v->marked())
                v->mark();
        }
    }
}

void List::expandAndAppend(JSValue* v)
{
    ASSERT(m_vector.size() == m_vector.capacity());
    
    // 4x growth would be excessive for a normal vector, but it's OK for Lists 
    // because they're short-lived.
    m_vector.reserveCapacity(m_vector.capacity() * 4);
    
    // As long as our size stays within our Vector's inline 
    // capacity, all our values are allocated on the stack, and 
    // therefore don't need explicit marking. Once our size exceeds
    // our Vector's inline capacity, though, our values move to the 
    // heap, where they do need explicit marking.
    if (!m_isInMarkSet) {
        markSet().add(this);
        m_isInMarkSet = true;
    }

    m_vector.uncheckedAppend(v);
}

} // namespace KJS
