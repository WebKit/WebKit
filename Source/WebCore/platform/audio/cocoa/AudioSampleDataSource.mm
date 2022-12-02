/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AudioSampleDataSource.h"

#import "AudioSampleBufferList.h"
#import "Logging.h"
#import "PlatformAudioData.h"
#import <AudioToolbox/AudioConverter.h>
#import <mach/mach.h>
#import <mach/mach_time.h>
#import <mutex>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <syslog.h>
#import <wtf/RunLoop.h>
#import <wtf/StringPrintStream.h>

#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {
using namespace JSC;

Ref<AudioSampleDataSource> AudioSampleDataSource::create(size_t maximumSampleCount, LoggerHelper& loggerHelper, size_t waitToStartForPushCount)
{
    return adoptRef(*new AudioSampleDataSource(maximumSampleCount, loggerHelper, waitToStartForPushCount));
}

AudioSampleDataSource::AudioSampleDataSource(size_t maximumSampleCount, LoggerHelper& loggerHelper, size_t waitToStartForPushCount)
    : m_waitToStartForPushCount(waitToStartForPushCount)
    , m_maximumSampleCount(maximumSampleCount)
#if !RELEASE_LOG_DISABLED
    , m_logger(loggerHelper.logger())
    , m_logIdentifier(loggerHelper.logIdentifier())
#endif
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(loggerHelper);
#endif
}

AudioSampleDataSource::~AudioSampleDataSource()
{
}

OSStatus AudioSampleDataSource::setupConverter()
{
    ASSERT(m_inputDescription && m_outputDescription);
    return m_converter.setFormats(*m_inputDescription, *m_outputDescription);
}

OSStatus AudioSampleDataSource::setInputFormat(const CAAudioStreamDescription& format)
{
    ASSERT(format.sampleRate() >= 0);

    m_inputDescription = CAAudioStreamDescription { format };
    if (m_outputDescription)
        return setupConverter();

    return 0;
}

OSStatus AudioSampleDataSource::setOutputFormat(const CAAudioStreamDescription& format)
{
    ASSERT(m_inputDescription);
    ASSERT(format.sampleRate() >= 0);

    if (m_outputDescription && *m_outputDescription == format)
        return noErr;

    m_outputDescription = CAAudioStreamDescription { format };

    {
        // Heap allocations are forbidden on the audio thread for performance reasons so we need to
        // explicitly allow the following allocation(s).
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
        m_ringBuffer = InProcessCARingBuffer::allocate(format, static_cast<size_t>(m_maximumSampleCount));
        RELEASE_ASSERT(m_ringBuffer);
        m_scratchBuffer = AudioSampleBufferList::create(m_outputDescription->streamDescription(), m_maximumSampleCount);
        m_converterInputOffset = 0;
    }

    return setupConverter();
}

MediaTime AudioSampleDataSource::hostTime() const
{
    // Based on listing #2 from Apple Technical Q&A QA1398, modified to be thread-safe.
    static double frequency;
    static mach_timebase_info_data_t timebaseInfo;
    static std::once_flag initializeTimerOnceFlag;
    std::call_once(initializeTimerOnceFlag, [] {
        kern_return_t kr = mach_timebase_info(&timebaseInfo);
        frequency = 1e-9 * static_cast<double>(timebaseInfo.numer) / static_cast<double>(timebaseInfo.denom);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        ASSERT(timebaseInfo.denom);
    });

    return MediaTime::createWithDouble(mach_absolute_time() * frequency);
}

