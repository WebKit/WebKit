/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MediaPlaybackTargetMock.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

namespace WebCore {

Ref<MediaPlaybackTarget> MediaPlaybackTargetMock::create(const String& name, MediaPlaybackTargetContext::State state)
{
    return adoptRef(*new MediaPlaybackTargetMock(name, state));
}

MediaPlaybackTargetMock::MediaPlaybackTargetMock(const String& name, MediaPlaybackTargetContext::State state)
    : MediaPlaybackTarget()
    , m_name(name)
    , m_state(state)
{
}

MediaPlaybackTargetMock::~MediaPlaybackTargetMock() = default;

const MediaPlaybackTargetContext& MediaPlaybackTargetMock::targetContext() const
{
    m_context = MediaPlaybackTargetContext(m_name, m_state);
    return m_context;
}

MediaPlaybackTargetMock* toMediaPlaybackTargetMock(MediaPlaybackTarget* rep)
{
    return const_cast<MediaPlaybackTargetMock*>(toMediaPlaybackTargetMock(const_cast<const MediaPlaybackTarget*>(rep)));
}

const MediaPlaybackTargetMock* toMediaPlaybackTargetMock(const MediaPlaybackTarget* rep)
{
    ASSERT_WITH_SECURITY_IMPLICATION(rep->targetType() == MediaPlaybackTarget::Mock);
    return static_cast<const MediaPlaybackTargetMock*>(rep);
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
