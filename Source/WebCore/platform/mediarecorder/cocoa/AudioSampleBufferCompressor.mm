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

#include "config.h"
#include "AudioSampleBufferCompressor.h"

#if ENABLE(MEDIA_RECORDER) && USE(AVFOUNDATION)

#include "Logging.h"
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

namespace WebCore {

RefPtr<AudioSampleBufferCompressor> AudioSampleBufferCompressor::create(CMBufferQueueTriggerCallback callback, void* callbackObject, AudioFormatID format)
{
    Ref compressor = adoptRef(*new AudioSampleBufferCompressor(format));
    if (!compressor->initialize(callback, callbackObject))
        return nullptr;
    return compressor;
}

AudioSampleBufferCompressor::AudioSampleBufferCompressor(AudioFormatID format)
    : m_serialDispatchQueue(WorkQueue::create("com.apple.AudioSampleBufferCompressor"_s))
    , m_currentNativePresentationTimeStamp(PAL::kCMTimeInvalid)
    , m_currentOutputPresentationTimeStamp(PAL::kCMTimeInvalid)
    , m_remainingPrimeDuration(PAL::kCMTimeInvalid)
    , m_outputCodecType(format)
{
}

AudioSampleBufferCompressor::~AudioSampleBufferCompressor()
{
    if (m_outputBufferQueue)
        PAL::CMBufferQueueRemoveTrigger(m_outputBufferQueue.get(), m_triggerToken);

    if (m_converter) {
        PAL::AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }
}

bool AudioSampleBufferCompressor::initialize(CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    CMBufferQueueRef inputBufferQueue;
    if (auto error = PAL::CMBufferQueueCreate(kCFAllocatorDefault, 0, PAL::CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &inputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBufferQueueCreate for m_inputBufferQueue failed with %d", error);
        return false;
    }
    m_inputBufferQueue = adoptCF(inputBufferQueue);

    CMBufferQueueRef outputBufferQueue;
    if (auto error = PAL::CMBufferQueueCreate(kCFAllocatorDefault, 0, PAL::CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &outputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBufferQueueCreate for m_outputBufferQueue failed with %d", error);
        return false;
    }
    m_outputBufferQueue = adoptCF(outputBufferQueue);
    PAL::CMBufferQueueInstallTrigger(m_outputBufferQueue.get(), callback, callbackObject, kCMBufferQueueTrigger_WhenDataBecomesReady, PAL::kCMTimeZero, &m_triggerToken);

    m_isEncoding = true;
    return true;
}

Ref<GenericPromise> AudioSampleBufferCompressor::drain()
{
    return invokeAsync(m_serialDispatchQueue, [weakThis = ThreadSafeWeakPtr { *this }, this] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndReject();

        m_isDraining = true;
        processSampleBuffers();
        m_isDraining = false;

        if (!m_converter)
            return GenericPromise::createAndReject();

        if (auto error = PAL::AudioConverterReset(m_converter)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor AudioConverterReset failed %d", error);
            return GenericPromise::createAndReject();
        }
        return GenericPromise::createAndResolve();
    });
}

Ref<GenericPromise> AudioSampleBufferCompressor::flushInternal(bool isFinished)
{
    return invokeAsync(m_serialDispatchQueue, [weakThis = ThreadSafeWeakPtr { *this }, this, isFinished] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndReject();

        m_isDraining = isFinished;
        processSampleBuffers();
        m_isDraining = false;

        if (!m_converter)
            return GenericPromise::createAndReject();

        if (isFinished) {
            m_isEncoding = false;

            if (auto error = PAL::CMBufferQueueMarkEndOfData(m_outputBufferQueue.get())) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBufferQueueMarkEndOfData failed %d", error);
                return GenericPromise::createAndReject();
            }
        }
        return GenericPromise::createAndResolve();
    });
}

void AudioSampleBufferCompressor::setBitsPerSecond(unsigned bitRate)
{
    m_outputBitRate = bitRate;
}

UInt32 AudioSampleBufferCompressor::defaultOutputBitRate(const AudioStreamBasicDescription& destinationFormat) const
{
    if (destinationFormat.mSampleRate >= 44100)
        return 192000;
    if (destinationFormat.mSampleRate < 22000)
        return 32000;
    return 64000;
}

