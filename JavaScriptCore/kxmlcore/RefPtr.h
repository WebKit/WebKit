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

#ifndef KXMLCORE_REF_PTR_H
#define KXMLCORE_REF_PTR_H

namespace KXMLCore {

    template <class T> class PassRefPtr;
    template <class T> class PassRefPtr_Ref;

    template <class T> class RefPtr
    {
    public:
        RefPtr() : m_ptr(0) {}
        RefPtr(T *ptr) : m_ptr(ptr) { if (ptr) ptr->ref(); }
        RefPtr(const RefPtr& o) : m_ptr(o.m_ptr) { if (T *ptr = m_ptr) ptr->ref(); }

        ~RefPtr() { if (T *ptr = m_ptr) ptr->deref(); }
        
        template <class U> RefPtr(const RefPtr<U>& o) : m_ptr(o.get()) { if (T *ptr = m_ptr) ptr->ref(); }
        
        T *get() const { return m_ptr; }
        
        T& operator*() const { return *m_ptr; }
        T *operator->() const { return m_ptr; }
        
        bool operator!() const { return !m_ptr; }
    
        // This conversion operator allows implicit conversion to bool but not to other integer types.
        typedef T * (RefPtr::*UnspecifiedBoolType)() const;
        operator UnspecifiedBoolType() const { return m_ptr ? &RefPtr::get : 0; }
        
        RefPtr& operator=(const RefPtr&);
        RefPtr& operator=(T *);
        RefPtr& operator=(PassRefPtr<T>&);
        RefPtr& operator=(PassRefPtr_Ref<T>);

    private:
        T *m_ptr;
    };
    
    template <class T> RefPtr<T>& RefPtr<T>::operator=(const RefPtr<T>& o) 
    {
        T *optr = o.m_ptr;
        if (optr)
            optr->ref();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }
    
    template <class T> inline RefPtr<T>& RefPtr<T>::operator=(T *optr)
    {
        if (optr)
            optr->ref();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }

    template <class T> inline RefPtr<T>& RefPtr<T>::operator=(PassRefPtr_Ref<T> ref)
    {
        if (m_ptr)
            m_ptr->deref();
        m_ptr = ref.m_ptr;
        return *this;
    }
    
    template <class T> inline RefPtr<T>& RefPtr<T>::operator=(PassRefPtr<T>& o) 
    {
        T *optr = o.release();
        if (optr)
            optr->ref();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }

    template <class T> inline bool operator==(const RefPtr<T>& a, const RefPtr<T>& b) 
    { 
        return a.get() == b.get(); 
    }

    template <class T> inline bool operator==(const RefPtr<T>& a, const T *b) 
    { 
        return a.get() == b; 
    }
    
    template <class T> inline bool operator==(const T *a, const RefPtr<T>& b) 
    {
        return a == b.get(); 
    }
    
    template <class T> inline bool operator!=(const RefPtr<T>& a, const RefPtr<T>& b) 
    { 
        return a.get() != b.get(); 
    }

    template <class T> inline bool operator!=(const RefPtr<T>& a, const T *b)
    {
        return a.get() != b; 
    }

    template <class T> inline bool operator!=(const T *a, const RefPtr<T>& b) 
    { 
        return a != b.get(); 
    }
    
    template <class T, class U> inline RefPtr<T> static_pointer_cast(const RefPtr<U>& p) 
    { 
        return RefPtr<T>(static_cast<T *>(p.get())); 
    }

    template <class T, class U> inline RefPtr<T> const_pointer_cast(const RefPtr<U>& p) 
    { 
        return RefPtr<T>(const_cast<T *>(p.get())); 
    }

} // namespace KXMLCore

using KXMLCore::RefPtr;
using KXMLCore::static_pointer_cast;
using KXMLCore::const_pointer_cast;

#endif // KXMLCORE_REF_PTR_H
