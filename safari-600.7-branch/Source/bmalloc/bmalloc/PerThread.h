/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PerThread_h
#define PerThread_h

#include "Inline.h"
#include <mutex>
#include <pthread.h>
#if defined(__has_include) && __has_include(<System/pthread_machdep.h>)
#include <System/pthread_machdep.h>
#endif

namespace bmalloc {

// Usage:
//     Object* object = PerThread<Object>::get();

template<typename T>
class PerThread {
public:
    static T* get();
    static T* getFastCase();
    static T* getSlowCase();

private:
    static void destructor(void*);
};

class Cache;

template<typename T> struct PerThreadStorage;

#if defined(__has_include) && __has_include(<System/pthread_machdep.h>)

// For now, we only support PerThread<Cache>. We can expand to other types by
// using more keys.

template<> struct PerThreadStorage<Cache> {
    static const pthread_key_t key = __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY0;
    static void* get() { return _pthread_getspecific_direct(key); }
    static void init(void* object, void (*destructor)(void*))
    {
        _pthread_setspecific_direct(key, object);
        pthread_key_init_np(key, destructor);
    }
};

#else

template<typename T> struct PerThreadStorage {
    static __thread void* object;
    static pthread_key_t key;
    static std::once_flag onceFlag;

    static void* get() { return object; }
    static void init(void* object, void (*destructor)(void*))
    {
        std::call_once(onceFlag, [destructor]() {
            pthread_key_create(&key, destructor);
        });
        pthread_setspecific(key, object);
        PerThreadStorage<Cache>::object = object;
    }
};

template<typename T> __thread void* PerThreadStorage<T>::object;
template<typename T> pthread_key_t PerThreadStorage<T>::key;
template<typename T> std::once_flag PerThreadStorage<T>::onceFlag;

#endif // defined(__has_include) && __has_include(<System/pthread_machdep.h>)

template<typename T>
INLINE T* PerThread<T>::getFastCase()
{
    return static_cast<T*>(PerThreadStorage<T>::get());
}

template<typename T>
inline T* PerThread<T>::get()
{
    T* t = getFastCase();
    if (!t)
        return getSlowCase();
    return t;
}

template<typename T>
void PerThread<T>::destructor(void* p)
{
    T* t = static_cast<T*>(p);
    delete t;
}

template<typename T>
T* PerThread<T>::getSlowCase()
{
    BASSERT(!getFastCase());
    T* t = new T;
    PerThreadStorage<T>::init(t, destructor);
    return t;
}

} // namespace bmalloc

#endif // PerThread_h
