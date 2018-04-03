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

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

struct ExceptionData;
struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialRequestOptions;

using CreationCallback = Function<void(const Vector<uint8_t>&, const Vector<uint8_t>&)>;
using RequestCallback = Function<void(const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&)>;
using ExceptionCallback = Function<void(const WebCore::ExceptionData&)>;

typedef void (^CompletionBlock)(SecKeyRef _Nullable referenceKey, NSArray * _Nullable certificates, NSError * _Nullable error);

// FIXME(182769): LocalAuthenticator should belongs to WebKit. However, we need unit tests.
class WEBCORE_EXPORT LocalAuthenticator {
    WTF_MAKE_NONCOPYABLE(LocalAuthenticator);
public:
    LocalAuthenticator();
    virtual ~LocalAuthenticator() = default;

    void makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions&, CreationCallback&&, ExceptionCallback&&);
    void getAssertion(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions&, RequestCallback&&, ExceptionCallback&&);
    bool isAvailable() const;

protected:
    // Apple Attestation is moved into this virtual method such that it can be overrided by self attestation for testing.
    virtual void issueClientCertificate(const String& rpId, const String& username, const Vector<uint8_t>& hash, CompletionBlock _Nonnull) const;

    WeakPtrFactory<LocalAuthenticator> m_weakFactory;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
