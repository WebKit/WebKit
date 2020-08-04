/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#import "CAAudioStreamDescription.h"
#import "CARingBuffer.h"
#import "Logging.h"
#import "MediaStreamTrackPrivate.h"
#import <AudioToolbox/AudioConverter.h>
#import <mach/mach.h>
#import <mach/mach_time.h>
#import <mutex>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <syslog.h>
#import <wtf/StringPrintStream.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {
using namespace PAL;
using namespace JSC;

Ref<AudioSampleDataSource> AudioSampleDataSource::create(size_t maximumSampleCount, LoggerHelper& loggerHelper)
{
    return adoptRef(*new AudioSampleDataSource(maximumSampleCount, loggerHelper));
}

AudioSampleDataSource::AudioSampleDataSource(size_t maximumSampleCount, LoggerHelper& loggerHelper)
    : m_inputSampleOffset(MediaTime::invalidTime())
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
    m_inputDescription = nullptr;
    m_outputDescription = nullptr;
    m_ringBuffer = nullptr;
    if (m_converter) {
        AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }
}

OSStatus AudioSampleDataSource::setupConverter()
{
    ASSERT(m_inputDescription && m_outputDescription);

    if (m_converter) {
        AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }

    if (*m_inputDescription == *m_outputDescription)
        return 0;

    OSStatus err = AudioConverterNew(&m_inputDescription->streamDescription(), &m_outputDescription->streamDescription(), &m_converter);
    if (err) {
        dispatch_async(dispatch_get_main_queue(), [this, protectedThis = makeRefPtr(*this), err] {
            ERROR_LOG("AudioConverterNew returned error ", err);
        });
    }

    return err;

}

OSStatus AudioSampleDataSource::setInputFormat(const CAAudioStreamDescription& format)
{
    ASSERT(format.sampleRate() >= 0);

    m_inputDescription = makeUnique<CAAudioStreamDescription>(format);
    if (m_outputDescription)
        return setupConverter();

    return 0;
}

OSStatus AudioSampleDataSource::setOutputFormat(const CAAudioStreamDescription& format)
{
    ASSERT(m_inputDescription);
    ASSERT(format.sampleRate() >= 0);

    m_outputDescription = makeUnique<CAAudioStreamDescription>(format);
    if (!m_ringBuffer)
        m_ringBuffer = makeUnique<CARingBuffer>();

    m_ringBuffer->allocate(format, static_cast<size_t>(m_maximumSampleCount));
    m_scratchBuffer = AudioSampleBufferList::create(m_outputDescription->streamDescription(), m_maximumSampleCount);

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
    MediaTime sampleTime = presentationTime;

    const AudioBufferList* sampleBufferList;
    if (m_converter) {
        m_scratchBuffer->reset();
        OSStatus err = m_scratchBuffer->copyFrom(bufferList, sampleCount, m_converter);
        if (err)
            return;

        sampleBufferList = m_scratchBuffer->bufferList().list();
        sampleCount = m_scratchBuffer->sampleCount();
        sampleTime = presentationTime.toTimeScale(m_outputDescription->sampleRate(), MediaTime::RoundingFlags::TowardZero);
    } else
        sampleBufferList = &bufferList;

    if (m_expectedNextPushedSampleTime.isValid() && abs(m_expectedNextPushedSampleTime - sampleTime).timeValue() == 1)
        sampleTime = m_expectedNextPushedSampleTime;
    m_expectedNextPushedSampleTime = sampleTime + MediaTime(sampleCount, sampleTime.timeScale());

    if (m_inputSampleOffset == MediaTime::invalidTime()) {
        m_inputSampleOffset = MediaTime(1 - sampleTime.timeValue(), sampleTime.timeScale());
        dispatch_async(dispatch_get_main_queue(), [inputSampleOffset = m_inputSampleOffset.timeValue(), maximumSampleCount = m_maximumSampleCount, this, protectedThis = makeRefPtr(*this)] {
            ERROR_LOG("pushSamples: input sample offset is ", inputSampleOffset, ", maximumSampleCount = ", maximumSampleCount);
        });
    }
    sampleTime += m_inputSampleOffset;

#if !LOG_DISABLED
    uint64_t startFrame1 = 0;
    uint64_t endFrame1 = 0;
    m_ringBuffer->getCurrentFrameBounds(startFrame1, endFrame1);
#endif

    m_ringBuffer->store(sampleBufferList, sampleCount, sampleTime.timeValue());
    m_lastPushedSampleCount = sampleCount;

#if !LOG_DISABLED
    uint64_t startFrame2 = 0;
    uint64_t endFrame2 = 0;
    m_ringBuffer->getCurrentFrameBounds(startFrame2, endFrame2);
    dispatch_async(dispatch_get_main_queue(), [sampleCount, sampleTime, presentationTime, absoluteTime = mach_absolute_time(), startFrame1, endFrame1, startFrame2, endFrame2] {
        LOG(MediaCaptureSamples, "@@ pushSamples: added %ld samples for time = %s (was %s), mach time = %lld", sampleCount, toString(sampleTime).utf8().data(), toString(presentationTime).utf8().data(), absoluteTime);
        LOG(MediaCaptureSamples, "@@ pushSamples: buffered range was [%lld .. %lld], is [%lld .. %lld]", startFrame1, endFrame1, startFrame2, endFrame2);
    });
#endif
}

