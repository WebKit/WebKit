/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CAAudioStreamDescription.h"
#include "MediaRecorderPrivate.h"
#include "MediaRecorderPrivateWriterCocoa.h"

using CVPixelBufferRef = struct __CVBuffer*;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebCore {

class MediaStreamPrivate;
class WebAudioBufferList;

class MediaRecorderPrivateAVFImpl final
    : public MediaRecorderPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<MediaRecorderPrivateAVFImpl> create(MediaStreamPrivate&, const MediaRecorderPrivateOptions&);
    ~MediaRecorderPrivateAVFImpl();

private:
    explicit MediaRecorderPrivateAVFImpl(Ref<MediaRecorderPrivateWriter>&&);

    // MediaRecorderPrivate
    void videoFrameAvailable(VideoFrame&, VideoFrameTimeMetadata) final;
    void fetchData(FetchDataCallback&&) final;
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;
    void startRecording(StartRecordingCallback&&) final;
    const String& mimeType() const final;

    void stopRecording(CompletionHandler<void()>&&) final;
    void pauseRecording(CompletionHandler<void()>&&) final;
    void resumeRecording(CompletionHandler<void()>&&) final;

    Ref<MediaRecorderPrivateWriter> m_writer;
    RefPtr<VideoFrame> m_blackFrame;
    std::optional<CAAudioStreamDescription> m_description;
    std::unique_ptr<WebAudioBufferList> m_audioBuffer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
