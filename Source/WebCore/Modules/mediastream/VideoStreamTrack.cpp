/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "VideoStreamTrack.h"

#include "Dictionary.h"
#include "ScriptExecutionContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

RefPtr<VideoStreamTrack> VideoStreamTrack::create(ScriptExecutionContext& context, const Dictionary& videoConstraints)
{
    return adoptRef(new VideoStreamTrack(context, *MediaStreamTrackPrivate::create(0), &videoConstraints));
}

RefPtr<VideoStreamTrack> VideoStreamTrack::create(ScriptExecutionContext& context, MediaStreamTrackPrivate& privateTrack)
{
    return adoptRef(new VideoStreamTrack(context, privateTrack, 0));
}

RefPtr<VideoStreamTrack> VideoStreamTrack::create(MediaStreamTrack& track)
{
    return adoptRef(new VideoStreamTrack(track));
}

VideoStreamTrack::VideoStreamTrack(ScriptExecutionContext& context, MediaStreamTrackPrivate& privateTrack, const Dictionary* videoConstraints)
    : MediaStreamTrack(context, privateTrack, videoConstraints)
{
}

VideoStreamTrack::VideoStreamTrack(MediaStreamTrack& track)
    : MediaStreamTrack(track)
{
}

const AtomicString& VideoStreamTrack::kind() const
{
    static NeverDestroyed<AtomicString> videoKind("video", AtomicString::ConstructFromLiteral);
    return videoKind;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
