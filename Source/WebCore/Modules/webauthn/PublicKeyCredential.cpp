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

#include "AuthenticatorCoordinator.h"
#include "AuthenticatorResponse.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/text/Base64.h>

namespace WebCore {

Ref<PublicKeyCredential> PublicKeyCredential::create(Ref<AuthenticatorResponse>&& response)
{
    return adoptRef(*new PublicKeyCredential(WTFMove(response)));
}

ArrayBuffer* PublicKeyCredential::rawId() const
{
    return m_response->rawId();
}

AuthenticationExtensionsClientOutputs PublicKeyCredential::getClientExtensionResults() const
{
    return m_response->extensions();
}

PublicKeyCredential::PublicKeyCredential(Ref<AuthenticatorResponse>&& response)
    : BasicCredential(WTF::base64URLEncode(response->rawId()->data(), response->rawId()->byteLength()), Type::PublicKey, Discovery::Remote)
    , m_response(WTFMove(response))
{
}

void PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(Document& document, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    if (!document.settings().webAuthenticationLocalAuthenticatorEnabled()) {
        promise.resolve(false);
        return;
    }
    document.page()->authenticatorCoordinator().isUserVerifyingPlatformAuthenticatorAvailable(WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
