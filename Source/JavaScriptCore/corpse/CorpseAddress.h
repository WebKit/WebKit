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

#include <bit>
#include <compare>
#include <cstddef>
#include <mach/mach.h>
#include <stdint.h>

#if CPU(ARM64E)
#include <ptrauth.h>
#endif

namespace JSC {
namespace Corpse {

// An address in the target corpse process. A corpse address can never be dereferenced
// by accident.
class Address {
public:
    Address() = default;
    explicit Address(mach_vm_address_t value)
        : m_value(value)
    {
    }
    explicit Address(const void* pointer)
        : m_value(reinterpret_cast<mach_vm_address_t>(pointer))
    {
    }

    mach_vm_address_t toMachVMAddress() const { return m_value; }
    explicit operator bool() const { return m_value; }
    template<typename T> explicit operator T() const = delete;

    Address stripped() const
    {
#if CPU(ARM64E)
        // We don't know if this is a code or data pointer. The 2 have different number of
        // bits. But we know that code pointers have more PAC bits. So, we'll conservatively
        // use XPACI to strip the max number of PAC bits.
        auto stripped = ptrauth_strip(reinterpret_cast<void*>(m_value), ptrauth_key_process_dependent_code);

        // While XPACI may have already stripped the MTE tag in data pointers as well,
        // we don't want to assume that code pointer PAC bits will always cover the MTE
        // nibble or non-zero data pointer top-bytes due to TBI (Top Byte Ignore). So,
        // let's explicitly clear the top byte to be sure.
        constexpr uintptr_t topByte = 0xffull << 56;
        uintptr_t strippedInt = std::bit_cast<uintptr_t>(stripped);
        strippedInt &= ~topByte;

        return Address(std::bit_cast<void*>(strippedInt));
#else
        return *this;
#endif
    }

    friend bool operator==(Address, Address) = default;
    friend auto operator<=>(Address, Address) = default;

    friend bool operator==(Address address, std::nullptr_t) { return !address.m_value; }

    Address operator+(uint64_t offset) const { return Address(m_value + offset); }
    Address operator-(uint64_t offset) const { return Address(m_value - offset); }

    uint64_t operator-(Address other) const { return m_value - other.m_value; }

private:
    mach_vm_address_t m_value { 0 };
};

} // namespace Corpse
} // namespace JSC

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
