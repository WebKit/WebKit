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

#include "ApplePayDateComponentsRange.h"
#include <optional>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ApplePayShippingMethod final {
    String label;
    String detail;
    String amount;
    String identifier;

#if ENABLE(APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
    std::optional<ApplePayDateComponentsRange> dateComponentsRange;
#endif

#if ENABLE(APPLE_PAY_SELECTED_SHIPPING_METHOD)
    bool selected { false };
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ApplePayShippingMethod> decode(Decoder&);
};

template<class Encoder>
void ApplePayShippingMethod::encode(Encoder& encoder) const
{
    encoder << label;
    encoder << detail;
    encoder << amount;
    encoder << identifier;
#if ENABLE(APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
    encoder << dateComponentsRange;
#endif
#if ENABLE(APPLE_PAY_SELECTED_SHIPPING_METHOD)
    encoder << selected;
#endif
}

template<class Decoder>
std::optional<ApplePayShippingMethod> ApplePayShippingMethod::decode(Decoder& decoder)
{
#define DECODE(name, type) \
    std::optional<type> name; \
    decoder >> name; \
    if (!name) \
        return std::nullopt; \

    DECODE(label, String)
    DECODE(detail, String)
    DECODE(amount, String)
    DECODE(identifier, String)
#if ENABLE(APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
    DECODE(dateComponentsRange, std::optional<ApplePayDateComponentsRange>)
#endif
#if ENABLE(APPLE_PAY_SELECTED_SHIPPING_METHOD)
    DECODE(selected, bool)
#endif

#undef DECODE

    return { {
        WTFMove(*label),
        WTFMove(*detail),
        WTFMove(*amount),
        WTFMove(*identifier),
#if ENABLE(APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
        WTFMove(*dateComponentsRange),
#endif
#if ENABLE(APPLE_PAY_SELECTED_SHIPPING_METHOD)
        WTFMove(*selected),
#endif
    } };
}

} // namespace WebCore

#endif
