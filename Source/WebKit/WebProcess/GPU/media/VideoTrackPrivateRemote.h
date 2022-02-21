/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
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

#if ENABLE(GPU_PROCESS)

#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/VideoTrackPrivate.h>

namespace WebKit {

class GPUProcessConnection;
class MediaPlayerPrivateRemote;
struct VideoTrackPrivateRemoteConfiguration;

class VideoTrackPrivateRemote
    : public WebCore::VideoTrackPrivate {
    WTF_MAKE_NONCOPYABLE(VideoTrackPrivateRemote)
public:
    static Ref<VideoTrackPrivateRemote> create(GPUProcessConnection& gpuProcessConnection, WebCore::MediaPlayerIdentifier playerIdentifier, TrackPrivateRemoteIdentifier idendifier, VideoTrackPrivateRemoteConfiguration&& configuration)
    {
        return adoptRef(*new VideoTrackPrivateRemote(gpuProcessConnection, playerIdentifier, idendifier, WTFMove(configuration)));
    }

    void updateConfiguration(VideoTrackPrivateRemoteConfiguration&&);

    using VideoTrackKind = WebCore::VideoTrackPrivate::Kind;
    VideoTrackKind kind() const final { return m_kind; }
    AtomString id() const final { return m_id; }
    AtomString label() const final { return m_label; }
    AtomString language() const final { return m_language; }
    int trackIndex() const final { return m_trackIndex; }
    MediaTime startTimeVariance() const final { return m_startTimeVariance; }

private:
    VideoTrackPrivateRemote(GPUProcessConnection&, WebCore::MediaPlayerIdentifier, TrackPrivateRemoteIdentifier, VideoTrackPrivateRemoteConfiguration&&);

    void setSelected(bool) final;

    WeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    WebCore::MediaPlayerIdentifier m_playerIdentifier;
    VideoTrackKind m_kind { None };
    AtomString m_id;
    AtomString m_label;
    AtomString m_language;
    int m_trackIndex { -1 };
    MediaTime m_startTimeVariance { MediaTime::zeroTime() };
    TrackPrivateRemoteIdentifier m_idendifier;
};

} // namespace WebKit

#endif
