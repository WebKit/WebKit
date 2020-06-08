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

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "Logging.h"
#include <AudioToolbox/AudioCodec.h>
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFormat.h>
#include <Foundation/Foundation.h>

#import <pal/cf/AudioToolboxSoftLink.h>

#define LOW_WATER_TIME_IN_SECONDS 0.1

namespace WebCore {

using namespace PAL;

std::unique_ptr<AudioSampleBufferCompressor> AudioSampleBufferCompressor::create(CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    auto compressor = std::unique_ptr<AudioSampleBufferCompressor>(new AudioSampleBufferCompressor());
    if (!compressor->initialize(callback, callbackObject))
        return nullptr;
    return compressor;
}

AudioSampleBufferCompressor::AudioSampleBufferCompressor()
    : m_serialDispatchQueue { dispatch_queue_create("com.apple.AudioSampleBufferCompressor", DISPATCH_QUEUE_SERIAL) }
    , m_lowWaterTime { CMTimeMakeWithSeconds(LOW_WATER_TIME_IN_SECONDS, 1000) }
    , m_outputCodecType { kAudioFormatMPEG4AAC }
{
}

AudioSampleBufferCompressor::~AudioSampleBufferCompressor()
{
    if (m_serialDispatchQueue)
        dispatch_release(m_serialDispatchQueue);
}

bool AudioSampleBufferCompressor::initialize(CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    CMBufferQueueRef inputBufferQueue;
    if (auto error = CMBufferQueueCreate(kCFAllocatorDefault, 0, CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &inputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBufferQueueCreate for m_inputBufferQueue failed with %d", error);
        return false;
    }
    m_inputBufferQueue = adoptCF(inputBufferQueue);

    CMBufferQueueRef outputBufferQueue;
    if (auto error = CMBufferQueueCreate(kCFAllocatorDefault, 0, CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &outputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBufferQueueCreate for m_outputBufferQueue failed with %d", error);
        return false;
    }
    m_outputBufferQueue = adoptCF(outputBufferQueue);
    CMBufferQueueInstallTrigger(m_outputBufferQueue.get(), callback, callbackObject, kCMBufferQueueTrigger_WhenDataBecomesReady, kCMTimeZero, NULL);

    m_isEncoding = true;
    return true;
}

void AudioSampleBufferCompressor::finish()
{
    dispatch_sync(m_serialDispatchQueue, ^{
        processSampleBuffersUntilLowWaterTime(kCMTimeInvalid);
        auto error = CMBufferQueueMarkEndOfData(m_outputBufferQueue.get());
        RELEASE_LOG_ERROR_IF(error, MediaStream, "AudioSampleBufferCompressor CMBufferQueueMarkEndOfData failed %d", error);
        m_isEncoding = false;
    });
}

bool AudioSampleBufferCompressor::initAudioConverterForSourceFormatDescription(CMFormatDescriptionRef formatDescription, AudioFormatID outputFormatID)
{
    const auto *audioFormatListItem = CMAudioFormatDescriptionGetRichestDecodableFormat(formatDescription);
    m_sourceFormat = audioFormatListItem->mASBD;

    memset(&m_destinationFormat, 0, sizeof(AudioStreamBasicDescription));
    m_destinationFormat.mFormatID = outputFormatID;
    m_destinationFormat.mSampleRate = m_sourceFormat.mSampleRate;
    m_destinationFormat.mChannelsPerFrame = m_sourceFormat.mChannelsPerFrame;

    UInt32 size = sizeof(m_destinationFormat);
    if (auto error = AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL, &size, &m_destinationFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor AudioFormatGetProperty failed with %d", error);
        return false;
    }

    AudioConverterRef converter;
    if (auto error = AudioConverterNew(&m_sourceFormat, &m_destinationFormat, &converter)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor AudioConverterNew failed with %d", error);
        return false;
    }
    m_converter = adoptCF(converter);

    size_t cookieSize = 0;
    const void *cookie = CMAudioFormatDescriptionGetMagicCookie(formatDescription, &cookieSize);
    if (cookieSize) {
        if (auto error = AudioConverterSetProperty(m_converter.get(), kAudioConverterDecompressionMagicCookie, (UInt32)cookieSize, cookie)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting kAudioConverterDecompressionMagicCookie failed with %d", error);
            return false;
        }
    }

    size = sizeof(m_sourceFormat);
    if (auto error = AudioConverterGetProperty(m_converter.get(), kAudioConverterCurrentInputStreamDescription, &size, &m_sourceFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterCurrentInputStreamDescription failed with %d", error);
        return false;
    }

    if (!m_sourceFormat.mBytesPerPacket) {
        RELEASE_LOG_ERROR(MediaStream, "mBytesPerPacket should not be zero");
        return false;
    }

    size = sizeof(m_destinationFormat);
    if (auto error = AudioConverterGetProperty(m_converter.get(), kAudioConverterCurrentOutputStreamDescription, &size, &m_destinationFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterCurrentOutputStreamDescription failed with %d", error);
        return false;
    }

    if (m_destinationFormat.mFormatID == kAudioFormatMPEG4AAC) {
        // FIXME: Set outputBitRate according MediaRecorderOptions.audioBitsPerSecond.
        UInt32 outputBitRate = 64000;
        if (m_destinationFormat.mSampleRate >= 44100)
            outputBitRate = 192000;
        else if (m_destinationFormat.mSampleRate < 22000)
            outputBitRate = 32000;

        size = sizeof(outputBitRate);
        if (auto error = AudioConverterSetProperty(m_converter.get(), kAudioConverterEncodeBitRate, size, &outputBitRate)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting kAudioConverterEncodeBitRate failed with %d", error);
            return false;
        }
    }

    if (!m_destinationFormat.mBytesPerPacket) {
        // If the destination format is VBR, we need to get max size per packet from the converter.
        size = sizeof(m_maxOutputPacketSize);

        if (auto error = AudioConverterGetProperty(m_converter.get(), kAudioConverterPropertyMaximumOutputPacketSize, &size, &m_maxOutputPacketSize)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterPropertyMaximumOutputPacketSize failed with %d", error);
            return false;
        }
    }

    auto destinationBufferSize = computeBufferSizeForAudioFormat(m_destinationFormat, m_maxOutputPacketSize, LOW_WATER_TIME_IN_SECONDS);
    if (m_destinationBuffer.size() < destinationBufferSize)
        m_destinationBuffer.resize(destinationBufferSize);
    if (!m_destinationFormat.mBytesPerPacket)
        m_destinationPacketDescriptions.resize(m_destinationBuffer.capacity() / m_maxOutputPacketSize);

    return true;
}

size_t AudioSampleBufferCompressor::computeBufferSizeForAudioFormat(AudioStreamBasicDescription format, UInt32 maxOutputPacketSize, Float32 duration)
{
    UInt32 numPackets = (format.mSampleRate * duration) / format.mFramesPerPacket;
    UInt32 outputPacketSize = format.mBytesPerPacket ? format.mBytesPerPacket : maxOutputPacketSize;
    UInt32 bufferSize = numPackets * outputPacketSize;

    return bufferSize;
}

void AudioSampleBufferCompressor::attachPrimingTrimsIfNeeded(CMSampleBufferRef buffer)
{
    if (CMTIME_IS_INVALID(m_remainingPrimeDuration)) {
        AudioConverterPrimeInfo primeInfo { 0, 0 };
        UInt32 size = sizeof(primeInfo);

        if (auto error = AudioConverterGetProperty(m_converter.get(), kAudioConverterPrimeInfo, &size, &primeInfo)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterPrimeInfo failed with %d", error);
            return;
        }

        m_remainingPrimeDuration = CMTimeMake(primeInfo.leadingFrames, m_destinationFormat.mSampleRate);
    }

    if (CMTIME_COMPARE_INLINE(kCMTimeZero, <, m_remainingPrimeDuration)) {
        CMTime sampleDuration = CMSampleBufferGetDuration(buffer);
        CMTime trimDuration = CMTimeMinimum(sampleDuration, m_remainingPrimeDuration);
        CFDictionaryRef trimAtStartDict = CMTimeCopyAsDictionary(trimDuration, kCFAllocatorDefault);
        CMSetAttachment(buffer, kCMSampleBufferAttachmentKey_TrimDurationAtStart, trimAtStartDict, kCMAttachmentMode_ShouldPropagate);
        CFRelease(trimAtStartDict);
        m_remainingPrimeDuration = CMTimeSubtract(m_remainingPrimeDuration, trimDuration);
    }
}

RetainPtr<NSNumber> AudioSampleBufferCompressor::gradualDecoderRefreshCount()
{
    UInt32 delaySize = sizeof(uint32_t);
    uint32_t originalDelayMode = 0;
    if (auto error = AudioConverterGetProperty(m_converter.get(), kAudioCodecPropertyDelayMode, &delaySize, &originalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioCodecPropertyDelayMode failed with %d", error);
        return nil;
    }

    uint32_t optimalDelayMode = kAudioCodecDelayMode_Optimal;
    if (auto error = AudioConverterSetProperty(m_converter.get(), kAudioCodecPropertyDelayMode, delaySize, &optimalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting kAudioCodecPropertyDelayMode failed with %d", error);
        return nil;
    }

    UInt32 primeSize = sizeof(AudioCodecPrimeInfo);
    AudioCodecPrimeInfo primeInfo { 0, 0 };
    if (auto error = AudioConverterGetProperty(m_converter.get(), kAudioCodecPropertyPrimeInfo, &primeSize, &primeInfo)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioCodecPropertyPrimeInfo failed with %d", error);
        return nil;
    }

    if (auto error = AudioConverterSetProperty(m_converter.get(), kAudioCodecPropertyDelayMode, delaySize, &originalDelayMode)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor setting kAudioCodecPropertyDelayMode failed with %d", error);
        return nil;
    }
    return adoptNS([NSNumber numberWithInt:(primeInfo.leadingFrames / m_destinationFormat.mFramesPerPacket)]);
}

CMSampleBufferRef AudioSampleBufferCompressor::sampleBufferWithNumPackets(UInt32 numPackets, AudioBufferList fillBufferList)
{
    Vector<char> cookie;
    if (!m_destinationFormatDescription) {
        UInt32 cookieSize = 0;

        auto error = AudioConverterGetPropertyInfo(m_converter.get(), kAudioConverterCompressionMagicCookie, &cookieSize, NULL);
        if ((error == noErr) && !!cookieSize) {
            cookie.resize(cookieSize);

            if (auto error = AudioConverterGetProperty(m_converter.get(), kAudioConverterCompressionMagicCookie, &cookieSize, cookie.data())) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor getting kAudioConverterCompressionMagicCookie failed with %d", error);
                return nil;
            }
        }

        CMFormatDescriptionRef destinationFormatDescription;
        if (auto error = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &m_destinationFormat, 0, NULL, cookieSize, cookie.data(), NULL, &destinationFormatDescription)) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMAudioFormatDescriptionCreate failed with %d", error);
            return nil;
        }
        m_destinationFormatDescription = adoptCF(destinationFormatDescription);
        m_gdrCountNum = gradualDecoderRefreshCount();
    }

    char *data = static_cast<char*>(fillBufferList.mBuffers[0].mData);
    size_t dataSize = fillBufferList.mBuffers[0].mDataByteSize;

    CMBlockBufferRef blockBuffer;
    if (auto error = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, dataSize, kCFAllocatorDefault, NULL, 0, dataSize, kCMBlockBufferAssureMemoryNowFlag, &blockBuffer)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferCreateWithMemoryBlock failed with %d", error);
        return nil;
    }
    auto buffer = adoptCF(blockBuffer);

    if (auto error = CMBlockBufferReplaceDataBytes(data, buffer.get(), 0, dataSize)) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferReplaceDataBytes failed with %d", error);
        return nil;
    }

    CMSampleBufferRef sampleBuffer;
    auto error = CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, buffer.get(), true, NULL, NULL, m_destinationFormatDescription.get(), numPackets, m_currentNativePresentationTimeStamp, m_destinationPacketDescriptions.data(), &sampleBuffer);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMAudioSampleBufferCreateWithPacketDescriptions failed with %d", error);
        return nil;
    }

