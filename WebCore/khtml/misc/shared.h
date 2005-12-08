/*
    This file is part of the KDE libraries

    Copyright (C) 2005 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KHTML_SHARED_H
#define KHTML_SHARED_H

#include <kxmlcore/RefPtr.h>

namespace khtml {

template<class T> class Shared
{
public:
    Shared() : m_refCount(0) { }

    void ref() { ++m_refCount; }
    void deref() { if (--m_refCount <= 0) delete static_cast<T*>(this); }
    bool hasOneRef() { return m_refCount == 1; }
    int refCount() const { return m_refCount; }

private:
    int m_refCount;

    Shared(const Shared&);
    Shared& operator=(const Shared&);
};

template<class T> class TreeShared
{
public:
    TreeShared() : m_refCount(0), m_parent(0) { }
    TreeShared(T* parent) : m_refCount(0), m_parent(parent) { }
    virtual ~TreeShared() { }

    virtual void removedLastRef() { delete static_cast<T*>(this); }

    void ref() { ++m_refCount;  }
    void deref() { if (--m_refCount <= 0 && !m_parent) removedLastRef(); }
    bool hasOneRef() { return m_refCount == 1; }
    int refCount() const { return m_refCount; }

    void setParent(T* parent) { m_parent = parent; }
    T* parent() const { return m_parent; }

private:
    int m_refCount;
    T* m_parent;

    TreeShared(const TreeShared&);
    TreeShared& operator=(const TreeShared&);
};

}

#endif
