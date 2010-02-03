/*
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2008 Collabora Ltd.
 *  Copyright (C) 2010 Company 100, Inc.
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

#ifndef OwnPtrBrew_h
#define OwnPtrBrew_h

#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

// Forward delcarations at this point avoid the need to include BREW includes
// in WTF headers.
typedef struct _IFileMgr IFileMgr;
typedef struct _IFile IFile;
typedef struct IBitmap IBitmap;

namespace WTF {

template <typename T> void freeOwnedPtrBrew(T* ptr);
template<> void freeOwnedPtrBrew<IFileMgr>(IFileMgr*);
template<> void freeOwnedPtrBrew<IFile>(IFile*);
template<> void freeOwnedPtrBrew<IBitmap>(IBitmap*);

template <typename T> class OwnPtrBrew : public Noncopyable {
public:
    explicit OwnPtrBrew(T* ptr = 0) : m_ptr(ptr) { }
    ~OwnPtrBrew() { freeOwnedPtrBrew(m_ptr); }

    T* get() const { return m_ptr; }
    T* release()
    {
        T* ptr = m_ptr;
        m_ptr = 0;
        return ptr;
    }

    T*& outPtr()
    {
        ASSERT(!m_ptr);
        return m_ptr;
    }

    void set(T* ptr)
    {
        ASSERT(!ptr || m_ptr != ptr);
        freeOwnedPtrBrew(m_ptr);
        m_ptr = ptr;
    }

    void clear()
    {
        freeOwnedPtrBrew(m_ptr);
        m_ptr = 0;
    }

    T& operator*() const
    {
        ASSERT(m_ptr);
        return *m_ptr;
    }

    T* operator->() const
    {
        ASSERT(m_ptr);
        return m_ptr;
    }

    bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef T* OwnPtrBrew::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_ptr ? &OwnPtrBrew::m_ptr : 0; }

    void swap(OwnPtrBrew& o) { std::swap(m_ptr, o.m_ptr); }

private:
    T* m_ptr;
};

template <typename T> inline void swap(OwnPtrBrew<T>& a, OwnPtrBrew<T>& b)
{
    a.swap(b);
}

template <typename T, typename U> inline bool operator==(const OwnPtrBrew<T>& a, U* b)
{
    return a.get() == b;
}

template <typename T, typename U> inline bool operator==(T* a, const OwnPtrBrew<U>& b)
{
    return a == b.get();
}

template <typename T, typename U> inline bool operator!=(const OwnPtrBrew<T>& a, U* b)
{
    return a.get() != b;
}

template <typename T, typename U> inline bool operator!=(T* a, const OwnPtrBrew<U>& b)
{
    return a != b.get();
}

template <typename T> inline typename OwnPtrBrew<T>::PtrType getPtr(const OwnPtrBrew<T>& p)
{
    return p.get();
}

} // namespace WTF

using WTF::OwnPtrBrew;

#endif // OwnPtrBrew_h
