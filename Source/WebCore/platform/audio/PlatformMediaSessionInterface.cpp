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

#include "MediaSessionManagerInterface.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class EmptyPlatformMediaSessionClient final : public PlatformMediaSessionClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(EmptyPlatformMediaSessionClient);
public:
    RefPtr<MediaSessionManagerInterface> NODELETE sessionManager() const final { return nullptr; }
    PlatformMediaSessionMediaType NODELETE mediaType() const final { return PlatformMediaSessionMediaType::None; }
    PlatformMediaSessionMediaType NODELETE presentationType() const final { return PlatformMediaSessionMediaType::None; }
    void NODELETE mayResumePlayback(bool) final { }
    void NODELETE suspendPlayback() final { }
    bool NODELETE canReceiveRemoteControlCommands() const final { return false; }
    void NODELETE didReceiveRemoteControlCommand(PlatformMediaSessionRemoteControlCommandType, const PlatformMediaSessionRemoteCommandArgument&) final { }
    bool NODELETE supportsSeeking() const final { return false; }
    bool NODELETE shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSessionInterruptionType) const final { return false; }
    std::optional<MediaSessionGroupIdentifier> NODELETE mediaSessionGroupIdentifier() const final { return std::nullopt; }

#if !RELEASE_LOG_DISABLED
    virtual const Logger& logger() const { return emptyLogger(); }
    virtual uint64_t NODELETE logIdentifier() const { return 0; }
#endif
};

PlatformMediaSessionClient& emptyPlatformMediaSessionClient()
{
    static NeverDestroyed<EmptyPlatformMediaSessionClient> client { };
    return client;
}

PlatformMediaSessionClient::~PlatformMediaSessionClient() = default;

}
