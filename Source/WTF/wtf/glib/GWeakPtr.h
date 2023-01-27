/*
 *  Copyright (C) 2023 Igalia S.L.
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

#pragma once

#if USE(GLIB)

#include <glib-object.h>
#include <wtf/Noncopyable.h>

namespace WTF {

template <typename T> class GWeakPtr {
    WTF_MAKE_NONCOPYABLE(GWeakPtr);
public:
    GWeakPtr() = default;

    explicit GWeakPtr(T* ptr)
        : m_ptr(ptr)
    {
        RELEASE_ASSERT(!ptr || G_IS_OBJECT(ptr));
        addWeakPtr();
    }

    GWeakPtr(GWeakPtr&& other)
    {
        reset(other.get());
        other.reset();
    }

    ~GWeakPtr()
    {
        removeWeakPtr();
    }

    T& operator*() const
    {
        ASSERT(m_ptr);
        return *m_ptr;
    }

    T* operator->() const
    {
        ASSERT(m_ptr);
        return m_ptr;
    }

    T* get() const
    {
        return m_ptr;
    }

    void reset(T* ptr = nullptr)
    {
        RELEASE_ASSERT(!ptr || G_IS_OBJECT(ptr));
        removeWeakPtr();
        m_ptr = ptr;
        addWeakPtr();
    }

    GWeakPtr& operator=(std::nullptr_t)
    {
        reset();
        return *this;
    }

    bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef T* GWeakPtr::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_ptr ? &GWeakPtr::m_ptr : 0; }

private:
    void addWeakPtr()
    {
        if (m_ptr)
            g_object_add_weak_pointer(G_OBJECT(m_ptr), reinterpret_cast<void**>(&m_ptr));
    }

    void removeWeakPtr()
    {
        if (m_ptr)
            g_object_remove_weak_pointer(G_OBJECT(m_ptr), reinterpret_cast<void**>(&m_ptr));
    }

    T* m_ptr { nullptr };
};

} // namespace WTF

using WTF::GWeakPtr;

#endif // USE(GLIB)
