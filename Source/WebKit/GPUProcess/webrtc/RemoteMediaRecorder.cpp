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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteVideoFrameObjectHeap.h"
#include "SharedCARingBuffer.h"
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/CompletionHandler.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, (&m_gpuConnectionToWebProcess.connection()))

namespace WebKit {
using namespace WebCore;
static constexpr Seconds mediaRecorderDefaultTimeout { 1_s };

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
    , m_recordAudio(recordAudio)
    , m_sharedVideoFrameReader(Ref { gpuConnectionToWebProcess.videoFrameObjectHeap() }, { gpuConnectionToWebProcess.webProcessIdentity() })
{
}

RemoteMediaRecorder::~RemoteMediaRecorder()
{
}

void RemoteMediaRecorder::audioSamplesStorageChanged(ConsumerSharedCARingBuffer::Handle&& handle, const WebCore::CAAudioStreamDescription& description)
{
    MESSAGE_CHECK(m_recordAudio);
    m_audioBufferList = nullptr;
    m_ringBuffer = ConsumerSharedCARingBuffer::map(description, WTFMove(handle));
    if (!m_ringBuffer)
        return;
    m_description = description;
    m_audioBufferList = makeUnique<WebAudioBufferList>(*m_description);
}

void RemoteMediaRecorder::audioSamplesAvailable(MediaTime time, uint64_t numberOfFrames)
{
    MESSAGE_CHECK(m_ringBuffer);
    MESSAGE_CHECK(m_audioBufferList);
    MESSAGE_CHECK(m_description && WebAudioBufferList::isSupportedDescription(*m_description, numberOfFrames));

    m_audioBufferList->setSampleCount(numberOfFrames);
    m_ringBuffer->fetch(m_audioBufferList->list(), numberOfFrames, time.timeValue());

    m_writer->appendAudioSampleBuffer(*m_audioBufferList, *m_description, time, numberOfFrames);
}

void RemoteMediaRecorder::videoFrameAvailable(SharedVideoFrame&& sharedVideoFrame)
{
    if (auto frame = m_sharedVideoFrameReader.read(WTFMove(sharedVideoFrame)))
        m_writer->appendVideoFrame(*frame);
}

void RemoteMediaRecorder::fetchData(CompletionHandler<void(IPC::DataReference&&, double)>&& completionHandler)
{
    m_writer->fetchData([completionHandler = WTFMove(completionHandler)](auto&& data, auto timeCode) mutable {
        auto buffer = data ? data->makeContiguous() : RefPtr<WebCore::SharedBuffer>();
        auto* pointer = buffer ? buffer->data() : nullptr;
        completionHandler(IPC::DataReference { pointer, data ? data->size() : 0 }, timeCode);
    });
}

void RemoteMediaRecorder::stopRecording(CompletionHandler<void()>&& completionHandler)
{
    m_writer->stopRecording();
    completionHandler();
}

void RemoteMediaRecorder::pause(CompletionHandler<void()>&& completionHandler)
{
    m_writer->pause();
    completionHandler();
}

void RemoteMediaRecorder::resume(CompletionHandler<void()>&& completionHandler)
{
    m_writer->resume();
    completionHandler();
}

void RemoteMediaRecorder::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteMediaRecorder::setSharedVideoFrameMemory(const SharedMemory::Handle& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(handle);
}

}

#undef MESSAGE_CHECK

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
