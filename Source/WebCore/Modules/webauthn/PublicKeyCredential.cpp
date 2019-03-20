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

#include "config.h"
#include "PublicKeyCredential.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorAssertionResponse.h"
#include "AuthenticatorAttestationResponse.h"
#include "AuthenticatorCoordinator.h"
#include "AuthenticatorResponse.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "PublicKeyCredentialData.h"
#include <wtf/text/Base64.h>

namespace WebCore {

RefPtr<PublicKeyCredential> PublicKeyCredential::tryCreate(const PublicKeyCredentialData& data)
{
    if (!data.rawId || !data.clientDataJSON)
        return nullptr;

    if (data.isAuthenticatorAttestationResponse) {
        if (!data.attestationObject)
            return nullptr;

        return adoptRef(*new PublicKeyCredential(data.rawId.releaseNonNull(), AuthenticatorAttestationResponse::create(data.clientDataJSON.releaseNonNull(), data.attestationObject.releaseNonNull()), { data.appid }));
    }

    if (!data.authenticatorData || !data.signature)
        return nullptr;

    return adoptRef(*new PublicKeyCredential(data.rawId.releaseNonNull(), AuthenticatorAssertionResponse::create(data.clientDataJSON.releaseNonNull(), data.authenticatorData.releaseNonNull(), data.signature.releaseNonNull(), WTFMove(data.userHandle)), { data.appid }));
}

PublicKeyCredential::PublicKeyCredential(Ref<ArrayBuffer>&& id, Ref<AuthenticatorResponse>&& response, AuthenticationExtensionsClientOutputs&& extensions)
    : BasicCredential(WTF::base64URLEncode(id->data(), id->byteLength()), Type::PublicKey, Discovery::Remote)
    , m_rawId(WTFMove(id))
    , m_response(WTFMove(response))
    , m_extensions(WTFMove(extensions))
{
}

PublicKeyCredential::AuthenticationExtensionsClientOutputs PublicKeyCredential::getClientExtensionResults() const
{
    return m_extensions;
}

void PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(Document& document, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    document.page()->authenticatorCoordinator().isUserVerifyingPlatformAuthenticatorAvailable(WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
