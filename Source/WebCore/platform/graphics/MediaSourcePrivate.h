/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "MediaPlayer.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContentType;
class SourceBufferPrivate;

class MediaSourcePrivate : public RefCounted<MediaSourcePrivate> {
public:
    typedef Vector<String> CodecsArray;

    MediaSourcePrivate() = default;
    virtual ~MediaSourcePrivate() = default;

    enum class AddStatus : uint8_t {
        Ok,
        NotSupported,
        ReachedIdLimit
    };
    virtual AddStatus addSourceBuffer(const ContentType&, bool webMParserEnabled, RefPtr<SourceBufferPrivate>&) = 0;
    virtual void durationChanged(const MediaTime&) = 0;
    virtual void bufferedChanged(const PlatformTimeRanges&) { }
    enum EndOfStreamStatus { EosNoError, EosNetworkError, EosDecodeError };
    virtual void markEndOfStream(EndOfStreamStatus) = 0;
    virtual void unmarkEndOfStream() = 0;
    virtual bool isEnded() const = 0;

    virtual MediaPlayer::ReadyState readyState() const = 0;
    virtual void setReadyState(MediaPlayer::ReadyState) = 0;

    virtual void setIsSeeking(bool isSeeking) { m_isSeeking = isSeeking; }
    virtual void waitForSeekCompleted() = 0;
    virtual void seekCompleted() = 0;

    virtual void setTimeFudgeFactor(const MediaTime& fudgeFactor) { m_timeFudgeFactor = fudgeFactor; }

    MediaTime timeFudgeFactor() const { return m_timeFudgeFactor; }
    bool isSeeking() const { return m_isSeeking; }

private:
    MediaTime m_timeFudgeFactor;
    bool m_isSeeking { false };
};

String convertEnumerationToString(MediaSourcePrivate::AddStatus);
String convertEnumerationToString(MediaSourcePrivate::EndOfStreamStatus);

} // namespace WebCore

namespace WTF {

template<typename Type> struct LogArgument;

template <>
struct LogArgument<WebCore::MediaSourcePrivate::AddStatus> {
    static String toString(const WebCore::MediaSourcePrivate::AddStatus status)
    {
        return convertEnumerationToString(status);
    }
};

template <>
struct LogArgument<WebCore::MediaSourcePrivate::EndOfStreamStatus> {
    static String toString(const WebCore::MediaSourcePrivate::EndOfStreamStatus status)
    {
        return convertEnumerationToString(status);
    }
};

template<> struct EnumTraits<WebCore::MediaSourcePrivate::AddStatus> {
    using values = EnumValues<
        WebCore::MediaSourcePrivate::AddStatus,
        WebCore::MediaSourcePrivate::AddStatus::Ok,
        WebCore::MediaSourcePrivate::AddStatus::NotSupported,
        WebCore::MediaSourcePrivate::AddStatus::ReachedIdLimit
    >;
};

} // namespace WTF

#endif