    if ([m_gdrCountNum intValue])
        CMSetAttachment(sampleBuffer, kCMSampleBufferAttachmentKey_GradualDecoderRefresh, (__bridge CFTypeRef)m_gdrCountNum.get(), kCMAttachmentMode_ShouldPropagate);

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

    const UInt32 numPacketsToCopy = *numOutputPacketsPtr;
    size_t numBytesToCopy = (numPacketsToCopy * m_sourceFormat.mBytesPerPacket);

    if (audioBufferList->mNumberBuffers == 1) {
        size_t currentOffsetInSourceBuffer = 0;

        if (m_sourceBuffer.size() < numBytesToCopy)
            m_sourceBuffer.resize(numBytesToCopy);

        while (numBytesToCopy) {
            if (m_sampleBlockBufferSize <= m_currentOffsetInSampleBlockBuffer) {
                if (m_sampleBlockBuffer) {
                    m_sampleBlockBuffer = nullptr;
                    m_sampleBlockBufferSize = 0;
                }

                if (CMBufferQueueIsEmpty(m_inputBufferQueue.get()))
                    break;

                auto sampleBuffer = adoptCF((CMSampleBufferRef)(const_cast<void*>(CMBufferQueueDequeueAndRetain(m_inputBufferQueue.get()))));
                m_sampleBlockBuffer = adoptCF((CMBlockBufferRef)(const_cast<void*>(CFRetain(CMSampleBufferGetDataBuffer(sampleBuffer.get())))));
                if (!m_sampleBlockBuffer) {
                    RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMSampleBufferGetDataBuffer failed");
                    m_sampleBlockBufferSize = 0;
                    m_currentOffsetInSampleBlockBuffer = 0;
                    continue;
                }
                m_sampleBlockBufferSize = CMBlockBufferGetDataLength(m_sampleBlockBuffer.get());
                m_currentOffsetInSampleBlockBuffer = 0;
            }

            if (m_sampleBlockBuffer) {
                size_t numBytesToCopyFromSampleBbuf = std::min(numBytesToCopy, (m_sampleBlockBufferSize - m_currentOffsetInSampleBlockBuffer));
                if (auto error = CMBlockBufferCopyDataBytes(m_sampleBlockBuffer.get(), m_currentOffsetInSampleBlockBuffer, numBytesToCopyFromSampleBbuf, (m_sourceBuffer.data() + currentOffsetInSourceBuffer))) {
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
        m_sourceBuffer.resize(2 * numBytesToCopy);
    auto* firstChannel = m_sourceBuffer.data();
    auto* secondChannel = m_sourceBuffer.data() + numBytesToCopy;

    size_t currentOffsetInSourceBuffer = 0;
    while (numBytesToCopy) {
        if (m_sampleBlockBufferSize <= m_currentOffsetInSampleBlockBuffer) {
            if (m_sampleBlockBuffer) {
                m_sampleBlockBuffer = nullptr;
                m_sampleBlockBufferSize = 0;
            }

            if (CMBufferQueueIsEmpty(m_inputBufferQueue.get()))
                break;

            auto sampleBuffer = adoptCF((CMSampleBufferRef)(const_cast<void*>(CMBufferQueueDequeueAndRetain(m_inputBufferQueue.get()))));
            m_sampleBlockBuffer = adoptCF((CMBlockBufferRef)(const_cast<void*>(CFRetain(CMSampleBufferGetDataBuffer(sampleBuffer.get())))));
            if (!m_sampleBlockBuffer) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMSampleBufferGetDataBuffer failed");
                m_sampleBlockBufferSize = 0;
                m_currentOffsetInSampleBlockBuffer = 0;
                continue;
            }
            m_sampleBlockBufferSize = CMBlockBufferGetDataLength(m_sampleBlockBuffer.get()) / 2;
            m_currentOffsetInSampleBlockBuffer = 0;
        }

        if (m_sampleBlockBuffer) {
            size_t numBytesToCopyFromSampleBbuf = std::min(numBytesToCopy, (m_sampleBlockBufferSize - m_currentOffsetInSampleBlockBuffer));
            if (auto error = CMBlockBufferCopyDataBytes(m_sampleBlockBuffer.get(), m_currentOffsetInSampleBlockBuffer, numBytesToCopyFromSampleBbuf, (firstChannel + currentOffsetInSourceBuffer))) {
                RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBlockBufferCopyDataBytes first channel failed with %d", error);
                return error;
            }
            if (auto error = CMBlockBufferCopyDataBytes(m_sampleBlockBuffer.get(), m_currentOffsetInSampleBlockBuffer + m_sampleBlockBufferSize, numBytesToCopyFromSampleBbuf, (secondChannel + currentOffsetInSourceBuffer))) {
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

void AudioSampleBufferCompressor::processSampleBuffersUntilLowWaterTime(CMTime lowWaterTime)
{
    if (!m_converter) {
        if (CMBufferQueueIsEmpty(m_inputBufferQueue.get()))
            return;

        auto buffer = (CMSampleBufferRef)(const_cast<void*>(CMBufferQueueGetHead(m_inputBufferQueue.get())));
        ASSERT(buffer);

        m_currentNativePresentationTimeStamp = CMSampleBufferGetPresentationTimeStamp(buffer);
        m_currentOutputPresentationTimeStamp = CMSampleBufferGetOutputPresentationTimeStamp(buffer);

        auto formatDescription = CMSampleBufferGetFormatDescription(buffer);
        if (!initAudioConverterForSourceFormatDescription(formatDescription, m_outputCodecType))
            return;
    }

    while (CMTIME_IS_INVALID(lowWaterTime) || CMTIME_COMPARE_INLINE(lowWaterTime, <, CMBufferQueueGetDuration(m_inputBufferQueue.get()))) {
        AudioBufferList fillBufferList;

        fillBufferList.mNumberBuffers = 1;
        fillBufferList.mBuffers[0].mNumberChannels = m_destinationFormat.mChannelsPerFrame;
        fillBufferList.mBuffers[0].mDataByteSize = (UInt32)m_destinationBuffer.capacity();
        fillBufferList.mBuffers[0].mData = m_destinationBuffer.data();

        UInt32 outputPacketSize = m_destinationFormat.mBytesPerPacket ? m_destinationFormat.mBytesPerPacket : m_maxOutputPacketSize;
        UInt32 numOutputPackets = (UInt32)m_destinationBuffer.capacity() / outputPacketSize;

        auto error = AudioConverterFillComplexBuffer(m_converter.get(), audioConverterComplexInputDataProc, this, &numOutputPackets, &fillBufferList, m_destinationPacketDescriptions.data());
        if (error) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor AudioConverterFillComplexBuffer failed with %d", error);
            return;
        }

        if (!numOutputPackets)
            break;

        auto buffer = sampleBufferWithNumPackets(numOutputPackets, fillBufferList);

        attachPrimingTrimsIfNeeded(buffer);

        error = CMSampleBufferSetOutputPresentationTimeStamp(buffer, m_currentOutputPresentationTimeStamp);
        if (error) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMSampleBufferSetOutputPresentationTimeStamp failed with %d", error);
            return;
        }

        CMTime nativeDuration = CMSampleBufferGetDuration(buffer);
        m_currentNativePresentationTimeStamp = CMTimeAdd(m_currentNativePresentationTimeStamp, nativeDuration);

        CMTime outputDuration = CMSampleBufferGetOutputDuration(buffer);
        m_currentOutputPresentationTimeStamp = CMTimeAdd(m_currentOutputPresentationTimeStamp, outputDuration);

        error = CMBufferQueueEnqueue(m_outputBufferQueue.get(), buffer);
        if (error) {
            RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferCompressor CMBufferQueueEnqueue failed with %d", error);
            return;
        }
    }
}

void AudioSampleBufferCompressor::processSampleBuffer(CMSampleBufferRef buffer)
{
    auto error = CMBufferQueueEnqueue(m_inputBufferQueue.get(), buffer);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "AudioSampleBufferCompressor CMBufferQueueEnqueue failed with %d", error);

    processSampleBuffersUntilLowWaterTime(m_lowWaterTime);
}

void AudioSampleBufferCompressor::addSampleBuffer(CMSampleBufferRef buffer)
{
    dispatch_sync(m_serialDispatchQueue, ^{
        if (m_isEncoding)
            processSampleBuffer(buffer);
    });
}

CMSampleBufferRef AudioSampleBufferCompressor::getOutputSampleBuffer()
{
    return (CMSampleBufferRef)(const_cast<void*>(CMBufferQueueGetHead(m_outputBufferQueue.get())));
}

RetainPtr<CMSampleBufferRef> AudioSampleBufferCompressor::takeOutputSampleBuffer()
{
    return adoptCF((CMSampleBufferRef)(const_cast<void*>(CMBufferQueueDequeueAndRetain(m_outputBufferQueue.get()))));
}

}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
