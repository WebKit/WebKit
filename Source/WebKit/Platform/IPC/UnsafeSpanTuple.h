/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/Vector.h>

namespace IPC {

// A tuple of equal-length arrays, decoded as a non-owning view: the pointers reference the source
// buffer directly (decoded in place). Over a Stream connection that buffer is sender-writable, so
// this is TOCTOU-unsafe; use only where the decoder-side access has been audited safe (like
// IPC::UnsafeSpan). See IPC::SpanTuple for the copying variant.
template<typename... Types>
class UnsafeSpanTuple {
public:
    UnsafeSpanTuple() = default;

    UnsafeSpanTuple(const Types*... data, size_t size)
        : m_size(size)
    {
        if (m_size)
            m_data = { data... };
    }

    bool isEmpty() const { return !m_size; }
    size_t size() const { return m_size; }

    template<unsigned I>
    auto data() const
    {
        return std::get<I>(m_data);
    }

    template<unsigned I>
    auto span() const
    {
        return unsafeMakeSpan(std::get<I>(m_data), m_size);
    }

private:
    size_t m_size { 0 };
    std::tuple<const Types*...> m_data;
};

// The copying variant of UnsafeSpanTuple: on decode the arrays are copied out of the source buffer
// into decoder-owned storage, so the pointers are stable against concurrent sender writes (like
// std::span over IPC). Prefer this over UnsafeSpanTuple unless in-place access has been audited.
template<typename... Types>
class SpanTuple {
public:
    SpanTuple() = default;

    SpanTuple(const Types*... data, size_t size)
        : m_size(size)
    {
        if (m_size)
            m_data = { data... };
    }

    bool isEmpty() const { return !m_size; }
    size_t size() const { return m_size; }

    template<unsigned I>
    auto data() const
    {
        return std::get<I>(m_data);
    }

    template<unsigned I>
    auto span() const
    {
        return unsafeMakeSpan(std::get<I>(m_data), m_size);
    }

private:
    size_t m_size { 0 };
    std::tuple<const Types*...> m_data;
};

}
