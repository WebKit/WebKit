/*
 * Copyright (C) 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef TreeShared_h
#define TreeShared_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>

// Remove this once we make all WebKit code compatible with stricter rules about TreeShared.
#define LOOSE_TREE_SHARED

namespace WebCore {

#ifndef NDEBUG
template<typename T> class TreeShared;
template<typename T> void adopted(TreeShared<T>*);
#endif

template<typename T> class TreeShared : public Noncopyable {
public:
    TreeShared(int initialRefCount = 1)
        : m_refCount(initialRefCount)
        , m_parent(0)
#ifndef NDEBUG
        , m_adoptionIsRequired(initialRefCount == 1)
#endif
    {
        ASSERT(isMainThread());
#ifndef NDEBUG
        m_deletionHasBegun = false;
        m_inRemovedLastRefFunction = false;
#endif
    }
    virtual ~TreeShared()
    {
        ASSERT(isMainThread());
        ASSERT(!m_refCount);
        ASSERT(m_deletionHasBegun);
#ifndef LOOSE_REF_COUNTED
        ASSERT(!m_adoptionIsRequired);
#endif
    }

    void ref()
    {
        ASSERT(isMainThread());
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
#ifndef LOOSE_REF_COUNTED
        ASSERT(!m_adoptionIsRequired);
#endif
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(isMainThread());
        ASSERT(m_refCount >= 0);
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
#ifndef LOOSE_REF_COUNTED
        ASSERT(!m_adoptionIsRequired);
#endif
        if (--m_refCount <= 0 && !m_parent) {
#ifndef NDEBUG
            m_inRemovedLastRefFunction = true;
#endif
            removedLastRef();
        }
    }

    bool hasOneRef() const
    {
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        return m_refCount == 1;
    }

    int refCount() const
    {
        return m_refCount;
    }

    void setParent(T* parent)
    { 
        ASSERT(isMainThread());
        m_parent = parent; 
    }

    T* parent() const
    {
        ASSERT(isMainThread());
        return m_parent;
    }

#ifndef NDEBUG
    bool m_deletionHasBegun;
    bool m_inRemovedLastRefFunction;
#endif

private:
#ifndef NDEBUG
    friend void adopted<>(TreeShared<T>*);
#endif

    virtual void removedLastRef()
    {
#ifndef NDEBUG
        m_deletionHasBegun = true;
#endif
        delete this;
    }

    int m_refCount;
    T* m_parent;
#ifndef NDEBUG
    bool m_adoptionIsRequired;
#endif
};

#ifndef NDEBUG

template<typename T> inline void adopted(TreeShared<T>* object)
{
    if (!object)
        return;
    ASSERT(!object->m_deletionHasBegun);
    ASSERT(!object->m_inRemovedLastRefFunction);
    object->m_adoptionIsRequired = false;
}

#endif

}

#endif // TreeShared.h
