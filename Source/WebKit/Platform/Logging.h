/*
 * Copyright (C) 2010, 2013-2025 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/text/WTFString.h>

#include "WebKitLogDefinitions.h"

#define COMMA() ,
#define OPTIONAL_ARGS(...) __VA_OPT__(COMMA()) __VA_ARGS__

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
#include "LogClient.h"

#define RELEASE_LOG_FORWARDABLE(category, logMessage, ...) do { \
    if (auto* client = downcast<LogClient>(WebCore::logClient().get())) \
        client->logMessage(__VA_ARGS__); \
    else \
        RELEASE_LOG(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__)); \
} while (0)

#define RELEASE_LOG_INFO_FORWARDABLE(category, logMessage, ...) do { \
    if (auto* client = downcast<LogClient>(WebCore::logClient().get())) \
        client->logMessage(__VA_ARGS__); \
    else \
        RELEASE_LOG_INFO(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__)); \
} while (0)

#define RELEASE_LOG_ERROR_FORWARDABLE(category, logMessage, ...) do { \
    if (auto* client = downcast<LogClient>(WebCore::logClient().get())) \
        client->logMessage(__VA_ARGS__); \
    else \
        RELEASE_LOG_ERROR(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__)); \
} while (0)

#define RELEASE_LOG_FAULT_FORWARDABLE(category, logMessage, ...) do { \
    if (auto* client = downcast<LogClient>(WebCore::logClient().get())) \
        client->logMessage(__VA_ARGS__); \
    else \
        RELEASE_LOG_FAULT(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__)); \
} while (0)
#else
#define RELEASE_LOG_FORWARDABLE(category, logMessage, ...) RELEASE_LOG(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))
#define RELEASE_LOG_INFO_FORWARDABLE(category, logMessage, ...) RELEASE_LOG_INFO(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))
#define RELEASE_LOG_ERROR_FORWARDABLE(category, logMessage, ...) RELEASE_LOG_ERROR(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))
#define RELEASE_LOG_FAULT_FORWARDABLE(category, logMessage, ...) RELEASE_LOG_FAULT(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))
#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX WebKit2Log
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define WEBKIT2_LOG_CHANNELS(M) \
    M(API) \
    M(ActivityState) \
    M(AdvancedPrivacyProtections) \
    M(AppSSO) \
    M(Animations) \
    M(Automation) \
    M(AutomationInteractions) \
    M(BackForward) \
    M(BackForwardCache) \
    M(CacheStorage) \
    M(ContentObservation) \
    M(ContentRuleLists) \
    M(ContextMenu) \
    M(DigitalCredentials) \
    M(DisplayLink) \
    M(DisplayLists) \
    M(DiskPersistency) \
    M(DragAndDrop) \
    M(EME) \
    M(EnhancedSecurity) \
    M(Extensions) \
    M(FrameTree) \
    M(Fullscreen) \
    M(Gamepad) \
    M(GestureHandling) \
    M(IPC) \
    M(IPCMessages) \
    M(ITPDebug) \
    M(IconDatabase) \
    M(Images) \
    M(ImageAnalysis) \
    M(IncrementalPDF) \
    M(IncrementalPDFVerbose) \
    M(IndexedDB) \
    M(Inspector) \
    M(KeyHandling) \
    M(Keychain) \
    M(Language) \
    M(Layers) \
    M(Layout) \
    M(Loading) \
    M(Media) \
    M(MemoryPressure) \
    M(ModelElement) \
    M(MouseHandling) \
    M(Network) \
    M(NetworkCache) \
    M(NetworkCacheSpeculativePreloading) \
    M(NetworkCacheStorage) \
    M(NetworkScheduling) \
    M(NetworkSession) \
    M(Notifications) \
    M(PDF) \
    M(PDFAsyncRendering) \
    M(PageLoadObserver) \
    M(Pasteboard) \
    M(PerformanceLogging) \
    M(PointerLock) \
    M(Plugins) \
    M(Printing) \
    M(PrivateClickMeasurement) \
    M(Process) \
    M(ProcessCapabilities) \
    M(ProcessSuspension) \
    M(ProcessSwapping) \
    M(ProximityNetworking) \
    M(Push) \
    M(RemoteLayerBuffers) \
    M(RemoteLayerTree) \
    M(Resize) \
    M(ResourceLoadStatistics) \
    M(ResourceMonitoring) \
    M(Sandbox) \
    M(ScreenTime) \
    M(ScrollAnimations) \
    M(Scrolling) \
    M(SecureCoding) \
    M(Selection) \
    M(ServiceWorker) \
    M(SessionState) \
    M(SharedDisplayLists) \
    M(SharedWorker) \
    M(SiteIsolation) \
    M(Storage) \
    M(StorageAPI) \
    M(SystemPreview) \
    M(Telephony) \
    M(TextExtraction) \
    M(TextInput) \
    M(TextInteraction) \
    M(Translation) \
    M(UIHitTesting) \
    M(UserContentController) \
    M(ViewGestures) \
    M(ViewState) \
    M(ViewportSizing) \
    M(VirtualMemory) \
    M(VisibleRects) \
    M(WebAuthn) \
    M(WebGL) \
    M(WebRTC) \
    M(WheelEvents) \
    M(Worker) \
    M(XR) \

WEBKIT2_LOG_CHANNELS(DECLARE_LOG_CHANNEL)

#undef DECLARE_LOG_CHANNEL

#ifdef __cplusplus
}
#endif

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED
