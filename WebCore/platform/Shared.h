/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef Shared_h
#define Shared_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

template<class T> class Shared : Noncopyable {
public:
    Shared()
        : m_refCount(0)
#ifndef NDEBUG
        , m_inDestructor(0)
#endif
    {
    }

    void ref()
    {
        ASSERT(!m_inDestructor);
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(!m_inDestructor);
        if (--m_refCount <= 0) {
#ifndef NDEBUG
            m_inDestructor = true;
#endif
            delete static_cast<T*>(this);
        }
    }

    bool hasOneRef()
    {
        ASSERT(!m_inDestructor);
        return m_refCount == 1;
    }

    int refCount() const
    {
        return m_refCount;
    }

private:
    int m_refCount;
#ifndef NDEBUG
    bool m_inDestructor;
#endif
};

template<class T> class TreeShared {
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
