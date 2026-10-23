/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/Forward.h>

namespace WTF {

// Swift cannot see LOG_DISABLED, so it decides whether LOG is compiled in from ASSERT_ENABLED.
static_assert(!LOG_DISABLED == !!ASSERT_ENABLED, "Swift's debugLog assumes that LOG is compiled in exactly when ASSERT_ENABLED is");

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX WTFLog
#endif

#define WTF_LOG_CHANNELS(M) \
    M(Language) \
    M(RefCountedLeaks) \
    M(Process) \
    M(Threading) \
    M(MemoryPressure) \
    M(SuspendableWorkQueue) \
    M(NativePromise) \

#undef DECLARE_LOG_CHANNEL
#define DECLARE_LOG_CHANNEL(name) \
    WTF_EXPORT_PRIVATE extern WTFLogChannel JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, name);

WTF_LOG_CHANNELS(DECLARE_LOG_CHANNEL)

// Lets Swift name a channel, e.g. WTF.LogChannel.Process(), since it cannot use LOG_CHANNEL.
struct LogChannel {
#define DEFINE_LOG_CHANNEL_ACCESSOR(name) static WTFLogChannel& name() { return LOG_CHANNEL(name); }
    WTF_LOG_CHANNELS(DEFINE_LOG_CHANNEL_ACCESSOR)
#undef DEFINE_LOG_CHANNEL_ACCESSOR
};

// Entry points for Swift, which cannot use the LOG and RELEASE_LOG macros.
// See the WTFLogChannel extension in Swift for the idiomatic API.

// Behaves like LOG_WITH_STREAM(channel, stream << message).
WTF_EXPORT_PRIVATE void logPreformatted(WTFLogChannel&, const String& message);

#if USE(OS_LOG) && !RELEASE_LOG_DISABLED
// RELEASE_LOG is os_log on this handle, so Swift can use os.Logger(osLog(channel)) directly.
inline os_log_t osLog(const WTFLogChannel& channel) { return channel.osLogChannel; }
#elif !RELEASE_LOG_DISABLED
enum class ReleaseLogType : uint8_t { Default, Info, Debug, Error, Fault };

// RELEASE_LOG(channel, "%s", message) through the same WTF_RELEASE_LOG_SEND backend, with the location of the Swift caller.
WTF_EXPORT_PRIVATE void releaseLogPreformatted(WTFLogChannel&, ReleaseLogType, const String& file, unsigned line, const String& function, const String& message);
#endif

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

} // namespace WTF
