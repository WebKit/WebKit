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

#ifndef VideoTrackPrivateMediaSourceAVFObjC_h
#define VideoTrackPrivateMediaSourceAVFObjC_h

#include "IntSize.h"
#include "VideoTrackPrivateAVF.h"

#if ENABLE(MEDIA_SOURCE) && ENABLE(VIDEO_TRACK)

OBJC_CLASS AVAssetTrack;
OBJC_CLASS AVPlayerItemTrack;

namespace WebCore {

class AVTrackPrivateAVFObjCImpl;
class SourceBufferPrivateAVFObjC;

class VideoTrackPrivateMediaSourceAVFObjC final : public VideoTrackPrivateAVF {
    WTF_MAKE_NONCOPYABLE(VideoTrackPrivateMediaSourceAVFObjC)
public:
    static RefPtr<VideoTrackPrivateMediaSourceAVFObjC> create(AVAssetTrack* track, SourceBufferPrivateAVFObjC* parent)
    {
        return adoptRef(new VideoTrackPrivateMediaSourceAVFObjC(track, parent));
    }

    virtual bool selected() const override;
    virtual void setSelected(bool) override;

    void setAssetTrack(AVAssetTrack*);
    AVAssetTrack* assetTrack() const;

    int trackID() { return m_trackID; }

    IntSize naturalSize() const;

private:
    explicit VideoTrackPrivateMediaSourceAVFObjC(AVAssetTrack*, SourceBufferPrivateAVFObjC* parent);
    
    void resetPropertiesFromTrack();

    std::unique_ptr<AVTrackPrivateAVFObjCImpl> m_impl;
    SourceBufferPrivateAVFObjC* m_parent;
    int m_trackID;
    bool m_selected;
};

}

#endif // ENABLE(MEDIA_SOURCE) && ENABLE(VIDEO_TRACK)

#endif
