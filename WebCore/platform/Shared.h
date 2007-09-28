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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
        , m_hasDeleted(false)
#endif
    {
    }

    void ref()
    {
        ASSERT(!m_hasDeleted);
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(!m_hasDeleted);
        if (--m_refCount <= 0) {
#ifndef NDEBUG
            m_hasDeleted = true;
#endif
            delete static_cast<T*>(this);
        }
    }

    bool hasOneRef()
    {
        ASSERT(!m_hasDeleted);
        return m_refCount == 1;
    }

    int refCount() const
    {
        return m_refCount;
    }

private:
    int m_refCount;
#ifndef NDEBUG
    bool m_hasDeleted;
#endif
};

template<class T> class TreeShared : Noncopyable {
public:
    TreeShared()
        : m_refCount(0)
        , m_parent(0)
#ifndef NDEBUG
        , m_hasRemovedLastRef(false)
#endif
    {
    }
    TreeShared(T* parent)
        : m_refCount(0)
        , m_parent(parent)
#ifndef NDEBUG
        , m_hasRemovedLastRef(false)
#endif
    {
    }
    virtual ~TreeShared() { }

    virtual void removedLastRef() { delete static_cast<T*>(this); }

    void ref()
    {
        ASSERT(!m_hasRemovedLastRef);
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(!m_hasRemovedLastRef);
        if (--m_refCount <= 0 && !m_parent) {
#ifndef NDEBUG
            m_hasRemovedLastRef = true;
#endif
            removedLastRef();
        }
    }

    bool hasOneRef() const
    {
        ASSERT(!m_hasRemovedLastRef);
        return m_refCount == 1;
    }

    int refCount() const
    {
        return m_refCount;
    }

    void setParent(T* parent) { m_parent = parent; }
    T* parent() const { return m_parent; }

private:
    int m_refCount;
    T* m_parent;
#ifndef NDEBUG
    bool m_hasRemovedLastRef;
#endif
};

}

#endif
