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

#include "config.h"
#include "AudioSampleBufferConverter.h"

#if USE(AVFOUNDATION)

#include "CAAudioStreamDescription.h"
#include "CMUtilities.h"
#include "Logging.h"
#include "WebAudioBufferList.h"
#include <AudioToolbox/AudioCodec.h>
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFormat.h>
#include <Foundation/Foundation.h>
#include <Foundation/NSValue.h>
#include <algorithm>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/NativePromise.h>
#include <wtf/Scope.h>

#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

// Error value we pass through the converter to signal that nothing has gone wrong during encoding and we're done processing the packet.
constexpr uint32_t kNoMoreDataErr = 'MOAR';

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

RefPtr<AudioSampleBufferConverter> AudioSampleBufferConverter::create(CMBufferQueueTriggerCallback callback, void* callbackObject, const Options& options)
{
    Ref converter = adoptRef(*new AudioSampleBufferConverter(options));
    if (!converter->initialize(callback, callbackObject))
        return nullptr;
    return converter;
}

AudioSampleBufferConverter::AudioSampleBufferConverter(const Options& options)
    : m_serialDispatchQueue(WorkQueue::create("com.apple.AudioSampleBufferConverter"_s))
    , m_destinationFormat(options.description ? *options.description : AudioStreamBasicDescription { })
    , m_currentNativePresentationTimeStamp(PAL::kCMTimeInvalid)
    , m_currentOutputPresentationTimeStamp(PAL::kCMTimeInvalid)
    , m_remainingPrimeDuration(PAL::kCMTimeInvalid)
    , m_outputCodecType(options.format)
    , m_options(options)
{
}

AudioSampleBufferConverter::~AudioSampleBufferConverter()
{
    if (m_outputBufferQueue)
        PAL::CMBufferQueueRemoveTrigger(m_outputBufferQueue.get(), m_triggerToken);

    if (m_converter) {
        PAL::AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }
}

bool AudioSampleBufferConverter::initialize(CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    CMBufferQueueRef inputBufferQueue;
    if (auto error = PAL::CMBufferQueueCreate(kCFAllocatorDefault, 0, PAL::CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &inputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMBufferQueueCreate for m_inputBufferQueue failed with %d", static_cast<int>(error));
        return false;
    }
    m_inputBufferQueue = adoptCF(inputBufferQueue);

    CMBufferQueueRef outputBufferQueue;
    if (auto error = PAL::CMBufferQueueCreate(kCFAllocatorDefault, 0, PAL::CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &outputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMBufferQueueCreate for m_outputBufferQueue failed with %d", static_cast<int>(error));
        return false;
    }
    m_outputBufferQueue = adoptCF(outputBufferQueue);
    PAL::CMBufferQueueInstallTrigger(m_outputBufferQueue.get(), callback, callbackObject, kCMBufferQueueTrigger_WhenDataBecomesReady, PAL::kCMTimeZero, &m_triggerToken);

    return true;
}

Ref<GenericPromise> AudioSampleBufferConverter::drain()
{
    return invokeAsync(queue(), [weakThis = ThreadSafeWeakPtr { *this }, this] {
        assertIsCurrent(queue().get());

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndReject();

        m_isDraining = true;
        processSampleBuffers();
        m_isDraining = false;

        if (!m_converter)
            return GenericPromise::createAndReject();

        if (auto error = PAL::AudioConverterReset(m_converter)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter AudioConverterReset failed %d", static_cast<int>(error));
            return GenericPromise::createAndReject();
        }
        return GenericPromise::createAndResolve();
    });
}

Ref<GenericPromise> AudioSampleBufferConverter::flushInternal(bool isFinished)
{
    return invokeAsync(queue(), [protectedThis = Ref { *this }, this, isFinished] {
        assertIsCurrent(queue().get());

        m_isDraining = isFinished;
        processSampleBuffers();
        m_isDraining = false;

        if (!m_converter)
            return GenericPromise::createAndReject();

        if (isFinished) {
            m_isEncoding = false;

            if (auto error = PAL::CMBufferQueueMarkEndOfData(m_outputBufferQueue.get())) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMBufferQueueMarkEndOfData failed %d", static_cast<int>(error));
                return GenericPromise::createAndReject();
            }
        }
        m_lastError = noErr;

        return GenericPromise::createAndResolve();
    });
}

UInt32 AudioSampleBufferConverter::defaultOutputBitRate(const AudioStreamBasicDescription& destinationFormat) const
{
    if (destinationFormat.mSampleRate >= 44100)
        return 192000;
    if (destinationFormat.mSampleRate < 22000)
        return 32000;
    return 64000;
}

static OSStatus computeSampleRate(auto& destinationFormat)
{
    auto originalDestinationFormat = destinationFormat;
    UInt32 size = sizeof(destinationFormat);
    if (auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL, &size, &destinationFormat); error != kAudioCodecUnsupportedFormatError)
        return error;
    destinationFormat.mSampleRate = 48000;
    if (auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL, &size, &destinationFormat))
        return error;
    if (originalDestinationFormat.mFramesPerPacket) {
        // Adjust mFramesPerPacket to match new sampling rate
        destinationFormat.mFramesPerPacket = originalDestinationFormat.mFramesPerPacket / originalDestinationFormat.mSampleRate * destinationFormat.mSampleRate;
    }
    return noErr;
}

