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
#include "Mutex.h"
#include "Sizes.h"

namespace bmalloc {

// Usage:
//     Object* object = PerProcess<Object>::get();
//     x = object->field->field;
//
// Object will be instantiated only once, even in the face of concurrency.
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
// std::lock_guard<Mutex> lock(PerProcess<Object>::mutex());
// Object* object = PerProcess<Object>::get(lock);
// if (globalFlag) { ... } // OK.

struct PerProcessData {
    const char* disambiguator;
    void* memory;
    size_t size;
    size_t alignment;
    Mutex mutex;
    bool isInitialized;
    PerProcessData* next;
};

constexpr unsigned stringHash(const char* string)
{
    unsigned result = 5381;
    while (char c = *string++)
        result = result * 33 + c;
    return result;
}

BEXPORT PerProcessData* getPerProcessData(unsigned disambiguatorHash, const char* disambiguator, size_t size, size_t alignment);

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
    
    static Mutex& mutex()
    {
        if (!s_data)
            coalesce();
        return s_data->mutex;
    }

private:
    static void coalesce()
    {
        if (s_data)
            return;
        
        const char* disambiguator = __PRETTY_FUNCTION__;
        s_data = getPerProcessData(stringHash(disambiguator), disambiguator, sizeof(T), std::alignment_of<T>::value);
    }
    
    BNO_INLINE static T* getSlowCase()
    {
        std::lock_guard<Mutex> lock(mutex());
        if (!s_object.load()) {
            if (s_data->isInitialized)
                s_object.store(static_cast<T*>(s_data->memory));
            else {
                T* t = new (s_data->memory) T(lock);
                s_object.store(t);
                s_data->isInitialized = true;
            }
        }
        return s_object.load();
    }

    static std::atomic<T*> s_object;
    static PerProcessData* s_data;
};

template<typename T>
std::atomic<T*> PerProcess<T>::s_object { nullptr };

template<typename T>
PerProcessData* PerProcess<T>::s_data { nullptr };

} // namespace bmalloc
