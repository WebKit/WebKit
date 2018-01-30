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

#include "config.h"
#include "AuthenticatorAssertionResponse.h"

#if ENABLE(WEB_AUTHN)

namespace WebCore {

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(RefPtr<ArrayBuffer>&& clientDataJSON, RefPtr<ArrayBuffer>&& authenticatorData, RefPtr<ArrayBuffer>&& signature, RefPtr<ArrayBuffer>&& userHandle)
    : AuthenticatorResponse(WTFMove(clientDataJSON))
    , m_authenticatorData(WTFMove(authenticatorData))
    , m_signature(WTFMove(signature))
    , m_userHandle(WTFMove(userHandle))
{
}

ArrayBuffer* AuthenticatorAssertionResponse::authenticatorData() const
{
    return m_authenticatorData.get();
}

ArrayBuffer* AuthenticatorAssertionResponse::signature() const
{
    return m_signature.get();
}

ArrayBuffer* AuthenticatorAssertionResponse::userHandle() const
{
    return m_userHandle.get();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
