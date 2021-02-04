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

#include "MediaTrackReader.h"
#include <WebCore/Logging.h>
#include <WebCore/MediaSample.h>
#include <WebCore/SampleMap.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/CompletionHandler.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/Variant.h>

#include <pal/cocoa/MediaToolboxSoftLink.h>

WTF_DECLARE_CF_TYPE_TRAIT(MTPluginSampleCursor);

namespace WebKit {

using namespace PAL;
using namespace WebCore;

static MediaTime assumedDecodeTime(const MediaTime& presentationTime, DecodeOrderSampleMap& map)
{
    MediaSample& firstSample = *map.begin()->second;
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
static int64_t stepIterator(int64_t stepsRemaining, typename OrderedMap::iterator& iterator, OrderedMap& samples)
{
    ASSERT(iterator != samples.end());
    if (stepsRemaining < 0)
        for (; stepsRemaining && iterator != samples.begin(); ++stepsRemaining, --iterator) { }
    else
        for (auto next = std::next(iterator); stepsRemaining && next != samples.end(); --stepsRemaining, iterator = next++) { }
    return stepsRemaining;
}

template<typename OrderedMap>
static bool stepTime(const MediaTime& delta, MediaTime& time, OrderedMap& samples, bool hasAllSamples, const MediaTime& trackDuration)
{
    time += delta;
    MediaSample& lastSample = *samples.rbegin()->second;
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
    auto locker = holdLock(cursor.m_locatorLock);
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
    , m_logger(cursor.m_logger.copyRef())
    , m_logIdentifier(m_trackReader->nextSampleCursorLogIdentifier(identifier()))
{
}

template<typename OrderedMap>
Optional<typename OrderedMap::iterator> MediaSampleCursor::locateIterator(OrderedMap& samples, bool hasAllSamples) const
{
    ASSERT(m_locatorLock.isLocked());
    using Iterator = typename OrderedMap::iterator;
    return WTF::switchOn(m_locator,
        [&](const MediaTime& presentationTime) -> Optional<Iterator> {
            auto iterator = upperBound(samples, presentationTime);
            if (iterator == samples.begin())
                m_locator = WTFMove(iterator);
            else if (hasAllSamples || iterator != samples.end())
                m_locator = std::prev(iterator);
            else
                return WTF::nullopt;
            return locateIterator(samples, hasAllSamples);
        },
        [&](const auto& otherIterator) {
            m_locator = otherIterator->second->presentationTime();
            return locateIterator(samples, hasAllSamples);
        },
        [&](const Iterator& iterator) {
            return iterator;
        }
    );
}

MediaSample* MediaSampleCursor::locateMediaSample(SampleMap& samples, bool hasAllSamples) const
{
    ASSERT(m_locatorLock.isLocked());
    return WTF::switchOn(m_locator,
        [&](const MediaTime&) -> MediaSample* {
            auto iterator = locateIterator(samples.presentationOrder(), hasAllSamples);
            if (!iterator)
                return nullptr;
            return locateMediaSample(samples, hasAllSamples);
        },
        [&](const auto& iterator) {
            return iterator->second.get();
        }
    );
}

MediaSampleCursor::Timing MediaSampleCursor::locateTiming(SampleMap& samples, bool hasAllSamples) const
{
    ASSERT(m_locatorLock.isLocked());
    return WTF::switchOn(m_locator,
        [&](const MediaTime& presentationTime) {
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
            return Timing {
                iterator->second->decodeTime(),
                iterator->second->presentationTime(),
                iterator->second->duration(),
            };
        }
    );
}

template<typename OrderedMap>
OSStatus MediaSampleCursor::stepInOrderedMap(int64_t stepsToTake, int64_t& stepsTaken)
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) -> OSStatus {
        auto& orderedMap = orderedSamples<OrderedMap>(samples);
        if (auto iterator = locateIterator(orderedMap, hasAllSamples)) {
            auto stepsRemaining = stepIterator(stepsToTake, *iterator, orderedMap);
            m_locator = WTFMove(*iterator);
            stepsTaken = stepsToTake - stepsRemaining;
            return noErr;
        }
        return kMTPluginSampleCursorError_LocationNotAvailable;
    });
}

