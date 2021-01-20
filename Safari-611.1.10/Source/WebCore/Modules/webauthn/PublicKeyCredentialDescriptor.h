/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorTransport.h"
#include "BufferSource.h"
#include "PublicKeyCredentialType.h"

namespace WebCore {

struct PublicKeyCredentialDescriptor {
    PublicKeyCredentialType type;
    BufferSource id; // id becomes idVector once it is passed to UIProcess.
    Vector<uint8_t> idVector;
    Vector<AuthenticatorTransport> transports;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<PublicKeyCredentialDescriptor> decode(Decoder&);
};

template<class Encoder>
void PublicKeyCredentialDescriptor::encode(Encoder& encoder) const
{
    encoder << type;
    encoder << static_cast<uint64_t>(id.length());
    encoder.encodeFixedLengthData(id.data(), id.length(), 1);
    encoder << transports;
}

template<class Decoder>
Optional<PublicKeyCredentialDescriptor> PublicKeyCredentialDescriptor::decode(Decoder& decoder)
{
    PublicKeyCredentialDescriptor result;
    if (!decoder.decode(result.type))
        return WTF::nullopt;
    if (!decoder.decode(result.idVector))
        return WTF::nullopt;
    if (!decoder.decode(result.transports))
        return WTF::nullopt;
    return result;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
