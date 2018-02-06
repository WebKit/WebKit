/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "BInline.h"
#include "Sizes.h"
#include "StaticMutex.h"
#include <mutex>

namespace bmalloc {

// Usage:
//     Object* object = PerProcess<Object>::get();
//     x = object->field->field;
//
// Object will be instantiated only once, even in the face of concurrency.
//
// WARNING: PerProcess<T> does not export its storage. So in actuality when
// used in multiple libraries / images it ends up being per-image. To truly
// declare a per-process singleton use SafePerProcess<T>.
//
// NOTE: If you observe global side-effects of the Object constructor, be
// sure to lock the Object mutex. For example:
//
// Object() : m_field(...) { globalFlag = true }
//
// Object* object = PerProcess<Object>::get();
// x = object->m_field; // OK
// if (globalFlag) { ... } // Undefined behavior.
//
// std::lock_guard<StaticMutex> lock(PerProcess<Object>::mutex());
// Object* object = PerProcess<Object>::get(lock);
// if (globalFlag) { ... } // OK.

template<typename T>
class PerProcess {
public:
    static T* get()
    {
        T* object = getFastCase();
        if (!object)
            return getSlowCase();
        return object;
    }

    static T* getFastCase()
    {
        return s_object.load(std::memory_order_relaxed);
    }
    
    static StaticMutex& mutex() { return s_mutex; }

private:
    BNO_INLINE static T* getSlowCase()
    {
        std::lock_guard<StaticMutex> lock(s_mutex);
        if (!s_object.load(std::memory_order_consume)) {
            T* t = new (&s_memory) T(lock);
            s_object.store(t, std::memory_order_release);
        }
        return s_object.load(std::memory_order_consume);
    }

    static std::atomic<T*> s_object;
    static StaticMutex s_mutex;

    typedef typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type Memory;
    static Memory s_memory;
};

template<typename T>
std::atomic<T*> PerProcess<T>::s_object;

template<typename T>
StaticMutex PerProcess<T>::s_mutex;

template<typename T>
typename PerProcess<T>::Memory PerProcess<T>::s_memory;


// SafePerProcess<T> behaves like PerProcess<T>, but its usage
// requires DECLARE/DEFINE macros that export symbols that allow for
// a single shared instance is across images in the process.

template<typename T> struct SafePerProcessStorageTraits;

template<typename T>
class BEXPORT SafePerProcess {
public:
    using Storage = typename SafePerProcessStorageTraits<T>::Storage;

    static T* get()
    {
        T* object = getFastCase();
        if (!object)
            return getSlowCase();
        return object;
    }

    static T* getFastCase()
    {
        return (Storage::s_object).load(std::memory_order_relaxed);
    }

    static StaticMutex& mutex() { return Storage::s_mutex; }

    using Memory = typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type;

private:
    BNO_INLINE static T* getSlowCase()
    {
        std::lock_guard<StaticMutex> lock(Storage::s_mutex);
        if (!Storage::s_object.load(std::memory_order_consume)) {
            T* t = new (&Storage::s_memory) T(lock);
            Storage::s_object.store(t, std::memory_order_release);
        }
        return Storage::s_object.load(std::memory_order_consume);
    }
};

#define DECLARE_SAFE_PER_PROCESS_STORAGE(Type) \
    template<> struct SafePerProcessStorageTraits<Type> { \
        struct BEXPORT Storage { \
            BEXPORT static std::atomic<Type*> s_object; \
            BEXPORT static StaticMutex s_mutex; \
            BEXPORT static SafePerProcess<Type>::Memory s_memory; \
        }; \
    };

#define DEFINE_SAFE_PER_PROCESS_STORAGE(Type) \
    std::atomic<Type*> SafePerProcessStorageTraits<Type>::Storage::s_object; \
    StaticMutex SafePerProcessStorageTraits<Type>::Storage::s_mutex; \
    SafePerProcess<Type>::Memory SafePerProcessStorageTraits<Type>::Storage::s_memory;

} // namespace bmalloc