OSStatus AudioSampleBufferConverter::initAudioConverterForSourceFormatDescription(CMFormatDescriptionRef formatDescription, AudioFormatID outputFormatID)
{
    assertIsCurrent(queue().get());

    const auto *audioFormatListItem = PAL::CMAudioFormatDescriptionGetRichestDecodableFormat(formatDescription);
    m_sourceFormat = audioFormatListItem->mASBD;

    if (!m_destinationFormat.mFormatID || !m_destinationFormat.mSampleRate) {
        m_destinationFormat.mFormatID = outputFormatID;
        m_destinationFormat.mChannelsPerFrame = m_sourceFormat.mChannelsPerFrame;
        m_destinationFormat.mSampleRate = m_sourceFormat.mSampleRate;
    }

    if (auto error = computeSampleRate(m_destinationFormat))
        return error;

    AudioConverterRef converter;
    auto error = PAL::AudioConverterNew(&m_sourceFormat, &m_destinationFormat, &converter);
    if (error == kAudioConverterErr_FormatNotSupported) {
        m_destinationFormat.mSampleRate = 48000;
        error = PAL::AudioConverterNew(&m_sourceFormat, &m_destinationFormat, &converter);
    }
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter AudioConverterNew failed with %d", static_cast<int>(error));
        return error;
    }
    m_converter = converter;

    auto cleanupInCaseOfError = makeScopeExit([&] {
        assertIsCurrent(queue().get());

        PAL::AudioConverterDispose(m_converter);
        m_converter = nullptr;
    });

    size_t cookieSize = 0;
    const void *cookie = PAL::CMAudioFormatDescriptionGetMagicCookie(formatDescription, &cookieSize);
    if (cookieSize) {
        if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioConverterDecompressionMagicCookie, (UInt32)cookieSize, cookie)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting kAudioConverterDecompressionMagicCookie failed with %d", static_cast<int>(error));
            return error;
        }
    }

    UInt32 size = sizeof(m_sourceFormat);
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterCurrentInputStreamDescription, &size, &m_sourceFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter getting kAudioConverterCurrentInputStreamDescription failed with %d", static_cast<int>(error));
        return error;
    }

    size = sizeof(m_destinationFormat);
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterCurrentOutputStreamDescription, &size, &m_destinationFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter getting kAudioConverterCurrentOutputStreamDescription failed with %d", static_cast<int>(error));
        return error;
    }

    if (!isPCM()) {
        bool shouldSetDefaultOutputBitRate = true;
        if (m_options.outputBitRate) {
            if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioConverterEncodeBitRate, sizeof(*m_options.outputBitRate), &*m_options.outputBitRate))
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting kAudioConverterEncodeBitRate failed with %d", static_cast<int>(error));
            else
                shouldSetDefaultOutputBitRate = false;
        }
        if (shouldSetDefaultOutputBitRate) {
            auto outputBitRate = defaultOutputBitRate(m_destinationFormat);
            if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioConverterEncodeBitRate, sizeof(outputBitRate), &outputBitRate))
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting default kAudioConverterEncodeBitRate failed with %d", static_cast<int>(error));
            else
                m_defaultBitRate = outputBitRate;
        }
    }

    if (isPCM() && m_options.preSkip) {
        AudioConverterPrimeInfo primeInfo = { static_cast<UInt32>(*m_options.preSkip), 0 };
        if (auto error = PAL::AudioConverterSetProperty(converter, kAudioConverterPrimeInfo, sizeof(primeInfo), &primeInfo))
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting default kAudioConverterPrimeInfo failed with %d", static_cast<int>(error));
    }

    if (!isPCM()) {
        AudioConverterPrimeInfo primeInfo { 0, 0 };
        UInt32 size = sizeof(primeInfo);
        if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterPrimeInfo, &size, &primeInfo))
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter getting kAudioConverterPrimeInfo failed with %d", static_cast<int>(error));
        else
            m_preSkip = primeInfo.leadingFrames;
        m_remainingPrimeDuration = PAL::CMTimeMake(primeInfo.leadingFrames, m_destinationFormat.mSampleRate);

        if (m_options.bitrateMode) {
            UInt32 bitrateMode = *m_options.bitrateMode == BitrateMode::Variable ? kAudioCodecBitRateControlMode_Variable : kAudioCodecBitRateControlMode_Constant;
            if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioCodecPropertyBitRateControlMode, sizeof(bitrateMode), &bitrateMode))
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting kAudioCodecPropertyBitRateControlMode failed with %d", static_cast<int>(error));
        }
        if (m_options.complexity) {
            if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioCodecPropertyQualitySetting, sizeof(*m_options.complexity), &m_options.complexity.value()))
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting kAudioCodecPropertyQualitySetting failed with %d", static_cast<int>(error));
        }

        // Only operational with Opus encoder.
        if (m_options.packetlossperc) {
            if (auto error = PAL::AudioConverterSetProperty(m_converter, 'plsp', sizeof(*m_options.packetlossperc), &m_options.packetlossperc.value()))
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting packetlossperc failed with %d", static_cast<int>(error));
        }
        if (m_options.useinbandfec) {
            if (auto error = PAL::AudioConverterSetProperty(m_converter, 'pfec', sizeof(*m_options.useinbandfec), &m_options.useinbandfec.value()))
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting useinbandfec failed with %d", static_cast<int>(error));
        }
    }

    if (!m_destinationFormat.mBytesPerPacket) {
        // If the destination format is VBR, we need to get max size per packet from the converter.
        size = sizeof(m_maxOutputPacketSize);

        if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterPropertyMaximumOutputPacketSize, &size, &m_maxOutputPacketSize)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter getting kAudioConverterPropertyMaximumOutputPacketSize failed with %d", static_cast<int>(error));
            return error;
        }
    } else
        m_maxOutputPacketSize = m_destinationFormat.mBytesPerPacket;

    cleanupInCaseOfError.release();

    size_t sizeNeededForDestination = CAAudioStreamDescription(m_destinationFormat).numberOfChannelStreams() * m_maxOutputPacketSize;
    if (m_destinationBuffer.size() < sizeNeededForDestination)
        m_destinationBuffer.grow(sizeNeededForDestination);

    m_destinationPacketDescriptions.reserveInitialCapacity(m_destinationBuffer.capacity() / m_maxOutputPacketSize);
    return noErr;
}

