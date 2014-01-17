/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef WTF_PassRefPtr_h
#define WTF_PassRefPtr_h

#include "PassRef.h"

namespace WTF {

    template<typename T> PassRefPtr<T> adoptRef(T*);

    template<typename T> ALWAYS_INLINE void refIfNotNull(T* ptr)
    {
        if (LIKELY(ptr != nullptr))
            ptr->ref();
    }

    template<typename T> ALWAYS_INLINE void derefIfNotNull(T* ptr)
    {
        if (LIKELY(ptr != nullptr))
            ptr->deref();
    }

    template<typename T> class PassRefPtr {
    public:
        PassRefPtr() : m_ptr(nullptr) { }
        PassRefPtr(T* ptr) : m_ptr(ptr) { refIfNotNull(ptr); }
        // It somewhat breaks the type system to allow transfer of ownership out of
        // a const PassRefPtr. However, it makes it much easier to work with PassRefPtr
        // temporaries, and we don't have a need to use real const PassRefPtrs anyway.
        PassRefPtr(const PassRefPtr& o) : m_ptr(o.leakRef()) { }
        template<typename U> PassRefPtr(const PassRefPtr<U>& o) : m_ptr(o.leakRef()) { }

        ALWAYS_INLINE ~PassRefPtr() { derefIfNotNull(m_ptr); }

        template<typename U> PassRefPtr(const RefPtr<U>&);
        template<typename U> PassRefPtr(PassRef<U> reference) : m_ptr(&reference.leakRef()) { }

        T* get() const { return m_ptr; }

        T* leakRef() const WARN_UNUSED_RETURN;

        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }

        bool operator!() const { return !m_ptr; }

        // This conversion operator allows implicit conversion to bool but not to other integer types.
        typedef T* (PassRefPtr::*UnspecifiedBoolType);
        operator UnspecifiedBoolType() const { return m_ptr ? &PassRefPtr::m_ptr : nullptr; }

        friend PassRefPtr adoptRef<T>(T*);

    private:
        PassRefPtr& operator=(const PassRefPtr&) = delete;

        enum AdoptTag { Adopt };
        PassRefPtr(T* ptr, AdoptTag) : m_ptr(ptr) { }

        mutable T* m_ptr;
    };
    
    template<typename T> template<typename U> inline PassRefPtr<T>::PassRefPtr(const RefPtr<U>& o)
        : m_ptr(o.get())
    {
        T* ptr = m_ptr;
        refIfNotNull(ptr);
    }

    template<typename T> inline T* PassRefPtr<T>::leakRef() const
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    template<typename T, typename U> inline bool operator==(const PassRefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() == b.get(); 
    }

    template<typename T, typename U> inline bool operator==(const PassRefPtr<T>& a, const RefPtr<U>& b) 
    { 
        return a.get() == b.get(); 
    }

    template<typename T, typename U> inline bool operator==(const RefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() == b.get(); 
    }

    template<typename T, typename U> inline bool operator==(const PassRefPtr<T>& a, U* b) 
    { 
        return a.get() == b; 
    }
    
    template<typename T, typename U> inline bool operator==(T* a, const PassRefPtr<U>& b) 
    {
        return a == b.get(); 
    }
    
    template<typename T, typename U> inline bool operator!=(const PassRefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() != b.get(); 
    }

    template<typename T, typename U> inline bool operator!=(const PassRefPtr<T>& a, const RefPtr<U>& b) 
    { 
        return a.get() != b.get(); 
    }

    template<typename T, typename U> inline bool operator!=(const RefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() != b.get(); 
    }

    template<typename T, typename U> inline bool operator!=(const PassRefPtr<T>& a, U* b)
    {
        return a.get() != b; 
    }

    template<typename T, typename U> inline bool operator!=(T* a, const PassRefPtr<U>& b) 
    { 
        return a != b.get(); 
    }
    
    template<typename T> inline PassRefPtr<T> adoptRef(T* p)
    {
        adopted(p);
        return PassRefPtr<T>(p, PassRefPtr<T>::Adopt);
    }

    template<typename T, typename U> inline PassRefPtr<T> static_pointer_cast(const PassRefPtr<U>& p) 
    { 
        return adoptRef(static_cast<T*>(p.leakRef())); 
    }

    template<typename T> inline T* getPtr(const PassRefPtr<T>& p)
    {
        return p.get();
    }

} // namespace WTF

using WTF::PassRefPtr;
using WTF::adoptRef;
using WTF::static_pointer_cast;

#endif // WTF_PassRefPtr_h
