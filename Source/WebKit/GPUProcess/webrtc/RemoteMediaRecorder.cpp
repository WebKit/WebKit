/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteMediaRecorder.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "Connection.h"
#include "SharedRingBufferStorage.h"
#include <WebCore/CARingBuffer.h>
#include <WebCore/ImageTransferSessionVT.h>
#include <WebCore/RemoteVideoSample.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/CompletionHandler.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, (&m_gpuConnectionToWebProcess.connection()))

namespace WebKit {
using namespace WebCore;

std::unique_ptr<RemoteMediaRecorder> RemoteMediaRecorder::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, MediaRecorderIdentifier identifier, bool recordAudio, bool recordVideo, const MediaRecorderPrivateOptions& options)
{
    auto writer = MediaRecorderPrivateWriter::create(recordAudio, recordVideo, options);
    if (!writer)
        return nullptr;
    return std::unique_ptr<RemoteMediaRecorder>(new RemoteMediaRecorder { gpuConnectionToWebProcess, identifier, writer.releaseNonNull(), recordAudio });
}

RemoteMediaRecorder::RemoteMediaRecorder(GPUConnectionToWebProcess& gpuConnectionToWebProcess, MediaRecorderIdentifier identifier, Ref<MediaRecorderPrivateWriter>&& writer, bool recordAudio)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_identifier(identifier)
    , m_writer(WTFMove(writer))
{
    if (recordAudio)
        m_ringBuffer = makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(nullptr));
}

RemoteMediaRecorder::~RemoteMediaRecorder()
{
}

SharedRingBufferStorage& RemoteMediaRecorder::storage()
{
    return static_cast<SharedRingBufferStorage&>(m_ringBuffer->storage());
}

void RemoteMediaRecorder::audioSamplesStorageChanged(const SharedMemory::IPCHandle& ipcHandle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    MESSAGE_CHECK(m_ringBuffer);

    m_description = description;

    if (ipcHandle.handle.isNull()) {
        m_ringBuffer->deallocate();
        storage().setReadOnly(false);
        storage().setStorage(nullptr);
        return;
    }

    auto memory = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);
    storage().setStorage(WTFMove(memory));
    storage().setReadOnly(true);

    m_ringBuffer->allocate(m_description, numberOfFrames);
    m_audioBufferList = makeUnique<WebAudioBufferList>(m_description);
}

void RemoteMediaRecorder::audioSamplesAvailable(MediaTime time, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame)
{
    MESSAGE_CHECK(m_ringBuffer);
    MESSAGE_CHECK(m_audioBufferList);
    MESSAGE_CHECK(WebAudioBufferList::isSupportedDescription(m_description, numberOfFrames));

    m_ringBuffer->setCurrentFrameBounds(startFrame, endFrame);

    m_audioBufferList->setSampleCount(numberOfFrames);
    m_ringBuffer->fetch(m_audioBufferList->list(), numberOfFrames, time.timeValue());

    m_writer->appendAudioSampleBuffer(*m_audioBufferList, m_description, time, numberOfFrames);
}

void RemoteMediaRecorder::videoSampleAvailable(WebCore::RemoteVideoSample&& remoteSample)
{
    if (!m_imageTransferSession || m_imageTransferSession->pixelFormat() != remoteSample.videoFormat())
        m_imageTransferSession = ImageTransferSessionVT::create(remoteSample.videoFormat());

    if (!m_imageTransferSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sampleBuffer = m_imageTransferSession->createMediaSample(remoteSample);
    if (!sampleBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_writer->appendVideoSampleBuffer(sampleBuffer->platformSample().sample.cmSampleBuffer);
}

void RemoteMediaRecorder::fetchData(CompletionHandler<void(IPC::DataReference&&, const String& mimeType)>&& completionHandler)
{
    static NeverDestroyed<const String> mp4MimeType(MAKE_STATIC_STRING_IMPL("video/mp4"));
    m_writer->fetchData([&mp4MimeType = mp4MimeType.get(), completionHandler = WTFMove(completionHandler)](auto&& data) mutable {
        auto* pointer = reinterpret_cast<const uint8_t*>(data ? data->data() : nullptr);
        completionHandler(IPC::DataReference { pointer, data ? data->size() : 0 }, mp4MimeType);
    });
}

void RemoteMediaRecorder::stopRecording()
{
    m_writer->stopRecording();
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
