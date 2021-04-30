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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include "MediaPlaybackTarget.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaPlaybackTargetMock : public MediaPlaybackTarget {
public:
    WEBCORE_EXPORT static Ref<MediaPlaybackTarget> create(const String&, MediaPlaybackTargetContext::MockState);

    virtual ~MediaPlaybackTargetMock();

    TargetType targetType() const final { return MediaPlaybackTarget::TargetType::Mock; }
    const MediaPlaybackTargetContext& targetContext() const final { return m_context; }

    MediaPlaybackTargetContext::MockState state() const { return m_context.mockState(); }

protected:
    MediaPlaybackTargetMock(const String&, MediaPlaybackTargetContext::MockState);

    MediaPlaybackTargetContext m_context;
};

MediaPlaybackTargetMock* toMediaPlaybackTargetMock(MediaPlaybackTarget*);
const MediaPlaybackTargetMock* toMediaPlaybackTargetMock(const MediaPlaybackTarget*);

}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
