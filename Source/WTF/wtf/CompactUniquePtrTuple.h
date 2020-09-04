/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <memory>
#include <wtf/CompactPointerTuple.h>
#include <wtf/Noncopyable.h>

namespace WTF {

template<typename T, typename Type, typename Deleter = std::default_delete<T>> class CompactUniquePtrTuple;

template<typename T, typename Type, typename... Args>
ALWAYS_INLINE CompactUniquePtrTuple<T, Type> makeCompactUniquePtr(Args&&... args)
{
    return CompactUniquePtrTuple<T, Type>(makeUnique<T>(std::forward<Args>(args)...));
}

template<typename T, typename Type, typename Deleter, typename... Args>
ALWAYS_INLINE CompactUniquePtrTuple<T, Type> makeCompactUniquePtr(Args&&... args)
{
    return CompactUniquePtrTuple<T, Type, Deleter>(makeUnique<T>(std::forward<Args>(args)...));
}

template<typename T, typename Type, typename Deleter>
class CompactUniquePtrTuple final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CompactUniquePtrTuple);
public:
    CompactUniquePtrTuple() = default;

    template <typename U>
    CompactUniquePtrTuple(CompactUniquePtrTuple<U, Type, Deleter>&& other)
        : m_data { std::exchange(other.m_data, { }) }
    {
    }

    ~CompactUniquePtrTuple()
    {
        setPointer(nullptr);
    }

    template <typename U>
    CompactUniquePtrTuple<T, Type, Deleter>& operator=(CompactUniquePtrTuple<U, Type, Deleter>&& other)
    {
        auto moved = WTFMove(other);
        std::swap(m_data, moved.m_data);
        return *this;
    }

    T* pointer() const { return m_data.pointer(); }

    std::unique_ptr<T, Deleter> moveToUniquePtr()
    {
        T* pointer = m_data.pointer();
        m_data.setPointer(nullptr);
        return std::unique_ptr<T, Deleter>(pointer);
    }

    void setPointer(nullptr_t)
    {
        deletePointer();
        m_data.setPointer(nullptr);
    }

    template <typename U>
    void setPointer(std::unique_ptr<U, Deleter>&& pointer)
    {
        deletePointer();
        m_data.setPointer(pointer.release());
    }

    Type type() const { return m_data.type(); }

    void setType(Type type)
    {
        m_data.setType(type);
    }

private:
    CompactUniquePtrTuple(std::unique_ptr<T>&& pointer)
    {
        m_data.setPointer(pointer.release());
    }

    void deletePointer()
    {
        if (T* pointer = m_data.pointer())
            Deleter()(pointer);
    }

    template<typename U, typename E, typename... Args> friend CompactUniquePtrTuple<U, E> makeCompactUniquePtr(Args&&... args);
    template<typename U, typename E, typename D, typename... Args> friend CompactUniquePtrTuple<U, E, D> makeCompactUniquePtr(Args&&... args);

    CompactPointerTuple<T*, Type> m_data;
};

} // namespace WTF

using WTF::CompactUniquePtrTuple;
using WTF::makeCompactUniquePtr;
