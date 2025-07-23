/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include "AVTrackPrivateAVFObjCImplClient.h"
#include "AudioTrackPrivate.h"
#include "InbandTextTrackPrivate.h"
#include "PlatformVideoColorSpace.h"
#include "SpatialVideoMetadata.h"
#include "VideoTrackPrivate.h"
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS AVAssetTrack;
OBJC_CLASS AVPlayerItem;
OBJC_CLASS AVPlayerItemTrack;
OBJC_CLASS AVMediaSelectionGroup;
OBJC_CLASS AVMediaSelectionOption;

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebCore {
class AVTrackPrivateAVFObjCImpl;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::AVTrackPrivateAVFObjCImpl> : std::true_type { };
}

namespace WebCore {

class MediaSelectionOptionAVFObjC;

struct PlatformVideoTrackConfiguration;
struct PlatformAudioTrackConfiguration;
struct VideoProjectionMetadata;

class AVTrackPrivateAVFObjCImpl final : public RefCountedAndCanMakeWeakPtr<AVTrackPrivateAVFObjCImpl> {
    WTF_MAKE_TZONE_ALLOCATED(AVTrackPrivateAVFObjCImpl);
public:
    static Ref<AVTrackPrivateAVFObjCImpl> create(AVPlayerItemTrack* track) { return adoptRef(*new AVTrackPrivateAVFObjCImpl(track)); }
    static Ref<AVTrackPrivateAVFObjCImpl> create(AVAssetTrack* track) { return adoptRef(*new AVTrackPrivateAVFObjCImpl(track)); }
    static Ref<AVTrackPrivateAVFObjCImpl> create(MediaSelectionOptionAVFObjC& option) { return adoptRef(*new AVTrackPrivateAVFObjCImpl(option)); }
    ~AVTrackPrivateAVFObjCImpl();

    void setVideoTrackClient(WeakPtr<AVTrackPrivateAVFObjCImplVideoClient>&& client) { m_videoTrackClient = WTFMove(client); }
    void setAudioTrackClient(WeakPtr<AVTrackPrivateAVFObjCImplAudioClient>&& client) { m_audioTrackClient = WTFMove(client); }

    AVPlayerItemTrack* playerItemTrack() const { return m_playerItemTrack.get(); }
    AVAssetTrack* assetTrack() const { return m_assetTrack.get(); }
    MediaSelectionOptionAVFObjC* mediaSelectionOption() const { return m_mediaSelectionOption.get(); }

    bool enabled() const;
    void setEnabled(bool);

    AudioTrackPrivate::Kind audioKind() const;
    VideoTrackPrivate::Kind videoKind() const;
    InbandTextTrackPrivate::Kind textKind() const;

    static InbandTextTrackPrivate::Kind textKindForAVAssetTrack(const AVAssetTrack*);
    static InbandTextTrackPrivate::Kind textKindForAVMediaSelectionOption(const AVMediaSelectionOption*);

    int index() const;
    TrackID id() const;
    AtomString label() const;
    AtomString language() const;

    static String languageForAVAssetTrack(AVAssetTrack*);
    static String languageForAVMediaSelectionOption(AVMediaSelectionOption *);

    PlatformVideoTrackConfiguration videoTrackConfiguration() const;
    PlatformAudioTrackConfiguration audioTrackConfiguration() const;

    using ReadyState = TrackPrivateBase::ReadyState;
    ReadyState readyState() const;

private:
    AVTrackPrivateAVFObjCImpl(AVPlayerItemTrack*);
    AVTrackPrivateAVFObjCImpl(AVAssetTrack*);
    AVTrackPrivateAVFObjCImpl(MediaSelectionOptionAVFObjC&);

    void initializeAssetTrack();
    void initializationCompleted();

    String codec() const;
    uint32_t width() const;
    uint32_t height() const;
    PlatformVideoColorSpace colorSpace() const;
    double framerate() const;
    uint64_t bitrate() const;
    std::optional<SpatialVideoMetadata> spatialVideoMetadata() const;
    std::optional<VideoProjectionMetadata> videoProjectionMetadata() const;
    uint32_t sampleRate() const;
    uint32_t numberOfChannels() const;

    const RetainPtr<AVPlayerItemTrack> m_playerItemTrack;
    const RefPtr<MediaSelectionOptionAVFObjC> m_mediaSelectionOption;
    const RetainPtr<AVAssetTrack> m_assetTrack;
    WeakPtr<AVTrackPrivateAVFObjCImplVideoClient> m_videoTrackClient;
    WeakPtr<AVTrackPrivateAVFObjCImplAudioClient> m_audioTrackClient;
};

}

#endif // ENABLE(VIDEO)

#endif
