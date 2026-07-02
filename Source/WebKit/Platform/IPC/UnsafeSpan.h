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
#include <wtf/StdLibExtras.h>

namespace IPC {

// A span argument for IPC data that can be referred to in place, without a copy preventing TOCTOU.
// Should be only used for data that is not validated at all, such as pixel value data, or
// values that are copied out in the decoder, like matrix values copied to matrix members.
// Decoding std::span is always safe.
// Decoding IPC::UnsafeSpan should be used as an optimization if the decoder is manually
// verified as being safe against simultaneous modifications to the underlying buffer data.
template<typename T, size_t Extent = std::dynamic_extent>
class UnsafeSpan {
public:
    using SpanType = std::span<T, Extent>;

    UnsafeSpan() = default;
    UnsafeSpan(SpanType span)
        : m_span(span)
    {
    }

    SpanType span() const LIFETIME_BOUND { return m_span; }

    auto data() const LIFETIME_BOUND { return m_span.data(); }
    size_t size() const { return m_span.size(); }
    size_t size_bytes() const { return m_span.size_bytes(); } // NOLINT
    bool empty() const { return m_span.empty(); }
    auto begin() const LIFETIME_BOUND { return m_span.begin(); }
    auto end() const LIFETIME_BOUND { return m_span.end(); }
    decltype(auto) operator[](size_t i) const LIFETIME_BOUND { return m_span[i]; }

private:
    SpanType m_span;
};

} // namespace IPC
