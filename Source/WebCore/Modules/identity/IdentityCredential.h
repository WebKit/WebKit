/*
 * Copyright (C) 2026 Shopify Inc. All rights reserved.
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

#if ENABLE(FEDCM)

#include <WebCore/BasicCredential.h>
#include <WebCore/JSDOMPromiseDeferredForward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
struct CredentialRequestOptions;
struct IdentityCredentialCreateOptions;
using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class IdentityCredential final : public BasicCredential {
public:
    static Ref<IdentityCredential> create(const String& token, bool isAutoSelected);

    virtual ~IdentityCredential();

    const String& token() const { return m_token; }
    bool isAutoSelected() const { return m_isAutoSelected; }

    static void discoverFromExternalSource(const Document&, CredentialPromise&&, CredentialRequestOptions&&);
    static void createIdentityCredential(const Document&, CredentialPromise&&, IdentityCredentialCreateOptions&&);

private:
    IdentityCredential(const String& token, bool isAutoSelected);

    Type credentialType() const final { return Type::Identity; }

    String m_token;
    bool m_isAutoSelected { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_CREDENTIAL(IdentityCredential, BasicCredential::Type::Identity)

#endif // ENABLE(FEDCM)
