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

#include <wtf/Forward.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

enum class MediaPlaybackTargetType : uint8_t {
    AVOutputContext,
    Mock,
    Serialized,
};

class MediaPlaybackTarget : public ThreadSafeRefCounted<MediaPlaybackTarget> {
public:
    using Type = MediaPlaybackTargetType;

    virtual ~MediaPlaybackTarget() = default;

    Type type() const { return m_type; }

    virtual bool hasActiveRoute() const = 0;
    virtual String deviceName() const = 0;
    virtual bool supportsRemoteVideoPlayback() const = 0;

protected:
    MediaPlaybackTarget(Type type)
        : m_type { type }
    {
    }

private:
    // This should be const, however IPC's Decoder's handling doesn't allow for const member.
    Type m_type;
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