void AudioSampleDataSource::pushSamplesInternal(const AudioBufferList& bufferList, const MediaTime& presentationTime, size_t sampleCount)
{
    int64_t ringBufferIndexToWrite = presentationTime.toTimeScale(m_outputDescription->sampleRate()).timeValue();

    int64_t offset = 0;
    const AudioBufferList* sampleBufferList;

    if (m_converter.updateBufferedAmount(m_lastBufferedAmount, sampleCount)) {
        m_scratchBuffer->reset();
        m_converter.convert(bufferList, *m_scratchBuffer, sampleCount);
        auto expectedSampleCount = sampleCount * m_outputDescription->sampleRate() / m_inputDescription->sampleRate();

        if (m_converter.isRegular() && expectedSampleCount > m_scratchBuffer->sampleCount()) {
            // Sometimes converter is not writing enough data, for instance on first chunk conversion.
            // Pretend this is the case to keep pusher and puller in sync.
            offset = 0;
            sampleCount = expectedSampleCount;
            if (m_scratchBuffer->sampleCount() > sampleCount)
                m_scratchBuffer->setSampleCount(sampleCount);
        } else {
            offset = m_scratchBuffer->sampleCount() - expectedSampleCount;
            sampleCount = m_scratchBuffer->sampleCount();
        }
        sampleBufferList = m_scratchBuffer->bufferList().list();
    } else
        sampleBufferList = &bufferList;

    if (!m_inputSampleOffset) {
        m_inputSampleOffset = 0 - ringBufferIndexToWrite;
        ringBufferIndexToWrite = 0;
    } else
        ringBufferIndexToWrite += *m_inputSampleOffset;

    if (m_converterInputOffset)
        ringBufferIndexToWrite += m_converterInputOffset;

    if (m_expectedNextPushedSampleTimeValue && abs((float)m_expectedNextPushedSampleTimeValue - (float)ringBufferIndexToWrite) <= 1)
        ringBufferIndexToWrite = m_expectedNextPushedSampleTimeValue;

    m_expectedNextPushedSampleTimeValue = ringBufferIndexToWrite + sampleCount;

    if (m_isInNeedOfMoreData) {
        m_isInNeedOfMoreData = false;
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
        RunLoop::main().dispatch([logIdentifier = LOGIDENTIFIER, sampleCount, this, protectedThis = Ref { *this }] {
            ALWAYS_LOG(logIdentifier, "needed more data, pushing ", sampleCount, " samples");
        });
    }

    m_ringBuffer->store(sampleBufferList, sampleCount, ringBufferIndexToWrite);

    m_converterInputOffset += offset;
    m_lastPushedSampleCount = sampleCount;
}

void AudioSampleDataSource::pushSamples(const AudioStreamBasicDescription& sampleDescription, CMSampleBufferRef sampleBuffer)
{
    ASSERT_UNUSED(sampleDescription, *m_inputDescription == sampleDescription);

    WebAudioBufferList list(*m_inputDescription, sampleBuffer);
    pushSamplesInternal(list, PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer)), PAL::CMSampleBufferGetNumSamples(sampleBuffer));
}

void AudioSampleDataSource::pushSamples(const MediaTime& sampleTime, const PlatformAudioData& audioData, size_t sampleCount)
{
    ASSERT(is<WebAudioBufferList>(audioData));
    pushSamplesInternal(*downcast<WebAudioBufferList>(audioData).list(), sampleTime, sampleCount);
}

bool AudioSampleDataSource::pullSamples(AudioBufferList& buffer, size_t sampleCount, uint64_t timeStamp, double /*hostTime*/, PullMode mode)
{
    size_t byteCount = sampleCount * m_outputDescription->bytesPerFrame();

    ASSERT(buffer.mNumberBuffers == m_ringBuffer->channelCount());
    if (buffer.mNumberBuffers != m_ringBuffer->channelCount()) {
        if (mode != AudioSampleDataSource::Mix)
            AudioSampleBufferList::zeroABL(buffer, byteCount);
        return false;
    }

    if (m_muted || !m_inputSampleOffset) {
        if (mode != AudioSampleDataSource::Mix)
            AudioSampleBufferList::zeroABL(buffer, byteCount);
        return false;
    }

    auto [startFrame, endFrame] = m_ringBuffer->getFetchTimeBounds();

    ASSERT(m_waitToStartForPushCount);

    uint64_t buffered = endFrame - startFrame;
    if (m_shouldComputeOutputSampleOffset) {
        auto minimumBuffer = std::max<size_t>(m_waitToStartForPushCount * m_lastPushedSampleCount, m_converter.regularBufferSize());
        if (buffered < minimumBuffer) {
            // We wait for one chunk of value before starting to play.
            if (mode != AudioSampleDataSource::Mix)
                AudioSampleBufferList::zeroABL(buffer, byteCount);
            return false;
        }
        m_outputSampleOffset = endFrame - timeStamp - minimumBuffer;
        m_shouldComputeOutputSampleOffset = false;
    }

    timeStamp += m_outputSampleOffset;

    if (timeStamp < startFrame || timeStamp + sampleCount > endFrame) {
        if (!m_isInNeedOfMoreData) {
            m_isInNeedOfMoreData = true;
            DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
            RunLoop::main().dispatch([logIdentifier = LOGIDENTIFIER, timeStamp, startFrame = startFrame, endFrame = endFrame, sampleCount, outputSampleOffset = m_outputSampleOffset, this, protectedThis = Ref { *this }] {
                ERROR_LOG(logIdentifier, "need more data, sample ", timeStamp, " with offset ", outputSampleOffset, ", trying to get ", sampleCount, " samples, but not completely in range [", startFrame, " .. ", endFrame, "]");
            });
        }
        m_shouldComputeOutputSampleOffset = true;
        if (mode != AudioSampleDataSource::Mix)
            AudioSampleBufferList::zeroABL(buffer, byteCount);
        return false;
    }

    m_lastBufferedAmount = endFrame - timeStamp - sampleCount;
    return pullSamplesInternal(buffer, sampleCount, timeStamp, mode);
}

