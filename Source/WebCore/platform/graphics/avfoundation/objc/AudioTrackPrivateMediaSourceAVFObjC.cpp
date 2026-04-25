/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "AudioTrackPrivateMediaSourceAVFObjC.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MEDIA_SOURCE)

#include "AVTrackPrivateAVFObjCImpl.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioTrackPrivateMediaSourceAVFObjC);

AudioTrackPrivateMediaSourceAVFObjC::AudioTrackPrivateMediaSourceAVFObjC(AVAssetTrack* track)
    : m_impl(AVTrackPrivateAVFObjCImpl::create(track))
{
    resetPropertiesFromTrack();
}

AudioTrackPrivateMediaSourceAVFObjC::~AudioTrackPrivateMediaSourceAVFObjC() = default;

void AudioTrackPrivateMediaSourceAVFObjC::resetPropertiesFromTrack()
{
    setKind(m_impl->audioKind());
    setId(m_impl->id());
    setLabel(m_impl->label());
    setLanguage(m_impl->language());
    setConfiguration(m_impl->audioTrackConfiguration());
}

AVAssetTrack* AudioTrackPrivateMediaSourceAVFObjC::assetTrack()
{
    return m_impl->assetTrack();
}

void AudioTrackPrivateMediaSourceAVFObjC::setEnabled(bool enabled)
{
    if (enabled == this->enabled())
        return;

    AudioTrackPrivateAVF::setEnabled(enabled);
}

}

#endif