void AudioSampleDataSource::pushSamples(const AudioStreamBasicDescription& sampleDescription, CMSampleBufferRef sampleBuffer)
{
    ASSERT_UNUSED(sampleDescription, *m_inputDescription == sampleDescription);
    ASSERT(m_ringBuffer);
    
    WebAudioBufferList list(*m_inputDescription, sampleBuffer);
    pushSamplesInternal(list, PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer)), PAL::CMSampleBufferGetNumSamples(sampleBuffer));
}

void AudioSampleDataSource::pushSamples(const MediaTime& sampleTime, const PlatformAudioData& audioData, size_t sampleCount)
{
    ASSERT(is<WebAudioBufferList>(audioData));
    pushSamplesInternal(*downcast<WebAudioBufferList>(audioData).list(), sampleTime, sampleCount);
}

bool AudioSampleDataSource::pullSamplesInternal(AudioBufferList& buffer, size_t& sampleCount, uint64_t timeStamp, double /*hostTime*/, PullMode mode)
{
    size_t byteCount = sampleCount * m_outputDescription->bytesPerFrame();

    ASSERT(buffer.mNumberBuffers == m_ringBuffer->channelCount());
    if (buffer.mNumberBuffers != m_ringBuffer->channelCount()) {
        AudioSampleBufferList::zeroABL(buffer, byteCount);
        sampleCount = 0;
        return false;
    }

    if (!m_ringBuffer || m_muted || m_inputSampleOffset == MediaTime::invalidTime()) {
        AudioSampleBufferList::zeroABL(buffer, byteCount);
        sampleCount = 0;
        return false;
    }

    uint64_t startFrame = 0;
    uint64_t endFrame = 0;
    m_ringBuffer->getCurrentFrameBounds(startFrame, endFrame);

    if (m_transitioningFromPaused) {
        uint64_t buffered = endFrame - startFrame;
        if (buffered < sampleCount * 2) {
            AudioSampleBufferList::zeroABL(buffer, byteCount);
            sampleCount = 0;
            return false;
        }

        const double twentyMS = .02;
        const double tenMS = .01;
        const double fiveMS = .005;
        double sampleRate = m_outputDescription->sampleRate();
        m_outputSampleOffset = (endFrame - sampleCount) - timeStamp;
        if (m_lastPushedSampleCount > sampleRate * twentyMS)
            m_outputSampleOffset -= sampleRate * twentyMS;
        else if (m_lastPushedSampleCount > sampleRate * tenMS)
            m_outputSampleOffset -= sampleRate * tenMS;
        else if (m_lastPushedSampleCount > sampleRate * fiveMS)
            m_outputSampleOffset -= sampleRate * fiveMS;

        m_transitioningFromPaused = false;
    }

    timeStamp += m_outputSampleOffset;

#if !LOG_DISABLED
    dispatch_async(dispatch_get_main_queue(), [sampleCount, timeStamp, sampleOffset = m_outputSampleOffset] {
        LOG(MediaCaptureSamples, "** pullSamplesInternal: asking for %ld samples at time = %lld (was %lld)", sampleCount, timeStamp, timeStamp - sampleOffset);
    });
#endif

    uint64_t framesAvailable = sampleCount;
    if (timeStamp < startFrame || timeStamp + sampleCount > endFrame) {
        if (timeStamp + sampleCount < startFrame || timeStamp >= endFrame)
            framesAvailable = 0;
        else if (timeStamp < startFrame)
            framesAvailable = timeStamp + sampleCount - startFrame;
        else
            framesAvailable = timeStamp + sampleCount - endFrame;

#if !RELEASE_LOG_DISABLED
        dispatch_async(dispatch_get_main_queue(), [timeStamp, startFrame, endFrame, framesAvailable, sampleCount, this, protectedThis = makeRefPtr(*this)] {
            ALWAYS_LOG("sample ", timeStamp, " is not completely in range [", startFrame, " .. ", endFrame, "], returning ", framesAvailable, " frames");
            if (framesAvailable < sampleCount)
                ERROR_LOG("not enough data available, returning zeroes");
        });
#endif

        if (framesAvailable < sampleCount) {
            m_outputSampleOffset -= sampleCount - framesAvailable;
            AudioSampleBufferList::zeroABL(buffer, byteCount);
            return false;
        }
    }

    if (mode == Copy) {
        m_ringBuffer->fetch(&buffer, sampleCount, timeStamp, CARingBuffer::Copy);
        if (m_volume < EquivalentToMaxVolume)
            AudioSampleBufferList::applyGain(buffer, m_volume, m_outputDescription->format());
        return true;
    }

    if (m_volume >= EquivalentToMaxVolume) {
        m_ringBuffer->fetch(&buffer, sampleCount, timeStamp, CARingBuffer::Mix);
        return true;
    }

    if (m_scratchBuffer->copyFrom(*m_ringBuffer.get(), sampleCount, timeStamp, CARingBuffer::Copy))
        return false;

    m_scratchBuffer->applyGain(m_volume);
    m_scratchBuffer->mixFrom(buffer, sampleCount);
    if (m_scratchBuffer->copyTo(buffer, sampleCount))
        return false;

    return true;
}

