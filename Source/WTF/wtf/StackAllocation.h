/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <wtf/Assertions.h>

namespace WTF {

class StackAllocationSpecification {
public:
    enum class Kind : std::uint8_t {
        Default = 0, // OS provided stack, OS default size
        SizeOnly, // OS provided stack, specified size
        SizeAndLocation, // Preallocated stack with known size
    };

    StackAllocationSpecification() = default;

    static StackAllocationSpecification RequestSize(std::size_t bytes)
    {
        StackAllocationSpecification s(Kind::SizeOnly);
        s.storage.sizeBytes = bytes;
        return s;
    }

    static StackAllocationSpecification CustomStack(std::span<std::byte> stack)
    {
        StackAllocationSpecification s(Kind::SizeAndLocation);
        s.storage.stack = stack;
        return s;
    }

    Kind kind() const { return m_kind; }
    bool isKind(Kind kind) const { return this->kind() == kind; }

    std::size_t sizeBytes() const
    {
        RELEASE_ASSERT(m_kind == Kind::SizeOnly || m_kind == Kind::SizeAndLocation);
        if (m_kind == Kind::SizeOnly)
            return storage.sizeBytes;
        if (m_kind == Kind::SizeAndLocation)
            return storage.stack.size_bytes();
        RELEASE_ASSERT_NOT_REACHED();
    }

    std::span<std::byte> stackSpan() const
    {
        RELEASE_ASSERT(m_kind == Kind::SizeAndLocation);
        return storage.stack;
    }

private:
    explicit StackAllocationSpecification(Kind k)
        : m_kind(k) { }

    Kind m_kind { Kind::Default };

    union Storage {
        constexpr Storage() : sizeBytes(0) { }
        std::size_t sizeBytes;
        std::span<std::byte> stack;
    } storage { };
};

} // namespace WTF

using WTF::StackAllocationSpecification;
