// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef MarkStack_h
#define MarkStack_h

#include <wtf/Vector.h>
#include "collector.h"
#include "value.h"
#include <stdio.h>

namespace KJS {

    class MarkStack {
    public:
        void push(JSValue* value)
        {
            if (value->marked())
                return;
            JSCell* cell = value->asCell();
            Collector::markCell(cell);
            if (!Collector::cellMayHaveRefs(cell))
                return;
            ASSERT(m_stack.size() < m_stack.capacity());
            m_stack.uncheckedAppend(cell);
        }

        void push(JSCell* cell)
        {
            if (cell->marked())
                return;
            Collector::markCell(cell);
            if (!Collector::cellMayHaveRefs(cell))
                return;
            ASSERT(m_stack.size() < m_stack.capacity());
            m_stack.uncheckedAppend(cell);
        }

        void pushAtom(JSValue* value)
        {
            if (value->marked())
                return;
            JSCell* cell = value->asCell();
            Collector::markCell(cell);
        }

        void pushAtom(JSCell* cell)
        {
            if (cell->marked())
                return;
            Collector::markCell(cell);
        }

        JSCell* pop()
        {
            ASSERT(m_stack.size() > 0);
            JSCell* cell = m_stack.last();
            m_stack.removeLast();
            return cell;
        }

        bool isEmpty()
        {
            return m_stack.isEmpty();
        }

        void reserveCapacity(size_t size) 
        {
            m_stack.reserveCapacity(size);
        }

    private:
        Vector<JSCell*> m_stack;
    };

}

#endif // MarkStack_h
