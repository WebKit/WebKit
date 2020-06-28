/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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

#include <pal/LogMacros.h>
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX WebKit2Log
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define WEBKIT2_LOG_CHANNELS(M) \
    M(ActivityState) \
    M(AdClickAttribution) \
    M(AppSSO) \
    M(Automation) \
    M(AutomationInteractions) \
    M(BackForward) \
    M(BackForwardCache) \
    M(CacheStorage) \
    M(ContentObservation) \
    M(ContextMenu) \
    M(DiskPersistency) \
    M(DragAndDrop) \
    M(Fullscreen) \
    M(Gamepad) \
    M(IPC) \
    M(ITPDebug) \
    M(IconDatabase) \
    M(IncrementalPDF) \
    M(IncrementalPDFVerbose) \
    M(IndexedDB) \
    M(KeyHandling) \
    M(Layers) \
    M(Layout) \
    M(Loading) \
    M(LocalStorageDatabaseTracker) \
    M(Media) \
    M(MemoryPressure) \
    M(MouseHandling) \
    M(Network) \
    M(NetworkCache) \
    M(NetworkCacheSpeculativePreloading) \
    M(NetworkCacheStorage) \
    M(NetworkScheduling) \
    M(NetworkSession) \
    M(PerformanceLogging) \
    M(Plugins) \
    M(Printing) \
    M(Process) \
    M(ProcessSuspension) \
    M(ProcessSwapping) \
    M(ProximityNetworking) \
    M(RemoteLayerTree) \
    M(Resize) \
    M(ResourceLoadStatistics) \
    M(Scrolling) \
    M(Selection) \
    M(ServiceWorker) \
    M(SessionState) \
    M(Storage) \
    M(StorageAPI) \
    M(TextInput) \
    M(UIHitTesting) \
    M(ViewGestures) \
    M(ViewState) \
    M(ViewportSizing) \
    M(VirtualMemory) \
    M(VisibleRects) \
    M(WebGL) \
    M(WebRTC) \

WEBKIT2_LOG_CHANNELS(DECLARE_LOG_CHANNEL)

#undef DECLARE_LOG_CHANNEL

namespace WebKit {
WTFLogChannel* getLogChannel(const String&);
} // namespace WebKit

#ifdef __cplusplus
}
#endif

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

