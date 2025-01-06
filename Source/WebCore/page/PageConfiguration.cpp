/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PageConfiguration.h"

#include "AlternativeTextClient.h"
#include "ApplicationCacheStorage.h"
#include "AttachmentElementClient.h"
#include "BackForwardClient.h"
#include "BadgeClient.h"
#include "BroadcastChannelRegistry.h"
#include "CacheStorageProvider.h"
#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "CookieJar.h"
#include "CryptoClient.h"
#include "DatabaseProvider.h"
#include "DiagnosticLoggingClient.h"
#include "DragClient.h"
#include "EditorClient.h"
#include "Frame.h"
#include "HistoryItem.h"
#include "InspectorClient.h"
#include "LocalFrameLoaderClient.h"
#include "ModelPlayerProvider.h"
#include "PerformanceLoggingClient.h"
#include "PluginInfoProvider.h"
#include "ProcessSyncClient.h"
#include "ProgressTrackerClient.h"
#include "RemoteFrameClient.h"
#include "ScreenOrientationManager.h"
#include "SocketProvider.h"
#include "SpeechRecognitionProvider.h"
#include "SpeechSynthesisClient.h"
#include "StorageNamespaceProvider.h"
#include "StorageProvider.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "ValidationMessageClient.h"
#include "VisitedLinkStore.h"
#include "WebRTCProvider.h"
#include <wtf/TZoneMallocInlines.h>
#if ENABLE(WEB_AUTHN)
#include "AuthenticatorCoordinatorClient.h"
#include "CredentialRequestCoordinatorClient.h"
#endif
#if ENABLE(APPLE_PAY)
#include "PaymentCoordinatorClient.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageConfiguration);

PageConfiguration::PageConfiguration(
    std::optional<PageIdentifier> identifier,
    PAL::SessionID sessionID,
    UniqueRef<EditorClient>&& editorClient,
    Ref<SocketProvider>&& socketProvider,
    UniqueRef<WebRTCProvider>&& webRTCProvider,
    Ref<CacheStorageProvider>&& cacheStorageProvider,
    Ref<UserContentProvider>&& userContentProvider,
    Ref<BackForwardClient>&& backForwardClient,
    Ref<CookieJar>&& cookieJar,
    UniqueRef<ProgressTrackerClient>&& progressTrackerClient,
    MainFrameCreationParameters&& mainFrameCreationParameters,
    FrameIdentifier mainFrameIdentifier,
    RefPtr<Frame>&& mainFrameOpener,
    UniqueRef<SpeechRecognitionProvider>&& speechRecognitionProvider,
    Ref<BroadcastChannelRegistry>&& broadcastChannelRegistry,
    UniqueRef<StorageProvider>&& storageProvider,
    UniqueRef<ModelPlayerProvider>&& modelPlayerProvider,
    Ref<BadgeClient>&& badgeClient,
    Ref<HistoryItemClient>&& historyItemClient,
#if ENABLE(CONTEXT_MENUS)
    UniqueRef<ContextMenuClient>&& contextMenuClient,
#endif
#if ENABLE(APPLE_PAY)
    Ref<PaymentCoordinatorClient>&& paymentCoordinatorClient,
#endif
    UniqueRef<ChromeClient>&& chromeClient,
    UniqueRef<CryptoClient>&& cryptoClient,
    UniqueRef<ProcessSyncClient>&& processSyncClient
)
    : identifier(identifier)
    , sessionID(sessionID)
    , chromeClient(WTFMove(chromeClient))
#if ENABLE(CONTEXT_MENUS)
    , contextMenuClient(WTFMove(contextMenuClient))
#endif
    , editorClient(WTFMove(editorClient))
    , socketProvider(WTFMove(socketProvider))
#if ENABLE(APPLE_PAY)
    , paymentCoordinatorClient(WTFMove(paymentCoordinatorClient))
#endif
    , webRTCProvider(WTFMove(webRTCProvider))
    , progressTrackerClient(WTFMove(progressTrackerClient))
    , backForwardClient(WTFMove(backForwardClient))
    , cookieJar(WTFMove(cookieJar))
    , mainFrameCreationParameters(WTFMove(mainFrameCreationParameters))
    , mainFrameIdentifier(WTFMove(mainFrameIdentifier))
    , mainFrameOpener(WTFMove(mainFrameOpener))
    , cacheStorageProvider(WTFMove(cacheStorageProvider))
    , userContentProvider(WTFMove(userContentProvider))
    , broadcastChannelRegistry(WTFMove(broadcastChannelRegistry))
    , speechRecognitionProvider(WTFMove(speechRecognitionProvider))
    , storageProvider(WTFMove(storageProvider))
    , modelPlayerProvider(WTFMove(modelPlayerProvider))
    , badgeClient(WTFMove(badgeClient))
    , historyItemClient(WTFMove(historyItemClient))
    , cryptoClient(WTFMove(cryptoClient))
    , processSyncClient(WTFMove(processSyncClient))
{
}

PageConfiguration::~PageConfiguration() = default;
PageConfiguration::PageConfiguration(PageConfiguration&&) = default;

}
