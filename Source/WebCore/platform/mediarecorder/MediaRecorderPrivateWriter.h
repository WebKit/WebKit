/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_RECORDER)

#include <span>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class FragmentedSharedBuffer;
struct MediaRecorderPrivateOptions;
class PlatformAudioData;
class VideoFrame;

class WEBCORE_EXPORT MediaRecorderPrivateWriter : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaRecorderPrivateWriter, WTF::DestructionThread::Main> {
public:
    static RefPtr<MediaRecorderPrivateWriter> create(bool hasAudio, bool hasVideo, const MediaRecorderPrivateOptions&);
    virtual ~MediaRecorderPrivateWriter() = default;
    virtual bool initialize(const MediaRecorderPrivateOptions&) = 0;

    virtual void appendVideoFrame(VideoFrame&) = 0;
    virtual void appendAudioSampleBuffer(const PlatformAudioData&, const AudioStreamDescription&, const WTF::MediaTime&, size_t) = 0;
    virtual void stopRecording() = 0;
    virtual void fetchData(CompletionHandler<void(RefPtr<FragmentedSharedBuffer>&&, double)>&&) = 0;

    virtual void pause() = 0;
    virtual void resume() = 0;

    virtual void appendData(std::span<const uint8_t>) = 0;

    virtual const String& mimeType() const = 0;
    virtual unsigned audioBitRate() const = 0;
    virtual unsigned videoBitRate() const = 0;

    virtual void close() = 0;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
