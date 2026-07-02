#ifndef _DESHAREDPTR_HPP
#define _DESHAREDPTR_HPP
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
 * \brief Shared pointer.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deAtomic.h"

#include <exception>
#include <algorithm>

namespace de
{

//! Shared pointer self-test.
void SharedPtr_selfTest(void);

class DeadReferenceException : public std::exception
{
public:
    DeadReferenceException(void) throw() : std::exception()
    {
    }

    const char *what(void) const throw()
    {
        return "DeadReferenceException";
    }
};

struct SharedPtrStateBase
{
    SharedPtrStateBase(void) : strongRefCount(0), weakRefCount(0)
    {
    }

    virtual ~SharedPtrStateBase(void) throw()
    {
    }
    virtual void deletePtr(void) throw() = 0;

    volatile int32_t strongRefCount;
    volatile int32_t weakRefCount; //!< WeakPtr references + StrongPtr references.
};

template <typename Type, typename Deleter>
struct SharedPtrState : public SharedPtrStateBase
{
    SharedPtrState(Type *ptr, Deleter deleter) : m_ptr(ptr), m_deleter(deleter)
    {
    }

    virtual ~SharedPtrState(void) throw()
    {
        DE_ASSERT(!m_ptr);
    }

    virtual void deletePtr(void) throw()
    {
        m_deleter(m_ptr);
        m_ptr = nullptr;
    }

private:
    Type *m_ptr;
    Deleter m_deleter;
};

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

/*--------------------------------------------------------------------*//*!
 * \brief Shared pointer
 *
 * SharedPtr is smart pointer for managing shared ownership to a pointer.
 * Multiple SharedPtrs can maintain ownership to the pointer and it is
 * destructed when last SharedPtr is destroyed.
 *
 * SharedPtr can also be NULL.
 *//*--------------------------------------------------------------------*/
template <typename T>
class SharedPtr
{
public:
    SharedPtr(void);
    SharedPtr(const SharedPtr<T> &other);
    explicit SharedPtr(T *ptr);

    template <typename Deleter>
    SharedPtr(T *ptr, Deleter deleter);

    template <typename Y>
    explicit SharedPtr(const SharedPtr<Y> &other);

    template <typename Y>
    explicit SharedPtr(const WeakPtr<Y> &other);

    ~SharedPtr(void);

    template <typename Y>
    SharedPtr &operator=(const SharedPtr<Y> &other);
    SharedPtr &operator=(const SharedPtr<T> &other);

    template <typename Y>
    SharedPtr &operator=(const WeakPtr<Y> &other);

    T *get(void) const throw()
    {
        return m_ptr;
    } //!< Get stored pointer.
    T *operator->(void) const throw()
    {
        return m_ptr;
    } //!< Get stored pointer.
    T &operator*(void) const throw()
    {
        return *m_ptr;
    } //!< De-reference pointer.

    operator bool(void) const throw()
    {
        return !!m_ptr;
    }

    void swap(SharedPtr<T> &other);

    void clear(void);

    template <typename Y>
    operator SharedPtr<Y>(void) const;

private:
    void acquire(void);
    void acquireFromWeak(const WeakPtr<T> &other);
    void release(void);

    T *m_ptr;
    SharedPtrStateBase *m_state;

    friend class WeakPtr<T>;

    template <typename U>
    friend class SharedPtr;
};

/*--------------------------------------------------------------------*//*!
 * \brief Weak pointer
 *
 * WeakPtr manages weak references to objects owned by SharedPtr. Shared
 * pointer can be converted to weak pointer and vice versa. Weak pointer
 * differs from SharedPtr by not affecting the lifetime of the managed
 * object.
 *
 * WeakPtr can be converted back to SharedPtr but that operation can fail
 * if the object is no longer live. In such case DeadReferenceException
 * will be thrown.
 *//*--------------------------------------------------------------------*/
template <typename T>
class WeakPtr
{
public:
    WeakPtr(void);
    WeakPtr(const WeakPtr<T> &other);

    explicit WeakPtr(const SharedPtr<T> &other);
    ~WeakPtr(void);

    WeakPtr &operator=(const WeakPtr<T> &other);
    WeakPtr &operator=(const SharedPtr<T> &other);

    SharedPtr<T> lock(void);

private:
    void acquire(void);
    void release(void);

    T *m_ptr;
    SharedPtrStateBase *m_state;

