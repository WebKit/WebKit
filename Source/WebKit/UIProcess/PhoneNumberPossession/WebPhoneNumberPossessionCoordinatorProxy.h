/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include "MessageReceiver.h"
#include "PhoneNumberPossessionTypes.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/MockPhoneNumberPossessionConfiguration.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {

class WebFrameProxy;
class WebPageProxy;
class WebProcessProxy;
struct SharedPreferencesForWebProcess;

class WebPhoneNumberPossessionCoordinatorProxy final : public IPC::MessageReceiver, public RefCounted<WebPhoneNumberPossessionCoordinatorProxy> {
    WTF_MAKE_TZONE_ALLOCATED(WebPhoneNumberPossessionCoordinatorProxy);
    WTF_MAKE_NONCOPYABLE(WebPhoneNumberPossessionCoordinatorProxy);
public:
    static Ref<WebPhoneNumberPossessionCoordinatorProxy> create(WebPageProxy&);
    ~WebPhoneNumberPossessionCoordinatorProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(IPC::Connection&) const;

    void cancelRequestForFrame(WebCore::FrameIdentifier, CompletionHandler<void()>&& = [] { });
    void cancelPendingRequest(CompletionHandler<void()>&& = [] { });
    void clearMockConfigurationForTesting() { m_mockConfiguration = std::nullopt; }

private:
    explicit WebPhoneNumberPossessionCoordinatorProxy(WebPageProxy&);

    using RequestCompletionHandler = CompletionHandler<void(std::expected<WebCore::PhoneNumberPossessionResponse, WebCore::ExceptionData>&&)>;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void requestPhoneNumberPossession(IPC::Connection&, WebCore::FrameIdentifier, WebCore::PhoneNumberPossessionRequest&&, RequestCompletionHandler&&);
    void isAvailable(IPC::Connection&, WebCore::FrameIdentifier, CompletionHandler<void(bool)>&&);
    void cancel(IPC::Connection&, WebCore::FrameIdentifier, CompletionHandler<void()>&&);
    void setMockConfiguration(WebCore::MockPhoneNumberPossessionConfiguration&&);

    std::expected<Ref<WebFrameProxy>, WebCore::ExceptionData> frameForMessage(IPC::Connection&, WebCore::FrameIdentifier) const;
    void didCompleteRequest(uint64_t identifier, PhoneNumberPossessionResult&&);

    void platformIsAvailable(CompletionHandler<void(bool)>&&);
    void platformRequestPhoneNumberPossession(WebCore::PhoneNumberPossessionRequest&&, const WebCore::SecurityOriginData&, WebPageProxy&, CompletionHandler<void(PhoneNumberPossessionResult&&)>&&);

    struct PendingRequest {
        uint64_t identifier;
        WebCore::FrameIdentifier frameID;
        RequestCompletionHandler completionHandler;
        Function<void(CompletionHandler<void()>&&)> cancel;
    };

    WeakPtr<WebPageProxy> m_webPageProxy;
    const Ref<WebProcessProxy> m_process;
    const WebCore::PageIdentifier m_webPageID;
    std::optional<WebCore::MockPhoneNumberPossessionConfiguration> m_mockConfiguration;
    std::optional<PendingRequest> m_pendingRequest;
    uint64_t m_lastRequestIdentifier { 0 };

    MonotonicTime m_lastUsedActivationTimestamp { -MonotonicTime::infinity() };
};

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
