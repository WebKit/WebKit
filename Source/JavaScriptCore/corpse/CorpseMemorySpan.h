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

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA)

#include <JavaScriptCore/CorpseMemory.h>
#include <cstddef>
#include <limits>
#include <span>
#include <stdint.h>
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>

namespace JSC {
namespace Corpse {

// A span-like handle on a span of const T mapped out of remote corpse memory into this
// process' virtual address space. Span will retain and keep the underlying Region (which
// backs this memory mapping) alive.
//
// Invalid when the underlying Region could not be mapped, in which case error() says
// why and the span reads as empty. Moving Span moves its reference to the underlying
// Region rather than duplicating it.

template<typename T>
class Memory::Span {
public:
    using ConstT = std::add_const_t<T>;
    using iterator = typename std::span<ConstT>::iterator;
    using reverse_iterator = typename std::span<ConstT>::reverse_iterator;

    Span() = default;
    ~Span() { release(); }

    Span(Span&& other) { *this = WTF::move(other); }
    Span& operator=(Span&&);

    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;

    bool isValid() const { return m_data; }
    explicit operator bool() const { return isValid(); }

    Error error() const { return m_error; }
    KernelResult kernResult() const { return m_kernResult; }

    Address address() const { return m_address; }

    operator std::span<ConstT>() const LIFETIME_BOUND { return stdSpan(); }

    ConstT* data() const LIFETIME_BOUND { return m_data; }
    size_t size() const { return m_size; }
    size_t sizeInBytes() const { return m_size * sizeof(T); }
    bool empty() const { return !m_size; }

    iterator begin() const LIFETIME_BOUND { return stdSpan().begin(); }
    iterator end() const LIFETIME_BOUND { return stdSpan().end(); }
    reverse_iterator rbegin() const LIFETIME_BOUND { return stdSpan().rbegin(); }
    reverse_iterator rend() const LIFETIME_BOUND { return stdSpan().rend(); }

    ConstT& front() const LIFETIME_BOUND { return stdSpan().front(); }
    ConstT& back() const LIFETIME_BOUND { return stdSpan().back(); }
    ConstT& operator[](size_t index) const LIFETIME_BOUND { return stdSpan()[index]; }

    // Sub-ranges are views on this handle's region rather than handles of their own, so
    // they do not keep it mapped by themselves.
    std::span<ConstT> first(size_t count) const LIFETIME_BOUND { return stdSpan().first(count); }
    std::span<ConstT> last(size_t count) const LIFETIME_BOUND { return stdSpan().last(count); }
    std::span<ConstT> subspan(size_t offset, size_t count = std::dynamic_extent) const LIFETIME_BOUND
    {
        return stdSpan().subspan(offset, count);
    }

private:
    friend class Memory;

    Span(Address address, size_t count, const MapResult& result)
        : m_data(reinterpret_cast<ConstT*>(result.data))
        // A request that mapped nothing holds no elements, so that an invalid handle
        // reads as empty rather than as a run over a null pointer.
        , m_size(result.data ? count : 0)
        , m_address(address)
        , m_region(result.region)
        , m_error(result.error)
        , m_kernResult(result.kernResult)
    {
    }

    // The one place a std::span is formed over the mapping. Empty when invalid, which
    // is what lets the accessors above be called without checking first.
    std::span<ConstT> stdSpan() const LIFETIME_BOUND { return unsafeMakeSpan(m_data, m_size); }

    void release();

    ConstT* m_data { nullptr };
    size_t m_size { 0 };
    Address m_address;

    // The region this holds a reference on, which is what keeps m_data mapped.
    Region* m_region { nullptr };

    Error m_error { Error::None };
    KernelResult m_kernResult { kernelSuccess };
};

template<typename T>
Memory::Span<T>& Memory::Span<T>::operator=(Span&& other)
{
    if (this == &other)
        return *this;

    release();

    m_data = other.m_data;
    m_size = other.m_size;
    m_address = other.m_address;
    m_region = other.m_region;
    m_error = other.m_error;
    m_kernResult = other.m_kernResult;

    other.m_data = nullptr;
    other.m_size = 0;
    other.m_region = nullptr;

    return *this;
}

template<typename T>
void Memory::Span<T>::release()
{
    if (m_region)
        m_region->release();
    m_data = nullptr;
    m_size = 0;
    m_region = nullptr;
}

template<typename T>
Memory::Span<T> Memory::span(Address address, size_t count)
{
    if (count > std::numeric_limits<size_t>::max() / sizeof(T))
        return Span<T>(address, count, MapResult { .error = Error::InvalidRequest });
    return Span<T>(address, count, map(address, count * sizeof(T)));
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
