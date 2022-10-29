/*
* Copyright (C) 2010 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/UUID.h>

#include <mutex>
#include <wtf/ASCIICType.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HexNumber.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WeakRandom.h>
#include <wtf/text/StringToIntegerConversion.h>

#if OS(DARWIN)
#include <sys/sysctl.h>
#endif

namespace WTF {

static ALWAYS_INLINE UInt128 convertRandomUInt128ToUUIDVersion4(UInt128 buffer)
{
    // By default, we generate a v4 UUID value, as per https://datatracker.ietf.org/doc/html/rfc4122#section-4.4.
    auto high = static_cast<uint64_t>((buffer >> 64) & 0xffffffffffff0fff) | 0x4000;
    auto low = static_cast<uint64_t>(buffer & 0x3fffffffffffffff) | 0x8000000000000000;

    return (static_cast<UInt128>(high) << 64) | low;
}

static UInt128 generateCryptographicallyRandomUUIDVersion4()
{
    UInt128 buffer { };
    static_assert(sizeof(buffer) == 16);
    cryptographicallyRandomValues(reinterpret_cast<unsigned char*>(&buffer), 16);
    return convertRandomUInt128ToUUIDVersion4(buffer);
}

UInt128 UUID::generateWeakRandomUUIDVersion4()
{
    static Lock lock;
    UInt128 buffer { 0 };
    {
        Locker locker { lock };
        static std::optional<WeakRandom> weakRandom;
        if (!weakRandom)
            weakRandom.emplace();
        buffer = static_cast<UInt128>(weakRandom->getUint64()) << 64 | weakRandom->getUint64();
    }
    return convertRandomUInt128ToUUIDVersion4(buffer);
}

UUID::UUID()
    : m_data(generateCryptographicallyRandomUUIDVersion4())
{
}

String UUID::toString() const
{
    return makeString(*this);
}

std::optional<UUID> UUID::parse(StringView value)
{
    // UUIDs have the form xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx with hexadecimal digits for x.
    if (value.length() != 36)
        return { };

    if (value[8] != '-' || value[13] != '-'  || value[18] != '-' || value[23] != '-')
        return { };

    // parseInteger may accept integers starting with +, let's check this beforehand.
    if (value[0] == '+' || value[9] == '+'  || value[19] == '+' || value[24] == '+')
        return { };

    auto firstValue = parseInteger<uint64_t>(value.left(8), 16);
    if (!firstValue)
        return { };

    auto secondValue = parseInteger<uint64_t>(value.substring(9, 4), 16);
    if (!secondValue)
        return { };

    auto thirdValue = parseInteger<uint64_t>(value.substring(14, 4), 16);
    if (!thirdValue)
        return { };

    auto fourthValue = parseInteger<uint64_t>(value.substring(19, 4), 16);
    if (!fourthValue)
        return { };

    auto fifthValue = parseInteger<uint64_t>(value.substring(24, 12), 16);
    if (!fifthValue)
        return { };

    uint64_t high = (*firstValue << 32) | (*secondValue << 16) | *thirdValue;
    uint64_t low = (*fourthValue << 48) | *fifthValue;

    auto result = (static_cast<UInt128>(high) << 64) | low;
    if (result == deletedValue)
        return { };

    return UUID(result);
}

std::optional<UUID> UUID::parseVersion4(StringView value)
{
    auto uuid = parse(value);
    if (!uuid)
        return { };

    // Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx with hexadecimal digits for x and one of 8, 9, A, or B for y.
    auto high = static_cast<uint64_t>(uuid->m_data >> 64);
    if ((high & 0xf000) != 0x4000)
        return { };

    auto low = static_cast<uint64_t>(uuid->m_data & 0xffffffffffffffff);
    if ((low >> 62) != 2)
        return { };

    return uuid;
}

String createVersion4UUIDString()
{
    return makeString(UUID::createVersion4());
}

String bootSessionUUIDString()
{
// #if OS(DARWIN)
//     static LazyNeverDestroyed<String> bootSessionUUID;
//     static std::once_flag onceKey;
//     std::call_once(onceKey, [] {
//         constexpr size_t maxUUIDLength = 37;
//         char uuid[maxUUIDLength];
//         size_t uuidLength = maxUUIDLength;
//         if (sysctlbyname("kern.bootsessionuuid", uuid, &uuidLength, nullptr, 0))
//             return;
//         bootSessionUUID.construct(static_cast<const char*>(uuid), uuidLength - 1);
//     });
//     return bootSessionUUID;
// #else
    return String();
// #endif
}

bool isVersion4UUID(StringView value)
{
    return !!UUID::parseVersion4(value);
}

} // namespace WTF
