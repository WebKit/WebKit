/*
 * Copyright (C) 2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Jian Li <jianli@chromium.org>
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Thread local storage is implemented by using either pthread API or Windows
 * native API. There is subtle semantic discrepancy for the cleanup function
 * implementation as noted below:
 *   @ In pthread implementation, the destructor function will be called
 *     repeatedly if there is still non-NULL value associated with the function.
 *   @ In Windows native implementation, the destructor function will be called
 *     only once.
 * This semantic discrepancy does not impose any problem because nowhere in
 * WebKit the repeated call bahavior is utilized.
 */

#pragma once

#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>

namespace WTF {

enum class CanBeGCThread {
    False,
    True
};

template<typename T, CanBeGCThread canBeGCThread = CanBeGCThread::False> class ThreadSpecific {
    WTF_MAKE_NONCOPYABLE(ThreadSpecific);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadSpecific();
    bool isSet(); // Useful as a fast check to see if this thread has set this value.
    T* operator->();
    operator T*();
    T& operator*();

private:
    // Not implemented. It's technically possible to destroy a thread specific key, but one would need
    // to make sure that all values have been destroyed already (usually, that all threads that used it
    // have exited). It's unlikely that any user of this call will be in that situation - and having
    // a destructor defined can be confusing, given that it has such strong pre-requisites to work correctly.
    ~ThreadSpecific();

    struct Data {
        WTF_MAKE_NONCOPYABLE(Data);
        WTF_MAKE_FAST_ALLOCATED;
    public:
        using PointerType = typename std::remove_const<T>::type*;

        Data(ThreadSpecific<T, canBeGCThread>* owner)
            : owner(owner)
        {
            // Set up thread-specific value's memory pointer before invoking constructor, in case any function it calls
            // needs to access the value, to avoid recursion.
            owner->setInTLS(this);
            new (NotNull, storagePointer()) T();
        }

        ~Data()
        {
            storagePointer()->~T();
            owner->setInTLS(nullptr);
        }

        PointerType storagePointer() const { return const_cast<PointerType>(reinterpret_cast<const T*>(&m_storage)); }

        typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type m_storage;
        ThreadSpecific<T, canBeGCThread>* owner;
    };

    T* get();
    T* set();
    void setInTLS(Data*);
    void static destroy(void* ptr);

#if USE(PTHREADS)
    pthread_key_t m_key { };
#elif OS(WINDOWS)
    int m_key;
#endif
};

#if USE(PTHREADS)

template<typename T, CanBeGCThread canBeGCThread>
inline ThreadSpecific<T, canBeGCThread>::ThreadSpecific()
{
    int error = pthread_key_create(&m_key, destroy);
    if (error)
        CRASH();
}

template<typename T, CanBeGCThread canBeGCThread>
inline T* ThreadSpecific<T, canBeGCThread>::get()
{
    Data* data = static_cast<Data*>(pthread_getspecific(m_key));
    if (data)
        return data->storagePointer();
    return nullptr;
}

template<typename T, CanBeGCThread canBeGCThread>
inline void ThreadSpecific<T, canBeGCThread>::setInTLS(Data* data)
{
    pthread_setspecific(m_key, data);
}

#elif OS(WINDOWS)

template<typename T, CanBeGCThread canBeGCThread>
inline ThreadSpecific<T, canBeGCThread>::ThreadSpecific()
    : m_key(-1)
{
    bool ok = Thread::SpecificStorage::allocateKey(m_key, destroy);
    if (!ok)
        CRASH();
}

template<typename T, CanBeGCThread canBeGCThread>
inline T* ThreadSpecific<T, canBeGCThread>::get()
{
    auto data = static_cast<Data*>(Thread::current().specificStorage().get(m_key));
    if (!data)
        return nullptr;
    return data->storagePointer();
}

template<typename T, CanBeGCThread canBeGCThread>
inline void ThreadSpecific<T, canBeGCThread>::setInTLS(Data* data)
{
    return Thread::current().specificStorage().set(m_key, data);
}

#else
#error ThreadSpecific is not implemented for this platform.
#endif

template<typename T, CanBeGCThread canBeGCThread>
inline void ThreadSpecific<T, canBeGCThread>::destroy(void* ptr)
{
    Data* data = static_cast<Data*>(ptr);

#if USE(PTHREADS)
    // We want get() to keep working while data destructor works, because it can be called indirectly by the destructor.
    // Some pthreads implementations zero out the pointer before calling destroy(), so we temporarily reset it.
    pthread_setspecific(data->owner->m_key, ptr);
#endif

    delete data;
}

template<typename T, CanBeGCThread canBeGCThread>
inline T* ThreadSpecific<T, canBeGCThread>::set()
{
    RELEASE_ASSERT(canBeGCThread == CanBeGCThread::True || !Thread::mayBeGCThread());
    ASSERT(!get());
    Data* data = new Data(this); // Data will set itself into TLS.
    ASSERT(get() == data->storagePointer());
    return data->storagePointer();
}

template<typename T, CanBeGCThread canBeGCThread>
inline bool ThreadSpecific<T, canBeGCThread>::isSet()
{
    return !!get();
}

template<typename T, CanBeGCThread canBeGCThread>
inline ThreadSpecific<T, canBeGCThread>::operator T*()
{
    if (T* ptr = get())
        return ptr;
    return set();
}

template<typename T, CanBeGCThread canBeGCThread>
inline T* ThreadSpecific<T, canBeGCThread>::operator->()
{
    return operator T*();
}

template<typename T, CanBeGCThread canBeGCThread>
inline T& ThreadSpecific<T, canBeGCThread>::operator*()
{
    return *operator T*();
}

} // namespace WTF

using WTF::ThreadSpecific;
