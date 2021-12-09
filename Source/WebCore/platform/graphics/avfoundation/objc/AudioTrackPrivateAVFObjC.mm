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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AudioTrackPrivateAVFObjC.h"
#import "AVTrackPrivateAVFObjCImpl.h"
#import "MediaSelectionGroupAVFObjC.h"

#if ENABLE(VIDEO)

namespace WebCore {

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(AVPlayerItemTrack* track)
    : m_impl(makeUnique<AVTrackPrivateAVFObjCImpl>(track))
{
    resetPropertiesFromTrack();
}

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(MediaSelectionOptionAVFObjC& option)
    : m_impl(makeUnique<AVTrackPrivateAVFObjCImpl>(option))
{
    resetPropertiesFromTrack();
}

void AudioTrackPrivateAVFObjC::resetPropertiesFromTrack()
{
    // Don't call this->setEnabled() because it also sets the enabled state of the
    // AVPlayerItemTrack
    AudioTrackPrivateAVF::setEnabled(m_impl->enabled());

    setTrackIndex(m_impl->index());
    setKind(m_impl->audioKind());
    setId(m_impl->id());
    setLabel(m_impl->label());
    setLanguage(m_impl->language());
}

void AudioTrackPrivateAVFObjC::setPlayerItemTrack(AVPlayerItemTrack *track)
{
    m_impl = makeUnique<AVTrackPrivateAVFObjCImpl>(track);
    resetPropertiesFromTrack();
}

AVPlayerItemTrack* AudioTrackPrivateAVFObjC::playerItemTrack()
{
    return m_impl->playerItemTrack();
}

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(AVAssetTrack* track)
    : m_impl(makeUnique<AVTrackPrivateAVFObjCImpl>(track))
{
    resetPropertiesFromTrack();
}

void AudioTrackPrivateAVFObjC::setAssetTrack(AVAssetTrack *track)
{
    m_impl = makeUnique<AVTrackPrivateAVFObjCImpl>(track);
    resetPropertiesFromTrack();
}

AVAssetTrack* AudioTrackPrivateAVFObjC::assetTrack()
{
    return m_impl->assetTrack();
}

void AudioTrackPrivateAVFObjC::setMediaSelectionOption(MediaSelectionOptionAVFObjC& option)
{
    m_impl = makeUnique<AVTrackPrivateAVFObjCImpl>(option);
    resetPropertiesFromTrack();
}

MediaSelectionOptionAVFObjC* AudioTrackPrivateAVFObjC::mediaSelectionOption()
{
    return m_impl->mediaSelectionOption();
}

void AudioTrackPrivateAVFObjC::setEnabled(bool enabled)
{
    AudioTrackPrivateAVF::setEnabled(enabled);
    m_impl->setEnabled(enabled);
}

}

#endif

