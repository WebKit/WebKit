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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "BasicCredential.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/Forward.h>

namespace WebCore {

class AuthenticatorResponse;

class PublicKeyCredential final : public BasicCredential {
public:
    static Ref<PublicKeyCredential> create(RefPtr<ArrayBuffer>&& id, RefPtr<AuthenticatorResponse>&& response)
    {
        return adoptRef(*new PublicKeyCredential(WTFMove(id), WTFMove(response)));
    }

    ArrayBuffer* rawId() const { return m_rawId.get(); }
    AuthenticatorResponse* response() const { return m_response.get(); }
    // Not support yet. Always throws.
    ExceptionOr<bool> getClientExtensionResults() const;

    static void isUserVerifyingPlatformAuthenticatorAvailable(DOMPromiseDeferred<IDLBoolean>&&);

private:
    PublicKeyCredential(RefPtr<ArrayBuffer>&& id, RefPtr<AuthenticatorResponse>&&);

    Type credentialType() const final { return Type::PublicKey; }

    RefPtr<ArrayBuffer> m_rawId;
    RefPtr<AuthenticatorResponse> m_response;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_CREDENTIAL(PublicKeyCredential, BasicCredential::Type::PublicKey)

#endif // ENABLE(WEB_AUTHN)
