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
#include "VulkanVolk.h"
#include <type_traits>

namespace WebCore {
namespace Vulkan {

template <typename VulkanType, VkStructureType vulkanStructureType>
    requires std::is_class_v<VulkanType> && std::is_trivially_copyable_v<VulkanType>
struct Structure {
    using Type = VulkanType;
    using Base = Structure<VulkanType, vulkanStructureType>;

    static constexpr VkStructureType typeCode = vulkanStructureType;

    Structure()
    {
        zeroBytes(m_inner);
        m_inner.sType = typeCode;
    }

    Structure(VulkanType&& value)
        : m_inner(WTF::move(value))
    {
        ASSERT(m_inner.sType == typeCode);
    }

    Structure(const Structure&) = default;
    Structure& operator=(const Structure&) = default;

    Structure(Structure&&) = default;
    Structure& operator=(Structure&&) = default;

    // Create another structure, make it the "next" to this, and return it.
    // This enables the following handy idiom to create chained Structure
    // subtype instances:
    //
    //   struct A : Structure<...> { };
    //   struct B : Structure<...> { };
    //   struct C : Structure<...> { };
    //
    //   A first;
    //   auto second = first.next<B>();
    //   auto third = second.next<C>();
    //
    // Then the resulting chain of structures is: first -> second -> third.
    //
    template <typename OtherType, typename... Args>
    requires std::derived_from<OtherType, Structure<typename OtherType::Type, OtherType::typeCode>>
    [[nodiscard]] OtherType next(Args&&... params)
    {
        OtherType nextStructure(std::forward<Args>(params)...);
        m_inner.pNext = nextStructure.ptr();
        return nextStructure;
    }

    const VulkanType& value() const LIFETIME_BOUND { return m_inner; }
    VulkanType& value() LIFETIME_BOUND { return m_inner; }

    const VulkanType* ptr() const LIFETIME_BOUND { return &m_inner; }
    VulkanType* ptr() LIFETIME_BOUND { return &m_inner; }

    const VulkanType* operator->() const LIFETIME_BOUND { return ptr(); }
    VulkanType* operator->() LIFETIME_BOUND { return ptr(); }

private:
    VulkanType m_inner;
};

} // namespace Vulkan
} // namespace WebCore

#endif // USE(VULKAN)