bool AudioSampleDataSource::pullAvalaibleSamplesAsChunks(AudioBufferList& buffer, size_t sampleCountPerChunk, uint64_t timeStamp, Function<void()>&& consumeFilledBuffer)
{
    if (!m_ringBuffer)
        return false;

    ASSERT(buffer.mNumberBuffers == m_ringBuffer->channelCount());
    if (buffer.mNumberBuffers != m_ringBuffer->channelCount())
        return false;

    uint64_t startFrame = 0;
    uint64_t endFrame = 0;
    m_ringBuffer->getCurrentFrameBounds(startFrame, endFrame);
    if (m_transitioningFromPaused) {
        m_outputSampleOffset = timeStamp + (endFrame - sampleCountPerChunk);
        m_transitioningFromPaused = false;
    }

    timeStamp += m_outputSampleOffset;

    if (timeStamp < startFrame)
        timeStamp = startFrame;

    startFrame = timeStamp;

    if (m_muted) {
        AudioSampleBufferList::zeroABL(buffer, sampleCountPerChunk * m_outputDescription->bytesPerFrame());
        while (endFrame - startFrame >= sampleCountPerChunk) {
            consumeFilledBuffer();
            startFrame += sampleCountPerChunk;
        }
        return true;
    }

    while (endFrame - startFrame >= sampleCountPerChunk) {
        if (m_ringBuffer->fetch(&buffer, sampleCountPerChunk, startFrame, CARingBuffer::Copy))
            return false;
        consumeFilledBuffer();
        startFrame += sampleCountPerChunk;
    }
    return true;
}

bool AudioSampleDataSource::pullSamples(AudioBufferList& buffer, size_t sampleCount, uint64_t timeStamp, double hostTime, PullMode mode)
{
    if (!m_ringBuffer) {
        size_t byteCount = sampleCount * m_outputDescription->bytesPerFrame();
        AudioSampleBufferList::zeroABL(buffer, byteCount);
        return false;
    }

    return pullSamplesInternal(buffer, sampleCount, timeStamp, hostTime, mode);
}

bool AudioSampleDataSource::pullSamples(AudioSampleBufferList& buffer, size_t sampleCount, uint64_t timeStamp, double hostTime, PullMode mode)
{
    if (!m_ringBuffer) {
        buffer.zero();
        return false;
    }

    if (!pullSamplesInternal(buffer.bufferList(), sampleCount, timeStamp, hostTime, mode))
        return false;

    buffer.setTimes(timeStamp, hostTime);
    buffer.setSampleCount(sampleCount);

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

#endif // ENABLE(MEDIA_STREAM)
