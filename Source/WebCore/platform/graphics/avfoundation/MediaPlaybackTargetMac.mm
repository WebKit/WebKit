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

#import "config.h"
#import "MediaPlaybackTargetMac.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#import <objc/runtime.h>
#import <pal/spi/mac/AVFoundationSPI.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

Ref<MediaPlaybackTarget> MediaPlaybackTargetMac::create(AVOutputContext *context)
{
    return adoptRef(*new MediaPlaybackTargetMac(context));
}

MediaPlaybackTargetMac::MediaPlaybackTargetMac(AVOutputContext *context)
    : MediaPlaybackTarget()
    , m_outputContext(context)
{
}

MediaPlaybackTargetMac::~MediaPlaybackTargetMac()
{
}

const MediaPlaybackTargetContext& MediaPlaybackTargetMac::targetContext() const
{
    m_context = MediaPlaybackTargetContext(m_outputContext.get());
    return m_context;
}

bool MediaPlaybackTargetMac::hasActiveRoute() const
{
    return m_outputContext && m_outputContext.get().deviceName;
}

String MediaPlaybackTargetMac::deviceName() const
{
    if (m_outputContext)
        return m_outputContext.get().deviceName;

    return emptyString();
}

MediaPlaybackTargetMac* toMediaPlaybackTargetMac(MediaPlaybackTarget* rep)
{
    return const_cast<MediaPlaybackTargetMac*>(toMediaPlaybackTargetMac(const_cast<const MediaPlaybackTarget*>(rep)));
}

const MediaPlaybackTargetMac* toMediaPlaybackTargetMac(const MediaPlaybackTarget* rep)
{
    ASSERT_WITH_SECURITY_IMPLICATION(rep->targetType() == MediaPlaybackTarget::AVFoundation);
    return static_cast<const MediaPlaybackTargetMac*>(rep);
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
