/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "MediaSampleCursor.h"

#if ENABLE(WEBM_FORMAT_READER)

#include "Logging.h"
#include "MediaTrackReader.h"
#include <WebCore/CMUtilities.h>
#include <WebCore/MediaSample.h>
#include <WebCore/SampleMap.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <variant>
#include <wtf/CompletionHandler.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>

#include <pal/cocoa/MediaToolboxSoftLink.h>

WTF_DECLARE_CF_TYPE_TRAIT(MTPluginSampleCursor);

namespace WebKit {

using namespace WebCore;

static MediaTime assumedDecodeTime(const MediaTime& presentationTime, DecodeOrderSampleMap& map)
{
    MediaSample& firstSample = map.begin()->second;
    return firstSample.decodeTime() == firstSample.presentationTime() ? presentationTime : MediaTime::invalidTime();
}

template<typename OrderedMap> static OrderedMap& orderedSamples(SampleMap&);

template<>
DecodeOrderSampleMap& orderedSamples(SampleMap& samples)
{
    return samples.decodeOrder();
}

template<>
PresentationOrderSampleMap& orderedSamples(SampleMap& samples)
{
    return samples.presentationOrder();
}

static DecodeOrderSampleMap::iterator upperBound(DecodeOrderSampleMap& map, const MediaTime& time)
{
    auto decodeTime = assumedDecodeTime(time, map);
    return map.findSampleAfterDecodeKey(std::make_pair(decodeTime, time));
}

static PresentationOrderSampleMap::iterator upperBound(PresentationOrderSampleMap& map, const MediaTime& time)
{
    return map.findSampleStartingAfterPresentationTime(time);
}

template<typename OrderedMap>
static int64_t stepIterator(int64_t stepsRemaining, typename OrderedMap::iterator& iterator, MediaSampleByteRange::const_iterator& innerIterator, OrderedMap& samples)
{
    ASSERT(iterator != samples.end());
    if (stepsRemaining < 0) {
        for (; stepsRemaining && iterator != samples.begin(); ++stepsRemaining) {
            if (innerIterator == static_cast<MediaSampleByteRange*>(iterator->second.ptr())->begin()) {
                --iterator;
                innerIterator = static_cast<MediaSampleByteRange*>(iterator->second.ptr())->end();
            }
            --innerIterator;
        }
        return stepsRemaining;
    }
    while (stepsRemaining) {
        auto lastInner = static_cast<MediaSampleByteRange*>(iterator->second.ptr())->end();
        for (auto nextInner = std::next(innerIterator); stepsRemaining && nextInner != lastInner; --stepsRemaining, innerIterator = nextInner++) { }
        if (!stepsRemaining)
            break;
        auto next = std::next(iterator);
        if (next == samples.end())
            break;
        iterator = next;
        innerIterator = static_cast<MediaSampleByteRange*>(iterator->second.ptr())->begin();
        --stepsRemaining;
    }
    return stepsRemaining;
}

template<typename OrderedMap>
static bool stepTime(const MediaTime& delta, MediaTime& time, OrderedMap& samples, bool hasAllSamples, const MediaTime& trackDuration)
{
    time += delta;
    MediaSample& lastSample = samples.rbegin()->second;
    auto firstTime = samples.begin()->second->presentationTime();
    auto lastTime = hasAllSamples ? lastSample.presentationTime() + lastSample.duration() : trackDuration;
    return time < firstTime || time >= lastTime;
}

CMBaseClassID MediaSampleCursor::wrapperClassID()
{
    return MTPluginSampleCursorGetClassID();
}

CoreMediaWrapped<MediaSampleCursor>* MediaSampleCursor::unwrap(CMBaseObjectRef object)
{
    return unwrap(checked_cf_cast<WrapperRef>(object));
}

RefPtr<MediaSampleCursor> MediaSampleCursor::createAtPresentationTime(Allocator&& allocator, MediaTrackReader& trackReader, MediaTime time)
{
    return adoptRef(new (allocator) MediaSampleCursor(WTFMove(allocator), trackReader, WTFMove(time)));
}

RefPtr<MediaSampleCursor> MediaSampleCursor::copy(Allocator&& allocator, const MediaSampleCursor& cursor)
{
    Locker locker { cursor.m_locatorLock };
    return adoptRef(new (allocator) MediaSampleCursor(WTFMove(allocator), cursor));
}

MediaSampleCursor::MediaSampleCursor(Allocator&& allocator, MediaTrackReader& trackReader, Locator locator)
    : CoreMediaWrapped(WTFMove(allocator))
    , m_trackReader(trackReader)
    , m_locator(WTFMove(locator))
    , m_logger(trackReader.logger())
    , m_logIdentifier(trackReader.nextSampleCursorLogIdentifier(identifier()))
{
}

MediaSampleCursor::MediaSampleCursor(Allocator&& allocator, const MediaSampleCursor& cursor)
    : CoreMediaWrapped(WTFMove(allocator))
    , m_trackReader(cursor.m_trackReader.copyRef())
    , m_locator(cursor.m_locator)
    , m_currentEntry(cursor.m_currentEntry)
    , m_logger(cursor.m_logger.copyRef())
    , m_logIdentifier(m_trackReader->nextSampleCursorLogIdentifier(identifier()))
{
}

template<typename OrderedMap>
std::optional<typename OrderedMap::iterator> MediaSampleCursor::locateIterator(OrderedMap& samples, bool hasAllSamples) const
{
    ASSERT(m_locatorLock.isLocked());
    using Iterator = typename OrderedMap::iterator;
    return WTF::switchOn(m_locator,
        [&](const MediaTime& presentationTime) -> std::optional<Iterator> {
            assertIsHeld(m_locatorLock);
            auto iterator = upperBound(samples, presentationTime);
            if (iterator == samples.begin())
                setLocator(WTFMove(iterator));
            else if (hasAllSamples || iterator != samples.end())
                setLocator(std::prev(iterator));
            else
                return std::nullopt;
            return locateIterator(samples, hasAllSamples);
        }, [&](const auto& otherIterator) -> std::optional<Iterator> {
            assertIsHeld(m_locatorLock);
            setLocator(otherIterator->second->presentationTime());
            return locateIterator(samples, hasAllSamples);
        }, [&](const Iterator& iterator) -> std::optional<Iterator> {
            return iterator;
        }
    );
}

void MediaSampleCursor::setLocator(Locator&& locator) const
{
    ASSERT(m_locatorLock.isLocked());
    if (locator == m_locator)
        return;
    m_locator = WTFMove(locator);
    WTF::switchOn(m_locator,
        [&](const MediaTime&) {
            assertIsHeld(m_locatorLock);
            m_currentEntry.reset();
        }, [&](const auto& locator) {
            assertIsHeld(m_locatorLock);
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(locator->second->platformSample().type == PlatformSample::ByteRangeSampleType);
            m_currentEntry = static_cast<const MediaSampleByteRange*>(locator->second.ptr())->begin();
        }
    );
}

MediaSampleCursor::SampleType MediaSampleCursor::locateMediaSample(SampleMap& samples, bool hasAllSamples) const
{
    ASSERT(m_locatorLock.isLocked());
    return WTF::switchOn(m_locator,
        [&](const MediaTime&) -> SampleType {
            assertIsHeld(m_locatorLock);
            auto iterator = locateIterator(samples.presentationOrder(), hasAllSamples);
            if (!iterator)
                return std::nullopt;
            return locateMediaSample(samples, hasAllSamples);
        },
        [&](const auto& iterator) {
            assertIsHeld(m_locatorLock);
            ASSERT(m_currentEntry.has_value());
            return SampleType { std::in_place, iterator->second.ptr(), m_currentEntry.value() };
        }
    );
}

MediaSampleCursor::Timing MediaSampleCursor::locateTiming(SampleMap& samples, bool hasAllSamples) const
{
    ASSERT(m_locatorLock.isLocked());
    return WTF::switchOn(m_locator,
        [&](const MediaTime& presentationTime) {
            assertIsHeld(m_locatorLock);
            if (locateMediaSample(samples, hasAllSamples))
                return locateTiming(samples, hasAllSamples);
            auto& clampedPresentationTime = std::min(presentationTime, m_trackReader->duration());
            return Timing {
                assumedDecodeTime(clampedPresentationTime, samples.decodeOrder()),
                clampedPresentationTime,
                MediaTime::zeroTime(),
            };
        },
        [&](const auto& iterator) {
            assertIsHeld(m_locatorLock);
            ASSERT(m_currentEntry.has_value());
            auto* entry = *m_currentEntry;
            return Timing {
                entry->decodeTime,
                entry->presentationTime,
                entry->duration,
            };
        }
    );
}

template<typename OrderedMap>
OSStatus MediaSampleCursor::stepInOrderedMap(int64_t stepsToTake, int64_t& stepsTaken)
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) -> OSStatus {
        assertIsHeld(m_locatorLock);
        auto& orderedMap = orderedSamples<OrderedMap>(samples);
        if (auto iterator = locateIterator(orderedMap, hasAllSamples)) {
            ASSERT(m_currentEntry.has_value());
            auto innerIterator = *m_currentEntry;
            auto stepsRemaining = stepIterator(stepsToTake, *iterator, innerIterator, orderedMap);
            m_locator = WTFMove(*iterator);
            m_currentEntry = WTFMove(innerIterator);
            stepsTaken = stepsToTake - stepsRemaining;
            return noErr;
        }
        return kMTPluginSampleCursorError_LocationNotAvailable;
    });
}

