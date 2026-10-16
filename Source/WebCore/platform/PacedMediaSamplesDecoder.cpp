/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PacedMediaSamplesDecoder.h"

#include "TrackInfo.h"
#include "VideoDecodeWindow.h"
#include <algorithm>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PacedMediaSamplesDecoder);

static constexpr MediaTime AudioDecodeLowWaterMark { 1, 2 };
static constexpr MediaTime AudioDecodeHighWaterMark { 1, 1 };

static constexpr double MinimumWakeDelay = 0.005;
static constexpr double MaximumWakeDelay = 1.0;

PacedMediaSamplesDecoder::PacedMediaSamplesDecoder(Ref<GuaranteedSerialFunctionDispatcher>&& dispatcher, Ref<MediaSamplesDecoder>&& decoder, bool isVideo, CurrentTimeProvider&& currentTime)
    : m_dispatcher(WTF::move(dispatcher))
    , m_decoder(WTF::move(decoder))
    , m_currentTime(WTF::move(currentTime))
    , m_isVideo(isVideo)
{
}

PacedMediaSamplesDecoder::~PacedMediaSamplesDecoder() = default;

Ref<PacedMediaSamplesDecoder::CreatePromise> PacedMediaSamplesDecoder::create(const TrackInfo& info, Filters filters, GuaranteedSerialFunctionDispatcher& dispatcher, CurrentTimeProvider&& currentTime)
{
    bool isVideo = info.isVideo();
    return MediaSamplesDecoder::create(info, filters)->whenSettled(dispatcher, [dispatcher = Ref { dispatcher }, isVideo, currentTime = WTF::move(currentTime)](auto&& result) mutable -> Ref<CreatePromise> {
        if (!result)
            return CreatePromise::createAndReject(result.error());
        return CreatePromise::createAndResolve(adoptRef(*new PacedMediaSamplesDecoder(WTF::move(dispatcher), WTF::move(*result), isVideo, WTF::move(currentTime))));
    });
}

bool PacedMediaSamplesDecoder::canContinueWith(const TrackInfo& info) const
{
    return m_decoder->canContinueWith(info);
}

Ref<PacedMediaSamplesDecoder::DecodePromise> PacedMediaSamplesDecoder::decode(MediaSamplesBlock&& block, std::optional<MediaTime> minimumUpcomingTime)
{
    assertIsCurrent(m_dispatcher.get());

    if (m_error)
        return DecodePromise::createAndReject(*m_error);

    RefPtr info = block.info();
    if (!info || !m_decoder->canContinueWith(*info))
        return DecodePromise::createAndReject(PlatformMediaError::InvalidState);

    Entry entry;
    entry.block = WTF::move(block);
    entry.minimumUpcomingTime = minimumUpcomingTime;
    return enqueue(WTF::move(entry));
}

Ref<PacedMediaSamplesDecoder::DecodePromise> PacedMediaSamplesDecoder::drain()
{
    assertIsCurrent(m_dispatcher.get());

    if (m_error)
        return DecodePromise::createAndReject(*m_error);

    Entry entry;
    entry.isDrain = true;
    return enqueue(WTF::move(entry));
}

Ref<MediaPromise> PacedMediaSamplesDecoder::flush()
{
    assertIsCurrent(m_dispatcher.get());

    ++m_generation;
    m_isDecoding = false;
    clearPending();
    updateReadiness();
    return m_decoder->flush();
}

bool PacedMediaSamplesDecoder::isReadyForMoreMediaData() const
{
    assertIsCurrent(m_dispatcher.get());

    return m_isReadyForMoreMediaData;
}

Ref<GenericPromise> PacedMediaSamplesDecoder::requestMediaDataWhenReady()
{
    assertIsCurrent(m_dispatcher.get());

    if (m_isReadyForMoreMediaData)
        return GenericPromise::createAndResolve();
    m_requestMediaDataProducer.emplace();
    return m_requestMediaDataProducer->promise();
}

void PacedMediaSamplesDecoder::stopRequestingMediaData()
{
    assertIsCurrent(m_dispatcher.get());

    m_requestMediaDataProducer.reset();
}

void PacedMediaSamplesDecoder::currentTimeDidChange()
{
    assertIsCurrent(m_dispatcher.get());

    m_wakeTime = std::nullopt;
    ++m_wakeID;
    pump();
    updateReadiness();
}