void AudioSampleBufferConverter::attachPrimingTrimsIfNeeded(CMSampleBufferRef buffer)
{
    assertIsCurrent(queue().get());

    using namespace PAL;

    ASSERT(CMTIME_IS_VALID(m_remainingPrimeDuration));

    if (CMTIME_COMPARE_INLINE(kCMTimeZero, <, m_remainingPrimeDuration)) {
        CMTime sampleDuration = CMSampleBufferGetDuration(buffer);
        CMTime trimDuration = CMTimeMinimum(sampleDuration, m_remainingPrimeDuration);
        auto trimAtStartDict = adoptCF(CMTimeCopyAsDictionary(trimDuration, kCFAllocatorDefault));
        CMSetAttachment(buffer, kCMSampleBufferAttachmentKey_TrimDurationAtStart, trimAtStartDict.get(), kCMAttachmentMode_ShouldPropagate);
        m_remainingPrimeDuration = CMTimeSubtract(m_remainingPrimeDuration, trimDuration);
    }
}

RetainPtr<NSNumber> AudioSampleBufferConverter::gradualDecoderRefreshCount()
{
    assertIsCurrent(queue().get());

    if (isPCM())
        return nil;
    UInt32 delaySize = sizeof(uint32_t);
    uint32_t originalDelayMode = 0;
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioCodecPropertyDelayMode, &delaySize, &originalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter getting kAudioCodecPropertyDelayMode failed with %d", static_cast<int>(error));
        return nil;
    }

    uint32_t optimalDelayMode = kAudioCodecDelayMode_Optimal;
    if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioCodecPropertyDelayMode, delaySize, &optimalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting kAudioCodecPropertyDelayMode failed with %d", static_cast<int>(error));
        return nil;
    }

    UInt32 primeSize = sizeof(AudioCodecPrimeInfo);
    AudioCodecPrimeInfo primeInfo { 0, 0 };
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioCodecPropertyPrimeInfo, &primeSize, &primeInfo)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter getting kAudioCodecPropertyPrimeInfo failed with %d", static_cast<int>(error));
        return nil;
    }

    if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioCodecPropertyDelayMode, delaySize, &originalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter setting kAudioCodecPropertyDelayMode failed with %d", static_cast<int>(error));
        return nil;
    }
    return retainPtr([NSNumber numberWithInt:(primeInfo.leadingFrames / m_destinationFormat.mFramesPerPacket)]);
}