OSStatus MediaSampleCursor::stepInPresentationTime(const MediaTime& delta, Boolean& wasPinned)
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) -> OSStatus {
        assertIsHeld(m_locatorLock);
        auto timing = locateTiming(samples, hasAllSamples);
        wasPinned = stepTime(delta, timing.presentationTime, samples.presentationOrder(), hasAllSamples, m_trackReader->duration());
        setLocator(timing.presentationTime);
        return noErr;
    });
}

template<typename Function>
OSStatus MediaSampleCursor::getSampleMap(Function&& function) const
{
    OSStatus status = noErr;
    Locker locker { m_locatorLock };
    m_trackReader->waitForSample([&](SampleMap& samples, bool hasAllSamples) {
        if (!samples.size())
            ERROR_LOG(LOGIDENTIFIER, "track ", m_trackReader->trackID(), " finished parsing with no samples.");
        status = samples.size() ? function(samples, hasAllSamples) : static_cast<OSStatus>(kMTPluginSampleCursorError_NoSamples);
        return true;
    });
    return status;
}

template<typename Function>
OSStatus MediaSampleCursor::getMediaSample(Function&& function) const
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) -> OSStatus {
        assertIsHeld(m_locatorLock);
        auto sample = locateMediaSample(samples, hasAllSamples);
        if (!sample)
            return kMTPluginSampleCursorError_LocationNotAvailable;
        DEBUG_LOG(LOGIDENTIFIER, "sample: ", *sample->first);
        function(*sample);
        return noErr;
    });
}