bool AudioSampleBufferCompressor::initAudioConverterForSourceFormatDescription(CMFormatDescriptionRef formatDescription, AudioFormatID outputFormatID)
{
    const auto *audioFormatListItem = PAL::CMAudioFormatDescriptionGetRichestDecodableFormat(formatDescription);
    m_sourceFormat = audioFormatListItem->mASBD;

    memset(&m_destinationFormat, 0, sizeof(AudioStreamBasicDescription));
    m_destinationFormat.mFormatID = outputFormatID;
    m_destinationFormat.mSampleRate = outputFormatID == kAudioFormatOpus ? 48000 : m_sourceFormat.mSampleRate;
    m_destinationFormat.mChannelsPerFrame = m_sourceFormat.mChannelsPerFrame;

    UInt32 size = sizeof(m_destinationFormat);
    if (auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL, &size, &m_destinationFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor AudioFormatGetProperty failed with %d", error);
        return false;
    }

    AudioConverterRef converter;
    auto error = PAL::AudioConverterNew(&m_sourceFormat, &m_destinationFormat, &converter);
    if (error == 'fmt?') {
        m_destinationFormat.mSampleRate = 44100;
        error = PAL::AudioConverterNew(&m_sourceFormat, &m_destinationFormat, &converter);
    }
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor AudioConverterNew failed with %d", error);
        return false;
    }
    m_converter = converter;

    auto cleanupInCaseOfError = makeScopeExit([&] {
        PAL::AudioConverterDispose(m_converter);
        m_converter = nullptr;
    });

    size_t cookieSize = 0;
    const void *cookie = PAL::CMAudioFormatDescriptionGetMagicCookie(formatDescription, &cookieSize);
    if (cookieSize) {
        if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioConverterDecompressionMagicCookie, (UInt32)cookieSize, cookie)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting kAudioConverterDecompressionMagicCookie failed with %d", error);
            return false;
        }
    }

    size = sizeof(m_sourceFormat);
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterCurrentInputStreamDescription, &size, &m_sourceFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterCurrentInputStreamDescription failed with %d", error);
        return false;
    }

    if (!m_sourceFormat.mBytesPerPacket) {
        RELEASE_LOG_ERROR(MediaStream, "mBytesPerPacket should not be zero");
        return false;
    }

    size = sizeof(m_destinationFormat);
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterCurrentOutputStreamDescription, &size, &m_destinationFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterCurrentOutputStreamDescription failed with %d", error);
        return false;
    }

    bool shouldSetDefaultOutputBitRate = true;
    if (m_outputBitRate) {
        auto error = PAL::AudioConverterSetProperty(m_converter, kAudioConverterEncodeBitRate, sizeof(*m_outputBitRate), &m_outputBitRate.value());
        RELEASE_LOG_ERROR_IF(error, MediaStream, "AudioSampleBufferCompressor setting kAudioConverterEncodeBitRate failed with %d", error);
        shouldSetDefaultOutputBitRate = !!error;
    }
    if (shouldSetDefaultOutputBitRate) {
        auto outputBitRate = defaultOutputBitRate(m_destinationFormat);
        size = sizeof(outputBitRate);
        if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioConverterEncodeBitRate, size, &outputBitRate))
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting default kAudioConverterEncodeBitRate failed with %d", error);
    }

    if (!m_destinationFormat.mBytesPerPacket) {
        // If the destination format is VBR, we need to get max size per packet from the converter.
        size = sizeof(m_maxOutputPacketSize);

        if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterPropertyMaximumOutputPacketSize, &size, &m_maxOutputPacketSize)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterPropertyMaximumOutputPacketSize failed with %d", error);
            return false;
        }
    } else
        m_maxOutputPacketSize = m_destinationFormat.mBytesPerPacket;

    cleanupInCaseOfError.release();

    if (m_destinationBuffer.size() < m_maxOutputPacketSize)
        m_destinationBuffer.grow(m_maxOutputPacketSize);

    return true;
}

