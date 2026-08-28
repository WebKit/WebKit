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

#include "config.h"
#include "CorpseAddressTest.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseAddress.h>
#include <mach/mach.h>
#include <type_traits>

#if CPU(ARM64E)
#include <ptrauth.h>
#endif

namespace JSCToolsTest {

using JSC::Corpse::Address;

void testAddress()
{
    if (!beginSuite("Address"))
        return;

    {
        Address none;
        TEST_ASSERT(!none, "a default Address is null");
        TEST_ASSERT(none == nullptr, "a default Address compares equal to nullptr");
        TEST_ASSERT_HEX_EQ(none.toMachVMAddress(), 0, "a default Address holds zero");
    }
    {
        Address address(static_cast<mach_vm_address_t>(0x1000));
        TEST_ASSERT(static_cast<bool>(address), "a non-zero Address is not null");
        TEST_ASSERT(!(address == nullptr), "a non-zero Address does not compare equal to nullptr");
        TEST_ASSERT_HEX_EQ(address.toMachVMAddress(), 0x1000, "an Address holds what it was given");
    }
    {
        int local = 0;
        Address address(&local);
        TEST_ASSERT_HEX_EQ(address.toMachVMAddress(), reinterpret_cast<uintptr_t>(&local),
            "an Address built from a pointer holds that pointer");
    }
    {
        // The whole point of the type: a corpse address must not be usable as a
        // local one by accident, so there is no conversion out of it.
        TEST_ASSERT(!(std::is_convertible_v<Address, uint64_t>),
            "an Address does not convert to an integer");
        TEST_ASSERT(!(std::is_convertible_v<Address, void*>),
            "an Address does not convert to a pointer");
    }
    {
        Address low(static_cast<mach_vm_address_t>(0x1000));
        Address high(static_cast<mach_vm_address_t>(0x2000));
        TEST_ASSERT(low < high, "Addresses order by value");
        TEST_ASSERT(high > low, "Addresses order by value the other way");
        TEST_ASSERT(low <= low && low >= low, "an Address is not less or greater than itself");
        TEST_ASSERT(low == Address(static_cast<mach_vm_address_t>(0x1000)), "equal values compare equal");
        TEST_ASSERT(low != high, "different values do not compare equal");
    }
    {
        Address base(static_cast<mach_vm_address_t>(0x1000));
        TEST_ASSERT_HEX_EQ((base + 0x20).toMachVMAddress(), 0x1020, "adding an offset moves forward");
        TEST_ASSERT_HEX_EQ((base - 0x20).toMachVMAddress(), 0x0fe0, "subtracting an offset moves back");
        TEST_ASSERT_HEX_EQ(Address(static_cast<mach_vm_address_t>(0x1030)) - base, 0x30,
            "subtracting two Addresses gives the distance between them");
    }
    {
        // A plain address has nothing to strip, whatever the platform.
        Address plain(static_cast<mach_vm_address_t>(0x0000000100002000));
        TEST_ASSERT_HEX_EQ(plain.stripped().toMachVMAddress(), 0x0000000100002000,
            "stripping an unsigned address changes nothing");
    }
#if CPU(ARM64E)
    {
        // A pointer read out of a corpse arrives signed for the target's context,
        // and must be reduced to the address it names before it is used as one.
        void* raw = reinterpret_cast<void*>(static_cast<uintptr_t>(0x0000000100002000));
        void* signedPointer;
        unsigned count = 0;
        constexpr unsigned maxRetryCount = 10;
        do {
            signedPointer = ptrauth_sign_unauthenticated(raw, ptrauth_key_process_dependent_code, 0);
        } while (signedPointer == raw && ++count <= maxRetryCount);
        TEST_ASSERT(count <= maxRetryCount, "unable to generate PAC signed pointer for test");
        TEST_ASSERT_HEX_EQ(Address(signedPointer).stripped().toMachVMAddress(),
            reinterpret_cast<uintptr_t>(raw), "stripping recovers the address a signed pointer names");
    }
    {
        // Top-byte-ignore and memory tagging both leave data in the top byte, which
        // is not part of the address either.
        Address tagged(static_cast<mach_vm_address_t>(0x4200000100002000));
        TEST_ASSERT_HEX_EQ(tagged.stripped().toMachVMAddress(), 0x0000000100002000,
            "stripping clears a tagged top byte");
    }
#endif
}

} // namespace JSCToolsTest

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
