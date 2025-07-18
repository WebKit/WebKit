/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "PlatformMediaSessionInterface.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

class EmptyPlatformMediaSessionClient final : public PlatformMediaSessionClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(EmptyPlatformMediaSessionClient);
public:
    PlatformMediaSessionMediaType mediaType() const final { return PlatformMediaSessionMediaType::None; }
    PlatformMediaSessionMediaType presentationType() const final { return PlatformMediaSessionMediaType::None; }
    void mayResumePlayback(bool) final { }
    void suspendPlayback() final { }
    bool canReceiveRemoteControlCommands() const final { return false; }
    void didReceiveRemoteControlCommand(PlatformMediaSessionRemoteControlCommandType, const PlatformMediaSessionRemoteCommandArgument&) final { }
    bool supportsSeeking() const final { return false; }
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSessionInterruptionType) const final { return false; }
    std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final { return std::nullopt; }
    void isActiveNowPlayingSessionChanged() final { }
    std::optional<ProcessID> mediaSessionPresentingApplicationPID() const final { return std::nullopt; }

#if !RELEASE_LOG_DISABLED
    virtual const Logger& logger() const { return emptyLogger(); }
    virtual uint64_t logIdentifier() const { return 0; }
#endif
};

PlatformMediaSessionClient& emptyPlatformMediaSessionClient()
{
    static NeverDestroyed<EmptyPlatformMediaSessionClient> client { };
    return client;
}

}
