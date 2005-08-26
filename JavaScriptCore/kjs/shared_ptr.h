// -*- c-basic-offset: 4 -*-
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

namespace kxmlcore {

template <class T> class SharedPtr
{
public:
    SharedPtr() : m_ptr(0) {}
    explicit SharedPtr(T *ptr) : m_ptr(ptr) { if (m_ptr) m_ptr->ref(); }
    SharedPtr(const SharedPtr &o) : m_ptr(o.m_ptr) { if (m_ptr) m_ptr->ref(); }
    ~SharedPtr() { if (m_ptr) m_ptr->deref(); }

    template <class U> explicit SharedPtr(SharedPtr<U> o)  : m_ptr(o.get()) { if (m_ptr) m_ptr->ref(); }
	
    bool isNull() const { return m_ptr == 0; }
    bool notNull() const { return m_ptr != 0; }

    void reset() { if (m_ptr) m_ptr->deref(); m_ptr = 0; }
    void reset(T *o) { if (o) o->ref(); if (m_ptr) m_ptr->deref(); m_ptr = o; }
    
    T * get() const { return m_ptr; }
    T &operator*() const { return *m_ptr; }
    T *operator->() const { return m_ptr; }

    bool operator!() const { return m_ptr == 0; }
    operator bool() const { return m_ptr != 0; }

    inline friend bool operator==(const SharedPtr &a, const SharedPtr &b) { return a.m_ptr == b.m_ptr; }
    inline friend bool operator==(const SharedPtr &a, const T *b) { return a.m_ptr == b; }
    inline friend bool operator==(const T *a, const SharedPtr &b) { return a == b.m_ptr; }

    SharedPtr &operator=(const SharedPtr &);
    SharedPtr &operator=(T *);

private:
    T* m_ptr;
};

template <class T> SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<T> &o) 
{
    if (o.m_ptr) 
        o.m_ptr->ref();
    if (m_ptr)
        m_ptr->deref();
    m_ptr = o.m_ptr;
    return *this;
}

template <class T> inline SharedPtr<T> &SharedPtr<T>::operator=(T *ptr) 
{
    if (ptr) 
        ptr->ref();
    if (m_ptr)
        m_ptr->deref();
    m_ptr = ptr;
    return *this;
}

template <class T> inline bool operator!=(const SharedPtr<T> &a, const SharedPtr<T> &b) { return !(a==b); }
template <class T> inline bool operator!=(const SharedPtr<T> &a, const T *b) { return !(a == b); }
template <class T> inline bool operator!=(const T *a, const SharedPtr<T> &b) { return !(a == b); }

template <class T, class U> inline SharedPtr<T> static_pointer_cast(const SharedPtr<U> &p) { return SharedPtr<T>(static_cast<T *>(p.get())); }
template <class T, class U> inline SharedPtr<T> const_pointer_cast(const SharedPtr<U> &p) { return SharedPtr<T>(const_cast<T *>(p.get())); }

}

#endif // KXMLCORE_SHARED_PTR_H