template<typename Function>
OSStatus MediaSampleCursor::getTiming(Function&& function) const
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) {
        assertIsHeld(m_locatorLock);
        auto timing = locateTiming(samples, hasAllSamples);
        DEBUG_LOG(LOGIDENTIFIER, "decodeTime: ", timing.decodeTime, ", presentationTime: ", timing.presentationTime, ", duration: ", timing.duration);
        function(timing);
        return noErr;
    });
}

OSStatus MediaSampleCursor::copyProperty(CFStringRef key, CFAllocatorRef, void*)
{
    ERROR_LOG(LOGIDENTIFIER, "asked for unsupported property ", String(key));
    return kCMBaseObjectError_ValueNotAvailable;
}

OSStatus MediaSampleCursor::copy(MTPluginSampleCursorRef* sampleCursor) const
{
    *sampleCursor = MediaSampleCursor::copy(allocator(), *this).leakRef()->wrapper();
    return noErr;
}

OSStatus MediaSampleCursor::stepInDecodeOrderAndReportStepsTaken(int64_t stepsToTake, int64_t* stepsTaken)
{
    return stepInOrderedMap<DecodeOrderSampleMap>(stepsToTake, *stepsTaken);
}

OSStatus MediaSampleCursor::stepInPresentationOrderAndReportStepsTaken(int64_t stepsToTake, int64_t* stepsTaken)
{
    return stepInOrderedMap<PresentationOrderSampleMap>(stepsToTake, *stepsTaken);
}

