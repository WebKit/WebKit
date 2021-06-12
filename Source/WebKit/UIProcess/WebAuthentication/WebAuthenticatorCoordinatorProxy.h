/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
enum class AuthenticatorAttachment;
struct ExceptionData;
struct PublicKeyCredentialCreationOptions;
struct AuthenticatorResponseData;
struct PublicKeyCredentialRequestOptions;
struct SecurityOriginData;
}

namespace WebKit {

class WebPageProxy;

struct FrameInfoData;
struct WebAuthenticationRequestData;

class WebAuthenticatorCoordinatorProxy : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebAuthenticatorCoordinatorProxy);
public:
    explicit WebAuthenticatorCoordinatorProxy(WebPageProxy&);
    ~WebAuthenticatorCoordinatorProxy();

private:
    using RequestCompletionHandler = CompletionHandler<void(const WebCore::AuthenticatorResponseData&, WebCore::AuthenticatorAttachment, const WebCore::ExceptionData&)>;
    using QueryCompletionHandler = CompletionHandler<void(bool)>;

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Receivers.
    void makeCredential(WebCore::FrameIdentifier, FrameInfoData&&, Vector<uint8_t>&& hash, WebCore::PublicKeyCredentialCreationOptions&&, bool processingUserGesture, RequestCompletionHandler&&);
    void getAssertion(WebCore::FrameIdentifier, FrameInfoData&&, Vector<uint8_t>&& hash, WebCore::PublicKeyCredentialRequestOptions&&, bool processingUserGesture, RequestCompletionHandler&&);
    void isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&&);

    void handleRequest(WebAuthenticationRequestData&&, RequestCompletionHandler&&);

    WebPageProxy& m_webPageProxy;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
