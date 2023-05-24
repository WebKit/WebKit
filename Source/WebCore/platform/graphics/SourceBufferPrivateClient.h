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

#include "MediaDescription.h"
#include <wtf/MediaTime.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AudioTrackPrivate;
class InbandTextTrackPrivate;
class MediaSample;
class MediaDescription;
class PlatformTimeRanges;
class VideoTrackPrivate;

class SourceBufferPrivateClient : public CanMakeWeakPtr<SourceBufferPrivateClient> {
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

    enum class ReceiveResult : uint8_t {
        Succeeded,
        AppendError,
        ClientDisconnected,
        BufferRemoved,
        IPCError,
    };
    virtual void sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&&, CompletionHandler<void(ReceiveResult)>&&) = 0;
    virtual void sourceBufferPrivateStreamEndedWithDecodeError() = 0;
    enum class AppendResult : uint8_t {
        Succeeded,
        ReadStreamFailed,
        ParsingFailed
    };
    virtual void sourceBufferPrivateAppendComplete(AppendResult) = 0;
    virtual void sourceBufferPrivateBufferedChanged(const PlatformTimeRanges&, CompletionHandler<void()>&&) = 0;
    virtual void sourceBufferPrivateTrackBuffersChanged(const Vector<PlatformTimeRanges>&) { };
    virtual void sourceBufferPrivateDurationChanged(const MediaTime&, CompletionHandler<void()>&&) = 0;
    virtual void sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime&) = 0;
    virtual void sourceBufferPrivateDidParseSample(double frameDuration) = 0;
    virtual void sourceBufferPrivateDidDropSample() = 0;
    virtual void sourceBufferPrivateDidReceiveRenderingError(int64_t errorCode) = 0;
    virtual void sourceBufferPrivateReportExtraMemoryCost(uint64_t) = 0;
    virtual bool isAsync() const { return false; }
};

String convertEnumerationToString(SourceBufferPrivateClient::ReceiveResult);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::SourceBufferPrivateClient::ReceiveResult> {
    static String toString(const WebCore::SourceBufferPrivateClient::ReceiveResult result)
    {
        return convertEnumerationToString(result);
    }
};

template<> struct EnumTraits<WebCore::SourceBufferPrivateClient::ReceiveResult> {
    using values = EnumValues<
        WebCore::SourceBufferPrivateClient::ReceiveResult,
        WebCore::SourceBufferPrivateClient::ReceiveResult::Succeeded,
        WebCore::SourceBufferPrivateClient::ReceiveResult::AppendError,
        WebCore::SourceBufferPrivateClient::ReceiveResult::ClientDisconnected,
        WebCore::SourceBufferPrivateClient::ReceiveResult::BufferRemoved,
        WebCore::SourceBufferPrivateClient::ReceiveResult::IPCError
    >;
};

template<> struct EnumTraits<WebCore::SourceBufferPrivateClient::AppendResult> {
    using values = EnumValues<
        WebCore::SourceBufferPrivateClient::AppendResult,
        WebCore::SourceBufferPrivateClient::AppendResult::Succeeded,
        WebCore::SourceBufferPrivateClient::AppendResult::ReadStreamFailed,
        WebCore::SourceBufferPrivateClient::AppendResult::ParsingFailed
    >;
};

} // namespace WTF

#endif // ENABLE(MEDIA_SOURCE)
