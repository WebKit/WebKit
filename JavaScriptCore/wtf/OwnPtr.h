// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef WTF_OwnPtr_h
#define WTF_OwnPtr_h

#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

#if PLATFORM(WIN)

typedef struct HBITMAP__* HBITMAP;
typedef struct HBRUSH__* HBRUSH;
typedef struct HFONT__* HFONT;
typedef struct HPALETTE__* HPALETTE;
typedef struct HPEN__* HPEN;
typedef struct HRGN__* HRGN;

#endif

namespace WTF {

    // Unlike most of our smart pointers, OwnPtr can take either the pointer type or the pointed-to type.

    // FIXME: Share a single RemovePointer class template with RetainPtr.
    template <typename T> struct OwnPtrRemovePointer { typedef T type; };
    template <typename T> struct OwnPtrRemovePointer<T*> { typedef T type; };

    template <typename T> inline void deleteOwnedPtr(T* ptr)
    {
        typedef char known[sizeof(T) ? 1 : -1];
        if (sizeof(known))
            delete ptr;
    }

#if PLATFORM(WIN)
    void deleteOwnedPtr(HBITMAP);
    void deleteOwnedPtr(HBRUSH);
    void deleteOwnedPtr(HFONT);
    void deleteOwnedPtr(HPALETTE);
    void deleteOwnedPtr(HPEN);
    void deleteOwnedPtr(HRGN);
#endif

    template <typename T> class OwnPtr : Noncopyable {
    public:
        typedef typename OwnPtrRemovePointer<T>::type ValueType;
        typedef ValueType* PtrType;

        explicit OwnPtr(PtrType ptr = 0) : m_ptr(ptr) { }
        ~OwnPtr() { deleteOwnedPtr(m_ptr); }

        PtrType get() const { return m_ptr; }
        PtrType release() { PtrType ptr = m_ptr; m_ptr = 0; return ptr; }

        void set(PtrType ptr) { ASSERT(!ptr || m_ptr != ptr); deleteOwnedPtr(m_ptr); m_ptr = ptr; }
        void clear() { deleteOwnedPtr(m_ptr); m_ptr = 0; }

        ValueType& operator*() const { ASSERT(m_ptr); return *m_ptr; }
        PtrType operator->() const { ASSERT(m_ptr); return m_ptr; }

        bool operator!() const { return !m_ptr; }

        // This conversion operator allows implicit conversion to bool but not to other integer types.
        typedef PtrType OwnPtr::*UnspecifiedBoolType;
        operator UnspecifiedBoolType() const { return m_ptr ? &OwnPtr::m_ptr : 0; }

        void swap(OwnPtr& o) { std::swap(m_ptr, o.m_ptr); }

    private:
        PtrType m_ptr;
    };
    
    template <typename T> inline void swap(OwnPtr<T>& a, OwnPtr<T>& b) { a.swap(b); }

    template <typename T, typename U> inline bool operator==(const OwnPtr<T>& a, U* b)
    { 
        return a.get() == b; 
    }
    
    template <typename T, typename U> inline bool operator==(T* a, const OwnPtr<U>& b) 
    {
        return a == b.get(); 
    }

    template <typename T, typename U> inline bool operator!=(const OwnPtr<T>& a, U* b)
    {
        return a.get() != b; 
    }

    template <typename T, typename U> inline bool operator!=(T* a, const OwnPtr<U>& b)
    { 
        return a != b.get(); 
    }
    
    template <typename T> inline typename OwnPtr<T>::PtrType getPtr(const OwnPtr<T>& p)
    {
        return p.get();
    }

} // namespace WTF

using WTF::OwnPtr;

#endif // WTF_OwnPtr_h
