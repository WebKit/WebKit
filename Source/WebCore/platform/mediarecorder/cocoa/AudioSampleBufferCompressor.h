/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_RECORDER) && USE(AVFOUNDATION)

#include <CoreMedia/CoreMedia.h>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;
typedef struct OpaqueAudioConverter* AudioConverterRef;
OBJC_CLASS NSNumber;

namespace WebCore {

class AudioSampleBufferCompressor : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioSampleBufferCompressor, WTF::DestructionThread::Main> {
public:
    static RefPtr<AudioSampleBufferCompressor> create(CMBufferQueueTriggerCallback, void* callbackObject, AudioFormatID = kAudioFormatMPEG4AAC);
    ~AudioSampleBufferCompressor();

    bool isEmpty() const;
    void setBitsPerSecond(unsigned);
    Ref<GenericPromise> finish() { return flushInternal(true); }
    Ref<GenericPromise> flush() { return flushInternal(false); }
    Ref<GenericPromise> drain();
    void addSampleBuffer(CMSampleBufferRef);
    CMSampleBufferRef getOutputSampleBuffer() const;
    RetainPtr<CMSampleBufferRef> takeOutputSampleBuffer();

    unsigned bitRate() const;

private:
    explicit AudioSampleBufferCompressor(AudioFormatID);
    bool initialize(CMBufferQueueTriggerCallback, void* callbackObject);
    UInt32 defaultOutputBitRate(const AudioStreamBasicDescription&) const;

    static OSStatus audioConverterComplexInputDataProc(AudioConverterRef, UInt32*, AudioBufferList*, AudioStreamPacketDescription**, void*);

    void processSampleBuffer(CMSampleBufferRef);
    bool initAudioConverterForSourceFormatDescription(CMFormatDescriptionRef, AudioFormatID);
    void attachPrimingTrimsIfNeeded(CMSampleBufferRef);
    RetainPtr<NSNumber> gradualDecoderRefreshCount();
    RetainPtr<CMSampleBufferRef> sampleBuffer(AudioBufferList);
    void processSampleBuffers();
    OSStatus provideSourceDataNumOutputPackets(UInt32*, AudioBufferList*, AudioStreamPacketDescription**);
    Ref<GenericPromise> flushInternal(bool isFinished);

    Ref<WorkQueue> m_serialDispatchQueue;
    CMTime m_lowWaterTime;

    RetainPtr<CMBufferQueueRef> m_outputBufferQueue;
    RetainPtr<CMBufferQueueRef> m_inputBufferQueue;
    bool m_isEncoding { false };
    bool m_isDraining { false };

    AudioConverterRef m_converter { nullptr };
    AudioStreamBasicDescription m_sourceFormat;
    AudioStreamBasicDescription m_destinationFormat;
    RetainPtr<CMFormatDescriptionRef> m_destinationFormatDescription;
    RetainPtr<NSNumber> m_gdrCountNum;
    UInt32 m_maxOutputPacketSize { 0 };
    AudioStreamPacketDescription m_destinationPacketDescriptions;

    CMTime m_currentNativePresentationTimeStamp;
    CMTime m_currentOutputPresentationTimeStamp;
    CMTime m_remainingPrimeDuration;

    Vector<uint8_t> m_sourceBuffer;
    Vector<uint8_t> m_destinationBuffer;

    CMBufferQueueTriggerToken m_triggerToken;
    RetainPtr<CMBlockBufferRef> m_sampleBlockBuffer;
    size_t m_sampleBlockBufferSize { 0 };
    size_t m_currentOffsetInSampleBlockBuffer { 0 };
    const AudioFormatID m_outputCodecType;
    std::optional<unsigned> m_outputBitRate;
};

}

#endif // ENABLE(MEDIA_RECORDER) && USE(AVFOUNDATION)
