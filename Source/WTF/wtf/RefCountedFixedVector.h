/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

    template<typename InputIterator>
    static Ref<RefCountedFixedVectorBase> create(InputIterator first, InputIterator last)
    {
        unsigned size = Checked<uint32_t> { std::distance(first, last) };
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, first, last));
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    static Ref<RefCountedFixedVectorBase> createFromVector(const Vector<T, inlineCapacity, OverflowHandler>& other)
    {
        unsigned size = Checked<uint32_t> { other.size() }.value();
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, std::begin(other), std::end(other)));
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    static Ref<RefCountedFixedVectorBase> createFromVector(Vector<T, inlineCapacity, OverflowHandler>&& other)
    {
        Vector<T, inlineCapacity, OverflowHandler> container = WTFMove(other);
        unsigned size = Checked<uint32_t> { container.size() }.value();
        return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) RefCountedFixedVectorBase(size, std::move_iterator { container.begin() }, std::move_iterator { container.end() }));
    }

    Ref<RefCountedFixedVectorBase> clone() const
    {
        return create(Base::begin(), Base::end());
    }

    bool operator==(const RefCountedFixedVectorBase& other) const
    {
        if (Base::size() != other.size())
            return false;
        for (unsigned i = 0; i < Base::size(); ++i) {
            if (Base::at(i) != other.at(i))
                return false;
        }
        return true;
    }

private:
    explicit RefCountedFixedVectorBase(unsigned size)
        : Base(size)
    {
    }

    template<typename InputIterator>
    RefCountedFixedVectorBase(unsigned size, InputIterator first, InputIterator last)
        : Base(size, first, last)
    {
    }
};

template<typename T>
using RefCountedFixedVector = RefCountedFixedVectorBase<T, false>;
template<typename T>
using ThreadSafeRefCountedFixedVector = RefCountedFixedVectorBase<T, true>;

} // namespace WTF

using WTF::RefCountedFixedVector;
using WTF::ThreadSafeRefCountedFixedVector;
