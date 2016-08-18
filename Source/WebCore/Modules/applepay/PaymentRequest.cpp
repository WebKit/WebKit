/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "PaymentRequest.h"

#if ENABLE(APPLE_PAY)

#include "SoftLinking.h"

namespace WebCore {

PaymentRequest::PaymentRequest()
{
}

PaymentRequest::~PaymentRequest()
{
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/PaymentRequestAdditions.cpp>)
#include <WebKitAdditions/PaymentRequestAdditions.cpp>
#else
static inline bool isAdditionalValidSupportedNetwork(unsigned, const String&)
{
    return false;
}
#endif

bool PaymentRequest::isValidSupportedNetwork(unsigned version, const String& supportedNetwork)
{
    if (supportedNetwork == "amex")
        return true;
    if (supportedNetwork == "chinaUnionPay")
        return true;
    if (supportedNetwork == "discover")
        return true;
    if (supportedNetwork == "interac")
        return true;
    if (supportedNetwork == "masterCard")
        return true;
    if (supportedNetwork == "privateLabel")
        return true;
    if (supportedNetwork == "visa")
        return true;

    return isAdditionalValidSupportedNetwork(version, supportedNetwork);
}

}

#endif
