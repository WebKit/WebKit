/*
 * Copyright (C) 2026 Igalia S.L.
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

#if USE(VULKAN)
#include <type_traits>
#include <wtf/Noncopyable.h>

namespace WebCore {
namespace Vulkan {

//
// Handles "own" the wrapped object and are responsible for destroying it.
// Therefore they can only be moved, to guarantee that destruction gets
// done only once.
//
template <typename VulkanType>
    requires std::is_pointer_v<VulkanType>
struct Handle {
    WTF_MAKE_NONCOPYABLE(Handle)

protected:
    using Base = Handle<VulkanType>;

public:
    Handle() = default;

    Handle(VulkanType ptr)
        : m_ptr(ptr)
    {
    }

    Handle(Handle&& other)
        : Handle(other.leakPtr())
    {
    }

    void swap(Handle& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    Handle& operator=(Handle&& other)
    {
        auto handle = WTF::move(other);
        swap(handle);
        return *this;
    }

    [[nodiscard]] VulkanType leakPtr()
    {
        return std::exchange(m_ptr, nullptr);
    }

    const VulkanType ptr() const LIFETIME_BOUND { return m_ptr; }
    VulkanType ptr() LIFETIME_BOUND { return m_ptr; }

    explicit operator bool() const { return !!m_ptr; }
    bool operator!() const { return !m_ptr; }

private:
    VulkanType m_ptr { nullptr };
};

//
// Borrowed handles do not "own" the object, therefore they can be copied.
// The main goal is to provide a similar API surface to Handle<T> and to
// allow attaching methods to them.
//
template <typename VulkanType>
    requires std::is_pointer_v<VulkanType>
struct BorrowedHandle {
    BorrowedHandle() = default;

    BorrowedHandle(const BorrowedHandle&) = default;
    BorrowedHandle& operator=(const BorrowedHandle&) = default;

    BorrowedHandle(BorrowedHandle&&) = default;
    BorrowedHandle& operator=(BorrowedHandle&&) = default;

    const VulkanType ptr() const LIFETIME_BOUND { return m_ptr; }
    VulkanType ptr() LIFETIME_BOUND { return m_ptr; }

    explicit operator bool() const { return !!m_ptr; }
    bool operator!() const { return !m_ptr; }

private:
    VulkanType m_ptr { nullptr };
};


} // namespace Vulkan
} // namespace WebCore

#endif // USE(VULKAN)
