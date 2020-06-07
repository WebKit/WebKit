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

#pragma once

#include "AudioTrackPrivateAVF.h"

#if ENABLE(MEDIA_SOURCE)

OBJC_CLASS AVAssetTrack;

namespace WebCore {

class AVTrackPrivateAVFObjCImpl;
class SourceBufferPrivateAVFObjC;

class AudioTrackPrivateMediaSourceAVFObjC final : public AudioTrackPrivateAVF {
    WTF_MAKE_NONCOPYABLE(AudioTrackPrivateMediaSourceAVFObjC)
public:
    static Ref<AudioTrackPrivateMediaSourceAVFObjC> create(AVAssetTrack *track, SourceBufferPrivateAVFObjC* parent)
    {
        return adoptRef(*new AudioTrackPrivateMediaSourceAVFObjC(track, parent));
    }

    void setEnabled(bool) final;

    void setAssetTrack(AVAssetTrack*);
    AVAssetTrack* assetTrack();

    int trackID() { return m_trackID; }

private:
    explicit AudioTrackPrivateMediaSourceAVFObjC(AVAssetTrack*, SourceBufferPrivateAVFObjC* parent);
    
    void resetPropertiesFromTrack();

    std::unique_ptr<AVTrackPrivateAVFObjCImpl> m_impl;
    SourceBufferPrivateAVFObjC* m_parent;
    int m_trackID;
};

}

#endif // ENABLE(MEDIA_SOURCE)
