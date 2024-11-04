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
#include "MediaRecorderPrivateEncoder.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>

using CVPixelBufferRef = struct __CVBuffer*;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebCore {

class ContentType;
class Document;
class MediaStreamPrivate;
class WebAudioBufferList;

class MediaRecorderPrivateAVFImpl final
    : public MediaRecorderPrivate {
    WTF_MAKE_TZONE_ALLOCATED(MediaRecorderPrivateAVFImpl);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaRecorderPrivateAVFImpl);
public:
    static std::unique_ptr<MediaRecorderPrivateAVFImpl> create(MediaStreamPrivate&, const MediaRecorderPrivateOptions&);
    ~MediaRecorderPrivateAVFImpl();

    static bool isTypeSupported(Document&, ContentType&);

private:
    explicit MediaRecorderPrivateAVFImpl(Ref<MediaRecorderPrivateEncoder>&&);

    // MediaRecorderPrivate
    void videoFrameAvailable(VideoFrame&, VideoFrameTimeMetadata) final;
    void fetchData(FetchDataCallback&&) final;
    void audioSamplesAvailable(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;
    void startRecording(StartRecordingCallback&&) final;
    String mimeType() const final;

    void stopRecording(CompletionHandler<void()>&&) final;
    void pauseRecording(CompletionHandler<void()>&&) final;
    void resumeRecording(CompletionHandler<void()>&&) final;

    const Ref<MediaRecorderPrivateEncoder> m_encoder;
    RefPtr<VideoFrame> m_blackFrame;
    std::optional<CAAudioStreamDescription> m_description;
    std::unique_ptr<WebAudioBufferList> m_audioBuffer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