OSStatus MediaSampleCursor::stepByDecodeTime(CMTime time, Boolean* wasPinned)
{
    return stepInPresentationTime(PAL::toMediaTime(time), *wasPinned);
}

OSStatus MediaSampleCursor::stepByPresentationTime(CMTime time, Boolean* wasPinned)
{
    return stepInPresentationTime(PAL::toMediaTime(time), *wasPinned);
}

CFComparisonResult MediaSampleCursor::compareInDecodeOrder(MTPluginSampleCursorRef otherCursorRef) const
{
    MediaSampleCursor* otherCursor = unwrap(otherCursorRef);
    if (!otherCursor || m_trackReader.ptr() != otherCursor->m_trackReader.ptr())
        return kCFCompareLessThan;

    if (this == otherCursor) {
        RELEASE_ASSERT(wrapper() == otherCursorRef);
        return kCFCompareEqualTo;
    }

    auto presentationTime = MediaTime::invalidTime();
    getTiming([&](const Timing& timing) {
        presentationTime = timing.presentationTime;
    });

    auto otherPresentationTime = MediaTime::invalidTime();
    otherCursor->getTiming([&](const Timing& timing) {
        otherPresentationTime = timing.presentationTime;
    });

    switch (presentationTime.compare(otherPresentationTime)) {
    case MediaTime::LessThan:
        return kCFCompareLessThan;
    case MediaTime::EqualTo:
        return kCFCompareEqualTo;
    case MediaTime::GreaterThan:
        return kCFCompareGreaterThan;
    }
}

OSStatus MediaSampleCursor::getSampleTiming(CMSampleTimingInfo* sampleTiming) const
{
    return getTiming([&](const Timing& timing) {
        *sampleTiming = {
            .duration = PAL::toCMTime(timing.duration),
            .presentationTimeStamp = PAL::toCMTime(timing.presentationTime),
            .decodeTimeStamp = PAL::toCMTime(timing.decodeTime),
        };
    });
}

OSStatus MediaSampleCursor::getSyncInfo(MTPluginSampleCursorSyncInfo* syncInfo) const
{
    return getMediaSample([&](auto& sample) {
        *syncInfo = {
            .fullSync = (sample.second->flags & MediaSample::IsSync) != 0
        };
    });
}

OSStatus MediaSampleCursor::copyFormatDescription(CMFormatDescriptionRef* formatDescriptionOut) const
{
    return getMediaSample([&](auto& sample) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(sample.first->platformSample().type == PlatformSample::ByteRangeSampleType);
        *formatDescriptionOut = createFormatDescriptionFromTrackInfo(sample.first->platformSample().sample.byteRangeSample.second).leakRef();
    });
}

OSStatus MediaSampleCursor::copySampleLocation(MTPluginSampleCursorStorageRange* storageRange, MTPluginByteSourceRef* byteSource) const
{
    return getMediaSample([&](auto& sample) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(sample.first->platformSample().type == PlatformSample::ByteRangeSampleType);
        auto& byteRange = std::get<MediaSample::ByteRange>(sample.second->data);
        *storageRange = {
            .offset = CheckedInt64(byteRange.byteOffset),
            .length = CheckedInt64(byteRange.byteLength),
        };
        *byteSource = retainPtr(sample.first->platformSample().sample.byteRangeSample.first).leakRef();
    });
}

OSStatus MediaSampleCursor::getPlayableHorizon(CMTime* playableHorizon) const
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) {
        assertIsHeld(m_locatorLock);
        const MediaSample& lastSample = samples.decodeOrder().rbegin()->second;
        auto timing = locateTiming(samples, hasAllSamples);
        *playableHorizon = PAL::toCMTime(lastSample.presentationTime() + lastSample.duration() - timing.presentationTime);
        return noErr;
    });
}

WTFLogChannel& MediaSampleCursor::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
