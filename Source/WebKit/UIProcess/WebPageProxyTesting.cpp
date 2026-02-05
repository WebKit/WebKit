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
#include "WebBackForwardList.h"
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
    return protect(protect(page())->legacyMainFrameProcess())->sendMessage(WTF::move(encoder), sendOptions);
}

bool WebPageProxyTesting::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return protect(protect(page())->legacyMainFrameProcess())->sendMessage(WTF::move(encoder), sendOptions, WTF::move(handler));
}

IPC::Connection* WebPageProxyTesting::messageSenderConnection() const
{
    return &protect(protect(page())->legacyMainFrameProcess())->connection();
}

uint64_t WebPageProxyTesting::messageSenderDestinationID() const
{
    return protect(page())->webPageIDInMainFrameProcess().toUInt64();
}

void WebPageProxyTesting::dispatchActivityStateUpdate()
{
    RunLoop::currentSingleton().dispatch([protectedPage = protect(page())] {
        protectedPage->updateActivityState();
        protectedPage->dispatchActivityStateChange();
    });
}

void WebPageProxyTesting::isLayerTreeFrozen(CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageTesting::IsLayerTreeFrozen(), WTF::move(completionHandler));
}

void WebPageProxyTesting::setCrossSiteLoadWithLinkDecorationForTesting(const URL& fromURL, const URL& toURL, bool wasFiltered, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->setCrossSiteLoadWithLinkDecorationForTesting(protect(page())->sessionID(), WebCore::RegistrableDomain { fromURL }, WebCore::RegistrableDomain { toURL }, wasFiltered, WTF::move(completionHandler));
}

void WebPageProxyTesting::setPermissionLevel(const String& origin, bool allowed)
{
    protect(page())->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPageTesting::SetPermissionLevel(origin, allowed), pageID);
    });
}

bool WebPageProxyTesting::isEditingCommandEnabled(const String& commandName)
{
    RefPtr focusedOrMainFrame = m_page->focusedOrMainFrame();
    auto targetFrameID = focusedOrMainFrame ? std::optional(focusedOrMainFrame->frameID()) : std::nullopt;
    auto sendResult = protect(page())->sendSyncToProcessContainingFrame(targetFrameID, Messages::WebPageTesting::IsEditingCommandEnabled(commandName), Seconds::infinity());
    if (!sendResult.succeeded())
        return false;
    auto [result] = sendResult.takeReply();
    return result;
}

void WebPageProxyTesting::dumpPrivateClickMeasurement(CompletionHandler<void(const String&)>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::DumpPrivateClickMeasurement(m_page->websiteDataStore().sessionID()), WTF::move(completionHandler));
}

void WebPageProxyTesting::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::ClearPrivateClickMeasurement(m_page->websiteDataStore().sessionID()), WTF::move(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementOverrideTimer(bool value, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementOverrideTimerForTesting(m_page->websiteDataStore().sessionID(), value), WTF::move(completionHandler));
}

void WebPageProxyTesting::markAttributedPrivateClickMeasurementsAsExpired(CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting(m_page->websiteDataStore().sessionID()), WTF::move(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementEphemeralMeasurement(bool value, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementEphemeralMeasurementForTesting(m_page->websiteDataStore().sessionID(), value), WTF::move(completionHandler));
}

void WebPageProxyTesting::simulatePrivateClickMeasurementSessionRestart(CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SimulatePrivateClickMeasurementSessionRestart(m_page->websiteDataStore().sessionID()), WTF::move(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementTokenPublicKeyURL(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenPublicKeyURLForTesting(m_page->websiteDataStore().sessionID(), url), WTF::move(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementTokenSignatureURL(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementTokenSignatureURLForTesting(m_page->websiteDataStore().sessionID(), url), WTF::move(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementAttributionReportURLs(const URL& sourceURL, const URL& destinationURL, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAttributionReportURLsForTesting(m_page->websiteDataStore().sessionID(), sourceURL, destinationURL), WTF::move(completionHandler));
}

void WebPageProxyTesting::markPrivateClickMeasurementsAsExpired(CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::MarkPrivateClickMeasurementsAsExpiredForTesting(m_page->websiteDataStore().sessionID()), WTF::move(completionHandler));
}

void WebPageProxyTesting::setPCMFraudPreventionValues(const String& unlinkableToken, const String& secretToken, const String& signature, const String& keyID, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SetPCMFraudPreventionValuesForTesting(m_page->websiteDataStore().sessionID(), unlinkableToken, secretToken, signature, keyID), WTF::move(completionHandler));
}

void WebPageProxyTesting::setPrivateClickMeasurementAppBundleID(const String& appBundleIDForTesting, CompletionHandler<void()>&& completionHandler)
{
    protect(protect(protect(page())->websiteDataStore())->networkProcess())->sendWithAsyncReply(Messages::NetworkProcess::SetPrivateClickMeasurementAppBundleIDForTesting(m_page->websiteDataStore().sessionID(), appBundleIDForTesting), WTF::move(completionHandler));
}

#if ENABLE(NOTIFICATIONS)
void WebPageProxyTesting::clearNotificationPermissionState()
{
    send(Messages::WebPageTesting::ClearNotificationPermissionState());
}
#endif

void WebPageProxyTesting::clearWheelEventTestMonitor()
{
    if (!protect(page())->hasRunningProcess())
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

void WebPageProxyTesting::setObscuredContentInsets(float top, float right, float bottom, float left, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageTesting::SetObscuredContentInsets(top, right, bottom, left), WTF::move(completionHandler));
}

void WebPageProxyTesting::resetStateBetweenTests()
{
    protect(protect(page())->legacyMainFrameProcess())->resetState();

    if (RefPtr mainFrame = m_page->mainFrame())
        mainFrame->disownOpener();

    protect(page())->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPageTesting::ResetStateBetweenTests(), pageID);
    });
}

void WebPageProxyTesting::clearBackForwardList(CompletionHandler<void()>&& completionHandler)
{
    Ref page = m_page.get();
    protect(page->backForwardListWrapper())->clear();

    Ref callbackAggregator = CallbackAggregator::create(WTF::move(completionHandler));
    page->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPageTesting::ClearCachedBackForwardListCounts(), [callbackAggregator] { }, pageID);
    });
}

void WebPageProxyTesting::setTracksRepaints(bool trackRepaints, CompletionHandler<void()>&& completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create(WTF::move(completionHandler));
    protect(page())->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPageTesting::SetTracksRepaints(trackRepaints), [callbackAggregator] { }, pageID);
    });
}

void WebPageProxyTesting::displayAndTrackRepaints(CompletionHandler<void()>&& completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create(WTF::move(completionHandler));
    protect(page())->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.sendWithAsyncReply(Messages::WebPageTesting::DisplayAndTrackRepaints(), [callbackAggregator] { }, pageID);
    });
}

} // namespace WebKit
