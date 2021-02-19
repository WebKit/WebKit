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

#if ENABLE(APPLE_PAY)

#include "ApplePayLineItemData.h"
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ApplePayLineItem final : public ApplePayLineItemData {
    enum class Type : bool {
        Pending,
        Final,
    };

    Type type { Type::Final };
    String label;
    String amount;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ApplePayLineItem> decode(Decoder&);
};

template<class Encoder>
void ApplePayLineItem::encode(Encoder& encoder) const
{
    ApplePayLineItemData::encode(encoder);
    encoder << type;
    encoder << label;
    encoder << amount;
}

template<class Decoder>
Optional<ApplePayLineItem> ApplePayLineItem::decode(Decoder& decoder)
{
    ApplePayLineItem result;

    if (!result.decodeData(decoder))
        return WTF::nullopt;

    Optional<Type> type;
    decoder >> type;
    if (!type)
        return WTF::nullopt;
    result.type = WTFMove(*type);

    Optional<String> label;
    decoder >> label;
    if (!label)
        return WTF::nullopt;
    result.label = WTFMove(*label);

    Optional<String> amount;
    decoder >> amount;
    if (!amount)
        return WTF::nullopt;
    result.amount = WTFMove(*amount);

    return result;
}

} // namespace WebCore

#endif