void AudioSampleBufferCompressor::attachPrimingTrimsIfNeeded(CMSampleBufferRef buffer)
{
    using namespace PAL;

    if (CMTIME_IS_INVALID(m_remainingPrimeDuration)) {
        AudioConverterPrimeInfo primeInfo { 0, 0 };
        UInt32 size = sizeof(primeInfo);

        if (auto error = AudioConverterGetProperty(m_converter, kAudioConverterPrimeInfo, &size, &primeInfo)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterPrimeInfo failed with %d", error);
            return;
        }

        m_remainingPrimeDuration = CMTimeMake(primeInfo.leadingFrames, m_destinationFormat.mSampleRate);
    }

    if (CMTIME_COMPARE_INLINE(kCMTimeZero, <, m_remainingPrimeDuration)) {
        CMTime sampleDuration = CMSampleBufferGetDuration(buffer);
        CMTime trimDuration = CMTimeMinimum(sampleDuration, m_remainingPrimeDuration);
        auto trimAtStartDict = adoptCF(CMTimeCopyAsDictionary(trimDuration, kCFAllocatorDefault));
        CMSetAttachment(buffer, kCMSampleBufferAttachmentKey_TrimDurationAtStart, trimAtStartDict.get(), kCMAttachmentMode_ShouldPropagate);
        m_remainingPrimeDuration = CMTimeSubtract(m_remainingPrimeDuration, trimDuration);
    }
}

RetainPtr<NSNumber> AudioSampleBufferCompressor::gradualDecoderRefreshCount()
{
    UInt32 delaySize = sizeof(uint32_t);
    uint32_t originalDelayMode = 0;
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioCodecPropertyDelayMode, &delaySize, &originalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioCodecPropertyDelayMode failed with %d", error);
        return nil;
    }

    uint32_t optimalDelayMode = kAudioCodecDelayMode_Optimal;
    if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioCodecPropertyDelayMode, delaySize, &optimalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting kAudioCodecPropertyDelayMode failed with %d", error);
        return nil;
    }

    UInt32 primeSize = sizeof(AudioCodecPrimeInfo);
    AudioCodecPrimeInfo primeInfo { 0, 0 };
    if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioCodecPropertyPrimeInfo, &primeSize, &primeInfo)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioCodecPropertyPrimeInfo failed with %d", error);
        return nil;
    }

    if (auto error = PAL::AudioConverterSetProperty(m_converter, kAudioCodecPropertyDelayMode, delaySize, &originalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting kAudioCodecPropertyDelayMode failed with %d", error);
        return nil;
    }
    return retainPtr([NSNumber numberWithInt:(primeInfo.leadingFrames / m_destinationFormat.mFramesPerPacket)]);
}

RetainPtr<CMSampleBufferRef> AudioSampleBufferCompressor::sampleBuffer(AudioBufferList fillBufferList)
{
    Vector<char> cookie;
    if (!m_destinationFormatDescription) {
        UInt32 cookieSize = 0;

        auto error = PAL::AudioConverterGetPropertyInfo(m_converter, kAudioConverterCompressionMagicCookie, &cookieSize, NULL);
        if ((error == noErr) && !!cookieSize) {
            cookie.grow(cookieSize);

            if (auto error = PAL::AudioConverterGetProperty(m_converter, kAudioConverterCompressionMagicCookie, &cookieSize, cookie.data())) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterCompressionMagicCookie failed with %d", error);
                return nil;
            }
        }

        CMFormatDescriptionRef destinationFormatDescription;
        if (auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &m_destinationFormat, 0, NULL, cookieSize, cookie.data(), NULL, &destinationFormatDescription)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMAudioFormatDescriptionCreate failed with %d", error);
            return nil;
        }
        m_destinationFormatDescription = adoptCF(destinationFormatDescription);
        m_gdrCountNum = gradualDecoderRefreshCount();
    }

    char *data = static_cast<char*>(fillBufferList.mBuffers[0].mData);
    size_t dataSize = fillBufferList.mBuffers[0].mDataByteSize;

    CMBlockBufferRef blockBuffer;
    if (auto error = PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, dataSize, kCFAllocatorDefault, NULL, 0, dataSize, kCMBlockBufferAssureMemoryNowFlag, &blockBuffer)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferCreateWithMemoryBlock failed with %d", error);
        return nil;
    }
    auto buffer = adoptCF(blockBuffer);

    if (auto error = PAL::CMBlockBufferReplaceDataBytes(data, buffer.get(), 0, dataSize)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferReplaceDataBytes failed with %d", error);
        return nil;
    }

    CMSampleBufferRef rawSampleBuffer;
    auto error = PAL::CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, buffer.get(), true, NULL, NULL, m_destinationFormatDescription.get(), 1, m_currentNativePresentationTimeStamp, &m_destinationPacketDescriptions, &rawSampleBuffer);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMAudioSampleBufferCreateWithPacketDescriptions failed with %d", error);
        return nil;
    }

    auto sampleBuffer = adoptCF(rawSampleBuffer);

    if ([m_gdrCountNum intValue])
        PAL::CMSetAttachment(sampleBuffer.get(), PAL::kCMSampleBufferAttachmentKey_GradualDecoderRefresh, (__bridge CFTypeRef)m_gdrCountNum.get(), kCMAttachmentMode_ShouldPropagate);

    return sampleBuffer;
}

