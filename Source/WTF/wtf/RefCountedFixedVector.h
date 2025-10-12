/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include <type_traits>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TrailingArray.h>

namespace WTF {

template<typename T, bool isThreadSafe>
class RefCountedFixedVectorBase final : public std::conditional<isThreadSafe, ThreadSafeRefCounted<RefCountedFixedVectorBase<T, isThreadSafe>>, RefCounted<RefCountedFixedVectorBase<T, isThreadSafe>>>::type, public TrailingArray<RefCountedFixedVectorBase<T, isThreadSafe>, T> {
public:
    using Base = TrailingArray<RefCountedFixedVectorBase<T, isThreadSafe>, T>;

    static Ref<RefCountedFixedVectorBase> create(unsigned size)
    {
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size));
    }

    static Ref<RefCountedFixedVectorBase> create(std::initializer_list<T> initializerList)
    {
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(initializerList.size()))) RefCountedFixedVectorBase(initializerList));
    }

    template<typename InputIterator>
    static Ref<RefCountedFixedVectorBase> create(InputIterator first, InputIterator last)
    {
        unsigned size = Checked<uint32_t> { std::distance(first, last) };
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, first, last));
    }

    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    static Ref<RefCountedFixedVectorBase> createFromVector(const Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>& other)
    {
        unsigned size = Checked<uint32_t> { other.size() }.value();
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, std::begin(other), std::end(other)));
    }

    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    static Ref<RefCountedFixedVectorBase> createFromVector(Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>&& other)
    {
        auto container = WTFMove(other);
        unsigned size = Checked<uint32_t> { container.size() }.value();
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, std::move_iterator { container.begin() }, std::move_iterator { container.end() }));
    }

    template<std::invocable<size_t> Generator>
    static Ref<RefCountedFixedVectorBase> createWithSizeFromGenerator(unsigned size, NOESCAPE Generator&& generator)
    {
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, std::forward<Generator>(generator)));
    }

    template<std::invocable<size_t> FailableGenerator>
    static RefPtr<RefCountedFixedVectorBase> createWithSizeFromFailableGenerator(unsigned size, NOESCAPE FailableGenerator&& generator)
    {
        auto result = adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(typename Base::Failable { }, size, std::forward<FailableGenerator>(generator)));
        if (result->size() != size)
            return nullptr;
        return result;
    }

    template<typename SizedRange, typename Mapper>
    static Ref<RefCountedFixedVectorBase> map(unsigned size, SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper)));
    }

    Ref<RefCountedFixedVectorBase> clone() const
    {
        return create(Base::begin(), Base::end());
    }

private:
    explicit RefCountedFixedVectorBase(unsigned size)
        : Base(size)
    {
    }

    explicit RefCountedFixedVectorBase(std::initializer_list<T> initializerList)
        : Base(initializerList)
    {
    }

    template<typename InputIterator>
    RefCountedFixedVectorBase(unsigned size, InputIterator first, InputIterator last)
        : Base(size, first, last)
    {
    }

    template<std::invocable<size_t> Generator>
    RefCountedFixedVectorBase(unsigned size, NOESCAPE Generator&& generator)
        : Base(size, std::forward<Generator>(generator))
    {
    }

    template<std::invocable<size_t> FailableGenerator>
    RefCountedFixedVectorBase(typename Base::Failable failable, unsigned size, NOESCAPE FailableGenerator&& generator)
        : Base(failable, size, std::forward<FailableGenerator>(generator))
    {
    }

    template<typename SizedRange, typename Mapper>
    RefCountedFixedVectorBase(unsigned size, SizedRange&& range, NOESCAPE Mapper&& mapper)
        : Base(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper))
    {
    }
};

template<typename T, bool isThreadSafe, typename U>
inline bool operator==(const RefCountedFixedVectorBase<T, isThreadSafe>& a, const U& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a.at(i) != b.at(i))
            return false;
    }
    return true;
}

template<typename T>
using RefCountedFixedVector = RefCountedFixedVectorBase<T, false>;
template<typename T>
using ThreadSafeRefCountedFixedVector = RefCountedFixedVectorBase<T, true>;

} // namespace WTF

using WTF::RefCountedFixedVector;
using WTF::ThreadSafeRefCountedFixedVector;
