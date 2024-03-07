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

#include <WebCore/MediaPlaybackTargetCocoa.h>
#include <WebCore/MediaPlaybackTargetMock.h>
#include <variant>

namespace WebKit {

class MediaPlaybackTargetContextSerialized final : public WebCore::MediaPlaybackTargetContext {
public:
    explicit MediaPlaybackTargetContextSerialized(const WebCore::MediaPlaybackTargetContext&);

    String deviceName() const final { return m_deviceName; }
    bool hasActiveRoute() const final { return m_hasActiveRoute; }
    bool supportsRemoteVideoPlayback() const { return m_supportsRemoteVideoPlayback; }

    std::variant<WebCore::MediaPlaybackTargetContextCocoa, WebCore::MediaPlaybackTargetContextMock> platformContext() const;

    // Used by IPC serializer.
    WebCore::MediaPlaybackTargetContextType targetType() const { return m_targetType; }
    WebCore::MediaPlaybackTargetContextMockState mockState() const { return m_state; }
    String contextID() const { return m_contextID; }
    String contextType() const { return m_contextType; }
    MediaPlaybackTargetContextSerialized(String&&, bool, bool, WebCore::MediaPlaybackTargetContextType, WebCore::MediaPlaybackTargetContextMockState, String&&, String&&);

private:
    String m_deviceName;
    bool m_hasActiveRoute { false };
    bool m_supportsRemoteVideoPlayback { false };
    // This should be const, however IPC's Decoder's handling doesn't allow for const member.
    WebCore::MediaPlaybackTargetContextType m_targetType;
    WebCore::MediaPlaybackTargetContextMockState m_state { WebCore::MediaPlaybackTargetContextMockState::Unknown };
    String m_contextID;
    String m_contextType;
};

class MediaPlaybackTargetSerialized final : public WebCore::MediaPlaybackTarget {
public:
    static Ref<MediaPlaybackTarget> create(MediaPlaybackTargetContextSerialized&& context)
    {
        return adoptRef(*new MediaPlaybackTargetSerialized(WTFMove(context)));
    }

    TargetType targetType() const final { return TargetType::Serialized; }
    const WebCore::MediaPlaybackTargetContext& targetContext() const final { return m_context; }

private:
    explicit MediaPlaybackTargetSerialized(MediaPlaybackTargetContextSerialized&& context)
        : m_context(WTFMove(context))
    {
    }

    MediaPlaybackTargetContextSerialized m_context;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::MediaPlaybackTargetContextSerialized)
static bool isType(const WebCore::MediaPlaybackTargetContext& context)
{
    return context.type() ==  WebCore::MediaPlaybackTargetContextType::Serialized;
}
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::MediaPlaybackTargetSerialized)
static bool isType(const WebCore::MediaPlaybackTarget& target)
{
    return target.targetType() ==  WebCore::MediaPlaybackTarget::TargetType::Serialized;
}
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
