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

#include "config.h"
#include "RemoteMediaRecorderPrivateWriter.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER)

#include "Connection.h"
#include "GPUProcessConnection.h"
#include "RemoteMediaRecorderPrivateWriterManagerMessages.h"
#include "RemoteTrackInfo.h"
#include <WebCore/MediaSample.h>
#include <wtf/MainThreadDispatcher.h>
#include <wtf/TZoneMallocInlines.h>

namespace IPC {
class Decoder;
class Encoder;
template<> struct ArgumentCoder<WebCore::MediaSamplesBlock::MediaSampleItem> {
    static void encode(Encoder&, const WebCore::MediaSamplesBlock::MediaSampleItem&);
    static std::optional<WebCore::MediaSamplesBlock::MediaSampleItem> decode(Decoder&);
};
}

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaRecorderPrivateWriter);

std::unique_ptr<WebCore::MediaRecorderPrivateWriter> RemoteMediaRecorderPrivateWriter::create(GPUProcessConnection& gpuProcessConnection, const String& type, MediaRecorderPrivateWriterListener& listener)
{
    return std::unique_ptr<MediaRecorderPrivateWriter> { new RemoteMediaRecorderPrivateWriter(gpuProcessConnection, type, listener) };
}

RemoteMediaRecorderPrivateWriter::RemoteMediaRecorderPrivateWriter(GPUProcessConnection& gpuProcessConnection, const String& type, MediaRecorderPrivateWriterListener& listener)
    : m_gpuProcessConnection(gpuProcessConnection)
    , m_listener(listener)
    , m_remoteMediaRecorderPrivateWriterIdentifier(RemoteMediaRecorderPrivateWriterIdentifier::generate())
{
    if (RefPtr gpuProcessConnection = m_gpuProcessConnection.get())
        gpuProcessConnection->protectedConnection()->send(Messages::RemoteMediaRecorderPrivateWriterManager::Create(m_remoteMediaRecorderPrivateWriterIdentifier, type), 0);
}

std::optional<uint8_t> RemoteMediaRecorderPrivateWriter::addAudioTrack(const AudioInfo& info)
{
    std::optional<uint8_t> trackNumber;
    callOnMainRunLoopAndWait([&] {
        if (RefPtr gpuProcessConnection = m_gpuProcessConnection.get()) {
            auto sendResult = gpuProcessConnection->protectedConnection()->sendSync(Messages::RemoteMediaRecorderPrivateWriterManager::AddAudioTrack(m_remoteMediaRecorderPrivateWriterIdentifier, RemoteAudioInfo { info }), 0);
            std::tie(trackNumber) = sendResult.takeReplyOr(std::optional<uint8_t> { });
        }
    });
    return trackNumber;
}

std::optional<uint8_t> RemoteMediaRecorderPrivateWriter::addVideoTrack(const VideoInfo& info, const std::optional<CGAffineTransform>& transform)
{
    std::optional<uint8_t> trackNumber;
    callOnMainRunLoopAndWait([&] {
        if (RefPtr gpuProcessConnection = m_gpuProcessConnection.get()) {
            auto sendResult = gpuProcessConnection->protectedConnection()->sendSync(Messages::RemoteMediaRecorderPrivateWriterManager::AddVideoTrack(m_remoteMediaRecorderPrivateWriterIdentifier, RemoteVideoInfo { info }, transform), 0);
            std::tie(trackNumber) = sendResult.takeReplyOr(std::optional<uint8_t> { });
        }
    });
    return trackNumber;
}

bool RemoteMediaRecorderPrivateWriter::allTracksAdded()
{
    bool result = false;
    callOnMainRunLoopAndWait([&] {
        if (RefPtr gpuProcessConnection = m_gpuProcessConnection.get()) {
            auto sendResult = gpuProcessConnection->protectedConnection()->sendSync(Messages::RemoteMediaRecorderPrivateWriterManager::AllTracksAdded(m_remoteMediaRecorderPrivateWriterIdentifier), 0);
            std::tie(result) = sendResult.takeReplyOr(false);
        }
    });
    return result;
}

Ref<MediaRecorderPrivateWriter::WriterPromise> RemoteMediaRecorderPrivateWriter::writeFrames(Deque<UniqueRef<MediaSamplesBlock>>&& samples, const MediaTime& endTime)
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection)
        return WriterPromise::createAndReject(Result::Failure);

    // FIXME: Should serialize with serialization.in file. Don't understand how that works. Keep getting static error about class sizes not matching.
    using BlockPair = std::pair<TrackInfo::TrackType, Vector<MediaSamplesBlock::MediaSampleItem>>;
    Vector<BlockPair> vectorSamples(samples.size(), [&](auto) {
        UniqueRef sample = samples.takeFirst();
        auto samples = sample->takeSamples();
        return BlockPair { sample->info()->type(), WTFMove(samples) };
    });
    return gpuProcessConnection->protectedConnection()->sendWithPromisedReply(Messages::RemoteMediaRecorderPrivateWriterManager::WriteFrames(m_remoteMediaRecorderPrivateWriterIdentifier, WTFMove(vectorSamples), endTime), 0)->whenSettled(MainThreadDispatcher::singleton(), [listener = m_listener](auto&& result) {
        RefPtr strongListener = listener.get();
        if (!strongListener || !result)
            return WriterPromise::createAndReject(Result::Failure); // IPCError.
        if (*result) {
            strongListener->appendData((*result).value()->span());
            return WriterPromise::createAndResolve();
        }
        return WriterPromise::createAndReject((*result).error());
    });
}

RemoteMediaRecorderPrivateWriter::Result RemoteMediaRecorderPrivateWriter::writeFrame(const MediaSamplesBlock&)
{
    ASSERT_NOT_REACHED();
    return Result::Failure;
}

Ref<GenericPromise> RemoteMediaRecorderPrivateWriter::close()
{
    m_isClosed = true;
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection)
        return GenericPromise::createAndReject();
    return gpuProcessConnection->protectedConnection()->sendWithPromisedReply(Messages::RemoteMediaRecorderPrivateWriterManager::Close(m_remoteMediaRecorderPrivateWriterIdentifier), 0)->whenSettled(MainThreadDispatcher::singleton(), [listener = m_listener](auto&& result) {
        RefPtr strongListener = listener.get();
        if (!strongListener || !result)
            return GenericPromise::createAndReject();
        if (RefPtr sharedBuffer = *result)
            strongListener->appendData(sharedBuffer->span());
        return GenericPromise::createAndResolve();
    });
}

Ref<GenericPromise> RemoteMediaRecorderPrivateWriter::close(const MediaTime& endTime)
{
    ASSERT_NOT_REACHED();
    return GenericPromise::createAndReject();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER)
