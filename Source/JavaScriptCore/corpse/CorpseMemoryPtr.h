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

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <JavaScriptCore/CorpseMemory.h>
#include <mach/mach.h>
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
// why and Ptr::operator bool() returns false. Upon acquiring a Ptr from Memory, clients
// should do the equivalent of a null check to make sure the Ptr is valid before using.
// operator bool() is provided as a convenience so that clients can express this idiom:
//
//    auto ptr = memory.ptr<uint64_t>(address);
//    if (ptr && *ptr == desired) ...
//
// Using when invalid produces undefined behavior.
//
// Moving Ptr moves its reference to the underlying Region rather than duplicating it.

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
    kern_return_t kernResult() const { return m_kernResult; }

    Address address() const { return m_address; }
    size_t sizeInBytes() const { return isValid() ? sizeof(T) : 0; }

    // Null when invalid.
    ConstT* get() const LIFETIME_BOUND { return m_data; }

    ConstT* operator->() const LIFETIME_BOUND
    {
        ASSERT(m_data);
        return m_data;
    }

    ConstT& operator*() const LIFETIME_BOUND { return *operator->(); }

private:
    friend class Memory;

    Ptr(Address address, MapResult&& result)
        : m_data(reinterpret_cast<ConstT*>(result.data))
        , m_address(address)
        , m_region(WTF::move(result.region))
        , m_error(result.error)
        , m_kernResult(result.kernResult)
    {
    }

    void release();

    ConstT* m_data { nullptr };
    Address m_address;
    RefPtr<Region> m_region;

    Error m_error { Error::None };
    kern_return_t m_kernResult { KERN_SUCCESS };
};

template<typename T>
Memory::Ptr<T>& Memory::Ptr<T>::operator=(Ptr&& other)
{
    if (this == &other)
        return *this;

    release();

    m_data = other.m_data;
    m_address = other.m_address;
    m_region = WTF::move(other.m_region);
    m_error = other.m_error;
    m_kernResult = other.m_kernResult;

    other.m_data = nullptr;

    return *this;
}

template<typename T>
void Memory::Ptr<T>::release()
{
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

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
