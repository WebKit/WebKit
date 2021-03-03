/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY)

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/ApplePayDetailsUpdateDataAdditions.h>
#endif

namespace WebCore {

struct ApplePayDetailsUpdateData {
#if defined(ApplePayDetailsUpdateDataAdditions_members)
    ApplePayDetailsUpdateDataAdditions_members
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ApplePayDetailsUpdateData> decode(Decoder&);

    template<class Decoder> WARN_UNUSED_RETURN bool decodeData(Decoder&);
};

template<class Encoder>
void ApplePayDetailsUpdateData::encode(Encoder& encoder) const
{
#if defined(ApplePayDetailsUpdateDataAdditions_encode)
    ApplePayDetailsUpdateDataAdditions_encode
#else
    UNUSED_PARAM(encoder);
#endif
}

template<class Decoder>
Optional<ApplePayDetailsUpdateData> ApplePayDetailsUpdateData::decode(Decoder& decoder)
{
    ApplePayDetailsUpdateData result;
    if (!result.decodeData(decoder))
        return WTF::nullopt;
    return result;
}

template<class Decoder>
bool ApplePayDetailsUpdateData::decodeData(Decoder& decoder)
{
#define DECODE(name, type) \
    Optional<type> name; \
    decoder >> name; \
    if (!name) \
        return false; \
    this->name = WTFMove(*name); \

#if defined(ApplePayDetailsUpdateDataAdditions_decodeData)
    ApplePayDetailsUpdateDataAdditions_decodeData
#else
    UNUSED_PARAM(decoder);
#endif

#undef DECODE

    return true;
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
