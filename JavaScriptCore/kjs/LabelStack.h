// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef KJS_LABEL_STACK_H
#define KJS_LABEL_STACK_H

#include "identifier.h"
#include <wtf/Noncopyable.h>

namespace KJS {
  /**
   * @short The "label set" in Ecma-262 spec
   */
  class LabelStack : Noncopyable {
  public:
    LabelStack()
      : tos(0)
    {
    }
    ~LabelStack();

    /**
     * If id is not empty and is not in the stack already, puts it on top of
     * the stack and returns true, otherwise returns false
     */
    bool push(const Identifier &id);
    /**
     * Is the id in the stack?
     */
    bool contains(const Identifier &id) const;
    /**
     * Removes from the stack the last pushed id (what else?)
     */
    void pop();

  private:
    struct StackElem {
      Identifier id;
      StackElem *prev;
    };

    StackElem *tos;
  };

inline LabelStack::~LabelStack()
{
    StackElem *prev;
    for (StackElem *e = tos; e; e = prev) {
        prev = e->prev;
        delete e;
    }
}

inline void LabelStack::pop()
{
    if (StackElem *e = tos) {
        tos = e->prev;
        delete e;
    }
}

}

#endif // KJS_LABEL_STACK_H
