/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "AuthenticatorResponse.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorAssertionResponse.h"
#include "AuthenticatorAttestationResponse.h"
#include "AuthenticatorResponseData.h"

namespace WebCore {

RefPtr<AuthenticatorResponse> AuthenticatorResponse::tryCreate(AuthenticatorResponseData&& data, AuthenticatorAttachment attachment)
{
    if (!data.rawId)
        return nullptr;

    if (data.isAuthenticatorAttestationResponse) {
        if (!data.attestationObject)
            return nullptr;

        auto response = AuthenticatorAttestationResponse::create(data.rawId.releaseNonNull(), data.attestationObject.releaseNonNull(), attachment, WTFMove(data.transports));
        if (data.extensionOutputs)
            response->setExtensions(WTFMove(*data.extensionOutputs));
        response->setClientDataJSON(data.clientDataJSON.releaseNonNull());
        return WTFMove(response);
    }

    if (!data.authenticatorData || !data.signature)
        return nullptr;

    Ref response = AuthenticatorAssertionResponse::create(data.rawId.releaseNonNull(), data.authenticatorData.releaseNonNull(), data.signature.releaseNonNull(), WTFMove(data.userHandle), WTFMove(data.extensionOutputs), attachment);
    response->setClientDataJSON(data.clientDataJSON.releaseNonNull());
    return WTFMove(response);
}

AuthenticatorResponseData AuthenticatorResponse::data() const
{
    AuthenticatorResponseData data;
    data.rawId = m_rawId.copyRef();
    data.extensionOutputs = m_extensions;
    data.clientDataJSON = m_clientDataJSON.copyRef();
    return data;
}

void AuthenticatorResponse::setExtensions(AuthenticationExtensionsClientOutputs&& extensions)
{
    m_extensions = WTFMove(extensions);
}

AuthenticationExtensionsClientOutputs AuthenticatorResponse::extensions() const
{
    auto result = m_extensions;

    // Clone ArrayBuffers to prevent detachment issues
    if (result.prf && result.prf->results) {
        if (result.prf->results->first)
            result.prf->results->first = ArrayBuffer::tryCreate(result.prf->results->first.get()->span());
        if (result.prf->results->second)
            result.prf->results->second = ArrayBuffer::tryCreate(result.prf->results->second.get()->span());
    }

    if (result.largeBlob && result.largeBlob->blob)
        result.largeBlob->blob = ArrayBuffer::tryCreate(result.largeBlob->blob.get()->span());

    return result;
}

void AuthenticatorResponse::setClientDataJSON(Ref<ArrayBuffer>&& clientDataJSON)
{
    m_clientDataJSON = WTFMove(clientDataJSON);
}

ArrayBuffer* AuthenticatorResponse::clientDataJSON() const
{
    return m_clientDataJSON.get();
}

AuthenticatorAttachment AuthenticatorResponse::attachment() const
{
    return m_attachment;
}

AuthenticatorResponse::AuthenticatorResponse(Ref<ArrayBuffer>&& rawId, AuthenticatorAttachment attachment)
    : m_rawId(WTFMove(rawId))
    , m_attachment(attachment)
{
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
