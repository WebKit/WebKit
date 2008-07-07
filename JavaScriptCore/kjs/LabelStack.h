/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef LabelStack_h
#define LabelStack_h

#include "identifier.h"
#include <wtf/Noncopyable.h>

namespace KJS {

    class LabelStack : Noncopyable {
    public:
        LabelStack()
            : m_topOfStack(0)
        {
        }
        ~LabelStack();

        bool push(const Identifier &id);
        bool contains(const Identifier &id) const;
        void pop();

    private:
        struct StackElem {
            Identifier id;
            StackElem* prev;
        };

        StackElem* m_topOfStack;
    };

    inline LabelStack::~LabelStack()
    {
        StackElem* prev;
        for (StackElem* e = m_topOfStack; e; e = prev) {
            prev = e->prev;
            delete e;
        }
    }

    inline void LabelStack::pop()
    {
        if (StackElem* e = m_topOfStack) {
            m_topOfStack = e->prev;
            delete e;
        }
    }

} // namespace KJS

#endif // LabelStack_h