OSStatus MediaSampleCursor::stepInPresentationTime(const MediaTime& delta, Boolean& wasPinned)
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) -> OSStatus {
        auto timing = locateTiming(samples, hasAllSamples);
        wasPinned = stepTime(delta, timing.presentationTime, samples.presentationOrder(), hasAllSamples, m_trackReader->duration());
        m_locator = timing.presentationTime;
        return noErr;
    });
}

template<typename Function>
OSStatus MediaSampleCursor::getSampleMap(Function&& function) const
{
    OSStatus status = noErr;
    auto locker = holdLock(m_locatorLock);
    m_trackReader->waitForSample([&](SampleMap& samples, bool hasAllSamples) {
        if (!samples.size())
            ERROR_LOG(LOGIDENTIFIER, "track ", m_trackReader->trackID(), " finished parsing with no samples.");
        status = samples.size() ? function(samples, hasAllSamples) : kMTPluginSampleCursorError_NoSamples;
        return true;
    });
    return status;
}

template<typename Function>
OSStatus MediaSampleCursor::getMediaSample(Function&& function) const
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) -> OSStatus {
        auto sample = locateMediaSample(samples, hasAllSamples);
        if (!sample)
            return kMTPluginSampleCursorError_LocationNotAvailable;
        DEBUG_LOG(LOGIDENTIFIER, "sample: ", *sample);
        function(*sample);
        return noErr;
    });
}

template<typename Function>
OSStatus MediaSampleCursor::getTiming(Function&& function) const
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) {
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
    OSStatus syncInfoStatus = noErr;
    auto getSampleStatus = getMediaSample([&](MediaSample& sample) {
        if (sample.hasSyncInfo()) {
            *syncInfo = {
                .fullSync = sample.isSync()
            };
            return;
        }
        syncInfoStatus = kCMBaseObjectError_ValueNotAvailable;
    });
    if (syncInfoStatus != noErr)
        return syncInfoStatus;
    return getSampleStatus;
}

OSStatus MediaSampleCursor::copyFormatDescription(CMFormatDescriptionRef* formatDescriptionOut) const
{
    return getMediaSample([&](MediaSample& sample) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(sample.platformSample().type == PlatformSample::ByteRangeSampleType);
        *formatDescriptionOut = retainPtr(sample.platformSample().sample.byteRangeSample.second).leakRef();
    });
}

OSStatus MediaSampleCursor::copySampleLocation(MTPluginSampleCursorStorageRange* storageRange, MTPluginByteSourceRef* byteSource) const
{
    return getMediaSample([&](MediaSample& sample) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(sample.platformSample().type == PlatformSample::ByteRangeSampleType);
        auto byteRange = *sample.byteRange();
        *storageRange = {
            .offset = CheckedInt64(byteRange.byteOffset).unsafeGet(),
            .length = CheckedInt64(byteRange.byteLength).unsafeGet(),
        };
        *byteSource = retainPtr(sample.platformSample().sample.byteRangeSample.first).leakRef();
    });
}

OSStatus MediaSampleCursor::getPlayableHorizon(CMTime* playableHorizon) const
{
    return getSampleMap([&](SampleMap& samples, bool hasAllSamples) {
        MediaSample& lastSample = *samples.decodeOrder().rbegin()->second;
        auto timing = locateTiming(samples, hasAllSamples);
        *playableHorizon = PAL::toCMTime(lastSample.presentationTime() + lastSample.duration() - timing.presentationTime);
        return noErr;
    });
}

WTFLogChannel& MediaSampleCursor::logChannel() const
{
    return LogMedia;
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
