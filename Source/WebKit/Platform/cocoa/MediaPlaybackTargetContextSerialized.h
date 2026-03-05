/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include "CoreIPCAVOutputContext.h"
#include <WebCore/MediaPlaybackTarget.h>
#include <wtf/Forward.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
enum class MediaPlaybackTargetMockState : uint8_t;
}

namespace WebKit {

class MediaPlaybackTargetContextSerialized {
public:
    explicit MediaPlaybackTargetContextSerialized(const WebCore::MediaPlaybackTarget&);

    String deviceName() const { return m_deviceName; }
    bool hasActiveRoute() const { return m_hasActiveRoute; }
    bool supportsRemoteVideoPlayback() const { return m_supportsRemoteVideoPlayback; }

    Ref<WebCore::MediaPlaybackTarget> playbackTarget() const;

    // Used by IPC serializer.
    WebCore::MediaPlaybackTargetType targetType() const { return m_targetType; }
    WebCore::MediaPlaybackTargetMockState mockState() const { return m_state; }
#if HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)
    CoreIPCAVOutputContext context() const { return m_context; }
    MediaPlaybackTargetContextSerialized(String&&, bool, bool, WebCore::MediaPlaybackTargetType, WebCore::MediaPlaybackTargetMockState, CoreIPCAVOutputContext&&, std::optional<WTF::UUID>&&);
#else
    String contextID() const { return m_contextID; }
    String contextType() const { return m_contextType; }
    MediaPlaybackTargetContextSerialized(String&&, bool, bool, WebCore::MediaPlaybackTargetType, WebCore::MediaPlaybackTargetMockState, String&&, String&&, std::optional<WTF::UUID>&&);
#endif
    const std::optional<WTF::UUID>& identifier() const { return m_identifier; }

private:
    String m_deviceName;
    bool m_hasActiveRoute { false };
    bool m_supportsRemoteVideoPlayback { false };
    // This should be const, however IPC's Decoder's handling doesn't allow for const member.
    WebCore::MediaPlaybackTargetType m_targetType;
    WebCore::MediaPlaybackTargetMockState m_state;
#if HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)
    CoreIPCAVOutputContext m_context;
#else
    String m_contextID;
    String m_contextType;
#endif
    std::optional<WTF::UUID> m_identifier;
};

} // namespace WebKit

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
