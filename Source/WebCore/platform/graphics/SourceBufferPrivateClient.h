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

#if ENABLE(MEDIA_SOURCE)

#include <wtf/MediaTime.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioTrackPrivate;
class InbandTextTrackPrivate;
class MediaSample;
class MediaDescription;
class VideoTrackPrivate;

class SourceBufferPrivateClient {
public:
    virtual ~SourceBufferPrivateClient() = default;

    struct InitializationSegment {
        MediaTime duration;

        struct AudioTrackInformation {
            RefPtr<MediaDescription> description;
            RefPtr<AudioTrackPrivate> track;
        };
        Vector<AudioTrackInformation> audioTracks;

        struct VideoTrackInformation {
            RefPtr<MediaDescription> description;
            RefPtr<VideoTrackPrivate> track;
        };
        Vector<VideoTrackInformation> videoTracks;

        struct TextTrackInformation {
            RefPtr<MediaDescription> description;
            RefPtr<InbandTextTrackPrivate> track;
        };
        Vector<TextTrackInformation> textTracks;
    };
    virtual void sourceBufferPrivateDidReceiveInitializationSegment(const InitializationSegment&) = 0;
    virtual void sourceBufferPrivateDidReceiveSample(MediaSample&) = 0;
    virtual bool sourceBufferPrivateHasAudio() const = 0;
    virtual bool sourceBufferPrivateHasVideo() const = 0;

    virtual void sourceBufferPrivateReenqueSamples(const AtomicString& trackID) = 0;
    virtual void sourceBufferPrivateDidBecomeReadyForMoreSamples(const AtomicString& trackID) = 0;

    virtual MediaTime sourceBufferPrivateFastSeekTimeForMediaTime(const MediaTime&, const MediaTime&, const MediaTime&) = 0;

    enum AppendResult { AppendSucceeded, ReadStreamFailed, ParsingFailed };
    virtual void sourceBufferPrivateAppendComplete(AppendResult) = 0;
    virtual void sourceBufferPrivateDidReceiveRenderingError(int errorCode) = 0;
};

}

#endif // ENABLE(MEDIA_SOURCE)
