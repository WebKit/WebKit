// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KXMLCORE_PASS_REF_PTR_H
#define KXMLCORE_PASS_REF_PTR_H

namespace KXMLCore {

    template<typename T> class RefPtr;
    template<typename T> class PassRefPtr;

    // PassRefPtr_Ref class is a helper to allow proper passing of PassRefPtr by value but not by const
    // reference
    template<typename T> 
    struct PassRefPtr_Ref
    {
        T* m_ptr;
        explicit PassRefPtr_Ref(T* p) : m_ptr(p) {}
    };

    template<typename T> 
    class PassRefPtr
    {
    public:
        PassRefPtr() : m_ptr(0) {}
        PassRefPtr(T *ptr) : m_ptr(ptr) { if (ptr) ptr->ref(); }
        PassRefPtr(const RefPtr<T>& o) : m_ptr(o.get()) { if (T *ptr = m_ptr) ptr->ref(); }

        ~PassRefPtr() { if (T *ptr = m_ptr) ptr->deref(); }
        
        template <class U> 
        PassRefPtr(const RefPtr<U>& o) : m_ptr(o.get()) { if (T *ptr = m_ptr) ptr->ref(); }
        
        T *get() const { return m_ptr; }

        T *release() { T *tmp = m_ptr; m_ptr = 0; return tmp; }

        static PassRefPtr<T> adopt(T *ptr) 
        { 
            PassRefPtr result; 
            result.m_ptr = ptr; 
            return result; 
        }
        
        T& operator*() const { return *m_ptr; }
        T *operator->() const { return m_ptr; }
        
        bool operator!() const { return !m_ptr; }

        // This conversion operator allows implicit conversion to bool but not to other integer types.
        typedef T* (PassRefPtr::*UnspecifiedBoolType)() const;
        operator UnspecifiedBoolType() const { return m_ptr ? &PassRefPtr::get : 0; }
        
        PassRefPtr& operator=(const RefPtr<T>&);
        PassRefPtr& operator=(T *);
        PassRefPtr& operator=(PassRefPtr&);

        PassRefPtr(PassRefPtr_Ref<T> ref) : m_ptr(ref.m_ptr) { }
      
        PassRefPtr& operator=(PassRefPtr_Ref<T> ref)
        {
            if (m_ptr)
                m_ptr->deref();
            m_ptr = ref.m_ptr;
            return *this;
        }
      
        template<typename U>
        operator PassRefPtr_Ref<U>()
        { 
            return PassRefPtr_Ref<U>(release()); 
        }

        template<typename U>
        operator PassRefPtr<U>()
        { 
            return PassRefPtr<U>(this->release()); 
        }
        
    private:
        T *m_ptr;
    };
    
    template <class T> inline PassRefPtr<T>& PassRefPtr<T>::operator=(const RefPtr<T>& o) 
    {
        T *optr = o.m_ptr;
        if (optr)
            optr->ref();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }
    
    template <class T> inline PassRefPtr<T>& PassRefPtr<T>::operator=(T *optr)
    {
        if (optr)
            optr->ref();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }

    template <class T> inline PassRefPtr<T>& PassRefPtr<T>::operator=(PassRefPtr<T>& ref)
    {
        T *optr = ref.release();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }
    
    template <class T> inline bool operator==(const PassRefPtr<T>& a, const PassRefPtr<T>& b) 
    { 
        return a.get() == b.get(); 
    }

    template <class T> inline bool operator==(const PassRefPtr<T>& a, const RefPtr<T>& b) 
    { 
        return a.get() == b.get(); 
    }

    template <class T> inline bool operator==(const RefPtr<T>& a, const PassRefPtr<T>& b) 
    { 
        return a.get() == b.get(); 
    }

    template <class T> inline bool operator==(const PassRefPtr<T>& a, const T *b) 
    { 
        return a.get() == b; 
    }
    
    template <class T> inline bool operator==(const T *a, const PassRefPtr<T>& b) 
    {
        return a == b.get(); 
    }
    
    template <class T> inline bool operator!=(const PassRefPtr<T>& a, const PassRefPtr<T>& b) 
    { 
        return a.get() != b.get(); 
    }

    template <class T> inline bool operator!=(const PassRefPtr<T>& a, const RefPtr<T>& b) 
    { 
        return a.get() != b.get(); 
    }

    template <class T> inline bool operator!=(const RefPtr<T>& a, const PassRefPtr<T>& b) 
    { 
        return a.get() != b.get(); 
    }

    template <class T> inline bool operator!=(const PassRefPtr<T>& a, const T *b)
    {
        return a.get() != b; 
    }

    template <class T> inline bool operator!=(const T *a, const PassRefPtr<T>& b) 
    { 
        return a != b.get(); 
    }
    
    template <class T, class U> inline PassRefPtr<T> static_pointer_cast(const PassRefPtr<U>& p) 
    { 
        return PassRefPtr<T>::adopt(static_cast<T *>(p.release())); 
    }

    template <class T, class U> inline PassRefPtr<T> const_pointer_cast(const PassRefPtr<U>& p) 
    { 
        return PassRefPtr<T>::adopt(const_cast<T *>(p.release())); 
    }

} // namespace KXMLCore

using KXMLCore::PassRefPtr;
using KXMLCore::static_pointer_cast;
using KXMLCore::const_pointer_cast;

#endif // KXMLCORE_PASS_REF_PTR_H
