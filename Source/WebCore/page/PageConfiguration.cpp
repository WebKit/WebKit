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
#include "BackForwardClient.h"
#include "CacheStorageProvider.h"
#include "CookieJar.h"
#include "DatabaseProvider.h"
#include "DiagnosticLoggingClient.h"
#include "DragClient.h"
#include "EditorClient.h"
#include "FrameLoaderClient.h"
#include "LibWebRTCProvider.h"
#include "MediaRecorderProvider.h"
#include "PerformanceLoggingClient.h"
#include "PlugInClient.h"
#include "PluginInfoProvider.h"
#include "ProgressTrackerClient.h"
#include "SocketProvider.h"
#include "SpeechSynthesisClient.h"
#include "StorageNamespaceProvider.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "ValidationMessageClient.h"
#include "VisitedLinkStore.h"
#if ENABLE(WEBGL)
#include "WebGLStateTracker.h"
#endif
#if ENABLE(WEB_AUTHN)
#include "AuthenticatorCoordinatorClient.h"
#endif

namespace WebCore {

PageConfiguration::PageConfiguration(PAL::SessionID sessionID, UniqueRef<EditorClient>&& editorClient, Ref<SocketProvider>&& socketProvider, UniqueRef<LibWebRTCProvider>&& libWebRTCProvider, Ref<CacheStorageProvider>&& cacheStorageProvider, Ref<BackForwardClient>&& backForwardClient, Ref<CookieJar>&& cookieJar, UniqueRef<ProgressTrackerClient>&& progressTrackerClient, UniqueRef<FrameLoaderClient>&& loaderClientForMainFrame, UniqueRef<MediaRecorderProvider>&& mediaRecorderProvider)
    : sessionID(sessionID)
    , editorClient(WTFMove(editorClient))
    , socketProvider(WTFMove(socketProvider))
    , libWebRTCProvider(WTFMove(libWebRTCProvider))
    , progressTrackerClient(WTFMove(progressTrackerClient))
    , backForwardClient(WTFMove(backForwardClient))
    , cookieJar(WTFMove(cookieJar))
    , loaderClientForMainFrame(WTFMove(loaderClientForMainFrame))
    , cacheStorageProvider(WTFMove(cacheStorageProvider))
    , mediaRecorderProvider(WTFMove(mediaRecorderProvider))
{
}

PageConfiguration::~PageConfiguration() = default;
PageConfiguration::PageConfiguration(PageConfiguration&&) = default;

}
