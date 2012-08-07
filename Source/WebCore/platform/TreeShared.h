/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

#ifndef NDEBUG
template<typename NodeType, typename ParentNodeType> class TreeShared;
template<typename NodeType, typename ParentNodeType> void adopted(TreeShared<NodeType, ParentNodeType>*);
#endif

template<typename NodeType, typename ParentNodeType> class TreeShared {
    WTF_MAKE_NONCOPYABLE(TreeShared);
protected:
    TreeShared()
        : m_parent(0)
        , m_refCount(1)
#ifndef NDEBUG
        , m_adoptionIsRequired(true)
#endif
    {
        ASSERT(isMainThread());
#ifndef NDEBUG
        m_deletionHasBegun = false;
        m_inRemovedLastRefFunction = false;
#endif
    }

    ~TreeShared()
    {
        ASSERT(isMainThread());
        ASSERT(!m_refCount);
        ASSERT(m_deletionHasBegun);
        ASSERT(!m_adoptionIsRequired);
    }

public:
    void ref()
    {
        ASSERT(isMainThread());
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        ASSERT(!m_adoptionIsRequired);
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(isMainThread());
        ASSERT(m_refCount >= 0);
        ASSERT(!m_deletionHasBegun);
        ASSERT(!m_inRemovedLastRefFunction);
        ASSERT(!m_adoptionIsRequired);
        if (--m_refCount <= 0 && !m_parent) {
#ifndef NDEBUG
            m_inRemovedLastRefFunction = true;
#endif
            static_cast<NodeType*>(this)->removedLastRef();
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

    void setParent(ParentNodeType* parent)
    { 
        ASSERT(isMainThread());
        m_parent = parent; 
    }

    ParentNodeType* parent() const
    {
        ASSERT(isMainThreadOrGCThread());
        return m_parent;
    }

#ifndef NDEBUG
    bool m_deletionHasBegun;
    bool m_inRemovedLastRefFunction;
#endif

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::DOM);
        info.addInstrumentedMember(m_parent);
    }

private:
#ifndef NDEBUG
    friend void adopted<>(TreeShared<NodeType, ParentNodeType>*);
#endif

    ParentNodeType* m_parent;
    int m_refCount;

#ifndef NDEBUG
    bool m_adoptionIsRequired;
#endif
};

#ifndef NDEBUG

template<typename NodeType, typename ParentNodeType> inline void adopted(TreeShared<NodeType, ParentNodeType>* object)
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
