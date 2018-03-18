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

#include "ExceptionData.h"
#include "ExceptionOr.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;

struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialRequestOptions;

struct CreationReturnBundle {
    CreationReturnBundle(Ref<ArrayBuffer>&& credentialId, Ref<ArrayBuffer>&& attestationObject)
        : credentialId(WTFMove(credentialId))
        , attestationObject(WTFMove(attestationObject))
    {
    }

    Ref<ArrayBuffer> credentialId;
    Ref<ArrayBuffer> attestationObject;
};
struct AssertionReturnBundle {
    AssertionReturnBundle(Ref<ArrayBuffer>&& credentialId, Ref<ArrayBuffer>&& authenticatorData, Ref<ArrayBuffer>&& signature, Ref<ArrayBuffer>&& userHandle)
        : credentialId(WTFMove(credentialId))
        , authenticatorData(WTFMove(authenticatorData))
        , signature(WTFMove(signature))
        , userHandle(WTFMove(userHandle))
    {
    }

    Ref<ArrayBuffer> credentialId;
    Ref<ArrayBuffer> authenticatorData;
    Ref<ArrayBuffer> signature;
    Ref<ArrayBuffer> userHandle;
};

using CreationCompletionHandler = CompletionHandler<void(ExceptionOr<CreationReturnBundle>&&)>;
using RequestCompletionHandler = CompletionHandler<void(ExceptionOr<AssertionReturnBundle>&&)>;
using QueryCompletionHandler = CompletionHandler<void(bool)>;

class CredentialsMessenger {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CredentialsMessenger);
public:
    CredentialsMessenger() = default;

    // Senders.
    virtual void makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions&, CreationCompletionHandler&&) = 0;
    virtual void getAssertion(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions&, RequestCompletionHandler&&) = 0;
    virtual void isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&&) = 0;

    // Receivers.
    WEBCORE_EXPORT void exceptionReply(uint64_t messageId, const ExceptionData&);
    virtual void makeCredentialReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& attestationObject) = 0;
    virtual void getAssertionReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle) = 0;
    virtual void isUserVerifyingPlatformAuthenticatorAvailableReply(uint64_t messageId, bool) = 0;

    auto& weakPtrFactory() const { return m_weakFactory; }

protected:
    virtual ~CredentialsMessenger() = default;

    WEBCORE_EXPORT uint64_t addCreationCompletionHandler(CreationCompletionHandler&&);
    WEBCORE_EXPORT CreationCompletionHandler takeCreationCompletionHandler(uint64_t);
    WEBCORE_EXPORT uint64_t addRequestCompletionHandler(RequestCompletionHandler&&);
    WEBCORE_EXPORT RequestCompletionHandler takeRequestCompletionHandler(uint64_t);
    WEBCORE_EXPORT uint64_t addQueryCompletionHandler(QueryCompletionHandler&&);
    WEBCORE_EXPORT QueryCompletionHandler takeQueryCompletionHandler(uint64_t);

private:
    WeakPtrFactory<CredentialsMessenger> m_weakFactory;

    enum CallBackClassifier : uint64_t {
        Creation = 0x01,
        Request = 0x02,
        Query = 0x03,
    };
    // The most significant byte is reserved as callback classifier.
    uint64_t m_accumulatedMessageId { 1 };
    HashMap<uint64_t, CreationCompletionHandler> m_pendingCreationCompletionHandlers;
    HashMap<uint64_t, RequestCompletionHandler> m_pendingRequestCompletionHandlers;
    HashMap<uint64_t, QueryCompletionHandler> m_pendingQueryCompletionHandlers;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
