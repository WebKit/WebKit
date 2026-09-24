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

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseAddress.h>
#include <type_traits>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

#if CPU(ARM64E)
#include <ptrauth.h>
#endif

namespace JSCToolsTest {

using JSC::Corpse::Address;
using JSC::Corpse::target_address_t;

void testAddress()
{
    SuiteTracer tracer("Address");
    if (!tracer.shouldRun())
        return;

    {
        Address none;
        TEST_ASSERT(!none, "a default Address is null");
        TEST_ASSERT(none == nullptr, "a default Address compares equal to nullptr");
        TEST_ASSERT_HEX_EQ(none.toTargetVMAddress(), 0, "a default Address holds zero");
    }
    {
        Address address(static_cast<uint64_t>(0x1000));
        TEST_ASSERT(static_cast<bool>(address), "a non-zero Address is not null");
        TEST_ASSERT(!(address == nullptr), "a non-zero Address does not compare equal to nullptr");
        TEST_ASSERT_HEX_EQ(address.toTargetVMAddress(), 0x1000, "an Address holds what it was given");
    }
    {
        int local = 0;
        Address address(&local);
        TEST_ASSERT_HEX_EQ(address.toTargetVMAddress(), reinterpret_cast<uintptr_t>(&local),
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
        Address low(static_cast<uint64_t>(0x1000));
        Address high(static_cast<uint64_t>(0x2000));
        TEST_ASSERT(low < high, "Addresses order by value");
        TEST_ASSERT(high > low, "Addresses order by value the other way");
        TEST_ASSERT(low <= low && low >= low, "an Address is not less or greater than itself");
        TEST_ASSERT(low == Address(static_cast<uint64_t>(0x1000)), "equal values compare equal");
        TEST_ASSERT(low != high, "different values do not compare equal");
    }
    {
        Address base(static_cast<uint64_t>(0x1000));
        TEST_ASSERT_HEX_EQ((base + 0x20).toTargetVMAddress(), 0x1020, "adding an offset moves forward");
        TEST_ASSERT_HEX_EQ((base - 0x20).toTargetVMAddress(), 0x0fe0, "subtracting an offset moves back");
        TEST_ASSERT_HEX_EQ(Address(static_cast<uint64_t>(0x1030)) - base, 0x30,
            "subtracting two Addresses gives the distance between them");
    }
    {
        // A plain address has nothing to strip, whatever the platform.
        Address plain(static_cast<uint64_t>(0x0000000100002000));
        TEST_ASSERT_HEX_EQ(plain.stripped().toTargetVMAddress(), 0x0000000100002000,
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
        TEST_ASSERT_HEX_EQ(Address(signedPointer).stripped().toTargetVMAddress(),
            reinterpret_cast<uintptr_t>(raw), "stripping recovers the address a signed pointer names");
    }
    {
        // Top-byte-ignore and memory tagging both leave data in the top byte, which
        // is not part of the address either.
        Address tagged(static_cast<uint64_t>(0x4200000100002000));
        TEST_ASSERT_HEX_EQ(tagged.stripped().toTargetVMAddress(), 0x0000000100002000,
            "stripping clears a tagged top byte");
    }
#endif

    {
        // Test Address as a hash table key.
        HashMap<Address, unsigned> map;
        Address first(static_cast<target_address_t>(0x100000000));
        Address second(static_cast<target_address_t>(0x100004000));

        map.add(first, 1u);
        map.add(second, 2u);
        TEST_ASSERT_EQ(map.size(), static_cast<unsigned>(2), "two addresses are two keys");
        TEST_ASSERT_EQ(map.get(first), static_cast<unsigned>(1), "the first key finds its value");
        TEST_ASSERT_EQ(map.get(second), static_cast<unsigned>(2), "so does the second");
        TEST_ASSERT(map.contains(first), "a key that was added is found");

        TEST_ASSERT(map.remove(first), "a key can be removed");
        TEST_ASSERT(!map.contains(first), "and is then not found");
        TEST_ASSERT(map.contains(second), "while the other key survives its removal");
        TEST_ASSERT_EQ(map.get(second), static_cast<unsigned>(2), "with its value intact");

        // Re-adding after a removal has to reuse the deleted bucket rather than trip
        // over it.
        map.add(first, 3u);
        TEST_ASSERT_EQ(map.get(first), static_cast<unsigned>(3), "a removed key can be added again");

        HashSet<Address> set;
        set.add(first);
        TEST_ASSERT(set.contains(first), "an Address works as a set element too");
        TEST_ASSERT(!set.contains(second), "and an absent one is absent");
    }
    {
        Address slot;
        WTF::HashTraits<Address>::constructDeletedValue(slot);
        TEST_ASSERT(WTF::HashTraits<Address>::isDeletedValue(slot),
            "the deleted key the traits construct is recognised as deleted");
        TEST_ASSERT(slot != Address(), "and is not the empty key");
        TEST_ASSERT(!WTF::HashTraits<Address>::isDeletedValue(Address()),
            "the empty key is not the deleted key");
    }
    {
        // Test that re-adding an Address to a HashMap (keyed on Address) doesn't result
        // in some entries being hidden due to confusion with deleted entries.

        // A truncated probe chain only shows up once keys collide. Test with churning.
        static constexpr size_t pageSize = 16384;
        static constexpr target_address_t keyBase = 0x100000000;
        static constexpr unsigned windowSize = 64;
        static constexpr unsigned rounds = 200;

        auto keyAt = [] (unsigned index) {
            return Address(keyBase + static_cast<target_address_t>(index) * pageSize);
        };

        HashMap<Address, unsigned> map;
        for (unsigned index = 0; index < windowSize; ++index)
            map.add(keyAt(index), index);

        unsigned first = 0;
        unsigned limit = windowSize;
        bool everyKeyFound = true;
        for (unsigned round = 0; round < rounds && everyKeyFound; ++round) {
            if (!map.remove(keyAt(first++))) {
                everyKeyFound = false;
                break;
            }
            map.add(keyAt(limit), limit);
            ++limit;

            for (unsigned index = first; index < limit; ++index) {
                if (map.find(keyAt(index)) == map.end()) {
                    everyKeyFound = false;
                    break;
                }
            }
        }
        TEST_ASSERT(everyKeyFound, "every key in the window survives repeated add and remove");
        TEST_ASSERT_EQ(map.size(), windowSize, "and the window keeps its size");
    }
}

} // namespace JSCToolsTest

#endif // ENABLE(MYA)