Expected<RetainPtr<CMSampleBufferRef>, OSStatus> AudioSampleBufferConverter::sampleBuffer(const WebAudioBufferList& fillBufferList, uint32_t numSamples)
{
    assertIsCurrent(queue().get());

    Vector<char> cookie;
    if (!m_destinationFormatDescription) {
        UInt32 cookieSize = 0;

        if (!PAL::AudioConverterGetPropertyInfo(m_converter, kAudioConverterCompressionMagicCookie, &cookieSize, nullptr) && !!cookieSize) {
            cookie.grow(cookieSize);

            if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterCompressionMagicCookie, &cookieSize, cookie.data())) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter getting kAudioConverterCompressionMagicCookie failed with %d", static_cast<int>(error));
                return makeUnexpected(error);
            }
        }

        CMFormatDescriptionRef destinationFormatDescription;
        if (auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &m_destinationFormat, 0, nullptr, cookieSize, cookie.data(), nullptr, &destinationFormatDescription)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMAudioFormatDescriptionCreate failed with %d", static_cast<int>(error));
            return makeUnexpected(error);
        }
        m_destinationFormatDescription = adoptCF(destinationFormatDescription);
        m_gdrCountNum = gradualDecoderRefreshCount();
    }

    CMSampleBufferRef rawSampleBuffer;
    if (auto error = PAL::CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, nullptr, false, nullptr, nullptr, m_destinationFormatDescription.get(), numSamples, m_currentNativePresentationTimeStamp, isPCM() ? nullptr : m_destinationPacketDescriptions.data(), &rawSampleBuffer)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMAudioSampleBufferCreateWithPacketDescriptions failed with %d", static_cast<int>(error));
        return makeUnexpected(error);
    }
    auto sampleBuffer = adoptCF(rawSampleBuffer);
    if (auto error = PAL::CMSampleBufferSetDataBufferFromAudioBufferList(sampleBuffer.get(), kCFAllocatorDefault, kCFAllocatorDefault, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, fillBufferList.list())) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMSampleBufferSetDataBufferFromAudioBufferList failed with %d", static_cast<int>(error));
        return makeUnexpected(error);
    }

    if ([m_gdrCountNum intValue])
        PAL::CMSetAttachment(sampleBuffer.get(), PAL::kCMSampleBufferAttachmentKey_GradualDecoderRefresh, (__bridge CFTypeRef)m_gdrCountNum.get(), kCMAttachmentMode_ShouldPropagate);

    return sampleBuffer;
}

OSStatus AudioSampleBufferConverter::audioConverterComplexInputDataProc(AudioConverterRef, UInt32 *numOutputPacketsPtr, AudioBufferList *bufferList, AudioStreamPacketDescription **packetDescriptionOut, void *audioSampleBufferCompressor)
{
    auto *converter = static_cast<AudioSampleBufferConverter*>(audioSampleBufferCompressor);
    return converter->provideSourceDataNumOutputPackets(numOutputPacketsPtr, bufferList, packetDescriptionOut);
}

