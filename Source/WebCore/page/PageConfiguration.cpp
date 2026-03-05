/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#include "DocumentSyncClient.h"
#include "DragClient.h"
#include "EditorClient.h"
#include "Frame.h"
#include "HistoryItem.h"
#include "InspectorBackendClient.h"
#include "LocalFrameLoaderClient.h"
#include "ModelPlayerProvider.h"
#include "PerformanceLoggingClient.h"
#include "PluginInfoProvider.h"
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
    Ref<ModelPlayerProvider>&& modelPlayerProvider,
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
    UniqueRef<DocumentSyncClient>&& documentSyncClient
#if ENABLE(WEB_AUTHN)
    , Ref<CredentialRequestCoordinatorClient>&& credentialRequestCoordinatorClient
#endif
)
    : identifier(identifier)
    , sessionID(sessionID)
    , chromeClient(WTF::move(chromeClient))
#if ENABLE(CONTEXT_MENUS)
    , contextMenuClient(WTF::move(contextMenuClient))
#endif
    , editorClient(WTF::move(editorClient))
    , socketProvider(WTF::move(socketProvider))
#if ENABLE(APPLE_PAY)
    , paymentCoordinatorClient(WTF::move(paymentCoordinatorClient))
#endif
    , webRTCProvider(WTF::move(webRTCProvider))
    , progressTrackerClient(WTF::move(progressTrackerClient))
    , backForwardClient(WTF::move(backForwardClient))
    , cookieJar(WTF::move(cookieJar))
    , mainFrameCreationParameters(WTF::move(mainFrameCreationParameters))
    , mainFrameIdentifier(WTF::move(mainFrameIdentifier))
    , mainFrameOpener(WTF::move(mainFrameOpener))
    , cacheStorageProvider(WTF::move(cacheStorageProvider))
    , userContentProvider(WTF::move(userContentProvider))
    , broadcastChannelRegistry(WTF::move(broadcastChannelRegistry))
    , speechRecognitionProvider(WTF::move(speechRecognitionProvider))
    , storageProvider(WTF::move(storageProvider))
    , modelPlayerProvider(WTF::move(modelPlayerProvider))
    , badgeClient(WTF::move(badgeClient))
    , historyItemClient(WTF::move(historyItemClient))
    , cryptoClient(WTF::move(cryptoClient))
    , documentSyncClient(WTF::move(documentSyncClient))
#if ENABLE(WEB_AUTHN)
    , credentialRequestCoordinatorClient(WTF::move(credentialRequestCoordinatorClient))
#endif
{
}

PageConfiguration::~PageConfiguration() = default;
PageConfiguration::PageConfiguration(PageConfiguration&&) = default;

}
