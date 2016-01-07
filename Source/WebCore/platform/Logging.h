/*
 * Copyright (C) 2003, 2006, 2013, 2015 Apple Inc.  All rights reserved.
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

#ifndef Logging_h
#define Logging_h

#include <functional>
#include <wtf/Assertions.h>
#include <wtf/Forward.h>

namespace WebCore {

#if LOG_DISABLED

#define LOG_RESULT(channel, function) ((void)0)
#define LOG_WITH_STREAM(channel, commands) ((void)0)

#else

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX Log
#endif

#define WEBCORE_LOG_CHANNELS(M) \
    M(Animations) \
    M(Archives) \
    M(Compositing) \
    M(ContentFiltering) \
    M(DisplayLists) \
    M(DOMTimers) \
    M(Editing) \
    M(Events) \
    M(FTP) \
    M(FileAPI) \
    M(Frames) \
    M(Fullscreen) \
    M(Gamepad) \
    M(History) \
    M(IconDatabase) \
    M(IndexedDB) \
    M(Layout) \
    M(Loading) \
    M(Media) \
    M(MediaSource) \
    M(MediaSourceSamples) \
    M(MemoryPressure) \
    M(Network) \
    M(NotYetImplemented) \
    M(PageCache) \
    M(PlatformLeaks) \
    M(Plugins) \
    M(PopupBlocking) \
    M(Progress) \
    M(RemoteInspector) \
    M(ResourceLoading) \
    M(SQLDatabase) \
    M(SVG) \
    M(Services) \
    M(Scrolling) \
    M(SpellingAndGrammar) \
    M(StorageAPI) \
    M(Threading) \
    M(WebAudio) \
    M(WebGL) \
    M(WebReplay) \
    M(WheelEventTestTriggers) \

#define DECLARE_LOG_CHANNEL(name) \
    WEBCORE_EXPORT extern WTFLogChannel JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, name);

WEBCORE_LOG_CHANNELS(DECLARE_LOG_CHANNEL)

#undef DECLARE_LOG_CHANNEL

String logLevelString();
bool isLogChannelEnabled(const String& name);
WEBCORE_EXPORT void initializeLoggingChannelsIfNecessary();
#ifndef NDEBUG
void registerNotifyCallback(const String& notifyID, std::function<void()> callback);
#endif

void logFunctionResult(WTFLogChannel*, std::function<const char*()>);

#define LOG_RESULT(channel, function) logFunctionResult(&JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, channel), function)

#define LOG_WITH_STREAM(channel, commands) logFunctionResult(&JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, channel), \
    [&]() { \
        TextStream stream(TextStream::LineMode::SingleLine); \
        commands; \
        return stream.release().utf8().data(); \
    });

#endif // !LOG_DISABLED

} // namespace WebCore

#endif // Logging_h
