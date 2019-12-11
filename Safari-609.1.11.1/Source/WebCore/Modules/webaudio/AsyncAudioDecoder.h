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

#pragma once

#include <memory>
#include <wtf/Forward.h>
#include <wtf/MessageQueue.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class AudioBuffer;
class AudioBufferCallback;

// AsyncAudioDecoder asynchronously decodes audio file data from an ArrayBuffer in a worker thread.
// Upon successful decoding, a completion callback will be invoked with the decoded PCM data in an AudioBuffer.

class AsyncAudioDecoder final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AsyncAudioDecoder);
public:
    AsyncAudioDecoder();
    ~AsyncAudioDecoder();

    // Must be called on the main thread.
    void decodeAsync(Ref<JSC::ArrayBuffer>&& audioData, float sampleRate, RefPtr<AudioBufferCallback>&& successCallback, RefPtr<AudioBufferCallback>&& errorCallback);

private:
    class DecodingTask {
        WTF_MAKE_NONCOPYABLE(DecodingTask);
        WTF_MAKE_FAST_ALLOCATED;
    public:
        DecodingTask(Ref<JSC::ArrayBuffer>&& audioData, float sampleRate, RefPtr<AudioBufferCallback>&& successCallback, RefPtr<AudioBufferCallback>&& errorCallback);
        void decode();
        
    private:
        JSC::ArrayBuffer& audioData() { return m_audioData; }
        float sampleRate() const { return m_sampleRate; }
        AudioBufferCallback* successCallback() { return m_successCallback.get(); }
        AudioBufferCallback* errorCallback() { return m_errorCallback.get(); }
        AudioBuffer* audioBuffer() { return m_audioBuffer.get(); }

        void notifyComplete();

        Ref<JSC::ArrayBuffer> m_audioData;
        float m_sampleRate;
        RefPtr<AudioBufferCallback> m_successCallback;
        RefPtr<AudioBufferCallback> m_errorCallback;
        RefPtr<AudioBuffer> m_audioBuffer;
    };
    
    void runLoop();

    RefPtr<Thread> m_thread;
    Lock m_threadCreationMutex;
    MessageQueue<DecodingTask> m_queue;
};

} // namespace WebCore
