/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "DigitalCredential.h"

#include <JavaScriptCore/Uint8Array.h>
#include <wtf/text/Base64.h>

#if ENABLE(WEB_AUTHN)

namespace WebCore {

Ref<DigitalCredential> DigitalCredential::create(Ref<Uint8Array>&& data, IdentityCredentialProtocol protocol)
{
    return adoptRef(*new DigitalCredential(WTFMove(data), protocol));
}

DigitalCredential::~DigitalCredential() = default;

DigitalCredential::DigitalCredential(Ref<Uint8Array>&& data, IdentityCredentialProtocol protocol)
    : BasicCredential(base64URLEncodeToString(data->span()), Type::DigitalCredential, Discovery::CredentialStore)
    , m_protocol(protocol)
    , m_data(WTFMove(data))
{
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
