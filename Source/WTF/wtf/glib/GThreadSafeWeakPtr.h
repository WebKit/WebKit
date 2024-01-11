/*
 *  Copyright (C) 2024 Igalia S.L.
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

#include "GRefPtr.h"

#include <glib-object.h>
#include <wtf/Noncopyable.h>

namespace WTF {

template <typename T> class GThreadSafeWeakPtr {
    WTF_MAKE_NONCOPYABLE(GThreadSafeWeakPtr);
public:
    GThreadSafeWeakPtr()
    {
        g_weak_ref_init(&m_ref, nullptr);
    }

    explicit GThreadSafeWeakPtr(T* ptr)
    {
        RELEASE_ASSERT(!ptr || G_IS_OBJECT(ptr));
        g_weak_ref_init(&m_ref, ptr);
    }

    GThreadSafeWeakPtr(GThreadSafeWeakPtr&& other)
    {
        auto strongRef = other.get();
        g_weak_ref_set(&other.m_ref, nullptr);
        g_weak_ref_init(&m_ref, strongRef.get());
    }

    ~GThreadSafeWeakPtr()
    {
        g_weak_ref_clear(&m_ref);
    }

    WARN_UNUSED_RETURN GRefPtr<T> get()
    {
        return adoptGRef(reinterpret_cast<T*>(g_weak_ref_get(&m_ref)));
    }

    void reset(T* ptr = nullptr)
    {
        RELEASE_ASSERT(!ptr || G_IS_OBJECT(ptr));
        g_weak_ref_set(&m_ref, ptr);
    }

    GThreadSafeWeakPtr& operator=(std::nullptr_t)
    {
        reset();
        return *this;
    }

private:
    GWeakRef m_ref;
};

} // namespace WTF

using WTF::GThreadSafeWeakPtr;

#endif // USE(GLIB)
