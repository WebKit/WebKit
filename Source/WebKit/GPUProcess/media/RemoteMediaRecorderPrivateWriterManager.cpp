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
#include "RemoteMediaRecorderPrivateWriterManager.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "RemoteMediaRecorderPrivateWriterIdentifier.h"
#include "RemoteMediaResourceManagerMessages.h"
#include <WebCore/MediaSample.h>
#include <wtf/Deque.h>
#include <wtf/TZoneMallocInlines.h>

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaRecorderPrivateWriterManager);

class RemoteMediaRecorderPrivateWriterProxy : public WebCore::MediaRecorderPrivateWriterListener {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaRecorderPrivateWriterProxy);
public:
    static Ref<RemoteMediaRecorderPrivateWriterProxy> create(const String& mimeType) { return adoptRef(*new RemoteMediaRecorderPrivateWriterProxy(mimeType)); }

    std::optional<uint8_t> addAudioTrack(const AudioInfo& description)
    {
        return m_writer->addAudioTrack(description);
    }

    std::optional<uint8_t> addVideoTrack(const VideoInfo& description, const std::optional<CGAffineTransform>& transform)
    {
        return m_writer->addVideoTrack(description, transform);
    }

    bool allTracksAdded()
    {
        return m_writer->allTracksAdded();
    }

    Ref<WebCore::MediaRecorderPrivateWriter::WriterPromise> writeFrames(Deque<UniqueRef<MediaSamplesBlock>>&& samples, const MediaTime& time)
    {
        return m_writer->writeFrames(WTFMove(samples), time);
    }

    Ref<GenericPromise> close()
    {
        return m_writer->close();
    }

    Ref<SharedBuffer> takeData()
    {
        Locker locker { m_lock };
        return m_data.takeAsContiguous();
    }

private:
    RemoteMediaRecorderPrivateWriterProxy(const String& mimeType)
        : m_writer(makeUniqueRefFromNonNullUniquePtr(MediaRecorderPrivateWriter::create(mimeType, *this)))
    {
    }

    void appendData(std::span<const uint8_t> data) final
    {
        Locker locker { m_lock };
        m_data.append(data);
    }

    const UniqueRef<MediaRecorderPrivateWriter> m_writer;
    Lock m_lock;
    SharedBufferBuilder m_data WTF_GUARDED_BY_LOCK(m_lock);
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaRecorderPrivateWriterProxy);

RemoteMediaRecorderPrivateWriterManager::RemoteMediaRecorderPrivateWriterManager(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
{
}

RemoteMediaRecorderPrivateWriterManager::~RemoteMediaRecorderPrivateWriterManager() = default;

void RemoteMediaRecorderPrivateWriterManager::ref() const
{
    m_gpuConnectionToWebProcess.get()->ref();
}

void RemoteMediaRecorderPrivateWriterManager::deref() const
{
    m_gpuConnectionToWebProcess.get()->deref();
}

void RemoteMediaRecorderPrivateWriterManager::create(RemoteMediaRecorderPrivateWriterIdentifier identifier, const String& mimeType)
{
    ASSERT(!m_remoteMediaRecorderPrivateWriters.contains(identifier));

    m_remoteMediaRecorderPrivateWriters.add(identifier, Writer { RemoteMediaRecorderPrivateWriterProxy::create(mimeType) });
}

void RemoteMediaRecorderPrivateWriterManager::addAudioTrack(RemoteMediaRecorderPrivateWriterIdentifier identifier, RemoteAudioInfo info, CompletionHandler<void(std::optional<uint8_t>)>&& completionHandler)
{
    ASSERT(m_remoteMediaRecorderPrivateWriters.contains(identifier));
    auto iterator = m_remoteMediaRecorderPrivateWriters.find(identifier);
    if (iterator == m_remoteMediaRecorderPrivateWriters.end())
        return;

    Ref audioInfo = info.toAudioInfo();
    iterator->value.audioInfo = audioInfo.ptr();
    RefPtr writer = iterator->value.proxy;
    auto result = writer->addAudioTrack(audioInfo.get());
    if (result)
        audioInfo->trackID = *result;
    completionHandler(result);
}

void RemoteMediaRecorderPrivateWriterManager::addVideoTrack(RemoteMediaRecorderPrivateWriterIdentifier identifier, RemoteVideoInfo info, std::optional<CGAffineTransform> transform, CompletionHandler<void(std::optional<uint8_t>)>&& completionHandler)
{
    ASSERT(m_remoteMediaRecorderPrivateWriters.contains(identifier));
    auto iterator = m_remoteMediaRecorderPrivateWriters.find(identifier);
    if (iterator == m_remoteMediaRecorderPrivateWriters.end())
        return;

    Ref videoInfo = info.toVideoInfo();
    iterator->value.videoInfo = videoInfo.ptr();
    RefPtr writer = iterator->value.proxy;
    auto result = writer->addVideoTrack(videoInfo.get(), transform);
    if (result)
        videoInfo->trackID = *result;
    completionHandler(result);
}

void RemoteMediaRecorderPrivateWriterManager::allTracksAdded(RemoteMediaRecorderPrivateWriterIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(m_remoteMediaRecorderPrivateWriters.contains(identifier));

    RefPtr writer = m_remoteMediaRecorderPrivateWriters.get(identifier).proxy;
    completionHandler(writer ? writer->allTracksAdded() : false);
}

void RemoteMediaRecorderPrivateWriterManager::writeFrames(RemoteMediaRecorderPrivateWriterIdentifier identifier, Vector<BlockPair>&& vectorSamples, const MediaTime& endTime, CompletionHandler<void(Expected<Ref<WebCore::SharedBuffer>, WebCore::MediaRecorderPrivateWriter::Result>)>&& completionHandler)
{
    ASSERT(m_remoteMediaRecorderPrivateWriters.contains(identifier));
    auto iterator = m_remoteMediaRecorderPrivateWriters.find(identifier);
    if (iterator == m_remoteMediaRecorderPrivateWriters.end())
        return;

    RefPtr audioInfo = iterator->value.audioInfo;
    RefPtr videoInfo = iterator->value.videoInfo;

    Deque<UniqueRef<MediaSamplesBlock>> samples;
    for (auto& sample : vectorSamples)
        samples.append(makeUniqueRef<MediaSamplesBlock>(sample.first == TrackInfo::TrackType::Audio ? audioInfo.get() : static_cast<TrackInfo*>(videoInfo.get()), WTFMove(sample.second)));

    RefPtr writer = m_remoteMediaRecorderPrivateWriters.get(identifier).proxy;
    writer->writeFrames(WTFMove(samples), endTime)->whenSettled(RunLoop::protectedMain(), [writer, completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        if (!result) {
            completionHandler(makeUnexpected(result.error()));
            return;
        }
        completionHandler(writer->takeData());
    });
}

void RemoteMediaRecorderPrivateWriterManager::close(RemoteMediaRecorderPrivateWriterIdentifier identifier, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>)>&& completionHandler)
{
    if (!m_remoteMediaRecorderPrivateWriters.contains(identifier)) {
        // Failsafe if already closed.
        completionHandler(SharedBuffer::create());
        return;
    }

    RefPtr writer = m_remoteMediaRecorderPrivateWriters.take(identifier).proxy;
    writer->close()->whenSettled(RunLoop::protectedMain(), [writer, completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(writer->takeData());
    });
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaRecorderPrivateWriterManager::sharedPreferencesForWebProcess() const
{
    if (RefPtr gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return gpuConnectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_RECORDER)

