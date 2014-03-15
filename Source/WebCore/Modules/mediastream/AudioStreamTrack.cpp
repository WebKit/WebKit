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

#if ENABLE(MEDIA_STREAM)

#include "AudioStreamTrack.h"

#include "Dictionary.h"
#include "ScriptExecutionContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

RefPtr<AudioStreamTrack> AudioStreamTrack::create(ScriptExecutionContext& context, const Dictionary& audioConstraints)
{
    return adoptRef(new AudioStreamTrack(context, *MediaStreamTrackPrivate::create(0), &audioConstraints));
}

RefPtr<AudioStreamTrack> AudioStreamTrack::create(ScriptExecutionContext& context, MediaStreamTrackPrivate& privateTrack)
{
    return adoptRef(new AudioStreamTrack(context, privateTrack, 0));
}

RefPtr<AudioStreamTrack> AudioStreamTrack::create(MediaStreamTrack& track)
{
    return adoptRef(new AudioStreamTrack(track));
}

AudioStreamTrack::AudioStreamTrack(ScriptExecutionContext& context, MediaStreamTrackPrivate& privateTrack, const Dictionary* audioConstraints)
    : MediaStreamTrack(context, privateTrack, audioConstraints)
{
}

AudioStreamTrack::AudioStreamTrack(MediaStreamTrack& track)
    : MediaStreamTrack(track)
{
}

const AtomicString& AudioStreamTrack::kind() const
{
    static NeverDestroyed<AtomicString> audioKind("audio", AtomicString::ConstructFromLiteral);
    return audioKind;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
