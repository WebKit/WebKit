/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER)

#include "RemoteMediaRecorderPrivateWriterIdentifier.h"
#include <WebCore/MediaRecorderPrivateWriter.h>
#include <WebCore/PlatformMediaResourceLoader.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/TZoneMalloc.h>

typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebKit {

class GPUProcessConnection;

class RemoteMediaRecorderPrivateWriter final : public WebCore::MediaRecorderPrivateWriter {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaRecorderPrivateWriter);
public:
    static std::unique_ptr<MediaRecorderPrivateWriter> create(GPUProcessConnection&, const String& type, WebCore::MediaRecorderPrivateWriterListener&);

private:
    RemoteMediaRecorderPrivateWriter(GPUProcessConnection&, const String& type, WebCore::MediaRecorderPrivateWriterListener&);
    std::optional<uint8_t> addAudioTrack(const WebCore::AudioInfo&) final;
    std::optional<uint8_t> addVideoTrack(const WebCore::VideoInfo&, const std::optional<CGAffineTransform>&) final;
    bool allTracksAdded() final;
    Ref<WriterPromise> writeFrames(Deque<UniqueRef<WebCore::MediaSamplesBlock>>&&, const MediaTime&) final;
    Result writeFrame(const WebCore::MediaSamplesBlock&) final;
    void forceNewSegment(const MediaTime&) final { };
    Ref<GenericPromise> close() final;
    Ref<GenericPromise> close(const MediaTime&) final;

    bool m_isClosed { false };
    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    ThreadSafeWeakPtr<WebCore::MediaRecorderPrivateWriterListener> m_listener;
    const RemoteMediaRecorderPrivateWriterIdentifier m_remoteMediaRecorderPrivateWriterIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA) ENABLE(MEDIA_RECORDER)

