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

#import "config.h"
#import "MediaPlaybackTargetCocoa.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

Ref<MediaPlaybackTarget> MediaPlaybackTargetCocoa::create(AVOutputContext *outputContext)
{
    return adoptRef(*new MediaPlaybackTargetCocoa(outputContext));
}

Ref<MediaPlaybackTarget> MediaPlaybackTargetCocoa::create(MediaPlaybackTargetContext&& context)
{
    return adoptRef(*new MediaPlaybackTargetCocoa(WTFMove(context)));
}

MediaPlaybackTargetCocoa::MediaPlaybackTargetCocoa(AVOutputContext *outputContext)
    : m_context(outputContext)
{
}

MediaPlaybackTargetCocoa::MediaPlaybackTargetCocoa(MediaPlaybackTargetContext&& context)
    : m_context(WTFMove(context))
{
}

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
Ref<MediaPlaybackTarget> MediaPlaybackTargetCocoa::create()
{
    auto *routingContextUID = [[PAL::getAVAudioSessionClass() sharedInstance] routingContextUID];
    return adoptRef(*new MediaPlaybackTargetCocoa([PAL::getAVOutputContextClass() outputContextForID:routingContextUID]));
}
#endif

MediaPlaybackTargetCocoa::~MediaPlaybackTargetCocoa()
{
}

MediaPlaybackTargetCocoa* toMediaPlaybackTargetCocoa(MediaPlaybackTarget* rep)
{
    return const_cast<MediaPlaybackTargetCocoa*>(toMediaPlaybackTargetCocoa(const_cast<const MediaPlaybackTarget*>(rep)));
}

const MediaPlaybackTargetCocoa* toMediaPlaybackTargetCocoa(const MediaPlaybackTarget* rep)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(rep->targetType() == MediaPlaybackTarget::TargetType::AVFoundation);
    return static_cast<const MediaPlaybackTargetCocoa*>(rep);
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
