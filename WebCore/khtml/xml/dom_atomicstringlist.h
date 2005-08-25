/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef _DOM_AtomicStringList_h_
#define _DOM_AtomicStringList_h_

#include "dom/dom_string.h"
#include "dom_atomicstring.h"

namespace DOM {

class AtomicStringList {
public:
    AtomicStringList() :m_next(0) {}
    AtomicStringList(const AtomicString& str, AtomicStringList* n = 0) :m_string(str), m_next(n) {}
    ~AtomicStringList() { delete m_next; }
    
    AtomicStringList* next() const { return m_next; }
    void setNext(AtomicStringList* n) { m_next = n; }
    const AtomicString& qstring() const { return m_string; }
    void setString(const AtomicString& str) { m_string = str; }

    AtomicStringList* clone() { return new AtomicStringList(m_string, m_next ? m_next->clone() : 0); }
    void clear() { m_string = nullAtom; delete m_next; m_next = 0; }
    
private:
    AtomicString m_string;
    AtomicStringList* m_next;
};

}
#endif
