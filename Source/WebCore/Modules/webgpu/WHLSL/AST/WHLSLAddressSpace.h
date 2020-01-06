/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include <cstdint>
#include <wtf/FastMalloc.h>
#include <wtf/Variant.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

enum class AddressSpace : uint8_t {
    Constant,
    Device,
    Threadgroup,
    Thread
};

ALWAYS_INLINE StringView toString(AddressSpace addressSpace)
{
    switch (addressSpace) {
    case AddressSpace::Constant:
        return "constant";
    case AddressSpace::Device:
        return "device";
    case AddressSpace::Threadgroup:
        return "threadgroup";
    default:
        ASSERT(addressSpace == AddressSpace::Thread);
        return "thread";
    }
}

struct LeftValue {
    AddressSpace addressSpace;
};

struct AbstractLeftValue {
};

struct RightValue {
};

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=198158 This wrapper might not be necessary.
class TypeAnnotation {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TypeAnnotation()
#if ASSERT_ENABLED
        : m_empty(true)
#endif
    {
    }

    TypeAnnotation(LeftValue v)
        : m_inner(v)
    {
    }

    TypeAnnotation(AbstractLeftValue v)
        : m_inner(v)
    {
    }

    TypeAnnotation(RightValue v)
        : m_inner(v)
    {
    }

    TypeAnnotation(const TypeAnnotation&) = default;
    TypeAnnotation(TypeAnnotation&& other) = default;

    TypeAnnotation& operator=(const TypeAnnotation&) = default;
    TypeAnnotation& operator=(TypeAnnotation&& other) = default;

    Optional<AddressSpace> leftAddressSpace() const
    {
        ASSERT(!m_empty);
        if (WTF::holds_alternative<LeftValue>(m_inner))
            return WTF::get<LeftValue>(m_inner).addressSpace;
        return WTF::nullopt;
    }

    bool isRightValue() const
    {
        ASSERT(!m_empty);
        return WTF::holds_alternative<RightValue>(m_inner);
    }

    template <typename Visitor> auto visit(const Visitor& visitor) -> decltype(WTF::visit(visitor, std::declval<Variant<LeftValue, AbstractLeftValue, RightValue>&>()))
    {
        ASSERT(!m_empty);
        return WTF::visit(visitor, m_inner);
    }

    bool isAbstractLeftValue() const
    {
        ASSERT(!m_empty);
        return WTF::holds_alternative<AbstractLeftValue>(m_inner);
    }

private:
    Variant<LeftValue, AbstractLeftValue, RightValue> m_inner;
#if ASSERT_ENABLED
    bool m_empty { false };
#endif
};

}

}

}

#endif
