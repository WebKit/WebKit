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

#ifndef VideoTrackPrivateAVFObjC_h
#define VideoTrackPrivateAVFObjC_h

#if ENABLE(VIDEO)

#include "AVTrackPrivateAVFObjCImplClient.h"
#include "VideoTrackPrivateAVF.h"
#include <wtf/TZoneMalloc.h>

OBJC_CLASS AVAssetTrack;
OBJC_CLASS AVPlayerItem;
OBJC_CLASS AVPlayerItemTrack;
OBJC_CLASS AVMediaSelectionGroup;
OBJC_CLASS AVMediaSelectionOption;

namespace WebCore {

class AVTrackPrivateAVFObjCImpl;
class MediaSelectionOptionAVFObjC;
struct PlatformVideoTrackConfiguration;

class VideoTrackPrivateAVFObjC final
    : public VideoTrackPrivateAVF
    , public AVTrackPrivateAVFObjCImplVideoClient {
    WTF_MAKE_TZONE_ALLOCATED(VideoTrackPrivateAVFObjC);
    WTF_MAKE_NONCOPYABLE(VideoTrackPrivateAVFObjC)
public:
    static RefPtr<VideoTrackPrivateAVFObjC> create(AVPlayerItemTrack* track)
    {
        return adoptRef(new VideoTrackPrivateAVFObjC(track));
    }

    static RefPtr<VideoTrackPrivateAVFObjC> create(AVAssetTrack* track)
    {
        return adoptRef(new VideoTrackPrivateAVFObjC(track));
    }

    static RefPtr<VideoTrackPrivateAVFObjC> create(MediaSelectionOptionAVFObjC& option)
    {
        return adoptRef(new VideoTrackPrivateAVFObjC(option));
    }

    void setSelected(bool) override;

    AVPlayerItemTrack* playerItemTrack();

    AVAssetTrack* assetTrack();

    MediaSelectionOptionAVFObjC* mediaSelectionOption();

    void ref() const final { VideoTrackPrivateAVF::ref(); }
    void deref() const final { VideoTrackPrivateAVF::deref(); }

private:
    friend class MediaPlayerPrivateAVFoundationObjC;
    explicit VideoTrackPrivateAVFObjC(AVPlayerItemTrack*);
    explicit VideoTrackPrivateAVFObjC(AVAssetTrack*);
    explicit VideoTrackPrivateAVFObjC(MediaSelectionOptionAVFObjC&);
    explicit VideoTrackPrivateAVFObjC(Ref<AVTrackPrivateAVFObjCImpl>&&);

    void resetPropertiesFromTrack();
    void trackReadyStateChanged(const AVTrackPrivateAVFObjCImpl&, ReadyState) final;
    void videoTrackConfigurationChanged(const AVTrackPrivateAVFObjCImpl&, PlatformVideoTrackConfiguration&&) final;
    const Ref<AVTrackPrivateAVFObjCImpl> m_impl;
};

}

#endif // ENABLE(VIDEO)

#endif // VideoTrackPrivateAVFObjC_h