OSStatus AudioSampleBufferCompressor::audioConverterComplexInputDataProc(AudioConverterRef, UInt32 *numOutputPacketsPtr, AudioBufferList *bufferList, AudioStreamPacketDescription **packetDescriptionOut, void *audioSampleBufferCompressor)
{
    auto *compressor = static_cast<AudioSampleBufferCompressor*>(audioSampleBufferCompressor);
    return compressor->provideSourceDataNumOutputPackets(numOutputPacketsPtr, bufferList, packetDescriptionOut);
}

OSStatus AudioSampleBufferCompressor::provideSourceDataNumOutputPackets(UInt32* numOutputPacketsPtr, AudioBufferList* audioBufferList, AudioStreamPacketDescription** packetDescriptionOut)
{
    if (packetDescriptionOut)
        *packetDescriptionOut = NULL;

    if (PAL::CMBufferQueueIsEmpty(m_inputBufferQueue.get()) && !m_isDraining && m_sampleBlockBufferSize <= m_currentOffsetInSampleBlockBuffer) {
        m_sampleBlockBuffer = nullptr;
        m_sampleBlockBufferSize = 0;
        m_currentOffsetInSampleBlockBuffer = 0;
        *numOutputPacketsPtr = 0;
        return kNoMoreDataErr;
    }

    size_t numBytesToCopy = m_sourceFormat.mBytesPerPacket;

    if (audioBufferList->mNumberBuffers == 1) {
        size_t currentOffsetInSourceBuffer = 0;

        if (m_sourceBuffer.size() < numBytesToCopy)
            m_sourceBuffer.grow(numBytesToCopy);

        while (numBytesToCopy) {
            if (m_sampleBlockBufferSize <= m_currentOffsetInSampleBlockBuffer) {
                if (m_sampleBlockBuffer) {
                    m_sampleBlockBuffer = nullptr;
                    m_sampleBlockBufferSize = 0;
                }

                if (PAL::CMBufferQueueIsEmpty(m_inputBufferQueue.get()))
                    break;

                auto sampleBuffer = adoptCF((CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_inputBufferQueue.get()))));
                m_sampleBlockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer.get());
                if (!m_sampleBlockBuffer) {
                    RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMSampleBufferGetDataBuffer failed");
                    m_sampleBlockBufferSize = 0;
                    m_currentOffsetInSampleBlockBuffer = 0;
                    continue;
                }
                m_sampleBlockBufferSize = PAL::CMBlockBufferGetDataLength(m_sampleBlockBuffer.get());
                m_currentOffsetInSampleBlockBuffer = 0;
            }

            if (m_sampleBlockBuffer) {
                size_t numBytesToCopyFromSampleBbuf = std::min(numBytesToCopy, (m_sampleBlockBufferSize - m_currentOffsetInSampleBlockBuffer));
                if (auto error = PAL::CMBlockBufferCopyDataBytes(m_sampleBlockBuffer.get(), m_currentOffsetInSampleBlockBuffer, numBytesToCopyFromSampleBbuf, (m_sourceBuffer.data() + currentOffsetInSourceBuffer))) {
                    RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferCopyDataBytes failed with %d", error);
                    return error;
                }
                numBytesToCopy -= numBytesToCopyFromSampleBbuf;
                currentOffsetInSourceBuffer += numBytesToCopyFromSampleBbuf;
                m_currentOffsetInSampleBlockBuffer += numBytesToCopyFromSampleBbuf;
            }
        }
        audioBufferList->mBuffers[0].mData = m_sourceBuffer.data();
        audioBufferList->mBuffers[0].mDataByteSize = currentOffsetInSourceBuffer;
        audioBufferList->mBuffers[0].mNumberChannels = m_sourceFormat.mChannelsPerFrame;

        *numOutputPacketsPtr = (audioBufferList->mBuffers[0].mDataByteSize / m_sourceFormat.mBytesPerPacket);
        return noErr;
    }

    ASSERT(audioBufferList->mNumberBuffers == 2);

    // FIXME: Support interleaved data by uninterleaving m_sourceBuffer if needed.
    ASSERT(m_sourceFormat.mFormatFlags & kAudioFormatFlagIsNonInterleaved);

    if (m_sourceBuffer.size() < 2 * numBytesToCopy)
        m_sourceBuffer.grow(2 * numBytesToCopy);
    auto* firstChannel = m_sourceBuffer.data();
    auto* secondChannel = m_sourceBuffer.data() + numBytesToCopy;

    size_t currentOffsetInSourceBuffer = 0;
    while (numBytesToCopy) {
        if (m_sampleBlockBufferSize <= m_currentOffsetInSampleBlockBuffer) {
            if (m_sampleBlockBuffer) {
                m_sampleBlockBuffer = nullptr;
                m_sampleBlockBufferSize = 0;
            }

            if (PAL::CMBufferQueueIsEmpty(m_inputBufferQueue.get()))
                break;

            auto sampleBuffer = adoptCF((CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_inputBufferQueue.get()))));
            m_sampleBlockBuffer = adoptCF((CMBlockBufferRef)(const_cast<void*>(CFRetain(PAL::CMSampleBufferGetDataBuffer(sampleBuffer.get())))));
            if (!m_sampleBlockBuffer) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMSampleBufferGetDataBuffer failed");
                m_sampleBlockBufferSize = 0;
                m_currentOffsetInSampleBlockBuffer = 0;
                continue;
            }
            m_sampleBlockBufferSize = PAL::CMBlockBufferGetDataLength(m_sampleBlockBuffer.get()) / 2;
            m_currentOffsetInSampleBlockBuffer = 0;
        }

        if (m_sampleBlockBuffer) {
            size_t numBytesToCopyFromSampleBbuf = std::min(numBytesToCopy, (m_sampleBlockBufferSize - m_currentOffsetInSampleBlockBuffer));
            if (auto error = PAL::CMBlockBufferCopyDataBytes(m_sampleBlockBuffer.get(), m_currentOffsetInSampleBlockBuffer, numBytesToCopyFromSampleBbuf, (firstChannel + currentOffsetInSourceBuffer))) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferCopyDataBytes first channel failed with %d", error);
                return error;
            }
            if (auto error = PAL::CMBlockBufferCopyDataBytes(m_sampleBlockBuffer.get(), m_currentOffsetInSampleBlockBuffer + m_sampleBlockBufferSize, numBytesToCopyFromSampleBbuf, (secondChannel + currentOffsetInSourceBuffer))) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferCopyDataBytes second channel failed with %d", error);
                return error;
            }
            numBytesToCopy -= numBytesToCopyFromSampleBbuf;
            currentOffsetInSourceBuffer += numBytesToCopyFromSampleBbuf;
            m_currentOffsetInSampleBlockBuffer += numBytesToCopyFromSampleBbuf;
        }
    }

    audioBufferList->mBuffers[0].mData = firstChannel;
    audioBufferList->mBuffers[0].mDataByteSize = currentOffsetInSourceBuffer;
    audioBufferList->mBuffers[0].mNumberChannels = 1;

    audioBufferList->mBuffers[1].mData = secondChannel;
    audioBufferList->mBuffers[1].mDataByteSize = currentOffsetInSourceBuffer;
    audioBufferList->mBuffers[1].mNumberChannels = 1;

    *numOutputPacketsPtr = (audioBufferList->mBuffers[0].mDataByteSize / m_sourceFormat.mBytesPerPacket);
    return noErr;
}

