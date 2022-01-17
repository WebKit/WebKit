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

    // We sanitize the value so as not loosing any data when going to Version 4 UUIDs.
    data[5] &= 0x0f;
    data[11] &= 0xcf;
}

unsigned UUID::hash() const
{
    return StringHasher::hashMemory(reinterpret_cast<const unsigned char*>(&m_data), 16);
}

String UUID::toString() const
{
    auto* randomData = reinterpret_cast<const unsigned*>(&m_data);

    // Format as Version 4 UUID.
    return makeString(
        hex(randomData[0], 8, Lowercase),
        '-',
        hex(randomData[1] >> 16, 4, Lowercase),
        "-4",
        hex(randomData[1] & 0x00000fff, 3, Lowercase),
        '-',
        hex((randomData[2] >> 30) | 0x8, 1, Lowercase),
        hex((randomData[2] >> 16) & 0x00000fff, 3, Lowercase),
        '-',
        hex(randomData[2] & 0x0000ffff, 4, Lowercase),
        hex(randomData[3], 8, Lowercase)
    );
}

std::optional<UUID> UUID::parse(StringView value)
{
    // Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx with hexadecimal digits for x and one of 8, 9, A, or B for y.
    if (value.length() != 36)
        return { };

    if (value[8] != '-' || value[13] != '-'  || value[14] != '4' || value[18] != '-' || value[23] != '-')
        return { };

    unsigned y = value[19];
    if (y == '8' || y == '9')
        y = y - '8';
    else if (y == 'a' || y == 'A')
        y = 2;
    else if (y == 'b' || y == 'B')
        y = 3;
    else
        return { };

    auto firstValue = parseInteger<unsigned>(value.substring(0, 8), 16);
    if (!firstValue)
        return { };

    auto secondValue = parseInteger<unsigned>(value.substring(9, 4), 16);
    if (!secondValue)
        return { };

    auto thirdValue = parseInteger<unsigned>(value.substring(15, 3), 16);
    if (!thirdValue)
        return { };

    auto fourthValue = parseInteger<unsigned>(value.substring(20, 3), 16);
    if (!fourthValue)
        return { };

    auto fifthValue = parseInteger<unsigned>(value.substring(24, 4), 16);
    if (!fifthValue)
        return { };

    auto sixthValue = parseInteger<unsigned>(value.substring(28, 8), 16);
    if (!sixthValue)
        return { };

    UInt128 uuidValue;

    auto* data = reinterpret_cast<unsigned*>(&uuidValue);
    data[0] = *firstValue;
    data[1] = (*secondValue << 16) | *thirdValue;
    data[2] = ((*fourthValue << 16) | *fifthValue) + (y << 30);
    data[3] = *sixthValue;

    return UUID(WTFMove(uuidValue));
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