bool AudioSampleDataSource::pullSamplesInternal(AudioBufferList& buffer, size_t sampleCount, uint64_t timeStamp, PullMode mode)
{
    if (mode == Copy) {
        m_ringBuffer->fetch(&buffer, sampleCount, timeStamp, CARingBuffer::Copy);
        if (m_volume < EquivalentToMaxVolume)
            AudioSampleBufferList::applyGain(buffer, m_volume, m_outputDescription->format());
        return true;
    }

    if (m_volume >= EquivalentToMaxVolume) {
        m_ringBuffer->fetch(&buffer, sampleCount, timeStamp, CARingBuffer::fetchModeForMixing(m_outputDescription->format()));
        return true;
    }

    if (m_scratchBuffer->copyFrom(*m_ringBuffer, sampleCount, timeStamp, CARingBuffer::Copy))
        return false;

    m_scratchBuffer->applyGain(m_volume);
    m_scratchBuffer->mixFrom(buffer, sampleCount);
    if (m_scratchBuffer->copyTo(buffer, sampleCount))
        return false;

    return true;
}

bool AudioSampleDataSource::pullAvailableSampleChunk(AudioBufferList& buffer, size_t sampleCount, uint64_t timeStamp, PullMode mode)
{
    ASSERT(buffer.mNumberBuffers == m_ringBuffer->channelCount());
    if (buffer.mNumberBuffers != m_ringBuffer->channelCount())
        return false;

    if (m_muted || !m_inputSampleOffset)
        return false;

    if (m_shouldComputeOutputSampleOffset) {
        m_shouldComputeOutputSampleOffset = false;
        m_outputSampleOffset = *m_inputSampleOffset;
    }

    timeStamp += m_outputSampleOffset;

    return pullSamplesInternal(buffer, sampleCount, timeStamp, mode);
}

bool AudioSampleDataSource::pullAvailableSamplesAsChunks(AudioBufferList& buffer, size_t sampleCountPerChunk, uint64_t timeStamp, Function<void()>&& consumeFilledBuffer)
{
    ASSERT(buffer.mNumberBuffers == m_ringBuffer->channelCount());
    if (buffer.mNumberBuffers != m_ringBuffer->channelCount())
        return false;

    auto [startFrame, endFrame] = m_ringBuffer->getFetchTimeBounds();
    if (m_shouldComputeOutputSampleOffset) {
        m_outputSampleOffset = timeStamp + (endFrame - sampleCountPerChunk);
        m_shouldComputeOutputSampleOffset = false;
    }

    timeStamp += m_outputSampleOffset;

    if (timeStamp < startFrame)
        timeStamp = startFrame;

    startFrame = timeStamp;

    ASSERT(endFrame >= startFrame);
    if (endFrame < startFrame)
        return false;

    if (m_muted) {
        AudioSampleBufferList::zeroABL(buffer, sampleCountPerChunk * m_outputDescription->bytesPerFrame());
        while (endFrame - startFrame >= sampleCountPerChunk) {
            consumeFilledBuffer();
            startFrame += sampleCountPerChunk;
        }
        return true;
    }

    while (endFrame - startFrame >= sampleCountPerChunk) {
        m_ringBuffer->fetch(&buffer, sampleCountPerChunk, startFrame, CARingBuffer::Copy);
        consumeFilledBuffer();
        startFrame += sampleCountPerChunk;
    }
    return true;
}

#if !RELEASE_LOG_DISABLED
void AudioSampleDataSource::setLogger(Ref<const Logger>&& logger, const void* logIdentifier)
{
    m_logger = WTFMove(logger);
    m_logIdentifier = logIdentifier;
}

WTFLogChannel& AudioSampleDataSource::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore
