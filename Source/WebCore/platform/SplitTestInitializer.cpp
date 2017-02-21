/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "SplitTestInitializer.h"

#if !PLATFORM(IOS) && OS(DARWIN) && USE(APPLE_INTERNAL_SDK)

#include <dlfcn.h>
#include <mutex>
#include <WebCore/SoftLinking.h>
#include <wtf/ASCIICType.h>
#include <wtf/Optional.h>
#include <wtf/SplitTest.h>

SOFT_LINK_PRIVATE_FRAMEWORK(CrashReporterSupport);
SOFT_LINK(CrashReporterSupport, CRGetUserUUID, CFStringRef, (void), ());

namespace {

struct UUID {
    uint64_t lo;
    uint64_t hi;
};

std::optional<UUID> getUUID()
{
    // The UUID we're about to obtain contains 128-bit of data, encoded as a
    // hexadecimal ASCII string with the following format:
    //     FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF
    static constexpr const size_t maxBytes = 37;
    static bool failed = true;
    static UUID uuid = { 0, 0 };
    static std::once_flag once;

    std::call_once(once, [] {
        // Always soft-fail, which will simply disable split testing.

        char userUUID[maxBytes] = { '\0' };
        CFStringRef (*softLinkCRGetUserUUID)(void) = (CFStringRef (*)(void)) dlsym(CrashReporterSupportLibrary(), "CRGetUserUUID");
        if (!softLinkCRGetUserUUID)
            return;

        CFStringRef userUUIDString = CRGetUserUUID();
        CFIndex length = CFStringGetLength(userUUIDString);
        CFRange range = CFRangeMake(0, length);
        CFIndex numConvertedCharacters = CFStringGetBytes(userUUIDString, range, kCFStringEncodingASCII, 0, false, reinterpret_cast<UInt8*>(userUUID), maxBytes - 1, nullptr);
        if (numConvertedCharacters <= 0 || static_cast<size_t>(numConvertedCharacters) >= maxBytes)
            return;

        size_t nibblesDecoded = 0;
        for (size_t position = 0; position < maxBytes; ++position) {
            uint64_t nibble;
            int c = toASCIILowerUnchecked(userUUID[position]);

            if (c >= 'a' && c <= 'f')
                nibble = c - 'a' + 10;
            else if (c >= '0' && c <= '9')
                nibble = c - '0';
            else
                continue; // Ignore non-hexdecimal ASCII nibbles.

            if (nibblesDecoded < 64 / 4) {
                uuid.lo <<= 4;
                uuid.lo |= nibble;
            } else {
                uuid.hi <<= 4;
                uuid.hi |= nibble;
            }

            ++nibblesDecoded;
        }

        failed = false;
    });

    return failed ? std::optional<UUID>() : uuid;
}

} // anonymous namespace

void initSplitTest()
{
    std::optional<UUID> uuid = getUUID();
    if (!uuid)
        return;

    SplitTest::singleton().initializeSingleton(uuid->lo, uuid->hi);
}

#else

void initSplitTest()
{
    // Don't do split testing on other platforms.
}

#endif
