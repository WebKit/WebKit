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
#include <wtf/text/StringToIntegerConversion.h>

#if OS(DARWIN)
#include <sys/sysctl.h>
#endif

namespace WTF {

UUID::UUID()
{
    static_assert(sizeof(m_data) == 16);
    auto* data = reinterpret_cast<unsigned char*>(&m_data);

    do {
        cryptographicallyRandomValues(data, 16);
    } while (m_data == emptyValue || m_data == deletedValue);

    // We sanitize the value to not loose any information when serializing as Version 4 UUID.
    auto high = static_cast<uint64_t>((m_data >> 64) & 0xffffffffffff0fff);
    auto low = static_cast<uint64_t>(m_data & 0x3fffffffffffffff);

    m_data = (static_cast<UInt128>(high) << 64) | low;
}

unsigned UUID::hash() const
{
    return StringHasher::hashMemory(reinterpret_cast<const unsigned char*>(&m_data), 16);
}

String UUID::toString() const
{
    auto high = static_cast<uint64_t>(m_data >> 64);
    auto low = static_cast<uint64_t>(m_data & 0x3fffffffffffffff);

    // Format as Version 4 UUID.
    return makeString(
        hex(high >> 32, 8, Lowercase),
        '-',
        hex((high >> 16) & 0xffff, 4, Lowercase),
        "-4",
        hex(high & 0xfff, 3, Lowercase),
        '-',
        hex((low >> 48) | 0x8000, 4, Lowercase),
        '-',
        hex(low & 0xffffffffffff, 12, Lowercase)
    );
}

std::optional<UUID> UUID::parse(StringView value)
{
    // Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx with hexadecimal digits for x and one of 8, 9, A, or B for y.
    if (value.length() != 36)
        return { };

    if (value[8] != '-' || value[13] != '-'  || value[14] != '4' || value[18] != '-' || value[23] != '-')
        return { };

    // parseInteger may accept integers starting with +, let's check this beforehand.
    if (value[0] == '+' || value[9] == '+'  || value[19] == '+' || value[24] == '+')
        return { };

    auto firstValue = parseInteger<uint64_t>(value.substring(0, 8), 16);
    if (!firstValue)
        return { };

    auto secondValue = parseInteger<uint64_t>(value.substring(9, 4), 16);
    if (!secondValue)
        return { };

    auto thirdValue = parseInteger<uint64_t>(value.substring(15, 3), 16);
    if (!thirdValue)
        return { };

    auto fourthValue = parseInteger<uint64_t>(value.substring(19, 4), 16);
    if (!fourthValue)
        return { };

    // Fourth value starts with 'y', it must be above 0x8000 and below 0xBFFFF.
    if ((*fourthValue & 0xc000) != 0x8000)
        return { };
    fourthValue = *fourthValue & 0x3fff;

    auto fifthValue = parseInteger<uint64_t>(value.substring(24, 12), 16);
    if (!fifthValue)
        return { };

    uint64_t high = (*firstValue << 32) | (*secondValue << 16) | *thirdValue;
    uint64_t low = (*fourthValue << 48) | *fifthValue;

    auto result = (static_cast<UInt128>(high) << 64) | low;
    if (result == emptyValue || result == deletedValue)
        return { };

    return UUID(result);
}

String createCanonicalUUIDString()
{
    return UUID::create().toString();
}

String bootSessionUUIDString()
{
#if OS(DARWIN)
    static LazyNeverDestroyed<String> bootSessionUUID;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        constexpr size_t maxUUIDLength = 37;
        char uuid[maxUUIDLength];
        size_t uuidLength = maxUUIDLength;
        if (sysctlbyname("kern.bootsessionuuid", uuid, &uuidLength, nullptr, 0))
            return;
        bootSessionUUID.construct(static_cast<const char*>(uuid), uuidLength - 1);
    });
    return bootSessionUUID;
#else
    return String();
#endif
}

bool isVersion4UUID(StringView value)
{
    return !!UUID::parse(value);
}

} // namespace WTF
