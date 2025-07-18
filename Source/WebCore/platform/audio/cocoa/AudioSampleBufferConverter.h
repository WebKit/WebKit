/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

#if USE(AVFOUNDATION)

#include "BitrateMode.h"
#include <CoreMedia/CoreMedia.h>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;
typedef struct OpaqueAudioConverter* AudioConverterRef;
OBJC_CLASS NSNumber;

namespace WebCore {

class WebAudioBufferList;

class AudioSampleBufferConverter : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioSampleBufferConverter> {
public:

#if ENABLE(WEB_CODECS)
    using BitrateMode = BitrateMode;
#else
    enum class BitrateMode {
        Constant,
        Variable
    };
#endif

    struct Options {
        AudioFormatID format { kAudioFormatMPEG4AAC };
        std::optional<AudioStreamBasicDescription> description { };
        std::optional<unsigned> outputBitRate { };
        bool generateTimestamp { true };
        std::optional<unsigned> preSkip { };
        std::optional<BitrateMode> bitrateMode { };
        std::optional<unsigned> packetSize { };
        std::optional<unsigned> complexity { };
        std::optional<unsigned> packetlossperc { };
        std::optional<bool> useinbandfec { };
        std::optional<bool> usedtx { };
    };
    static RefPtr<AudioSampleBufferConverter> create(CMBufferQueueTriggerCallback, void* callbackObject, const Options&);
    ~AudioSampleBufferConverter();

    bool isEmpty() const;
    Ref<GenericPromise> finish() { return flushInternal(true); }
    Ref<GenericPromise> flush() { return flushInternal(false); }
    Ref<GenericPromise> drain();
    Ref<GenericPromise> addSampleBuffer(CMSampleBufferRef);
    CMSampleBufferRef getOutputSampleBuffer() const;
    RetainPtr<CMSampleBufferRef> takeOutputSampleBuffer();

    unsigned bitRate() const;
    unsigned preSkip() const { return m_preSkip; }

private:
    AudioSampleBufferConverter(const Options&);
    bool initialize(CMBufferQueueTriggerCallback, void* callbackObject);
    UInt32 defaultOutputBitRate(const AudioStreamBasicDescription&) const;

    static OSStatus audioConverterComplexInputDataProc(AudioConverterRef, UInt32*, AudioBufferList*, AudioStreamPacketDescription**, void*);

    void processSampleBuffer(CMSampleBufferRef);
    OSStatus initAudioConverterForSourceFormatDescription(CMFormatDescriptionRef, AudioFormatID);
    void attachPrimingTrimsIfNeeded(CMSampleBufferRef);
    RetainPtr<NSNumber> gradualDecoderRefreshCount();
    Expected<RetainPtr<CMSampleBufferRef>, OSStatus> sampleBuffer(const WebAudioBufferList&, uint32_t numSamples);
    void processSampleBuffers();
    OSStatus provideSourceDataNumOutputPackets(UInt32*, AudioBufferList*, AudioStreamPacketDescription**);
    Ref<GenericPromise> flushInternal(bool isFinished);

    bool isPCM() const;
    void setTimeFromSample(CMSampleBufferRef);

    WorkQueue& queue() const { return m_serialDispatchQueue; }
    const Ref<WorkQueue> m_serialDispatchQueue;

    const RetainPtr<CMBufferQueueRef> m_outputBufferQueue; // initialized on the caller's thread once, never modified after that.
    const RetainPtr<CMBufferQueueRef> m_inputBufferQueue; // initialized on the caller's thread once, never modified after that.
    bool m_isEncoding WTF_GUARDED_BY_CAPABILITY(queue()) { true };
    bool m_isDraining WTF_GUARDED_BY_CAPABILITY(queue()) { false };

    AudioConverterRef m_converter WTF_GUARDED_BY_CAPABILITY(queue()) { nullptr };
    AudioStreamBasicDescription m_sourceFormat WTF_GUARDED_BY_CAPABILITY(queue());
    AudioStreamBasicDescription m_destinationFormat WTF_GUARDED_BY_CAPABILITY(queue());
    RetainPtr<CMFormatDescriptionRef> m_destinationFormatDescription WTF_GUARDED_BY_CAPABILITY(queue());
    RetainPtr<NSNumber> m_gdrCountNum WTF_GUARDED_BY_CAPABILITY(queue());
    UInt32 m_maxOutputPacketSize WTF_GUARDED_BY_CAPABILITY(queue()) { 0 };
    Vector<AudioStreamPacketDescription> m_destinationPacketDescriptions WTF_GUARDED_BY_CAPABILITY(queue());

    CMTime m_currentNativePresentationTimeStamp WTF_GUARDED_BY_CAPABILITY(queue());
    CMTime m_currentOutputPresentationTimeStamp WTF_GUARDED_BY_CAPABILITY(queue());
    CMTime m_remainingPrimeDuration WTF_GUARDED_BY_CAPABILITY(queue());

    Vector<uint8_t> m_destinationBuffer WTF_GUARDED_BY_CAPABILITY(queue());

    CMBufferQueueTriggerToken m_triggerToken;
    RetainPtr<CMBlockBufferRef> m_blockBuffer WTF_GUARDED_BY_CAPABILITY(queue());
    Vector<AudioStreamPacketDescription> m_packetDescriptions WTF_GUARDED_BY_CAPABILITY(queue());
    OSStatus m_lastError WTF_GUARDED_BY_CAPABILITY(queue()) { 0 };
    const AudioFormatID m_outputCodecType;
    const Options m_options;
    std::atomic<unsigned> m_defaultBitRate { 0 };
    std::atomic<unsigned> m_preSkip { 0 };
};

}

#endif // ENABLE(MEDIA_RECORDER) && USE(AVFOUNDATION)
