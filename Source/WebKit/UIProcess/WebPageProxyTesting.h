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

#include "MessageSender.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class IntPoint;
}

namespace WebKit {

class WebPageProxy;

class WebPageProxyTesting : public IPC::MessageSender, public RefCounted<WebPageProxyTesting> {
    WTF_MAKE_TZONE_ALLOCATED(WebPageProxyTesting);
    WTF_MAKE_NONCOPYABLE(WebPageProxyTesting);
public:
    static Ref<WebPageProxyTesting> create(WebPageProxy& page) { return adoptRef(*new WebPageProxyTesting(page)); }

    void setDefersLoading(bool);
    void isLayerTreeFrozen(CompletionHandler<void(bool)>&&);
    void dispatchActivityStateUpdate();
    void setCrossSiteLoadWithLinkDecorationForTesting(const URL& fromURL, const URL& toURL, bool wasFiltered, CompletionHandler<void()>&&);
    void setPermissionLevel(const String& origin, bool allowed);
    bool isEditingCommandEnabled(const String& commandName);
    void resetStateBetweenTests();

    void dumpPrivateClickMeasurement(CompletionHandler<void(const String&)>&&);
    void clearPrivateClickMeasurement(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementOverrideTimer(bool value, CompletionHandler<void()>&&);
    void markAttributedPrivateClickMeasurementsAsExpired(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementEphemeralMeasurement(bool value, CompletionHandler<void()>&&);
    void simulatePrivateClickMeasurementSessionRestart(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenPublicKeyURL(const URL&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenSignatureURL(const URL&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementAttributionReportURLs(const URL& sourceURL, const URL& destinationURL, CompletionHandler<void()>&&);
    void markPrivateClickMeasurementsAsExpired(CompletionHandler<void()>&&);
    void setPCMFraudPreventionValues(const String& unlinkableToken, const String& secretToken, const String& signature, const String& keyID, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementAppBundleID(const String& appBundleIDForTesting, CompletionHandler<void()>&&);

#if ENABLE(NOTIFICATIONS)
    void clearNotificationPermissionState();
#endif

    void clearWheelEventTestMonitor();

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    void setIndexOfGetDisplayMediaDeviceSelectedForTesting(std::optional<unsigned>);
    void setSystemCanPromptForGetDisplayMediaForTesting(bool);
#endif

    void setTopContentInset(float, CompletionHandler<void()>&&);

    void clearBackForwardList(CompletionHandler<void()>&&);

    void setTracksRepaints(bool, CompletionHandler<void()>&&);
    void displayAndTrackRepaints(CompletionHandler<void()>&&);

private:
    explicit WebPageProxyTesting(WebPageProxy&);

    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>) final;
    bool sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&&, AsyncReplyHandler, OptionSet<IPC::SendOption>) final;

    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    Ref<WebPageProxy> protectedPage() const;

    WeakRef<WebPageProxy> m_page;
};

} // namespace WebKit
