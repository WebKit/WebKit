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

#pragma once

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include "MediaPlaybackTargetContextSerialized.h"
#include <WebCore/MediaPlaybackTarget.h>

namespace WebKit {

class MediaPlaybackTargetSerialized final : public WebCore::MediaPlaybackTarget {
public:
    static Ref<MediaPlaybackTargetSerialized> create(MediaPlaybackTargetContextSerialized&&);

    ~MediaPlaybackTargetSerialized();

    const MediaPlaybackTargetContextSerialized& context() const { return m_context; }

private:
    explicit MediaPlaybackTargetSerialized(MediaPlaybackTargetContextSerialized&&);

    // MediaPlaybackTarget
    String deviceName() const final { return m_context.deviceName(); }
    bool hasActiveRoute() const final { return m_context.hasActiveRoute(); }
    bool supportsRemoteVideoPlayback() const final { return m_context.supportsRemoteVideoPlayback(); }

    MediaPlaybackTargetContextSerialized m_context;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::MediaPlaybackTargetSerialized)
static bool isType(const WebCore::MediaPlaybackTarget& target)
{
    return target.type() == WebCore::MediaPlaybackTargetType::Serialized;
}
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