void AudioSampleBufferCompressor::processSampleBuffers()
{
    using namespace PAL; // For CMTIME_COMPARE_INLINE

    if (!m_converter) {
        if (CMBufferQueueIsEmpty(m_inputBufferQueue.get()))
            return;

        RetainPtr buffer = (CMSampleBufferRef)(const_cast<void*>(CMBufferQueueGetHead(m_inputBufferQueue.get())));
        ASSERT(buffer);

        m_currentNativePresentationTimeStamp = CMSampleBufferGetPresentationTimeStamp(buffer.get());
        m_currentOutputPresentationTimeStamp = CMSampleBufferGetOutputPresentationTimeStamp(buffer.get());

        RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(buffer.get());
        if (!initAudioConverterForSourceFormatDescription(formatDescription.get(), m_outputCodecType)) {
            // FIXME: Maybe we should error the media recorder if we are not able to get a correct converter.
            return;
        }
    }

    while (true) {
        AudioBufferList fillBufferList;

        fillBufferList.mNumberBuffers = 1;
        fillBufferList.mBuffers[0].mNumberChannels = m_destinationFormat.mChannelsPerFrame;
        fillBufferList.mBuffers[0].mDataByteSize = (UInt32)m_destinationBuffer.capacity();
        fillBufferList.mBuffers[0].mData = m_destinationBuffer.data();

        UInt32 numOutputPackets = 1;

        auto error = AudioConverterFillComplexBuffer(m_converter, audioConverterComplexInputDataProc, this, &numOutputPackets, &fillBufferList, &m_destinationPacketDescriptions);

        bool hasNoMoreData = error == kNoMoreDataErr;

        if (error && !hasNoMoreData) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor AudioConverterFillComplexBuffer failed with %d", error);
            return;
        }

        if (!numOutputPackets)
            break;

        RetainPtr buffer = sampleBuffer(fillBufferList);

        attachPrimingTrimsIfNeeded(buffer.get());

        error = CMSampleBufferSetOutputPresentationTimeStamp(buffer.get(), m_currentOutputPresentationTimeStamp);
        if (error) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMSampleBufferSetOutputPresentationTimeStamp failed with %d", error);
            return;
        }

        CMTime nativeDuration = CMSampleBufferGetDuration(buffer.get());
        m_currentNativePresentationTimeStamp = PAL::CMTimeAdd(m_currentNativePresentationTimeStamp, nativeDuration);

        CMTime outputDuration = CMSampleBufferGetOutputDuration(buffer.get());
        m_currentOutputPresentationTimeStamp = PAL::CMTimeAdd(m_currentOutputPresentationTimeStamp, outputDuration);

        error = CMBufferQueueEnqueue(m_outputBufferQueue.get(), buffer.get());
        if (error) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBufferQueueEnqueue failed with %d", error);
            return;
        }

        if (hasNoMoreData)
            break;
    }
}

