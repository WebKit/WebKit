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
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(VIDEO)

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioTrackPrivateAVFObjC);

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(AVPlayerItemTrack* track)
    : AudioTrackPrivateAVFObjC(AVTrackPrivateAVFObjCImpl::create(track))
{
}

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(AVAssetTrack* track)
    : AudioTrackPrivateAVFObjC(AVTrackPrivateAVFObjCImpl::create(track))
{
}

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(MediaSelectionOptionAVFObjC& option)
    : AudioTrackPrivateAVFObjC(AVTrackPrivateAVFObjCImpl::create(option))
{
}

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(Ref<AVTrackPrivateAVFObjCImpl>&& impl)
    : m_impl(WTF::move(impl))
    , m_audioTrackConfigurationObserver(AudioTrackConfigurationObserver::create([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->audioTrackConfigurationChanged();
    }))
{
    m_impl->setAudioTrackConfigurationObserver(m_audioTrackConfigurationObserver);
    resetPropertiesFromTrack();
}

AudioTrackPrivateAVFObjC::~AudioTrackPrivateAVFObjC() = default;

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

    // Occasionally, when tearing down an AVAssetTrack in a HLS stream, the track
    // will go from having a formatDescription (and therefore having valid values
    // for properties that are derived from the format description, like codec() or
    // sampleRate()) to not having a format description. AVAssetTrack is ostensibly an
    // invariant, and properties like formatDescription should never move from
    // non-null to null. When this happens, ignore the configuration change.
    auto newConfiguration = m_impl->audioTrackConfiguration();
    if (!configuration().codec.isEmpty() && newConfiguration.codec.isEmpty())
        return;

    setConfiguration(WTF::move(newConfiguration));
}

void AudioTrackPrivateAVFObjC::audioTrackConfigurationChanged()
{
    setConfiguration(m_impl->audioTrackConfiguration());
}

AVPlayerItemTrack* AudioTrackPrivateAVFObjC::playerItemTrack()
{
    return m_impl->playerItemTrack();
}

AVAssetTrack* AudioTrackPrivateAVFObjC::assetTrack()
{
    return m_impl->assetTrack();
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