Ref<PacedMediaSamplesDecoder::DecodePromise> PacedMediaSamplesDecoder::enqueue(Entry&& entry)
{
    assertIsCurrent(m_dispatcher.get());

    Ref promise = entry.producer.promise();
    if (!entry.block.isEmpty())
        ++m_queuedBlockCount;
    m_pending.append(WTF::move(entry));
    pump();
    updateReadiness();
    return promise;
}

void PacedMediaSamplesDecoder::pump()
{
    assertIsCurrent(m_dispatcher.get());

    while (!m_isDecoding && !m_error && !m_pending.isEmpty()) {
        auto& entry = m_pending.first();
        if (entry.isDrain) {
            submit(m_decoder->drain());
            return;
        }

        if (entry.block.isEmpty() && !entry.block.discontinuity()) {
            Entry finished = m_pending.takeFirst();
            finished.producer.resolve(WTF::move(finished.output));
            continue;
        }

        if (!entry.block.isEmpty()) {
            Seconds waitHint;
            if (!(m_isVideo ? shouldDecodeVideoNow(entry, waitHint) : shouldDecodeAudioNow(waitHint))) {
                scheduleWake(waitHint);
                return;
            }
            --m_queuedBlockCount;
        }
        submit(m_decoder->decode(std::exchange(entry.block, { })));
        return;
    }
}

bool PacedMediaSamplesDecoder::shouldDecodeAudioNow(Seconds& waitHint)
{
    assertIsCurrent(m_dispatcher.get());

    MediaTime currentTime = m_currentTime().time;
    if (!currentTime.isValid() || !m_decodedAudioEndTime.isValid()) {
        m_idledAtHighWaterMark = false;
        return true;
    }

    MediaTime lowWaterMarkTime = currentTime + AudioDecodeLowWaterMark;
    if (m_idledAtHighWaterMark) {
        if (m_decodedAudioEndTime >= lowWaterMarkTime) {
            waitHint = Seconds { (m_decodedAudioEndTime - lowWaterMarkTime).toDouble() };
            return false;
        }
        m_idledAtHighWaterMark = false;
    }

    if (m_decodedAudioEndTime >= currentTime + AudioDecodeHighWaterMark) {
        m_idledAtHighWaterMark = true;
        waitHint = Seconds { (m_decodedAudioEndTime - lowWaterMarkTime).toDouble() };
        return false;
    }
    return true;
}

bool PacedMediaSamplesDecoder::shouldDecodeVideoNow(const Entry& entry, Seconds& waitHint)
{
    assertIsCurrent(m_dispatcher.get());

    MediaTime currentTime = m_currentTime().time;
    if (!currentTime.isValid()) {
        m_decodedPresentationTimes.clear();
        m_idledAtHighWaterMark = false;
        return true;
    }

    while (m_decodedPresentationTimes.size() > 1 && m_decodedPresentationTimes[1] < currentTime)
        m_decodedPresentationTimes.removeAt(0);

    if (m_decodedPresentationTimes.isEmpty()) {
        m_idledAtHighWaterMark = false;
        return true;
    }

    MediaTime base = std::max(currentTime, m_decodedPresentationTimes.first());
    MediaTime lowWaterMarkTime = base + VideoDecodeWindow::lowWaterMark;
    MediaTime highWaterMarkTime = base + VideoDecodeWindow::highWaterMark;
    MediaTime endTime = m_decodedPresentationTimes.last();
    Seconds timeUntilLowWaterMark { (endTime - (currentTime + VideoDecodeWindow::lowWaterMark)).toDouble() };

    if (m_idledAtHighWaterMark) {
        if (endTime >= lowWaterMarkTime) {
            waitHint = timeUntilLowWaterMark;
            return false;
        }
        m_idledAtHighWaterMark = false;
    }

    // A sample presented before what is already decoded is needed regardless of the window, and an unknown
    // minimum upcoming time leaves decoding unbounded.
    if (endTime > highWaterMarkTime && (!entry.minimumUpcomingTime || entry.minimumUpcomingTime->isFinite())) {
        MediaTime nextTime = entry.block.presentationTime();
        MediaTime upcomingMinimum = entry.minimumUpcomingTime ? std::min(nextTime, *entry.minimumUpcomingTime) : nextTime;
        if (endTime < upcomingMinimum) {
            m_idledAtHighWaterMark = true;
            waitHint = timeUntilLowWaterMark;
            return false;
        }
    }
    // The frames of a block share a presentation time and are decoded together.
    return true;
}

