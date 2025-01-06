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

#include "MessageReceiver.h"
#include "RemoteMediaRecorderPrivateWriter.h"
#include "RemoteMediaRecorderPrivateWriterIdentifier.h"
#include "RemoteTrackInfo.h"
#include <WebCore/MediaSample.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Connection;
class Decoder;
class Encoder;

template<> struct ArgumentCoder<WebCore::MediaSamplesBlock::MediaSampleItem> {
    static void encode(Encoder&, const WebCore::MediaSamplesBlock::MediaSampleItem&);
    static std::optional<WebCore::MediaSamplesBlock::MediaSampleItem> decode(Decoder&);
};
}

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteMediaRecorderPrivateWriterProxy;
struct SharedPreferencesForWebProcess;

class RemoteMediaRecorderPrivateWriterManager : public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaRecorderPrivateWriterManager);
    WTF_MAKE_NONCOPYABLE(RemoteMediaRecorderPrivateWriterManager);
public:
    RemoteMediaRecorderPrivateWriterManager(GPUConnectionToWebProcess&);
    ~RemoteMediaRecorderPrivateWriterManager();

    // IPC::MessageReceiver
    void ref() const final;
    void deref() const final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    bool allowsExitUnderMemoryPressure() { return m_remoteMediaRecorderPrivateWriters.isEmpty(); }

    // Messages.
    void create(RemoteMediaRecorderPrivateWriterIdentifier, const String&);
    void addMediaRecorderPrivateWriter(RemoteMediaRecorderPrivateWriterIdentifier);
    void addAudioTrack(RemoteMediaRecorderPrivateWriterIdentifier, RemoteAudioInfo, CompletionHandler<void(std::optional<uint8_t>)>&&);
    void addVideoTrack(RemoteMediaRecorderPrivateWriterIdentifier, RemoteVideoInfo, std::optional<CGAffineTransform>, CompletionHandler<void(std::optional<uint8_t>)>&&);
    void allTracksAdded(RemoteMediaRecorderPrivateWriterIdentifier, CompletionHandler<void(bool)>&&);
    using BlockPair = std::pair<WebCore::TrackInfo::TrackType, Vector<WebCore::MediaSamplesBlock::MediaSampleItem>>;
    void writeFrames(RemoteMediaRecorderPrivateWriterIdentifier, Vector<BlockPair>&&, const MediaTime&, CompletionHandler<void(Expected<Ref<WebCore::SharedBuffer>, WebCore::MediaRecorderPrivateWriter::Result>)>&&);
    void close(RemoteMediaRecorderPrivateWriterIdentifier, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>)>&&);

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

private:
    HashMap<RemoteMediaRecorderPrivateWriterIdentifier, Ref<RemoteMediaRecorderPrivateWriterProxy>> m_remoteMediaRecorderPrivateWriters;
    RefPtr<WebCore::AudioInfo> m_audioInfo;
    RefPtr<WebCore::VideoInfo> m_videoInfo;

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
