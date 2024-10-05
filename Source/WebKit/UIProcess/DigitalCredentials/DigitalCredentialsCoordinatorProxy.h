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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "MessageReceiver.h"
#include <WebCore/CredentialRequestOptions.h>
#include <WebCore/DigitalCredentialRequestOptions.h>
#include <WebCore/FrameIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
struct ExceptionData;
struct DigitalCredentialRequestOptions;
}

namespace WebKit {
class WebPageProxy;
struct SharedPreferencesForWebProcess;
struct FrameInfoData;

// FIXME: We need to define a DigitalWalletResponse structure (https://webkit.org/b/278148)
// we are just initially handling exceptions as we build this out.
using DigitalRequestCompletionHandler = CompletionHandler<void(const WebCore::ExceptionData&)>;

class DigitalCredentialsCoordinatorProxy final : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DigitalCredentialsCoordinatorProxy);

public:
    explicit DigitalCredentialsCoordinatorProxy(WebPageProxy&);
    ~DigitalCredentialsCoordinatorProxy();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Receivers.
    void requestDigitalCredential(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::DigitalCredentialRequestOptions&&, DigitalRequestCompletionHandler&&);
    void cancel(CompletionHandler<void()>&&);

    Ref<WebPageProxy> protectedPage() const;

    WeakRef<WebPageProxy> m_page;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
