/*
 * Copyright (C) 2003-2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <pal/LogMacros.h>
#include <wtf/Assertions.h>
#include <wtf/Forward.h>

namespace WebCore {

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX Log
#endif

#define WEBCORE_LOG_CHANNELS(M) \
    M(Animations) \
    M(Archives) \
    M(Compositing) \
    M(ContentFiltering) \
    M(DatabaseTracker) \
    M(DisplayLists) \
    M(DOMTimers) \
    M(Editing) \
    M(EME) \
    M(Events) \
    M(FileAPI) \
    M(Filters) \
    M(Fonts) \
    M(Frames) \
    M(FTP) \
    M(Fullscreen) \
    M(Gamepad) \
    M(History) \
    M(IOSurface) \
    M(IconDatabase) \
    M(Images) \
    M(IndexedDB) \
    M(IndexedDBOperations) \
    M(Layers) \
    M(Layout) \
    M(FormattingContextLayout) \
    M(Loading) \
    M(Media) \
    M(MediaCaptureSamples) \
    M(MediaQueries) \
    M(MediaSource) \
    M(MediaStream) \
    M(MediaSourceSamples) \
    M(MemoryPressure) \
    M(MessagePorts) \
    M(Network) \
    M(NotYetImplemented) \
    M(PageCache) \
    M(PerformanceLogging) \
    M(PlatformLeaks) \
    M(Plugins) \
    M(PopupBlocking) \
    M(Progress) \
    M(RemoteInspector) \
    M(RequestAnimationFrame) \
    M(ResourceLoading) \
    M(ResourceLoadObserver) \
    M(ResourceLoadStatistics) \
    M(Scrolling) \
    M(Services) \
    M(ServiceWorker) \
    M(SpellingAndGrammar) \
    M(SQLDatabase) \
    M(StorageAPI) \
    M(SVG) \
    M(TextAutosizing) \
    M(Tiling) \
    M(Threading) \
    M(URLParser) \
    M(Viewports) \
    M(WebAudio) \
    M(WebGL) \
    M(WebGPU) \
    M(WebMetal) \
    M(WebRTC) \
    M(WebRTCStats) \
    M(WheelEventTestTriggers) \

#undef DECLARE_LOG_CHANNEL
#define DECLARE_LOG_CHANNEL(name) \
    WEBCORE_EXPORT extern WTFLogChannel JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, name);

WEBCORE_LOG_CHANNELS(DECLARE_LOG_CHANNEL)

String logLevelString();
bool isLogChannelEnabled(const String& name);
WEBCORE_EXPORT void setLogChannelToAccumulate(const String& name);

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

WEBCORE_EXPORT WTFLogChannel* getLogChannel(const String& name);

} // namespace WebCore
