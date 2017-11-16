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

#pragma once

#include <wtf/text/WTFString.h>

// FIXME: Remove this messy fallback path and require GCC 6 in May 2018.
#if COMPILER(GCC)
#if !GCC_VERSION_AT_LEAST(6, 0, 0)
#define NEED_FOURCC_LEGACY_CONSTRUCTOR
#include <cstring>
#endif
#endif

namespace WebCore {

struct FourCC {
    WEBCORE_EXPORT FourCC(uint32_t value) : value(value) { }

#ifdef NEED_FOURCC_LEGACY_CONSTRUCTOR
    // This constructor is risky because it creates ambiguous function
    // calls that will not exist except with old versions of GCC: the
    // initialization FourCC { 0 } is valid only when this legacy
    // constructor is not enabled, because the uint32_t constructor and
    // this constructor are equally-appropriate options. So the more
    // verbose initialization FourCC { uint32_t { 0 } } is required, but
    // developers will not realize this unless they use an old version of
    // GCC. Bad.
    FourCC(const char* data)
    {
        if (!data) {
            value = 0;
            return;
        }
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(strlen(data) == 4);
        value = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    }
#undef NEED_FOURCC_LEGACY_CONSTRUCTOR
#else
    template<std::size_t N>
    constexpr FourCC(const char (&data)[N])
    {
        static_assert((N - 1) == 4, "FourCC literals must be exactly 4 characters long");
        value = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    }
#endif

    String toString() const;
    WEBCORE_EXPORT static std::optional<FourCC> fromString(const String&);

    bool operator==(const FourCC& other) const { return value == other.value; }
    bool operator!=(const FourCC& other) const { return value != other.value; }

    uint32_t value;
};

}
