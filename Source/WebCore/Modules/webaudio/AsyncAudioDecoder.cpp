/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AsyncAudioDecoder.h"

#include "AudioBuffer.h"
#include "AudioBufferCallback.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/MainThread.h>

namespace WebCore {

AsyncAudioDecoder::AsyncAudioDecoder()
{
    // Start worker thread.
    LockHolder lock(m_threadCreationMutex);
    m_thread = Thread::create("Audio Decoder", [this] {
        runLoop();
    });
}

AsyncAudioDecoder::~AsyncAudioDecoder()
{
    m_queue.kill();
    
    // Stop thread.
    m_thread->waitForCompletion();
}

void AsyncAudioDecoder::decodeAsync(Ref<ArrayBuffer>&& audioData, float sampleRate, RefPtr<AudioBufferCallback>&& successCallback, RefPtr<AudioBufferCallback>&& errorCallback)
{
    ASSERT(isMainThread());

    auto decodingTask = makeUnique<DecodingTask>(WTFMove(audioData), sampleRate, WTFMove(successCallback), WTFMove(errorCallback));
    m_queue.append(WTFMove(decodingTask)); // note that ownership of the task is effectively taken by the queue.
}

void AsyncAudioDecoder::runLoop()
{
    ASSERT(!isMainThread());

    {
        // Wait for until we have m_thread established before starting the run loop.
        LockHolder lock(m_threadCreationMutex);
    }

    // Keep running decoding tasks until we're killed.
    while (auto decodingTask = m_queue.waitForMessage()) {
        // Let the task take care of its own ownership.
        // See DecodingTask::notifyComplete() for cleanup.
        decodingTask.release()->decode();
    }
}

AsyncAudioDecoder::DecodingTask::DecodingTask(Ref<ArrayBuffer>&& audioData, float sampleRate, RefPtr<AudioBufferCallback>&& successCallback, RefPtr<AudioBufferCallback>&& errorCallback)
    : m_audioData(WTFMove(audioData))
    , m_sampleRate(sampleRate)
    , m_successCallback(WTFMove(successCallback))
    , m_errorCallback(WTFMove(errorCallback))
{
}

void AsyncAudioDecoder::DecodingTask::decode()
{
    // Do the actual decoding and invoke the callback.
    m_audioBuffer = AudioBuffer::createFromAudioFileData(m_audioData->data(), m_audioData->byteLength(), false, sampleRate());
    
    // Decoding is finished, but we need to do the callbacks on the main thread.
    callOnMainThread([this] {
        notifyComplete();
    });
}

void AsyncAudioDecoder::DecodingTask::notifyComplete()
{
    if (audioBuffer() && successCallback())
        successCallback()->handleEvent(audioBuffer());
    else if (errorCallback())
        errorCallback()->handleEvent(audioBuffer());

    // Our ownership was given up in AsyncAudioDecoder::runLoop()
    // Make sure to clean up here.
    delete this;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
