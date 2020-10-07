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

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import <CoreMedia/CoreMedia.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct OpaqueAudioConverter* AudioConverterRef;

namespace WebCore {

class AudioSampleBufferCompressor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<AudioSampleBufferCompressor> create(CMBufferQueueTriggerCallback, void* callbackObject);
    ~AudioSampleBufferCompressor();

    void setBitsPerSecond(unsigned);
    void finish();
    void addSampleBuffer(CMSampleBufferRef);
    CMSampleBufferRef getOutputSampleBuffer();
    RetainPtr<CMSampleBufferRef> takeOutputSampleBuffer();

private:
    AudioSampleBufferCompressor();
    bool initialize(CMBufferQueueTriggerCallback, void* callbackObject);
    UInt32 outputBitRate(const AudioStreamBasicDescription&) const;

    static OSStatus audioConverterComplexInputDataProc(AudioConverterRef, UInt32*, AudioBufferList*, AudioStreamPacketDescription**, void*);

    void processSampleBuffer(CMSampleBufferRef);
    bool initAudioConverterForSourceFormatDescription(CMFormatDescriptionRef, AudioFormatID);
    size_t computeBufferSizeForAudioFormat(AudioStreamBasicDescription, UInt32, Float32);
    void attachPrimingTrimsIfNeeded(CMSampleBufferRef);
    RetainPtr<NSNumber> gradualDecoderRefreshCount();
    CMSampleBufferRef sampleBufferWithNumPackets(UInt32 numPackets, AudioBufferList);
    void processSampleBuffersUntilLowWaterTime(CMTime);
    OSStatus provideSourceDataNumOutputPackets(UInt32*, AudioBufferList*, AudioStreamPacketDescription**);

    dispatch_queue_t m_serialDispatchQueue;
    CMTime m_lowWaterTime { kCMTimeInvalid };

    RetainPtr<CMBufferQueueRef> m_outputBufferQueue;
    RetainPtr<CMBufferQueueRef> m_inputBufferQueue;
    bool m_isEncoding { false };

    AudioConverterRef m_converter { nullptr };
    AudioStreamBasicDescription m_sourceFormat;
    AudioStreamBasicDescription m_destinationFormat;
    RetainPtr<CMFormatDescriptionRef> m_destinationFormatDescription;
    RetainPtr<NSNumber> m_gdrCountNum;
    UInt32 m_maxOutputPacketSize { 0 };
    Vector<AudioStreamPacketDescription> m_destinationPacketDescriptions;

    CMTime m_currentNativePresentationTimeStamp { kCMTimeInvalid };
    CMTime m_currentOutputPresentationTimeStamp { kCMTimeInvalid };
    CMTime m_remainingPrimeDuration { kCMTimeInvalid };

    Vector<char> m_sourceBuffer;
    Vector<char> m_destinationBuffer;

    RetainPtr<CMBlockBufferRef> m_sampleBlockBuffer;
    size_t m_sampleBlockBufferSize { 0 };
    size_t m_currentOffsetInSampleBlockBuffer { 0 };
    AudioFormatID m_outputCodecType { kAudioFormatMPEG4AAC };
    Optional<unsigned> m_outputBitRate;
};

}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