void AudioSampleBufferCompressor::processSampleBuffer(CMSampleBufferRef buffer)
{
    auto error = PAL::CMBufferQueueEnqueue(m_inputBufferQueue.get(), buffer);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "AudioSampleBufferCompressor CMBufferQueueEnqueue failed with %d", error);

    processSampleBuffers();
}

void AudioSampleBufferCompressor::addSampleBuffer(CMSampleBufferRef buffer)
{
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    m_serialDispatchQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }, buffer = RetainPtr { buffer }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_isEncoding)
            return;
        protectedThis->processSampleBuffer(buffer.get());
    });
}

CMSampleBufferRef AudioSampleBufferCompressor::getOutputSampleBuffer() const
{
    return (CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueGetHead(m_outputBufferQueue.get())));
}

RetainPtr<CMSampleBufferRef> AudioSampleBufferCompressor::takeOutputSampleBuffer()
{
    return adoptCF((CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_outputBufferQueue.get()))));
}

unsigned AudioSampleBufferCompressor::bitRate() const
{
    return m_outputBitRate.value_or(0);
}

bool AudioSampleBufferCompressor::isEmpty() const
{
    return m_outputBufferQueue && PAL::CMBufferQueueIsEmpty(m_outputBufferQueue.get());
}

}

#endif // ENABLE(MEDIA_RECORDER) && USE(AVFOUNDATION)