void PacedMediaSamplesDecoder::submit(Ref<DecodePromise>&& promise)
{
    assertIsCurrent(m_dispatcher.get());

    m_isDecoding = true;
    promise->whenSettled(m_dispatcher.get(), [protectedThis = Ref { *this }, generation = m_generation](DecodePromise::Result&& result) {
        protectedThis->didSettle(generation, WTF::move(result));
    });
}

void PacedMediaSamplesDecoder::didSettle(uint32_t generation, DecodePromise::Result&& result)
{
    assertIsCurrent(m_dispatcher.get());

    if (generation != m_generation)
        return;

    m_isDecoding = false;
    if (!result) {
        fail(result.error());
        return;
    }

    recordOutput(*result);
    if (!m_pending.isEmpty()) {
        if (m_pending.first().isDrain) {
            Entry finished = m_pending.takeFirst();
            finished.producer.resolve(WTF::move(*result));
        } else
            m_pending.first().output.appendVector(WTF::move(*result));
    }
    pump();
    updateReadiness();
}

void PacedMediaSamplesDecoder::recordOutput(const Vector<MediaSamplesBlock>& blocks)
{
    assertIsCurrent(m_dispatcher.get());

    for (auto& block : blocks) {
        for (auto& item : block) {
            if (m_isVideo) {
                auto position = std::upper_bound(m_decodedPresentationTimes.begin(), m_decodedPresentationTimes.end(), item.presentationTime);
                m_decodedPresentationTimes.insert(position - m_decodedPresentationTimes.begin(), item.presentationTime);
                continue;
            }
            MediaTime endTime = item.presentationTime + item.duration;
            if (endTime.isValid() && (!m_decodedAudioEndTime.isValid() || endTime > m_decodedAudioEndTime))
                m_decodedAudioEndTime = endTime;
        }
    }
}

void PacedMediaSamplesDecoder::fail(PlatformMediaError error)
{
    assertIsCurrent(m_dispatcher.get());

    m_error = error;

    auto pending = std::exchange(m_pending, { });
    clearPending();
    for (auto& entry : pending)
        entry.producer.reject(error);
    updateReadiness();
}

void PacedMediaSamplesDecoder::clearPending()
{
    assertIsCurrent(m_dispatcher.get());

    m_pending.clear();
    m_decodedPresentationTimes.clear();
    m_decodedAudioEndTime = MediaTime::invalidTime();
    m_queuedBlockCount = 0;
    m_idledAtHighWaterMark = false;
    m_wakeTime = std::nullopt;
    ++m_wakeID;
}

void PacedMediaSamplesDecoder::scheduleWake(Seconds waitHint)
{
    assertIsCurrent(m_dispatcher.get());

    // The time isn't advancing; currentTimeDidChange() resumes the work.
    auto position = m_currentTime();
    if (position.rate <= 0)
        return;

    Seconds delay { std::clamp(waitHint.seconds() / position.rate, MinimumWakeDelay, MaximumWakeDelay) };
    auto wakeTime = MonotonicTime::now() + delay;
    if (m_wakeTime && *m_wakeTime <= wakeTime)
        return;

    m_wakeTime = wakeTime;
    uint32_t wakeID = ++m_wakeID;
    m_dispatcher->dispatchAfter(delay, [weakThis = ThreadSafeWeakPtr { *this }, wakeID] {
        if (RefPtr protectedThis { weakThis })
            protectedThis->wake(wakeID);
    });
}

void PacedMediaSamplesDecoder::wake(uint32_t wakeID)
{
    assertIsCurrent(m_dispatcher.get());

    if (wakeID != m_wakeID)
        return;
    m_wakeTime = std::nullopt;
    pump();
    updateReadiness();
}

void PacedMediaSamplesDecoder::updateReadiness()
{
    assertIsCurrent(m_dispatcher.get());

    bool wasReady = m_isReadyForMoreMediaData;
    bool isReady = wasReady;
    if (m_queuedBlockCount > VideoDecodeWindow::sampleQueueHighWaterMark)
        isReady = false;
    else if (m_queuedBlockCount <= VideoDecodeWindow::sampleQueueLowWaterMark)
        isReady = true;
    if (isReady == wasReady)
        return;

    m_isReadyForMoreMediaData = isReady;
    if (isReady) {
        if (auto producer = std::exchange(m_requestMediaDataProducer, std::nullopt))
            producer->resolve();
    }
}

} // namespace WebCore
