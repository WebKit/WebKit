/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include <WebCore/MediaPlaybackTarget.h>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class MediaPlaybackTargetMockState : uint8_t {
    Unknown = 0,
    OutputDeviceUnavailable = 1,
    OutputDeviceAvailable = 2,
};

class MediaPlaybackTargetMock final : public MediaPlaybackTarget {
public:
    using State = MediaPlaybackTargetMockState;

    WEBCORE_EXPORT static Ref<MediaPlaybackTargetMock> create(const String& mockDeviceName, State);

    ~MediaPlaybackTargetMock();

    State state() const { return m_mockState; }

private:
    MediaPlaybackTargetMock(const String& mockDeviceName, State);

    // MediaPlaybackTarget
    String deviceName() const final { return m_mockDeviceName; }
    bool hasActiveRoute() const final { return !m_mockDeviceName.isEmpty(); }
    bool supportsRemoteVideoPlayback() const final { return !m_mockDeviceName.isEmpty(); }

    String m_mockDeviceName;
    State m_mockState;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaPlaybackTargetMock)
static bool isType(const WebCore::MediaPlaybackTarget& target)
{
    return target.type() ==  WebCore::MediaPlaybackTargetType::Mock;
}
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
