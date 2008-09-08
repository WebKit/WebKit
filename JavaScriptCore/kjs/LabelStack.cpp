/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#include "LabelStack.h"

namespace JSC {

bool LabelStack::push(const Identifier& id)
{
    if (contains(id))
        return false;

    StackElem* newTopOfStack = new StackElem;
    newTopOfStack->id = id;
    newTopOfStack->prev = m_topOfStack;
    m_topOfStack = newTopOfStack;
    return true;
}

bool LabelStack::contains(const Identifier &id) const
{
    if (id.isEmpty())
        return true;

    for (StackElem* curr = m_topOfStack; curr; curr = curr->prev) {
        if (curr->id == id)
            return true;
    }

    return false;
}

} // namespace JSC
