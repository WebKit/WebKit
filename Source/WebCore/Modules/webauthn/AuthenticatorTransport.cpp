/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AuthenticatorTransport.h"

#if ENABLE(WEB_AUTHN)

#include "JSAuthenticatorTransport.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

std::optional<AuthenticatorTransport> toAuthenticatorTransport(const String& transport)
{
    if (transport == "usb"_s)
        return AuthenticatorTransport::Usb;
    if (transport == "nfc"_s)
        return AuthenticatorTransport::Nfc;
    if (transport == "ble"_s)
        return AuthenticatorTransport::Ble;
    if (transport == "internal"_s)
        return AuthenticatorTransport::Internal;
    if (transport == "cable"_s)
        return AuthenticatorTransport::Cable;
    if (transport == "hybrid"_s)
        return AuthenticatorTransport::Hybrid;
    if (transport == "smart-card"_s)
        return AuthenticatorTransport::SmartCard;
    return std::nullopt;
}

String toString(AuthenticatorTransport transport)
{
    return convertEnumerationToString(transport);
}

}

#endif