    friend class SharedPtr<T>;
};

// SharedPtr template implementation.

/*--------------------------------------------------------------------*//*!
 * \brief Construct empty shared pointer.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline SharedPtr<T>::SharedPtr(void) : m_ptr(nullptr)
                                     , m_state(nullptr)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct shared pointer from pointer.
 * \param ptr Pointer to be managed.
 *
 * Ownership of the pointer will be transferred to SharedPtr and future
 * SharedPtr's initialized or assigned from this SharedPtr.
 *
 * If allocation of shared state fails. The "ptr" argument will not be
 * released.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline SharedPtr<T>::SharedPtr(T *ptr) : m_ptr(nullptr)
                                       , m_state(nullptr)
{
    try
    {
        m_ptr                   = ptr;
        m_state                 = new SharedPtrState<T, DefaultDeleter<T>>(ptr, DefaultDeleter<T>());
        m_state->strongRefCount = 1;
        m_state->weakRefCount   = 1;
    }
    catch (...)
    {
        // \note ptr is not released.
        delete m_state;
        throw;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct shared pointer from pointer.
 * \param ptr Pointer to be managed.
 *
 * Ownership of the pointer will be transferred to SharedPtr and future
 * SharedPtr's initialized or assigned from this SharedPtr.
 *
 * Deleter must be callable type and deleter is called with the pointer
 * argument when the reference count becomes 0.
 *
 * If allocation of shared state fails. The "ptr" argument will not be
 * released.
 *
 * Calling deleter or calling destructor for deleter should never throw.
 *//*--------------------------------------------------------------------*/
template <typename T>
template <typename Deleter>
inline SharedPtr<T>::SharedPtr(T *ptr, Deleter deleter) : m_ptr(nullptr)
                                                        , m_state(nullptr)
{
    try
    {
        m_ptr                   = ptr;
        m_state                 = new SharedPtrState<T, Deleter>(ptr, deleter);
        m_state->strongRefCount = 1;
        m_state->weakRefCount   = 1;
    }
    catch (...)
    {
        // \note ptr is not released.
        delete m_state;
        throw;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shared pointer from another SharedPtr.
 * \param other Pointer to be shared.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline SharedPtr<T>::SharedPtr(const SharedPtr<T> &other) : m_ptr(other.m_ptr)
                                                          , m_state(other.m_state)
{
    acquire();
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shared pointer from another SharedPtr.
 * \param other Pointer to be shared.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template <typename T>
template <typename Y>
inline SharedPtr<T>::SharedPtr(const SharedPtr<Y> &other) : m_ptr(other.m_ptr)
                                                          , m_state(other.m_state)
{
    acquire();
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shared pointer from weak reference.
 * \param other Pointer to be shared.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template <typename T>
template <typename Y>
inline SharedPtr<T>::SharedPtr(const WeakPtr<Y> &other) : m_ptr(nullptr)
                                                        , m_state(nullptr)
{
    acquireFromWeak(other);
}

template <typename T>
inline SharedPtr<T>::~SharedPtr(void)
{
    release();
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from other shared pointer.
 * \param other Pointer to be shared.
 * \return Reference to this SharedPtr.
 *
 * Reference to current pointer is released and reference to new pointer is
 * acquired.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template <typename T>
template <typename Y>
inline SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<Y> &other)
{
    if (m_state == other.m_state)
        return *this;

    // Release current reference.
    release();

    // Copy from other and acquire reference.
    m_ptr   = other.m_ptr;
    m_state = other.m_state;

    acquire();

    return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from other shared pointer.
 * \param other Pointer to be shared.
 * \return Reference to this SharedPtr.
 *
 * Reference to current pointer is released and reference to new pointer is
 * acquired.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<T> &other)
{
    if (m_state == other.m_state)
        return *this;

    // Release current reference.
    release();

    // Copy from other and acquire reference.
    m_ptr   = other.m_ptr;
    m_state = other.m_state;

    acquire();

    return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from weak pointer.
 * \param other Weak reference.
 * \return Reference to this SharedPtr.
 *
 * Tries to acquire reference to WeakPtr, releases current reference and
 * holds reference to new pointer.
 *
 * If WeakPtr can't be acquired, throws DeadReferenceException and doesn't
 * release the current reference.
 *
 * If WeakPtr references same pointer as SharedPtr this call will always
 * succeed.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template <typename T>
template <typename Y>
inline SharedPtr<T> &SharedPtr<T>::operator=(const WeakPtr<Y> &other)
{
    if (m_state == other.m_state)
        return *this;

    {
        SharedPtr<T> sharedOther(other);
        *this = other;
    }

    return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Type conversion operator.
 *
 * T* must be convertible to Y*.
 *//*--------------------------------------------------------------------*/
template <class T>
template <typename Y>
inline SharedPtr<T>::operator SharedPtr<Y>(void) const
{
    return SharedPtr<Y>(*this);
}

/*--------------------------------------------------------------------*//*!
 * \brief Compare pointers.
 * \param a A
 * \param b B
 * \return true if A and B point to same object, false otherwise.
 *//*--------------------------------------------------------------------*/
template <class T, class U>
inline bool operator==(const SharedPtr<T> &a, const SharedPtr<U> &b) throw()
{
    return a.get() == b.get();
}

/*--------------------------------------------------------------------*//*!
 * \brief Compare pointers.
 * \param a A
 * \param b B
 * \return true if A and B point to different objects, false otherwise.
 *//*--------------------------------------------------------------------*/
template <class T, class U>
inline bool operator!=(const SharedPtr<T> &a, const SharedPtr<U> &b) throw()
{
    return a.get() != b.get();
}

/** Swap pointer contents. */
template <typename T>
inline void SharedPtr<T>::swap(SharedPtr<T> &other)
{
    using std::swap;
    swap(m_ptr, other.m_ptr);
    swap(m_state, other.m_state);
}

/** Swap operator for SharedPtr's. */
template <typename T>
inline void swap(SharedPtr<T> &a, SharedPtr<T> &b)
{
    a.swap(b);
}

/*--------------------------------------------------------------------*//*!
 * \brief Set pointer to null.
 *
 * clear() removes current reference and sets pointer to null value.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline void SharedPtr<T>::clear(void)
{
    release();
    m_ptr   = nullptr;
    m_state = nullptr;
}

template <typename T>
inline void SharedPtr<T>::acquireFromWeak(const WeakPtr<T> &weakRef)
{
    DE_ASSERT(!m_ptr && !m_state);

    SharedPtrStateBase *state = weakRef.m_state;

    if (!state)
        return; // Empty reference.

    {
        int32_t oldCount, newCount;

        // Do atomic compare and increment.
        do
        {
            oldCount = state->strongRefCount;
            if (oldCount == 0)
                throw DeadReferenceException();
            newCount = oldCount + 1;
        } while (deAtomicCompareExchange32((uint32_t volatile *)&state->strongRefCount, (uint32_t)oldCount,
                                           (uint32_t)newCount) != (uint32_t)oldCount);

        deAtomicIncrement32(&state->weakRefCount);
    }

    m_ptr   = weakRef.m_ptr;
    m_state = state;
}

template <typename T>
inline void SharedPtr<T>::acquire(void)
{
    if (m_state)
    {
        deAtomicIncrement32(&m_state->strongRefCount);
        deAtomicIncrement32(&m_state->weakRefCount);
    }
}

template <typename T>
inline void SharedPtr<T>::release(void)
{
    if (m_state)
    {
        if (deAtomicDecrement32(&m_state->strongRefCount) == 0)
        {
            m_ptr = nullptr;
            m_state->deletePtr();
        }

        if (deAtomicDecrement32(&m_state->weakRefCount) == 0)
        {
            delete m_state;
            m_state = nullptr;
        }
    }
}

// WeakPtr template implementation.

/*--------------------------------------------------------------------*//*!
 * \brief Construct empty weak pointer.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline WeakPtr<T>::WeakPtr(void) : m_ptr(nullptr)
                                 , m_state(nullptr)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct weak pointer from other weak reference.
 * \param other Weak reference.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline WeakPtr<T>::WeakPtr(const WeakPtr<T> &other) : m_ptr(other.m_ptr)
                                                    , m_state(other.m_state)
{
    acquire();
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct weak pointer from shared pointer.
 * \param other Shared pointer.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline WeakPtr<T>::WeakPtr(const SharedPtr<T> &other) : m_ptr(other.m_ptr)
                                                      , m_state(other.m_state)
{
    acquire();
}

template <typename T>
inline WeakPtr<T>::~WeakPtr(void)
{
    release();
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from another weak pointer.
 * \param other Weak reference.
 * \return Reference to this WeakPtr.
 *
 * The current weak reference is removed first and then a new weak reference
 * to the object pointed by other is taken.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline WeakPtr<T> &WeakPtr<T>::operator=(const WeakPtr<T> &other)
{
    if (this == &other)
        return *this;

    release();

    m_ptr   = other.m_ptr;
    m_state = other.m_state;

    acquire();

    return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from shared pointer.
 * \param other Shared pointer.
 * \return Reference to this WeakPtr.
 *
 * The current weak reference is removed first and then a new weak reference
 * to the object pointed by other is taken.
 *//*--------------------------------------------------------------------*/
template <typename T>
inline WeakPtr<T> &WeakPtr<T>::operator=(const SharedPtr<T> &other)
{
    release();

    m_ptr   = other.m_ptr;
    m_state = other.m_state;

    acquire();

    return *this;
}

template <typename T>
inline void WeakPtr<T>::acquire(void)
{
    if (m_state)
        deAtomicIncrement32(&m_state->weakRefCount);
}

template <typename T>
inline void WeakPtr<T>::release(void)
{
    if (m_state)
    {
        if (deAtomicDecrement32(&m_state->weakRefCount) == 0)
        {
            delete m_state;
            m_state = nullptr;
            m_ptr   = nullptr;
        }
    }
}

} // namespace de

#endif // _DESHAREDPTR_HPP
