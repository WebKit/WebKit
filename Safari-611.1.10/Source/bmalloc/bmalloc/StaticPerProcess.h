/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "BExport.h"
#include "BInline.h"
#include "Mutex.h"
#include "Sizes.h"

namespace bmalloc {

// StaticPerProcess<T> behaves like PerProcess<T>, but we need to explicitly define storage for T with EXTERN.
// In this way, we allocate storage for a per-process object statically instead of allocating memory at runtime.
// To enforce this, we have DECLARE and DEFINE macros. If you do not know about T of StaticPerProcess<T>, you should use PerProcess<T> instead.
//
// Usage:
//     In Object.h
//         class Object : public StaticPerProcess<Object> {
//             ...
//         };
//         DECLARE_STATIC_PER_PROCESS_STORAGE(Object);
//
//     In Object.cpp
//         DEFINE_STATIC_PER_PROCESS_STORAGE(Object);
//
//     Object* object = Object::get();
//     x = object->field->field;
//
// Object will be instantiated only once, even in the presence of concurrency.
//
template<typename T> struct StaticPerProcessStorageTraits;

template<typename T>
class StaticPerProcess {
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
        using Storage = typename StaticPerProcessStorageTraits<T>::Storage;
        return (Storage::s_object).load(std::memory_order_relaxed);
    }

    static Mutex& mutex()
    {
        using Storage = typename StaticPerProcessStorageTraits<T>::Storage;
        return Storage::s_mutex;
    }

private:
    BNO_INLINE static T* getSlowCase()
    {
        using Storage = typename StaticPerProcessStorageTraits<T>::Storage;
        LockHolder lock(Storage::s_mutex);
        if (!Storage::s_object.load(std::memory_order_consume)) {
            T* t = new (&Storage::s_memory) T(lock);
            Storage::s_object.store(t, std::memory_order_release);
        }
        return Storage::s_object.load(std::memory_order_consume);
    }
};

#define DECLARE_STATIC_PER_PROCESS_STORAGE(Type) \
template<> struct StaticPerProcessStorageTraits<Type> { \
    using Memory = typename std::aligned_storage<sizeof(Type), std::alignment_of<Type>::value>::type; \
    struct BEXPORT Storage { \
        static std::atomic<Type*> s_object; \
        static Mutex s_mutex; \
        static Memory s_memory; \
    }; \
};

#define DEFINE_STATIC_PER_PROCESS_STORAGE(Type) \
    std::atomic<Type*> StaticPerProcessStorageTraits<Type>::Storage::s_object { nullptr }; \
    Mutex StaticPerProcessStorageTraits<Type>::Storage::s_mutex { }; \
    StaticPerProcessStorageTraits<Type>::Memory StaticPerProcessStorageTraits<Type>::Storage::s_memory { };

} // namespace bmalloc
