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
#include <wtf/RetainPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS AVOutputContext;

namespace WebCore {

class MediaPlaybackTargetCocoa final : public MediaPlaybackTarget {
public:
    WEBCORE_EXPORT static Ref<MediaPlaybackTargetCocoa> create(RetainPtr<AVOutputContext>&&);

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
    WEBCORE_EXPORT static Ref<MediaPlaybackTargetCocoa> create();
#endif

    ~MediaPlaybackTargetCocoa();

    RetainPtr<AVOutputContext> outputContext() const { return m_outputContext.get(); }

private:
    explicit MediaPlaybackTargetCocoa(RetainPtr<AVOutputContext>&&);

    // MediaPlaybackTarget
    String deviceName() const final;
    bool hasActiveRoute() const final;
    bool supportsRemoteVideoPlayback() const final;

    RetainPtr<AVOutputContext> m_outputContext;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaPlaybackTargetCocoa)
static bool isType(const WebCore::MediaPlaybackTarget& target)
{
    return target.type() ==  WebCore::MediaPlaybackTargetType::AVOutputContext;
}
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
