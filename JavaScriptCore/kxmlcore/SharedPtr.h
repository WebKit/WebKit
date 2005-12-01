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

#ifndef KXMLCORE_SHARED_PTR_H
#define KXMLCORE_SHARED_PTR_H

namespace KXMLCore {

    // FIXME: Change template name to RefPtr?
    template <class T> class SharedPtr
    {
    public:
        SharedPtr() : m_ptr(NULL) {}
        SharedPtr(T *ptr) : m_ptr(ptr) { if (ptr) ptr->ref(); }
        SharedPtr(const SharedPtr &o) : m_ptr(o.m_ptr) { if (T *ptr = m_ptr) ptr->ref(); }
        ~SharedPtr() { if (T *ptr = m_ptr) ptr->deref(); }
        
        template <class U> SharedPtr(const SharedPtr<U> &o) : m_ptr(o.get()) { if (T *ptr = m_ptr) ptr->ref(); }
        
        // FIXME: Deprecate in favor of operators below, then remove?
        bool isNull() const { return m_ptr == NULL; }
        bool notNull() const { return m_ptr != NULL; }
        
        // FIXME: Deprecate in favor of operator=, then remove?
        void reset() { if (T *ptr = m_ptr) ptr->deref(); m_ptr = NULL; }
        void reset(T *o) { if (o) o->ref(); if (T *ptr = m_ptr) ptr->deref(); m_ptr = o; }
        
        T *get() const { return m_ptr; }
        
        T &operator*() const { return *m_ptr; }
        T *operator->() const { return m_ptr; }
        
        bool operator!() const { return m_ptr == NULL; }

    
        // this type conversion operator allows implicit conversion to
        // bool but not to other integer types

        typedef T * (SharedPtr::*UnspecifiedBoolType)() const;
        operator UnspecifiedBoolType() const
        {
            return m_ptr ? &SharedPtr::get : 0;
        }
        
        SharedPtr &operator=(const SharedPtr &);
        SharedPtr &operator=(T *);
        
    private:
        T *m_ptr;
    };
    
    template <class T> SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<T> &o) 
    {
        T *optr = o.m_ptr;
        if (optr)
            optr->ref();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }
    
    template <class T> inline SharedPtr<T> &SharedPtr<T>::operator=(T *optr)
    {
        if (optr)
            optr->ref();
        if (T *ptr = m_ptr)
            ptr->deref();
        m_ptr = optr;
        return *this;
    }
    
    template <class T> inline bool operator==(const SharedPtr<T> &a, const SharedPtr<T> &b) 
    { 
        return a.get() == b.get(); 
    }

    template <class T> inline bool operator==(const SharedPtr<T> &a, const T *b) 
    { 
        return a.get() == b; 
    }
    
    template <class T> inline bool operator==(const T *a, const SharedPtr<T> &b) 
    {
        return a == b.get(); 
    }
    
    template <class T> inline bool operator!=(const SharedPtr<T> &a, const SharedPtr<T> &b) 
    { 
        return a.get() != b.get(); 
    }

    template <class T> inline bool operator!=(const SharedPtr<T> &a, const T *b)
    {
        return a.get() != b; 
    }

    template <class T> inline bool operator!=(const T *a, const SharedPtr<T> &b) 
    { 
        return a != b.get(); 
    }
    
    template <class T, class U> inline SharedPtr<T> static_pointer_cast(const SharedPtr<U> &p) 
    { 
        return SharedPtr<T>(static_cast<T *>(p.get())); 
    }

    template <class T, class U> inline SharedPtr<T> const_pointer_cast(const SharedPtr<U> &p) 
    { 
        return SharedPtr<T>(const_cast<T *>(p.get())); 
    }

} // namespace KXMLCore

using KXMLCore::SharedPtr;
using KXMLCore::static_pointer_cast;
using KXMLCore::const_pointer_cast;

#endif // KXMLCORE_SHARED_PTR_H
