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

#include <iterator>
#include <memory>
#include <wtf/MallocCommon.h>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/SequesteredMalloc.h>
#include <wtf/TrailingArray.h>
#include <wtf/UniqueRef.h>

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(EmbeddedFixedVector);

template<typename T, typename Malloc = EmbeddedFixedVectorMalloc>
class EmbeddedFixedVector final : public TrailingArray<EmbeddedFixedVector<T, Malloc>, T> {
    WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(EmbeddedFixedVector, Malloc);
    WTF_MAKE_NONCOPYABLE(EmbeddedFixedVector);
    WTF_MAKE_NONMOVABLE(EmbeddedFixedVector);
public:
    using Base = TrailingArray<EmbeddedFixedVector<T, Malloc>, T>;

    static UniqueRef<EmbeddedFixedVector> create(unsigned size)
    {
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size) };
    }

    static UniqueRef<EmbeddedFixedVector> create(std::initializer_list<T> initializerList)
    {
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(initializerList.size()))) EmbeddedFixedVector(initializerList) };
    }

    template<typename U, size_t Extent>
    static UniqueRef<EmbeddedFixedVector> create(std::span<U, Extent> span)
    {
        unsigned size = Checked<uint32_t> { span.size() };
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(span) };
    }

    template<typename InputIterator>
    static UniqueRef<EmbeddedFixedVector> create(InputIterator first, InputIterator last)
    {
        unsigned size = Checked<uint32_t> { std::distance(first, last) };
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, first, last) };
    }

    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    static UniqueRef<EmbeddedFixedVector> createFromVector(const Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>& other)
    {
        unsigned size = Checked<uint32_t> { other.size() }.value();
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, other.begin(), other.end()) };
    }

    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    static UniqueRef<EmbeddedFixedVector> createFromVector(Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>&& other)
    {
        auto container = WTFMove(other);
        unsigned size = Checked<uint32_t> { container.size() }.value();
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, std::move_iterator { container.begin() }, std::move_iterator { container.end() }) };
    }

    template<typename... Args>
    static UniqueRef<EmbeddedFixedVector> createWithSizeAndConstructorArguments(unsigned size, Args&&... args)
    {
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, std::forward<Args>(args)...) };
    }

    template<std::invocable<size_t> Generator>
    static UniqueRef<EmbeddedFixedVector> createWithSizeFromGenerator(unsigned size, NOESCAPE Generator&& generator)
    {
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, std::forward<Generator>(generator)) };
    }

    template<std::invocable<size_t> FailableGenerator>
    static std::unique_ptr<EmbeddedFixedVector> createWithSizeFromFailableGenerator(unsigned size, NOESCAPE FailableGenerator&& generator)
    {
        auto result = std::unique_ptr<EmbeddedFixedVector> { new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(typename Base::Failable { }, size, std::forward<FailableGenerator>(generator)) };
        if (result->size() != size)
            return nullptr;
        return result;
    }

    template<typename SizedRange, typename Mapper>
    static UniqueRef<EmbeddedFixedVector> map(unsigned size, SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        return UniqueRef { *new (NotNull, Malloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper)) };
    }

    UniqueRef<EmbeddedFixedVector> clone() const
    {
        return create(Base::begin(), Base::end());
    }

    bool operator==(const EmbeddedFixedVector& other) const
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
    explicit EmbeddedFixedVector(unsigned size)
        : Base(size)
    {
    }

    EmbeddedFixedVector(std::initializer_list<T> initializerList)
        : Base(initializerList)
    {
    }

    template<typename U, size_t Extent>
    EmbeddedFixedVector(std::span<U, Extent> span)
        : Base(span)
    {
    }

    template<typename InputIterator>
    EmbeddedFixedVector(unsigned size, InputIterator first, InputIterator last)
        : Base(size, first, last)
    {
    }

    template<typename... Args>
    explicit EmbeddedFixedVector(unsigned size, Args&&... args) // create with given size and constructor arguments for all elements
        : Base(size, std::forward<Args>(args)...)
    {
    }

    template<std::invocable<size_t> Generator>
    EmbeddedFixedVector(unsigned size, NOESCAPE Generator&& generator)
        : Base(size, std::forward<Generator>(generator))
    {
    }

    template<std::invocable<size_t> FailableGenerator>
    EmbeddedFixedVector(typename Base::Failable failable, unsigned size, NOESCAPE FailableGenerator&& generator)
        : Base(failable, size, std::forward<FailableGenerator>(generator))
    {
    }

    template<typename SizedRange, typename Mapper>
    EmbeddedFixedVector(unsigned size, SizedRange&& range, NOESCAPE Mapper&& mapper)
        : Base(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper))
    {
    }
};

} // namespace WTF

using WTF::EmbeddedFixedVector;
