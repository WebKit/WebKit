// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */


#ifndef _KJS_PROTECT_H_
#define _KJS_PROTECT_H_

#include "value.h"
#include "protected_values.h"
#include "interpreter.h"

namespace KJS {

    inline void gcProtect(ValueImp *val) 
    { 
	ProtectedValues::increaseProtectCount(val);
    }

    inline void gcUnprotect(ValueImp *val)
    { 
	ProtectedValues::decreaseProtectCount(val);
    }

    inline void gcProtectNullTolerant(ValueImp *val) 
    {
	if (val) gcProtect(val);
    }

    inline void gcUnprotectNullTolerant(ValueImp *val) 
    {
	if (val) gcUnprotect(val);
    }
    
    // FIXME: Share more code with SharedPtr template? The only difference is the ref/deref operation.
    template <class T> class ProtectedPtr {
    public:
        ProtectedPtr() : m_ptr(NULL) { }
        ProtectedPtr(T *ptr);
        ProtectedPtr(const ProtectedPtr &);
        ~ProtectedPtr();

        template <class U> ProtectedPtr(const ProtectedPtr<U> &);
        
        T *get() const { return m_ptr; }
        operator T *() const { return m_ptr; }
        T *operator->() const { return m_ptr; }
        
        bool operator!() const { return m_ptr == NULL; }
        operator bool() const { return m_ptr != NULL; }
        
        ProtectedPtr &operator=(const ProtectedPtr &);
        ProtectedPtr &operator=(T *);
        
    private:
        T *m_ptr;
        
        operator int() const; // deliberately not implemented; helps prevent operator bool from converting to int accidentally
    };

    template <class T> ProtectedPtr<T>::ProtectedPtr(T *ptr)
        : m_ptr(ptr)
    {
        if (ptr) {
            InterpreterLock lock;
            gcProtect(ptr);
        }
    }

    template <class T> ProtectedPtr<T>::ProtectedPtr(const ProtectedPtr &o)
        : m_ptr(o.get())
    {
        if (T *ptr = m_ptr) {
            InterpreterLock lock;
            gcProtect(ptr);
        }
    }

    template <class T> ProtectedPtr<T>::~ProtectedPtr()
    {
        if (T *ptr = m_ptr) {
            InterpreterLock lock;
            gcUnprotect(ptr);
        }
    }

    template <class T> template <class U> ProtectedPtr<T>::ProtectedPtr(const ProtectedPtr<U> &o)
        : m_ptr(o.get())
    {
        if (T *ptr = m_ptr) {
            InterpreterLock lock;
            gcProtect(ptr);
        }
    }

    template <class T> ProtectedPtr<T> &ProtectedPtr<T>::operator=(const ProtectedPtr<T> &o) 
    {
        InterpreterLock lock;
        T *optr = o.m_ptr;
        gcProtectNullTolerant(optr);
        gcUnprotectNullTolerant(m_ptr);
        m_ptr = optr;
         return *this;
     }

    template <class T> inline ProtectedPtr<T> &ProtectedPtr<T>::operator=(T *optr)
    {
        InterpreterLock lock;
        gcProtectNullTolerant(optr);
        gcUnprotectNullTolerant(m_ptr);
        m_ptr = optr;
        return *this;
    }

    template <class T> inline bool operator==(const ProtectedPtr<T> &a, const ProtectedPtr<T> &b) { return a.get() == b.get(); }
    template <class T> inline bool operator==(const ProtectedPtr<T> &a, const T *b) { return a.get() == b; }
    template <class T> inline bool operator==(const T *a, const ProtectedPtr<T> &b) { return a == b.get(); }

    template <class T> inline bool operator!=(const ProtectedPtr<T> &a, const ProtectedPtr<T> &b) { return a.get() != b.get(); }
    template <class T> inline bool operator!=(const ProtectedPtr<T> &a, const T *b) { return a.get() != b; }
    template <class T> inline bool operator!=(const T *a, const ProtectedPtr<T> &b) { return a != b.get(); }
 
} // namespace

#endif
