#ifndef _DEUNIQUEPTR_HPP
#define _DEUNIQUEPTR_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Unique pointer.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

namespace de
{

//! Unique pointer self-test.
void UniquePtr_selfTest(void);

// Hide implementation-private types in a details namespace.
namespace details
{

//! Auxiliary struct used to pass references between unique pointers. To
//! ensure that managed pointers are deleted exactly once, this type should
//! not appear in user code.
template <typename T, class D>
struct PtrData
{
    PtrData(T *p, D d) : ptr(p), deleter(d)
    {
    }

    template <typename T2, class D2>
    PtrData(const PtrData<T2, D2> &d) : ptr(d.ptr)
                                      , deleter(d.deleter)
    {
    }

    T *ptr;
    D deleter;
};

template <typename T, class D>
class UniqueBase
{
public:
    typedef T element_type;
    typedef D deleter_type;

    T *get(void) const throw()
    {
        return m_data.ptr;
    } //!< Get stored pointer.
    D getDeleter(void) const throw()
    {
        return m_data.deleter;
    }
    T *operator->(void) const throw()
    {
        return get();
    } //!< Get stored pointer.
    T &operator*(void) const throw()
    {
        return *get();
    } //!< De-reference stored pointer.
    operator bool(void) const throw()
    {
        return !!get();
    }

protected:
    UniqueBase(T *ptr, D deleter) : m_data(ptr, deleter)
    {
    }
    UniqueBase(PtrData<T, D> data) : m_data(data)
    {
    }
    ~UniqueBase(void);

