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

#if ENABLE(MEDIA_RECORDER_WEBM)

#include "MediaRecorderPrivateWriter.h"
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class MediaRecorderPrivateWriterWebMDelegate;

class MediaRecorderPrivateWriterWebM final : public MediaRecorderPrivateWriter {
    WTF_MAKE_TZONE_ALLOCATED(MediaRecorderPrivateWriterWebM);
public:
    static std::unique_ptr<MediaRecorderPrivateWriter> create(MediaRecorderPrivateWriterListener&);

    ~MediaRecorderPrivateWriterWebM();
private:
    MediaRecorderPrivateWriterWebM(MediaRecorderPrivateWriterListener&);

    std::optional<uint8_t> addAudioTrack(CMFormatDescriptionRef) final;
    std::optional<uint8_t> addVideoTrack(CMFormatDescriptionRef, const std::optional<CGAffineTransform>&) final;
    bool allTracksAdded() final { return true; }
    Result muxFrame(const MediaSample&, uint8_t) final;
    void forceNewSegment(const WTF::MediaTime&) final;
    Ref<GenericPromise> close(const WTF::MediaTime&) final;

    const UniqueRef<MediaRecorderPrivateWriterWebMDelegate> m_delegate;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
