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

#ifndef AVTrackPrivateAVFObjCImpl_h
#define AVTrackPrivateAVFObjCImpl_h

#if ENABLE(VIDEO)

#include "AudioTrackPrivate.h"
#include "PlatformVideoColorSpace.h"
#include "VideoTrackPrivate.h"
#include <wtf/Observer.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS AVAssetTrack;
OBJC_CLASS AVPlayerItem;
OBJC_CLASS AVPlayerItemTrack;
OBJC_CLASS AVMediaSelectionGroup;
OBJC_CLASS AVMediaSelectionOption;

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebCore {

class MediaSelectionOptionAVFObjC;
struct PlatformVideoTrackConfiguration;
struct PlatformAudioTrackConfiguration;

class AVTrackPrivateAVFObjCImpl : public CanMakeWeakPtr<AVTrackPrivateAVFObjCImpl> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AVTrackPrivateAVFObjCImpl(AVPlayerItemTrack*);
    explicit AVTrackPrivateAVFObjCImpl(AVAssetTrack*);
    explicit AVTrackPrivateAVFObjCImpl(MediaSelectionOptionAVFObjC&);
    ~AVTrackPrivateAVFObjCImpl();

    AVPlayerItemTrack* playerItemTrack() const { return m_playerItemTrack.get(); }
    AVAssetTrack* assetTrack() const { return m_assetTrack.get(); }
    MediaSelectionOptionAVFObjC* mediaSelectionOption() const { return m_mediaSelectionOption.get(); }

    bool enabled() const;
    void setEnabled(bool);

    AudioTrackPrivate::Kind audioKind() const;
    VideoTrackPrivate::Kind videoKind() const;

    int index() const;
    TrackID id() const;
    AtomString label() const;
    AtomString language() const;

    static String languageForAVAssetTrack(AVAssetTrack*);
    static String languageForAVMediaSelectionOption(AVMediaSelectionOption *);

    PlatformVideoTrackConfiguration videoTrackConfiguration() const;
    using VideoTrackConfigurationObserver = Observer<void()>;
    void setVideoTrackConfigurationObserver(VideoTrackConfigurationObserver& observer) { m_videoTrackConfigurationObserver = observer; }

    PlatformAudioTrackConfiguration audioTrackConfiguration() const;
    using AudioTrackConfigurationObserver = Observer<void()>;
    void setAudioTrackConfigurationObserver(AudioTrackConfigurationObserver& observer) { m_audioTrackConfigurationObserver = observer; }

private:
    void initializeAssetTrack();

    String codec() const;
    uint32_t width() const;
    uint32_t height() const;
    PlatformVideoColorSpace colorSpace() const;
    double framerate() const;
    uint64_t bitrate() const;
    uint32_t sampleRate() const;
    uint32_t numberOfChannels() const;

    RetainPtr<AVPlayerItemTrack> m_playerItemTrack;
    RetainPtr<AVPlayerItem> m_playerItem;
    RefPtr<MediaSelectionOptionAVFObjC> m_mediaSelectionOption;
    RetainPtr<AVAssetTrack> m_assetTrack;
    WeakPtr<VideoTrackConfigurationObserver> m_videoTrackConfigurationObserver;
    WeakPtr<AudioTrackConfigurationObserver> m_audioTrackConfigurationObserver;
};

}

#endif // ENABLE(VIDEO)

#endif
