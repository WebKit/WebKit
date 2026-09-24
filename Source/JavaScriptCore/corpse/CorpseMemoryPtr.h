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
#include <stdint.h>
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>

namespace JSC {
namespace Corpse {

// A pointer-like handle to a const T instance mapped out of remote corpse memory into this
// process' virtual address space. Ptr will retain and keep the underlying Region (which
// backs this memory mapping) alive.
//
// Invalid when the underlying Region could not be mapped, in which case error() says
// why and get() is null. Moving Ptr moves its reference to the underlying Region rather
// than duplicating it.

template<typename T>
class Memory::Ptr {
public:
    using ConstT = std::add_const_t<T>;

    Ptr() = default;
    ~Ptr() { release(); }

    Ptr(Ptr&& other) { *this = WTF::move(other); }
    Ptr& operator=(Ptr&&);

    Ptr(const Ptr&) = delete;
    Ptr& operator=(const Ptr&) = delete;

    bool isValid() const { return m_data; }
    explicit operator bool() const { return isValid(); }

    Error error() const { return m_error; }
    KernelResult kernResult() const { return m_kernResult; }

    Address address() const { return m_address; }
    size_t sizeInBytes() const { return isValid() ? sizeof(T) : 0; }

    // Null when invalid.
    ConstT* get() const LIFETIME_BOUND { return m_data; }

    ConstT* operator->() const LIFETIME_BOUND
    {
        RELEASE_ASSERT(m_data);
        return m_data;
    }

    ConstT& operator*() const LIFETIME_BOUND { return *operator->(); }

private:
    friend class Memory;

    Ptr(Address address, const MapResult& result)
        : m_data(reinterpret_cast<ConstT*>(result.data))
        , m_address(address)
        , m_region(result.region)
        , m_error(result.error)
        , m_kernResult(result.kernResult)
    {
    }

    void release();

    ConstT* m_data { nullptr };
    Address m_address;

    // The mapped region backing the value of T. Ptr keeps this Region alive.
    Region* m_region { nullptr };

    Error m_error { Error::None };
    KernelResult m_kernResult { kernelSuccess };
};

template<typename T>
Memory::Ptr<T>& Memory::Ptr<T>::operator=(Ptr&& other)
{
    if (this == &other)
        return *this;

    release();

    m_data = other.m_data;
    m_address = other.m_address;
    m_region = other.m_region;
    m_error = other.m_error;
    m_kernResult = other.m_kernResult;

    other.m_data = nullptr;
    other.m_region = nullptr;

    return *this;
}

template<typename T>
void Memory::Ptr<T>::release()
{
    if (m_region)
        m_region->release();
    m_data = nullptr;
    m_region = nullptr;
}

template<typename T>
Memory::Ptr<T> Memory::ptr(Address address)
{
    return Ptr<T>(address, map(address, sizeof(T)));
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