    void reset(void);                        //!< Delete previous pointer, set to null.
    PtrData<T, D> releaseData(void) throw(); //!< Relinquish ownership, return pointer data.
    void assignData(PtrData<T, D> data);     //!< Set new pointer, delete previous pointer.

private:
    PtrData<T, D> m_data;
};

template <typename T, class D>
UniqueBase<T, D>::~UniqueBase(void)
{
    reset();
}

template <typename T, class D>
void UniqueBase<T, D>::reset(void)
{
    if (m_data.ptr != nullptr)
    {
        m_data.deleter(m_data.ptr);
        m_data.ptr = nullptr;
    }
}

template <typename T, class D>
PtrData<T, D> UniqueBase<T, D>::releaseData(void) throw()
{
    PtrData<T, D> data = m_data;
    m_data.ptr         = nullptr;
    return data;
}

template <typename T, class D>
void UniqueBase<T, D>::assignData(PtrData<T, D> data)
{
    if (data.ptr != m_data.ptr)
    {
        reset();
        m_data = data;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Movable unique pointer
 *
 * A MovePtr is smart pointer that retains sole ownership of a pointer and
 * destroys it when it is destroyed (for example when it goes out of scope).
 *
 * A MovePtr can be copied and assigned to. The pointer ownership is moved to
 * the newly constructer or assigned-to MovePtr. Upon assignment to a
 * MovePtr, the previously managed pointer is deleted.
 *
 *//*--------------------------------------------------------------------*/
template <typename T, class Deleter = DefaultDeleter<T>>
class MovePtr : public UniqueBase<T, Deleter>
{
public:
    MovePtr(void) : UniqueBase<T, Deleter>(nullptr, Deleter())
    {
    }
    explicit MovePtr(T *ptr, Deleter deleter = Deleter()) : UniqueBase<T, Deleter>(ptr, deleter)
    {
    }
    MovePtr(MovePtr<T, Deleter> &other) : UniqueBase<T, Deleter>(other.releaseData())
    {
    }

    MovePtr &operator=(MovePtr<T, Deleter> &other);
    T *release(void) throw();
    void clear(void)
    {
        this->reset();
    }

    // These implicit by-value conversions to and from a PtrData are used to
    // allow copying a MovePtr by value when returning from a function. To
    // ensure that the managed pointer gets deleted exactly once, the PtrData
    // should only exist as a temporary conversion step between two MovePtrs.
    MovePtr(PtrData<T, Deleter> data) : UniqueBase<T, Deleter>(data)
    {
    }
    MovePtr &operator=(PtrData<T, Deleter> data);

    template <typename U, class Del2>
    operator PtrData<U, Del2>(void)
    {
        return this->releaseData();
    }
};

template <typename T, class D>
MovePtr<T, D> &MovePtr<T, D>::operator=(PtrData<T, D> data)
{
    this->assignData(data);
    return *this;
}

template <typename T, class D>
MovePtr<T, D> &MovePtr<T, D>::operator=(MovePtr<T, D> &other)
{
    return (*this = other.releaseData());
}

//! Steal the managed pointer. The caller is responsible for explicitly
//! deleting the returned pointer.
template <typename T, class D>
inline T *MovePtr<T, D>::release(void) throw()
{
    return this->releaseData().ptr;
}

//! Construct a MovePtr from a pointer.
template <typename T>
inline MovePtr<T> movePtr(T *ptr)
{
    return MovePtr<T>(ptr);
}

//! Allocate and construct an object and return its address as a MovePtr.
template <typename T>
inline MovePtr<T> newMovePtr(void)
{
    return MovePtr<T>(new T());
}
template <typename T, typename P0>
inline MovePtr<T> newMovePtr(P0 p0)
{
    return MovePtr<T>(new T(p0));
}
template <typename T, typename P0, typename P1>
inline MovePtr<T> newMovePtr(P0 p0, P1 p1)
{
    return MovePtr<T>(new T(p0, p1));
}
template <typename T, typename P0, typename P1, typename P2>
inline MovePtr<T> newMovePtr(P0 p0, P1 p1, P2 p2)
{
    return MovePtr<T>(new T(p0, p1, p2));
}

/*--------------------------------------------------------------------*//*!
 * \brief Unique pointer
 *
 * UniquePtr is smart pointer that retains sole ownership of a pointer
 * and destroys it when UniquePtr is destroyed (for example when UniquePtr
 * goes out of scope).
 *
 * UniquePtr is not copyable or assignable. Pointer ownership can be transferred
 * from a UniquePtr only explicitly with the move() member function.
 *
 * A UniquePtr can be constructed from a MovePtr. In this case it assumes
 * ownership of the pointer from the MovePtr. Because a UniquePtr cannot be
 * copied, direct initialization syntax must be used, i.e.:
 *
 *        MovePtr<Foo> createFoo (void);
 *        UniquePtr<Foo> fooPtr(createFoo()); // NOT fooPtr = createFoo();
 *
 *//*--------------------------------------------------------------------*/
template <typename T, class Deleter = DefaultDeleter<T>>
class UniquePtr : public UniqueBase<T, Deleter>
{
public:
    explicit UniquePtr(T *ptr, Deleter deleter = Deleter());
    UniquePtr(PtrData<T, Deleter> data);
    MovePtr<T, Deleter> move(void);

private:
    UniquePtr(const UniquePtr<T> &other);           // Not allowed!
    UniquePtr operator=(const UniquePtr<T> &other); // Not allowed!
};

/*--------------------------------------------------------------------*//*!
 * \brief Construct unique pointer.
 * \param ptr Pointer to be managed.
 *
 * Pointer ownership is transferred to the UniquePtr.
 *//*--------------------------------------------------------------------*/
template <typename T, class Deleter>
inline UniquePtr<T, Deleter>::UniquePtr(T *ptr, Deleter deleter) : UniqueBase<T, Deleter>(ptr, deleter)
{
}

template <typename T, class Deleter>
inline UniquePtr<T, Deleter>::UniquePtr(PtrData<T, Deleter> data) : UniqueBase<T, Deleter>(data)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Relinquish ownership of pointer.
 *
 * This method returns a MovePtr that now owns the pointer. The pointer in
 * the UniquePtr is set to null.
 *//*--------------------------------------------------------------------*/
template <typename T, class Deleter>
inline MovePtr<T, Deleter> UniquePtr<T, Deleter>::move(void)
{
    return MovePtr<T, Deleter>(this->releaseData());
}

} // namespace details

using details::MovePtr;
using details::newMovePtr;
using details::UniquePtr;

} // namespace de

#endif // _DEUNIQUEPTR_HPP
