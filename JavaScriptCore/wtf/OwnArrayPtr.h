/*
 *  Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef WTF_OwnArrayPtr_h
#define WTF_OwnArrayPtr_h

#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

namespace WTF {

    template <typename T> class OwnArrayPtr : public Noncopyable {
    public:
        explicit OwnArrayPtr(T* ptr = 0) : m_ptr(ptr) { }
        ~OwnArrayPtr() { safeDelete(); }

        T* get() const { return m_ptr; }
        T* release() { T* ptr = m_ptr; m_ptr = 0; return ptr; }

        void set(T* ptr) { ASSERT(m_ptr != ptr); safeDelete(); m_ptr = ptr; }
        void clear() { safeDelete(); m_ptr = 0; }

        T& operator*() const { ASSERT(m_ptr); return *m_ptr; }
        T* operator->() const { ASSERT(m_ptr); return m_ptr; }

        T& operator[](std::ptrdiff_t i) const { ASSERT(m_ptr); ASSERT(i >= 0); return m_ptr[i]; }

        bool operator!() const { return !m_ptr; }

        // This conversion operator allows implicit conversion to bool but not to other integer types.
#if COMPILER(WINSCW)
        operator bool() const { return m_ptr; }
#else
        typedef T* OwnArrayPtr::*UnspecifiedBoolType;
        operator UnspecifiedBoolType() const { return m_ptr ? &OwnArrayPtr::m_ptr : 0; }
#endif

        void swap(OwnArrayPtr& o) { std::swap(m_ptr, o.m_ptr); }

    private:
        void safeDelete() { typedef char known[sizeof(T) ? 1 : -1]; if (sizeof(known)) delete [] m_ptr; }

        T* m_ptr;
    };
    
    template <typename T> inline void swap(OwnArrayPtr<T>& a, OwnArrayPtr<T>& b) { a.swap(b); }

    template <typename T> inline T* getPtr(const OwnArrayPtr<T>& p)
    {
        return p.get();
    }

} // namespace WTF

using WTF::OwnArrayPtr;

#endif // WTF_OwnArrayPtr_h
