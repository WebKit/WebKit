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
#include <wtf/Compiler.h>

namespace IPC {

// A non-owning view over decoded message data, byte-identical on the wire to
// std::span<const T, Extent>. On a stream connection its decoded storage aliases
// the sender-writable shared buffer, so it is TOCTOU-prone and must only be used
// for data the receiver has audited to be inert (opaque bytes it never interprets
// for control flow, bounds, or indexing). It is the audited opt-out that skips the
// defensive copy the plain std::span coder performs on a stream connection.
template<typename T, size_t Extent = std::dynamic_extent>
class UnsafeSpan {
public:
    UnsafeSpan() = default;
    UnsafeSpan(std::span<const T, Extent> span)
        : m_span(span)
    {
    }

    std::span<const T, Extent> span() const LIFETIME_BOUND { return m_span; }

    auto begin() const { return m_span.begin(); }
    auto end() const { return m_span.end(); }
    const T* data() const LIFETIME_BOUND { return m_span.data(); }
    size_t size() const { return m_span.size(); }
    bool empty() const { return m_span.empty(); }
    decltype(auto) operator[](size_t index) const { return m_span[index]; }

private:
    std::span<const T, Extent> m_span;
};

}
