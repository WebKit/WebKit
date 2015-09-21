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
#include "MediaStreamPrivateAVFObjC.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "ContentType.h"
#import "ExceptionCodePlaceholder.h"
#import "MediaPlayerPrivateMediaStreamAVFObjC.h"
#import "MediaStreamPrivate.h"
#import "SoftLinking.h"
#import "SourceBufferPrivateAVFObjC.h"
#import <objc/runtime.h>
#import <wtf/text/AtomicString.h>
#import <wtf/text/StringBuilder.h>

namespace WebCore {

#pragma mark -
#pragma mark MediaStreamPrivateAVFObjC

RefPtr<MediaStreamPrivateAVFObjC> MediaStreamPrivateAVFObjC::create(MediaPlayerPrivateMediaStreamAVFObjC& parent, MediaStreamPrivate& stream)
{
    return adoptRef(new MediaStreamPrivateAVFObjC(parent, stream));
}

MediaStreamPrivateAVFObjC::MediaStreamPrivateAVFObjC(MediaPlayerPrivateMediaStreamAVFObjC& parent, MediaStreamPrivate& stream)
    : MediaStreamPrivate(*stream.client())
    , m_player(&parent)
{
}

MediaStreamPrivateAVFObjC::~MediaStreamPrivateAVFObjC()
{
}

MediaTime MediaStreamPrivateAVFObjC::duration()
{
    return MediaTime::positiveInfiniteTime();
}

std::unique_ptr<PlatformTimeRanges> MediaStreamPrivateAVFObjC::buffered()
{
    return std::unique_ptr<PlatformTimeRanges>();
}

MediaPlayer::ReadyState MediaStreamPrivateAVFObjC::readyState() const
{
    return m_player->readyState();
}

void MediaStreamPrivateAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    m_player->setReadyState(readyState);
}

bool MediaStreamPrivateAVFObjC::hasAudio() const
{
    return m_player->hasAudio();
}

bool MediaStreamPrivateAVFObjC::hasVideo() const
{
    return m_player->hasVideo();
}

FloatSize MediaStreamPrivateAVFObjC::naturalSize() const
{
    return m_player->naturalSize();
}
    
}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
