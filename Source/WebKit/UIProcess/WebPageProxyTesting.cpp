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
#include "WebPageProxyTesting.h"

#include "Connection.h"
#include "MessageSenderInlines.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageTestingMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/IntPoint.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#include "DisplayCaptureSessionManager.h"
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPageProxyTesting);

WebPageProxyTesting::WebPageProxyTesting(WebPageProxy& page)
    : m_page(page)
{
}

bool WebPageProxyTesting::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions)
{
    return protectedPage()->protectedLegacyMainFrameProcess()->sendMessage(WTFMove(encoder), sendOptions);
}

bool WebPageProxyTesting::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return protectedPage()->protectedLegacyMainFrameProcess()->sendMessage(WTFMove(encoder), sendOptions, WTFMove(handler));
}

IPC::Connection* WebPageProxyTesting::messageSenderConnection() const
{
    return &m_page->legacyMainFrameProcess().connection();
}

uint64_t WebPageProxyTesting::messageSenderDestinationID() const
{
    return m_page->webPageIDInMainFrameProcess().toUInt64();
}

void WebPageProxyTesting::setDefersLoading(bool defersLoading)
{
    send(Messages::WebPageTesting::SetDefersLoading(defersLoading));
}

void WebPageProxyTesting::dispatchActivityStateUpdate()
{
    RunLoop::current().dispatch([protectedPage = Ref { m_page.get() }] {
        protectedPage->updateActivityState();
        protectedPage->dispatchActivityStateChange();
    });
}

void WebPageProxyTesting::isLayerTreeFrozen(CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageTesting::IsLayerTreeFrozen(), WTFMove(completionHandler));
}

void WebPageProxyTesting::setCrossSiteLoadWithLinkDecorationForTesting(const URL& fromURL, const URL& toURL, bool wasFiltered, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->setCrossSiteLoadWithLinkDecorationForTesting(m_page->sessionID(), WebCore::RegistrableDomain { fromURL }, WebCore::RegistrableDomain { toURL }, wasFiltered, WTFMove(completionHandler));
}

void WebPageProxyTesting::setPermissionLevel(const String& origin, bool allowed)
{
    protectedPage()->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPageTesting::SetPermissionLevel(origin, allowed), pageID);
    });
}

bool WebPageProxyTesting::isEditingCommandEnabled(const String& commandName)
{
    RefPtr focusedOrMainFrame = m_page->focusedOrMainFrame();
    auto targetFrameID = focusedOrMainFrame ? std::optional(focusedOrMainFrame->frameID()) : std::nullopt;
    auto sendResult = protectedPage()->sendSyncToProcessContainingFrame(targetFrameID, Messages::WebPageTesting::IsEditingCommandEnabled(commandName), Seconds::infinity());
    if (!sendResult.succeeded())
        return false;
    auto [result] = sendResult.takeReply();
    return result;
}

void WebPageProxyTesting::dumpPrivateClickMeasurement(CompletionHandler<void(const String&)>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::DumpPrivateClickMeasurement(m_page->websiteDataStore().sessionID()), WTFMove(completionHandler));
}

void WebPageProxyTesting::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClearPrivateClickMeasurement(m_page->websiteDataStore().sessionID()), WTFMove(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementOverrideTimer(bool value, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementOverrideTimerForTesting(m_page->websiteDataStore().sessionID(), value), WTFMove(completionHandler));
}

void WebPageProxyTesting::markAttributedPrivateClickMeasurementsAsExpired(CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting(m_page->websiteDataStore().sessionID()), WTFMove(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementEphemeralMeasurement(bool value, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementEphemeralMeasurementForTesting(m_page->websiteDataStore().sessionID(), value), WTFMove(completionHandler));
}

void WebPageProxyTesting::simulatePrivateClickMeasurementSessionRestart(CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SimulatePrivateClickMeasurementSessionRestart(m_page->websiteDataStore().sessionID()), WTFMove(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementTokenPublicKeyURL(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenPublicKeyURLForTesting(m_page->websiteDataStore().sessionID(), url), WTFMove(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementTokenSignatureURL(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenSignatureURLForTesting(m_page->websiteDataStore().sessionID(), url), WTFMove(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementAttributionReportURLs(const URL& sourceURL, const URL& destinationURL, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAttributionReportURLsForTesting(m_page->websiteDataStore().sessionID(), sourceURL, destinationURL), WTFMove(completionHandler));
}

void WebPageProxyTesting::markPrivateClickMeasurementsAsExpired(CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::MarkPrivateClickMeasurementsAsExpiredForTesting(m_page->websiteDataStore().sessionID()), WTFMove(completionHandler));
}

void WebPageProxyTesting::setPCMFraudPreventionValues(const String& unlinkableToken, const String& secretToken, const String& signature, const String& keyID, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPCMFraudPreventionValuesForTesting(m_page->websiteDataStore().sessionID(), unlinkableToken, secretToken, signature, keyID), WTFMove(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementAppBundleID(const String& appBundleIDForTesting, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->protectedWebsiteDataStore()->protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAppBundleIDForTesting(m_page->websiteDataStore().sessionID(), appBundleIDForTesting), WTFMove(completionHandler));
}

#if ENABLE(NOTIFICATIONS)
void WebPageProxyTesting::clearNotificationPermissionState()
{
    send(Messages::WebPageTesting::ClearNotificationPermissionState());
}
#endif

void WebPageProxyTesting::clearWheelEventTestMonitor()
{
    if (!m_page->hasRunningProcess())
        return;
    send(Messages::WebPageTesting::ClearWheelEventTestMonitor());
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
void WebPageProxyTesting::setIndexOfGetDisplayMediaDeviceSelectedForTesting(std::optional<unsigned> index)
{
    DisplayCaptureSessionManager::singleton().setIndexOfDeviceSelectedForTesting(index);
}

void WebPageProxyTesting::setSystemCanPromptForGetDisplayMediaForTesting(bool canPrompt)
{
    DisplayCaptureSessionManager::singleton().setSystemCanPromptForTesting(canPrompt);
}
#endif

void WebPageProxyTesting::setTopContentInset(float contentInset, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageTesting::SetTopContentInset(contentInset), WTFMove(completionHandler));
}

Ref<WebPageProxy> WebPageProxyTesting::protectedPage() const
{
    return m_page.get();
}

void WebPageProxyTesting::setPageScaleFactor(float scaleFactor, IntPoint point, CompletionHandler<void()>&& completionHandler)
{
    Ref callback = CallbackAggregator::create(WTFMove(completionHandler));
    protectedPage()->forEachWebContentProcess([&](auto& process, auto pageID) {
        process.sendWithAsyncReply(Messages::WebPageTesting::SetPageScaleFactor(scaleFactor, point), [callback] { }, pageID);
    });
}

} // namespace WebKit
