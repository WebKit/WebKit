/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <span>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// Mixin for implementing RAII holders for memory that has custom alloc and free functionality.
template<typename T> class AllocSpanMixin {
    WTF_MAKE_NONCOPYABLE(AllocSpanMixin);
public:
    AllocSpanMixin(AllocSpanMixin&& other)
        : m_span(other.leakSpan())
    {
    }

    void swap(AllocSpanMixin& other)
    {
        std::swap(m_span, other.m_span);
    }

    size_t sizeInBytes() const { return m_span.size_bytes(); }

    std::span<const T> span() const LIFETIME_BOUND { return m_span; }
    std::span<T> mutableSpan() LIFETIME_BOUND { return m_span; }
    std::span<T> leakSpan() WARN_UNUSED_RETURN { return std::exchange(m_span, std::span<T> { }); }

    T& operator[](size_t i) LIFETIME_BOUND { return m_span[i]; }
    const T& operator[](size_t i) const LIFETIME_BOUND { return m_span[i]; }

    explicit operator bool() const
    {
        return !!m_span.data();
    }

    bool operator!() const
    {
        return !m_span.data();
    }

protected:
    AllocSpanMixin() = default;
    explicit AllocSpanMixin(std::span<T> span)
        : m_span(span)
    {
    }
    explicit AllocSpanMixin(void* ptr, size_t sizeInBytes)
        : m_span(unsafeMakeSpan(static_cast<T*>(ptr), sizeInBytes / sizeof(T)))
    {
        RELEASE_ASSERT(!(sizeInBytes % sizeof(T)));
    }

private:
    std::span<T> m_span;
};

}
