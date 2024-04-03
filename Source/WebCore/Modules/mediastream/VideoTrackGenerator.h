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

#if ENABLE(MEDIA_STREAM) && ENABLE(WEB_CODECS)

#include "ExceptionOr.h"
#include "RealtimeMediaSource.h"
#include "WritableStreamSink.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaStreamTrack;
class ScriptExecutionContext;
class WritableStream;

class VideoTrackGenerator : public RefCounted<VideoTrackGenerator> {
    WTF_MAKE_ISO_ALLOCATED(VideoTrackGenerator);
public:
    static ExceptionOr<Ref<VideoTrackGenerator>> create(ScriptExecutionContext&);
    ~VideoTrackGenerator();

    void setMuted(ScriptExecutionContext&, bool);
    bool muted(ScriptExecutionContext&) const { return m_muted; }

    Ref<WritableStream> writable();
    Ref<MediaStreamTrack> track();

private:
    class Sink;
    VideoTrackGenerator(Ref<Sink>&&, Ref<WritableStream>&&, Ref<MediaStreamTrack>&&);

    class Source final : public RealtimeMediaSource, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Source, WTF::DestructionThread::MainRunLoop> {
    public:
        static Ref<Source> create(ScriptExecutionContextIdentifier identifier) { return adoptRef(*new Source(identifier)); }

        void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Source, WTF::DestructionThread::MainRunLoop>::ref(); }
        void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Source, WTF::DestructionThread::MainRunLoop>::deref(); }
        ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Source, WTF::DestructionThread::MainRunLoop>::controlBlock(); }

        void writeVideoFrame(VideoFrame&, VideoFrameTimeMetadata);

        void setWritable(WritableStream&);

    private:
        explicit Source(ScriptExecutionContextIdentifier);

        const RealtimeMediaSourceCapabilities& capabilities() final { return m_capabilities; }
        const RealtimeMediaSourceSettings& settings() final { return m_settings; }
        void endProducingData() final;

        ScriptExecutionContextIdentifier m_contextIdentifier;
        WeakPtr<WritableStream> m_writable;

        RealtimeMediaSourceCapabilities m_capabilities;
        RealtimeMediaSourceSettings m_settings;
        IntSize m_videoFrameSize;
        IntSize m_maxVideoFrameSize;
    };

    class Sink final : public WritableStreamSink {
    public:
        static Ref<Sink> create(Ref<Source>&& source) { return adoptRef(*new Sink(WTFMove(source))); }

        void setMuted(bool muted) { m_muted = muted; }

    private:
        explicit Sink(Ref<Source>&&);

        void write(ScriptExecutionContext&, JSC::JSValue, DOMPromiseDeferred<void>&&) final;
        void close() final;
        void error(String&&) final;

        bool m_muted { false };
        Ref<Source> m_source;
    };

    bool m_muted { false };
    bool m_hasMutedChanged { false };
    Ref<Sink> m_sink;
    Ref<WritableStream> m_writable;
    Ref<MediaStreamTrack> m_track;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && ENABLE(WEB_CODECS)
