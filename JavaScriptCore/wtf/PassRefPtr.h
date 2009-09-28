/*
 *  Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "AlwaysInline.h"

namespace WTF {

    template<typename T> class RefPtr;
    template<typename T> class PassRefPtr;
    template <typename T> PassRefPtr<T> adoptRef(T*);
    
    // Remove inline for winscw compiler to prevent the compiler agressively resolving 
    // T::deref(), which will fail compiling when PassRefPtr<T> is used as class member 
    // or function arguments before T is defined.
    template<typename T> 
#if !COMPILER(WINSCW)
    inline 
#endif
    void derefIfNotNull(T* ptr)
    {
        if (UNLIKELY(ptr != 0))
            ptr->deref();
    }

    template<typename T> class PassRefPtr {
    public:
        PassRefPtr() : m_ptr(0) {}
        PassRefPtr(T* ptr) : m_ptr(ptr) { if (ptr) ptr->ref(); }
        // It somewhat breaks the type system to allow transfer of ownership out of
        // a const PassRefPtr. However, it makes it much easier to work with PassRefPtr
        // temporaries, and we don't really have a need to use real const PassRefPtrs 
        // anyway.
        PassRefPtr(const PassRefPtr& o) : m_ptr(o.releaseRef()) {}
        template <typename U> PassRefPtr(const PassRefPtr<U>& o) : m_ptr(o.releaseRef()) { }

        ALWAYS_INLINE ~PassRefPtr() { derefIfNotNull<T>(m_ptr); }

        template <class U> 
        PassRefPtr(const RefPtr<U>& o) : m_ptr(o.get()) { if (T* ptr = m_ptr) ptr->ref(); }
        
        T* get() const { return m_ptr; }

        void clear() { if (T* ptr = m_ptr) ptr->deref(); m_ptr = 0; }
        T* releaseRef() const { T* tmp = m_ptr; m_ptr = 0; return tmp; }

        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }
        
        bool operator!() const { return !m_ptr; }

        // This conversion operator allows implicit conversion to bool but not to other integer types.
        typedef T* (PassRefPtr::*UnspecifiedBoolType);
        operator UnspecifiedBoolType() const { return m_ptr ? &PassRefPtr::m_ptr : 0; }

        PassRefPtr& operator=(T*);
        PassRefPtr& operator=(const PassRefPtr&);
        template <typename U> PassRefPtr& operator=(const PassRefPtr<U>&);
        template <typename U> PassRefPtr& operator=(const RefPtr<U>&);

        friend PassRefPtr adoptRef<T>(T*);
    private:
        // adopting constructor
        PassRefPtr(T* ptr, bool) : m_ptr(ptr) {}
        mutable T* m_ptr;
    };
    
    // NonNullPassRefPtr: Optimized for passing non-null pointers. A NonNullPassRefPtr
    // begins life non-null, and can only become null through a call to releaseRef()
    // or clear().

    // FIXME: NonNullPassRefPtr could just inherit from PassRefPtr. However,
    // if we use inheritance, GCC's optimizer fails to realize that destruction
    // of a released NonNullPassRefPtr is a no-op. So, for now, just copy the
    // most important code from PassRefPtr.
    template <typename T> class NonNullPassRefPtr {
    public:
        NonNullPassRefPtr(T* ptr)
            : m_ptr(ptr)
        {
            ASSERT(m_ptr);
            m_ptr->ref();
        }

        template <class U> NonNullPassRefPtr(const RefPtr<U>& o)
            : m_ptr(o.get())
        {
            ASSERT(m_ptr);
            m_ptr->ref();
        }

        NonNullPassRefPtr(const NonNullPassRefPtr& o)
            : m_ptr(o.releaseRef())
        {
            ASSERT(m_ptr);
        }

        template <class U> NonNullPassRefPtr(const NonNullPassRefPtr<U>& o)
            : m_ptr(o.releaseRef())
        {
            ASSERT(m_ptr);
        }

        template <class U> NonNullPassRefPtr(const PassRefPtr<U>& o)
            : m_ptr(o.releaseRef())
        {
            ASSERT(m_ptr);
        }

        ALWAYS_INLINE ~NonNullPassRefPtr() { derefIfNotNull(m_ptr); }

        T* get() const { return m_ptr; }

        void clear() { derefIfNotNull(m_ptr); m_ptr = 0; }
        T* releaseRef() const { T* tmp = m_ptr; m_ptr = 0; return tmp; }

        T& operator*() const { return *m_ptr; }
        T* operator->() const { return m_ptr; }

    private:
        mutable T* m_ptr;
    };

    template <typename T> template <typename U> inline PassRefPtr<T>& PassRefPtr<T>::operator=(const RefPtr<U>& o) 
    {
        T* optr = o.get();
        if (optr)
            optr->ref();
        T* ptr = m_ptr;
        m_ptr = optr;
        if (ptr)
            ptr->deref();
        return *this;
    }
    
    template <typename T> inline PassRefPtr<T>& PassRefPtr<T>::operator=(T* optr)
    {
        if (optr)
            optr->ref();
        T* ptr = m_ptr;
        m_ptr = optr;
        if (ptr)
            ptr->deref();
        return *this;
    }

    template <typename T> inline PassRefPtr<T>& PassRefPtr<T>::operator=(const PassRefPtr<T>& ref)
    {
        T* ptr = m_ptr;
        m_ptr = ref.releaseRef();
        if (ptr)
            ptr->deref();
        return *this;
    }
    
    template <typename T> template <typename U> inline PassRefPtr<T>& PassRefPtr<T>::operator=(const PassRefPtr<U>& ref)
    {
        T* ptr = m_ptr;
        m_ptr = ref.releaseRef();
        if (ptr)
            ptr->deref();
        return *this;
    }
    
    template <typename T, typename U> inline bool operator==(const PassRefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() == b.get(); 
    }

    template <typename T, typename U> inline bool operator==(const PassRefPtr<T>& a, const RefPtr<U>& b) 
    { 
        return a.get() == b.get(); 
    }

    template <typename T, typename U> inline bool operator==(const RefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() == b.get(); 
    }

    template <typename T, typename U> inline bool operator==(const PassRefPtr<T>& a, U* b) 
    { 
        return a.get() == b; 
    }
    
    template <typename T, typename U> inline bool operator==(T* a, const PassRefPtr<U>& b) 
    {
        return a == b.get(); 
    }
    
    template <typename T, typename U> inline bool operator!=(const PassRefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() != b.get(); 
    }

    template <typename T, typename U> inline bool operator!=(const PassRefPtr<T>& a, const RefPtr<U>& b) 
    { 
        return a.get() != b.get(); 
    }

    template <typename T, typename U> inline bool operator!=(const RefPtr<T>& a, const PassRefPtr<U>& b) 
    { 
        return a.get() != b.get(); 
    }

    template <typename T, typename U> inline bool operator!=(const PassRefPtr<T>& a, U* b)
    {
        return a.get() != b; 
    }

    template <typename T, typename U> inline bool operator!=(T* a, const PassRefPtr<U>& b) 
    { 
        return a != b.get(); 
    }
    
    template <typename T> inline PassRefPtr<T> adoptRef(T* p)
    {
        return PassRefPtr<T>(p, true);
    }

    template <typename T, typename U> inline PassRefPtr<T> static_pointer_cast(const PassRefPtr<U>& p) 
    { 
        return adoptRef(static_cast<T*>(p.releaseRef())); 
    }

    template <typename T, typename U> inline PassRefPtr<T> const_pointer_cast(const PassRefPtr<U>& p) 
    { 
        return adoptRef(const_cast<T*>(p.releaseRef())); 
    }

    template <typename T> inline T* getPtr(const PassRefPtr<T>& p)
    {
        return p.get();
    }

} // namespace WTF

using WTF::PassRefPtr;
using WTF::NonNullPassRefPtr;
using WTF::adoptRef;
using WTF::static_pointer_cast;
using WTF::const_pointer_cast;

#endif // WTF_PassRefPtr_h
