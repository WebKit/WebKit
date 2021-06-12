/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMSessionType.h"
#include <wtf/HashSet.h>

namespace WebCore {

struct CDMRestrictions {
    bool distinctiveIdentifierDenied { false };
    bool persistentStateDenied { false };
    HashSet<CDMSessionType, WTF::IntHash<CDMSessionType>, WTF::StrongEnumHashTraits<CDMSessionType>> deniedSessionTypes;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << distinctiveIdentifierDenied;
        encoder << persistentStateDenied;
        encoder << deniedSessionTypes;
    }

    template <class Decoder>
    static std::optional<CDMRestrictions> decode(Decoder& decoder)
    {
        std::optional<bool> distinctiveIdentifierDenied;
        decoder >> distinctiveIdentifierDenied;
        if (!distinctiveIdentifierDenied)
            return std::nullopt;

        std::optional<bool> persistentStateDenied;
        decoder >> persistentStateDenied;
        if (!persistentStateDenied)
            return std::nullopt;

        std::optional<HashSet<CDMSessionType, WTF::IntHash<CDMSessionType>, WTF::StrongEnumHashTraits<CDMSessionType>>> deniedSessionTypes;
        decoder >> deniedSessionTypes;
        if (!deniedSessionTypes)
            return std::nullopt;

        return {{
            *distinctiveIdentifierDenied,
            *persistentStateDenied,
            WTFMove(*deniedSessionTypes),
        }};
    }
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
