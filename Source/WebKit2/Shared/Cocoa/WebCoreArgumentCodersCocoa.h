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

#include <WebCore/PaymentHeaders.h>

namespace IPC {

template<> struct ArgumentCoder<WebCore::Payment> {
    static void encode(ArgumentEncoder&, const WebCore::Payment&);
    static bool decode(ArgumentDecoder&, WebCore::Payment&);
};

template<> struct ArgumentCoder<WebCore::PaymentContact> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentContact&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentContact&);
};

template<> struct ArgumentCoder<WebCore::PaymentMerchantSession> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentMerchantSession&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentMerchantSession&);
};

template<> struct ArgumentCoder<WebCore::PaymentMethod> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentMethod&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentMethod&);
};

template<> struct ArgumentCoder<WebCore::PaymentRequest> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentRequest&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentRequest&);
};

template<> struct ArgumentCoder<WebCore::PaymentRequest::AddressFields> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentRequest::AddressFields&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentRequest::AddressFields&);
};

template<> struct ArgumentCoder<WebCore::PaymentRequest::LineItem> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentRequest::LineItem&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentRequest::LineItem&);
};

template<> struct ArgumentCoder<WebCore::PaymentRequest::MerchantCapabilities> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentRequest::MerchantCapabilities&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentRequest::MerchantCapabilities&);
};

template<> struct ArgumentCoder<WebCore::PaymentRequest::ShippingMethod> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentRequest::ShippingMethod&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentRequest::ShippingMethod&);
};

template<> struct ArgumentCoder<WebCore::PaymentRequest::SupportedNetworks> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentRequest::SupportedNetworks&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentRequest::SupportedNetworks&);
};

template<> struct ArgumentCoder<WebCore::PaymentRequest::TotalAndLineItems> {
    static void encode(ArgumentEncoder&, const WebCore::PaymentRequest::TotalAndLineItems&);
    static bool decode(ArgumentDecoder&, WebCore::PaymentRequest::TotalAndLineItems&);
};

}

#endif
