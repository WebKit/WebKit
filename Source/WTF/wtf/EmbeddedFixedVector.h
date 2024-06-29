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

#include <iterator>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/TrailingArray.h>
#include <wtf/UniqueRef.h>

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(EmbeddedFixedVector);

template<typename T>
class EmbeddedFixedVector final : public TrailingArray<EmbeddedFixedVector<T>, T> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(EmbeddedFixedVector);
    WTF_MAKE_NONCOPYABLE(EmbeddedFixedVector);
    WTF_MAKE_NONMOVABLE(EmbeddedFixedVector);
public:
    using Base = TrailingArray<EmbeddedFixedVector<T>, T>;

    static UniqueRef<EmbeddedFixedVector> create(unsigned size)
    {
        return UniqueRef { *new (NotNull, EmbeddedFixedVectorMalloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size) };
    }

    template<typename InputIterator>
    static UniqueRef<EmbeddedFixedVector> create(InputIterator first, InputIterator last)
    {
        unsigned size = Checked<uint32_t> { std::distance(first, last) };
        return UniqueRef { *new (NotNull, EmbeddedFixedVectorMalloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, first, last) };
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    static UniqueRef<EmbeddedFixedVector> createFromVector(const Vector<T, inlineCapacity, OverflowHandler>& other)
    {
        unsigned size = Checked<uint32_t> { other.size() }.value();
        return UniqueRef { *new (NotNull, EmbeddedFixedVectorMalloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, other.begin(), other.end()) };
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    static UniqueRef<EmbeddedFixedVector> createFromVector(Vector<T, inlineCapacity, OverflowHandler>&& other)
    {
        Vector<T, inlineCapacity, OverflowHandler> container = WTFMove(other);
        unsigned size = Checked<uint32_t> { container.size() }.value();
        return UniqueRef { *new (NotNull, EmbeddedFixedVectorMalloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, std::move_iterator { container.begin() }, std::move_iterator { container.end() }) };
    }

    template<typename... Args>
    static UniqueRef<EmbeddedFixedVector> createWithSizeAndConstructorArguments(unsigned size, Args&&... args)
    {
        return UniqueRef { *new (NotNull, EmbeddedFixedVectorMalloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(size, std::forward<Args>(args)...) };
    }

    template<std::invocable<size_t> Generator>
    static std::unique_ptr<EmbeddedFixedVector> createWithSizeFromGenerator(unsigned size, Generator&& generator)
    {

        auto result = std::unique_ptr<EmbeddedFixedVector> { new (NotNull, EmbeddedFixedVectorMalloc::malloc(Base::allocationSize(size))) EmbeddedFixedVector(typename Base::Failable { }, size, std::forward<Generator>(generator)) };
        if (result->size() != size)
            return nullptr;
        return result;
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

    template<std::invocable<size_t> FailableGenerator>
    EmbeddedFixedVector(typename Base::Failable failable, unsigned size, FailableGenerator&& generator)
        : Base(failable, size, std::forward<FailableGenerator>(generator))
    {
    }
};

} // namespace WTF

using WTF::EmbeddedFixedVector;
