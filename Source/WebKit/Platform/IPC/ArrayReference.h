/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include <limits>
#include <wtf/Vector.h>

namespace IPC {

inline constexpr size_t arrayReferenceDynamicExtent = std::numeric_limits<size_t>::max();

template <typename T, size_t Extent = arrayReferenceDynamicExtent>
class ArrayReference;

template <typename T>
class ArrayReference<T, arrayReferenceDynamicExtent> {
public:
    ArrayReference() = default;

    ArrayReference(const T* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    template <size_t inlineCapacity>
    ArrayReference(const Vector<T, inlineCapacity>& vector)
        : m_data(vector.data())
        , m_size(vector.size())
    {
    }

    bool isEmpty() const { return !m_size; }

    size_t size() const { return m_size; }
    const T* data() const
    {
        if (isEmpty())
            return nullptr;
        return m_data;
    }

    Vector<T> vector() const
    {
        Vector<T> result;
        result.append(m_data, m_size);

        return result;
    }

private:
    const T* m_data { nullptr };
    size_t m_size { 0 };
};

template <typename T, size_t Extent>
class ArrayReference {
public:
    ArrayReference() = default;

    ArrayReference(const T* data, size_t size)
        : m_data(data)
    {
        ASSERT_UNUSED(size, size == Extent);
    }

    template <size_t inlineCapacity>
    ArrayReference(const Vector<T, inlineCapacity>& vector)
        : m_data(vector.data())
    {
        ASSERT(vector.size() == Extent);
    }

    constexpr bool isEmpty() const { return !Extent; }

    constexpr size_t size() const { return Extent; }

    const T* data() const
    {
        if (isEmpty())
            return nullptr;
        return m_data;
    }

    Vector<T> vector() const
    {
        return { m_data, Extent };
    }

private:
    const T* m_data { nullptr };
};

template<typename T, size_t inlineCapacity>
ArrayReference(const WTF::Vector<T, inlineCapacity>&) -> ArrayReference<T>;

} // namespace IPC