OSStatus AudioSampleBufferConverter::provideSourceDataNumOutputPackets(UInt32* numOutputPacketsPtr, AudioBufferList* audioBufferList, AudioStreamPacketDescription** packetDescriptionOut)
{
    assertIsCurrent(queue().get());

    if (packetDescriptionOut)
        *packetDescriptionOut = NULL;

    if (PAL::CMBufferQueueIsEmpty(m_inputBufferQueue.get())) {
        *numOutputPacketsPtr = 0;
        return m_isDraining ? noErr : kNoMoreDataErr;
    }

    auto sampleBuffer = adoptCF((CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_inputBufferQueue.get()))));
    size_t listSize = 0;
    if (auto result = PAL::CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer.get(), &listSize, nullptr, 0, kCFAllocatorSystemDefault, kCFAllocatorSystemDefault, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, nullptr))
        return result;

    std::unique_ptr<AudioBufferList> list { static_cast<AudioBufferList*>(::operator new (listSize)) };
    CMBlockBufferRef buffer = nullptr;
    if (auto result = PAL::CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer.get(), nullptr, list.get(), listSize, kCFAllocatorSystemDefault, kCFAllocatorSystemDefault, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &buffer))
        return result;
    m_blockBuffer = adoptCF(buffer);

    if (audioBufferList->mNumberBuffers != list->mNumberBuffers)
        return kAudioConverterErr_BadPropertySizeError;

    if (!m_options.generateTimestamp)
        setTimeFromSample(sampleBuffer.get());

    for (size_t index = 0; index < list->mNumberBuffers; index++)
        audioBufferList->mBuffers[index] = list->mBuffers[index];

    m_packetDescriptions = getPacketDescriptions(sampleBuffer.get());
    if (packetDescriptionOut) {
        *numOutputPacketsPtr = m_packetDescriptions.size();
        *packetDescriptionOut = m_packetDescriptions.data();
    } else if (m_sourceFormat.mFormatID == kAudioFormatLinearPCM) {
        ASSERT(audioBufferList->mNumberBuffers && m_sourceFormat.mBytesPerPacket);
        *numOutputPacketsPtr = (audioBufferList->mBuffers[0].mDataByteSize / m_sourceFormat.mBytesPerPacket);
    } else
        *numOutputPacketsPtr = 1;

    return noErr;
}

bool AudioSampleBufferConverter::isPCM() const
{
    assertIsCurrent(queue().get());

    return m_destinationFormat.mFormatID == kAudioFormatLinearPCM;
}

void AudioSampleBufferConverter::setTimeFromSample(CMSampleBufferRef sample)
{
    assertIsCurrent(queue().get());

    m_currentNativePresentationTimeStamp = PAL::CMSampleBufferGetPresentationTimeStamp(sample);
    m_currentOutputPresentationTimeStamp = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sample);
}

