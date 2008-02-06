/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef RefCounted_h
#define RefCounted_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

namespace WTF {

template<class T> class RefCounted : Noncopyable {
public:
    RefCounted(int initialRefCount = 0)
        : m_refCount(initialRefCount)
#ifndef NDEBUG
        , m_deletionHasBegun(false)
#endif
    {
    }

    void ref()
    {
        ASSERT(!m_deletionHasBegun);
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(!m_deletionHasBegun);
        ASSERT(m_refCount > 0);
        if (m_refCount == 1) {
#ifndef NDEBUG
            m_deletionHasBegun = true;
#endif
            delete static_cast<T*>(this);
        } else 
            --m_refCount;
    }

    bool hasOneRef()
    {
        ASSERT(!m_deletionHasBegun);
        return m_refCount == 1;
    }

    int refCount() const
    {
        return m_refCount;
    }

private:
    int m_refCount;
#ifndef NDEBUG
    bool m_deletionHasBegun;
#endif
};

} // namespace WTF

using WTF::RefCounted;

#endif // RefCounted_h
