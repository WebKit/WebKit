/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CheckedArithmetic.h>
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(UniqueArray);
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(UniqueArrayElement);

template<bool isTriviallyDestructible, typename T> struct UniqueArrayMaker;

template<typename T>
struct UniqueArrayFree {
    static_assert(std::is_trivially_destructible<T>::value, "");

    void operator()(T* pointer) const
    {
        UniqueArrayMalloc::free(const_cast<typename std::remove_cv<T>::type*>(pointer));
    }
};

template<typename T>
struct UniqueArrayFree<T[]> {
    static_assert(std::is_trivially_destructible<T>::value, "");

    void operator()(T* pointer) const
    {
        UniqueArrayMalloc::free(const_cast<typename std::remove_cv<T>::type*>(pointer));
    }
};


template<typename T>
struct UniqueArrayMaker<true, T> {
    using ResultType = typename std::unique_ptr<T[], UniqueArrayFree<T[]>>;

    static ResultType make(size_t size)
    {
        // C++ `new T[N]` stores its `N` to somewhere. Otherwise, `delete []` cannot destroy
        // these N elements. But we do not want to increase the size of allocated memory.
        // If it is acceptable, we can just use Vector<T> instead. So this UniqueArray<T> only
        // accepts the type T which has a trivial destructor. This allows us to skip calling
        // destructors for N elements. And this allows UniqueArray<T> not to store its N size.
        static_assert(std::is_trivially_destructible<T>::value, "");

        // Do not use placement new like `new (storage) T[size]()`. `new T[size]()` requires
        // larger storage than the `sizeof(T) * size` storage since it want to store `size`
        // to somewhere.
        T* storage = static_cast<T*>(UniqueArrayMalloc::malloc(Checked<size_t>(sizeof(T)) * size));
        VectorTypeOperations<T>::initialize(storage, storage + size);
        return ResultType(storage);
    }
};

template<typename T>
struct UniqueArrayMaker<false, T> {
    // Since we do not know how to store/retrieve N size to/from allocated memory when calling new [] and delete [],
    // we use new [] and delete [] operators simply. We create UniqueArrayElement container for the type T.
    // UniqueArrayElement has new [] and delete [] operators for FastMalloc. We allocate UniqueArrayElement[] and cast
    // it to T[]. When deleting, the custom deleter casts T[] to UniqueArrayElement[] and deletes it.
    class UniqueArrayElement {
        WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(UniqueArrayElement);
    public:
        struct Deleter {
            void operator()(T* pointer)
            {
                delete [] bitwise_cast<UniqueArrayElement*>(pointer);
            };
        };

        UniqueArrayElement() = default;

        T value { };
    };
    static_assert(sizeof(T) == sizeof(UniqueArrayElement), "");

    using ResultType = typename std::unique_ptr<T[], typename UniqueArrayElement::Deleter>;

    static ResultType make(size_t size)
    {
        return ResultType(bitwise_cast<T*>(new UniqueArrayElement[size]()));
    }
};

template<typename T>
using UniqueArray = typename UniqueArrayMaker<std::is_trivially_destructible<T>::value, T>::ResultType;

template<typename T>
UniqueArray<T> makeUniqueArray(size_t size)
{
    static_assert(std::is_same<typename std::remove_extent<T>::type, T>::value, "");
    return UniqueArrayMaker<std::is_trivially_destructible<T>::value, T>::make(size);
}

} // namespace WTF

using WTF::UniqueArray;
using WTF::makeUniqueArray;