void AudioSampleBufferConverter::processSampleBuffers()
{
    assertIsCurrent(queue().get());
    using namespace PAL; // For CMTIME_COMPARE_INLINE

    if (!m_converter) {
        if (CMBufferQueueIsEmpty(m_inputBufferQueue.get()))
            return;

        RetainPtr buffer = (CMSampleBufferRef)(const_cast<void*>(CMBufferQueueGetHead(m_inputBufferQueue.get())));
        ASSERT(buffer);

        if (m_options.generateTimestamp)
            setTimeFromSample(buffer.get());

        RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(buffer.get());
        if (auto error = initAudioConverterForSourceFormatDescription(formatDescription.get(), m_outputCodecType)) {
            m_lastError = error;
            return;
        }
    }

    constexpr int kMaxPCMFrames = 2048;
    while (true) {
        WebAudioBufferList fillBufferList(m_destinationFormat, isPCM() ? kMaxPCMFrames : 0);

        if (!isPCM()) {
            ASSERT(fillBufferList.bufferCount() == 1);
            auto* buffer = fillBufferList.buffer(0);
            buffer->mDataByteSize = m_destinationBuffer.capacity();
            buffer->mData = m_destinationBuffer.data();
        }
        size_t sizeRemaining = fillBufferList.buffer(0)->mDataByteSize;
        size_t bytesWritten = 0;

        UInt32 numOutputPackets = sizeRemaining / m_maxOutputPacketSize;
        bool hasNoMoreData = false;
        uint32_t numSamplesDecoded = 0;

        do {
            if (isPCM()) {
                for (auto& buffer : fillBufferList.buffers()) {
                    buffer.mDataByteSize = sizeRemaining;
                    buffer.mData = static_cast<uint8_t*>(buffer.mData) + bytesWritten;
                }
            }
            if (auto error = AudioConverterFillComplexBuffer(m_converter, audioConverterComplexInputDataProc, this, &numOutputPackets, fillBufferList.list(), isPCM() ? nullptr : m_destinationPacketDescriptions.data())) {
                if (error != kNoMoreDataErr) {
                    RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter AudioConverterFillComplexBuffer failed with %d", static_cast<int>(error));
                    m_lastError = error;
                    return;
                }
                hasNoMoreData = true;
            }
            bytesWritten += fillBufferList.buffer(0)->mDataByteSize;
            sizeRemaining -= fillBufferList.buffer(0)->mDataByteSize;
            numSamplesDecoded += numOutputPackets;
        } while (isPCM() && !hasNoMoreData && numOutputPackets && sizeRemaining);

        if (!numOutputPackets && !bytesWritten)
            break;

        if (isPCM()) {
            fillBufferList.reset();
            for (auto& buffer : fillBufferList.buffers())
                buffer.mDataByteSize = bytesWritten;
        }
        auto sampleOrError = sampleBuffer(fillBufferList, numSamplesDecoded);
        if (!sampleOrError) {
            m_lastError = sampleOrError.error();
            return;
        }
        RetainPtr buffer = WTFMove(*sampleOrError);

        // FIXME: "Test encoding Opus with additional parameters: Opus with frameDuration" will fail otherwise, it is more correct to set the priming trims at all time.
        if (!isPCM() && m_options.generateTimestamp)
            attachPrimingTrimsIfNeeded(buffer.get());

        if (auto error = CMSampleBufferSetOutputPresentationTimeStamp(buffer.get(), m_currentOutputPresentationTimeStamp))
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMSampleBufferSetOutputPresentationTimeStamp failed with %d", static_cast<int>(error)); // not a fatal error. Is this even possible?

        CMTime nativeDuration = CMSampleBufferGetDuration(buffer.get());
        m_currentNativePresentationTimeStamp = PAL::CMTimeAdd(m_currentNativePresentationTimeStamp, nativeDuration);

        CMTime outputDuration = CMSampleBufferGetOutputDuration(buffer.get());
        m_currentOutputPresentationTimeStamp = PAL::CMTimeAdd(m_currentOutputPresentationTimeStamp, outputDuration);

        if (auto error = CMBufferQueueEnqueue(m_outputBufferQueue.get(), buffer.get())) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMBufferQueueEnqueue failed with %d", static_cast<int>(error));
            m_lastError = error;
            return;
        }

        if (hasNoMoreData || !numOutputPackets)
            break;
    }
}

void AudioSampleBufferConverter::processSampleBuffer(CMSampleBufferRef buffer)
{
    auto error = PAL::CMBufferQueueEnqueue(m_inputBufferQueue.get(), buffer);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "AudioSampleBufferConverter CMBufferQueueEnqueue failed with %d", static_cast<int>(error));

    processSampleBuffers();
}

Ref<GenericPromise> AudioSampleBufferConverter::addSampleBuffer(CMSampleBufferRef buffer)
{
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    return invokeAsync(queue(), [weakThis = ThreadSafeWeakPtr { *this }, buffer = RetainPtr { buffer }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndReject();
        assertIsCurrent(protectedThis->queue().get());
        if (!protectedThis->m_isEncoding)
            return GenericPromise::createAndReject();
        protectedThis->processSampleBuffer(buffer.get());
        return protectedThis->m_lastError ? GenericPromise::createAndReject() : GenericPromise::createAndResolve();
    });
}

CMSampleBufferRef AudioSampleBufferConverter::getOutputSampleBuffer() const
{
    return (CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueGetHead(m_outputBufferQueue.get())));
}

RetainPtr<CMSampleBufferRef> AudioSampleBufferConverter::takeOutputSampleBuffer()
{
    return adoptCF((CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_outputBufferQueue.get()))));
}

unsigned AudioSampleBufferConverter::bitRate() const
{
    return m_options.outputBitRate.value_or(m_defaultBitRate.load());
}

bool AudioSampleBufferConverter::isEmpty() const
{
    return m_outputBufferQueue && PAL::CMBufferQueueIsEmpty(m_outputBufferQueue.get());
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MEDIA_RECORDER) && USE(AVFOUNDATION)
