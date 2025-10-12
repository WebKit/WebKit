/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if HAVE(DIGITAL_CREDENTIALS_UI)

#include "MessageReceiver.h"
#include <WebCore/CredentialRequestCoordinatorClient.h>
#include <WebCore/DigitalCredentialsProtocols.h>
#include <WebCore/Document.h>
#include <WebCore/UnvalidatedDigitalCredentialRequest.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class SecurityOriginData;
struct DigitalCredentialsRequestData;
struct DigitalCredentialsResponseData;
struct ExceptionData;
struct MobileDocumentRequest;
struct OpenID4VPRequest;
struct PageIdentifierType;
using PageIdentifier = ObjectIdentifier<PageIdentifierType>;
}

namespace WebKit {

class WebPage;

class DigitalCredentialsCoordinator : public WebCore::CredentialRequestCoordinatorClient, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(DigitalCredentialsCoordinator);

public:
    explicit DigitalCredentialsCoordinator(WebPage&);
    ~DigitalCredentialsCoordinator();

    static Ref<DigitalCredentialsCoordinator> create(WebPage&);
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void showDigitalCredentialsPicker(Vector<WebCore::UnvalidatedDigitalCredentialRequest>&&, const WebCore::DigitalCredentialsRequestData&, CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&) final;
    void dismissDigitalCredentialsPicker(CompletionHandler<void(bool)>&&) final;
    WebCore::ExceptionOr<Vector<WebCore::ValidatedDigitalCredentialRequest>> validateAndParseDigitalCredentialRequests(const WebCore::SecurityOrigin&, const WebCore::Document&, const Vector<WebCore::UnvalidatedDigitalCredentialRequest>&) final;
    void provideRawDigitalCredentialRequests(CompletionHandler<void(Vector<WebCore::UnvalidatedDigitalCredentialRequest>&&)>&&);

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    WeakPtr<WebPage> m_page;
    const WebCore::PageIdentifier m_pageIdentifier;
    Vector<WebCore::UnvalidatedDigitalCredentialRequest> m_rawRequests;
};

} // namespace WebKit

#endif // ENABLE(DIGITAL_CREDENTIALS_UI)
